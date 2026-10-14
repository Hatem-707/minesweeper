# IMU bring-up and Madgwick filter: plan

Status: **plan approved; decisions in §9 settled on 2026-10-11. Steps 0–2 and 4–6 are implemented and partly verified; see §10 for results and remaining work.**

> **Superseded in part (2026-10-15):** the Madgwick filter is dropped. The Kalman
> localization node fuses calibrated raw gyro directly, with a gyro-bias state, a
> zero-rate update at stops, and an optional gated magnetometer. Still valid from this
> plan: the ESP32 test rig, the serial reader and its timestamp de-jitter, calibration
> (`calibration.cpp`, `imu_calibrate.py`), and the startup bias window. Retired:
> `madgwick.cpp`, its tests, and the filter part of `madgwick_node.cpp`. See
> `docs/research/2026-10-14-imu-fusion-madgwick-vs-kalman.md`.

## Context

- The architecture (`minesweeper-architecture.md`) calls for a Madgwick-filtered
  IMU that feeds the Kalman localization node, along with wheel odometry.
- The production IMU path is MPU6050 → STM32F401 (`minesweeper_firmware/`) →
  UART + COBS → Pi serial bridge → ROS. The STM32 already reads the MPU6050
  (`Core/Src/mpu6050.c`) and sends raw counts in `TelemetryPacket_t`
  (`Core/Inc/protocol.h`, CRC-8/SMBUS, 115200 baud, 20 ms period).
- For testing we use an **ESP32 on `/dev/ttyUSB0`** (CP2102,
  `/dev/serial/by-id/usb-Silicon_Labs_CP2102_USB_to_UART_Bridge_Controller_0001-if00-port0`)
  with the MPU6050 on **SDA 21 / SCL 22**.
- ROS runs in Docker (`Dockerfile`, `ros:lyrical-ros-core-resolute` + `ros-base`).
  ROS is not installed on the host (Arch).

Goal: the Madgwick node is developed and validated **now**, against the ESP32. It
should run unchanged when the STM32 + production bridge replace the test parts.

### Guiding decisions

1. **The ESP32 speaks the STM32's wire format byte-for-byte.** It sends the same
   `TelemetryPacket_t` (encoder/metal fields zeroed), the same COBS framing and the
   same CRC, using the same MPU6050 register setup. It compiles the repo's own
   `cobs.c` / `protocol.c`, not copies of them. The test reader therefore
   exercises the real protocol, and swapping to the STM32 changes nothing upstream
   of ROS.
2. **The MCU stays dumb.** Firmware sends raw counts only. Unit scaling happens
   in the reader. Calibration and fusion happen in the filter node. Calibration
   is tied to the *MPU6050 module*, not to the MCU, so it must not live in firmware.
3. **The filter node knows nothing about the MCU.** It consumes only a standard
   `sensor_msgs/Imu` topic. It has no serial, COBS or packet dependency and can
   run from a rosbag.
4. **The filter core is plain C++ with no ROS**, so it can be unit-tested with
   gtest and reused elsewhere if needed.

## 1. Data flow

```text
 MPU6050 ──I2C 100 kHz──▶ ESP32 (test)  ──UART0/USB 115200, COBS+CRC8──▶ imu_serial_test (rclpy, Docker)
                          STM32 (prod)   ──USART2 115200, same frames ──▶ production serial bridge
                                                                              │
                                              /imu/data_raw  sensor_msgs/Imu  ▼  (SI units, uncalibrated,
                                                                                 orientation_covariance[0] = -1)
                                                                   minesweeper_imu_filter (C++)
                                                                   calibration → Madgwick 6-DoF
                                                                              │
                                              /imu/data      sensor_msgs/Imu  ▼  (calibrated rates/accel + orientation)
                                              /imu/rpy       geometry_msgs/Vector3Stamped (debug, degrees)
                                                                              │
                                                                   Kalman localization node (later)
```

The topic names follow the `imu_tools` convention (`/imu/data_raw` → `/imu/data`),
so the reference `imu_filter_madgwick` can be put on the same topics for comparison.

## 2. Artifacts

