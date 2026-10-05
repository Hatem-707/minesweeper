#include "ui.hpp"
#include "camera.hpp"
#include "spin_lock.hpp"
#include "state.hpp"
#include <cmath>
#include <cstdlib>
#include <fmt/format.h>
#include <memory>
#include <raylib.h>
#include <string>
#include <utility>

bool
is_inside(std::pair<float, float> point, Rectangle area)
{
  return point.first >= area.x && point.first < (area.x + area.width) &&
         point.second >= area.y && point.second < (area.y + area.height);
}

Drawable::Drawable(const UiStore& ui, Rectangle rec)
  : rectangle(rec)
  , ui(ui)
{
}

Rectangle
Drawable::get_rec(float pad) const
{
  return { rectangle.x + pad,
           rectangle.y + pad,
           rectangle.width - 2 * pad,
           rectangle.height - 2 * pad };
}

void
Drawable::draw(float dt)
{
  self_update(dt);
  self_draw();
  draw_children(dt);
}

void
Drawable::draw_children(float dt)
{
  for (auto& child : children) {
    child->draw(dt);
  }
}

Section::Section(const UiStore& ui,
                 Rectangle rec,
                 std::string name,
                 Color color)
  : Drawable(ui, rec)
  , name(std::move(name))
  , color(color)
{
}

Rectangle
Section::get_title_rec()
{
  Rectangle rec = get_rec();
  int width = MeasureText(name.c_str(), ui.title_font) + ui.text_padding * 2;
  int height = ui.title_font;
  int x = static_cast<int>(rec.x) + static_cast<int>(rec.width) / 2 - width / 2;
  int y = static_cast<int>(rec.y);
  return { static_cast<float>(x),
           static_cast<float>(y),
           static_cast<float>(width),
           static_cast<float>(height) };
}

void
Section::self_draw()
{
  Rectangle rec = get_rec(ui.section_padding / 2);
  DrawRectangleRoundedLinesEx(rec, ui.roundness, 0, 2, color);
  rec = get_title_rec();
  DrawRectangleRec(rec, ui.background);
  DrawText(name.c_str(),
           rec.x + ui.text_padding,
           rec.y + ui.section_padding / 2.f - rec.height / 2,
           ui.title_font,
           color);
}

Color
CellState::get_color(const UiStore& ui) const
{

  switch (mine_state) {
    case MineState::unchecked:
      return ui.unchecked;
    case MineState::clean:
      return ui.clean;
    case MineState::surface_mine:
      return ui.surface_mine;
    case MineState::buried_mine:
      return ui.buried_mine;
    default:
      return ui.background;
  }
}

void
CellState::update_state(float dt, const UiStore& ui)
{
  if (!std::isfinite(dt) || dt <= 0.0f) {
    return;
  }

  // Exact solution of a damped spring: stable for long frames and independent
  // of frame rate. Changing the target preserves the current velocity.
  const float frequency = ui.spring_frequency;
  const float damping_ratio = ui.spring_damping_ratio;
  const float damping = damping_ratio * frequency;
  const float damped_frequency =
    frequency * std::sqrt(1.0f - damping_ratio * damping_ratio);
  const float displacement = scale - target_scale;
  const float decay = std::exp(-damping * dt);
  const float cosine = std::cos(damped_frequency * dt);
  const float sine = std::sin(damped_frequency * dt);
  const float next_displacement =
    decay * (displacement * cosine +
             (velocity + damping * displacement) / damped_frequency * sine);
  velocity =
    decay * (velocity * cosine -
             (damping * velocity + frequency * frequency * displacement) /
               damped_frequency * sine);
  scale = target_scale + next_displacement;

  if (std::abs(next_displacement) < 0.0001f && std::abs(velocity) < 0.001f) {
    scale = target_scale;
    velocity = 0.0f;
  }
}

void
CellState::set_mine_state(MineState mine_state)
{
  this->mine_state = mine_state;
}

void
CellState::set_target_scale(float scale)
{
  if (std::isfinite(scale) && scale > 0.0f) {
    target_scale = scale;
  }
}

MapCell::MapCell(const UiStore& ui, Rectangle rec, StateHandler& state_handler)
  : Drawable(ui, rec)
{
  hover_connection = state_handler.register_mouse_movement(
    [this](std::pair<float, float> mouse_pos) {
      const bool inside = is_inside(mouse_pos, get_rec(this->ui.cell_padding));
      if (inside != hovered) {
        hovered = inside;
        state.set_target_scale(hovered ? this->ui.hover_scale : 1.0f);
      }
    });
}

void
MapCell::self_update(float dt)
{
  state.update_state(dt, ui);
}

Rectangle
MapCell::get_draw_rec() const
{
  Rectangle rec = get_rec(ui.cell_padding);
  const float scale = state.get_scale();
  rec.x -= rec.width * (scale - 1.0f) / 2.0f;
  rec.y -= rec.height * (scale - 1.0f) / 2.0f;
  rec.width *= scale;
  rec.height *= scale;
  return rec;
}

void
MapCell::self_draw()
{
  Color color = state.get_color(ui);
  Rectangle rec = get_draw_rec();
  DrawRectangleRounded(rec, ui.roundness, 0, color);
}

void
MapCell::set_state(CellState target_state)
{
  state = std::move(target_state);
  state.set_target_scale(hovered ? this->ui.hover_scale : 1.0f);
}

