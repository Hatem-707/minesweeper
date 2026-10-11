#include "pickup.hpp"
#include <gtest/gtest.h>

TEST(PickupTest, UprightRequestEmitsOneCommand)
{
  PickupController controller;
  controller.update({ .requested = true, .arm_upright = true }, 0.016f);
  EXPECT_EQ(controller.take_command(), PickupController::command_value);
  EXPECT_FALSE(controller.take_command());
  controller.update({ .arm_upright = true }, 0.016f);
  EXPECT_FALSE(controller.take_command());
}

TEST(PickupTest, RequestWhileNotUprightIsIgnoredAndFlashes)
{
  PickupController controller;
  controller.update({ .requested = true, .arm_upright = false }, 0.016f);
  EXPECT_FALSE(controller.take_command());
  EXPECT_FLOAT_EQ(controller.state().rejected_remaining,
                  PickupController::rejected_flash_seconds);
  controller.update({}, 0.1f);
  EXPECT_NEAR(controller.state().rejected_remaining,
              PickupController::rejected_flash_seconds - 0.1f,
              1e-6);
  controller.update({}, 1.f);
  EXPECT_FLOAT_EQ(controller.state().rejected_remaining, 0.f);
}

TEST(PickupTest, UnfocusedRequestIsDropped)
{
  PickupController controller;
  controller.update(
    { .requested = true, .arm_upright = true, .focused = false }, 0.016f);
  EXPECT_FALSE(controller.take_command());
  EXPECT_FLOAT_EQ(controller.state().rejected_remaining, 0.f);
}

TEST(PickupTest, DefaultStateNeverEmits)
{
  PickupController controller;
  controller.update({ .requested = true }, 0.016f);
  EXPECT_FALSE(controller.take_command());
  EXPECT_FALSE(controller.state().arm_upright);
}

TEST(PickupTest, StateReflectsSwitch)
{
  PickupController controller;
  controller.update({ .arm_upright = true }, 0.016f);
  EXPECT_TRUE(controller.state().arm_upright);
  controller.update({ .arm_upright = false }, 0.016f);
  EXPECT_FALSE(controller.state().arm_upright);
}
