#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fmt/base.h>
#include <memory>
#include <random>
#include <raylib.h>
#include <stdexcept>
#include <stop_token>
#include <thread>

#include "camera.hpp"
#include "spin_lock.hpp"
#include "state.hpp"
#include "ui.hpp"

class UIComponents final : protected Drawable
{
  friend class App;
  void self_draw() override {};
  UIComponents(const UiStore&, GstCamera&, SpinLock<Telemetry>&, StateHandler&);
};

UIComponents::UIComponents(const UiStore& ui,
                           GstCamera& camera,
                           SpinLock<Telemetry>& telemetry,
                           StateHandler& state_handler)
  : Drawable(ui, ui.screen_rect())
{
  children.push_back(std::make_unique<Map>(ui, state_handler));
  children.push_back(std::make_unique<Feed>(ui, camera));
  children.push_back(std::make_unique<Controls>(ui, telemetry));
  children.push_back(std::make_unique<Warnings>(ui));
}

// Declared before camera/scene members so the window outlives their resources.
class AppWindow
{
public:
  explicit AppWindow(const UiStore& ui)
  {
    InitWindow(ui.screen_width, ui.screen_height, "Control Panel");
    if (!IsWindowReady()) {
      throw std::runtime_error("Unable to initialize the control panel window");
    }
    SetTargetFPS(ui.target_fps);
  }
  ~AppWindow() { CloseWindow(); }
  AppWindow(const AppWindow&) = delete;
  AppWindow& operator=(const AppWindow&) = delete;
};

class App
{
  const UiStore ui;
  AppWindow window;
  GstCamera camera;
  SpinLock<Telemetry> telemetry;
  StateHandler state_handler;
  UIComponents comp;
  std::jthread test_thread;

public:
  explicit App(const std::filesystem::path& config_file);
  void run();
};

App::App(const std::filesystem::path& config_file)
  : ui(config_file)
  , window(ui)
  , camera(ui.camera_resolution, ui.camera_resolution)
  , telemetry(Telemetry())
  , comp(ui, camera, telemetry, state_handler)
  , test_thread([this](std::stop_token stop) {
    auto angle = std::uniform_real_distribution<>(-180.f, 180);
    auto pos = std::uniform_real_distribution<>(0, 20);
    auto health = std::uniform_real_distribution<>(0, 1);
    auto thrust = std::uniform_int_distribution<int16_t>(-255, 255);
    auto gen = std::random_device();
    while (!stop.stop_requested()) {
      {
        auto guard = telemetry.unlock();
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
    state_handler.handle_input();
    BeginDrawing();
    ClearBackground(ui.background);
    comp.draw(GetFrameTime());
    EndDrawing();
  }
}

int
main(int argc, char** argv)
{
  if (argc > 2) {
    fmt::println(stderr, "Usage: {} [styles.toml]", argv[0]);
    return 1;
  }
  try {
    const auto config_file =
      argc == 2
        ? std::filesystem::path(argv[1])
        : std::filesystem::path(GetApplicationDirectory()) / "styles.toml";
    App app(config_file);
    app.run();
  } catch (const std::exception& error) {
    fmt::println(stderr, "Control panel startup failed: {}", error.what());
    return 1;
  }
}