| # | Path | What | Lifetime |
|---|---|---|---|
| A1 | `test_rigs/esp32_imu/platformio.ini` | PlatformIO project: `espressif32` platform pinned, `board = esp32dev`, `framework = arduino`, `CORE_DEBUG_LEVEL=0` (no log text on the wire), `monitor_speed = 115200` | test only |
| A2 | `test_rigs/esp32_imu/src/main.cpp` | Setup: `Wire.begin(21, 22, 100000)`, `Serial.begin(115200)`, MPU init. Loop: every 20 ms read 14 bytes, fill `TelemetryPacket_t`, CRC, COBS, `0x00` delimiter. Same control flow as `Send_Telemetry()` in the STM32 `main.c`. | test only |
| A3 | `test_rigs/esp32_imu/src/mpu6050_esp.{h,cpp}` | Port of `mpu6050.c` with `HAL_I2C_Mem_*` replaced by `Wire`. The register values and order are identical (PWR_MGMT_1=0, SMPLRT_DIV=7, CONFIG=3, ±250 dps, ±2 g), and so is the WHO_AM_I check. Like the STM32 driver, the check tests only that the I2C transfer succeeds, not the value. | test only |
| A4 | `test_rigs/esp32_imu/lib/shared_protocol/library.json` | Builds `minesweeper_firmware/Core/Src/{cobs.c,protocol.c}` with `Core/Inc` as include dir, straight from the STM32 tree (no copies). Must be listed in `lib_deps`; the dependency finder does not pick it up on its own. | test only |
| A5 | `ros_vol/src/imu_serial_test/` | `ament_python` package, one node `imu_serial_reader`, plus `wire.py` (decoding, no ROS) and pytest tests. It opens the port with DTR/RTS deasserted (see §3.2), reads it, splits on `0x00`, COBS-decodes, checks the length (24) and CRC, unpacks `<BiiHhhhhhhB`, converts to SI and publishes `/imu/data_raw`. It counts CRC errors and seq gaps and logs them every 5 s. It reconnects when the port disappears. | test only |
| A6 | `ros_vol/src/minesweeper_imu_filter/include/minesweeper_imu_filter/madgwick.hpp`, `src/madgwick.cpp` | ROS-free core: `class Madgwick { init_from_accel(a, yaw); update(gyro, accel, dt) → UpdateResult; quat(); }`, with `Vec3`/`Quat`/`Mat3` in `math.hpp` (no Eigen). | production |
| A7 | `.../include/.../calibration.hpp`, `src/calibration.cpp` | ROS-free: `apply(raw) → calibrated` (accel `M·(a − b)`, gyro `s ⊙ (ω − b)`, mount rotation `R_mount`), and `GyroBiasEstimator` (stationarity detector plus startup and online bias). | production |
| A8 | `.../src/madgwick_node.cpp` | rclcpp node. Subscribes to `/imu/data_raw` (best effort, keep last 10). Publishes `/imu/data` and `/imu/rpy`. All tunables are parameters. | production |
| A9 | `.../config/imu_filter.yaml`, `.../config/imu_calib_<module-id>.yaml` | Filter parameters; per-module calibration (see §6). Both are ROS parameter files (no YAML library in the node); `imu_calib_uncalibrated.yaml` is the identity template. | production |
| A10 | `.../test/test_madgwick.cpp`, `test_calibration.cpp` | gtest (`ament_cmake_gtest`), see §5.3 | production |
| A11 | `.../scripts/imu_calibrate.py` | rclpy CLI that records `/imu/data_raw` (`/imu/data` for `spin`) and merges results into a calibration file. Modes: `gyro`, `accel6`, `level`, `spin`, `allan`. | production tool |
| A12 | `.../launch/imu_bench.launch.py` | Starts A5 + A8 with the port as a launch argument | test only |
| A13 | `Dockerfile` (edit) | Appends `python3-serial` and `ros-lyrical-ament-cmake-gtest` as a **new final layer**, so existing layers stay cached | — |
| A14 | this document | | |

### Parameters (A5, A8)

