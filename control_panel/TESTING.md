# Tests

The GoogleTest suite covers the IPC messages, the ZeroMQ wire (reset sending,
telemetry and vision receipt), link health, and the mine map: trail marking,
detector offset, surface/buried classification by the arm window, merging,
and stale-input handling. These tests run without raylib, a window, or a
camera. UI rendering, configuration parsing, and font tests are not maintained.

From the repository root:

```sh
cmake -S control_panel -B control_panel/build -DBUILD_TESTING=ON
cmake --build control_panel/build
ctest --test-dir control_panel/build --output-on-failure
```

GoogleTest must be installed when `BUILD_TESTING=ON`. For an application-only
build, configure with `-DBUILD_TESTING=OFF`.
