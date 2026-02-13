#include <gtest/gtest.h>

// Example test to ensure framework is working
TEST(SanityCheck, BasicAssertions) {
  EXPECT_TRUE(true);
  EXPECT_EQ(1 + 1, 2);
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