`imu_serial_reader`: `port`, `baud` (115200), `frame_id` (`imu_link`),
`accel_fs_g` (2), `gyro_fs_dps` (250), `nominal_period_s` (0.02).
Scale factors: accel `counts / 16384 · 9.80665` m/s², gyro `counts / 131 · π/180` rad/s.

`madgwick_node` (all in `config/imu_filter.yaml`):
- `filter.beta` (0.1).
- `filter.beta_startup` (set to 0.1 = `beta`, i.e. off). The node seeds roll and
  pitch from 3 s of averaged standstill accel, which is already accurate. On the
  bench, a startup gain of 1.0 only added about ±2° of dither: a normalized
  gradient step dithers by about 2·β·dt.
- `filter.accel_gate` (0.10 g). Skips the accel correction when `| |a| − g |` is
  above the gate. 0.10 rather than 0.15: a 0.5 g lateral push only makes
  |a| = 1.118 g.
- `filter.dt_max` (0.1 s). On a larger gap, roll and pitch are re-seeded from
  accel and yaw is kept.
- `bias.*` (estimator, §6.4), `output_frame` (`base_link`), `publish_rpy` (true),
  `orientation_stddev.*`.
- The calibration file is a second parameter file, passed as the launch argument
  `calibration:=...`.

### Timestamps and dt

The packet has no device timestamp. Stamping with host receive time adds USB/CP2102
jitter (milliseconds), which goes straight into `dt`. The fix belongs in the
**reader**, because it knows about the MCU:

- The reader de-jitters: `stamp = t_anchor + n · nominal_period`, and re-anchors to
  receive time whenever the prediction is off by more than ½ period. It must
  re-anchor rather than trust `seq` alone: the STM32 **skips a packet without
  incrementing `seq` when the I2C read fails** (`Send_Telemetry` returns early), so
  a time gap can occur with no seq gap.
- The filter node just uses `header.stamp` differences, clamped to `(0, dt_max]`.
  The production bridge must provide the same stamp quality (requirement noted
  for the teammate).

## 3. Environment setup

Priorities: fast install, small disk use, nothing global that is hard to remove.
The host has about 36 GB free.

### 3.1 Firmware toolchain (repo-local)

PlatformIO lives **inside the repo** in `test_rigs/esp32_imu/`. Nothing is
installed globally, and deleting the directory's ignored folders removes all of it.

```fish
cd test_rigs/esp32_imu
uv sync                       # .venv/ from pyproject.toml + uv.lock (platformio pinned), ~18 MB
.venv/bin/pio run             # first run fills .pio-core/ (platformio.ini: core_dir = .pio-core)
.venv/bin/pio run -t upload   # needs the uucp group (below)
```

- `pyproject.toml` and `uv.lock` are committed. `.venv/`, `.pio-core/` and `.pio/`
  are git-ignored, so `uv sync` reproduces the exact toolchain on any machine.
- Measured: `.pio-core/` is **1.5 GB**. This is more than the 0.6–1 GB estimated,
  because the platform 7.1.3 Arduino core bundles prebuilt libraries. The
  espressif32 platform is pinned to 7.1.3. To remove everything:
  `rm -rf test_rigs/esp32_imu/{.venv,.pio-core,.pio}`.
- Rejected alternatives:
  - **ESP-IDF**: 2–3 GB and a slow install.
  - **arduino-cli + esp32 core 3.x**: pulls toolchains for every ESP chip and
    ends up larger than PlatformIO.
  - **MicroPython**: the smallest option (about 5 MB), but it cannot compile the
    repo's `cobs.c`/`protocol.c`, which defeats guiding decision 1.
- **Serial permissions:** `/dev/ttyUSB0` is `root:uucp 660`. Run
  `sudo usermod -aG uucp $USER` once, then log in again. Until then, flash
  through the ROS container, which reaches the port via `--device`:
  `docker run --rm --device=<by-id>:/dev/ttyIMU -v $PWD:/rig:ro minesweeper-ros python3 /rig/.pio-core/packages/tool-esptoolpy/esptool.py --chip esp32 --port /dev/ttyIMU --baud 460800 write_flash -z 0x1000 /rig/.pio/build/esp32dev/bootloader.bin 0x8000 /rig/.pio/build/esp32dev/partitions.bin 0xe000 /rig/.pio-core/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin 0x10000 /rig/.pio/build/esp32dev/firmware.bin`
