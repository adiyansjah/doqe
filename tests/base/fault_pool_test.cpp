#include "../support/fault_pool.hpp"
#include "doqe/base/memory_pool.hpp"
#include "doqe/base/status.hpp"

#include <gtest/gtest.h>

namespace {

// The sizes are test inputs and expected pool charges.
// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers)

TEST(FaultPoolTest, FailsOnlyTheSelectedCallAndDelegatesAccounting) {
  auto backing = doqe::base::MakeFlatPool(16);
  ASSERT_TRUE(backing);
  auto *fault = new doqe::test::FaultPool(*backing);
  const auto pool = doqe::base::PoolPtr::Adopt(fault);
  fault->FailAt(2, doqe::base::Code::ResourceExhausted);
  const auto first = pool->Allocate(8, 8);
  ASSERT_TRUE(first);
  const auto second = pool->Allocate(8, 8);
  ASSERT_FALSE(second);
  EXPECT_EQ(second.error().code(), doqe::base::Code::ResourceExhausted);
  const auto third = pool->Allocate(8, 8);
  ASSERT_TRUE(third);
  EXPECT_EQ(fault->calls(), 3U);
  EXPECT_EQ(pool->live_bytes(), pool->limit());
  EXPECT_EQ((*backing)->live_bytes(), 16U);
  pool->Free(*first, 8, 8);
  pool->Free(*third, 8, 8);
  EXPECT_EQ((*backing)->live_bytes(), 0U);
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers)

} // namespace
