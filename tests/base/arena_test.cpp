#include "doqe/base/arena.hpp"
#include "doqe/base/memory_pool.hpp"
#include "doqe/base/status.hpp"
#include <cstddef>

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <utility>

namespace {

// The sizes are test inputs and expected pool charges.
// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers)

using doqe::base::Arena;
using doqe::base::Code;

TEST(ArenaTest, LazyGrowthDedicatedChunksAndExactCharges) {
  const auto pool = doqe::base::MakeFlatPool(std::size_t{256} * 1024U);
  ASSERT_TRUE(pool);
  {
    Arena arena(*pool);
    EXPECT_EQ((*pool)->live_bytes(), 0U);
    ASSERT_TRUE(arena.Allocate(0, 64));
    EXPECT_EQ((*pool)->live_bytes(), 0U);
    const auto small = arena.Allocate(4000, 64);
    ASSERT_TRUE(small);
    // Verify the alignment of the actual address.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(*small) % 64, 0U);
    EXPECT_EQ((*pool)->live_bytes(), 4096U);
    ASSERT_TRUE(arena.Allocate(65536, 64));
    const auto after_dedicated = (*pool)->live_bytes();
    EXPECT_GT(after_dedicated, 4096U + 65536U);
    ASSERT_TRUE(arena.Allocate(5000, 64));
    EXPECT_EQ((*pool)->live_bytes(), after_dedicated + 8192U);
    const Arena moved(std::move(arena));
    EXPECT_EQ((*pool)->live_bytes(), after_dedicated + 8192U);
  }
  EXPECT_EQ((*pool)->live_bytes(), 0U);
}

TEST(ArenaTest, SavepointRestoresCursorAndGrowthAfterNewChunks) {
  const auto pool = doqe::base::MakeFlatPool(32768);
  ASSERT_TRUE(pool);
  Arena arena(*pool);
  ASSERT_TRUE(arena.Allocate(3000, 1));
  const auto live_before = (*pool)->live_bytes();
  void *rewound = nullptr;
  {
    const Arena::Savepoint savepoint(arena);
    const auto scratch = arena.Allocate(1000, 1);
    ASSERT_TRUE(scratch);
    rewound = *scratch;
    ASSERT_TRUE(arena.Allocate(5000, 1));
    EXPECT_EQ((*pool)->live_bytes(), live_before + 8192U);
  }
  EXPECT_EQ((*pool)->live_bytes(), live_before);
  const auto reused = arena.Allocate(1000, 1);
  ASSERT_TRUE(reused);
  EXPECT_EQ(*reused, rewound);
  ASSERT_TRUE(arena.Allocate(5000, 1));
  EXPECT_EQ((*pool)->live_bytes(), live_before + 8192U);
}

TEST(ArenaTest, OverflowInvalidAlignmentAndBudgetLeaveArenaUsable) {
  const auto pool = doqe::base::MakeFlatPool(4096);
  ASSERT_TRUE(pool);
  Arena arena(*pool);
  const auto bad_alignment = arena.Allocate(1, 3);
  ASSERT_FALSE(bad_alignment);
  EXPECT_EQ(bad_alignment.error().code(), Code::InvalidArgument);
  const auto overflow = arena.Allocate(std::numeric_limits<std::size_t>::max(), 64);
  ASSERT_FALSE(overflow);
  EXPECT_EQ(overflow.error().code(), Code::ResourceExhausted);
  ASSERT_TRUE(arena.Allocate(3000, 1));
  const auto budget = arena.Allocate(4096, 1);
  ASSERT_FALSE(budget);
  EXPECT_EQ(budget.error().code(), Code::ResourceExhausted);
  EXPECT_EQ((*pool)->live_bytes(), 4096U);
  const auto values = arena.MakeArray<int>(2);
  ASSERT_TRUE(values);
  EXPECT_EQ(values->front(), 0);
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers)

} // namespace
