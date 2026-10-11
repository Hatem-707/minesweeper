#pragma once
// GUI <-> ROS bridge <-> vision IPC contract, v1. See ROS_CONTRACT.md.
//
// One ZeroMQ frame carries one packed struct that starts with Header. The
// first byte is the MessageType, so SUB sockets can prefix-filter on it.
// Flags are std::uint8_t holding exactly 0 or 1; any other value, an unknown
// enum value, a non-finite float or an out-of-range value rejects the message.
#include <bit>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <optional>
#include <span>
#include <zmq.hpp>

namespace msg {
inline constexpr std::uint8_t protocol_version = 1;
// Age reported before a source's first sample.
inline constexpr std::uint32_t never = std::numeric_limits<std::uint32_t>::max();
// Signed motor-command limit for Drive::forward and Drive::left.
inline constexpr float drive_limit = 255;
// Commands older than this on receipt are dropped, never replayed.
inline constexpr std::int64_t command_expiry_ns = 200'000'000;

inline constexpr const char* command_endpoint = "ipc:///tmp/minesweeper/cmd.ipc";
inline constexpr const char* telemetry_endpoint =
  "ipc:///tmp/minesweeper/telemetry.ipc";
inline constexpr const char* vision_endpoint =
  "ipc:///tmp/minesweeper/vision.ipc";

static_assert(std::endian::native == std::endian::little);

enum class MessageType : std::uint8_t
{
  // GUI -> bridge
  Drive = 1,
  Pickup = 2,
  ResetPose = 3,
  // bridge -> GUI
  Telemetry = 16,
  // vision -> GUI
  Vision = 32,
};

// Map-frame heading: value * 90 degrees counterclockwise from +X.
enum class Heading : std::uint8_t
{
  PosX = 0,
  PosY = 1,
  NegX = 2,
  NegY = 3,
};

#pragma pack(push, 1)
struct Header
{
  MessageType type;
  std::uint8_t version;
  std::uint32_t seq;    // Per sender and type; +1 per send.
  std::int64_t sent_ns; // Sender CLOCK_MONOTONIC.
};

// Sent at 20 Hz while running, including zeros and while speed-locked.
// Motor commands, not velocities. Signs follow ROS (REP 103); the bridge
// copies them to Twist.linear.x and Twist.angular.z unchanged.
struct Drive
{
  Header header;
  float forward; // [-drive_limit, drive_limit], positive forward.
  float left;    // [-drive_limit, drive_limit], positive turns left (CCW).
};

// Sent once per accepted press; the message type is the command.
struct Pickup
{
  Header header;
};

// Operator anchor reset of the localization base pose.
struct ResetPose
{
  Header header;
  double x; // Map metres.
  double y;
  Heading heading;
};

// Sent by the bridge at 20 Hz with the latest robot state.
struct Telemetry
{
  Header header;
  std::uint32_t localization_age_ms; // Since the last odometry sample.
  std::uint32_t hardware_age_ms;     // Since the staler of arm/metal samples.
  double x;                          // Map metres.
  double y;
  double heading_rad;   // Counterclockwise from +X.
  float speed_mps;      // Signed forward speed.
  float yaw_rate_rps;   // Positive turns left.
  std::uint8_t arm_upright;
  std::uint8_t metal_detected; // OR of every sample since the last Telemetry.
};

// Sent once per processed frame, including frames with nothing found.
struct Vision
{
  Header header;               // sent_ns is taken after inference.
  std::int64_t frame_ns;       // CLOCK_MONOTONIC when the frame was received.
  std::uint8_t surface_detected; // Inside the inspection region.
  float confidence;            // [0, 1]; 0 when there is no candidate.
};
#pragma pack(pop)

static_assert(sizeof(Header) == 14);
static_assert(sizeof(Drive) == 22);
static_assert(sizeof(Pickup) == 14);
static_assert(sizeof(ResetPose) == 31);
static_assert(sizeof(Telemetry) == 56);
static_assert(sizeof(Vision) == 27);

template<typename T>
struct TypeOf;
template<>
struct TypeOf<Drive>
{
  static constexpr MessageType value = MessageType::Drive;
};
template<>
struct TypeOf<Pickup>
{
  static constexpr MessageType value = MessageType::Pickup;
};
template<>
struct TypeOf<ResetPose>
{
  static constexpr MessageType value = MessageType::ResetPose;
};
template<>
struct TypeOf<Telemetry>
{
  static constexpr MessageType value = MessageType::Telemetry;
};
template<>
struct TypeOf<Vision>
{
  static constexpr MessageType value = MessageType::Vision;
};

// libstdc++ and libc++ implement steady_clock with CLOCK_MONOTONIC, which is
// shared by every process on the host, including Docker containers.
inline std::int64_t
monotonic_ns()
{
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
           std::chrono::steady_clock::now().time_since_epoch())
    .count();
}

