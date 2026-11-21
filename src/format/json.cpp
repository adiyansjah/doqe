#include "doqe/format/json.hpp"

#include "doqe/base/arena.hpp"
#include "doqe/base/status.hpp"
#include "doqe/base/utf8.hpp"

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <span>
#include <string_view>
#include <system_error>

namespace doqe::format {
namespace {

using base::Code;
using base::Fail;
using base::Result;
using Kind = JsonValue::Kind;

[[nodiscard]] std::unexpected<base::Status> InvalidJson() noexcept {
  return Fail(Code::InvalidArgument, "invalid JSON");
}

constexpr bool Digit(char input) noexcept { return input >= '0' && input <= '9'; }

constexpr int Hex(char input) noexcept {
  if (Digit(input)) {
    return input - '0';
  }
  constexpr int first_hex_letter = 10;
  if (input >= 'a' && input <= 'f') {
    return input - 'a' + first_hex_letter;
  }
  if (input >= 'A' && input <= 'F') {
    return input - 'A' + first_hex_letter;
  }
  return -1;
}

void Emit(std::span<char> &output, char byte) noexcept {
  output.front() = byte;
  output = output.subspan(1);
}

// A Unicode escape supplies at least six bytes of output capacity.
// Keep the encoding masks and code-point boundaries in their standard notation.
// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers)
void EncodeUtf8(std::uint32_t point, std::span<char> &output) noexcept {
  const auto continuation = [](std::uint32_t payload) {
    return static_cast<char>(0x80U | (payload & 0x3fU));
  };
  if (point <= 0x7fU) {
    Emit(output, static_cast<char>(point));
    return;
  }
  if (point <= 0x7ffU) {
    Emit(output, static_cast<char>(0xc0U | (point >> 6U)));
  } else if (point <= 0xffffU) {
    Emit(output, static_cast<char>(0xe0U | (point >> 12U)));
    Emit(output, continuation(point >> 6U));
  } else {
    Emit(output, static_cast<char>(0xf0U | (point >> 18U)));
    Emit(output, continuation(point >> 12U));
    Emit(output, continuation(point >> 6U));
  }
  Emit(output, continuation(point));
}
// NOLINTEND(cppcoreguidelines-avoid-magic-numbers)

template <class T> struct Entry {
  T value{};
  Entry *next = nullptr;
};

// Keep a temporary list in the arena and allocate the final contiguous view once.
template <class T> class Collection {
public:
  [[nodiscard]] Result<void> Append(base::Arena &arena, T value) noexcept {
    auto entry = arena.MakeArray<Entry<T>>(1);
    if (!entry) {
      return std::unexpected(entry.error());
    }
    entry->front().value = value;
    *tail_ = &entry->front();
    tail_ = &entry->front().next;
    ++size_;
    return {};
  }

  [[nodiscard]] Result<std::span<T>> Flatten(base::Arena &arena) const noexcept {
    auto entries = arena.MakeArray<T>(size_);
    if (!entries) {
      return std::unexpected(entries.error());
    }
    const auto *entry = first_;
    for (auto &value : *entries) {
      value = entry->value;
      entry = entry->next;
    }
    return entries;
  }

private:
  Entry<T> *first_ = nullptr;
  Entry<T> **tail_ = &first_;
  std::size_t size_ = 0;
};

} // namespace

class JsonParser {
public:
  JsonParser(std::string_view input, base::Arena &arena) noexcept : input_(input), arena_(&arena) {}

  [[nodiscard]] Result<const JsonValue *> Parse() noexcept {
    auto root = Value(0);
    if (!root) {
      return std::unexpected(root.error());
    }
    Whitespace();
    if (!input_.empty()) {
      return InvalidJson();
    }
    return *root;
  }

private:
  [[nodiscard]] char Peek() const noexcept { return input_.empty() ? '\0' : input_.front(); }

  bool Consume(char expected) noexcept {
    if (input_.empty() || input_.front() != expected) {
      return false;
    }
    input_.remove_prefix(1);
    return true;
  }

  void Whitespace() noexcept {
    while (!input_.empty()) {
      const auto byte = input_.front();
      if (byte != ' ' && byte != '\t' && byte != '\r' && byte != '\n') {
        return;
      }
      input_.remove_prefix(1);
    }
  }

