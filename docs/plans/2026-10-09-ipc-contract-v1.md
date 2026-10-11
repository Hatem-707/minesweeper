# GUI ⇄ ROS bridge ⇄ vision message contract (v1)

## Context

`control_panel/src/messages.hpp` is a stale sketch (int16 velocity, a `MineUpdate`
that assumes the Pi classifies mines, a `Telemetry` that clashes with the one in
`state.hpp`, an unused `MessageMeta`). `../ROS_CONTRACT.md` has sound ROS-side
ideas but a JSON/ROUTER-DEALER wire format and over-engineered parts (service-based
reset, per-sensor ages, ROS timestamps in vision). This plan freezes v1 so
`src/wire.hpp` (empty shell), a C++ rclcpp bridge and a C++ ONNX process all
`#include` one header.

Decisions (all confirmed in review):
- Everything is C++ (GUI, bridge, ONNX). ONNX stays a **separate process**
  (decoupled from the GUI) and reuses `GstCamera` (`src/camera.cpp`) for RTSP.
- **Single-part** ZMQ messages: one frame = one packed struct; the type is the
  header's first byte (`MessageType : uint8_t`), so SUB prefix filtering still works.
- Mines are mapped in the GUI; the bridge forwards pose + raw metal flag only.
- Vision goes ONNX → GUI over ZMQ, never ROS. No ROS/robot time on the wire:
  everything is stamped with laptop `CLOCK_MONOTONIC`.
- Health is reported **per Pi node** (localization, hardware interface).
- Reset is a fire-and-forget topic; failure is visible in the next pose reading.
  It sets x, y and heading snapped to one of four directions.
