#ifndef DOQE_BASE_STATUS_HPP
#define DOQE_BASE_STATUS_HPP

#include <cstdint>
#include <expected>
#include <string_view>

namespace doqe::base {

enum class Code : std::uint8_t {
  Cancelled,
  DeadlineExceeded,
  ResourceExhausted,
  OutOfMemory,
  CommitConflict,
  CommitStateUnknown,
  NotFound,
  AlreadyExists,
  InvalidArgument,
  Unsupported,
  IoError,
  Internal,
};

[[nodiscard]] std::string_view CodeName(Code code) noexcept;

class Status {
public:
  explicit Status(Code code, std::string_view message = {}, std::string_view context = {}) noexcept;
  Status(const Status &other) noexcept;
  Status(Status &&other) noexcept;
  Status &operator=(const Status &other) noexcept;
  Status &operator=(Status &&other) noexcept;
  ~Status() noexcept;

  [[nodiscard]] static Status OutOfMemory() noexcept;
  [[nodiscard]] Code code() const noexcept { return code_; }
  // These views borrow the detail owned by this Status.
  [[nodiscard]] std::string_view message() const noexcept;
  [[nodiscard]] std::string_view context() const noexcept;

private:
  struct Detail;
  Detail *detail_ = nullptr;
  Code code_;
};

static_assert(sizeof(Status) == 2 * sizeof(void *));

template <class T> using Result = std::expected<T, Status>;

[[nodiscard]] inline std::unexpected<Status> Fail(Code code, std::string_view message = {},
                                                  std::string_view context = {}) noexcept {
  return std::unexpected(Status(code, message, context));
}

} // namespace doqe::base

#endif // DOQE_BASE_STATUS_HPP
