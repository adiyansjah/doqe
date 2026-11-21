#include "doqe/base/arena.hpp"
#include "doqe/base/memory_pool.hpp"
#include "doqe/format/json.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string_view>

namespace {

// The parser bounds container depth before publishing a DOM.
// NOLINTNEXTLINE(misc-no-recursion)
void CheckDom(const doqe::format::JsonValue &value) noexcept {
  using Kind = doqe::format::JsonValue::Kind;
  if (value.kind() == Kind::Array) {
    for (const auto *element : value.elements()) {
      CheckDom(*element);
    }
  }
  if (value.kind() == Kind::Object) {
    for (const auto &member : value.members()) {
      if (value.Find(member.key) != member.value) {
        std::abort();
      }
      CheckDom(*member.value);
    }
  }
  if (value.kind() == Kind::Number) {
    const auto integer = value.AsInt64();
    if (integer && value.text().find_first_of(".eE") != std::string_view::npos) {
      std::abort();
    }
  }
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size) {
  constexpr std::size_t query_budget = std::size_t{32} * 1024U * 1024U;
  static const auto pool = doqe::base::MakeFlatPool(query_budget);
  if (!pool) {
    std::abort();
  }
  {
    // libFuzzer provides the input as bytes, and char can alias their representation.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    const std::string_view input(reinterpret_cast<const char *>(data), size);
    doqe::base::Arena arena(*pool);
    const auto result = doqe::format::ParseJson(input, arena);
    if (result) {
      CheckDom(**result);
    }
  }
  if ((*pool)->live_bytes() != 0) {
    std::abort();
  }
  return 0;
}