- Use the `/dev/serial/by-id/...` path everywhere, never `ttyUSB0`, so the device
  name does not change when other USB serial devices are plugged in.

### 3.2 ROS (Docker, reusing the existing image)

- **No new image.** Extend the existing `Dockerfile` with one appended layer (A13).
  The image is already used for the bridge, so the extra disk cost is that one
  layer (a few MB). No `ros-desktop`/rviz (GBs): orientation is checked numerically
  via `/imu/rpy` and the gtests.
- The Docker daemon was **not running** when this plan was written:
  `sudo systemctl start docker` (or `enable --now`).
- Run:

```fish
docker build -t minesweeper-ros .
docker run --rm -it --net=host \
  --user (id -u):(id -g) --group-add (getent group uucp | cut -d: -f3) -e HOME=/tmp \
  --device=/dev/serial/by-id/usb-Silicon_Labs_CP2102_USB_to_UART_Bridge_Controller_0001-if00-port0:/dev/ttyIMU \
  -v $PWD/ros_vol:/ros_vol -v $PWD/protocol:/protocol \
  minesweeper-ros bash
# inside:
cd /ros_vol && colcon build --packages-select imu_serial_test minesweeper_imu_filter
colcon test --packages-select minesweeper_imu_filter && colcon test-result --verbose
source install/setup.bash && ros2 launch minesweeper_imu_filter imu_bench.launch.py port:=/dev/ttyIMU
```

- `--user` keeps `build/`, `install/` and `log/` owned by you, not root. As a
  non-root user, the container needs `--group-add` for the uucp group.
- Since `--device` is used instead of `--privileged`, unplugging the ESP32 means
  restarting the container. That is acceptable for a test rig.
- **Opening the port resets the ESP32.** DevKit boards wire DTR/RTS to EN/IO0, and
  with pyserial's default (lines asserted) the reset can land in the bootloader,
  which then sends nothing. The reader opens with DTR/RTS deasserted, which always
  boots the app. The boot banner is discarded as "overflows" in the reader's stats.
  The STM32's adapter is unaffected.
- Optional reference oracle: `apt install ros-lyrical-imu-filter-madgwick`
  (small) inside the container, for the comparison test in §5.4. Do not bake it
  into the image.

## 4. Implementation order

| Step | Work | Done when |
|---|---|---|
| 0 | Environment (§3): PlatformIO, uucp group, docker daemon, image builds | `pio --version`, `docker run … ros2 --help` work |
| 1 | ESP32 firmware A1–A4 | §5.1 passes |
| 2 | Serial reader A5 + Docker layer A13 | §5.2 passes |
| 3 | Record bags: 10 min stationary, hand-rotation sequence, 1 h stationary (for Allan variance, overnight is fine) | bags under `ros_vol/bags/` (git-ignored) |
| 4 | Madgwick core + calibration core A6, A7, gtests A10 | §5.3 passes |
| 5 | Node A8, config A9, launch A12 | §5.4 passes on live data and on bags |
| 6 | Calibration tool A11; bench calibration of the module (§6.2) | YAML committed for the module |
| 7 | On-robot (STM32 + production bridge): installation and runtime calibration (§6.3–6.4) | §5.5 passes |

## 5. Success criteria

### 5.1 Firmware (ESP32)

- [ ] Init succeeds; if the I2C transfer fails, the board sends no frames and
      blinks the LED (mirrors `Error_Handler`). Note: neither driver compares the
      WHO_AM_I value against `0x68`, so a clone chip with a different ID would pass.
- [ ] Frame rate 50 Hz ± 2 % over 10 min.
- [ ] 0 CRC failures and 0 length mismatches over 10 min. The ESP32 boot-ROM
      garbage at reset is tolerated: frames before the first delimiter are discarded.
- [ ] Flat and stationary, Z up: accel ≈ (0, 0, +16384) counts, each axis within
      ±5 % FS; gyro within ±(datasheet ZRO) counts.
- [ ] A byte-level diff of a frame against the STM32 struct layout matches
      (24-byte payload, little-endian, CRC over the first 23 bytes).

