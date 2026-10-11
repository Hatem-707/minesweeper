#include <filesystem>
#include <fmt/base.h>
#include <memory>
#include <raylib.h>
#include <stdexcept>

#include "camera.hpp"
#include "state.hpp"
#include "ui.hpp"
#include "wire.hpp"

class UIComponents final : protected Drawable
{
  friend class App;
  void self_draw() override {};
  UIComponents(const UiStore&,
               const UiFonts&,
               GstCamera&,
               Wire&,
               StateHandler&,
               DriveController&,
               PickupController&);
};

UIComponents::UIComponents(const UiStore& ui,
                           const UiFonts& fonts,
                           GstCamera& camera,
                           Wire& wire,
                           StateHandler& state_handler,
                           DriveController& drive,
                           PickupController& pickup)
  : Drawable(ui, ui.screen_rect())
{
  children.push_back(std::make_unique<Map>(ui, state_handler));
  children.push_back(std::make_unique<Feed>(ui, camera));
  children.push_back(
    std::make_unique<Controls>(ui, wire, state_handler, drive, pickup));
  children.push_back(std::make_unique<Warnings>(ui));
  set_fonts(fonts);
}

// Declared before font/camera/scene members so the window outlives their
// resources.
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
  UiFonts fonts;
  GstCamera camera;
  Wire wire;
  StateHandler state_handler;
  DriveController drive;
  PickupController pickup;
  UIComponents comp;

public:
  explicit App(const std::filesystem::path& config_file);
  void run();
};

App::App(const std::filesystem::path& config_file)
  : ui(config_file)
  , window(ui)
  , fonts(std::filesystem::path(GetApplicationDirectory()) /
            "assets/fonts/Roboto-Regular.ttf",
          { ui.control_font, ui.title_font })
  , camera(ui.camera_resolution, ui.camera_resolution)
  , drive(ui.drive_ramp_rate, ui.maximum_thrust)
  , comp(ui, fonts, camera, wire, state_handler, drive, pickup)
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
