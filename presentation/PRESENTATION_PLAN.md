# Minesweepers 2026 — Presentation Plan (slide level)

## 1. What the jury expects (from the 2026 rulebook)

Source: *Minesweeper 2026 Rule Book* (Academia & Juniors), linked from landminefree.org.

- **Format:** 15 min oral presentation + 5 min Q&A, top-8 Academia teams, Round 2 (finals). **Every team
  member must present and answer questions**, and the robot must be in the room. Only a projector is
  provided; bring the laptop.
- **Presentation score sheet (Appendix E, /50, worth 10% of the final score):**

  | Criterion | Pts | Covered by slides |
  |---|---:|---|
  | Teamwork and presentation | 5 | all, 25 |
  | Overall mechanical design and locomotion | 5 | 4–8 |
  | Landmine disposal / marking mechanisms | 5 | 8, 21 |
  | Sensors and landmine detection mechanism | 5 | 11, 21 |
  | Systems design and operation | 5 | 3, 18 |
  | Control and electrical system | 5 | 10–16 |
  | **Mapping and localization system** | **7** | 20 |
  | Arena navigation plan | 5 | 23 |
  | Rough environment and high heat | 5 | 9 |
  | Budget / working plan | 5 | 25 |
  | Design evaluation | 5 | 9, 22 |
  | Bonus | +3 | — |

  Your three sections (mechanical / electronics / software) cover about 30 of the 50 points. The plan adds
  short slides for the other criteria (navigation plan, heat and sand, budget and timeline, disposal) so no
  criterion gets zero. **Mapping and localization is the largest single criterion (7 pts) and gets a full
  slide.**
