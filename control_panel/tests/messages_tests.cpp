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

TEST(MessagesTest, ResetRoundTripsThroughZmqFrame)
{
  std::uint32_t seq = 7;
  msg::ResetPose reset{};
  reset.x = 1.5;
  reset.y = 19.5;
  reset.heading = msg::Heading::NegX;
  const auto frame = msg::encode(reset, seq);
  EXPECT_EQ(seq, 8u);
  EXPECT_EQ(msg::peek_type(frame), msg::MessageType::ResetPose);
  const auto decoded = msg::decode<msg::ResetPose>(frame);
  ASSERT_TRUE(decoded);
  EXPECT_EQ(decoded->header.version, msg::protocol_version);
  EXPECT_EQ(decoded->header.seq, 7u);
  EXPECT_DOUBLE_EQ(decoded->x, 1.5);
  EXPECT_DOUBLE_EQ(decoded->y, 19.5);
  EXPECT_EQ(decoded->heading, msg::Heading::NegX);
}

TEST(MessagesTest, TelemetryAndVisionRoundTrip)
{
  msg::Telemetry telemetry{};
  telemetry.localization_age_ms = msg::never;
  telemetry.arm_upright = 1;
  telemetry.metal_detected = 1;
  const auto decoded = msg::decode<msg::Telemetry>(bytes(stamped(telemetry)));
  ASSERT_TRUE(decoded);
  EXPECT_EQ(decoded->localization_age_ms, msg::never);
  EXPECT_EQ(decoded->arm_upright, 1);
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
  const auto reset = stamped(msg::ResetPose{});
  EXPECT_FALSE(
    msg::decode<msg::ResetPose>(bytes(reset).first(sizeof(reset) - 1)));

  auto wrong_version = reset;
  wrong_version.header.version = msg::protocol_version + 1;
  EXPECT_FALSE(msg::decode<msg::ResetPose>(bytes(wrong_version)));

  auto wrong_type = reset;
  wrong_type.header.type = msg::MessageType::Telemetry;
  EXPECT_FALSE(msg::decode<msg::ResetPose>(bytes(wrong_type)));
}

TEST(MessagesTest, RejectsOutOfRangeValues)
{
  msg::ResetPose reset{};
  reset.heading = msg::Heading(4);
  EXPECT_FALSE(msg::decode<msg::ResetPose>(bytes(stamped(reset))));
  reset.heading = msg::Heading::PosX;
  reset.x = std::numeric_limits<double>::infinity();
  EXPECT_FALSE(msg::decode<msg::ResetPose>(bytes(stamped(reset))));

  msg::Telemetry telemetry{};
  telemetry.arm_upright = 2;
  EXPECT_FALSE(msg::decode<msg::Telemetry>(bytes(stamped(telemetry))));
  telemetry.arm_upright = 0;
  telemetry.heading_rad = std::numeric_limits<double>::quiet_NaN();
  EXPECT_FALSE(msg::decode<msg::Telemetry>(bytes(stamped(telemetry))));

  msg::Vision vision{};
  vision.confidence = 1.5f;
  EXPECT_FALSE(msg::decode<msg::Vision>(bytes(stamped(vision))));
}

TEST(MessagesTest, CommandsExpireAfter200Milliseconds)
{
  const auto reset = stamped(msg::ResetPose{});
  EXPECT_FALSE(msg::is_expired(reset.header, 1000 + msg::command_expiry_ns));
  EXPECT_TRUE(msg::is_expired(reset.header, 1001 + msg::command_expiry_ns));
}
