#include "link.hpp"
#include "wire.hpp"
#include <chrono>
#include <filesystem>
#include <gtest/gtest.h>
#include <optional>
#include <string>
#include <thread>
#include <unistd.h>

namespace {
using namespace std::chrono_literals;

// Stands in for the bridge and the vision process on private ipc paths.
class Peers
{
public:
  std::filesystem::path directory =
    std::filesystem::temp_directory_path() /
    ("minesweeper-test-" + std::to_string(::getpid()));
  WireEndpoints endpoints;
  zmq::context_t context;
  zmq::socket_t commands{ context, zmq::socket_type::pull };
  zmq::socket_t telemetry{ context, zmq::socket_type::pub };
  zmq::socket_t vision{ context, zmq::socket_type::pub };

  Peers()
  {
    std::filesystem::create_directories(directory);
    endpoints = { "ipc://" + (directory / "cmd.ipc").string(),
                  "ipc://" + (directory / "telemetry.ipc").string(),
                  "ipc://" + (directory / "vision.ipc").string() };
    for (auto* socket : { &commands, &telemetry, &vision }) {
      socket->set(zmq::sockopt::linger, 0);
    }
    commands.set(zmq::sockopt::rcvtimeo, 1000);
    commands.bind(endpoints.command);
    telemetry.bind(endpoints.telemetry);
    vision.bind(endpoints.vision);
  }
  ~Peers() { std::filesystem::remove_all(directory); }

  // Receives commands until one of type T arrives.
  template<typename T>
  std::optional<T> receive()
  {
    for (int attempt = 0; attempt < 100; ++attempt) {
      zmq::message_t frame;
      if (!commands.recv(frame)) {
        return std::nullopt;
      }
      if (auto message = msg::decode<T>(frame)) {
        return message;
      }
    }
    return std::nullopt;
  }

  // PUB/SUB drops messages until the subscription is connected, so keep
  // publishing until the wire reports it.
  template<typename T, typename Done>
  bool publish_until(zmq::socket_t& socket, T message, Done done)
  {
    std::uint32_t seq = 0;
    for (int attempt = 0; attempt < 100; ++attempt) {
      socket.send(msg::encode(message, seq), zmq::send_flags::none);
      std::this_thread::sleep_for(10ms);
      if (done()) {
        return true;
      }
    }
    return false;
  }
};
}

TEST(LinkHealthTest, NothingReceivedIsUnhealthy)
{
  const auto health = link_health(RobotLink{}, 1'000'000'000);
  EXPECT_FALSE(health.bridge);
  EXPECT_FALSE(health.localization);
  EXPECT_FALSE(health.hardware);
  EXPECT_FALSE(health.vision);
}

TEST(LinkHealthTest, PerNodeAgesAndBridgeReceiptAge)
{
  RobotLink link;
  link.telemetry = msg::Telemetry{};
  link.telemetry->localization_age_ms = 20;
  link.telemetry->hardware_age_ms = 900;
  link.telemetry->arm_upright = 1;
  link.telemetry_received_ns = 1'000'000'000;

  auto health = link_health(link, 1'100'000'000);
  EXPECT_TRUE(health.bridge);
  EXPECT_TRUE(health.localization);
  EXPECT_FALSE(health.hardware);
  EXPECT_FALSE(arm_upright(link, health)); // Stale hardware is not upright.

  link.telemetry->hardware_age_ms = 20;
  health = link_health(link, 1'100'000'000);
  EXPECT_TRUE(arm_upright(link, health));

  health = link_health(link, 1'000'000'000 + stale_after_ns + 1);
  EXPECT_FALSE(health.bridge);
  EXPECT_FALSE(health.localization);
  EXPECT_FALSE(arm_upright(link, health));
}

TEST(WireTest, SendsDriveAndOneShotCommands)
{
  Peers peers;
  Wire wire(peers.endpoints);
  wire.set_drive(100, -300); // Clamped to the contract limit.
  std::optional<msg::Drive> drive;
  for (int attempt = 0; attempt < 20 && (!drive || drive->forward == 0);
       ++attempt) {
    drive = peers.receive<msg::Drive>();
    wire.set_drive(100, -300);
  }
  ASSERT_TRUE(drive);
  EXPECT_FLOAT_EQ(drive->forward, 100);
  EXPECT_FLOAT_EQ(drive->left, -msg::drive_limit);
  EXPECT_FALSE(msg::is_expired(drive->header));

  wire.send_pickup();
  EXPECT_TRUE(peers.receive<msg::Pickup>());

  wire.send_reset(1.5, 2, msg::Heading::NegX);
  const auto reset = peers.receive<msg::ResetPose>();
  ASSERT_TRUE(reset);
  EXPECT_DOUBLE_EQ(reset->x, 1.5);
  EXPECT_EQ(reset->heading, msg::Heading::NegX);
}

TEST(WireTest, DriveFallsBackToZeroWhenTheUiStopsRefreshing)
{
  Peers peers;
  Wire wire(peers.endpoints);
  wire.set_drive(100, 0);
  std::this_thread::sleep_for(
    std::chrono::nanoseconds(Wire::drive_refresh_timeout_ns) + 100ms);
  // Discard drives queued before the timeout.
  peers.commands.set(zmq::sockopt::rcvtimeo, 0);
  zmq::message_t frame;
  while (peers.commands.recv(frame)) {
  }
  peers.commands.set(zmq::sockopt::rcvtimeo, 1000);
  const auto drive = peers.receive<msg::Drive>();
  ASSERT_TRUE(drive);
  EXPECT_FLOAT_EQ(drive->forward, 0);
}

TEST(WireTest, ReceivesTelemetryVisionAndMetalEdges)
{
  Peers peers;
  Wire wire(peers.endpoints);
  msg::Telemetry telemetry{};
  telemetry.x = 3;
  telemetry.metal_detected = 1;
  ASSERT_TRUE(peers.publish_until(
    peers.telemetry, telemetry, [&] { return wire.link().telemetry.has_value(); }));
  EXPECT_DOUBLE_EQ(wire.link().telemetry->x, 3);
  // Repeated positive samples are one rising edge.
  const auto hits = wire.take_metal_hits();
  ASSERT_EQ(hits.size(), 1u);
  EXPECT_DOUBLE_EQ(hits[0].telemetry.x, 3);

  msg::Vision vision{};
  vision.surface_detected = 1;
  vision.confidence = 0.9f;
  ASSERT_TRUE(peers.publish_until(
    peers.vision, vision, [&] { return wire.link().vision.has_value(); }));
  EXPECT_TRUE(link_health(wire.link()).vision);
  EXPECT_FALSE(wire.take_vision().empty());
}
