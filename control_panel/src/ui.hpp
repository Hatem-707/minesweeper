#pragma once
#include "camera.hpp"
#include <array>
#include <fmt/base.h>
#include <memory>
#include <raylib.h>
#include <string>
#include <vector>

namespace UI {
constexpr int CELLS_NUM = 20;
constexpr int SCREEN_HEIGHT = 800;
constexpr int SCREEN_WIDTH = 1280;
constexpr int APP_PAD = 16;
constexpr int MAP_HEIGHT = static_cast<int>(0.9 * SCREEN_HEIGHT - 2 * APP_PAD);
constexpr int MAP_WIDTH = static_cast<int>(0.7 * SCREEN_WIDTH - 2 * APP_PAD);
constexpr int MAP_X = static_cast<int>(0.3 * SCREEN_WIDTH + APP_PAD);
constexpr int MAP_Y = APP_PAD;
constexpr int SECTION_PAD = 20;
constexpr int CELL_PAD = 2;
constexpr float DEFAULT_ROUNDNESS = 0.025;
constexpr int TEXT_PAD = 8;
constexpr int TEXT_SIZE = 20;
constexpr int FEED_X = APP_PAD;
constexpr int FEED_Y = APP_PAD;
constexpr int FEED_WIDTH = static_cast<int>(0.3 * SCREEN_WIDTH - 2 * APP_PAD);
constexpr int FEED_HEIGHT = static_cast<int>(0.5 * MAP_HEIGHT);
constexpr int CONTROLS_X = APP_PAD;
constexpr int CONTROLS_Y = FEED_HEIGHT + FEED_Y;
constexpr int CONTROLS_HEIGHT = FEED_HEIGHT;
constexpr int CONTROLS_WIDTH = FEED_WIDTH;
constexpr int CAMERA_RES = 720;
}

namespace COLORS {
constexpr Color grey = { 88, 110, 117, 255 };
constexpr Color red = { 220, 50, 47, 255 };
constexpr Color green = { 133, 153, 0, 255 };
constexpr Color blue = { 38, 139, 210, 255 };
constexpr Color orange = { 203, 75, 22, 255 };
constexpr Color teal = { 42, 161, 152, 255 };
constexpr Color pink = { 211, 54, 130, 255 };
constexpr Color background = { 253, 246, 226, 255 };
}

class Drawable
{
protected:
  int x;
  int y;
  int width;
  int height;

public:
  std::vector<std::unique_ptr<Drawable>> childern;
  virtual ~Drawable() = default;
  Drawable(int x, int y, int width, int height);
  Drawable(Rectangle rec);
  void draw();
  virtual void self_draw() = 0;
  Rectangle get_rec(int pad = 0);
};



class Section : public Drawable
{
  std::string name;
  Color color;

  Rectangle get_title_rec();

public:
  Section(int x, int y, int width, int height, std::string name, Color color);
  virtual ~Section() = default;
  void self_draw() final;
};

enum class CellState
{
  unchecked,
  clean,
  surface_mine,
  buried_mine,
};

class MapCell final : public Drawable
{
  CellState state;
  Color get_color()
  {
    switch (state) {
      case CellState::unchecked:
        return COLORS::grey;
      case CellState::clean:
        return COLORS::green;
      case CellState::surface_mine:
        return COLORS::orange;
      case CellState::buried_mine:
        return COLORS::red;
      default:
        return COLORS::background;
    }
  };

public:
  MapCell(int x, int y, int width, int height);
  void self_draw() override;
  void set_state(CellState target_state);
};

class Map final : public Section
{
  std::array<MapCell*, UI::CELLS_NUM * UI::CELLS_NUM> cells;

public:
  Map();
  MapCell& cell_at(int x, int y);
};

class CameraFeed final : public Drawable
{
  GstCamera& camera;
  void self_draw() override;

public:
  CameraFeed(GstCamera& camera, Rectangle feed_rec);
};

class Feed final : public Section
{

public:
  Feed(GstCamera& camera_texture);
};

class Controls final : public Section
{
public:
  Controls();
};
