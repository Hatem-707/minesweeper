#include "wire.hpp"
#include <utility>

Wire::Wire(const WireEndpoints& endpoints)
  : commands(context, zmq::socket_type::push)
  , updates(context, zmq::socket_type::sub)
  , pending(std::vector<msg::ResetPose>())
  , state(RobotLink())
  , samples(std::deque<TelemetrySample>())
{
  // Commands are never queued for an absent bridge and replayed later.
  commands.set(zmq::sockopt::immediate, 1);
  commands.set(zmq::sockopt::sndhwm, 4);
  commands.set(zmq::sockopt::linger, 0);
  commands.connect(endpoints.command);
  updates.set(zmq::sockopt::rcvhwm, 64);
  updates.set(zmq::sockopt::linger, 0);
  updates.set(zmq::sockopt::subscribe, "");
  updates.connect(endpoints.telemetry);
  updates.connect(endpoints.vision);
  worker = std::jthread([this](std::stop_token stop) { run(stop); });
}

void
Wire::send_reset(double x, double y, msg::Heading heading)
{
  msg::ResetPose reset{};
  reset.x = x;
  reset.y = y;
  reset.heading = heading;
  pending.unlock().data.push_back(reset);
}

RobotLink
Wire::link()
{
  return state.unlock().data;
}

std::vector<TelemetrySample>
Wire::take_telemetry()
{
  auto guard = samples.unlock();
  std::vector<TelemetrySample> result(guard.data.begin(), guard.data.end());
  guard.data.clear();
  return result;
}

void
Wire::send_pending()
{
  std::vector<msg::ResetPose> queued;
  std::swap(queued, pending.unlock().data);
  for (const auto& reset : queued) {
    // Fails without blocking when no bridge is connected; the command is
    // dropped rather than delivered late.
    (void)commands.send(msg::encode(reset, reset_seq),
                        zmq::send_flags::dontwait);
  }
}

void
Wire::receive(const zmq::message_t& frame)
{
  const auto now = msg::monotonic_ns();
  switch (msg::peek_type(frame).value_or(msg::MessageType{})) {
    case msg::MessageType::Telemetry:
      if (const auto telemetry = msg::decode<msg::Telemetry>(frame)) {
        {
          auto guard = state.unlock();
          guard.data.telemetry = *telemetry;
          guard.data.telemetry_received_ns = now;
        }
        auto guard = samples.unlock();
        if (guard.data.size() == telemetry_limit) {
          guard.data.pop_front();
        }
        guard.data.push_back({ *telemetry, now });
      }
      break;
    case msg::MessageType::Vision:
      if (const auto vision = msg::decode<msg::Vision>(frame)) {
        auto guard = state.unlock();
        guard.data.vision = *vision;
        guard.data.vision_received_ns = now;
      }
      break;
    default:
      break;
  }
}

void
Wire::run(std::stop_token stop)
{
  // Bounds the latency of reset commands queued by the UI.
  constexpr auto max_wait = std::chrono::milliseconds(10);
  std::vector<zmq::pollitem_t> items{
    { static_cast<void*>(updates), 0, ZMQ_POLLIN, 0 }
  };
  while (!stop.stop_requested()) {
    send_pending();
    zmq::poll(items, max_wait);
    if (items[0].revents & ZMQ_POLLIN) {
      zmq::message_t frame;
      while (updates.recv(frame, zmq::recv_flags::dontwait)) {
        receive(frame);
      }
    }
  }
}
