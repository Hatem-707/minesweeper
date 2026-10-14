#include "mine_map.hpp"
#include <cmath>
#include <gtest/gtest.h>

namespace {
constexpr std::int64_t second = 1'000'000'000;

msg::Telemetry
reading(double x, double y, bool metal, bool arm_upright = true)
{
  msg::Telemetry telemetry{};
  telemetry.x = x;
  telemetry.y = y;
  telemetry.metal_detected = metal;
  telemetry.arm_upright = arm_upright;
  return telemetry;
}

MapConfig
config()
{
  return { .grid_size = 20,
           .field_size_m = 20,
           .classify_seconds = 5,
           .classify_distance_m = 0.5 };
}
}

TEST(MineMapTest, CellsFollowTheMapFrame)
{
  const MineMap map(config());
  EXPECT_EQ(map.cell_of(0, 0), (Cell{ 0, 0 }));
  EXPECT_EQ(map.cell_of(3.7, 12.2), (Cell{ 3, 12 }));
  EXPECT_EQ(map.cell_of(19.99, 19.99), (Cell{ 19, 19 }));
  EXPECT_FALSE(map.cell_of(-0.01, 5));
  EXPECT_FALSE(map.cell_of(5, 20));
  EXPECT_FALSE(map.cell_of(NAN, 5));
  const auto [x, y] = map.center_of({ 3, 12 });
  EXPECT_DOUBLE_EQ(x, 3.5);
  EXPECT_DOUBLE_EQ(y, 12.5);
}

TEST(MineMapTest, FreshPoseMarksTheTrailClean)
{
  MineMap map(config());
  map.update(reading(1.5, 2.5, false), 0);
  EXPECT_EQ(map.at({ 1, 2 }), MineState::clean);
  EXPECT_EQ(map.at({ 2, 2 }), MineState::unchecked);

  auto stale = reading(5.5, 5.5, false);
  stale.localization_age_ms = msg::never;
  map.update(stale, 0);
  EXPECT_EQ(map.at({ 5, 5 }), MineState::unchecked);
}

TEST(MineMapTest, ArmLoweredInsideTheWindowIsSurface)
{
  MineMap map(config());
  map.update(reading(4.5, 4.5, true), 0);
  EXPECT_EQ(map.at({ 4, 4 }), MineState::pending);
  map.update(reading(4.5, 4.5, true, false), 2 * second);
  EXPECT_EQ(map.at({ 4, 4 }), MineState::surface_mine);
  EXPECT_EQ(map.count(MineState::surface_mine), 1);
  EXPECT_EQ(map.take_messages().size(), 2u);
}

TEST(MineMapTest, ArmStillUprightAfterTheTimeoutIsBuried)
{
  MineMap map(config());
  map.update(reading(4.5, 4.5, true), 0);
  map.tick(5 * second);
  EXPECT_EQ(map.at({ 4, 4 }), MineState::pending);
  map.tick(5 * second + 1);
  EXPECT_EQ(map.at({ 4, 4 }), MineState::buried_mine);
  // Lowering the arm later does not reclassify it.
  map.update(reading(4.5, 4.5, false, false), 6 * second);
  EXPECT_EQ(map.at({ 4, 4 }), MineState::buried_mine);
}

TEST(MineMapTest, DrivingAwayClosesTheWindow)
{
  MineMap map(config());
  map.update(reading(4.5, 4.5, true), 0);
  map.update(reading(4.9, 4.5, false), second);
  EXPECT_EQ(map.at({ 4, 4 }), MineState::pending);
  map.update(reading(5.1, 4.5, false), second);
  EXPECT_EQ(map.at({ 4, 4 }), MineState::buried_mine);
  EXPECT_EQ(map.at({ 5, 4 }), MineState::clean);
}

TEST(MineMapTest, DetectorOffsetIsRotatedByHeading)
{
  auto settings = config();
  settings.detector_dx = 1;
  MineMap map(settings);
  auto telemetry = reading(4.5, 4.5, true);
  telemetry.heading_rad = M_PI / 2; // Facing +Y: the detector is at y + 1.
  map.update(telemetry, 0);
  EXPECT_EQ(map.at({ 4, 5 }), MineState::pending);
}

TEST(MineMapTest, StaleInputsNeverCreateMines)
{
  MineMap map(config());
  auto telemetry = reading(4.5, 4.5, true);
  telemetry.localization_age_ms = msg::never;
  map.update(telemetry, 0);
  EXPECT_EQ(map.count(MineState::pending), 0);
  EXPECT_EQ(map.take_messages().size(), 1u);

  telemetry = reading(4.5, 4.5, true);
  telemetry.hardware_age_ms = msg::never;
  map.update(telemetry, 0);
  EXPECT_EQ(map.count(MineState::pending), 0);
  // Fresh hardware with metal still high is a new rising edge.
  map.update(reading(4.5, 4.5, true), 0);
  EXPECT_EQ(map.count(MineState::pending), 1);
}

TEST(MineMapTest, RepeatHitsMergeAndNeverDowngradeSurface)
{
  MineMap map(config());
  map.update(reading(4.5, 4.5, true), 0);
  map.update(reading(4.5, 4.5, true, false), second);
  map.update(reading(4.5, 4.5, false), 2 * second);
  map.update(reading(4.5, 4.5, true), 3 * second);
  EXPECT_EQ(map.at({ 4, 4 }), MineState::pending);
  map.tick(9 * second);
  EXPECT_EQ(map.at({ 4, 4 }), MineState::surface_mine);
  EXPECT_EQ(map.count(MineState::surface_mine), 1);
}

TEST(MineMapTest, NewHitElsewhereResolvesThePendingOneAsBuried)
{
  auto settings = config();
  settings.classify_distance_m = 10;
  MineMap map(settings);
  map.update(reading(4.5, 4.5, true), 0);
  map.update(reading(6.5, 4.5, false), second);
  map.update(reading(6.5, 4.5, true), second);
  EXPECT_EQ(map.at({ 4, 4 }), MineState::buried_mine);
  EXPECT_EQ(map.at({ 6, 4 }), MineState::pending);
}
