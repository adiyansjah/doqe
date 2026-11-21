#include "doqe/base/memory_pool.hpp"
#include "doqe/base/status.hpp"
#include "test_probes.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <latch>
#include <thread>
#include <utility>

namespace doqe::base {
namespace {

// Tests use concrete sizes and inspect the allocator address alignment.
// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers,cppcoreguidelines-pro-type-reinterpret-cast)
TEST(MemoryPoolTest, LimitAlignmentAndZero) {
  auto owner = MakeFlatPool(64);
  ASSERT_TRUE(owner);
  auto &pool = **owner;
  EXPECT_EQ(pool.limit(), 64U);
  for (const auto alignment : std::array<std::size_t, 7>{1, 2, 4, 8, 16, 32, 64}) {
    const auto zero = pool.Allocate(0, alignment);
    ASSERT_TRUE(zero);
    EXPECT_EQ(*zero, nullptr);
    pool.Free(nullptr, 0, alignment);
    const auto exact = pool.Allocate(64, alignment);
    ASSERT_TRUE(exact);
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(*exact) % alignment, 0U);
    EXPECT_EQ(pool.live_bytes(), 64U);
    const auto over = pool.Allocate(1, alignment);
    ASSERT_FALSE(over);
    EXPECT_EQ(over.error().code(), Code::ResourceExhausted);
    pool.Free(*exact, 64, alignment);
    EXPECT_EQ(pool.live_bytes(), 0U);
  }
  for (const auto bad : std::array<std::size_t, 4>{0, 3, 63, 128}) {
    const auto invalid = pool.Allocate(0, bad);
    ASSERT_FALSE(invalid);
    EXPECT_EQ(invalid.error().code(), Code::InvalidArgument);
  }
}

struct AllocationProbe {
  bool fail_allocate = false;
  bool fail_record = false;
  unsigned attempts = 0;
  unsigned frees = 0;
};

TEST(MemoryPoolTest, AllocationAndRecordingFailuresRefundReservation) {
  AllocationProbe probe;
  auto owner = testing::MakeTestPool(
      10, {.context = &probe,
           .fail_allocation =
               [](void *context) noexcept {
                 auto &probe = *static_cast<AllocationProbe *>(context);
                 ++probe.attempts;
                 return probe.fail_allocate;
               },
           .fail_recording =
               [](void *context) noexcept {
                 return static_cast<AllocationProbe *>(context)->fail_record;
               },
           .backing_freed =
               [](void *context) noexcept { ++static_cast<AllocationProbe *>(context)->frees; }});
  ASSERT_TRUE(owner);
  auto &pool = **owner;
  ASSERT_TRUE(pool.Allocate(0, 64));
  EXPECT_EQ(probe.attempts, 0U);
  probe.fail_allocate = true;
  const auto allocation = pool.Allocate(10, 1);
  ASSERT_FALSE(allocation);
  EXPECT_EQ(allocation.error().code(), Code::OutOfMemory);
  EXPECT_TRUE(allocation.error().message().empty());
  EXPECT_EQ(pool.live_bytes(), 0U);
  EXPECT_EQ(probe.frees, 0U);

  probe.fail_allocate = false;
  probe.fail_record = true;
  const auto record = pool.Allocate(10, 2);
  ASSERT_FALSE(record);
  EXPECT_EQ(record.error().code(), Code::OutOfMemory);
  EXPECT_TRUE(record.error().message().empty());
  EXPECT_TRUE(record.error().context().empty());
  EXPECT_EQ(pool.live_bytes(), 0U);
  EXPECT_EQ(probe.attempts, 2U);
  EXPECT_EQ(probe.frees, 1U);
}

struct Overlap {
  std::latch reserved{1};
  std::latch freed{1};
  bool armed = false;
  bool fail = false;
};

class MemoryPoolOverlapTest : public ::testing::TestWithParam<bool> {};

TEST_P(MemoryPoolOverlapTest, FreeDuringReservedAllocation) {
  Overlap overlap;
  overlap.fail = GetParam();
  auto owner = testing::MakeTestPool(
      64, {.context = &overlap, .fail_allocation = [](void *context) noexcept {
             auto &overlap = *static_cast<Overlap *>(context);
             if (!overlap.armed) {
               return false;
             }
             overlap.reserved.count_down();
             overlap.freed.wait();
             return overlap.fail;
           }});
  ASSERT_TRUE(owner);
  auto existing = (*owner)->Allocate(29, 1);
  ASSERT_TRUE(existing);
  overlap.armed = true;
  std::thread consumer([pool = *owner, memory = *existing, &overlap] {
    overlap.reserved.wait();
    EXPECT_EQ(pool->live_bytes(), 64U);
    pool->Free(memory, 29, 1);
    overlap.freed.count_down();
  });
  const auto second = (*owner)->Allocate(35, 64);
  consumer.join();
  if (overlap.fail) {
    ASSERT_FALSE(second);
    EXPECT_EQ(second.error().code(), Code::OutOfMemory);
    EXPECT_EQ((*owner)->live_bytes(), 0U);
  } else {
    ASSERT_TRUE(second);
    EXPECT_EQ((*owner)->live_bytes(), 35U);
    (*owner)->Free(*second, 35, 64);
    EXPECT_EQ((*owner)->live_bytes(), 0U);
  }
}

INSTANTIATE_TEST_SUITE_P(SuccessAndRollback, MemoryPoolOverlapTest, ::testing::Bool());

TEST(MemoryPoolTest, FreeBetweenLoadAndReservationDoesNotRestoreStaleBytes) {
  struct PendingFree {
    MemoryPool *pool = nullptr;
    void *memory = nullptr;
  } pending;
  const auto owner = testing::MakeTestPool(
      64, {.context = &pending, .before_reserve = [](void *context) noexcept {
             auto &pending = *static_cast<PendingFree *>(context);
             if (auto *memory = std::exchange(pending.memory, nullptr); memory != nullptr) {
               pending.pool->Free(memory, 29, 1);
             }
           }});
  ASSERT_TRUE(owner);
  pending.pool = &**owner;
  const auto first = pending.pool->Allocate(29, 1);
  ASSERT_TRUE(first);
  pending.memory = *first;

  const auto second = pending.pool->Allocate(35, 64);
  ASSERT_TRUE(second);
  EXPECT_EQ(pending.memory, nullptr);
  EXPECT_EQ(pending.pool->live_bytes(), 35U);
  pending.pool->Free(*second, 35, 64);
  if (pending.memory != nullptr) {
    pending.pool->Free(pending.memory, 29, 1);
  }
  EXPECT_EQ(pending.pool->live_bytes(), 0U);
  // Preserve the stale-reservation assertion if the final-reference check would abort.
  if (pending.pool->live_bytes() != 0) {
    testing::InjectLiveBytes(*pending.pool, 0);
  }
}

struct Destruction {
  unsigned count = 0;
  std::thread::id thread;
};

TEST(MemoryPoolTest, FinalEmptyReferenceDestroysOnReleasingThread) {
  Destruction destruction;
  auto query =
      testing::MakeTestPool(0, {.context = &destruction, .destroyed = [](void *context) noexcept {
                                  auto &state = *static_cast<Destruction *>(context);
                                  ++state.count;
                                  state.thread = std::this_thread::get_id();
                                }});
  ASSERT_TRUE(query);
  auto consumer_reference = *query;
  *query = {};
  EXPECT_EQ(destruction.count, 0U);
  std::latch release{1};
  std::thread consumer([pool = std::move(consumer_reference), &release] mutable {
    release.wait();
    EXPECT_EQ(pool->live_bytes(), 0U);
    pool = {};
  });
  const auto thread = consumer.get_id();
  release.count_down();
  consumer.join();
  EXPECT_EQ(destruction.count, 1U);
  EXPECT_EQ(destruction.thread, thread);
}

TEST(MemoryPoolDeathTest, IncorrectSizeAndAlignmentAbort) {
  EXPECT_DEATH(
      {
        const auto pool = *MakeFlatPool(16);
        auto allocation = *pool->Allocate(8, 8);
        pool->Free(allocation, 7, 8);
      },
      "size or alignment mismatch");
  EXPECT_DEATH(
      {
        const auto pool = *MakeFlatPool(16);
        auto allocation = *pool->Allocate(8, 8);
        pool->Free(allocation, 8, 4);
      },
      "size or alignment mismatch");
}

TEST(MemoryPoolDeathTest, LiveBytesOnFinalReleaseAbort) {
  EXPECT_DEATH(
      {
        auto pool = *testing::MakeTestPool(16, {});
        testing::InjectLiveBytes(*pool, 1);
        pool = {};
      },
      "destroyed with live bytes");
}

TEST(MemoryPoolDeathTest, LiveBytesAboveLimitAbortOnReserve) {
  EXPECT_DEATH(
      {
        const auto pool = *testing::MakeTestPool(16, {});
        testing::InjectLiveBytes(*pool, 17);
        const auto allocation = pool->Allocate(1, 1);
        if (allocation) {
          pool->Free(*allocation, 1, 1);
        }
        testing::InjectLiveBytes(*pool, 0);
      },
      "MemoryPool live bytes exceed limit");
}

#ifndef NDEBUG
TEST(MemoryPoolDeathTest, UnknownPointerAndDoubleFreeAbort) {
  EXPECT_DEATH(
      {
        const auto pool = *MakeFlatPool(16);
        std::uint64_t unknown = 0;
        pool->Free(&unknown, sizeof(unknown), alignof(decltype(unknown)));
      },
      "unknown or already freed pointer");
  EXPECT_DEATH(
      {
        const auto pool = *MakeFlatPool(16);
        auto allocation = *pool->Allocate(8, 8);
        pool->Free(allocation, 8, 8);
        pool->Free(allocation, 8, 8);
      },
      "unknown or already freed pointer");
}
#endif

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers,cppcoreguidelines-pro-type-reinterpret-cast)

} // namespace
} // namespace doqe::base
