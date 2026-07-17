#include <array>
#include <fmt/base.h>
#include <raylib.h>
#include <thread>

#include "ui.hpp"

class Commponents final : protected Drawable
{
  friend class App;
  void self_draw() override {};
  Commponents(Texture2D& camera_texture);
};

Commponents::Commponents(Texture2D& camera_texture)
  : Drawable(0, 0, UI::SCREEN_WIDTH, UI::SCREEN_HEIGHT)
{
  childern.push_back(std::make_unique<Map>());
  childern.push_back(std::make_unique<Feed>(camera_texture));
  childern.push_back(std::make_unique<Controls>());
}

class App
{
  Texture2D camera_texture;
  Commponents comp;
  std::array<std::jthread, 1> sub_threads;

public:
  App();
  void run();
};

App::App()
  : camera_texture(LoadTextureFromImage(
      GenImageColor(UI::CAMERA_RES, UI::CAMERA_RES, WHITE)))
  , comp(camera_texture)
{
}

void
App::run()
{
  while (!WindowShouldClose()) {
    BeginDrawing();
    comp.draw();
    ClearBackground(COLORS::background);
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
