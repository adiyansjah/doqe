#include "doqe/base/memory_pool.hpp"
#include "doqe/base/status.hpp"
#include "doqe/column/column.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <limits>
#include <memory>
#include <new> // IWYU pragma: keep
#include <span>
#include <utility>

namespace doqe::column {
namespace {

constexpr std::size_t kAlignment = 64;

} // namespace

struct Buffer::Storage {
  base::PoolPtr pool{};
  void *allocation = nullptr;
  void *data_allocation = nullptr;
  std::byte *data = nullptr;
  const void *copied_data = nullptr;
  std::size_t charge = 0;
  std::atomic_size_t references{1};
};

Buffer::Buffer(const Buffer &other) noexcept
    : storage_(other.storage_), offset_(other.offset_), size_(other.size_) {
  if (storage_ != nullptr) {
    storage_->references.fetch_add(1, std::memory_order_relaxed);
  }
}

Buffer::Buffer(Buffer &&other) noexcept
    : storage_(std::exchange(other.storage_, nullptr)), offset_(std::exchange(other.offset_, 0)),
      size_(std::exchange(other.size_, 0)) {}

Buffer &Buffer::operator=(const Buffer &other) noexcept { return *this = Buffer(other); }

Buffer &Buffer::operator=(Buffer &&other) noexcept {
  Buffer incoming(std::move(other));
  std::swap(storage_, incoming.storage_);
  std::swap(offset_, incoming.offset_);
  std::swap(size_, incoming.size_);
  return *this;
}

Buffer::~Buffer() noexcept {
  if (storage_ != nullptr && storage_->references.fetch_sub(1, std::memory_order_acq_rel) == 1) {
    const auto pool = std::move(storage_->pool);
    pool->Free(storage_->data_allocation, storage_->charge, kAlignment);
    auto *allocation = storage_->allocation;
    std::destroy_at(storage_);
    pool->Free(allocation, sizeof(Storage), alignof(Storage));
  }
}

base::Result<std::size_t> Buffer::PaddedSize(std::size_t size) noexcept {
  if (size > std::numeric_limits<std::size_t>::max() - (kAlignment - 1)) {
    return base::Fail(base::Code::ResourceExhausted);
  }
  return (size + kAlignment - 1) & ~(kAlignment - 1);
}

base::Result<Buffer> Buffer::Copy(base::MemoryPool &pool,
                                  std::span<const std::byte> bytes) noexcept {
  if (bytes.empty()) {
    return Buffer{};
  }
  const auto charge = PaddedSize(bytes.size());
  if (!charge) {
    return std::unexpected(charge.error());
  }
  const auto data = pool.Allocate(*charge, kAlignment);
  if (!data) {
    return std::unexpected(data.error());
  }
  const auto control = pool.Allocate(sizeof(Storage), alignof(Storage));
  if (!control) {
    pool.Free(*data, *charge, kAlignment);
    return std::unexpected(control.error());
  }
  const auto payload = std::span(::new (*data) std::byte[*charge], *charge);
  const auto padding = payload.subspan(bytes.size());
  std::memset(padding.data(), 0, padding.size());

  Buffer result;
  result.storage_ = std::construct_at(static_cast<Storage *>(*control));
  result.storage_->allocation = *control;
  result.storage_->pool = base::PoolPtr::Retain(pool);
  result.storage_->data_allocation = *data;
  result.storage_->data = payload.data();
  // memcpy also starts the lifetimes of the implicit-lifetime elements
  // (such as int64 values or int32 offsets) read by Arrow consumers.
  result.storage_->copied_data = std::memcpy(payload.data(), bytes.data(), bytes.size());
  result.storage_->charge = *charge;
  result.size_ = bytes.size();
  return result;
}

base::Result<Buffer> Buffer::Slice(std::size_t offset, std::size_t length) const noexcept {
  if (offset > size_ || length > size_ - offset) {
    return base::Fail(base::Code::InvalidArgument);
  }
  Buffer result(*this);
  result.offset_ += offset;
  result.size_ = length;
  return result;
}

std::span<const std::byte> Buffer::bytes() const noexcept {
  if (storage_ == nullptr) {
    return {};
  }
  // Both extent and offset come from Copy and bounds-checked Slice.
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  return {storage_->data + offset_, size_};
}

std::span<const std::uint8_t> Buffer::uint8_view() const noexcept {
  if (storage_ == nullptr) {
    return {};
  }
  // memcpy's result points to the first implicitly created uint8_t element, the same
  // elements Arrow consumers index. Offset and extent come from Copy and bounds-checked Slice.
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  return {static_cast<const std::uint8_t *>(storage_->copied_data) + offset_, size_};
}

const void *Buffer::copied_data() const noexcept {
  return storage_ == nullptr ? nullptr : storage_->copied_data;
}

} // namespace doqe::column
