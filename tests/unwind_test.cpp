#include "throw_for_unwind_test.hpp"

#include <gtest/gtest.h>

#include <stdexcept>

TEST(Runtime, UnwindsAcrossFunctionBoundary) {
  int destroyed = 0;
  class Guard {
  public:
    explicit Guard(int &count) : count_(&count) {}
    ~Guard() { ++*count_; }
    Guard(const Guard &) = delete;
    Guard &operator=(const Guard &) = delete;
    Guard(Guard &&) = delete;
    Guard &operator=(Guard &&) = delete;

  private:
    int *count_;
  };

  try {
    const Guard guard{destroyed};
    throw_for_unwind_test();
    ADD_FAILURE() << "Expected an exception";
  } catch (const std::runtime_error &error) {
    EXPECT_STREQ(error.what(), "unwind test");
    EXPECT_EQ(destroyed, 1);
  }
}
