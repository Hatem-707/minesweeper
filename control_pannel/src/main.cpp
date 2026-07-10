#include <algorithm>
#include <fmt/base.h>
#include <memory>
#include <raylib.h>
#include <string>
#include <utility>
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

  Drawable(int x, int y, int width, int height);
  void draw();
  virtual void self_draw() = 0;
  Rectangle get_rec(int pad);
};

Drawable::Drawable(int x, int y, int width, int height)
  : x(x)
  , y(y)
  , width(width)
  , height(height)
{
}

Rectangle
Drawable::get_rec(int pad = 0)
{
  return {
    static_cast<float>(x + pad),
    static_cast<float>(y + pad),
    static_cast<float>(width - 2 * pad),
    static_cast<float>(height - 2 * pad),
  };
}

void
Drawable::draw()
{
  self_draw();
  for (auto& child : childern) {
    child->draw();
  }
}

class Section : public Drawable
{
  std::string name;
  Color color;

  Rectangle get_title_rec();

public:
  Section(int x, int y, int width, int height, std::string name, Color color);
  void self_draw() final;
};

Section::Section(int x,
                 int y,
                 int width,
                 int height,
                 std::string name,
                 Color color)
  : Drawable(x, y, width, height)
  , name(name)
  , color(color)
{
}

Rectangle
Section::get_title_rec()
{
  int width = MeasureText(name.c_str(), UI::TEXT_SIZE) + UI::TEXT_PAD * 2;
  int height = UI::TEXT_SIZE;
  int x = this->x + this->width / 2 - width / 2;
  int y = this->y;
  return { static_cast<float>(x),
           static_cast<float>(y),
           static_cast<float>(width),
           static_cast<float>(height) };
}

void
Section::self_draw()
{
  Rectangle rec = get_rec(UI::SECTION_PAD / 2);
  DrawRectangleRoundedLinesEx(rec, UI::DEFAULT_ROUNDNESS, 0, 2, color);
  rec = get_title_rec();
  DrawRectangleRec(rec, COLORS::background);
  DrawText(name.c_str(),
           rec.x + UI::TEXT_PAD,
           rec.y + UI::SECTION_PAD / 2 - rec.height / 2,
           UI::TEXT_SIZE,
           color);
}

enum class CellState
{
  unchecked,
  clean,
  surface_mine,
  burried_mine,
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
      case CellState::burried_mine:
        return COLORS::red;
    }
  };

public:
  MapCell(int x, int y, int width, int height);
  void self_draw() override;
  void set_state(CellState target_state);
};

MapCell::MapCell(int x, int y, int width, int height)
  : Drawable(x, y, width, height)
  , state(CellState::unchecked)
{
}

void
MapCell::self_draw()
{
  Color color = get_color();
  Rectangle rec = get_rec(UI::CELL_PAD);
  DrawRectangleRounded(rec, UI::DEFAULT_ROUNDNESS, 0, color);
  for (auto& child : childern) {
    child->draw();
  }
}

class Map final : public Section
{
public:
  Map();
};

Map::Map()
  : Section(UI::MAP_X,
            UI::MAP_Y,
            UI::MAP_WIDTH,
            UI::MAP_HEIGHT,
            "Field Map",
            COLORS::teal)
{
  Rectangle rec = get_rec(UI::SECTION_PAD);

  int cell_width = rec.width / UI::CELLS_NUM;
  int cell_height = rec.height / UI::CELLS_NUM;

  int leftover_x = (static_cast<int>(rec.width) % UI::CELLS_NUM) / 2;
  int leftover_y = (static_cast<int>(rec.height) % UI::CELLS_NUM) / 2;

  for (int i = 0; i < UI::CELLS_NUM; i++) {
    for (int j = 0; j < UI::CELLS_NUM; j++) {
      auto cell = std::make_unique<MapCell>(
        static_cast<int>(rec.x + i * cell_width + leftover_x),
        static_cast<int>(rec.y + j * cell_height + leftover_y),
        cell_width,
        cell_height);
      childern.push_back(std::move(cell));
    }
  }
}

class Feed final : public Section
{

public:
  Feed();
};

Feed::Feed()
  : Section(UI::FEED_X,
            UI::FEED_Y,
            UI::FEED_WIDTH,
            UI::FEED_HEIGHT,
            "Camera Feed",
            COLORS::pink)
{
  Rectangle rec = get_rec(UI::SECTION_PAD);
  int edge_len = std::min(rec.width, rec.height);
  int x = rec.x + rec.width / 2 - edge_len / 2;
  int y = rec.y + rec.height / 2 - edge_len / 2;
  auto mockup_feed = std::make_unique<MapCell>(x, y, edge_len, edge_len);

  childern.push_back(std::move(mockup_feed));
}

class Controls final : public Section
{
public:
  Controls();
};

Controls::Controls()
  : Section(UI::CONTROLS_X,
            UI::CONTROLS_Y,
            UI::CONTROLS_WIDTH,
            UI::CONTROLS_HEIGHT,
            "Controls",
            COLORS::blue)
{
}

class Commponents final : protected Drawable
{
  friend class App;
  void self_draw() override {};
  Commponents();
};

Commponents::Commponents()
  : Drawable(0, 0, UI::SCREEN_WIDTH, UI::SCREEN_HEIGHT)
{
  childern.push_back(std::make_unique<Map>());
  childern.push_back(std::make_unique<Feed>());
  childern.push_back(std::make_unique<Controls>());
}

class App
{
  Commponents comp;

public:
  void run();
};

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
  App app;
  InitWindow(UI::SCREEN_WIDTH, UI::SCREEN_HEIGHT, "Control Panel");
  // SetWindowState(FLAG_WINDOW_RESIZABLE);
  SetTargetFPS(60);
  app.run();
  CloseWindow();
}
