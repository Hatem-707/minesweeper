# Software completion plan for October 17, 2026

Planning date: October 5, 2026. There are 12 calendar workdays available, October 5-16 inclusive.

Target: an integrated competition workflow by October 11, feature completion on October 13, and a tested release by October 15. October 16 is the recovery and preparation buffer.

## Assumptions and feasibility

Confirmed: two developers; you will do most of the software, while your teammate mainly handles embedded controls and publishing sensor readings to ROS. Teleoperation is allowed. You have three hours per day and have chosen AI-assisted code implementation to retain the full architecture as the target.

This remains an ambitious target schedule, not a dependable promise of completion. No detection model has been trained; the team plans to use YOLO. Hardware access, teammate availability, existing code outside this checkout, and target-object training data are still unconfirmed. AI can accelerate implementation and offline checks, but actual sensor calibration, robot behavior, and field performance require hardware evidence.

Confirmed visual target: the competition "mine" is a metal box in a sandy or grassy field. Plan a single-class `metal_box` detector. This makes the vision task more narrowly defined; actual box visibility and camera conditions still need validation.

At three hours/day, you have at most 36 hours across Oct 5-16. Protect the last three days (nine hours) for field testing, critical fixes, and recovery, leaving at most 27 hours for implementation review and earlier integration. The full architecture remains the target; missed milestones should trigger a fresh assessment of blockers, assistance, and feasibility rather than an automatic scope change.

The planned competition mode is teleoperation. Defer autonomous navigation and coverage planning. Confirm any remaining mandatory automatic behaviors, particularly detection, coordinate reporting, and retrieval, against the scoring rules.

## AI-assisted implementation approach

- You own design decisions, code review, integration, and acceptance. Use AI to implement bounded components, explain unfamiliar code, diagnose failures, and prepare meaningful checks. AI-written code is provisional until the appropriate checks pass.
- Deliver complete message paths rather than disconnected scaffolds: first GUI command to real motor and telemetry back to GUI; then sensor to pose/map; then camera and metal readings to decision and arm action.
- Agree each component's inputs, outputs, units, error behavior, and acceptance criterion before implementation. Keep the agreed interfaces shared with your teammate.
- Use mock ROS readings and serial fixtures for development while hardware work proceeds. Replace mocks with real hardware early; offline checks cannot establish field readiness.
- Keep daily changes small enough to review in your three-hour session. Use a working rhythm of 15 minutes to select the target, 75 minutes for implementation/review, 60 minutes for integration and testing, and 30 minutes for fixes and recording the known-good state. Adapt this split to hardware availability.
- Begin collecting representative target-object footage on Oct 5. Reserve separate capture sessions for validation so adjacent frames from one recording do not serve as the only evidence of generalization.
- YOLO implementation, data labeling, training, evaluation, and integration are distinct tasks. Do not assume selecting YOLO supplies a competition-ready detector. If targets differ from the pretrained classes, establish suitable custom training data and fine-tuning.
- Keep vision focused on the actual competition boxes. Collect robot-camera examples in both sand and grass, at expected distances and angles, under relevant lighting and partial occlusion. Include empty field scenes and confusing background objects when preparing and evaluating data.
- Evaluate on separate capture sessions and record missed visible boxes and false alarms against the acceptance criteria. A box hidden by grass may be a surface object without a visual detection; do not treat visual absence alone as proof of burial. Use an unknown classification or operator confirmation where permitted.

