#include "doqe/base/arena.hpp"
#include "doqe/base/memory_pool.hpp"
#include "doqe/base/status.hpp"
#include "doqe/format/json.hpp"
#include <cstddef>

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <string>
#include <string_view>

namespace {

// Tests use literal boundary values and index fixture spans after size assertions.
// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers,cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)

using doqe::base::Code;
using doqe::format::ParseJson;

TEST(JsonTest, ExactIntegersAndUnicodeStrings) {
  const auto pool = doqe::base::MakeFlatPool(65536);
  ASSERT_TRUE(pool);
  doqe::base::Arena arena(*pool);
  const auto json = ParseJson(
      R"({"n":[-9223372036854775808,9223372036854775807,9007199254740993,9223372036854775808,-9223372036854775809,1.0,1e0],"\u006b":"a\u0000\uD834\uDD1E\n"})",
      arena);
  ASSERT_TRUE(json);
  const auto *numbers = (*json)->Find("n");
  ASSERT_NE(numbers, nullptr);
  const auto entries = numbers->elements();
  ASSERT_EQ(entries.size(), 7U);
  EXPECT_EQ(entries[0]->AsInt64(), std::numeric_limits<std::int64_t>::min());
  EXPECT_EQ(entries[1]->AsInt64(), std::numeric_limits<std::int64_t>::max());
  EXPECT_EQ(entries[2]->AsInt64(), 9007199254740993LL);
  for (const auto *element : entries.subspan(3)) {
    const auto value = element->AsInt64();
    ASSERT_FALSE(value);
    EXPECT_EQ(value.error().code(), Code::InvalidArgument);
  }
  const auto *key = (*json)->Find("k");
  ASSERT_NE(key, nullptr);
  EXPECT_EQ(key->text(), std::string_view("a\0\xf0\x9d\x84\x9e\n", 7));
}

TEST(JsonTest, RejectsMalformedDocumentsAndDecodedDuplicateKeys) {
  const auto pool = doqe::base::MakeFlatPool(65536);
  ASSERT_TRUE(pool);
  doqe::base::Arena arena(*pool);
  for (const std::string_view input :
       {"", "null 0", "[0,]", "{\"a\":1,}", "01", "1e+", R"("\q")", R"("\uDC00")",
        R"("\uD800\u0041")", R"("\uD800")", "\"\xed\xa0\x80\"", "\"\xc0\x80\"",
        "\"\xf4\x90\x80\x80\"", "\"\x01\"", "[", R"({"a":0,"\u0061":1})"}) {
    const auto result = ParseJson(input, arena);
    ASSERT_FALSE(result) << input;
    EXPECT_EQ(result.error().code(), Code::InvalidArgument) << input;
  }
}

TEST(JsonTest, FailedParseRestoresChargesAndPreservesEarlierValues) {
  const auto pool = doqe::base::MakeFlatPool(262144);
  ASSERT_TRUE(pool);
  doqe::base::Arena arena(*pool);
  const auto prior = ParseJson(R"("kept")", arena);
  ASSERT_TRUE(prior);
  const auto live_before = (*pool)->live_bytes();
  const std::string input = "[\"" + std::string(70000, 'a') + "\",]";
  const auto failed = ParseJson(input, arena);
  ASSERT_FALSE(failed);
  EXPECT_EQ(failed.error().code(), Code::InvalidArgument);
  EXPECT_EQ((*pool)->live_bytes(), live_before);
  EXPECT_EQ((*prior)->text(), "kept");
}

TEST(JsonTest, ExactSizeAndDepthLimits) {
  const auto pool = doqe::base::MakeFlatPool(65536);
  ASSERT_TRUE(pool);
  doqe::base::Arena arena(*pool);
  constexpr std::size_t size_limit = std::size_t{16} * 1024U * 1024U;
  std::string input(size_limit - 4, ' ');
  input += "null";
  ASSERT_TRUE(ParseJson(input, arena));
  input += ' ';
  const auto too_big = ParseJson(input, arena);
  ASSERT_FALSE(too_big);
  EXPECT_EQ(too_big.error().code(), Code::Unsupported);

  const std::string depth = std::string(64, '[') + "null" + std::string(64, ']');
  ASSERT_TRUE(ParseJson(depth, arena));
  const auto too_deep = ParseJson("[" + depth + "]", arena);
  ASSERT_FALSE(too_deep);
  EXPECT_EQ(too_deep.error().code(), Code::Unsupported);
}

TEST(JsonTest, LargeObjectAndDuplicateNearEnd) {
  const auto pool = doqe::base::MakeFlatPool(std::size_t{24} * 1024U * 1024U);
  ASSERT_TRUE(pool);
  doqe::base::Arena arena(*pool);
  std::string input = "{";
  for (int index = 0; index < 20000; ++index) {
    input += "\"key" + std::to_string(index) + "\":0,";
  }
  const auto json = ParseJson(input + "\"last\":1}", arena);
  ASSERT_TRUE(json);
  EXPECT_NE((*json)->Find("key19999"), nullptr);
  const auto duplicate = ParseJson(input + R"("\u006bey19999":1})", arena);
  ASSERT_FALSE(duplicate);
  EXPECT_EQ(duplicate.error().code(), Code::InvalidArgument);
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers,cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)

} // namespace
