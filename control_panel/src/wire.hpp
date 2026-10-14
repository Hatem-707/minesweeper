#pragma once
#include <chrono>
#include <cstdint>
#include <deque>
#include <minesweeper/messages.hpp>
#include <stop_token>
#include <string>
#include <thread>
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

struct TelemetrySample
{
  msg::Telemetry telemetry;
  std::int64_t received_ns;
};

// Owns the ZeroMQ sockets to the ROS bridge and the vision process. A worker
// thread forwards reset commands and receives updates; the UI thread only
// touches the locked snapshots below.
class Wire
{
public:
  static constexpr std::size_t telemetry_limit = 256;

  explicit Wire(const WireEndpoints& endpoints = {});
  Wire(const Wire&) = delete;
  Wire& operator=(const Wire&) = delete;

  void send_reset(double x, double y, msg::Heading heading);

  RobotLink link();
  // Every Telemetry since the last call, oldest first; drained by the map.
  std::vector<TelemetrySample> take_telemetry();

private:
  zmq::context_t context;
  zmq::socket_t commands;
  zmq::socket_t updates;
  SpinLock<std::vector<msg::ResetPose>> pending;
  SpinLock<RobotLink> state;
  SpinLock<std::deque<TelemetrySample>> samples;
  // Only used by the worker thread.
  std::uint32_t reset_seq = 0;
  // Declared last: stopped and joined before the sockets close.
  std::jthread worker;

  void run(std::stop_token stop);
  void send_pending();
  void receive(const zmq::message_t& frame);
};
