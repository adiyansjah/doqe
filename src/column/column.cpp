#include "doqe/column/column.hpp"
#include "column_internal.hpp"
#include "doqe/base/memory_pool.hpp"
#include "doqe/base/status.hpp"
#include "doqe/base/utf8.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <limits>
#include <memory>
#include <new> // IWYU pragma: keep
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>

namespace doqe::column {
namespace {

constexpr auto kMaxUtf8Bytes = static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max());

constexpr auto kBitsPerByte = static_cast<std::size_t>(std::numeric_limits<unsigned char>::digits);

bool GetBit(std::span<const std::uint8_t> bitmap, std::size_t index) noexcept {
  // The array validates the bitmap extent and checks the row before calling.
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
  const auto byte = bitmap[index / kBitsPerByte];
  return (byte & (1U << (index % kBitsPerByte))) != 0U;
}

} // namespace

base::Result<void> CheckUtf8DataSize(std::size_t size) noexcept {
  if (size > kMaxUtf8Bytes) {
    return base::Fail(base::Code::Unsupported);
  }
  return {};
}

base::Result<Utf8Append> CheckUtf8Append(std::size_t bytes_used, std::size_t value_size) noexcept {
  if (value_size > kMaxUtf8Bytes) {
    return base::Fail(base::Code::Unsupported);
  }
  if (bytes_used > kMaxUtf8Bytes) {
    return base::Fail(base::Code::InvalidArgument);
  }
  return value_size <= kMaxUtf8Bytes - bytes_used ? Utf8Append::Fits : Utf8Append::NewBatch;
}

Array::Array(Array &&other) noexcept
    : validity_(std::move(other.validity_)), values_(std::move(other.values_)),
      utf8_(std::move(other.utf8_)), offset_(std::exchange(other.offset_, 0)),
      length_(std::exchange(other.length_, 0)), null_count_(std::exchange(other.null_count_, 0)),
      type_(std::exchange(other.type_, Type::Int64)) {}

Array &Array::operator=(Array &&other) noexcept {
  Array incoming(std::move(other));
  std::swap(validity_, incoming.validity_);
  std::swap(values_, incoming.values_);
  std::swap(utf8_, incoming.utf8_);
  std::swap(offset_, incoming.offset_);
  std::swap(length_, incoming.length_);
  std::swap(null_count_, incoming.null_count_);
  std::swap(type_, incoming.type_);
  return *this;
}

base::Result<void> Array::SetValidity(base::MemoryPool &pool,
                                      std::span<const std::uint8_t> bitmap) noexcept {
  if (bitmap.empty()) {
    return {};
  }
  const auto rows = static_cast<std::size_t>(length_);
  if (bitmap.size() != rows / kBitsPerByte + (rows % kBitsPerByte != 0 ? 1 : 0)) {
    return base::Fail(base::Code::InvalidArgument);
  }
  for (std::size_t row = 0; row < rows; ++row) {
    if (!GetBit(bitmap, row)) {
      ++null_count_;
    }
  }
  if (null_count_ != 0) {
    auto buffer = Buffer::Copy(pool, std::as_bytes(bitmap));
    if (!buffer) {
      return std::unexpected(buffer.error());
    }
    validity_ = std::move(*buffer);
  }
  return {};
}

base::Result<Array> Array::Int64(base::MemoryPool &pool, std::span<const std::int64_t> values,
                                 std::span<const std::uint8_t> validity) noexcept {
  if (!std::in_range<std::int64_t>(values.size()) ||
      values.size() > std::numeric_limits<std::size_t>::max() / sizeof(std::int64_t)) {
    return base::Fail(base::Code::ResourceExhausted);
  }
  Array result;
  result.length_ = static_cast<std::int64_t>(values.size());
  if (auto bitmap = result.SetValidity(pool, validity); !bitmap) {
    return std::unexpected(bitmap.error());
  }
  auto buffer = Buffer::Copy(pool, std::as_bytes(values));
  if (!buffer) {
    return std::unexpected(buffer.error());
  }
  result.values_ = std::move(*buffer);
  return result;
}

base::Result<Array> Array::Utf8(base::MemoryPool &pool, std::span<const std::int32_t> offsets,
                                std::string_view data, std::span<const std::uint8_t> validity,
                                Type type) noexcept {
  switch (type) {
  case Type::LargeUtf8:
  case Type::Utf8View:
    return base::Fail(base::Code::Unsupported);
  case Type::Utf8:
    break;
  default:
    return base::Fail(base::Code::InvalidArgument);
  }
  if (auto extent = CheckUtf8DataSize(data.size()); !extent) {
    return std::unexpected(extent.error());
  }
  if (offsets.empty() || offsets.front() != 0 || !std::cmp_equal(offsets.back(), data.size()) ||
      !std::ranges::is_sorted(offsets)) {
    return base::Fail(base::Code::InvalidArgument);
  }
  if (!std::in_range<std::int64_t>(offsets.size() - 1) ||
      offsets.size() > std::numeric_limits<std::size_t>::max() / sizeof(std::int32_t)) {
    return base::Fail(base::Code::ResourceExhausted);
  }
  Array result;
  result.type_ = Type::Utf8;
  result.length_ = static_cast<std::int64_t>(offsets.size() - 1);
  if (auto bitmap = result.SetValidity(pool, validity); !bitmap) {
    return std::unexpected(bitmap.error());
  }
  auto indices = Buffer::Copy(pool, std::as_bytes(offsets));
  if (!indices) {
    return std::unexpected(indices.error());
  }
  auto bytes = Buffer::Copy(pool, std::as_bytes(std::span(data)));
  if (!bytes) {
    return std::unexpected(bytes.error());
  }
  result.values_ = std::move(*indices);
  result.utf8_ = std::move(*bytes);
  return result;
}

