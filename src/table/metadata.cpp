#include "doqe/table/metadata.hpp"

#include "doqe/base/arena.hpp"
#include "doqe/base/memory_pool.hpp"
#include "doqe/base/status.hpp"
#include "doqe/column/column.hpp"
#include "doqe/format/json.hpp"
#include "doqe/io/file_system.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <span>
#include <string_view>
#include <utility>

namespace doqe::table {
namespace {

using base::Arena;
using base::Code;
using base::Fail;
using base::Result;
using format::JsonValue;
using Kind = JsonValue::Kind;

[[nodiscard]] bool Is(const JsonValue *value, Kind kind) noexcept {
  return value != nullptr && value->kind() == kind;
}

[[nodiscard]] Result<std::int64_t> Integer(const JsonValue *value, std::string_view key) noexcept {
  const auto *field = value->Find(key);
  if (field == nullptr) {
    return Fail(Code::InvalidArgument, "missing integer", key);
  }
  return field->AsInt64();
}

[[nodiscard]] bool FitsInt32(std::int64_t id) noexcept {
  return id >= std::numeric_limits<std::int32_t>::min() &&
         id <= std::numeric_limits<std::int32_t>::max();
}

[[nodiscard]] Result<std::int32_t> Id(const JsonValue *value, std::string_view key) noexcept {
  const auto id = Integer(value, key);
  if (!id) {
    return std::unexpected(id.error());
  }
  if (!FitsInt32(*id)) {
    return Fail(Code::InvalidArgument, "ID exceeds int32 range", key);
  }
  return static_cast<std::int32_t>(*id);
}

struct IndexedValue {
  std::int64_t id = 0;
  const JsonValue *value = nullptr;
};

[[nodiscard]] Result<std::span<IndexedValue>> Index(const JsonValue *array, std::string_view id_key,
                                                    Arena &arena) noexcept {
  if (!Is(array, Kind::Array)) {
    return Fail(Code::InvalidArgument, "expected array", id_key);
  }
  const auto elements = array->elements();
  auto index = arena.MakeArray<IndexedValue>(elements.size());
  if (!index) {
    return std::unexpected(index.error());
  }
  auto entries = *index;
  for (const auto *item : elements) {
    if (!Is(item, Kind::Object)) {
      return Fail(Code::InvalidArgument, "expected object", id_key);
    }
    const auto id = Integer(item, id_key);
    if (!id) {
      return std::unexpected(id.error());
    }
    if (id_key != "snapshot-id" && !FitsInt32(*id)) {
      return Fail(Code::InvalidArgument, "ID exceeds int32 range", id_key);
    }
    entries.front() = {.id = *id, .value = item};
    entries = entries.subspan(1);
  }
  std::ranges::sort(*index, {}, &IndexedValue::id);
  if (std::ranges::adjacent_find(*index, {}, &IndexedValue::id) != index->end()) {
    return Fail(Code::InvalidArgument, "duplicate metadata ID", id_key);
  }
  return index;
}

[[nodiscard]] const JsonValue *Lookup(std::span<const IndexedValue> index,
                                      std::int64_t id) noexcept {
  const auto found = std::ranges::lower_bound(index, id, {}, &IndexedValue::id);
  return found == index.end() || found->id != id ? nullptr : found->value;
}

// The first traversal validates shape and counts IDs. The second fills one
// exactly sized, arena-backed array that can be sorted for duplicate detection.
class FieldIds {
public:
  explicit FieldIds(std::span<std::int32_t> output = {}) noexcept : output_(output) {}
  [[nodiscard]] std::size_t size() const noexcept { return size_; }

