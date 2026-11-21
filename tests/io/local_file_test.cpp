#include "../support/fault_pool.hpp"
#include "../support/temp_file.hpp"
#include "doqe/base/arena.hpp"
#include "doqe/base/memory_pool.hpp"
#include "doqe/base/status.hpp"
#include "doqe/io/file_system.hpp"
#include "local_file.hpp"
#include <cstdint>

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#ifdef __APPLE__
#include <sys/fcntl.h>
#else
#include <fcntl.h>
#endif
#include <limits>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <unistd.h>

namespace {

using doqe::base::Code;
using namespace std::string_view_literals;

TEST(LocalFileTest, ReadsStatSizeAndExactRanges) {
  const doqe::test::TempFile file;
  ASSERT_TRUE(file.Write("abcde"));
  const auto backing = doqe::base::MakeFlatPool(8192);
  ASSERT_TRUE(backing);
  auto *fault = new doqe::test::FaultPool(*backing);
  const auto pool = doqe::base::PoolPtr::Adopt(fault);
  doqe::base::Arena arena(pool);
  const doqe::io::LocalFileSystem io;
  const auto capped = io.ReadWhole(file.path(), 4, arena);
  ASSERT_FALSE(capped);
  EXPECT_EQ(capped.error().code(), Code::Unsupported);
  EXPECT_EQ(fault->calls(), 0U);
  EXPECT_EQ(pool->live_bytes(), 0U);
  const auto whole = io.ReadWhole(file.path(), 5, arena);
  ASSERT_TRUE(whole);
  EXPECT_EQ(*whole, "abcde");

  std::array<std::byte, 2> bytes{};
  ASSERT_TRUE(io.ReadRange(file.path(), 2, bytes, arena));
  EXPECT_EQ(bytes, (std::array{std::byte{'c'}, std::byte{'d'}}));
  const auto short_read = io.ReadRange(file.path(), 4, bytes, arena);
  ASSERT_FALSE(short_read);
  EXPECT_EQ(short_read.error().code(), Code::IoError);
  const auto overflow =
      io.ReadRange(file.path(), std::numeric_limits<std::uint64_t>::max(), bytes, arena);
  ASSERT_FALSE(overflow);
  EXPECT_EQ(overflow.error().code(), Code::InvalidArgument);
}

TEST(LocalFileTest, RangeReadClassifiesNonRegularAndMissingPaths) {
  const doqe::test::TempFile file;
  ASSERT_TRUE(file.Write("abc"));
  const auto pool = doqe::base::MakeFlatPool(8192);
  ASSERT_TRUE(pool);
  doqe::base::Arena arena(*pool);
  const doqe::io::LocalFileSystem io;
  std::array<std::byte, 1> bytes{};

  const auto directory = io.ReadRange(file.directory(), 0, bytes, arena);
  ASSERT_FALSE(directory);
  EXPECT_EQ(directory.error().code(), Code::IoError);
  for (const auto &path : {file.path() + ".missing", file.path() + "/child"}) {
    const auto missing = io.ReadRange(path, 0, bytes, arena);
    ASSERT_FALSE(missing) << path;
    EXPECT_EQ(missing.error().code(), Code::NotFound) << path;
  }
}

TEST(LocalFileTest, DetectsGrowthAndShrinkageAfterStat) {
  const doqe::test::TempFile file;
  const auto pool = doqe::base::MakeFlatPool(8192);
  ASSERT_TRUE(pool);
  doqe::base::Arena arena(*pool);
  constexpr std::string_view original = "abc";
  for (const auto replacement : {"a", "abcdef"}) {
    ASSERT_TRUE(file.Write(original));
    // A mode is required only when opening with O_CREAT.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
    const int fd = open(file.path().c_str(), O_RDONLY | O_CLOEXEC);
    ASSERT_GE(fd, 0);
    struct stat info{};
    const int stat_result = fstat(fd, &info);
    const bool written = file.Write(replacement);
    const auto live_before = (*pool)->live_bytes();
    const auto read = doqe::io::ReadStatSize(fd, static_cast<std::size_t>(info.st_size), arena);
    EXPECT_EQ(close(fd), 0);
    ASSERT_EQ(stat_result, 0);
    ASSERT_TRUE(written);
    ASSERT_FALSE(read);
    EXPECT_EQ(read.error().code(), Code::IoError);
    EXPECT_EQ((*pool)->live_bytes(), live_before);
    EXPECT_EQ(read.error().message(), std::string_view(replacement).size() < original.size()
                                          ? "short file read"
                                          : "file grew after stat");
  }

  ASSERT_TRUE(file.Write(""));
  const doqe::io::LocalFileSystem io;
  const auto empty = io.ReadWhole(file.path(), 0, arena);
  ASSERT_TRUE(empty);
  EXPECT_TRUE(empty->empty());
}

TEST(LocalFileTest, ClassifiesLiteralPathsAndFileUris) {
  const doqe::test::TempFile file;
  const doqe::test::TempFile literal("percent%query?fragment#");
  ASSERT_TRUE(file.Write("ok"));
  ASSERT_TRUE(literal.Write("ok"));
  const auto pool = doqe::base::MakeFlatPool(16384);
  ASSERT_TRUE(pool);
  doqe::base::Arena arena(*pool);
  const doqe::io::LocalFileSystem io;
  for (const auto &location : {file.path(), literal.path(), "file://" + file.path(),
                               "FILE://" + file.path(), "file:" + file.path()}) {
    const auto result = io.ReadWhole(location, 2, arena);
    ASSERT_TRUE(result) << location;
    EXPECT_EQ(*result, "ok");
  }
  const auto check = [&](std::string_view location, Code code) {
    const auto result = io.ReadWhole(location, 2, arena);
    ASSERT_FALSE(result) << location;
    EXPECT_EQ(result.error().code(), code) << location;
  };
  for (const auto location :
       {"file:/a%20b", "file:/a%", "file://localhost/p", "file://other/p", "HTTPS://host/p"}) {
    check(location, Code::Unsupported);
  }
  for (const auto location :
       {"file:/a?q", "file:/a#f", "relative", "relative/path:literal", "1:relative", ""}) {
    check(location, Code::InvalidArgument);
  }
  check("file:/a%20b?q", Code::InvalidArgument);
  check("file:/a%20b#f", Code::InvalidArgument);
  check("file:/x\0y"sv, Code::InvalidArgument);
  check(file.path() + ".missing", Code::NotFound);
  check(file.path() + "/child", Code::NotFound);
  check(file.directory(), Code::IoError);
}

} // namespace
