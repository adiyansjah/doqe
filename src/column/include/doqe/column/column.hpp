#ifndef DOQE_COLUMN_COLUMN_HPP
#define DOQE_COLUMN_COLUMN_HPP

#include "doqe/base/memory_pool.hpp"
#include "doqe/base/status.hpp"
#include "doqe/column/arrow/c/abi.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace doqe::column {

class Buffer {
public:
  Buffer() noexcept = default;
  Buffer(const Buffer &other) noexcept;
  Buffer(Buffer &&other) noexcept;
  Buffer &operator=(const Buffer &other) noexcept;
  Buffer &operator=(Buffer &&other) noexcept;
  ~Buffer() noexcept;

  [[nodiscard]] static base::Result<std::size_t> PaddedSize(std::size_t size) noexcept;
  [[nodiscard]] static base::Result<Buffer> Copy(base::MemoryPool &pool,
                                                 std::span<const std::byte> bytes) noexcept;
  [[nodiscard]] base::Result<Buffer> Slice(std::size_t offset, std::size_t length) const noexcept;
  // Borrowed until this owner is destroyed or assigned.
  [[nodiscard]] std::span<const std::byte> bytes() const noexcept;

private:
  friend class Array;
  // Array stores whole buffers and applies any row offset in the ArrowArray.
  [[nodiscard]] const void *copied_data() const noexcept;
  // The uint8_t elements that Arrow consumers index; native validity reads use the same view.
  [[nodiscard]] std::span<const std::uint8_t> uint8_view() const noexcept;

  struct Storage;
  Storage *storage_ = nullptr;
  std::size_t offset_ = 0;
  std::size_t size_ = 0;
};

enum class Type : std::uint8_t { Int64, Utf8, LargeUtf8, Utf8View };
enum class Utf8Append : std::uint8_t { Fits, NewBatch };

// The reader checks each value before building offsets. NewBatch signals it to retry
// the entire row in an empty batch. The reader also validates the value's UTF-8.
[[nodiscard]] base::Result<Utf8Append> CheckUtf8Append(std::size_t bytes_used,
                                                       std::size_t value_size) noexcept;

class Array {
public:
  Array() noexcept = default;
  Array(const Array &) noexcept = default;
  Array(Array &&other) noexcept;
  Array &operator=(const Array &) noexcept = default;
  Array &operator=(Array &&other) noexcept;
  ~Array() noexcept = default;

  // Optional Arrow validity bitmap, least significant bit first, with one bit per row.
  [[nodiscard]] static base::Result<Array>
  Int64(base::MemoryPool &pool, std::span<const std::int64_t> values,
        std::span<const std::uint8_t> validity = {}) noexcept;
  // Input data has already been validated as UTF-8 by the reader.
  [[nodiscard]] static base::Result<Array>
  Utf8(base::MemoryPool &pool, std::span<const std::int32_t> offsets, std::string_view data,
       std::span<const std::uint8_t> validity = {}, Type type = Type::Utf8) noexcept;
  [[nodiscard]] base::Result<Array> Slice(std::int64_t start, std::int64_t length) const noexcept;
  [[nodiscard]] Type type() const noexcept { return type_; }
  [[nodiscard]] std::int64_t length() const noexcept { return length_; }
  [[nodiscard]] std::int64_t null_count() const noexcept { return null_count_; }
  [[nodiscard]] bool is_valid(std::int64_t row) const noexcept;

  // Outputs are caller-owned and must not contain live exports.
  [[nodiscard]] base::Result<void> Export(base::MemoryPool &pool, ArrowArray &data,
                                          ArrowSchema &schema) const noexcept;

private:
  friend class Batch;

  [[nodiscard]] base::Result<void> ExportData(base::MemoryPool &pool,
                                              ArrowArray &out) const noexcept;
  [[nodiscard]] base::Result<void> SetValidity(base::MemoryPool &pool,
                                               std::span<const std::uint8_t> bitmap) noexcept;

  Buffer validity_;
  Buffer values_; // int64 values or UTF-8 offsets
  Buffer utf8_;
  std::int64_t offset_ = 0;
  std::int64_t length_ = 0;
  std::int64_t null_count_ = 0;
  Type type_ = Type::Int64;
};

struct Field {
  std::int32_t id = 0;
  std::string_view name;
  Array array;
};

class Batch {
public:
  Batch(const Batch &) = delete;
  Batch(Batch &&other) noexcept;
  Batch &operator=(const Batch &) = delete;
  Batch &operator=(Batch &&other) noexcept;
  ~Batch() noexcept;

  [[nodiscard]] static base::Result<Batch> Make(base::MemoryPool &pool, std::int64_t length,
                                                std::span<const Field> fields) noexcept;
  [[nodiscard]] std::int64_t length() const noexcept { return length_; }
  // Borrowed until the batch is destroyed or assigned.
  [[nodiscard]] std::span<const Field> fields() const noexcept { return fields_; }
  [[nodiscard]] const Field *Find(std::int32_t field_id) const noexcept;
  // Outputs are caller-owned and must not contain live exports.
  [[nodiscard]] base::Result<void> Export(ArrowArray &data, ArrowSchema &schema) const noexcept;

private:
  explicit Batch(base::MemoryPool &pool, std::int64_t length) noexcept;

  base::PoolPtr pool_;
  void *field_allocation_ = nullptr;
  std::span<Field> fields_;
  void *name_allocation_ = nullptr;
  std::size_t name_bytes_ = 0;
  std::int64_t length_;
};

} // namespace doqe::column

#endif // DOQE_COLUMN_COLUMN_HPP
