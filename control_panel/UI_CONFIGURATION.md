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
| `Controls` | Telemetry-table fraction of the controls interior |
| `Colors` | Background, text, mine-state colors, and thrust color |
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