### 5.2 Serial reader (`imu_serial_test`)

- [ ] `ros2 topic hz /imu/data_raw` reads 50 ± 1 Hz, and the std of stamp
      differences is under 1 ms (de-jitter works).
- [ ] At rest: `|a| = 9.81 ± 0.3 m/s²` (uncalibrated).
- [ ] At rest, raw gyro: each axis within ±20 °/s (±0.35 rad/s). This is the
      MPU6050 zero-rate-output tolerance from the datasheet, so a larger offset
      points to a wiring, scaling or sensor fault rather than ordinary bias.
- [ ] At rest, bias-corrected (`/imu/data`, 60 s after ready): the mean of each
      axis is within ±0.1 °/s (±0.0017 rad/s). Z must also meet the stricter
      yaw-drift limit in §5.4 (< 1 °/min, i.e. about 0.017 °/s).
- [ ] Units and signs: rotating the board +90° about each axis by hand gives a
      positive rate on that axis (right-hand rule), and the gravity component moves
      to the expected axis.
- [ ] Malformed frames (fed through a pty with fuzzed bytes) are dropped and
      counted, never crash the node.
- [ ] Reset the ESP32 mid-stream: the node logs a seq discontinuity and resumes
      within 2 s.

### 5.3 Filter core (gtest, no hardware)

- [ ] Static gravity, any tilt, starting from level: orientation is within 0.5° of
      the true roll/pitch by 2.5 s (2 s at the startup gain, then 0.5 s at `beta`
      to settle the startup dither).
- [ ] Pure gyro integration (accel gated out): constant 90°/s about Z for 1 s
      gives yaw 90° ± 0.1°.
- [ ] Normalization: |q| = 1 ± 1e-6 after 10⁶ updates with noisy input.
- [ ] Gating: a 0.5 g lateral acceleration pulse changes roll/pitch by less than 0.5°.
- [ ] Degenerate input (zero accel, NaN, dt ≤ 0, dt > dt_max) leaves the state
      finite and reported as reinitialized/skipped.
- [ ] Calibration math: known bias/scale/mount rotation round-trip within 1e-9.
- [ ] Stationarity detector: true on the stationary bag, false during the rotation bag.

### 5.4 Filter node, bench (ESP32)

- [ ] **Independence:** the node builds and runs with only `rclcpp`,
      `sensor_msgs` and `geometry_msgs`, and gives identical output live and
      from `ros2 bag play`.
- [ ] Hand-held angles against a square/protractor: ±90° roll and pitch read
      90 ± 2°. Back to flat reads 0 ± 1°.
- [ ] Stationary 10 min after startup bias calibration: roll/pitch drift < 0.5°,
      **yaw drift < 1°/min**. This is the number that matters for localization.
- [ ] Bench 360° yaw rotation (against a drawn mark) returns to the start heading
      ± 3° before scale calibration, ± 1° after.
- [ ] Same bag through `imu_filter_madgwick` (same beta, `use_mag:=false`,
      `world_frame:=enu`): roll/pitch agree within 1°, yaw within 2° over 5 min.
- [ ] Processing latency (sub callback → publish) p99 < 1 ms on the laptop.
      Container CPU < 5 % of one core on the Pi 4.

### 5.5 On-robot (STM32 + production bridge)

- [ ] Swap check: the filter node and its config are unchanged. Only the
      publisher of `/imu/data_raw` differs.
- [ ] Boot on the field with no human handling: ready (bias estimated) within
      5 s of standstill.
- [ ] Spin in place 5 turns: integrated yaw within ± 2° of 1800°.
- [ ] 10-min teleop session ending at the start pose: IMU-only heading error < 5°.

## 6. Calibration

### 6.1 What matters for this robot

The robot drives on a flat field, and the Kalman node mainly needs **heading
(yaw) and yaw rate**. A 6-axis MPU6050 has no magnetometer, so **yaw is not
observable from the accelerometer**: any residual gyro-Z error integrates
directly into heading. Roll and pitch are corrected continuously by gravity, so
accelerometer errors only cause small, bounded tilt errors. An accel bias
δ gives a tilt error of about δ/g, so 0.05 m/s² ≈ 0.3°. Priorities follow from this:

