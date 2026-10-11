#include "drive.hpp"
#include <gtest/gtest.h>
#include <limits>
#include <stdexcept>

TEST(DriveTest, ForwardAndTurnCommandsRampIndependentlyAndClamp)
{
  DriveController drive;
  drive.update({ .up = true }, 3);
  EXPECT_FLOAT_EQ(drive.state().linear, 255);
  drive.update({ .up = true, .left = true }, 0.5f);
  EXPECT_FLOAT_EQ(drive.state().angular, 42.5f);
  EXPECT_FLOAT_EQ(drive.state().linear, 255);
  drive.update({ .up = true, .left = true }, 10);
  EXPECT_FLOAT_EQ(drive.state().angular, 255);
}

TEST(DriveTest, RampIsFrameRateIndependent)
{
  DriveController thirty, sixty;
  for (int frame = 0; frame < 30; ++frame)
    thirty.update({ .up = true, .right = true }, 1.0f / 30);
  for (int frame = 0; frame < 60; ++frame)
    sixty.update({ .up = true, .right = true }, 1.0f / 60);
  EXPECT_NEAR(thirty.state().angular, sixty.state().angular, 0.001f);
  EXPECT_NEAR(thirty.state().linear, 85, 0.001f);
}

TEST(DriveTest, LockFreezesBothAxesAndSpaceToggles)
{
  DriveController drive;
  DriveInput input{ .up = true, .left = true };
  drive.update(input, 1);
  input.toggle_lock = true;
  drive.update(input, 1);
  EXPECT_TRUE(drive.state().locked);
  EXPECT_FLOAT_EQ(drive.state().angular, 85);
  EXPECT_FLOAT_EQ(drive.state().linear, 85);
  input.toggle_lock = false;
  drive.update(input, 10);
  EXPECT_FLOAT_EQ(drive.state().linear, 85);
  input.toggle_lock = true;
  drive.update(input, 1);
  EXPECT_FALSE(drive.state().locked);
  EXPECT_FLOAT_EQ(drive.state().linear, 170);
}

TEST(DriveTest, NewDirectionUnlocksBothAxes)
{
  DriveController drive;
  drive.update({ .up = true }, 1);
  drive.update({ .up = true, .toggle_lock = true }, 0);
  drive.update({ .up = true, .left = true }, 0.5f);
  EXPECT_FALSE(drive.state().locked);
  EXPECT_FLOAT_EQ(drive.state().angular, 42.5f);
  EXPECT_FLOAT_EQ(drive.state().linear, 127.5f);
}

TEST(DriveTest, PartialReleaseAndRestoringLockedCombinationKeepsBothThrusts)
{
  DriveController drive;
  drive.update({ .up = true, .left = true }, 1);
  drive.update({ .up = true, .left = true, .toggle_lock = true }, 0);
  drive.update({ .up = true }, 1);
  EXPECT_TRUE(drive.state().locked);
  EXPECT_FLOAT_EQ(drive.state().angular, 85);
  EXPECT_FLOAT_EQ(drive.state().linear, 85);
  drive.update({ .up = true, .left = true }, 0.5f);
  EXPECT_TRUE(drive.state().locked);
  EXPECT_FLOAT_EQ(drive.state().angular, 85);
  EXPECT_FLOAT_EQ(drive.state().linear, 85);
}

TEST(DriveTest, SameCombinationAfterFullReleaseKeepsLockSnapshot)
{
  DriveController drive;
  drive.update({ .up = true, .left = true }, 1);
  drive.update({ .up = true, .left = true, .toggle_lock = true }, 0);
  drive.update({}, 5);
  drive.update({ .up = true, .left = true }, 1);
  EXPECT_TRUE(drive.state().locked);
  EXPECT_FLOAT_EQ(drive.state().angular, 85);
  EXPECT_FLOAT_EQ(drive.state().linear, 85);
}

TEST(DriveTest, DifferentCombinationAfterPartialReleaseUnlocksBothAxes)
{
  DriveController drive;
  drive.update({ .up = true, .left = true }, 1);
  drive.update({ .up = true, .left = true, .toggle_lock = true }, 0);
  drive.update({ .up = true }, 1);
  drive.update({ .up = true, .right = true }, 0.5f);
  EXPECT_FALSE(drive.state().locked);
  EXPECT_FLOAT_EQ(drive.state().angular, -42.5f);
  EXPECT_FLOAT_EQ(drive.state().linear, 127.5f);
}

TEST(DriveTest, AllReleasedWhileLockedKeepsCommandsUntilSpaceUnlocks)
{
  DriveController drive;
  drive.update({ .up = true, .left = true }, 1);
  drive.update({ .up = true, .left = true, .toggle_lock = true }, 0);
  drive.update({}, 0);
  drive.update({}, 10);
  EXPECT_TRUE(drive.state().locked);
  EXPECT_FLOAT_EQ(drive.state().angular, 85);
  EXPECT_FLOAT_EQ(drive.state().linear, 85);
  drive.update({ .toggle_lock = true }, 1);
  EXPECT_FALSE(drive.state().locked);
  EXPECT_FLOAT_EQ(drive.state().angular, 0);
  EXPECT_FLOAT_EQ(drive.state().linear, 0);
  drive.update({ .toggle_lock = true }, 1);
  EXPECT_FALSE(drive.state().locked);
}

