# GUI / ROS / vision contract — v1

Frozen October 9, 2026. The wire format is defined in code by
`protocol/include/minesweeper/messages.hpp`; this document explains it. If they
disagree, the header wins and this document is fixed.

## Architecture and ownership

```text
 GUI  PUSH ──connect──▶ ipc:///tmp/minesweeper/cmd.ipc       ◀──bind── PULL  ROS bridge ──DDS──▶ Pi
 GUI  SUB  ──connect──▶ ipc:///tmp/minesweeper/telemetry.ipc ◀──bind── PUB   ROS bridge ◀─DDS──  Pi
          └─connect──▶ ipc:///tmp/minesweeper/vision.ipc     ◀──bind── PUB   vision (ONNX, RTSP)
 Robot RTSP ─────────────────────────────────────────────▶ GUI camera view
```

- All three processes are C++ and include the same header.
- **ROS bridge** (rclcpp): translates messages only. No mapping, no fusion.
- **Vision process**: separate executable, reuses `GstCamera` to read RTSP, runs
  ONNX Runtime, publishes one `Vision` result per frame. No ROS. Kept separate
  so an inference stall or crash cannot freeze the GUI.
- **GUI**: owns mine mapping (IDs, coordinates, classification, dedup, persistence).
  No ROS mine topic exists.
- The Dockerized bridge needs one bind mount: `-v /tmp/minesweeper:/tmp/minesweeper`.

## 1. Wire format

- One ZeroMQ frame = one `#pragma pack(1)` struct starting with `Header`,
  little-endian, IEEE-754 floats.
- `Header.type` (`MessageType`, 1 byte) is the first byte, so a SUB socket can
  prefix-filter on it. One GUI SUB socket connects to both PUB endpoints.
- `Header.version` must equal `protocol_version` (1).
- `Header.seq`: per sender and type, +1 per send; reveals drops and restarts.
- `Header.sent_ns`: sender `CLOCK_MONOTONIC`, shared by all host processes and
  containers. Commands older than 200 ms on receipt are dropped.
- **Booleans are `std::uint8_t`, exactly 0 or 1.** Not `bool`: copying a byte
  other than 0/1 into a `bool` is undefined behaviour, so it cannot be validated
  after decoding.
- A receiver drops a message with the wrong size for its type, unknown type,
  wrong version, a flag outside {0, 1}, an unknown enum value, a non-finite
  float or an out-of-range value. `msg::decode<T>` does all of this.
- No robot/ROS time crosses the wire. Everything the GUI correlates is laptop
  monotonic time, so no robot↔laptop clock sync is needed.

GUI PUSH socket: `ZMQ_IMMEDIATE=1`, `ZMQ_SNDHWM=4`, `ZMQ_LINGER=0`, non-blocking
sends. Commands are never queued for an absent bridge and replayed later.

## 2. Messages

| Type | Id | Dir | Size | Fields |
|---|---:|---|---:|---|
| `Drive` | 1 | GUI → bridge | 22 | `float forward, left` in [-255, 255] |
| `Pickup` | 2 | GUI → bridge | 14 | header only |
| `ResetPose` | 3 | GUI → bridge | 31 | `double x, y` (m), `Heading heading` |
| `Telemetry` | 16 | bridge → GUI | 56 | `u32 localization_age_ms, hardware_age_ms`, `double x, y, heading_rad`, `float speed_mps, yaw_rate_rps`, `u8 arm_upright, metal_detected` |
| `Vision` | 32 | vision → GUI | 27 | `i64 frame_ns`, `u8 surface_detected`, `float confidence` [0, 1] |

`Heading` is one of `PosX, PosY, NegX, NegY` (value × 90° CCW from +X).

## 3. ROS mapping (bridge)

All standard message types; no custom interface package.

| Wire | ROS topic | ROS type | QoS | Translation |
|---|---|---|---|---|
| `Drive` | `/cmd_vel` | `geometry_msgs/msg/Twist` | best effort, keep last 1, lifespan 200 ms | `linear.x = forward`, `angular.z = left`, all else 0 |
| `Pickup` | `/cmd_pickup` | `std_msgs/msg/UInt8` | reliable, keep last 1 | publish `data = 1` once |
| `ResetPose` | `/reset_pose` | `geometry_msgs/msg/PoseStamped` | reliable, keep last 1, volatile | `frame_id = "map"`, position `(x, y, 0)`, orientation = yaw `heading · π/2` |
| `Telemetry` | `/odometry/filtered` | `nav_msgs/msg/Odometry` | best effort, keep last 1 | pose x/y, yaw from quaternion, `twist.linear.x`, `twist.angular.z` |
| ″ | `/arm_upright` | `std_msgs/msg/Bool` | best effort, keep last 1 | latest value |
| ″ | `/metal_detected` | `std_msgs/msg/Bool` | reliable, keep last 20 | OR-latched until the next `Telemetry` |
| `Vision` | — | — | — | not ROS |