| Quantity | Effect if wrong | How it can be observed | Where it is done |
|---|---|---|---|
| **Gyro bias** (esp. Z) | heading drifts linearly (bias × time) | only needs **standstill** | every boot + online at every stop |
| **Gyro Z scale** | heading error proportional to rotation (datasheet sensitivity tolerance is a few %, i.e. degrees per turn) | rotate a **known** angle | on-robot, robot spins itself |
| Mount roll/pitch | gravity leaks into X/Y, wrong "level" | one **level** pose | on-robot, once per remount |
| Accel bias + scale | small tilt error | needs **≥ 6 orientations** | bench, before mounting, once per module |
| Noise densities | sets beta and covariances | long standstill log | bench, once |
| Temperature dependence of gyro bias | bias wanders during warm-up | log bias vs temp | covered by online re-estimation; see 6.5 |

Everything that needs the sensor tumbled is done **on the bench, before
mounting**. On the robot we calibrate only things that standstill or the robot's
own motion can observe. The sensor never has to be moved by hand on the robot.

### 6.2 Bench (per MPU6050 module, before mounting)

Label the module (e.g. `imu-A`). The YAML is named after it, because the
calibration belongs to the module.

1. **Warm up** 5 min powered.
2. **Gyro bias + noise:** `imu_calibrate.py gyro --duration 60` gives the bias
   and std per axis.
3. **Accel 6-face:** `imu_calibrate.py accel6` prompts for each face (+X, −X, +Y,
   −Y, +Z, −Z) against a squared block, records 5 s each, and rejects any face
   where the gyro shows motion. It solves per axis for scale and bias
   (`b = (a⁺ + a⁻)/2`, `s = 2g/(a⁺ − a⁻)`). Optional: 12–24 free orientations
   with an ellipsoid fit for the full 3×3 `M` (cross-axis). Only do this if the
   6-face residual is above 0.05 m/s².
4. **Allan variance** (1 h stationary bag, `imu_calibrate.py allan`): angle random
   walk and bias instability for gyro, velocity random walk for accel. Use these
   for the covariances in `/imu/data`. Madgwick's rule is
   `beta = √(3/4) · (total gyro error)`. White noise alone (σ ≈ 0.0013 rad/s on
   this module) only gives a floor of about 0.001. Residual bias and scale error
   dominate, so start at 0.1 and tune on the bench bags.
5. Verify: re-run 6-face with the calibration applied. Each face must read
   ±9.81 ± 0.05 m/s².

### 6.3 Installation (on robot, once per mount; no sensor handling)

1. **Mount alignment (level):** park the robot on the flattest available floor
   (check with a spirit level on the chassis), then run
   `imu_calibrate.py level --duration 30`. The mean gravity vector gives the
   roll/pitch of the mount, stored as `R_mount`. Yaw mounting misalignment
   does not affect yaw rate about the vertical axis, so it is ignored. The
   nominal mount (which IMU axis points forward) comes from the CAD/assembly and
   is written into the YAML by hand.
2. **Gyro Z scale (spin test):** put tape marks on the floor and drive the robot
   to spin in place for N = 5 full turns CW, then 5 CCW. Stop on the mark using
   the camera or the operator's eye. Run `imu_calibrate.py spin --turns 5`.
   `scale_z = 1800° / integrated`, averaged over CW and CCW; a CW/CCW asymmetry
   reveals residual bias. The robot does the moving, so no handling is needed.
   Cross-check: localization's own rotation from wheel odometry should agree
   roughly, but wheel slip makes it a weak reference.
3. Commit the updated YAML. The filter node loads it at startup.

### 6.4 Runtime (every boot and continuously)

- **Startup bias:** the node starts in `INITIALIZING`. It waits for a 3 s window
  where the gyro std is below threshold (from 6.2) and `| |a| − g |` is small. It
  then sets the gyro bias to the window mean and initializes roll/pitch from accel
  and yaw = 0 (or the reset pose). Until then it publishes nothing on `/imu/data`.
  This is consistent with the per-node staleness in `ROS_CONTRACT.md`.