- `/cmd_vel` uses standard `linear.x` forward.
- Flags are `std::uint8_t`, 0/1 only (any other value rejects the message —
  `memcpy` of a non-0/1 byte into `bool` is UB and can't be validated afterwards).
- Out of scope now: GUI reset UI, vision-latency calibration.

## 1. Topology

```text
 GUI  PUSH ──connect──▶ ipc:///tmp/minesweeper/cmd.ipc       ◀──bind── PULL  bridge ──DDS──▶ Pi
 GUI  SUB  ──connect──▶ ipc:///tmp/minesweeper/telemetry.ipc ◀──bind── PUB   bridge ◀─DDS──  Pi
          └─connect──▶ ipc:///tmp/minesweeper/vision.ipc     ◀──bind── PUB   onnx (RTSP via GstCamera)
```

- One GUI SUB socket connects to both PUB endpoints; `MessageType` distinguishes them.
- All endpoints in `/tmp/minesweeper/` → one bind mount for the Dockerized bridge.
  Host and container share the kernel `CLOCK_MONOTONIC`.
- GUI PUSH: `ZMQ_IMMEDIATE=1`, `ZMQ_SNDHWM=4`, `ZMQ_LINGER=0`, `dontwait` sends —
  commands are never queued for an absent bridge and replayed later.
- Receivers drain everything available per poll and act on the newest message.

## 2. Framing & validation

- One frame = one `#pragma pack(1)` struct starting with `Header`. Little-endian
  (`static_assert(std::endian::native == std::endian::little)`).
- Drop a message if: size ≠ `sizeof` the struct for its type, unknown type,
  `version ≠ protocol_version`, flag byte ∉ {0,1}, enum out of range,
  non-finite float, or value out of range.
- `seq`: per sender, per type, +1 each send — detects drops and peer restarts.
- `sent_ns`: sender `CLOCK_MONOTONIC`. Commands older than 200 ms on receipt are dropped.

## 3. Structs (`messages.hpp`, namespace `msg`)

```cpp
#pragma once
#include <bit>
#include <cstdint>

namespace msg {
inline constexpr std::uint8_t protocol_version = 1;
inline constexpr std::uint32_t never = UINT32_MAX;   // age before first sample
static_assert(std::endian::native == std::endian::little);

enum class MessageType : std::uint8_t {
  Drive = 1, Pickup = 2, ResetPose = 3,   // GUI -> bridge
  Telemetry = 16,                         // bridge -> GUI
  Vision = 32,                            // onnx -> GUI
};

// Map-frame heading; value * 90° CCW from +X.
enum class Heading : std::uint8_t { PosX = 0, PosY = 1, NegX = 2, NegY = 3 };

#pragma pack(push, 1)
struct Header {
  MessageType   type;      // first byte: SUB prefix filter
  std::uint8_t  version;   // protocol_version
  std::uint32_t seq;
  std::int64_t  sent_ns;   // sender CLOCK_MONOTONIC
};

struct Drive {             // 20 Hz while running, zeros and speed-lock included
  Header header;
  float forward;           // [-255, 255], + forward    (DriveState::linear)
  float left;              // [-255, 255], + turn left / CCW (DriveState::angular)
};

struct Pickup {            // once per accepted press; the type is the command
  Header header;
};

struct ResetPose {         // operator anchor reset
  Header  header;
  double  x;               // map metres
  double  y;
  Heading heading;
};

struct Telemetry {         // bridge 20 Hz timer, latest robot state
  Header header;
  std::uint32_t localization_age_ms;  // since last /odometry/filtered; never = none yet
  std::uint32_t hardware_age_ms;      // since the staler of /arm_upright, /metal_detected
  double x;                           // map metres
  double y;
  double heading_rad;                 // yaw, CCW from +X
  float  speed_mps;                   // twist.linear.x
  float  yaw_rate_rps;                // twist.angular.z, + left
  std::uint8_t arm_upright;           // 0/1
  std::uint8_t metal_detected;        // 0/1, OR of all samples since last Telemetry
};

struct Vision {            // one per processed frame, including "nothing found"
  Header header;           // sent_ns = after inference
  std::int64_t frame_ns;   // CLOCK_MONOTONIC when the appsink delivered the frame
  std::uint8_t surface_detected;  // 0/1, in the inspection region
  float confidence;        // [0, 1], 0 when no candidate
};
#pragma pack(pop)

static_assert(sizeof(Header) == 14);
static_assert(sizeof(Drive) == 22);
static_assert(sizeof(Pickup) == 14);
static_assert(sizeof(ResetPose) == 31);
static_assert(sizeof(Telemetry) == 56);
static_assert(sizeof(Vision) == 27);

template<typename T> struct TypeOf;   // TypeOf<Drive>::value == MessageType::Drive ...
template<typename T> zmq::message_t encode(T&, std::uint32_t& seq);   // fills header
template<typename T> std::optional<T> decode(const zmq::message_t&);  // full §2 validation
}
```

Notes:
- `metal_detected` is OR-latched between sends so a short pulse between 20 Hz
  ticks is never lost. Pose and metal arrive in the same message, so a mine's
  position is the pose in the `Telemetry` where metal rises (+ detector offset).
- Vision ↔ metal association compares `Vision::frame_ns` with `Telemetry` receipt
  time (both laptop monotonic). `sent_ns − frame_ns` = inference latency.
- Before a node's first sample its fields are 0 and its age is `never`; the GUI
  shows "no data", not zeros.

## 4. Wire ⇄ ROS mapping (bridge)

| Wire type | Dir | ROS topic | ROS type | QoS | Translation |
|---|---|---|---|---|---|
| `Drive` | GUI→ROS | `/cmd_vel` | `geometry_msgs/msg/Twist` | best effort, keep last 1, lifespan 200 ms | `linear.x = forward`, `angular.z = left`, rest 0. Values are motor commands ±255, not m/s |
| `Pickup` | GUI→ROS | `/cmd_pickup` | `std_msgs/msg/UInt8` | reliable, keep last 1 | publish `data = 1` once |
| `ResetPose` | GUI→ROS | `/reset_pose` | `geometry_msgs/msg/PoseStamped` | reliable, keep last 1, volatile | `frame_id = "map"`, position `x, y, 0`, orientation = yaw `heading·π/2` as quaternion; Kalman node replaces its base pose |
| `Telemetry` | ROS→GUI | `/odometry/filtered` | `nav_msgs/msg/Odometry` | best effort, keep last 1 | x, y, yaw from quaternion, `twist.linear.x`, `twist.angular.z` → `localization_age_ms` |
| ″ | ″ | `/arm_upright` | `std_msgs/msg/Bool` | best effort, keep last 1 | latest value → `hardware_age_ms` |
| ″ | ″ | `/metal_detected` | `std_msgs/msg/Bool` | reliable, keep last 20 | OR-latched until next send → `hardware_age_ms` |
| `Vision` | ONNX→GUI | — | — | — | not ROS |

Standard message types only → no custom `minesweeper_interfaces` package.
Frame conventions: `map` frame, origin at the chosen start corner, +X right,
+Y up, heading 0 = +X, CCW positive; GUI inverts Y only when drawing.

## 5. Timing / safety

- GUI sends `Drive` at 20 Hz on its own timer (not FPS-bound), including zeros;
  focus loss is already zeroed by `DriveController` (`src/drive.cpp`).
- Bridge drops commands with `now − sent_ns > 200 ms`, and publishes one zero
  Twist when there's no fresh `Drive` for 200 ms. Pi timeout + ESP32 250 ms watchdog stay.
- `Pickup` only from `PickupController::take_command()` (`src/pickup.hpp`); the
  200 ms age check + `ZMQ_IMMEDIATE` prevent late delivery after reconnect.
- GUI freshness: bridge stale if no `Telemetry` for 500 ms; each node stale if
  its age > 500 ms; vision stale if no `Vision` for 500 ms. Stale hardware ⇒ arm
  treated as not upright (blocks pickup). Stale values stay visible, greyed out.

## 6. Implementation steps

1. **Shared header** at `../protocol/include/minesweeper/messages.hpp` (dep: cppzmq
   for encode/decode); control_panel, bridge and onnx add that include dir.
   Delete `src/messages.hpp`.
2. **GUI model.** Replace `Telemetry` in `src/state.hpp` with the last
   `msg::Telemetry` + receipt time (drop `health`; show per-node health instead),
   still in `SpinLock` (`src/spin_lock.hpp`). Update `TableControls::self_draw`
   and `PickupControl::self_update` in `src/ui.cpp`.
3. **Wire** (`src/wire.hpp/.cpp`): context, PUSH, SUB; a `std::jthread` polls SUB,
   updates the snapshot, queues Vision/metal edges for the future mapping module;
   20 Hz drive sender reading `DriveController::state()`; `send_pickup()`,
   `send_reset(x, y, heading)` (API only — GUI reset UI deferred). PUSH used only
   from the wire thread; UI posts one-shot commands through a small locked queue.
   Remove the demo `test_thread` in `src/main.cpp`.
4. **Bridge** (`../ros_vol/src/minesweeper_bridge`, rclcpp, ament_cmake): PULL +
   PUB, 20 Hz telemetry timer, drive-expiry timer, translations from §4.
5. **ONNX** executable (later): `GstCamera` → ONNX Runtime C++ → PUB `Vision`.
6. Rewrite `../ROS_CONTRACT.md` to match this v1 (keep mapping rationale).

## 7. Verification

- `tests/messages_tests.cpp` in `controller_tests`: round-trip each type; reject
  wrong size/type/version, flag byte 2, heading 4, NaN, drive > 255; seq increments.
- Bridge without robot: `ros2 topic echo /cmd_vel` while driving (forward on
  `linear.x`, left = positive `angular.z`, zero 200 ms after killing the GUI);
  `ros2 topic pub` on `/odometry/filtered`, `/arm_upright`, `/metal_detected` →
  GUI updates; stop one publisher → only that node goes stale.
- A one-sample `/metal_detected` true pulse between ticks still reaches the GUI.
- Send a `ResetPose` from a test client → `ros2 topic echo /reset_pose` shows
  correct x/y and yaw quaternion for each of the four headings.
- Restart bridge mid-drive: robot receives zero, GUI shows bridge stale, no motion
  resumes until a new key press.

## 8. Remaining suggestions (non-blocking)

- Reset loss vs. reset failure look the same in the pose reading; that's fine
  for an operator watching the map, just don't build automation that assumes delivery.
- Vision `frame_ns` is receipt time, so it lags the real capture by the RTSP
  pipeline delay. Until that's characterised, confirm vision while the robot
  is stationary; no timestamping work planned now.

## 9. Implementation status (2026-10-09)

| Step | Status | Where |
|---|---|---|
| 1. Shared header | Done, tested | `protocol/include/minesweeper/messages.hpp`, `control_panel/tests/messages_tests.cpp` |
| 2. GUI model | Done | `control_panel/src/link.hpp/.cpp` (`RobotLink`, `link_health`, `arm_upright`); `Telemetry` removed from `state.hpp`; table shows Link/Arm/Pos/Speed/Heading |
| 3. Wire | Done, tested over real ipc sockets | `control_panel/src/wire.hpp/.cpp`, `control_panel/tests/wire_tests.cpp`; demo thread removed from `main.cpp`. Extra safety: drive falls back to zero if the UI stops calling `set_drive` for 200 ms |
| 4. Bridge | Done; builds in the Dockerfile image (ROS lyrical) with no warnings; integration-tested against the real `Wire` (drive/zero fallback/expiry stop, pickup, reset quaternion, telemetry decode, metal pulse latch, odometry and bridge staleness) | `ros_vol/src/minesweeper_bridge`; Dockerfile installs `libzmq3-dev cppzmq-dev` |
| 5. ONNX vision | **Deferred.** Prep done: GStreamer capture split into `control_panel/src/camera_source.hpp/.cpp` (no raylib, configurable URI, `CameraBuffer::frame_ns`) | — |
| 6. Docs | Done | `ROS_CONTRACT.md`, `control_panel/UI_CONFIGURATION.md` |

Also: `styles.toml` `[Drive] maximum_thrust` is now capped at `msg::drive_limit` (255).
GUI reset UI is deferred; `Wire::send_reset(x, y, heading)` exists.
`Wire::take_metal_hits()` / `take_vision()` are ready for the mapping module.

Revision (2026-10-09): turn sign switched to the ROS convention end to end. `DriveState::angular` and `msg::Drive::left` are positive for a left (counterclockwise) turn; the bridge copies it to `angular.z` without negation.
