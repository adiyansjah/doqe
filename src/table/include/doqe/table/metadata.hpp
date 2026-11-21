#ifndef DOQE_TABLE_METADATA_HPP
#define DOQE_TABLE_METADATA_HPP

#include "doqe/base/arena.hpp"
#include "doqe/base/memory_pool.hpp"
#include "doqe/base/status.hpp"
#include "doqe/column/column.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace doqe::io {
class FileSystem;
}

namespace doqe::table {

struct SchemaField {
  std::int32_t id = 0;
  std::string_view name;
  bool required = false;
  column::Type type = column::Type::Int64;
};

struct PartitionSpec {
  std::int32_t id = 0;
  std::span<const std::int32_t> identity_source_ids;
};

struct Snapshot {
  std::int64_t id;
  std::string_view manifest_list;
};

class TableMetadata {
public:
  TableMetadata(const TableMetadata &) = delete;
  TableMetadata &operator=(const TableMetadata &) = delete;
  TableMetadata(TableMetadata &&other) noexcept;
  TableMetadata &operator=(TableMetadata &&other) noexcept;
  ~TableMetadata() = default;

  // Views borrow the arena. The owner can be destroyed on another thread with
  // exclusive access, even after the caller releases its own pool reference.
  [[nodiscard]] std::int32_t current_schema_id() const noexcept { return current_schema_id_; }
  [[nodiscard]] std::span<const SchemaField> fields() const noexcept { return fields_; }
  [[nodiscard]] std::span<const PartitionSpec> partition_specs() const noexcept { return specs_; }
  [[nodiscard]] std::optional<Snapshot> current_snapshot() const noexcept { return snapshot_; }

private:
  friend base::Result<TableMetadata>
  ReadMetadata(std::string_view location, const io::FileSystem &io, base::PoolPtr pool) noexcept;

  TableMetadata(base::Arena arena, std::int32_t current_schema_id,
                std::span<const SchemaField> fields, std::span<const PartitionSpec> specs,
                std::optional<Snapshot> snapshot) noexcept;

  base::Arena arena_;
  std::int32_t current_schema_id_;
  std::span<const SchemaField> fields_;
  std::span<const PartitionSpec> specs_;
  std::optional<Snapshot> snapshot_;
};

[[nodiscard]] base::Result<TableMetadata>
ReadMetadata(std::string_view location, const io::FileSystem &io, base::PoolPtr pool) noexcept;

} // namespace doqe::table

#endif // DOQE_TABLE_METADATA_HPP