- **Online bias (ZUPT-style):** whenever the stationarity detector holds for ≥ 2 s,
  update the bias by a slow EMA (time constant about 30 s). The robot stops at
  every mine inspection, so there are frequent chances. The detector can later
  also require `/cmd_vel = 0` and unchanged encoders. The IMU-only detector can be
  fooled by smooth constant-rate motion, which a teleoperated robot rarely
  produces, but the extra signals make it robust.
- Clamp the bias update rate and magnitude, and log the bias every minute. A
  growing bias is a hardware or thermal warning.
- Heading drift that remains is the Kalman node's job (wheel odometry + manual
  edge-cell and heading fixes, fused as measurements, never as hard resets). The
  filter does not try to fix yaw on its own.

### 6.5 Temperature (deferred)

MPU6050 gyro bias shifts with temperature, mostly during warm-up. The online
re-estimation in 6.4 covers it for runs of competition length. The current
`TelemetryPacket_t` has no `temp_raw`. Proposal for the teammate: add it (2
bytes), so bias vs temperature can be logged and, if needed, modeled. **Deferred
until after §5.5** (decision 3, §9). The ESP32 does not add it unless the STM32
does (guiding decision 1).

### 6.6 Calibration file format

A ROS parameter file for `madgwick_node`, written by `imu_calibrate.py` and passed
as `calibration:=...` to the launch file. The identity template is
`config/imu_calib_uncalibrated.yaml`.

```yaml
# config/imu_calib_imu-A.yaml
madgwick_node:
  ros__parameters:
    calibration:
      module_id: imu-A
      created: '2026-10-12'
      accel:
        bias: [0.0, 0.0, 0.0]        # m/s², sensor frame, subtracted first
        matrix: [1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0]
        noise_std: [0.03, 0.03, 0.03]
      gyro:
        bias: [0.0, 0.0, 0.0]        # rad/s; used only with bias.estimate: false
        scale: [1.0, 1.0, 1.0]
        noise_std: [0.002, 0.002, 0.002]
      mount:                          # sensor → base_link
        nominal_rpy_deg: [0.0, 0.0, 0.0]          # from assembly, by hand
        level_correction_rpy_deg: [0.0, 0.0, 0.0] # from `level`
```

All numbers must be floats (`1.0`, not `1`). ROS rejects arrays that mix
integers and doubles.

## 7. Frames and conventions

- Output follows REP 103/145: `base_link` x forward, y left, z up. Orientation is
  `base_link` relative to an ENU-like world frame with yaw = 0 at
  initialization/reset.
- The filter node applies `R_mount` itself and publishes `/imu/data` with
  `frame_id = base_link`. The custom Kalman node therefore needs no TF lookup,
  and no `base_link → imu_link` static TF is published (decision 1, §9).
- `/imu/data_raw` stays in `imu_link` (sensor axes), uncalibrated.

## 8. Risks

| Risk | Mitigation |
|---|---|
| 50 Hz is on the low side for Madgwick during fast spins | Fine for a slow ground robot. If the spin test fails, raise the telemetry rate (the link has room: 26 B × 100 Hz = 2.6 kB/s of 11.5 kB/s) |
| ±2 g saturates on chassis vibration/bumps | Accel gating already ignores those samples. If clipping (±32767) is frequent in bags, move to ±4 g in both firmwares |
| ±250 dps too small for spin-in-place | Check peak `|ω_z|` in the spin test. If it exceeds about 200 dps, go to ±500 dps (both firmwares) |
| ESP32 Arduino I2C behaves differently from the STM32 HAL (timeouts, clock stretching) | Test-only difference. The STM32 path is validated again in §5.5 |
| Production bridge stamps packets with raw receive time | Requirement in §2 "Timestamps and dt", passed on to the teammate |

## 9. Decisions (settled 2026-10-11)

1. **The filter node applies the mount rotation.** `R_mount` comes from the
   calibration YAML, `/imu/data` is published in `base_link`, and no static TF is
   used. The Kalman node stays TF-free.
2. **The test reader is Python** (`ament_python`, `rclpy` + `pyserial`). It is
   test-only; the production bridge stays C++.