  // Container recursion is bounded by json_depth_limit.
  // NOLINTNEXTLINE(misc-no-recursion)
  [[nodiscard]] Result<JsonValue *> Value(std::size_t depth) noexcept {
    Whitespace();
    auto storage = arena_->MakeArray<JsonValue>(1);
    if (!storage) {
      return std::unexpected(storage.error());
    }
    auto &value = storage->front();
    Result<void> parsed;
    switch (Peek()) {
    case '[':
    case '{':
      if (depth == json_depth_limit) {
        return Fail(Code::Unsupported, "JSON exceeds nesting limit");
      }
      parsed = Peek() == '[' ? Array(value, depth + 1) : Object(value, depth + 1);
      break;
    case 'n':
      parsed = Literal("null");
      break;
    case 't':
      value.kind_ = Kind::Boolean;
      value.boolean_ = true;
      parsed = Literal("true");
      break;
    case 'f':
      value.kind_ = Kind::Boolean;
      parsed = Literal("false");
      break;
    case '"':
      parsed = StoreText(value, Kind::String, String());
      break;
    default:
      parsed = StoreText(value, Kind::Number, Number());
      break;
    }
    if (!parsed) {
      return std::unexpected(parsed.error());
    }
    return &value;
  }

  [[nodiscard]] static Result<void> StoreText(JsonValue &value, Kind kind,
                                              Result<std::string_view> text) noexcept {
    if (!text) {
      return std::unexpected(text.error());
    }
    value.kind_ = kind;
    value.text_ = *text;
    return {};
  }

  [[nodiscard]] Result<void> Literal(std::string_view literal) noexcept {
    if (!input_.starts_with(literal)) {
      return InvalidJson();
    }
    input_.remove_prefix(literal.size());
    return {};
  }

  // Container recursion is bounded by json_depth_limit.
  // NOLINTNEXTLINE(misc-no-recursion)
  [[nodiscard]] Result<void> Array(JsonValue &value, std::size_t depth) noexcept {
    input_.remove_prefix(1);
    value.kind_ = Kind::Array;
    Whitespace();
    if (Consume(']')) {
      return {};
    }
    Collection<const JsonValue *> list;
    while (true) {
      const auto child = Value(depth);
      if (!child) {
        return std::unexpected(child.error());
      }
      const auto added = list.Append(*arena_, *child);
      if (!added) {
        return added;
      }
      Whitespace();
      if (!Consume(',')) {
        break;
      }
    }
    if (!Consume(']')) {
      return InvalidJson();
    }
    const auto elements = list.Flatten(*arena_);
    if (!elements) {
      return std::unexpected(elements.error());
    }
    value.elements_ = *elements;
    return {};
  }

  // Container recursion is bounded by json_depth_limit.
  // NOLINTNEXTLINE(misc-no-recursion)
  [[nodiscard]] Result<void> Object(JsonValue &value, std::size_t depth) noexcept {
    input_.remove_prefix(1);
    value.kind_ = Kind::Object;
    Whitespace();
    if (Consume('}')) {
      return {};
    }
    Collection<JsonMember> list;
    while (true) {
      Whitespace();
      const auto key = String();
      if (!key) {
        return std::unexpected(key.error());
      }
      Whitespace();
      if (!Consume(':')) {
        return InvalidJson();
      }
      const auto child = Value(depth);
      if (!child) {
        return std::unexpected(child.error());
      }
      const auto added = list.Append(*arena_, JsonMember{.key = *key, .value = *child});
      if (!added) {
        return added;
      }
      Whitespace();
      if (!Consume(',')) {
        break;
      }
    }
    if (!Consume('}')) {
      return InvalidJson();
    }
    const auto members = list.Flatten(*arena_);
    if (!members) {
      return std::unexpected(members.error());
    }
    std::ranges::sort(*members, {}, &JsonMember::key);
    if (std::ranges::adjacent_find(*members, {}, &JsonMember::key) != members->end()) {
      return Fail(Code::InvalidArgument, "duplicate JSON object key");
    }
    value.members_ = *members;
    return {};
  }

  [[nodiscard]] Result<std::string_view> Number() noexcept {
    auto lexeme = input_;
    Consume('-');
    if (!Consume('0')) {
      if (!Digit(Peek())) {
        return InvalidJson();
      }
      Digits();
    }
    if (Consume('.')) {
      if (!Digit(Peek())) {
        return InvalidJson();
      }
      Digits();
    }
    if (Consume('e') || Consume('E')) {
      if (!Consume('+')) {
        Consume('-');
      }
      if (!Digit(Peek())) {
        return InvalidJson();
      }
      Digits();
    }
    lexeme.remove_suffix(input_.size());
    return CopyText(lexeme);
  }

  void Digits() noexcept {
    while (Digit(Peek())) {
      input_.remove_prefix(1);
    }
  }

  [[nodiscard]] Result<std::string_view> CopyText(std::string_view text) noexcept {
    if (text.empty()) {
      return std::string_view{};
    }
    auto chars = arena_->MakeArray<char>(text.size());
    if (!chars) {
      return std::unexpected(chars.error());
    }
    std::ranges::copy(text, chars->begin());
    return std::string_view(chars->data(), chars->size());
  }