Reference for model preparation: [Ultralytics custom-data training](https://docs.ultralytics.com/yolov5/tutorials/train-custom-data/) and [COCO pretrained classes](https://docs.ultralytics.com/datasets/detect/coco/).

## Ownership and handoffs

| Owner | Responsibilities | Handoff |
|---|---|---|
| Teammate | ESP32 motor/encoder/ADC/arm/magnet firmware, COBS serial transport, Pi hardware-interface node, ROS sensor publishing, command execution, and embedded watchdog. | Agreed ROS messages on Oct 5; real actuation and readings by Oct 6-7; calibrated sensor data and tested arm controls by Oct 8-10. |
| You, with AI-assisted implementation | GUI input and real telemetry, GUI/ROS bridge, RTSP integration, localization and map/reset, YOLO/ONNX node, mine decisions and retrieval sequence coordination, deployment and release. Own review and hardware acceptance. | Complete GUI-to-robot demonstration Oct 7; map/pose Oct 9; vision and decision integration Oct 10-11. |
| Both | Interface decisions, sensor calibration, daily robot tests, command-loss tests, field rehearsals, startup and rollback. | Demonstrate the integrated system every day; feature freeze Oct 13. |

On Oct 5, your teammate should provide a mock ROS publisher or recorded readings using the agreed interfaces. Develop the GUI/bridge and sensor consumers against these while the firmware is completed. Use the same message definitions on real hardware to avoid a second integration effort.

## Evidence from this checkout

Source architecture: `minesweeper.pdf`, dated June 25, 2026, particularly Figure 1 and sections 4-6.

- The Raylib GUI has map, camera, and telemetry display components. `control_panel/src/main.cpp` currently generates random telemetry.
- `control_panel/src/state.cpp` registers callbacks but its input loop currently logs key presses without dispatching those callbacks.
- `control_panel/src/messages.hpp` contains initial message definitions. `control_panel/src/wire.hpp` contains a ZeroMQ communication stub; `wire.cpp` is empty. The PDF specifies a single Unix domain socket, so the transport and message contract need an explicit decision.
- The RTSP server and GStreamer client have implementations, but the camera client currently targets localhost. Robot-to-laptop operation and recovery are unverified.
- The Dockerfile installs ROS tooling but does not package a robot application, discover devices, or provide system bring-up.
- No ESP32 firmware, robot ROS packages, laptop ROS bridge, inference node, model, or automated tests were found in the visible project files. They may exist elsewhere.

No robot operation or successful build is claimed by this assessment. Existing local modifications were left intact.

## Resolve on October 5

1. List the scoring requirements, mandatory autonomy, run duration, coordinate tolerance, permitted operator intervention, and required outputs. Turn them into acceptance criteria.
2. Locate existing firmware, ROS packages, and hardware calibration. Assign one owner to each component. Obtain representative mine samples and begin image collection/labeling; no trained detector currently exists.
3. Freeze UART and GUI/bridge message contracts: framing, checksum, byte order, units, coordinate frames, version, sequence number, and freshness semantics. Reconcile GUI velocity values with ROS velocity units and remove ambiguity between the two current telemetry definitions.
4. Specify how `/detection_vision` reaches the Pi decision node. The PDF describes vision publication to the laptop bridge but the Pi needs that information for fusion. Choose a direct Pi subscription or explicit bridge forwarding.
5. Specify command expiry at the bridge/Pi as well as the ESP32's 250 ms valid-command watchdog. Continuing to resend an old command would conceal laptop disconnection from the ESP32.
6. Distinguish an operational camera that sees no mine from a failed or stale vision stream. Missing vision must not automatically classify a detection as underground.
7. Fix the ROS environment and target-device build approach; build and start a minimal container on the Pi immediately. Leave final deployment packaging for later, not the first target-device check.

## Daily schedule

| Date | Main work | Required end-of-day evidence |
|---|---|---|
| Oct 5 | Inventory existing work; confirm rules; freeze protocols, ownership, and acceptance thresholds; verify Pi build environment; begin YOLO data collection. | Shared interface specification, scope checklist, minimal container running on Pi, firmware availability established, and representative target images collected. |
| Oct 6 | ESP32/Pi serial integration: COBS, checksum, encoder and ADC telemetry, motor commands, watchdog, basic arm/magnet commands. | Bench demonstration of valid commands and telemetry; corrupt packets rejected; motor PWM becomes zero after 250 ms without valid commands. |
| Oct 7 | Complete GUI input and socket layer, laptop ROS bridge, Pi hardware interface, and actual RTSP endpoint configuration. | GUI drives the real robot through the complete command path; real telemetry and camera feed appear; releasing controls and breaking links stops movement. |
| Oct 8 | Encoder calibration, wheel odometry, IMU readings/filtering, coordinate frames, and position-reset command path. In parallel, prepare labeled YOLO data and start an initial fine-tuning run. | Known straight and turning motions produce plausible pose inputs; GUI reset reaches the Pi; logs capture repeatable trials; training pipeline runs on the target classes. |
| Oct 9 | Integrate localization, map coordinates, reset behavior, and persistent mine records. | Measured course and anchor reset meet the tolerance chosen on Oct 5; map axes and sensor offsets are correct. |
| Oct 10 | Calibrate metal threshold/smoothing; evaluate the single-class metal-box detector on separate sand/grass recordings; integrate ONNX inference and connect vision output to decision node. | Metal events and fresh visual detections reach the Pi; detector performance meets the Oct 5 acceptance criteria; visible, occluded, buried, empty-field, and camera-failure cases are recorded. |
| Oct 11 | Integrate surface/underground decisions, stop-before-arm behavior, bounded arm sequence, magnet commands, and recovery. | One complete run: drive, detect, record position, distinguish valid surface/underground cases, and retrieve a surface object. |
| Oct 12 | Complete device discovery, host-network deployment, device mappings, bind-mounted parameters, launch order, reconnects, and logs. | Cold startup works on the actual robot without code edits; thresholds and calibration change through configuration. |
| Oct 13 | Finish required workflow gaps and rehearse on a representative field. Freeze features at day's end. | A complete rule-compliant run from cold startup. Any remaining mandatory gaps are named explicitly. |
| Oct 14 | Field calibration and endurance: turns, wheel slip, localization drift, detection thresholds, lighting, arm operation, and operator workflow. | Full-duration runs meet the chosen tolerances and scoring requirements; each fix has a reproducible before/after observation. |
| Oct 15 | Fault injection, repeat rehearsals, and release selection. | Three consecutive complete rehearsals; link failures stop motion; five successful cold startups; known-good version selected and backed up. |
| Oct 16 | Recovery buffer, critical fixes only, final smoke test, offline deployment backup, operator briefing, and packing. | Tested binaries/container, firmware, model, configuration, source revision, startup instructions, and rollback procedure available offline. |
| Oct 17 | Competition startup and pre-run checks. | Confirm camera, controls, command timeout, pose anchor, detector, arm, and logging before starting. |

Dates are integration gates, not a requirement for both developers to work on the same component. Your teammate works ahead on embedded controls and sensor publishing while you complete their consumers. After the hardware interface is stable, your teammate concentrates on arm behavior, calibration, and failure testing while you finish vision, decisions, and deployment. Deployment verification begins Oct 5 and continues throughout.

## Critical path and scope decisions

The first critical path is:

`ESP32 control -> UART interface -> Pi ROS commands -> laptop bridge -> GUI -> real driving demonstration`

The scoring workflow then depends on:

`real sensors + localization + fresh vision -> mine decision -> coordinate record -> stop and arm sequence`

- If real driving is not demonstrated by Oct 7, focus available effort and AI implementation assistance on control and communication. Reassess the remaining schedule against the actual blocker.
- If the model is unusable on real footage by Oct 10, identify whether data coverage, labeling, training, or runtime integration is responsible and reassess feasibility. AI-generated inference code does not establish detector accuracy.
- If the complete detection/retrieval workflow is missing on Oct 11, concentrate both owners on that workflow and explicitly assess whether the full target can still pass field testing by Oct 15.
- Teleoperation is allowed, so autonomous navigation and coverage planning are outside this release's scope.
- After Oct 13, accept only fixes needed to pass the agreed acceptance criteria. Re-test the affected workflow after every change.
- Defer Jetson migration, broad refactoring, UI animation/polish, and additional optimizations unless they are necessary for acceptance.

## Daily working rhythm

- Start with a 15-minute review of the previous demonstration, current blocker, and today's acceptance criterion.
- Use AI-assisted implementation and review for the critical path, followed by integration with adjacent components.
- Reserve roughly 60 minutes each day for integration, a robot demonstration when hardware is available, and log review. Test new hardware behavior as soon as it exists.
- Save the day's known-good revision and matching firmware/model/configuration. Maintain a short list of remaining mandatory failures.

## Final acceptance checklist

- [ ] GUI commands reach the robot and the operator can stop it immediately.
- [ ] The ESP32 drops corrupted packets and zeroes motor PWM after the specified 250 ms timeout without valid commands.
- [ ] Laptop, bridge, Wi-Fi, and UART failures expire motion commands; stale commands are not replayed on reconnection.
- [ ] RTSP and inference recover from interruption; old video/detections are visibly marked unavailable and excluded from fusion.
- [ ] Pose, map coordinates, detector/camera offsets, and manual anchor resets meet the agreed field tolerance.
- [ ] Surface and underground decisions pass representative trials; absent vision is handled as unknown.
- [ ] Mine records survive restart and repeated sightings do not create misleading duplicates.
- [ ] Arm/magnet behavior has tested limits, timeouts, and an operator recovery procedure.
- [ ] The operator can complete the required route and scoring workflow in teleoperated mode.
- [ ] Device discovery and deployment pass five cold startups on actual hardware.
- [ ] Three consecutive rehearsals complete the required competition workflow.
- [ ] Release files, configuration, startup instructions, and rollback are backed up offline.
