#ifndef DOQE_BASE_UTF8_HPP
#define DOQE_BASE_UTF8_HPP

#include <cstddef>
#include <string_view>

namespace doqe::base {

// Return the byte length of the longest valid UTF-8 prefix within the byte limit.
[[nodiscard]] std::size_t Utf8Prefix(std::string_view text, std::size_t limit) noexcept;

} // namespace doqe::base

#endif // DOQE_BASE_UTF8_HPP
