#ifndef DOQE_FORMAT_JSON_HPP
#define DOQE_FORMAT_JSON_HPP

#include "doqe/base/arena.hpp"
#include "doqe/base/status.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace doqe::format {

inline constexpr std::size_t json_size_limit = std::size_t{16} * 1024U * 1024U;
inline constexpr std::size_t json_depth_limit = 64;

class JsonParser;
class JsonValue;

struct JsonMember {
  std::string_view key;
  const JsonValue *value = nullptr;
};

class JsonValue {
public:
  enum class Kind : std::uint8_t { Null, Boolean, Number, String, Array, Object };

  [[nodiscard]] Kind kind() const noexcept { return kind_; }
  // String values and original number lexemes.
  [[nodiscard]] std::string_view text() const noexcept { return text_; }
  [[nodiscard]] bool boolean() const noexcept { return boolean_; }
  [[nodiscard]] std::span<const JsonValue *const> elements() const noexcept { return elements_; }
  [[nodiscard]] std::span<const JsonMember> members() const noexcept { return members_; }
  [[nodiscard]] const JsonValue *Find(std::string_view key) const noexcept;
  [[nodiscard]] base::Result<std::int64_t> AsInt64() const noexcept;

private:
  friend class JsonParser;

  Kind kind_ = Kind::Null;
  bool boolean_ = false;
  std::string_view text_;
  std::span<const JsonValue *const> elements_;
  std::span<const JsonMember> members_;
};

// Values and text borrow the arena and survive release of the original input.
[[nodiscard]] base::Result<const JsonValue *> ParseJson(std::string_view input,
                                                        base::Arena &arena) noexcept;

} // namespace doqe::format

#endif // DOQE_FORMAT_JSON_HPP