### Velocity

`/cmd_vel` carries **motor commands in [-255, 255], not m/s or rad/s**, despite
the Twist type. Positive `linear.x` is forward; positive `angular.z` turns left
(counterclockwise, REP 103). The GUI uses the same signs end to end, so the
bridge copies both values unchanged. The hardware
interface does final motor mixing, limiting and rounding.

| Action | linear.x | angular.z |
|---|---:|---:|
| Stop | 0 | 0 |
| Forward | 100 | 0 |
| Reverse | -100 | 0 |
| Turn left in place | 0 | 100 |
| Forward and right | 100 | -50 |

### Pickup

The micro-controller runs the whole pickup sequence (arm and magnet) from one
command. The GUI sends `Pickup` once per accepted press, only while the arm reads
upright; the robot should also ignore it when the arm isn't upright or a sequence
is running. No abort command exists.

### Reset

Fire-and-forget. The Kalman node replaces its base pose with the given x/y and
the heading snapped to one of four directions. There is no reply: a reset that
didn't apply (or was lost) shows up as the next pose not matching. Reset does
not erase or shift recorded mines.

### Pose conventions

`map` frame; origin at the chosen start corner; +X right across the map, +Y up;
heading 0 faces +X and increases counterclockwise. The GUI inverts Y only when drawing.

## 4. Telemetry and health

The bridge sends `Telemetry` on its own 20 Hz timer with the latest values.

- Health is per Pi node. `localization_age_ms` = time since the last
  `/odometry/filtered`; `hardware_age_ms` = time since the staler of
  `/arm_upright` and `/metal_detected` (both from the hardware interface).
  Before a node's first sample its fields are 0 and its age is `msg::never`.
- `metal_detected` is the OR of every sample since the last `Telemetry`, so a
  short pulse between ticks is never lost.
- GUI freshness: bridge stale after 500 ms without `Telemetry`; a node stale when
  its age exceeds 500 ms; vision stale after 500 ms without `Vision`. Stale values
  stay visible but greyed, never replaced by zeros. Stale hardware means the arm
  is treated as not upright, which blocks pickup.

## 5. Command timing and safety

- The GUI sends `Drive` at 20 Hz on its own timer, including zeros and while
  speed-locked. Focus loss zeroes drive and clears the lock.
- The bridge drops commands older than 200 ms and publishes one zero Twist when
  no fresh `Drive` arrives for 200 ms. It never republishes stale commands.
- The Pi applies its own motion timeout; the ESP32 keeps its 250 ms watchdog.
  These are per-stage timeouts, not an end-to-end stopping guarantee.
- Reconnecting never resumes motion: a new key press is required.
- The bridge is the only GUI-side publisher of `/cmd_vel` and `/cmd_pickup`.

## 6. Mapping (GUI)

1. A rising `metal_detected` in a `Telemetry` starts one detection candidate.
   Its position is that same message's pose, plus the configured detector offset
   `(dx, dy)` rotated by the heading:
   `mine_x = x + dx·cos θ − dy·sin θ`, `mine_y = y + dx·sin θ + dy·cos θ`.
2. Vision evidence is matched by comparing `Vision.frame_ns` with the
   `Telemetry` receipt time (both laptop monotonic). `frame_ns` is receipt time
   at the vision process, so it lags the real capture by the RTSP pipeline delay;
   until that is characterised, confirm while stationary.
3. Metal + fresh positive vision → SURFACE. Metal + fresh negative vision →
   BURIED candidate. Metal with stale/no vision → UNKNOWN. Vision alone never
   creates a located mine.
4. Repeated hits within a configurable distance merge into the existing mine.
   Falling metal rearms detection; it never deletes a mine.
5. Records (ID, x/y, classification, time, evidence) persist on the laptop.

## 7. Vision process

- One `Vision` per processed frame, including frames with nothing found.
- `surface_detected` means a qualifying detection inside the configured
  inspection region near the metal detector, not anywhere in the image.
  `confidence` is the best score in that region, 0 if none.
- If the camera or inference fails, publish nothing; the GUI's 500 ms
  freshness marks vision unavailable. Never send made-up negatives.
- `sent_ns − frame_ns` is the inference latency.

## References

- `minesweeper.pdf`, sections 4–5 and Figure 1.
- [Twist](https://github.com/ros2/common_interfaces/blob/rolling/geometry_msgs/msg/Twist.msg),
  [Odometry](https://github.com/ros2/common_interfaces/blob/rolling/nav_msgs/msg/Odometry.msg),
  [PoseStamped](https://github.com/ros2/common_interfaces/blob/rolling/geometry_msgs/msg/PoseStamped.msg)
- [REP 103 units and coordinate conventions](https://github.com/ros-infrastructure/rep/blob/master/rep-0103.rst)
- [ZeroMQ socket patterns](https://libzmq.readthedocs.io/en/latest/zmq_socket.html)
