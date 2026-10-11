#pragma once
#include <chrono>
#include <cstdint>
#include <deque>
#include <minesweeper/messages.hpp>
#include <stop_token>
#include <string>
#include <thread>
#include <variant>
#include <vector>
#include <zmq.hpp>

#include "link.hpp"
#include "spin_lock.hpp"

struct WireEndpoints
{
  std::string command = msg::command_endpoint;
  std::string telemetry = msg::telemetry_endpoint;
  std::string vision = msg::vision_endpoint;
};

// A Telemetry in which metal_detected rose from 0 to 1.
struct MetalHit
{
  msg::Telemetry telemetry;
  std::int64_t received_ns;
};

struct VisionResult
{
  msg::Vision vision;
  std::int64_t received_ns;
};

// Owns the ZeroMQ sockets to the ROS bridge and the vision process. A worker
// thread sends Drive at 20 Hz, forwards one-shot commands and receives
// updates; the UI thread only touches the locked snapshots below.
class Wire
{
public:
  static constexpr auto drive_period = std::chrono::milliseconds(50);
  // Drive falls back to zero when the UI stops refreshing it, e.g. a hung
  // render loop, so a frozen GUI cannot keep the robot moving.
  static constexpr std::int64_t drive_refresh_timeout_ns = 200'000'000;
  static constexpr std::size_t observation_limit = 256;

  explicit Wire(const WireEndpoints& endpoints = {});
  Wire(const Wire&) = delete;
  Wire& operator=(const Wire&) = delete;

  // Called every UI frame with the current motor commands.
  void set_drive(float forward, float left);
  void send_pickup();
  void send_reset(double x, double y, msg::Heading heading);

  RobotLink link();
  // Drained by the mapping module.
  std::vector<MetalHit> take_metal_hits();
  std::vector<VisionResult> take_vision();

private:
  struct DriveRequest
  {
    float forward = 0;
    float left = 0;
    std::int64_t updated_ns = 0;
  };
  struct Observations
  {
    std::deque<MetalHit> metal;
    std::deque<VisionResult> vision;
  };
  using Command = std::variant<msg::Pickup, msg::ResetPose>;

  zmq::context_t context;
  zmq::socket_t commands;
  zmq::socket_t updates;
  SpinLock<DriveRequest> drive;
  SpinLock<std::vector<Command>> pending;
  SpinLock<RobotLink> state;
  SpinLock<Observations> observations;
  // Only used by the worker thread.
  std::uint32_t drive_seq = 0;
  std::uint32_t pickup_seq = 0;
  std::uint32_t reset_seq = 0;
  bool metal_previous = false;
  // Declared last: stopped and joined before the sockets close.
  std::jthread worker;

  void run(std::stop_token stop);
  void send_drive();
  void send_pending();
  void receive(const zmq::message_t& frame);
  template<typename T>
  void send(const T& message, std::uint32_t& seq);
};