  [[nodiscard]] Result<void> Add(const JsonValue *value, std::string_view key) noexcept {
    constexpr std::int32_t largest_data_id = 2147483447;
    const auto id = Id(value, key);
    if (!id) {
      return std::unexpected(id.error());
    }
    if (*id > largest_data_id) {
      return Fail(Code::InvalidArgument, "reserved Iceberg data field ID", key);
    }
    if (!output_.empty()) {
      output_.front() = *id;
      output_ = output_.subspan(1);
    }
    ++size_;
    return {};
  }

private:
  std::span<std::int32_t> output_;
  std::size_t size_ = 0;
};

[[nodiscard]] Result<void> ValidateType(const JsonValue *type, FieldIds &ids) noexcept;

// Recursion follows the DOM, which already enforces the JSON nesting ceiling.
// NOLINTNEXTLINE(misc-no-recursion)
[[nodiscard]] Result<void> ValidateFields(const JsonValue *fields, FieldIds &ids) noexcept {
  if (!Is(fields, Kind::Array)) {
    return Fail(Code::InvalidArgument, "expected schema fields array");
  }
  for (const auto *field : fields->elements()) {
    if (!Is(field, Kind::Object) || !Is(field->Find("name"), Kind::String) ||
        !Is(field->Find("required"), Kind::Boolean)) {
      return Fail(Code::InvalidArgument, "invalid schema field");
    }
    const auto id = ids.Add(field, "id");
    if (!id) {
      return id;
    }
    const auto type = ValidateType(field->Find("type"), ids);
    if (!type) {
      return type;
    }
  }
  return {};
}

// NOLINTNEXTLINE(misc-no-recursion)
[[nodiscard]] Result<void> ValidateType(const JsonValue *type, FieldIds &ids) noexcept {
  if (Is(type, Kind::String)) {
    const auto name = type->text();
    if (!name.empty() && name != "struct" && name != "list" && name != "map") {
      return {};
    }
  }
  if (!Is(type, Kind::Object) || !Is(type->Find("type"), Kind::String)) {
    return Fail(Code::InvalidArgument, "invalid schema field type");
  }
  const auto name = type->Find("type")->text();
  if (name == "struct") {
    return ValidateFields(type->Find("fields"), ids);
  }
  if (name == "list" && Is(type->Find("element-required"), Kind::Boolean)) {
    const auto element = ids.Add(type, "element-id");
    if (!element) {
      return element;
    }
    return ValidateType(type->Find("element"), ids);
  }
  if (name == "map" && Is(type->Find("value-required"), Kind::Boolean)) {
    const auto key_id = ids.Add(type, "key-id");
    if (!key_id) {
      return key_id;
    }
    const auto key = ValidateType(type->Find("key"), ids);
    if (!key) {
      return key;
    }
    const auto value_id = ids.Add(type, "value-id");
    if (!value_id) {
      return value_id;
    }
    return ValidateType(type->Find("value"), ids);
  }
  return Fail(Code::InvalidArgument, "invalid nested schema field type");
}

[[nodiscard]] Result<void> ValidateSchema(const JsonValue *schema, Arena &arena) noexcept {
  const auto *type = schema->Find("type");
  if (!Is(type, Kind::String) || type->text() != "struct") {
    return Fail(Code::InvalidArgument, "expected struct schema");
  }
  const auto *fields = schema->Find("fields");
  FieldIds count;
  const auto validated = ValidateFields(fields, count);
  if (!validated) {
    return validated;
  }
  auto storage = arena.MakeArray<std::int32_t>(count.size());
  if (!storage) {
    return std::unexpected(storage.error());
  }
  FieldIds ids{*storage};
  const auto collected = ValidateFields(fields, ids);
  if (!collected) {
    return collected;
  }
  std::ranges::sort(*storage);
  if (std::ranges::adjacent_find(*storage) != storage->end()) {
    return Fail(Code::InvalidArgument, "duplicate schema field ID");
  }
  return {};
}

[[nodiscard]] Result<column::Type> ProjectedType(const JsonValue *type, std::int32_t id) noexcept {
  if (type->kind() == Kind::String) {
    if (type->text() == "long") {
      return column::Type::Int64;
    }
    if (type->text() == "string") {
      return column::Type::Utf8;
    }
  }
  constexpr std::string_view prefix = "unsupported type for field ID ";
  std::array<char, prefix.size() + std::numeric_limits<std::int32_t>::digits10 + 2> message{};
  const auto begin = std::ranges::copy(prefix, message.begin()).out;
  const auto converted = std::to_chars(begin, message.end(), id);
  return Fail(Code::Unsupported, std::string_view(message.data(), converted.ptr));
}

[[nodiscard]] Result<std::span<SchemaField>> Project(const JsonValue *schema,
                                                     Arena &arena) noexcept {
  const auto fields = schema->Find("fields")->elements();
  auto projected = arena.MakeArray<SchemaField>(fields.size());
  if (!projected) {
    return std::unexpected(projected.error());
  }
  auto output = *projected;
  for (const auto *field : fields) {
    const auto id = Id(field, "id");
    if (!id) {
      return std::unexpected(id.error());
    }
    const auto type = ProjectedType(field->Find("type"), *id);
    if (!type) {
      return std::unexpected(type.error());
    }
    output.front() = {.id = *id,
                      .name = field->Find("name")->text(),
                      .required = field->Find("required")->boolean(),
                      .type = *type};
    output = output.subspan(1);
  }
  return projected;
}

[[nodiscard]] Result<std::span<PartitionSpec>> PartitionSpecs(std::span<const IndexedValue> index,
                                                              Arena &arena) noexcept {
  auto specs = arena.MakeArray<PartitionSpec>(index.size());
  if (!specs) {
    return std::unexpected(specs.error());
  }
  auto output = *specs;
  for (const auto &entry : index) {
    const auto *fields = entry.value->Find("fields");
    if (!Is(fields, Kind::Array)) {
      return Fail(Code::InvalidArgument, "expected partition fields array");
    }
    auto identities = arena.MakeArray<std::int32_t>(fields->elements().size());
    if (!identities) {
      return std::unexpected(identities.error());
    }
    auto remaining = *identities;
    for (const auto *field : fields->elements()) {
      if (!Is(field, Kind::Object) || !Is(field->Find("transform"), Kind::String)) {
        return Fail(Code::InvalidArgument, "invalid partition transform");
      }
      const auto id = Id(field, "source-id");
      if (!id) {
        return std::unexpected(id.error());
      }
      if (field->Find("transform")->text() == "identity") {
        remaining.front() = *id;
        remaining = remaining.subspan(1);
      }
    }
    output.front() = {.id = static_cast<std::int32_t>(entry.id),
                      .identity_source_ids =
                          identities->first(identities->size() - remaining.size())};
    output = output.subspan(1);
  }
  return specs;
}

[[nodiscard]] Result<std::optional<Snapshot>> ResolveSnapshot(const JsonValue *root,
                                                              std::span<const IndexedValue> schemas,
                                                              Arena &arena) noexcept {
  constexpr std::int64_t no_snapshot = -1;
  std::int64_t selected = no_snapshot;
  const auto *current = root->Find("current-snapshot-id");
  if (current != nullptr && !Is(current, Kind::Null)) {
    const auto id = current->AsInt64();
    if (!id) {
      return std::unexpected(id.error());
    }
    selected = *id;
  }
  if (const auto *refs = root->Find("refs"); refs != nullptr) {
    if (!Is(refs, Kind::Object)) {
      return Fail(Code::InvalidArgument, "expected refs object");
    }
    if (const auto *main = refs->Find("main"); main != nullptr) {
      const auto main_id = Integer(main, "snapshot-id");
      if (!main_id || *main_id == no_snapshot || *main_id != selected) {
        return Fail(Code::InvalidArgument, "main ref disagrees with current snapshot ID");
      }
    }
  }

  std::span<const IndexedValue> snapshots;
  if (const auto *array = root->Find("snapshots"); array != nullptr) {
    const auto index = Index(array, "snapshot-id", arena);
    if (!index) {
      return std::unexpected(index.error());
    }
    snapshots = *index;
  }
  if (selected == no_snapshot) {
    return std::nullopt;
  }
  const auto *snapshot = Lookup(snapshots, selected);
  if (snapshot == nullptr) {
    return Fail(Code::InvalidArgument, "current snapshot ID does not resolve");
  }
  const auto *location = snapshot->Find("manifest-list");
  if (!Is(location, Kind::String) || location->text().empty()) {
    return Fail(Code::InvalidArgument, "current snapshot requires a manifest-list location");
  }
  if (snapshot->Find("schema-id") != nullptr) {
    const auto schema_id = Id(snapshot, "schema-id");
    if (!schema_id || Lookup(schemas, *schema_id) == nullptr) {
      return Fail(Code::InvalidArgument, "current snapshot schema ID does not resolve");
    }
  }
  return Snapshot{.id = selected, .manifest_list = location->text()};
}

} // namespace

TableMetadata::TableMetadata(base::Arena arena, std::int32_t current_schema_id,
                             std::span<const SchemaField> fields,
                             std::span<const PartitionSpec> specs,
                             std::optional<Snapshot> snapshot) noexcept
    : arena_(std::move(arena)), current_schema_id_(current_schema_id), fields_(fields),
      specs_(specs), snapshot_(snapshot) {}

TableMetadata::TableMetadata(TableMetadata &&other) noexcept
    : arena_(std::move(other.arena_)),
      current_schema_id_(std::exchange(other.current_schema_id_, 0)),
      fields_(std::exchange(other.fields_, {})), specs_(std::exchange(other.specs_, {})),
      snapshot_(std::exchange(other.snapshot_, {})) {}

TableMetadata &TableMetadata::operator=(TableMetadata &&other) noexcept {
  if (this != &other) {
    arena_ = std::move(other.arena_);
    current_schema_id_ = std::exchange(other.current_schema_id_, 0);
    fields_ = std::exchange(other.fields_, {});
    specs_ = std::exchange(other.specs_, {});
    snapshot_ = std::exchange(other.snapshot_, {});
  }
  return *this;
}

Result<TableMetadata> ReadMetadata(std::string_view location, const io::FileSystem &io,
                                   base::PoolPtr pool) noexcept {
  Arena arena(std::move(pool));
  const auto input = io.ReadWhole(location, format::json_size_limit, arena);
  if (!input) {
    return std::unexpected(input.error());
  }
  if (input->starts_with("\x1f\x8b")) {
    return Fail(Code::Unsupported, "gzip metadata is unsupported", location);
  }
  const auto json = format::ParseJson(*input, arena);
  if (!json) {
    return std::unexpected(json.error());
  }
  const auto *root = *json;
  const auto *version = root->Find("format-version");
  if (!Is(version, Kind::Number) ||
      version->text().find_first_of(".eE") != std::string_view::npos) {
    return Fail(Code::InvalidArgument, "expected integer format-version", location);
  }
  // The JSON grammar gives v2 a single integer spelling regardless of the int64 range.
  if (version->text() != "2") {
    return Fail(Code::Unsupported, "only Iceberg metadata v2 is supported", location);
  }
  const auto schema_id = Id(root, "current-schema-id");
  if (!schema_id) {
    return std::unexpected(schema_id.error());
  }
  const auto schemas = Index(root->Find("schemas"), "schema-id", arena);
  if (!schemas) {
    return std::unexpected(schemas.error());
  }
  const auto *current_schema = Lookup(*schemas, *schema_id);
  if (current_schema == nullptr) {
    return Fail(Code::InvalidArgument, "current schema ID does not resolve");
  }
  for (const auto &schema : *schemas) {
    const auto valid = ValidateSchema(schema.value, arena);
    if (!valid) {
      return std::unexpected(valid.error());
    }
  }
  const auto spec_index = Index(root->Find("partition-specs"), "spec-id", arena);
  if (!spec_index) {
    return std::unexpected(spec_index.error());
  }
  const auto specs = PartitionSpecs(*spec_index, arena);
  if (!specs) {
    return std::unexpected(specs.error());
  }
  const auto snapshot = ResolveSnapshot(root, *schemas, arena);
  if (!snapshot) {
    return std::unexpected(snapshot.error());
  }
  const auto fields = Project(current_schema, arena);
  if (!fields) {
    return std::unexpected(fields.error());
  }
  return TableMetadata(std::move(arena), *schema_id, *fields, *specs, *snapshot);
}

} // namespace doqe::table
