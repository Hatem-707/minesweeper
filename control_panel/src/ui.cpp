#include "ui.hpp"
#include "camera.hpp"
#include "spin_lock.hpp"
#include "state.hpp"
#include <fmt/format.h>
#include <memory>
#include <raylib.h>
#include <string>

Drawable::Drawable(int x, int y, int width, int height)
  : x(x)
  , y(y)
  , width(width)
  , height(height)
{
}

Drawable::Drawable(Rectangle rec)
  : x(rec.x)
  , y(rec.y)
  , width(rec.width)
  , height(rec.height)
{
}

Rectangle
Drawable::get_rec(int pad)
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
  for (auto& child : children) {
    child->draw();
  }
}

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

  int width = MeasureText(name.c_str(), UI::TITLE_FONT) + UI::TEXT_PAD * 2;
  int height = UI::TITLE_FONT;
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
           UI::TITLE_FONT,
           color);
}

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
}

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
      cells[i * UI::CELLS_NUM + j] = cell.get();
      children.push_back(std::move(cell));
    }
  }
}

MapCell&
Map::cell_at(int x, int y)
{
  return *cells[y * UI::CELLS_NUM + x];
}

CameraFeed::CameraFeed(GstCamera& camera, Rectangle feed_rec)
  : Drawable(feed_rec)
  , camera(camera)
{
  if (feed_rec.width != feed_rec.height) {
    fmt::println("WARN!!! Feed Recatangle isn't a square");
  }
}

void
CameraFeed::self_draw()
{
  Rectangle rec = get_rec();
  Texture2D texture = camera.get_texture();
  float scale = rec.width / texture.width;
  DrawTextureEx(texture, { rec.x, rec.y }, 0, scale, WHITE);
}

Feed::Feed(GstCamera& camera)
  : Section(UI::FEED_X,
            UI::FEED_Y,
            UI::FEED_WIDTH,
            UI::FEED_HEIGHT,
            "Camera Feed",
            COLORS::pink)
{
  Rectangle rec = get_rec(UI::SECTION_PAD);
  float edge_len = std::min(rec.width, rec.height);
  float x = rec.x + rec.width / 2 - edge_len / 2;
  float y = rec.y + rec.height / 2 - edge_len / 2;
  Rectangle feed_rec{ x, y, edge_len, edge_len };
  auto feed = std::make_unique<CameraFeed>(camera, feed_rec);

  children.push_back(std::move(feed));
}

SliderThrust::SliderThrust(Rectangle rec, SpinLock<AppState>& state)
  : Drawable(rec)
  , state(state)
{
}

void
SliderThrust::self_draw()
{
  DrawRectangleRec(get_rec(), COLORS::green);
}

ThrustControls::ThrustControls(Rectangle rec, SpinLock<AppState>& state)
  : Section(rec.x, rec.y, rec.width, rec.height, "Thrust", COLORS::red)
{
  Rectangle drawing_rec = get_rec(UI::SECTION_PAD);
  children.push_back(std::make_unique<SliderThrust>(drawing_rec, state));
}

TableControls::TableControls(Rectangle rec, SpinLock<AppState>& state)
  : Drawable(rec)
  , state(state)
{
}

std::pair<int, int>
TableControls::get_entry_pos(int x, int y)
{
  Rectangle rec = get_rec(UI::SECTION_PAD / 2);
  float width = rec.width / 2;
  float height = rec.height / 3;
  return { rec.x + x * width, rec.y + y * height };
}

void
TableControls::self_draw()
{

  std::string health, pos_x, pos_y, magnet, speed, heading;
  {
    auto gaurd = state.unlock();
    health = fmt::format("{0}{1:>7.3f}", first_column[0], gaurd.data.health);
    pos_x = fmt::format("{0}{1:>7.3f}", first_column[1], gaurd.data.pos_x);
    speed = fmt::format("{0}{1:>7.3f}", first_column[2], gaurd.data.speed);
    magnet = fmt::format("{0}{1:>7}", second_column[0], gaurd.data.magnet);
    pos_y = fmt::format("{0}{1:>7.3f}", second_column[1], gaurd.data.pos_y);
    heading = fmt::format("{0}{1:>7.3f}", second_column[2], gaurd.data.heading);
  }
  auto health_pos = get_entry_pos(0, 0);
  auto pos_x_pos = get_entry_pos(0, 1);
  auto speed_pos = get_entry_pos(0, 2);
  auto magnet_pos = get_entry_pos(1, 0);
  auto pos_y_pos = get_entry_pos(1, 1);
  auto heading_pos = get_entry_pos(1, 2);

  DrawText(
    health.c_str(), health_pos.first, health_pos.second, UI::ELE_FONT, BLACK);
  DrawText(
    pos_x.c_str(), pos_x_pos.first, pos_x_pos.second, UI::ELE_FONT, BLACK);
  DrawText(
    speed.c_str(), speed_pos.first, speed_pos.second, UI::ELE_FONT, BLACK);
  DrawText(
    magnet.c_str(), magnet_pos.first, magnet_pos.second, UI::ELE_FONT, BLACK);
  DrawText(
    pos_y.c_str(), pos_y_pos.first, pos_y_pos.second, UI::ELE_FONT, BLACK);
  DrawText(heading.c_str(),
           heading_pos.first,
           heading_pos.second,
           UI::ELE_FONT,
           BLACK);
}

Controls::Controls(SpinLock<AppState>& state)
  : Section(UI::CONTROLS_X,
            UI::CONTROLS_Y,
            UI::CONTROLS_WIDTH,
            UI::CONTROLS_HEIGHT,
            "Controls",
            COLORS::blue)
{
  const auto& [x, y, width, height] = get_rec(UI::SECTION_PAD);
  Rectangle table_rec = { x, y, width, height * 0.75f };
  Rectangle thrust_rec = { x, y + 0.75f * height, width, 0.25f * height };

  children.push_back(std::make_unique<TableControls>(table_rec, state));
  children.push_back(std::make_unique<ThrustControls>(thrust_rec, state));
}

Warnings::Warnings()
  : Section(UI::WARNINGS_X,
            UI::WARNINGS_Y,
            UI::WARNINGS_WIDTH,
            UI::WARNINGS_HEIGHT,
            "Warnings & Messages",
            COLORS::orange)
{
  Rectangle rec = get_rec(UI::SECTION_PAD);
  children.push_back(
    std::make_unique<MapCell>(rec.x, rec.y, rec.width, rec.height));
}
