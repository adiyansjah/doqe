#include "doqe/base/status.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <expected>
#include <gtest/gtest.h>
#include <limits>
#include <new>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace {

struct HeapCalls {
  unsigned allocations = 0;
  unsigned deallocations = 0;
  bool fail = false;
};

HeapCalls &AllocationCalls() noexcept {
  static constinit thread_local HeapCalls calls;
  return calls;
}

// Count calls at the global C++ allocation boundary. ASan still instruments the
// underlying malloc, aligned_alloc and free calls.
// The replacement operators own the malloc/free boundary for this executable.
// NOLINTBEGIN(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory)
void *CountedAllocate(std::size_t size, std::size_t alignment = 0) noexcept {
  ++AllocationCalls().allocations;
  if (AllocationCalls().fail) {
    return nullptr;
  }
  size = std::max(size, std::size_t{1});
  if (alignment == 0) {
    return std::malloc(size);
  }
  // aligned_alloc requires a size that is a multiple of the alignment.
  const auto extra = alignment - 1;
  if (size > std::numeric_limits<std::size_t>::max() - extra) {
    return nullptr;
  }
  return std::aligned_alloc(alignment, (size + extra) & ~extra);
}

void *AllocateOrThrow(std::size_t size, std::size_t alignment = 0) {
  for (;;) {
    if (auto *memory = CountedAllocate(size, alignment); memory != nullptr) {
      return memory;
    }
    const auto handler = std::get_new_handler();
    if (handler == nullptr) {
      throw std::bad_alloc{};
    }
    handler();
  }
}

void *AllocateNoThrow(std::size_t size, std::size_t alignment = 0) noexcept {
  try {
    return AllocateOrThrow(size, alignment);
  } catch (...) {
    return nullptr;
  }
}

void CountedFree(void *memory) noexcept {
  if (memory != nullptr) {
    ++AllocationCalls().deallocations;
    std::free(memory);
  }
}

// NOLINTEND(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory)

} // namespace

// NOLINTBEGIN(cppcoreguidelines-owning-memory)
void *operator new(std::size_t size) { return AllocateOrThrow(size); }
void *operator new(std::size_t size, const std::nothrow_t &) noexcept {
  return AllocateNoThrow(size);
}
void operator delete(void *memory) noexcept { CountedFree(memory); }
void operator delete(void *memory, std::size_t) noexcept { CountedFree(memory); }
void operator delete(void *memory, const std::nothrow_t &) noexcept { CountedFree(memory); }
void *operator new(std::size_t size, std::align_val_t alignment) {
  return AllocateOrThrow(size, static_cast<std::size_t>(alignment));
}
void *operator new(std::size_t size, std::align_val_t alignment, const std::nothrow_t &) noexcept {
  return AllocateNoThrow(size, static_cast<std::size_t>(alignment));
}
void operator delete(void *memory, std::align_val_t) noexcept { CountedFree(memory); }
void operator delete(void *memory, std::size_t, std::align_val_t) noexcept { CountedFree(memory); }
void operator delete(void *memory, std::align_val_t, const std::nothrow_t &) noexcept {
  CountedFree(memory);
}

void *operator new[](std::size_t size) { return AllocateOrThrow(size); }
void *operator new[](std::size_t size, const std::nothrow_t &) noexcept {
  return AllocateNoThrow(size);
}
void operator delete[](void *memory) noexcept { CountedFree(memory); }
void operator delete[](void *memory, std::size_t) noexcept { CountedFree(memory); }
void operator delete[](void *memory, const std::nothrow_t &) noexcept { CountedFree(memory); }
void *operator new[](std::size_t size, std::align_val_t alignment) {
  return AllocateOrThrow(size, static_cast<std::size_t>(alignment));
}
void *operator new[](std::size_t size, std::align_val_t alignment,
                     const std::nothrow_t &) noexcept {
  return AllocateNoThrow(size, static_cast<std::size_t>(alignment));
}
void operator delete[](void *memory, std::align_val_t) noexcept { CountedFree(memory); }
void operator delete[](void *memory, std::size_t, std::align_val_t) noexcept {
  CountedFree(memory);
}
void operator delete[](void *memory, std::align_val_t, const std::nothrow_t &) noexcept {
  CountedFree(memory);
}

