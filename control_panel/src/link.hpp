#pragma once
#include <cstdint>
#include <minesweeper/messages.hpp>
#include <optional>

// Latest messages received from the bridge and the vision process, with the
// local CLOCK_MONOTONIC time each one arrived.
struct RobotLink
{
  std::optional<msg::Telemetry> telemetry;
  std::int64_t telemetry_received_ns = 0;
  std::optional<msg::Vision> vision;
  std::int64_t vision_received_ns = 0;
};

struct LinkHealth
{
  bool bridge = false;       // Telemetry is arriving.
  bool localization = false; // Odometry is fresh at the bridge.
  bool hardware = false;     // Arm and metal readings are fresh at the bridge.
  bool vision = false;       // Vision results are arriving.
};

inline constexpr std::int64_t stale_after_ns = 500'000'000;
inline constexpr std::uint32_t stale_after_ms = 500;

// A stale source is never healthy, whatever its last reported values were.
LinkHealth
link_health(const RobotLink& link, std::int64_t now_ns = msg::monotonic_ns());

// Stale or missing hardware feedback reads as not upright, which blocks pickup.
bool
arm_upright(const RobotLink& link, const LinkHealth& health);