base::Result<Array> Array::Slice(std::int64_t start, std::int64_t length) const noexcept {
  if (start < 0 || length < 0 || start > length_ || length > length_ - start) {
    return base::Fail(base::Code::InvalidArgument);
  }
  auto result = *this;
  result.offset_ += start;
  result.length_ = length;
  result.null_count_ = 0;
  if (null_count_ != 0) {
    for (std::int64_t row = 0; row < length; ++row) {
      if (!result.is_valid(row)) {
        ++result.null_count_;
      }
    }
  }
  return result;
}

bool Array::is_valid(std::int64_t row) const noexcept {
  if (row < 0 || row >= length_) {
    return false;
  }
  const auto bitmap = validity_.uint8_view();
  return bitmap.empty() || GetBit(bitmap, static_cast<std::size_t>(offset_ + row));
}

Batch::Batch(base::MemoryPool &pool, std::int64_t length) noexcept
    : pool_(base::PoolPtr::Retain(pool)), length_(length) {}

Batch::Batch(Batch &&other) noexcept
    : pool_(std::move(other.pool_)),
      field_allocation_(std::exchange(other.field_allocation_, nullptr)),
      fields_(std::exchange(other.fields_, {})),
      name_allocation_(std::exchange(other.name_allocation_, nullptr)),
      name_bytes_(std::exchange(other.name_bytes_, 0)), length_(std::exchange(other.length_, 0)) {}

Batch &Batch::operator=(Batch &&other) noexcept {
  Batch prior(std::move(other));
  std::swap(pool_, prior.pool_);
  std::swap(field_allocation_, prior.field_allocation_);
  std::swap(fields_, prior.fields_);
  std::swap(name_allocation_, prior.name_allocation_);
  std::swap(name_bytes_, prior.name_bytes_);
  std::swap(length_, prior.length_);
  return *this;
}

Batch::~Batch() noexcept {
  if (field_allocation_ != nullptr) {
    std::ranges::destroy(fields_);
    pool_->Free(field_allocation_, fields_.size_bytes(), alignof(Field));
    pool_->Free(name_allocation_, name_bytes_, alignof(char));
  }
}

base::Result<Batch> Batch::Make(base::MemoryPool &pool, std::int64_t length,
                                std::span<const Field> fields) noexcept {
  if (length < 0) {
    return base::Fail(base::Code::InvalidArgument);
  }
  if (!std::in_range<std::int64_t>(fields.size()) ||
      fields.size() > std::numeric_limits<std::size_t>::max() / sizeof(Field)) {
    return base::Fail(base::Code::ResourceExhausted);
  }
  std::size_t name_bytes = 0;
  std::size_t seen = 0;
  for (const auto &field : fields) {
    if (field.array.length() != length || field.name.find('\0') != std::string_view::npos ||
        base::Utf8Prefix(field.name, field.name.size()) != field.name.size() ||
        std::ranges::any_of(fields.first(seen),
                            [&field](const Field &earlier) { return field.id == earlier.id; })) {
      return base::Fail(base::Code::InvalidArgument);
    }
    if (field.name.size() > std::numeric_limits<std::size_t>::max() - name_bytes) {
      return base::Fail(base::Code::ResourceExhausted);
    }
    name_bytes += field.name.size();
    ++seen;
  }
  Batch result(pool, length);
  if (fields.empty()) {
    return result;
  }
  const auto charge = fields.size_bytes();
  auto memory = pool.Allocate(charge, alignof(Field));
  if (!memory) {
    return std::unexpected(memory.error());
  }
  result.field_allocation_ = *memory;
  // Construct the whole array before any further allocation can fail.
  static_assert(std::is_nothrow_default_constructible_v<Field>);
  result.fields_ = {::new (*memory) Field[fields.size()]{}, fields.size()};
  std::span<char> names;
  if (name_bytes != 0) {
    const auto allocation = pool.Allocate(name_bytes, alignof(char));
    if (!allocation) {
      return std::unexpected(allocation.error());
    }
    result.name_allocation_ = *allocation;
    result.name_bytes_ = name_bytes;
    names = {::new (*allocation) char[name_bytes], name_bytes};
  }
  for (auto &destination : result.fields_) {
    const auto &field = fields.front();
    fields = fields.subspan(1);
    std::string_view name;
    if (!field.name.empty()) {
      std::memcpy(names.data(), field.name.data(), field.name.size());
      name = {names.data(), field.name.size()};
      names = names.subspan(field.name.size());
    }
    destination = Field{.id = field.id, .name = name, .array = field.array};
  }
  return result;
}

const Field *Batch::Find(std::int32_t field_id) const noexcept {
  const auto all = fields();
  const auto field = std::ranges::find(all, field_id, &Field::id);
  return field == all.end() ? nullptr : &*field;
}

} // namespace doqe::column
