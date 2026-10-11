#include <minesweeper/messages.hpp>
#include <gtest/gtest.h>
#include <limits>

namespace {
template<typename T>
std::span<const std::byte>
bytes(const T& message)
{
  return { reinterpret_cast<const std::byte*>(&message), sizeof(message) };
}

template<typename T>
T
stamped(T message)
{
  std::uint32_t seq = 0;
  msg::stamp(message, seq, 1000);
  return message;
}
}

TEST(MessagesTest, DriveRoundTripsThroughZmqFrame)
{
  std::uint32_t seq = 7;
  msg::Drive drive{};
  drive.forward = 120.5f;
  drive.left = -255;
  const auto frame = msg::encode(drive, seq);
  EXPECT_EQ(seq, 8u);
  EXPECT_EQ(msg::peek_type(frame), msg::MessageType::Drive);
  const auto decoded = msg::decode<msg::Drive>(frame);
  ASSERT_TRUE(decoded);
  EXPECT_EQ(decoded->header.version, msg::protocol_version);
  EXPECT_EQ(decoded->header.seq, 7u);
  EXPECT_FLOAT_EQ(decoded->forward, 120.5f);
  EXPECT_FLOAT_EQ(decoded->left, -255);
}

TEST(MessagesTest, EveryTypeRoundTrips)
{
  msg::ResetPose reset{};
  reset.x = 20;
  reset.y = 0.5;
  reset.heading = msg::Heading::NegY;
  EXPECT_TRUE(msg::decode<msg::ResetPose>(bytes(stamped(reset))));
  EXPECT_TRUE(msg::decode<msg::Pickup>(bytes(stamped(msg::Pickup{}))));

  msg::Telemetry telemetry{};
  telemetry.localization_age_ms = msg::never;
  telemetry.arm_upright = 1;
  telemetry.metal_detected = 1;
  const auto decoded = msg::decode<msg::Telemetry>(bytes(stamped(telemetry)));
  ASSERT_TRUE(decoded);
  EXPECT_EQ(decoded->localization_age_ms, msg::never);
  EXPECT_EQ(decoded->metal_detected, 1);

  msg::Vision vision{};
  vision.frame_ns = 42;
  vision.surface_detected = 1;
  vision.confidence = 1;
  EXPECT_TRUE(msg::decode<msg::Vision>(bytes(stamped(vision))));
}

TEST(MessagesTest, TypeIsTheFirstByte)
{
  const auto vision = stamped(msg::Vision{});
  EXPECT_EQ(std::uint8_t(bytes(vision).front()), 32);
  EXPECT_FALSE(msg::peek_type(std::span<const std::byte>{}));
}

TEST(MessagesTest, RejectsWrongSizeTypeAndVersion)
{
  const auto drive = stamped(msg::Drive{});
  EXPECT_FALSE(msg::decode<msg::Drive>(bytes(drive).first(sizeof(drive) - 1)));
  EXPECT_FALSE(msg::decode<msg::Pickup>(bytes(drive).first(sizeof(msg::Pickup))));

  auto wrong_version = drive;
  wrong_version.header.version = msg::protocol_version + 1;
  EXPECT_FALSE(msg::decode<msg::Drive>(bytes(wrong_version)));

  auto wrong_type = drive;
  wrong_type.header.type = msg::MessageType::Telemetry;
  EXPECT_FALSE(msg::decode<msg::Drive>(bytes(wrong_type)));
}

TEST(MessagesTest, RejectsOutOfRangeValues)
{
  msg::Drive drive{};
  drive.forward = msg::drive_limit + 1;
  EXPECT_FALSE(msg::decode<msg::Drive>(bytes(stamped(drive))));
  drive.forward = std::numeric_limits<float>::quiet_NaN();
  EXPECT_FALSE(msg::decode<msg::Drive>(bytes(stamped(drive))));

  msg::ResetPose reset{};
  reset.heading = msg::Heading(4);
  EXPECT_FALSE(msg::decode<msg::ResetPose>(bytes(stamped(reset))));
  reset.heading = msg::Heading::PosX;
  reset.x = std::numeric_limits<double>::infinity();
  EXPECT_FALSE(msg::decode<msg::ResetPose>(bytes(stamped(reset))));

  msg::Telemetry telemetry{};
  telemetry.arm_upright = 2;
  EXPECT_FALSE(msg::decode<msg::Telemetry>(bytes(stamped(telemetry))));

  msg::Vision vision{};
  vision.confidence = 1.5f;
  EXPECT_FALSE(msg::decode<msg::Vision>(bytes(stamped(vision))));
}

TEST(MessagesTest, CommandsExpireAfter200Milliseconds)
{
  const auto drive = stamped(msg::Drive{});
  EXPECT_FALSE(msg::is_expired(drive.header, 1000 + msg::command_expiry_ns));
  EXPECT_TRUE(msg::is_expired(drive.header, 1001 + msg::command_expiry_ns));
}
