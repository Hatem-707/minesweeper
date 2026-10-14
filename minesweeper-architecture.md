# Minesweeper Robot — Software Architecture

> Source: `minesweeper.pdf` (June 25, 2026). Converted to Markdown for agent/LLM consumption — preserves structure, topic names, and requirements as explicit facts/lists rather than prose paragraphs, to make it easier to parse programmatically.

## Abstract

Autonomous/teleoperated Minesweeper Robot for a 20x20 meter competition field.

- **Dual-compute architecture**: Raspberry Pi 4 (high-level ROS 2 logic) + ESP32 (real-time hardware interfacing)
- **Localization**: custom Kalman filter
- **Mine detection**: sensor fusion of analog metal detection + ONNX-based object detection
- **Deployment**: Dockerized
- **GUI decoupling**: a dedicated ROS2-to-UDS bridge on the laptop subscribes to all ROS 2 topics (including ONNX detection results) and forwards consolidated telemetry to a Raylib GUI over a single Unix Domain Socket (UDS), fully decoupling the GUI from ROS.

---

## 1. System Overview

- Heavy compute (AI inference, GUI rendering) offloaded to a **Base Station laptop**.
- Onboard **Raspberry Pi 4** handles sensor data processing, filtering, and network streaming.
- Design goal: high resistance to network latency/jitter (expected in competition environments).

---

## 2. Hardware Layer & Peripherals

### Raspberry Pi 4 — Main Onboard Compute
- Runs Ubuntu + ROS 2
- Interfaces: I2C IMU, USB Camera, ESP32 (via UART)

### ESP32 — Real-Time Controller
- Handles high-frequency hardware loops
- Reads rotary encoders
- Samples analog metal detector (oversampling for noise reduction)
- Drives: dual-track/4-wheel motors, arm servo/motor, electromagnet

### Base Station (Laptop)
- Raylib C++ control station (UDS client)
- ONNX object detection node (ROS 2 node, publishes detection results)
- ROS2-to-UDS bridge (subscribes to all relevant ROS topics, exposes them over UDS)

---

## 3. Architecture Diagram

Data-flow summary (see original PDF Figure 1 for the visual diagram):

```
I2C IMU ──I2C──┐
USB Camera ────┼──> Raspberry Pi 4 (Dockerized, ROS 2)
ESP32 ──UART(COBS, packed)──┘
        │
        ├─ ROS 2: Kalman Filter        → /pose
        ├─ ROS 2: IMU reader           → /imu/data_raw (raw gyro/accel, fused directly by the Kalman filter)
        ├─ ROS 2: Hardware Interface   → odom
        ├─ ROS 2: Decision & State     → /detection_raw
        └─ RTSP Video Streamer         → RTSP stream (video, outside DDS)

Raspberry Pi ──DDS(Telemetry)──> ROS2-to-UDS Bridge (Laptop)
                                    ▲
ONNX Node (Laptop) ──DDS(detection)┘   [subscribes to RTSP stream via OpenCV]
                                    │
ROS2-to-UDS Bridge ──UDS(Telemetry & Cmds)──> Raylib GUI (UDS Client)
Raylib GUI ──/cmd_vel (via bridge → ROS 2)──> Raspberry Pi
```

Hardware wiring: Motors & Encoders, Metal Detector (ADC), Arm Motor & Magnet ← ESP32 (USB/UART link to Pi).

---

## 4. Communication & Protocols

### 4.1 Pi ↔ ESP32 (UART)

Custom byte-stuffing protocol using **COBS (Consistent Overhead Byte Stuffing)** — chosen because Micro-ROS over serial is unreliable for this use case.

| Requirement | Detail |
|---|---|
| Data Integrity | Packets strictly delimited, verified via checksums; corrupted data dropped instantly |
| Memory Alignment | All UART structs use explicit compiler packing directives to prevent padding mismatches between 32-bit ESP32 and 64-bit Pi |
| Watchdog Timer | ESP32 enforces a strict **250 ms timeout**. Delayed velocity packet (expected jitter) → robot coasts. No packet beyond threshold → ESP32 zeroes motor PWM (fail-safe) |

### 4.2 Robot ↔ Laptop (Wi-Fi)

- **Video**: raw video is explicitly excluded from ROS 2 DDS to conserve bandwidth. Pi hosts an **RTSP server**; laptop is an RTSP client; OpenCV pulls the stream natively and feeds it to the ONNX model. Multiple RTSP clients can view the feed (e.g. for debugging) without duplicating network load.
- **DDS scope**: ROS 2 DDS is used **only** between (a) the Raspberry Pi and the ROS2-to-UDS bridge, and (b) the ONNX node and the bridge.

**Topics:**

| Publisher | Topic | Content |
|---|---|---|
| Raspberry Pi | `/pose` | Filtered robot pose from the Kalman filter |
| Raspberry Pi | `/detection_raw` | Combined mine detection status (decision node: metal detector + visual confidence) |
| Raspberry Pi | `/odom` | Raw odometry (optional) |
| ONNX Node (laptop) | `/detection_vision` | Bounding boxes, class labels, confidence scores |

- **ROS2-to-UDS bridge**: subscribes to all topics above (from Pi and from ONNX node), consolidates the data, forwards to the Raylib GUI over UDS. Also receives velocity commands from the GUI over the same socket and republishes them as `/cmd_vel` to the Pi.

### 4.3 Laptop Internal Communication (Unix Domain Socket)

All bridge ↔ GUI communication occurs over a **single UDS**.

