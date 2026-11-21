#ifndef DOQE_IO_FILE_SYSTEM_HPP
#define DOQE_IO_FILE_SYSTEM_HPP

#include "doqe/base/arena.hpp"
#include "doqe/base/status.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace doqe::io {

class FileSystem {
public:
  FileSystem() = default;
  FileSystem(const FileSystem &) = delete;
  FileSystem &operator=(const FileSystem &) = delete;
  FileSystem(FileSystem &&) = delete;
  FileSystem &operator=(FileSystem &&) = delete;
  virtual ~FileSystem() = default;
  // A successful whole-file view borrows storage from arena.
  [[nodiscard]] virtual base::Result<std::string_view>
  ReadWhole(std::string_view location, std::size_t cap, base::Arena &arena) const noexcept = 0;
  [[nodiscard]] virtual base::Result<void> ReadRange(std::string_view location,
                                                     std::uint64_t offset,
                                                     std::span<std::byte> output,
                                                     base::Arena &arena) const noexcept = 0;
};

class LocalFileSystem final : public FileSystem {
public:
  [[nodiscard]] base::Result<std::string_view>
  ReadWhole(std::string_view location, std::size_t cap, base::Arena &arena) const noexcept override;
  [[nodiscard]] base::Result<void> ReadRange(std::string_view location, std::uint64_t offset,
                                             std::span<std::byte> output,
                                             base::Arena &arena) const noexcept override;
};

} // namespace doqe::io

#endif // DOQE_IO_FILE_SYSTEM_HPP
