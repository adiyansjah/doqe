#include "doqe/base/memory_pool.hpp"
#include "doqe/base/status.hpp"
#include "doqe/column/arrow/c/abi.h"
#include "doqe/column/column.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <limits>
#include <memory>
#include <new> // IWYU pragma: keep
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>

namespace doqe::column {
namespace {

template <class CData> class Children {
public:
  [[nodiscard]] base::Result<void> Allocate(base::MemoryPool &pool, std::size_t count) noexcept {
    if (count == 0) {
      return {};
    }
    static_assert(sizeof(CData) >= sizeof(CData *));
    if (count > std::numeric_limits<std::size_t>::max() / sizeof(CData)) {
      return base::Fail(base::Code::ResourceExhausted);
    }
    const auto entries = pool.Allocate(count * sizeof(CData), alignof(CData));
    if (!entries) {
      return std::unexpected(entries.error());
    }
    storage_allocation = *entries;
    // Initialize every release callback before allocating the links.
    static_assert(std::is_nothrow_default_constructible_v<CData>);
    storage = {::new (*entries) CData[count]{}, count};
    const auto links = pool.Allocate(count * sizeof(CData *), alignof(CData *));
    if (!links) {
      return std::unexpected(links.error());
    }
    pointers_allocation = *links;
    pointers = {::new (*links) CData *[count], count};
    auto entries_to_link = storage;
    for (auto &pointer : pointers) {
      pointer = &entries_to_link.front();
      entries_to_link = entries_to_link.subspan(1);
    }
    return {};
  }

  void Release(base::MemoryPool &pool) noexcept {
    for (auto &child : storage) {
      if (child.release != nullptr) {
        child.release(&child);
      }
    }
    std::ranges::destroy(storage);
    pool.Free(pointers_allocation, pointers.size_bytes(), alignof(CData *));
    pool.Free(storage_allocation, storage.size_bytes(), alignof(CData));
  }

  // The caller iterates the field count used to allocate this span.
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
  [[nodiscard]] CData &operator[](std::size_t index) noexcept { return storage[index]; }
  [[nodiscard]] CData **data() noexcept { return pointers.data(); }

private:
  void *storage_allocation = nullptr;
  std::span<CData> storage{};
  void *pointers_allocation = nullptr;
  std::span<CData *> pointers{};
};

struct ArrayState {
  base::PoolPtr pool{};
  void *allocation = nullptr;
  Children<ArrowArray> children{};
  Array backing;
  // This pointer table is a subobject of the constructed export state.
  std::array<const void *, 3> buffers{};
};

struct SchemaState {
  base::PoolPtr pool{};
  void *allocation = nullptr;
  Children<ArrowSchema> children{};
  void *name_allocation = nullptr;
  std::size_t name_bytes = 0;
};

template <class State, class CData> void Release(CData *out) noexcept {
  auto *state = static_cast<State *>(out->private_data);
  const auto pool = std::move(state->pool);
  state->children.Release(*pool);
  if constexpr (std::is_same_v<State, SchemaState>) {
    pool->Free(state->name_allocation, state->name_bytes, alignof(char));
  }
  auto *allocation = state->allocation;
  std::destroy_at(state);
  pool->Free(allocation, sizeof(State), alignof(State));
  out->private_data = nullptr;
  out->release = nullptr;
}

template <class State, class CData>
[[nodiscard]] base::Result<State *> BeginExport(base::MemoryPool &pool, CData &out) noexcept {
  const auto memory = pool.Allocate(sizeof(State), alignof(State));
  if (!memory) {
    return std::unexpected(memory.error());
  }
  auto *state = std::construct_at(static_cast<State *>(*memory));
  state->allocation = *memory;
  state->pool = base::PoolPtr::Retain(pool);
  out = {};
  out.private_data = state;
  out.release = Release<State, CData>;
  return state;
}

template <class CData> void Rollback(CData &out) noexcept {
  if (out.release != nullptr) {
    out.release(&out);
    out = {};
  }
}

[[nodiscard]] base::Result<void> ExportSchema(base::MemoryPool &pool, const char *format,
                                              std::optional<std::string_view> name,
                                              ArrowSchema &out) noexcept {
  auto state = BeginExport<SchemaState>(pool, out);
  if (!state) {
    return std::unexpected(state.error());
  }
  out.format = format;
  if (!name) {
    return {};
  }
  out.flags = ARROW_FLAG_NULLABLE;
  out.name = "";
  if (name->empty()) {
    return {};
  }
  if (name->size() == std::numeric_limits<std::size_t>::max()) {
    Rollback(out);
    return base::Fail(base::Code::ResourceExhausted);
  }
  const auto bytes = name->size() + 1;
  const auto memory = pool.Allocate(bytes, alignof(char));
  if (!memory) {
    Rollback(out);
    return std::unexpected(memory.error());
  }
  const auto characters = std::span(::new (*memory) char[bytes], bytes);
  std::memcpy(characters.data(), name->data(), name->size());
  characters.back() = '\0';
  (*state)->name_allocation = *memory;
  (*state)->name_bytes = bytes;
  out.name = characters.data();
  return {};
}

const char *Format(Type type) noexcept { return type == Type::Int64 ? "l" : "u"; }

} // namespace

base::Result<void> Array::ExportData(base::MemoryPool &pool, ArrowArray &out) const noexcept {
  auto state = BeginExport<ArrayState>(pool, out);
  if (!state) {
    return std::unexpected(state.error());
  }
  (*state)->backing = *this;
  (*state)->buffers = {null_count_ == 0 ? nullptr : validity_.copied_data(), values_.copied_data(),
                       utf8_.copied_data()};
  out.buffers = (*state)->buffers.data();
  out.n_buffers = type_ == Type::Utf8 ? 3 : 2;
  out.length = length_;
  out.null_count = null_count_;
  out.offset = offset_;
  return {};
}

base::Result<void> Array::Export(base::MemoryPool &pool, ArrowArray &data,
                                 ArrowSchema &schema) const noexcept {
  data = {};
  schema = {};
  auto array = ExportData(pool, data);
  if (!array) {
    return array;
  }
  auto type = ExportSchema(pool, Format(type_), std::nullopt, schema);
  if (!type) {
    Rollback(data);
    return type;
  }
  schema.flags = ARROW_FLAG_NULLABLE;
  return {};
}

base::Result<void> Batch::Export(ArrowArray &data, ArrowSchema &schema) const noexcept {
  data = {};
  schema = {};
  if (!pool_) {
    return base::Fail(base::Code::InvalidArgument);
  }
  auto &pool = *pool_;
  auto data_state = BeginExport<ArrayState>(pool, data);
  if (!data_state) {
    return std::unexpected(data_state.error());
  }
  auto schema_state = BeginExport<SchemaState>(pool, schema);
  if (!schema_state) {
    Rollback(data);
    return std::unexpected(schema_state.error());
  }
  const auto populate = [&] noexcept -> base::Result<void> {
    auto &arrays = (*data_state)->children;
    auto &schemas = (*schema_state)->children;
    if (auto allocated = arrays.Allocate(pool, fields_.size()); !allocated) {
      return allocated;
    }
    if (auto allocated = schemas.Allocate(pool, fields_.size()); !allocated) {
      return allocated;
    }
    std::size_t i = 0;
    // Both child tables were allocated with one entry per field.
    // NOLINTBEGIN(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    for (const auto &field : fields()) {
      if (auto child = field.array.ExportData(pool, arrays[i]); !child) {
        return child;
      }
      if (auto child = ExportSchema(pool, Format(field.array.type()), field.name, schemas[i]);
          !child) {
        return child;
      }
      ++i;
    }
    // NOLINTEND(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    data.length = length_;
    data.n_buffers = 1;
    data.buffers = (*data_state)->buffers.data();
    data.n_children = static_cast<std::int64_t>(fields_.size());
    data.children = arrays.data();
    schema.format = "+s";
    schema.n_children = data.n_children;
    schema.children = schemas.data();
    return {};
  };
  auto result = populate();
  if (!result) {
    Rollback(data);
    Rollback(schema);
  }
  return result;
}

} // namespace doqe::column