TEST(DriveTest, AllReleasedWithoutLockStopsBothAxes)
{
  DriveController drive;
  drive.update({ .up = true, .left = true }, 1);
  drive.update({}, 1);
  EXPECT_FALSE(drive.state().locked);
  EXPECT_FLOAT_EQ(drive.state().angular, 0);
  EXPECT_FLOAT_EQ(drive.state().linear, 0);
}

TEST(DriveTest, DirectionPressedAfterFullReleaseUnlocksBothAxes)
{
  DriveController drive;
  drive.update({ .up = true }, 1);
  drive.update({ .up = true, .toggle_lock = true }, 0);
  drive.update({}, 1);
  drive.update({ .right = true }, 0.5f);
  EXPECT_FALSE(drive.state().locked);
  EXPECT_FLOAT_EQ(drive.state().angular, -42.5f);
  EXPECT_FLOAT_EQ(drive.state().linear, 0);
}

TEST(DriveTest, FocusLossStopsLockedThrustAfterAllKeysAreReleased)
{
  DriveController drive;
  drive.update({ .up = true }, 1);
  drive.update({ .up = true, .toggle_lock = true }, 0);
  drive.update({}, 1);
  drive.update({ .focused = false }, 0);
  EXPECT_FALSE(drive.state().locked);
  EXPECT_FLOAT_EQ(drive.state().linear, 0);
  drive.update({ .up = true }, 0.5f);
  EXPECT_FLOAT_EQ(drive.state().linear, 42.5f);
}

TEST(DriveTest, UnlockedReleaseZerosOnlyReleasedAxis)
{
  DriveController drive;
  drive.update({ .up = true, .left = true }, 1);
  drive.update({ .up = true }, 0.5f);
  EXPECT_FLOAT_EQ(drive.state().angular, 0);
  EXPECT_FLOAT_EQ(drive.state().linear, 127.5f);
}

TEST(DriveTest, OppositeKeysCancelTheirAxisAndReversalStartsAtZero)
{
  DriveController drive;
  drive.update({ .up = true, .right = true }, 1);
  drive.update({ .up = true, .left = true, .right = true }, 0.5f);
  EXPECT_FLOAT_EQ(drive.state().angular, 0);
  EXPECT_FLOAT_EQ(drive.state().linear, 127.5f);
  drive.update({ .down = true, .left = true }, 0.25f);
  EXPECT_FLOAT_EQ(drive.state().linear, -21.25f);
  EXPECT_FLOAT_EQ(drive.state().angular, 21.25f);
}

TEST(DriveTest, NewDirectionTakesPrecedenceOverSimultaneousSpace)
{
  DriveController drive;
  drive.update({ .up = true }, 1);
  drive.update({ .up = true, .toggle_lock = true }, 0);
  drive.update({ .up = true, .right = true, .toggle_lock = true }, 0.5f);
  EXPECT_FALSE(drive.state().locked);
  EXPECT_FLOAT_EQ(drive.state().linear, 127.5f);
}

TEST(DriveTest, InvalidDeltaDoesNotPoisonThrustAndReleaseStillStops)
{
  DriveController drive;
  drive.update({ .up = true }, 1);
  for (float dt : { -1.0f,
                    std::numeric_limits<float>::quiet_NaN(),
                    std::numeric_limits<float>::infinity() }) {
    drive.update({ .up = true }, dt);
    EXPECT_FLOAT_EQ(drive.state().linear, 85);
  }
  drive.update({}, std::numeric_limits<float>::quiet_NaN());
  EXPECT_FLOAT_EQ(drive.state().linear, 0);
}

TEST(DriveTest, MovementCombinationsProduceIndependentLinearAndAngularCommands)
{
  const DriveInput directions[]{
    { .up = true },    { .up = true, .right = true },
    { .right = true }, { .down = true, .right = true },
    { .down = true },  { .down = true, .left = true },
    { .left = true },  { .up = true, .left = true }
  };
  const float linear[]{ 85, 85, 0, -85, -85, -85, 0, 85 };
  const float angular[]{ 0, -85, -85, -85, 0, 85, 85, 85 };
  for (int index = 0; index < 8; ++index) {
    DriveController drive;
    drive.update(directions[index], 1);
    EXPECT_FLOAT_EQ(drive.state().linear, linear[index]);
    EXPECT_FLOAT_EQ(drive.state().angular, angular[index]);
  }
}

TEST(DriveTest, CustomRateAndLimitAreUsedAndInvalidSettingsRejected)
{
  DriveController drive(10, 20);
  drive.update({ .up = true }, 1);
  EXPECT_FLOAT_EQ(drive.state().linear, 10);
  drive.update({ .up = true }, 10);
  EXPECT_FLOAT_EQ(drive.state().linear, 20);
  EXPECT_THROW(DriveController(0, 20), std::invalid_argument);
  EXPECT_THROW(DriveController(10, -1), std::invalid_argument);
  EXPECT_THROW(DriveController(std::numeric_limits<float>::infinity(), 20),
               std::invalid_argument);
}
