#include <array>
#include <fmt/base.h>
#include <memory>
#include <raylib.h>
#include <thread>

#include "camera.hpp"
#include "spin_lock.hpp"
#include "state.hpp"
#include "ui.hpp"

class Components final : protected Drawable
{
  friend class App;
  void self_draw() override {};
  Components(GstCamera& camera_texture, SpinLock<AppState>& state);
};

Components::Components(GstCamera& camera, SpinLock<AppState>& state)
  : Drawable(0, 0, UI::SCREEN_WIDTH, UI::SCREEN_HEIGHT)
{
  children.push_back(std::make_unique<Map>());
  children.push_back(std::make_unique<Feed>(camera));
  children.push_back(std::make_unique<Controls>());
  children.push_back(std::make_unique<Warnings>());
}

class App
{
  GstCamera camera;
  SpinLock<AppState> state;
  Components comp;
  std::array<std::jthread, 1> sub_threads;

public:
  App();
  void run();
};

App::App()
  : camera(UI::CAMERA_RES, UI::CAMERA_RES)
  , state(AppState())
  , comp(camera, state)
{
}

void
App::run()
{
  while (!WindowShouldClose()) {
    BeginDrawing();
    ClearBackground(COLORS::background);
    comp.draw();
    EndDrawing();
  }
}

int
main()
{
  InitWindow(UI::SCREEN_WIDTH, UI::SCREEN_HEIGHT, "Control Panel");
  App app;
  // SetWindowState(FLAG_WINDOW_RESIZABLE);
  SetTargetFPS(60);
  app.run();
  CloseWindow();
}
