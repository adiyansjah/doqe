#include <gtest/gtest.h>

#include <stdexcept>

void throw_for_unwind_test();

TEST(Runtime, UnwindsAcrossFunctionBoundary) {
  int destroyed = 0;
  struct Guard {
    int &count;
    ~Guard() { ++count; }
  };

  try {
    Guard guard{destroyed};
    throw_for_unwind_test();
    ADD_FAILURE() << "Expected an exception";
  } catch (const std::runtime_error &error) {
    EXPECT_STREQ(error.what(), "unwind test");
    EXPECT_EQ(destroyed, 1);
  }
}