3. **`TelemetryPacket_t` stays unchanged in this plan.** No `temp_raw`, no device
   timestamp. Both firmwares send the current 24-byte packet. Temperature
   modeling (§6.5) and device timestamps are revisited only after §5.5 passes.

## 10. Implementation status (2026-10-12)

| Step | State |
|---|---|
| 0 Environment | Done. Repo-local PlatformIO (§3.1); `minesweeper-ros` image rebuilt with A13 (1.03 GB, sharing layers with the bridge image). |
| 1 Firmware | Done, flashed. Builds `cobs.c`/`protocol.c` from the STM32 tree. RAM 6.6 %, flash 21 %. |
| 2 Reader | Done; 9 pytest tests (COBS round-trip, CRC check value, fuzzing, chunking, boot banner, de-jitter, lost frame vs I2C skip). |
| 3 Bags | **Not done**: needs a person to move the board. |
| 4 Core + gtests | Done; 14 gtests pass. |
| 5 Node, config, launch | Done; bench-checked while stationary only (below). |
| 6 Calibration tool | Done. `gyro`, `level` and `allan` were run live and the node loads their output. `accel6` and `spin` are interactive and untested. No module calibration has been committed yet. |
| 7 On-robot | Not started (needs the STM32 + production bridge). |

### Measured on the bench (ESP32, uncalibrated module, board lying still)

| Criterion | Result | |
|---|---|---|
| §5.1 frame rate 50 Hz ± 2 % | 50.00 Hz over 30 s | pass |
| §5.1 no CRC/length errors | 0 bad, 0 seq gaps (30 s; 10 min not yet run) | pass (short) |
| §5.1 raw accel within ±5 % FS of (0, 0, 16384) | (453, −1095, 16975) | pass |
| §5.2 rate 50 ± 1 Hz, stamp-interval std < 1 ms | 50.05 Hz, 0.000 ms, 0 re-anchors | pass |
| §5.2 \|a\| = 9.81 ± 0.3 m/s² at rest (uncalibrated) | 10.19 m/s² | **fail**: module reads 1.038 g; `accel6` should fix it |
| §5.2 raw gyro each axis within ±20 °/s at rest | largest axis 2.56 °/s (x) | pass |
| §5.2 bias-corrected mean within ±0.1 °/s per axis | not yet run | pending |
| §5.3 all core tests | 14/14 | pass (convergence bound moved to 2.5 s, see §5.3) |
| §5.5 ready within 5 s of standstill | 3.3 s | pass (bench) |
| §5.4 stationary roll/pitch drift < 0.5° | +0.34° / +0.02° over 2 min | pass (short) |
| §5.4 yaw drift < 1 °/min | −0.18 °/min over 2 min | pass (short; 10-min run pending) |

Module noise (20 s capture): gyro σ ≈ 0.08–0.10 °/s, accel σ ≈ 0.02–0.035 m/s².
Gyro bias ≈ (2.56, 1.50, −0.14) °/s, steady over several minutes.

### Remaining, needs a person at the bench

Run inside the container from §3.2, with `source install/setup.bash` and
`ros2 launch minesweeper_imu_filter imu_bench.launch.py` running in another
shell (`docker exec -it <container> bash`).

1. **§5.4 hand angles**: `ros2 topic echo /imu/rpy`. Tilt to ±90° roll and pitch
   against a square, then back to flat.
2. **§5.4 bench 360° yaw**: rotate on the desk back to a drawn mark.
3. **§5.4 10-min stationary drift**: `ros2 bag record -o bags/still_10min /imu/data_raw`,
   then replay it to re-check.
4. **§6.2 bench calibration**: label the module (`imu-A`), warm it up 5 min, then
   run `imu_calibrate.py gyro`, `accel6`, and later `allan --duration 3600`, all
   with `--calib src/minesweeper_imu_filter/config/imu_calib_imu-A.yaml --module-id imu-A`.
   Commit the resulting YAML.
5. **§5.4 reference comparison**: `apt install ros-lyrical-imu-filter-madgwick` in
   a throwaway container, then play the same bag through both filters.
