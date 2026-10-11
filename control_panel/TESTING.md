# Controller tests

The GoogleTest suite covers drive ramping, limits, direction changes, speed lock,
and focus loss, plus pickup gating on the arm-upright switch. These tests run without
raylib, a window, or a camera. UI rendering, configuration parsing, font, and
animation tests are not maintained.

From the repository root:

```sh
cmake -S control_panel -B control_panel/build -DBUILD_TESTING=ON
cmake --build control_panel/build
ctest --test-dir control_panel/build --output-on-failure
```

GoogleTest must be installed when `BUILD_TESTING=ON`. For an application-only
build, configure with `-DBUILD_TESTING=OFF`.