namespace detail {
// Members of packed structs are passed by value: binding references to them
// is not allowed.
inline bool
is_flag(std::uint8_t value)
{
  return value <= 1;
}

inline bool
is_drive_value(float value)
{
  return std::isfinite(value) && std::abs(value) <= drive_limit;
}

inline bool
is_valid(const Drive& m)
{
  return is_drive_value(m.forward) && is_drive_value(m.left);
}

inline bool
is_valid(const Pickup&)
{
  return true;
}

inline bool
is_valid(const ResetPose& m)
{
  return std::isfinite(m.x) && std::isfinite(m.y) &&
         std::uint8_t(m.heading) <= std::uint8_t(Heading::NegY);
}

inline bool
is_valid(const Telemetry& m)
{
  return std::isfinite(m.x) && std::isfinite(m.y) &&
         std::isfinite(m.heading_rad) && std::isfinite(m.speed_mps) &&
         std::isfinite(m.yaw_rate_rps) && is_flag(m.arm_upright) &&
         is_flag(m.metal_detected);
}

inline bool
is_valid(const Vision& m)
{
  return is_flag(m.surface_detected) && std::isfinite(m.confidence) &&
         m.confidence >= 0 && m.confidence <= 1;
}
}

// Fills the header and advances seq; the caller owns one seq per type.
template<typename T>
void
stamp(T& message, std::uint32_t& seq, std::int64_t sent_ns = monotonic_ns())
{
  message.header = { .type = TypeOf<T>::value,
                     .version = protocol_version,
                     .seq = seq++,
                     .sent_ns = sent_ns };
}

template<typename T>
zmq::message_t
encode(T message, std::uint32_t& seq)
{
  stamp(message, seq);
  return zmq::message_t(&message, sizeof(message));
}

// Type of a received frame, for dispatching to decode<T>.
inline std::optional<MessageType>
peek_type(std::span<const std::byte> bytes)
{
  if (bytes.empty()) {
    return std::nullopt;
  }
  return MessageType(bytes.front());
}

template<typename T>
std::optional<T>
decode(std::span<const std::byte> bytes)
{
  if (bytes.size() != sizeof(T)) {
    return std::nullopt;
  }
  T message;
  std::memcpy(&message, bytes.data(), sizeof(T));
  if (message.header.type != TypeOf<T>::value ||
      message.header.version != protocol_version ||
      !detail::is_valid(message)) {
    return std::nullopt;
  }
  return message;
}

inline std::span<const std::byte>
bytes_of(const zmq::message_t& frame)
{
  return { static_cast<const std::byte*>(frame.data()), frame.size() };
}

inline std::optional<MessageType>
peek_type(const zmq::message_t& frame)
{
  return peek_type(bytes_of(frame));
}

template<typename T>
std::optional<T>
decode(const zmq::message_t& frame)
{
  return decode<T>(bytes_of(frame));
}

// For commands: true when the sender's timestamp is too old to act on.
inline bool
is_expired(const Header& header, std::int64_t now_ns = monotonic_ns())
{
  return now_ns - header.sent_ns > command_expiry_ns;
}
}
