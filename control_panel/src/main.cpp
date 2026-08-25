#include <chrono>
#include <cstdint>
#include <fmt/base.h>
#include <memory>
#include <random>
#include <raylib.h>
#include <stop_token>
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
  children.push_back(std::make_unique<Controls>(state));
  children.push_back(std::make_unique<Warnings>());
}

class App
{
  GstCamera camera;
  SpinLock<AppState> state;
  Components comp;
  std::jthread test_thread;

public:
  App();
  void run();
};

App::App()
  : camera(UI::CAMERA_RES, UI::CAMERA_RES)
  , state(AppState())
  , comp(camera, state)
  , test_thread([this](std::stop_token stop) {
    auto angle = std::uniform_real_distribution<>(-180.f, 180);
    auto pos = std::uniform_real_distribution<>(0, 20);
    auto health = std::uniform_real_distribution<>(0, 1);
    auto thrust = std::uniform_int_distribution<int16_t>(-255, 255);
    auto gen = std::random_device();
    while (!stop.stop_requested()) {
      {
        auto guard = state.unlock();
        guard.data.heading = angle(gen);
        guard.data.pos_x = pos(gen);
        guard.data.pos_y = pos(gen);
        guard.data.speed = pos(gen);
        guard.data.health = health(gen);
        guard.data.thrust = thrust(gen);
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  })
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
