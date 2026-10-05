# GUI input and animation tests

Tests use GoogleTest and run without a window, camera, or GPU. Test-local raylib
input functions provide controlled frames; drawing functions record cell bounds.
They also cover startup TOML loading/validation, layout changes between startups,
and exactly one animation update per element in the combined drawing traversal.
The production executable continues using raylib normally.

GoogleTest must be available through its CMake package when `BUILD_TESTING=ON`.

From the repository root:

```sh
cmake -S control_panel -B control_panel/build -DBUILD_TESTING=ON
cmake --build control_panel/build
ctest --test-dir control_panel/build --output-on-failure
```

For an application-only build, configure with `-DBUILD_TESTING=OFF`.

Input registration returns an `InputConnection`. Store it as a member of the
element or scene that owns the callback. Destroying the connection, or calling
`disconnect()`, unregisters the callback. Registration changes during dispatch
take effect on the next frame; disconnections skip callbacks that have not yet
run in the current frame. Input handling and registration run on the GUI thread.
