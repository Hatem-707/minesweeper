# GUI / ROS / vision contract — v2

v2, October 15, 2026: the robot is driven by RC straight to the motors. The GUI
is a map and camera stream; its only command is `ResetPose`. v1 (October 9)
had `Drive` and `Pickup`. The wire format is defined in code by
`protocol/include/minesweeper/messages.hpp`; this document explains it. If they
disagree, the header wins and this document is fixed.

## Architecture and ownership

```text
 GUI  PUSH ──connect──▶ ipc:///tmp/minesweeper/cmd.ipc       ◀──bind── PULL  ROS bridge ──DDS──▶ Pi (/reset_pose)
 GUI  SUB  ──connect──▶ ipc:///tmp/minesweeper/telemetry.ipc ◀──bind── PUB   ROS bridge ◀─DDS──  Pi
          └─connect──▶ ipc:///tmp/minesweeper/vision.ipc     ◀──bind── PUB   vision (ONNX, RTSP)
 Robot RTSP ─────────────────────────────────────────────▶ GUI camera view
```

- All three processes are C++ and include the same header.
- **ROS bridge** (rclcpp): translates messages only. No mapping, no fusion.
- **Vision process**: separate executable, reuses `GstCamera` to read RTSP, runs
  ONNX Runtime, publishes one `Vision` result per frame. No ROS. Kept separate
  so an inference stall or crash cannot freeze the GUI.
- **GUI**: owns mine mapping (IDs, coordinates, classification, dedup), kept in
  memory for the session. No ROS mine topic exists.
- The Dockerized bridge needs one bind mount: `-v /tmp/minesweeper:/tmp/minesweeper`.

## 1. Wire format

- One ZeroMQ frame = one `#pragma pack(1)` struct starting with `Header`,
  little-endian, IEEE-754 floats.
- `Header.type` (`MessageType`, 1 byte) is the first byte, so a SUB socket can
  prefix-filter on it. One GUI SUB socket connects to both PUB endpoints.
- `Header.version` must equal `protocol_version` (2).
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
sends. A reset is never queued for an absent bridge and replayed later.

## 2. Messages

| Type | Id | Dir | Size | Fields |
|---|---:|---|---:|---|
| `ResetPose` | 3 | GUI → bridge | 31 | `double x, y` (m), `Heading heading` |
| `Telemetry` | 16 | bridge → GUI | 48 | `u32 localization_age_ms, hardware_age_ms`, `double x, y, heading_rad`, `u8 arm_upright, metal_detected` |
| `Vision` | 32 | vision → GUI | 27 | `i64 frame_ns`, `u8 surface_detected`, `float confidence` [0, 1] |

`Heading` is one of `PosX, PosY, NegX, NegY` (value × 90° CCW from +X).

## 3. ROS mapping (bridge)

All standard message types; no custom interface package. The bridge publishes
nothing except `/reset_pose`; motors and the arm are driven by RC.

| Wire | ROS topic | ROS type | QoS | Translation |
|---|---|---|---|---|
| `ResetPose` | `/reset_pose` | `geometry_msgs/msg/PoseStamped` | reliable, keep last 1, volatile | `frame_id = "map"`, position `(x, y, 0)`, orientation = yaw `heading · π/2` |
| `Telemetry` | `/odometry/filtered` | `nav_msgs/msg/Odometry` | best effort, keep last 1 | pose x/y, yaw from quaternion |
| ″ | `/contact_switch` | `std_msgs/msg/UInt8` | best effort, keep last 10 | `arm_upright` = latest value ≠ 0 (1 = arm upright) |
| ″ | `/metal_detector` | `std_msgs/msg/UInt8` | best effort, keep last 20 | `metal_detected`: value ≠ 0, OR-latched until the next `Telemetry` |
| `Vision` | — | — | — | not ROS |

The sensor subscriptions are best effort so they match reliable and best-effort
publishers alike.

### Reset

Fire-and-forget. The Kalman node replaces its base pose with the given x/y and
the heading snapped to one of four directions. There is no reply: a reset that
didn't apply (or was lost) shows up as the next pose not matching. Reset does
not erase or shift recorded mines. Resets older than 200 ms on receipt are
dropped.

In the GUI: right-click a map cell, choose the heading with the arrow keys
(Up = +Y, Right = +X, Down = −Y, Left = −X; shown as an arrow on the cell),
then press Enter. Esc cancels. The pose sent is the cell centre.

### Pose conventions

`map` frame; origin at the chosen start corner; +X right across the map, +Y up;
heading 0 faces +X and increases counterclockwise. The GUI inverts Y only when drawing.

## 4. Telemetry and health

The bridge sends `Telemetry` on its own 20 Hz timer with the latest values.

- Health is per Pi node. `localization_age_ms` = time since the last
  `/odometry/filtered`; `hardware_age_ms` = time since the staler of
  `/contact_switch` and `/metal_detector`. Before a node's first sample its
  fields are 0 and its age is `msg::never`.
- `metal_detected` is the OR of every sample since the last `Telemetry`, so a
  short pulse between ticks is never lost.
- GUI freshness: bridge stale after 500 ms without `Telemetry`; a node stale when
  its age exceeds 500 ms; vision stale after 500 ms without `Vision`. Stale values
  stay visible but greyed, never replaced by zeros.

## 5. Mapping (GUI)

The field is a 20 × 20 grid over `Map.field_size` metres (20 m by default).

1. The detector position is the pose plus the configured offset `(dx, dy)`
   rotated by the heading:
   `det_x = x + dx·cos θ − dy·sin θ`, `det_y = y + dx·sin θ + dy·cos θ`.
   While localization is fresh, the cell under the detector turns *clean*.
2. A rising `metal_detected` (with fresh localization and hardware) starts a
   *pending* mine in the detector's cell.
3. If `arm_upright` reads 0 (the operator lowered the arm to pick it up) within
   `Classification.seconds`, and before the robot moves
   `Classification.distance` metres, the mine is **SURFACE**. Otherwise it is
   **BURIED**.
4. A new hit in another cell closes the pending one as BURIED. Repeat hits in
   the same cell merge; a surface mine is never downgraded to buried.
5. Stale localization or hardware never creates a mine; the GUI logs why.
6. The map lives in memory for the session. Vision is shown as link health only
   and is not used for classification.

## 6. Vision process

- One `Vision` per processed frame, including frames with nothing found.
- `surface_detected` means a qualifying detection inside the configured
  inspection region near the metal detector, not anywhere in the image.
  `confidence` is the best score in that region, 0 if none.
- If the camera or inference fails, publish nothing; the GUI's 500 ms
  freshness marks vision unavailable. Never send made-up negatives.
- `sent_ns − frame_ns` is the inference latency.

## References

- `minesweeper.pdf`, sections 4–5 and Figure 1.
- [Odometry](https://github.com/ros2/common_interfaces/blob/rolling/nav_msgs/msg/Odometry.msg),
  [PoseStamped](https://github.com/ros2/common_interfaces/blob/rolling/geometry_msgs/msg/PoseStamped.msg)
- [REP 103 units and coordinate conventions](https://github.com/ros-infrastructure/rep/blob/master/rep-0103.rst)
- [ZeroMQ socket patterns](https://libzmq.readthedocs.io/en/latest/zmq_socket.html)