- **Ultra-Low Latency**: bypasses the network stack; near-zero-latency IPC for high-frequency telemetry/control
- **Decoupled Processing**: bridge and GUI are separate processes — heavy ROS 2/ONNX workloads cannot freeze the GUI
- **Consolidated Data Stream**: bridge aggregates pose, detection flags, vision results, and other state into a single binary or JSON protocol
- **Bidirectional Command Channel**: same socket carries velocity commands and manual position/heading fixes from GUI → bridge → Pi (over ROS 2)
- **Future Jetson Migration**: bridge+ONNX stack could move onboard to an NVIDIA Jetson; UDS would be replaced by a TCP proxy, but GUI code is unchanged since the protocol stays identical

---

## 5. Autonomy & Sensor Fusion

### 5.1 Localization (Kalman Filter)

- No absolute positioning (no GPS) — deliberately excluded to reduce complexity/inaccuracy.
- **One 2D EKF, no Madgwick stage.** State `[x, y, θ, v, ω, b_gz]` (b_gz = gyro Z bias). Rationale, evidence and tuning values: `docs/research/2026-10-14-imu-fusion-madgwick-vs-kalman.md`.
- Fuses:
  1. **Track odometry** — forward speed v (trusted); track yaw rate only with a large variance, because tracks skid in turns
  2. **Gyro Z** — calibrated (`calibration.cpp`), the main yaw-rate source
  3. **Zero-rate update** — when both encoders read zero for ≥ 0.5 s, ω = 0 and v = 0 are fused; this estimates the gyro bias online
  4. **Magnetometer heading (optional, off by default)** — only through a gate: arm-up switch closed ≥ 2 s (magnets off), tracks stopped, field strength and dip angle near calibration, robot level, innovation test. The magnets and motors run on a separate controller, so the arm switch and encoders stand in for magnet state and motor current
- **Drift correction: manual fixes, never hard resets.** Every fix is a Kalman measurement weighed against the filter's own uncertainty; the covariance is never zeroed.
  - **Position:** the edges of the field carry cell labels. From any edge cell the operator sends that cell; the GUI reports its centre. Worst case 0.5 m per axis; fused with σ ≈ 0.29 m per axis.
  - **Heading:** the operator snaps to one of four grid directions, only when the robot is roughly aligned (worst case 22.5°); fused with σ ≈ 13°. Mainly for startup and recovery.
  - A fix that disagrees with the estimate beyond the 99% χ² limit is shown to the operator for confirmation instead of being applied (catches a wrong cell or direction).

### 5.2 Mine Detection (Decision Logic)

Dedicated sensor fusion node on the Pi evaluates two inputs:
- Raw analog signal from the metal detector (smoothed via moving average on ESP32)
- Object detection confidence from the ONNX model

| Condition | Result |
|---|---|
| Analog signal > threshold **AND** visual confidence high | **Surface Mine**: log coordinate, arrest movement, sequence magnetic arm to secure object |
| Analog signal > threshold **BUT** visual confidence negligible | **Underground Mine**: map coordinate as underground threat, bypass arm sequence |

- Detection flags published on `/detection_raw`; ONNX node publishes detailed vision data on `/detection_vision`; bridge merges both streams for the GUI.

---

## 6. Deployment Strategy

Deployment on the Raspberry Pi is strictly containerized using **Docker**. Image is built directly from version control on the target hardware (avoids cross-compilation errors).

### 6.1 Configuration Management

- All system parameters (Kalman tuning matrices, metal detector analog thresholds, robot kinematics) live in a config file.
- File resides on the Pi's host filesystem, mapped as a **bind volume** into the container.
- Enables on-site parameter tuning at competition without a container rebuild.

### 6.2 Hardware Port Discovery Requirements

Linux assigns peripheral paths dynamically based on boot order, so an automated discovery script must run **before** launching the container:

1. Identify the ESP32 via its specific USB vendor/product IDs
2. Identify the active USB camera endpoint
3. Export these dynamic hardware paths into a standard environment variables file, so the container binds to the correct physical devices regardless of boot order

### 6.3 Docker Deployment Requirements

| Requirement | Detail |
|---|---|
| Network Isolation Bypass | Container **must** run in **host networking mode** — required for ROS 2 DDS multicast to discover/communicate with the laptop Base Station over Wi-Fi; also exposes the RTSP server port to the local network automatically |
| Hardware Access | Container must have adequate privileges or explicit device mappings for UART paths, I2C bus, and video hardware |
| Runtime Parameter Injection | Deployment process must pass the dynamically discovered hardware ports as launch arguments to the core ROS bring-up sequence |

---

## Quick-reference summary for agents

- **Two compute nodes**: Pi 4 (ROS 2, sensors, control loop coordination) and ESP32 (real-time motor/sensor I/O).
- **Three transport layers**: UART+COBS (Pi↔ESP32), ROS 2 DDS (Pi↔bridge, ONNX↔bridge), UDS (bridge↔GUI).
- **Video is out-of-band**: RTSP, not DDS.
- **Fail-safe**: 250ms UART watchdog on ESP32 zeroes PWM if no velocity command arrives.
- **No GPS**: localization is one 2D EKF fusing track odometry, raw gyro (with a bias state) and an optional gated magnetometer; no Madgwick. Manual edge-cell position fixes and four-way heading fixes enter as measurements, never as hard resets.
- **Mine classification**: metal-detector threshold × vision confidence → surface (arm engages) vs. underground (arm bypassed).
- **Docker**: host networking mode required; bind-mounted config; hardware ports discovered and injected at container launch time.
