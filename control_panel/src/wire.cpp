#include "wire.hpp"
#include <algorithm>
#include <cmath>
#include <utility>

namespace {
template<typename T>
void
push_bounded(std::deque<T>& queue, T value, std::size_t limit)
{
  if (queue.size() == limit) {
    queue.pop_front();
  }
  queue.push_back(std::move(value));
}

float
clamp_drive(float value)
{
  return std::isfinite(value)
           ? std::clamp(value, -msg::drive_limit, msg::drive_limit)
           : 0.f;
}
}

Wire::Wire(const WireEndpoints& endpoints)
  : commands(context, zmq::socket_type::push)
  , updates(context, zmq::socket_type::sub)
  , drive(DriveRequest())
  , pending(std::vector<Command>())
  , state(RobotLink())
  , observations(Observations())
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
Wire::set_drive(float forward, float left)
{
  auto guard = drive.unlock();
  guard.data = { clamp_drive(forward), clamp_drive(left), msg::monotonic_ns() };
}

void
Wire::send_pickup()
{
  pending.unlock().data.push_back(msg::Pickup{});
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

std::vector<MetalHit>
Wire::take_metal_hits()
{
  auto guard = observations.unlock();
  std::vector<MetalHit> hits(guard.data.metal.begin(), guard.data.metal.end());
  guard.data.metal.clear();
  return hits;
}

std::vector<VisionResult>
Wire::take_vision()
{
  auto guard = observations.unlock();
  std::vector<VisionResult> results(guard.data.vision.begin(),
                                    guard.data.vision.end());
  guard.data.vision.clear();
  return results;
}

template<typename T>
void
Wire::send(const T& message, std::uint32_t& seq)
{
  // Fails without blocking when no bridge is connected; the command is
  // dropped rather than delivered late.
  (void)commands.send(msg::encode(message, seq), zmq::send_flags::dontwait);
}

void
Wire::send_drive()
{
  const auto request = drive.unlock().data;
  msg::Drive message{};
  if (msg::monotonic_ns() - request.updated_ns <= drive_refresh_timeout_ns) {
    message.forward = request.forward;
    message.left = request.left;
  }
  send(message, drive_seq);
}

void
Wire::send_pending()
{
  std::vector<Command> queued;
  std::swap(queued, pending.unlock().data);
  for (const auto& command : queued) {
    if (const auto* pickup = std::get_if<msg::Pickup>(&command)) {
      send(*pickup, pickup_seq);
    } else {
      send(std::get<msg::ResetPose>(command), reset_seq);
    }
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
        const bool metal = telemetry->metal_detected == 1;
        if (metal && !metal_previous) {
          push_bounded(observations.unlock().data.metal,
                       MetalHit{ *telemetry, now },
                       observation_limit);
        }
        metal_previous = metal;
      }
      break;
    case msg::MessageType::Vision:
      if (const auto vision = msg::decode<msg::Vision>(frame)) {
        {
          auto guard = state.unlock();
          guard.data.vision = *vision;
          guard.data.vision_received_ns = now;
        }
        push_bounded(observations.unlock().data.vision,
                     VisionResult{ *vision, now },
                     observation_limit);
      }
      break;
    default:
      break;
  }
}

void
Wire::run(std::stop_token stop)
{
  using clock = std::chrono::steady_clock;
  // Bounds the latency of one-shot commands queued by the UI.
  constexpr auto max_wait = std::chrono::milliseconds(10);
  auto next_drive = clock::now();
  std::vector<zmq::pollitem_t> items{
    { static_cast<void*>(updates), 0, ZMQ_POLLIN, 0 }
  };
  while (!stop.stop_requested()) {
    send_pending();
    const auto now = clock::now();
    if (now >= next_drive) {
      send_drive();
      next_drive += drive_period;
      if (next_drive <= now) {
        next_drive = now + drive_period;
      }
    }
    const auto wait = std::clamp(
      std::chrono::duration_cast<std::chrono::milliseconds>(next_drive - now),
      std::chrono::milliseconds(0),
      max_wait);
    zmq::poll(items, wait);
    if (items[0].revents & ZMQ_POLLIN) {
      zmq::message_t frame;
      while (updates.recv(frame, zmq::recv_flags::dontwait)) {
        receive(frame);
      }
    }
  }
}