- **Mission facts worth showing the jury that we designed for:** 20 × 20 m outdoor arena (grass, sand, stones,
  inclines, ditches). Buried mines are 10 cm steel cubes up to 10 cm deep; surface mines are 5 cm cubes.
  Each detection must trigger a light and a siren (−3 pts each time it doesn't). The map must be generated
  automatically, without human intervention, on a 19 × 19 grid (columns A–S, rows 1–19). Surface mines are
  collected (10 pts automatic, 5 pts manual). 20 min per run. **The organisers provide no Wi-Fi and no
  power**, which is why the robot carries its own access point. Robot mass limit is 40 kg. **Using ROS
  multiplies the score by 1.2.**

## 2. Deck shape

- Template: `Minesweepers Presentation.pptx` (16:9). Slide 1 is the title, slide 2 is the content layout
  (teal title top-left, logo top-right, footer), slide 3 is "Thank You".
- Size: **26 slides**: 23 content slides plus a title and 2 closing slides. Section dividers reuse the
  dark title-slide background. Timing assumes about 35–40 s per content slide and none on dividers.
  Slides marked *(backup)* are kept after "Thank You" for Q&A.
- Time budget: intro 1.5 min · mechanical 3.5 min · electronics 4 min · software 4.5 min · close 1.5 min.
- Visual rule: one idea per slide, a diagram or photo on every slide, at most 5 bullets, numbers instead
  of adjectives. Diagrams are drawn natively in PowerPoint so they stay sharp and editable.

Photo IDs refer to the labelled table in §4.

---

## 3. Slides

### Opening

**S1 — Title** *(template slide 1)*
- "Minesweepers Competition 2026 · 13th Edition", team name, "Academia".
- Visual: hero photo P6 (clean three-quarter view) cut out or framed on the right.

**S2 — The mission and our answer** (~45 s)
- Left column, *the arena demands*: 20 × 20 m rough outdoor terrain · buried 10 cm cubes ≤ 10 cm deep +
  5 cm surface cubes · signal (light + siren) · automatic 19 × 19 map · collect surface mines · 20 min ·
  no Wi-Fi or power.
- Right column, *our design response* (one line each): tracked chassis → terrain · metal detector + camera
  → detection and classification · EKF + corner resets → map · electromagnet arm → collection · onboard
  access point → link · ROS 2 → ×1.2 bonus.
- Visual: a two-column "requirement → design choice" table with arrows. This is the slide that shows the
  judges we read the rules.

**S3 — Robot at a glance** (~45 s)
- Hero photo P5 (top-down) with numbered call-outs: tracks, drive pods, central spine, control panel and
  e-stop, arm + electromagnet, metal detector, camera, electronics bay.
- Spec strip at the bottom: mass · L × W × H · top speed · battery · run time · compute (Pi 4 + STM32) ·
  link (onboard AP).
- Data needed: numbers in the spec strip.

### Section A — Mechanical systems and robot structure

**S4 — Divider: "Mechanical Systems"**

**S5 — Body structure** (~40 s)
- Chassis layout: two independent track pods joined by a central spine / bridge; the electronics bay and
  control panel sit on the spine; the arm is mounted on [front/side].
- Materials and why: painted steel/aluminium sheet frame (stiffness, weldable/boltable), aluminium
  profiles, white 3D-printed/POM brackets (light and quick to iterate).
- Mass distribution: low centre of gravity between the tracks for inclines and ditches.
- Visual: P4 (rear view) + P19 (bare chassis), and a simple top-view schematic marking the zones.
- Data needed: materials, overall mass, CoG height.

**S6 — Why tracks over wheels in the desert** (~45 s, key slide)
- Sand is a low-bearing-strength soil, so wheels sink and dig in. Tracks spread the load over a long
  contact patch.
- **Ground-pressure calculation** (the judges will like real numbers):
  p = m·g / (2 · L_contact · W_track), compared with 4 wheels with about 4 × (contact width × ~3 cm) patch.
  Show both kPa values in a bar chart.
- Grousers (the slats) give traction on loose sand and grass; the long footprint bridges ditches and
  stones; the robot keeps climbing on inclines.
- Tie to the rulebook's own weight-on-mine argument: lower ground pressure = less load concentrated on
  any spot.
- Honest trade-offs: skid-steer turning needs more torque, there is a risk of track throw, more parts.
  Each is answered on S7.
- Visual: wheel-vs-track sinkage sketch + kPa bar chart; photo P7 or P11 (robot on sand).
- Data needed: robot mass, contact length, track width, wheel diameter/width of the alternative.

**S7 — Track construction** (~45 s)
- Exploded/annotated diagram of one track pod: drive sprocket ← chain ← gearmotor, aluminium slat
  grousers bolted to [chain / rubber belt], white idlers (top), black road wheels (bottom), tensioner.
- Design details: slat profile (step edge = grouser), slat pitch, fastening, how tension is set, how the
  track is kept from coming off (guides/flanges).
- Visual: P21 (single slats) → P2 (slats on chain + idler) → P12 (assembled pod), in a left-to-right
  "part → assembly" strip.
- Data needed: slat material/profile, slat count, belt or chain type, tensioning method.

**S8 — Drive train and pickup arm** (~45 s)
- Drive: 2 DC gearmotors (P17), one per track, skid-steer. Gear ratio, rated torque/rpm, chain ratio →
  top speed and a torque check for the steepest incline: τ ≥ m·g·sin θ · r_sprocket / 2.
- Pickup / disposal: arm with gearmotor (P14) and electromagnet at its tip (P18). The single "Pickup"
  command runs the whole sequence on the STM32: lower → energise → lift → release into the container.
  An upright limit switch interlocks it. Covers the "disposal mechanism" criterion and the
  10-point *automatic collection* score.
- Visual: P17 + P14 + P18 and a 4-step sequence pictogram.
- Data needed: motor model, ratio, rpm, torque; container location; magnet holding force.

**S9 — Built for sand and heat (rough environment)** (~35 s)
- Sand: open slat tracks shed sand (self-cleaning), sealed bearings/idlers, covered electronics bay.
- Heat: Egyptian outdoor sun; electronics ventilation / shading, component temperature ratings, Pi
  thermal throttling mitigations, light-coloured surfaces.
- Field-tested: photo strip of P7, P11, P13 (sand, outdoors) + P1 (grass). Mention test-run evidence if
  any (duration, max slope).
- Data needed: what was actually done for heat/dust; test results.

### Section B — Electronic components and control system

**S10 — Divider: "Electronics & Control"**

**S11 — Sensors and controllers** (~45 s)
- Table: component · role · interface · location:
  - Raspberry Pi 4: high-level compute, ROS 2 (Docker)
  - **STM32 [model]**: real-time I/O: motor PWM, encoders, ADC, arm, magnet
  - Wheel encoders: odometry (STM32 timer encoder mode)
  - IMU [model]: heading/rate (I2C → Pi)
  - Metal detector [model/type]: buried + surface detection (analog → STM32 ADC)
  - USB camera: surface-mine vision + operator feed (USB → Pi)
  - Motor drivers [model]: tracks + arm
  - Electromagnet, arm limit switch, light + siren, e-stop
  - Wi-Fi access point [model]
- Visual: table with small component icons/photos.
- Data needed: exact models for the bracketed items.

**S12 — Why we switched from ESP32 to STM32** (~30 s)
- Hardware quadrature-encoder timers (no lost counts at speed), deterministic interrupt timing with no
  Wi-Fi/RTOS stack jitter, better ADC for the detector signal, more timers/PWM channels. Wi-Fi isn't
  needed on the MCU because the AP and the Pi handle networking.
- Visual: before/after comparison card.
- Data needed: your team's actual reasons, if different.

**S13 — Electrical architecture** (~40 s)
- Block diagram: battery → main fuse → **e-stop** → distribution → (motor drivers at battery voltage) ·
  (buck → 5 V Pi) · (buck → [x] V AP) · (magnet driver) · (STM32 3.3 V).
- Signal paths overlaid in a different colour: UART, I2C, USB, PWM, ADC, GPIO.
- Visual: one clean diagram; the e-stop and the control-panel photo (P1/P5) as an inset.
- Data needed: battery chemistry/voltage/capacity, converters, fuse ratings.

**S14 — On-robot communication** (~40 s)
- Pi ↔ STM32: UART with **COBS framing + checksum**, packed structs (identical layout on both sides),
  corrupted frames are dropped. Commands go down (drive, pickup) and sensors come up (encoders,
  metal, arm-upright).
- STM32 **250 ms watchdog**: no valid command → motor PWM to zero.
- Pi peripherals: IMU on I2C, camera on USB.
- Visual: frame-layout diagram `[COBS | type | payload | checksum | 0x00]` + wiring sketch.
- Data needed: baud rate, update rates, whether still COBS after the STM switch.

**S15 — Robot ↔ control station: onboard access point** (~45 s)
- The arena has no Wi-Fi, so the robot carries its own AP. The laptop joins it at the base station
  outside the fence; there's no dependency on venue infrastructure.
- What crosses the link: ROS 2 DDS (cmd_vel 20 Hz, odometry, metal, arm state) + **RTSP H.264 video
  out of band** (x264 zerolatency, not through DDS, to save bandwidth).
- Range: the 20 × 20 m arena puts the diagonal at about 28 m plus the distance to the base station. Note the
  antenna/band choice and the measured RSSI or latency, if available.
- Visual: arena top-view sketch with robot AP, laptop, and the two streams; bandwidth bar (video vs control).
- Data needed: AP model, band, measured range/latency.

**S16 — Power budget** (~45 s)
- Table: component · voltage · typical current · peak current · power, for track motors ×2, arm motor,
  electromagnet, Pi 4, STM32, AP, camera, IMU, detector, light/siren.
- Totals: average W and peak W, battery Wh → **estimated run time vs the 20 min match** with margin.
  Peak current vs the battery C-rating and the fuse.
- Visual: horizontal bar chart (average W per component) + run-time callout.
- Data needed: all numbers (measured preferred, datasheet acceptable; say which).

**S17 — Safety: stopping is layered** (~30 s)
- Chain of independent timeouts: GUI focus loss → zero · GUI 20 Hz refresh (stops if the UI hangs,
  200 ms) · bridge drops commands > 200 ms old → publishes zero · Pi motion timeout · STM32 250 ms
  watchdog · hardware e-stop. Reconnecting never resumes motion.
- Visual: horizontal "defence in depth" chain diagram, laptop → robot.

### Section C — Software stack

**S18 — Divider: "Software Stack"** (subtitle: "ROS 2 end to end, ×1.2 bonus")

**S19 — Architecture overview** (~50 s)
- Three tiers diagram (updated from the stale PDF; the PDF's UDS and Pi-side fusion are replaced):
  - **STM32 firmware:** real-time loops, watchdog.
  - **Raspberry Pi 4 (ROS 2 in Docker):** hardware interface node, IMU (Madgwick), Kalman filter →
    `/odometry/filtered`, RTSP server.
  - **Laptop:** ROS 2 bridge (Docker) ⇄ **ZeroMQ IPC** ⇄ Raylib GUI; separate vision process (RTSP →
    ONNX) → ZeroMQ → GUI.
- Why separate processes: an inference stall or ROS crash cannot freeze the operator GUI.
- Visual: the architecture diagram (redraw of ROS_CONTRACT.md topology).

**S20 — Mapping and localization (7 pts)** (~60 s, key slide)
- Localization: Kalman filter fusing wheel odometry (STM32 encoders) + IMU heading. No GPS.
- Drift control: operator **corner reset** sets x, y and snaps heading to one of 4 directions at a known
  arena corner.
- Mine position = robot pose + detector offset rotated by heading:
  `x_m = x + dx·cosθ − dy·sinθ`, `y_m = y + dx·sinθ + dy·cosθ`.
- Repeated hits within a merge radius merge into one mine, and records persist across restarts. The map is
  generated **automatically** (rule requirement) on the 19 × 19 A–S / 1–19 grid.
- Visual: arena grid with the robot path, a mine marker, and the offset vector drawn; GUI map screenshot.
- Data needed: measured drift over a test lap (for "design evaluation").

**S21 — Mine detection and classification** (~45 s)
- Metal detector rising edge = one detection candidate → **light + siren** immediately.
- Fusion with vision (YOLO single-class "metal box", ONNX Runtime on the laptop):
  metal + fresh positive vision → **SURFACE** (pickup enabled) · metal + fresh negative vision → **BURIED** ·
  metal + stale/no vision → **UNKNOWN** (never guessed). Vision alone never creates a mine.
- Visual: decision table / small flowchart + camera frame with a bounding box.
- Data needed: model accuracy on your sand/grass captures, if available.

**S22 — Control station GUI** (~45 s)
- Screenshot of the Raylib control panel with call-outs: live mine map, camera feed, drive controls
  (ramp to 255 in 3 s, speed lock), pickup button (only enabled when the arm reads upright), link
  health (stale data is greyed, never shown as zero).
- C++20 with Raylib + GStreamer; config in `styles.toml`.
- Visual: full-width screenshot.
- Data needed: a screenshot taken while connected to the robot.

**S23 — Arena navigation plan** (~35 s)
- Teleoperated lawnmower (boustrophedon) sweep: lane spacing = detector width minus overlap. Start at a
  corner, re-anchor the pose at each corner reached, deposit surface mines in a corner container.
- Time budget: lanes × length / speed vs 20 min, with the reserve for pickups. This feeds the 30-pt
  "complete surf of field" bonus.
- Visual: arena drawing with the sweep path and corner reset points.
- Data needed: detector width, cruise speed.

**S24 — Engineering quality and reliability** *(could be backup)* (~30 s)
- One versioned binary contract header shared by GUI, bridge and vision; every message is validated
  (size, version, ranges, finite floats). 20 Hz telemetry, 500 ms staleness.
- Dockerized robot stack: host networking, bind-mounted config (tune thresholds on site without a
  rebuild), automatic device discovery at boot.
- Unit tests (GoogleTest) for drive, pickup, messages, wire.
- Visual: small icons/checklist.

### Closing

**S25 — Team, budget and working plan** (~45 s)
- Team photo and roles (who presented which part).
- Budget table by subsystem (mechanical, electronics, compute, misc) with the total.
- Timeline: development milestones up to the competition (from `competition-plan.md`: integrated
  drive Oct 7 → mapping Oct 9 → vision Oct 10–11 → freeze Oct 13 → rehearsals Oct 15).
- Future: full autonomy (another ×1.2), onboard Jetson for vision.
- Data needed: team, budget numbers.

**S26 — Thank You / Q&A** *(template slide 3)*

**Backup slides (after Thank You):** message table (ROS_CONTRACT §2), full power table, ROS topic/QoS
table, wiring diagram, test-run data.

---

## 4. Which photos go where

| ID | File (WhatsApp Image …) | Content | Use |
|---|---|---|---|
| P1 | 2026-10-08 10.57.42 PM (1) | Top-down track, control panel + e-stops, grass | S9, S13 |
| P2 | 2026-10-08 10.57.42 PM (2) | Slats + drive chain + idler close-up | **S7** |
| P3 | 2026-10-08 10.57.42 PM | Person fitting loose track belt | S7 (optional, assembly) |
| P4 | 2026-10-08 10.57.43 PM (1) | Rear view, twin tracks, spine | **S5** |
| P5 | 2026-10-08 10.57.43 PM (2) | Top-down whole robot, panel | **S3** |
| P6 | 2026-10-08 10.57.43 PM (3) | Clean three-quarter, grass | **S1 hero** |
| P7 | 2026-10-08 10.57.43 PM (4) | On sand by palm | S6 / S9 |
| P8 | 2026-10-08 10.57.43 PM (5) | Wider on sand | spare |
| P9 | 2026-10-08 10.57.43 PM (6) | Track end close-up | spare |
| P10 | 2026-10-08 10.57.43 PM | Wide, cluttered | skip |
| P11 | 2026-10-08 10.57.44 PM (1) | Rear-oblique on sand | S6 / S9 |
| P12 | 2026-10-08 10.57.44 PM (2) | Front of one track pod | **S7** |
| P13 | 2026-10-08 10.57.44 PM | Oblique on sand | S9 |
| P14 | 2026-10-09 2.09.45 AM (1) | Arm + gearmotor (to confirm) | **S8** |
| P15/P16 | 2026-10-09 2.09.45 AM / 2.09.46 AM | Bracket + threaded rod (to confirm) | S5 or S8 |
| P17 | 2026-10-10 7.38.58 PM | Two DC gearmotors | **S8** |
| P18 | 2026-10-10 7.39.14 PM | Cylinder under white block, likely electromagnet (to confirm) | **S8** |
| P19 | 2026-10-10 7.39.29 PM | Bare chassis, idlers, road wheels | **S5** |
| P20 | 2026-10-10 7.40.33 PM (1) | Slats laid on plate | S7 (optional) |
| P21 | 2026-10-10 7.40.33 PM | Single aluminium slats | **S7** |

The video (`WhatsApp Video … 10.57.45 PM.mp4`) can be embedded on S6 or S9 as a short loop of the robot
driving on sand.

## 5. Corrections to the stale software PDF

The deck must not repeat these outdated parts of `minesweeper.pdf` (June):
- ESP32 → **STM32**.
- Single Unix Domain Socket → **ZeroMQ IPC** (PUSH for commands, SUB for telemetry and vision).
- Mine fusion on the Pi (`/detection_raw`) → **mapping and fusion in the GUI**. The Pi publishes only pose,
  metal and arm state.
- ONNX as a ROS node → **separate non-ROS vision process**.
- `/pose` → `/odometry/filtered`. Reset zeroes covariance → reset sets x/y + heading snapped to 4 directions.
