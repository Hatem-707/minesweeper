# Startup UI configuration

`App` owns a `UiStore`, which reads TOML once before opening the window. Every
scene element receives a reference to that store. Layout is calculated during
scene construction; changing the file affects the next startup.

To use the source configuration directly, run this from the repository root:

```sh
./control_panel/build/control_panel ./control_panel/styles.toml
```

Edit `control_panel/styles.toml`, then restart that command. No rebuild is needed.
An optional command-line argument selects any other TOML file. Without an
argument, the app reads `styles.toml` beside the executable. CMake places a copy
there when configuring the build; that copy can also be edited directly.

The settings are grouped as follows:

| Table | Parameters |
|---|---|
| `Screen` | Window width, height, and target FPS |
| `Camera` | Initial camera texture resolution |
| `Style` | Padding, font sizes, and rectangle roundness |
| `Map`, `Camera view`, `Controls`, `Warnings` | Panel position, size, and border color |
| `Controls` | Telemetry-table fraction, control font, arrow size, pickup button height, and spacing |
| `Drive` | Motor-command ramp rate and signed limit for linear and angular commands |
| `Colors` | Background, text, mine-state colors, unlocked/locked thrust colors, and control surface |
| `Hover` | Cell scale, spring angular frequency, and damping ratio |

Panel positions and sizes are fractions of the screen rectangle after applying
`app_padding`. Other padding and font sizes are in pixels. Nested elements are
derived from their parent rectangles. The map remains a 20x20 grid; cell sizes
follow its configured dimensions and padding. Colors accept RGB or RGBA integer
arrays with channels from 0 to 255.

Omitted settings use the defaults in `UiStore`. Missing files, invalid TOML,
incorrect types, and layouts with no usable interior report a startup error.
The spring currently uses an underdamped response: `damping_ratio` must be
between 0 and 1, exclusive.

`Drawable::draw(dt)` calls `self_update(dt)`, `self_draw()`, and then traverses
children. The map adjusts child drawing order so its hovered cell is last.
Every child still updates exactly once. There is no separate update traversal,
configuration polling, or runtime layout synchronization.

`App` also owns `UiFonts`, which loads the bundled Roboto Regular TrueType font
using raylib `LoadFontEx` after opening the window. The control and title font sizes each get
a shared atlas (one atlas when the sizes match); text uses `DrawTextEx` and `MeasureTextEx` with matching
font and spacing. Text positions are rounded to pixels, and atlases use bilinear
filtering. Changing TOML font sizes loads the corresponding atlases on the next
startup. All fonts are unloaded before the window closes. The executable expects
`assets/fonts/Roboto-Regular.ttf` beside it; CMake copies that asset and its
license when configuring. A missing or unreadable font reports a startup error.

## Keyboard driving

The default window is 1600 × 900 with a roughly 520 px left column. The Controls panel keeps Link, Arm,
Pos X, Pos Y, Speed, and Heading in a compact table above the keyboard
indicators and the forward/reverse and turning meters.
Readings come from `Wire` (the ZeroMQ link to the ROS bridge, see
`../ROS_CONTRACT.md`); commands use a separate, app-owned `DriveController`,
whose output `Wire` sends at 20 Hz. Link shows bridge, localization and
hardware health; stale readings are faded and missing ones show `--`.

W/Up commands forward movement (positive linear); S/Down commands reverse
(negative linear). A/Left commands a left turn (positive angular), and D/Right
commands a right turn (negative angular), matching the ROS convention
(counterclockwise positive). Curved arrows in the left/right squares
indicate rotation. Holding only a turn key commands rotation in place; combining
forward/reverse with a turn commands a curved path. The meters label their
positive and negative ends to show this sign convention.

These are signed motor commands, currently ±255, without calibrated speed units.
Direction labels, signed readings, and lock text share the `Controls.font` size.
Each signed reading sits to the right of its vertical bar.
`DriveState.linear` and `DriveState.angular` ramp independently at
`Drive.ramp_rate` command units/second, capped at `Drive.maximum_thrust`.
The defaults are 85 units/second and ±255, taking three seconds to reach the limit.
Opposing keys cancel their command; reversing a command starts its ramp from zero.
W and Up are aliases, so holding both does not double the ramp or count as a new
movement input. No heading is calculated from the two commands. Orientation and
Speed remain measured robot readings; motor mixing belongs to the robot side.

Space toggles the speed lock once per press. Locked commands are green; unlocked
commands are red. The movement combination at locking is saved. Pressing a
different combination unlocks both commands (and takes precedence over Space if
both occur in the same frame). Releasing keys does not erase the saved
combination; pressing that same combination again keeps the lock. While locked,
releasing any or all keys preserves both values. Pressing Space again with no
movement keys held unlocks and stops both commands. Losing window focus also
stops both and clears the lock. While unlocked, releasing forward/reverse zeros
only the linear command; releasing turn keys zeros only the angular command.
The four squares highlight held keys with an empty center; the meters show the
commands held by the lock even after the keys are released.

The squares and lock drawings are keyboard indicators; mouse driving is
not implemented. Drive and pickup rectangles are calculated once at startup.
The turn indicators use the bundled Material Icons PNG, mirrored for left turns;
the texture loads once and is unloaded before the window closes.

## Pickup

The arm runs a micro-controller sequence: the GUI sends a single `uint8` command
(`1`) and the robot does the rest. One full-width button below the speed lock
(`Controls.button_height`, default 64 pixels) triggers it; press **J** or click it.
Each press is a single command, not a hold, and there is no confirmation step.
`App` owns a `PickupController` alongside `DriveController`; each input snapshot
is processed once during the pickup control's graph update, and the pending
command is read with `take_command()` and sent once through `Wire`.

The arm has two states from its contact switch, `msg::Telemetry::arm_upright`: upright
or not. It also appears as the Arm reading in the telemetry table. Presses are
ignored unless the arm is upright and the window is focused. Upright shows a green
button; not upright shows it grey with muted text. An ignored press flashes a red
outline for about 0.3 seconds. Before any feedback arrives, or while hardware
feedback is stale, the arm counts as not upright, so nothing is sent. There is no GUI-side busy or cooldown state; the
micro-controller must ignore a request while a sequence is already running.

## Camera display

The stream retains its native frame dimensions. It is scaled uniformly to fit
inside the Camera Feed panel's padded rectangle and centered, preserving its
aspect ratio without cropping. Unused space shows the panel background.
`Camera.resolution` only sizes the initial placeholder before a frame arrives.

## Input callbacks

Input registration returns an `InputConnection`. Store it as a member of the
element or scene that owns the callback. Destroying the connection, or calling
`disconnect()`, unregisters the callback. Registration changes during dispatch
take effect on the next frame; disconnections skip callbacks that have not yet
run in the current frame. Input handling and registration run on the GUI thread.
`register_key_down(key, callback)` supplies a Boolean every frame, including
`false` on release or focus loss; `register_key_press` remains available for
edge-triggered actions such as Space. Controls combine all key snapshots before
updating the drive state once in their scene graph update.
`register_mouse_down(callback, button)` supplies the mouse position and held
state every frame, including false when focus or the cursor is unavailable.
It runs after click callbacks so a button can capture a press before processing
the held state. The pickup control converts a key press or click into one request per frame
before updating its controller.
