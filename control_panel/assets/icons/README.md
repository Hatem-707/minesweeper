# Turn indicator

`turn-right.png` is the 96 × 96 transparent `rotate_right` PNG from
[Google Material Icons](https://github.com/google/material-design-icons/blob/master/png/image/rotate_right/materialicons/48dp/2x/baseline_rotate_right_black_48dp.png).
Downloaded October 7, 2026. Apache License 2.0; see `LICENSE-Material-Icons.txt`.

SHA-256: `6927d429d2c66dff8c8179f1643f91884e6caa309d9a3661da77d36ab3122e88`

CMake copies the PNG and license beside the executable. DriveControls loads it
once, inverts its black pixels to white for tinting with the configured text
color, mirrors it for left turns, and unloads it on destruction.