  [[nodiscard]] Result<std::string_view> String() noexcept {
    if (!Consume('"')) {
      return InvalidJson();
    }
    bool escaped = false;
    auto remaining = input_;
    while (!remaining.empty() && remaining.front() != '"') {
      const char current = remaining.front();
      remaining.remove_prefix(1);
      constexpr unsigned char first_printable = 0x20;
      if (static_cast<unsigned char>(current) < first_printable) {
        return InvalidJson();
      }
      if (current == '\\') {
        if (remaining.empty()) {
          return InvalidJson();
        }
        escaped = true;
        remaining.remove_prefix(1);
      }
    }
    if (remaining.empty()) {
      return InvalidJson();
    }
    auto encoded = input_;
    encoded.remove_suffix(remaining.size());
    input_ = remaining;
    input_.remove_prefix(1);
    return escaped ? Decode(encoded) : CopyText(encoded);
  }

  [[nodiscard]] static Result<std::uint32_t> CodeUnit(std::string_view &encoded) noexcept {
    constexpr std::size_t digits_per_unit = 4;
    constexpr unsigned bits_per_digit = 4;
    if (encoded.size() < digits_per_unit) {
      return InvalidJson();
    }
    std::uint32_t point = 0;
    auto digits = encoded;
    digits.remove_suffix(encoded.size() - digits_per_unit);
    for (const char digit : digits) {
      const auto value = Hex(digit);
      if (value < 0) {
        return InvalidJson();
      }
      point = (point << bits_per_digit) | static_cast<std::uint32_t>(value);
    }
    encoded.remove_prefix(digits_per_unit);
    return point;
  }

  [[nodiscard]] static Result<std::uint32_t> CodePoint(std::string_view &encoded) noexcept {
    const auto first = CodeUnit(encoded);
    if (!first) {
      return std::unexpected(first.error());
    }
    constexpr std::uint32_t high_start = 0xd800;
    constexpr std::uint32_t low_start = 0xdc00;
    constexpr std::uint32_t low_end = 0xdfff;
    constexpr std::uint32_t supplementary_start = 0x10000;
    constexpr unsigned surrogate_payload_bits = 10;
    if (*first < high_start || *first > low_end) {
      return *first;
    }
    if (*first >= low_start || !encoded.starts_with("\\u")) {
      return InvalidJson();
    }
    encoded.remove_prefix(2);
    const auto second = CodeUnit(encoded);
    if (!second || *second < low_start || *second > low_end) {
      return InvalidJson();
    }
    return supplementary_start + ((*first - high_start) << surrogate_payload_bits) +
           (*second - low_start);
  }

  [[nodiscard]] Result<std::string_view> Decode(std::string_view encoded) noexcept {
    auto output = arena_->MakeArray<char>(encoded.size());
    if (!output) {
      return std::unexpected(output.error());
    }
    auto remaining = *output;
    while (!encoded.empty()) {
      char current = encoded.front();
      encoded.remove_prefix(1);
      if (current != '\\') {
        Emit(remaining, current);
        continue;
      }
      current = encoded.front();
      encoded.remove_prefix(1);
      if (current == 'u') {
        const auto point = CodePoint(encoded);
        if (!point) {
          return std::unexpected(point.error());
        }
        EncodeUtf8(*point, remaining);
        continue;
      }
      constexpr std::string_view escapes = "\"\\/bfnrt";
      constexpr std::string_view decoded = "\"\\/\b\f\n\r\t";
      const auto index = escapes.find(current);
      if (index == std::string_view::npos) {
        return InvalidJson();
      }
      static_assert(escapes.size() == decoded.size());
      // The index came from the equally sized escape alphabet.
      // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
      Emit(remaining, decoded[index]);
    }
    return std::string_view(output->data(), output->size() - remaining.size());
  }

  std::string_view input_;
  base::Arena *arena_;
};

const JsonValue *JsonValue::Find(std::string_view key) const noexcept {
  const auto found = std::ranges::lower_bound(members_, key, {}, &JsonMember::key);
  return found != members_.end() && found->key == key ? found->value : nullptr;
}

Result<std::int64_t> JsonValue::AsInt64() const noexcept {
  if (kind_ != Kind::Number) {
    return Fail(Code::InvalidArgument, "expected JSON integer");
  }
  std::int64_t value = 0;
  const auto *end = std::to_address(text_.end());
  const auto parsed = std::from_chars(std::to_address(text_.begin()), end, value);
  if (parsed.ec != std::errc{} || parsed.ptr != end) {
    return Fail(Code::InvalidArgument, "expected int64 JSON integer");
  }
  return value;
}

Result<const JsonValue *> ParseJson(std::string_view input, base::Arena &arena) noexcept {
  if (input.size() > json_size_limit) {
    return Fail(Code::Unsupported, "JSON exceeds size limit");
  }
  if (base::Utf8Prefix(input, input.size()) != input.size()) {
    return Fail(Code::InvalidArgument, "invalid UTF-8 in JSON");
  }
  base::Arena::Savepoint savepoint(arena);
  const auto result = JsonParser(input, arena).Parse();
  if (result) {
    savepoint.Commit();
  }
  return result;
}

} // namespace doqe::format
