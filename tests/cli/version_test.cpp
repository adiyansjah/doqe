#include "version.hpp"

#include <gtest/gtest.h>

TEST(Version, FormatsGitProvenance) {
  EXPECT_EQ(doqe::cli::version_string("0.1.0", "abc1234", false), "doqe 0.1.0 (abc1234)");
  EXPECT_EQ(doqe::cli::version_string("0.1.0", "abc1234", true), "doqe 0.1.0 (abc1234-dirty)");
  EXPECT_EQ(doqe::cli::version_string("0.1.0", "unknown", false), "doqe 0.1.0 (unknown)");
}