Map::Map(const UiStore& ui, StateHandler& state_handler)
  : Section(ui,
            ui.map.bounds.resolve(ui.content_rect()),
            "Field Map",
            ui.map.color)
{
  const Rectangle rec = get_rec(ui.section_padding);
  const float cell_width = rec.width / UiStore::grid_size;
  const float cell_height = rec.height / UiStore::grid_size;
  for (int y = 0; y < UiStore::grid_size; ++y) {
    for (int x = 0; x < UiStore::grid_size; ++x) {
      const Rectangle cell_rect{
        rec.x + x * cell_width, rec.y + y * cell_height, cell_width, cell_height
      };
      auto cell = std::make_unique<MapCell>(ui, cell_rect, state_handler);
      cells[y * UiStore::grid_size + x] = cell.get();
      children.push_back(std::move(cell));
    }
  }
}

void
Map::draw_children(float dt)
{
  MapCell* hovered_cell = nullptr;
  for (auto* cell : cells) {
    if (cell->is_hovered()) {
      hovered_cell = cell;
      break;
    }
  }
  for (auto& child : children) {
    if (child.get() != hovered_cell) {
      child->draw(dt);
    }
  }
  // Draw the hovered cell last so its enlarged edges remain visible.
  if (hovered_cell) {
    hovered_cell->draw(dt);
  }
}

MapCell&
Map::cell_at(int x, int y)
{
  return *cells[y * UiStore::grid_size + x];
}

CameraFeed::CameraFeed(const UiStore& ui, GstCamera& camera, Rectangle feed_rec)
  : Drawable(ui, feed_rec)
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

Feed::Feed(const UiStore& ui, GstCamera& camera)
  : Section(ui,
            ui.feed.bounds.resolve(ui.content_rect()),
            "Camera Feed",
            ui.feed.color)
{
  Rectangle rec = get_rec(ui.section_padding);
  float edge_len = std::min(rec.width, rec.height);
  float x = rec.x + rec.width / 2 - edge_len / 2;
  float y = rec.y + rec.height / 2 - edge_len / 2;
  Rectangle feed_rec{ x, y, edge_len, edge_len };
  auto feed = std::make_unique<CameraFeed>(ui, camera, feed_rec);

  children.push_back(std::move(feed));
}

SliderThrust::SliderThrust(const UiStore& ui,
                           Rectangle rec,
                           SpinLock<Telemetry>& state)
  : Drawable(ui, rec)
  , state(state)
{
}

void
SliderThrust::self_draw()
{
  auto rec = get_rec(ui.section_padding / 4);
  DrawRectangleRoundedLinesEx(rec, ui.roundness, 128, 4, ui.unchecked);
  DrawRectangleRounded(
    { rec.x,
      rec.y,
      rec.width * static_cast<float>(std::abs(state.unlock().data.thrust)) /
        255.f,
      rec.height },
    ui.roundness,
    128,
    ui.thrust);
}

ThrustControls::ThrustControls(const UiStore& ui,
                               Rectangle rec,
                               SpinLock<Telemetry>& state)
  : Section(ui, rec, "Thrust", ui.thrust)
{
  Rectangle drawing_rec = get_rec(ui.section_padding);
  children.push_back(std::make_unique<SliderThrust>(ui, drawing_rec, state));
}

TableControls::TableControls(const UiStore& ui,
                             Rectangle rec,
                             SpinLock<Telemetry>& state)
  : Drawable(ui, rec)
  , state(state)
{
}

std::pair<int, int>
TableControls::get_entry_pos(int x, int y)
{
  Rectangle rec = get_rec(ui.section_padding / 2);
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

  DrawText(health.c_str(),
           health_pos.first,
           health_pos.second,
           ui.element_font,
           ui.text);
  DrawText(
    pos_x.c_str(), pos_x_pos.first, pos_x_pos.second, ui.element_font, ui.text);
  DrawText(
    speed.c_str(), speed_pos.first, speed_pos.second, ui.element_font, ui.text);
  DrawText(magnet.c_str(),
           magnet_pos.first,
           magnet_pos.second,
           ui.element_font,
           ui.text);
  DrawText(
    pos_y.c_str(), pos_y_pos.first, pos_y_pos.second, ui.element_font, ui.text);
  DrawText(heading.c_str(),
           heading_pos.first,
           heading_pos.second,
           ui.element_font,
           ui.text);
}

Controls::Controls(const UiStore& ui, SpinLock<Telemetry>& state)
  : Section(ui,
            ui.controls.bounds.resolve(ui.content_rect()),
            "Controls",
            ui.controls.color)
{
  const auto [x, y, width, height] = get_rec(ui.section_padding);
  const float table_height = height * ui.table_fraction;
  Rectangle table_rec{ x, y, width, table_height };
  Rectangle thrust_rec{ x, y + table_height, width, height - table_height };
  children.push_back(std::make_unique<TableControls>(ui, table_rec, state));
  children.push_back(std::make_unique<ThrustControls>(ui, thrust_rec, state));
}

Warnings::Warnings(const UiStore& ui)
  : Section(ui,
            ui.warnings.bounds.resolve(ui.content_rect()),
            "Warnings & Messages",
            ui.warnings.color)
{
}
