#include "../utils.h"
#include "test_random_utils.h"
#include <gtest/gtest.h>

TEST(TestRandomUtils, WrapsQueuedDoubleRolls)
{
    clear_test_random_values();
    push_test_random_value(0.25);

    EXPECT_DOUBLE_EQ(number(), 0.25);

    clear_test_random_values();
}

TEST(TestRandomUtils, WrapsQueuedIntegerRollsUsingScaledRange)
{
    clear_test_random_values();
    push_test_random_value(0.60);

    EXPECT_EQ(number(10, 14), 13);

    clear_test_random_values();
}

TEST(TestRandomUtils, CountsTheQueuedRollsNotYetDrawn)
{
    clear_test_random_values();
    push_test_random_value(0.25);
    push_test_random_value(0.5);

    const double first_roll = number();
    const std::size_t remaining_rolls = test_random_values_remaining();

    EXPECT_DOUBLE_EQ(first_roll, 0.25);
    EXPECT_EQ(remaining_rolls, 1u) << "Expected one of the two queued rolls to remain.";

    clear_test_random_values();
}
