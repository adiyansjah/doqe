#include "local_file.hpp"

#include "doqe/base/arena.hpp"
#include "doqe/base/status.hpp"
#include "doqe/io/file_system.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string_view>
#ifdef __APPLE__
#include <sys/fcntl.h>
#include <sys/syslimits.h>
#else
#include <climits>
#include <fcntl.h>
#endif
#include <limits>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>

namespace doqe::io {
namespace {

using base::Code;
using base::Fail;
using base::Result;
using FileOffset = decltype(std::declval<struct stat>().st_size);

class Descriptor {
public:
  explicit Descriptor(int fd) noexcept : fd_(fd) {}
  Descriptor(const Descriptor &) = delete;
  Descriptor &operator=(const Descriptor &) = delete;
  Descriptor(Descriptor &&other) noexcept : fd_(std::exchange(other.fd_, -1)) {}
  Descriptor &operator=(Descriptor &&) = delete;
  ~Descriptor() {
    if (fd_ >= 0) {
      close(fd_);
    }
  }
  [[nodiscard]] int get() const noexcept { return fd_; }

private:
  int fd_;
};

constexpr bool IsAlpha(char byte) noexcept {
  return (byte >= 'A' && byte <= 'Z') || (byte >= 'a' && byte <= 'z');
}

bool IsScheme(std::string_view text) noexcept {
  if (text.empty() || !IsAlpha(text.front())) {
    return false;
  }
  text.remove_prefix(1);
  return std::ranges::all_of(text, [](char byte) {
    return IsAlpha(byte) || (byte >= '0' && byte <= '9') || byte == '+' || byte == '-' ||
           byte == '.';
  });
}

[[nodiscard]] Result<std::string_view> LocalPath(std::string_view location) noexcept {
  if (location.find('\0') != std::string_view::npos) {
    return Fail(Code::InvalidArgument, "location contains NUL");
  }
  if (location.starts_with('/')) {
    return location;
  }
  const auto colon = location.find(':');
  if (colon == std::string_view::npos) {
    return Fail(Code::InvalidArgument, "local path must be absolute");
  }
  auto scheme = location;
  scheme.remove_suffix(location.size() - colon);
  if (IsScheme(scheme)) {
    if (!std::ranges::equal(scheme, std::string_view("file"), [](char byte, char lower) {
          return byte == lower || byte == lower - ('a' - 'A');
        })) {
      return Fail(Code::Unsupported, "only file URIs are supported");
    }
    location.remove_prefix(colon + 1);
    if (location.find_first_of("?#") != std::string_view::npos) {
      return Fail(Code::InvalidArgument, "file URI contains a query or fragment");
    }
    if (location.find('%') != std::string_view::npos) {
      return Fail(Code::Unsupported, "escaped file URIs are unsupported");
    }
    if (location.starts_with("//")) {
      location.remove_prefix(2);
      if (!location.empty() && !location.starts_with('/')) {
        return Fail(Code::Unsupported, "file URI authority is unsupported");
      }
    }
    if (location.starts_with('/')) {
      return location;
    }
  }
  return Fail(Code::InvalidArgument, "local path must be absolute");
}

[[nodiscard]] Result<Descriptor> Open(std::string_view location) noexcept {
  const auto path = LocalPath(location);
  if (!path) {
    return std::unexpected(path.error());
  }
  // The OS path argument needs a terminator. Keep this syscall buffer on the stack
  // so the file-size cap is checked before allocating any reader storage.
  std::array<char, PATH_MAX> terminated{};
  if (path->size() >= terminated.size()) {
    return Fail(Code::IoError, "local path exceeds OS limit", location);
  }
  std::ranges::copy(*path, terminated.begin());
  int fd = -1;
  while (true) {
    // A mode is required only when opening with O_CREAT.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
    fd = open(terminated.data(), O_RDONLY | O_CLOEXEC | O_NONBLOCK);
    if (fd >= 0 || errno != EINTR) {
      break;
    }
  }
  if (fd < 0) {
    const auto error = errno;
    return Fail(error == ENOENT || error == ENOTDIR ? Code::NotFound : Code::IoError,
                "cannot open file", location);
  }
  return Descriptor(fd);
}

[[nodiscard]] Result<std::uint64_t> StatSize(int fd) noexcept {
  struct stat info{};
  if (fstat(fd, &info) < 0 || !S_ISREG(info.st_mode) || info.st_size < 0) {
    return Fail(Code::IoError, "cannot stat regular file");
  }
  return static_cast<std::uint64_t>(info.st_size);
}

[[nodiscard]] Result<std::size_t> ReadSome(int fd, std::uint64_t offset,
                                           std::span<std::byte> output) noexcept {
  const auto amount =
      std::min(output.size(), static_cast<std::size_t>(std::numeric_limits<int>::max()));
  while (true) {
    const auto size = pread(fd, output.data(), amount, static_cast<FileOffset>(offset));
    if (size >= 0) {
      return static_cast<std::size_t>(size);
    }
    if (errno != EINTR) {
      return Fail(Code::IoError, "file read failed");
    }
  }
}

[[nodiscard]] Result<void> ReadExact(int fd, std::uint64_t offset,
                                     std::span<std::byte> output) noexcept {
  while (!output.empty()) {
    const auto count = ReadSome(fd, offset, output);
    if (!count) {
      return std::unexpected(count.error());
    }
    if (*count == 0) {
      return Fail(Code::IoError, "short file read");
    }
    offset += *count;
    output = output.subspan(*count);
  }
  return {};
}

} // namespace

Result<std::string_view> ReadStatSize(int fd, std::size_t size, base::Arena &arena) noexcept {
  base::Arena::Savepoint savepoint(arena);
  const auto storage = arena.Allocate(size, alignof(char));
  if (!storage) {
    return std::unexpected(storage.error());
  }
  const auto bytes = std::span(static_cast<std::byte *>(*storage), size);
  const auto read = ReadExact(fd, 0, bytes);
  if (!read) {
    return std::unexpected(read.error());
  }
  std::array<std::byte, 1> probe{};
  const auto extra = ReadSome(fd, size, probe);
  if (!extra) {
    return std::unexpected(extra.error());
  }
  if (*extra != 0) {
    return Fail(Code::IoError, "file grew after stat");
  }
  savepoint.Commit();
  if (bytes.empty()) {
    return std::string_view{};
  }
  return std::string_view(static_cast<const char *>(*storage), size);
}

Result<std::string_view> LocalFileSystem::ReadWhole(std::string_view location, std::size_t cap,
                                                    base::Arena &arena) const noexcept {
  const auto fd = Open(location);
  if (!fd) {
    return std::unexpected(fd.error());
  }
  const auto size = StatSize(fd->get());
  if (!size) {
    return std::unexpected(size.error());
  }
  if (*size > cap) {
    return Fail(Code::Unsupported, "file exceeds read limit", location);
  }
  return ReadStatSize(fd->get(), static_cast<std::size_t>(*size), arena);
}

Result<void> LocalFileSystem::ReadRange(std::string_view location, std::uint64_t offset,
                                        std::span<std::byte> output, base::Arena &) const noexcept {
  constexpr auto limit = static_cast<std::uint64_t>(std::numeric_limits<FileOffset>::max());
  if (offset > limit || output.size() > limit - offset) {
    return Fail(Code::InvalidArgument, "file range overflows offset", location);
  }
  const auto fd = Open(location);
  if (!fd) {
    return std::unexpected(fd.error());
  }
  const auto size = StatSize(fd->get());
  if (!size) {
    return std::unexpected(size.error());
  }
  if (offset + output.size() > *size) {
    return Fail(Code::IoError, "file range exceeds file size", location);
  }
  return ReadExact(fd->get(), offset, output);
}

} // namespace doqe::io
