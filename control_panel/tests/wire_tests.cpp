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
  link.telemetry_received_ns = 1'000'000'000;

  auto health = link_health(link, 1'100'000'000);
  EXPECT_TRUE(health.bridge);
  EXPECT_TRUE(health.localization);
  EXPECT_FALSE(health.hardware);

  health = link_health(link, 1'000'000'000 + stale_after_ns + 1);
  EXPECT_FALSE(health.bridge);
  EXPECT_FALSE(health.localization);
}

TEST(WireTest, SendsResetPose)
{
  Peers peers;
  Wire wire(peers.endpoints);
  // PUSH with ZMQ_IMMEDIATE drops sends until the pipe is up, so retry.
  std::optional<msg::ResetPose> reset;
  peers.commands.set(zmq::sockopt::rcvtimeo, 50);
  for (int attempt = 0; attempt < 40 && !reset; ++attempt) {
    wire.send_reset(1.5, 2, msg::Heading::NegX);
    reset = peers.receive<msg::ResetPose>();
  }
  ASSERT_TRUE(reset);
  EXPECT_DOUBLE_EQ(reset->x, 1.5);
  EXPECT_DOUBLE_EQ(reset->y, 2);
  EXPECT_EQ(reset->heading, msg::Heading::NegX);
  EXPECT_FALSE(msg::is_expired(reset->header));
}

TEST(WireTest, ReceivesTelemetryAndVision)
{
  Peers peers;
  Wire wire(peers.endpoints);
  msg::Telemetry telemetry{};
  telemetry.x = 3;
  telemetry.metal_detected = 1;
  ASSERT_TRUE(peers.publish_until(
    peers.telemetry, telemetry, [&] { return wire.link().telemetry.has_value(); }));
  EXPECT_DOUBLE_EQ(wire.link().telemetry->x, 3);
  // Every sample is queued for the map, oldest first.
  const auto samples = wire.take_telemetry();
  ASSERT_FALSE(samples.empty());
  EXPECT_DOUBLE_EQ(samples.front().telemetry.x, 3);

  msg::Vision vision{};
  vision.surface_detected = 1;
  vision.confidence = 0.9f;
  ASSERT_TRUE(peers.publish_until(
    peers.vision, vision, [&] { return wire.link().vision.has_value(); }));
  EXPECT_TRUE(link_health(wire.link()).vision);
}