// NOLINTEND(cppcoreguidelines-owning-memory)

namespace doqe::base {
namespace {

constexpr std::size_t kStatusAbiBytes = 16;
static_assert(sizeof(Status) == kStatusAbiBytes);
static_assert(std::is_same_v<std::underlying_type_t<Code>, std::uint8_t>);
static_assert(std::is_nothrow_constructible_v<Status, Code, std::string_view, std::string_view>);
static_assert(std::is_nothrow_copy_constructible_v<Status>);
static_assert(std::is_nothrow_move_constructible_v<Status>);
static_assert(std::is_nothrow_copy_assignable_v<Status>);
static_assert(std::is_nothrow_move_assignable_v<Status>);
static_assert(std::is_nothrow_destructible_v<Status>);
static_assert(noexcept(Status::OutOfMemory()));
static_assert(noexcept(Fail(Code::Internal)));

TEST(StatusTest, Names) {
  EXPECT_EQ(CodeName(Code::Cancelled), "Cancelled");
  EXPECT_EQ(CodeName(Code::DeadlineExceeded), "DeadlineExceeded");
  EXPECT_EQ(CodeName(Code::ResourceExhausted), "ResourceExhausted");
  EXPECT_EQ(CodeName(Code::OutOfMemory), "OutOfMemory");
  EXPECT_EQ(CodeName(Code::CommitConflict), "CommitConflict");
  EXPECT_EQ(CodeName(Code::CommitStateUnknown), "CommitStateUnknown");
  EXPECT_EQ(CodeName(Code::NotFound), "NotFound");
  EXPECT_EQ(CodeName(Code::AlreadyExists), "AlreadyExists");
  EXPECT_EQ(CodeName(Code::InvalidArgument), "InvalidArgument");
  EXPECT_EQ(CodeName(Code::Unsupported), "Unsupported");
  EXPECT_EQ(CodeName(Code::IoError), "IoError");
  EXPECT_EQ(CodeName(Code::Internal), "Internal");
  // Fixed underlying type permits future codes unknown to this build.
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  EXPECT_EQ(CodeName(static_cast<Code>(255)), "Unknown");
}

TEST(StatusTest, MessageCapKeepsCompleteCodePoints) {
  const std::string prefix(255, 'm');
  const Status status(Code::IoError, prefix + "é", "ctx");
  EXPECT_EQ(status.message(), prefix);
  EXPECT_EQ(status.context(), "c");
  const std::string exact = std::string(254, 'm') + "é";
  EXPECT_EQ(Status(Code::IoError, exact, "ctx").message(), exact);
}

TEST(StatusTest, ContextCapKeepsCompleteCodePoints) {
  const std::string message(253, 'm');
  const Status status(Code::IoError, message, "x🚀");
  EXPECT_EQ(status.message(), message);
  EXPECT_EQ(status.context(), "x");
  EXPECT_EQ(Status(Code::IoError, message, "xé").context(), "xé");
}

TEST(StatusTest, MalformedInputEndsAtLastValidBoundary) {
  for (const auto suffix :
       {"\x80", "\xc0\xaf", "\xe0\x80\xaf", "\xe0\x9f\xbf", "\xed\xa0\x80", "\xf0\x80\x80\xaf",
        "\xf0\x8f\xbf\xbf", "\xf4\x90\x80\x80", "\xf5\x80\x80\x80", "\xf6\x80\x80\x80",
        "\xf7\x80\x80\x80", "\xe2\x82", "\xe2!x"}) {
    const Status status(Code::InvalidArgument, std::string("é") + suffix, "ctx");
    EXPECT_EQ(status.message(), "é");
    EXPECT_EQ(status.context(), "ctx");
  }
  EXPECT_EQ(Status(Code::IoError, "",
                   "a\xff"
                   "b")
                .context(),
            "a");
}

TEST(StatusTest, OwnsDetailAndSharesWithoutAllocating) {
  std::string message = "bad input";
  std::string context = "read";
  const Status original(Code::InvalidArgument, message, context);
  message.clear();
  context.clear();
  const auto before = AllocationCalls();
  auto copied = original;
  const auto moved = std::move(copied);
  Status assigned(Code::Internal);
  assigned = original;
  Status move_assigned(Code::Internal);
  move_assigned = std::move(assigned);
  const auto after = AllocationCalls();
  EXPECT_EQ(moved.message(), "bad input");
  EXPECT_EQ(move_assigned.context(), "read");
  EXPECT_EQ(moved.message().data(), original.message().data());
  EXPECT_EQ(move_assigned.context().data(), original.context().data());
  EXPECT_EQ(after.allocations, before.allocations);
  EXPECT_EQ(after.deallocations, before.deallocations);
}

class StatusAssignmentTest : public ::testing::TestWithParam<bool> {};

TEST_P(StatusAssignmentTest, OverwrittenDetailIsFreedAndIncomingDetailStaysShared) {
  const bool move = GetParam();
  const auto before = AllocationCalls();
  HeapCalls after_assignment;
  HeapCalls after_sharers;
  bool shared = false;
  bool source_correct = false;
  bool original_correct = false;
  {
    const Status original(Code::IoError, "incoming", "read");
    {
      auto incoming = original;
      Status target(Code::Internal, "overwritten");
      if (move) {
        target = std::move(incoming);
      } else {
        target = incoming;
      }
      after_assignment = AllocationCalls();
      shared = target.message().data() == original.message().data() &&
               target.context().data() == original.context().data();
      // Status moves explicitly clear the source's diagnostic detail.
      // NOLINTNEXTLINE(bugprone-use-after-move,clang-analyzer-cplusplus.Move)
      source_correct = incoming.message() == (move ? "" : original.message());
    }
    after_sharers = AllocationCalls();
    original_correct = original.message() == "incoming" && original.context() == "read";
  }
  const auto after_last_owner = AllocationCalls();
  EXPECT_TRUE(shared);
  EXPECT_TRUE(source_correct);
  EXPECT_TRUE(original_correct);
  EXPECT_EQ(after_assignment.allocations - before.allocations, 2U);
  EXPECT_EQ(after_assignment.deallocations - before.deallocations, 1U);
  EXPECT_EQ(after_sharers.deallocations, after_assignment.deallocations);
  EXPECT_EQ(after_last_owner.deallocations - before.deallocations, 2U);
}

INSTANTIATE_TEST_SUITE_P(CopyAndMove, StatusAssignmentTest, ::testing::Bool());

TEST(StatusTest, OomExpectedAndFallback) {
  const auto before = AllocationCalls();
  const Status oom(Code::OutOfMemory, "ignored", "ignored");
  const std::unexpected failure(Status::OutOfMemory());
  const Result<int> value(failure);
  const Result<void> empty(failure);
  const auto after = AllocationCalls();
  ASSERT_FALSE(value);
  ASSERT_FALSE(empty);
  EXPECT_EQ(value.error().code(), Code::OutOfMemory);
  EXPECT_EQ(empty.error().code(), Code::OutOfMemory);
  EXPECT_TRUE(oom.message().empty());
  EXPECT_TRUE(oom.context().empty());
  EXPECT_EQ(after.allocations, before.allocations);
  EXPECT_EQ(after.deallocations, before.deallocations);

  const auto before_fallback = AllocationCalls();
  AllocationCalls().fail = true;
  const Status fallback(Code::IoError, "detail unavailable", "read");
  AllocationCalls().fail = false;
  EXPECT_EQ(AllocationCalls().allocations - before_fallback.allocations, 1U);
  EXPECT_EQ(fallback.code(), Code::IoError);
  EXPECT_TRUE(fallback.message().empty());
  EXPECT_TRUE(fallback.context().empty());
}

} // namespace
} // namespace doqe::base
