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
| `Map`, `Camera view`, `Status`, `Messages` | Panel position, size, and border color |
| `Map` | `field_size`: metres along each side of the square field |
| `Status` | `font`: status and message font size |
| `Detector` | `dx`, `dy`: metal detector offset from the robot pose (forward, left), metres |
| `Classification` | `seconds`, `distance`: window after a metal hit in which lowering the arm marks SURFACE |
| `Colors` | Background, text, cell states (unchecked, clean, pending, surface, buried), robot, and reset selection |
| `Hover` | Cell scale, spring angular frequency, and damping ratio |
