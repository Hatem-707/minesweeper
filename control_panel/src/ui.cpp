#include "ui.hpp"
#include "camera.hpp"

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
  camera.release_texture();
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

Controls::Controls()
  : Section(UI::CONTROLS_X,
            UI::CONTROLS_Y,
            UI::CONTROLS_WIDTH,
            UI::CONTROLS_HEIGHT,
            "Controls",
            COLORS::blue)
{
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
  children.push_back(std::make_unique<MapCell>(rec.x, rec.y, rec.width, rec.height));
}
