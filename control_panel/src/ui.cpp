#include "ui.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fmt/format.h>
#include <memory>
#include <raylib.h>
#include <stdexcept>
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

void
Drawable::set_fonts(const UiFonts& fonts)
{
  this->fonts = &fonts;
  for (auto& child : children) {
    child->set_fonts(fonts);
  }
}

void
Drawable::draw_text(const char* text, float x, float y, int size, Color color)
  const
{
  DrawTextEx(fonts ? fonts->get(size) : GetFontDefault(),
             text,
             { std::round(x), std::round(y) },
             static_cast<float>(size),
             0,
             color);
}

float
Drawable::measure_text(const char* text, int size) const
{
  return MeasureTextEx(fonts ? fonts->get(size) : GetFontDefault(),
                       text,
                       static_cast<float>(size),
                       0)
    .x;
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
  int width =
    static_cast<int>(std::ceil(measure_text(name.c_str(), ui.title_font))) +
    ui.text_padding * 2;
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
  draw_text(name.c_str(),
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
    case MineState::pending:
      return ui.pending;
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

MapCell::MapCell(const UiStore& ui,
                 Rectangle rec,
                 StateHandler& state_handler,
                 const MineMap& mines,
                 Cell cell)
  : Drawable(ui, rec)
  , mines(mines)
  , cell(cell)
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
  state.set_mine_state(mines.at(cell));
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
  DrawRectangleRounded(get_draw_rec(), ui.roundness, 0, state.get_color(ui));
}

void
MessageLog::add(const std::string& message)
{
  const int seconds = static_cast<int>(GetTime());
  entries.push_front(
    fmt::format("[{:02}:{:02}] {}", seconds / 60, seconds % 60, message));
  if (entries.size() > limit) {
    entries.pop_back();
  }
}

namespace {
const char*
heading_name(msg::Heading heading)
{
  switch (heading) {
    case msg::Heading::PosX:
      return "+X";
    case msg::Heading::PosY:
      return "+Y";
    case msg::Heading::NegX:
      return "-X";
    case msg::Heading::NegY:
      return "-Y";
  }
  return "?";
}

// raylib rotates clockwise on screen; the arrow image points up (+Y).
float
screen_degrees(double heading_rad)
{
  return static_cast<float>(90.0 - heading_rad * 180.0 / PI);
}
}

Map::Map(const UiStore& ui,
         StateHandler& state_handler,
         const MineMap& mines,
         Wire& wire,
         MessageLog& log)
  : Section(ui,
            ui.map.bounds.resolve(ui.content_rect()),
            "Field Map",
            ui.map.color)
  , mines(mines)
  , wire(wire)
  , log(log)
{
  for (int y = 0; y < UiStore::grid_size; ++y) {
    for (int x = 0; x < UiStore::grid_size; ++x) {
      const Cell cell{ x, y };
      auto drawable = std::make_unique<MapCell>(
        ui, cell_rec(cell), state_handler, mines, cell);
      cells[y * UiStore::grid_size + x] = drawable.get();
      children.push_back(std::move(drawable));
    }
  }

  const auto file =
    std::filesystem::path(GetApplicationDirectory()) / "assets/icons/arrow.png";
  arrow = LoadTexture(file.string().c_str());
  if (arrow.id == 0) {
    throw std::runtime_error("Unable to load arrow icon: " + file.string());
  }
  SetTextureFilter(arrow, TEXTURE_FILTER_BILINEAR);

  connections.push_back(state_handler.register_mouse_click(
    [this](auto position) { select(position); }, MOUSE_BUTTON_RIGHT));
  const std::array<std::pair<int, msg::Heading>, 4> headings{
    { { KEY_UP, msg::Heading::PosY },
      { KEY_RIGHT, msg::Heading::PosX },
      { KEY_DOWN, msg::Heading::NegY },
      { KEY_LEFT, msg::Heading::NegX } }
  };
  for (const auto& [key, heading] : headings) {
    connections.push_back(
      state_handler.register_key_press(key, [this, heading] {
        if (reset_cell) {
          reset_heading = heading;
          update_prompt();
        }
      }));
  }
  for (int key : { KEY_ENTER, KEY_KP_ENTER }) {
    connections.push_back(
      state_handler.register_key_press(key, [this] { send_reset(); }));
  }
  connections.push_back(
    state_handler.register_key_press(KEY_ESCAPE, [this] { cancel_reset(); }));
}

Map::~Map()
{
  UnloadTexture(arrow);
}

Rectangle
Map::field_rec() const
{
  return get_rec(ui.section_padding);
}

Rectangle
Map::cell_rec(Cell cell) const
{
  // Map +Y is up; screen rows grow downwards.
  const Rectangle rec = field_rec();
  const float width = rec.width / UiStore::grid_size;
  const float height = rec.height / UiStore::grid_size;
  return { rec.x + cell.x * width,
           rec.y + (UiStore::grid_size - 1 - cell.y) * height,
           width,
           height };
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
  draw_reset();
  draw_robot();
}

void
Map::draw_arrow(Vector2 center, float size, float degrees, Color tint) const
{
  DrawTexturePro(arrow,
                 { 0, 0, float(arrow.width), float(arrow.height) },
                 { center.x, center.y, size, size },
                 { size / 2, size / 2 },
                 degrees,
                 tint);
}

void
Map::draw_robot() const
{
  const auto link = wire.link();
  if (!link.telemetry || link.telemetry->localization_age_ms == msg::never) {
    return;
  }
  // A stale pose stays visible but faded.
  const bool fresh = link_health(link).localization;
  const Rectangle rec = field_rec();
  const double field = mines.config().field_size_m;
  const Vector2 center{
    rec.x + float(link.telemetry->x / field) * rec.width,
    rec.y + rec.height - float(link.telemetry->y / field) * rec.height
  };
  const float size =
    std::min(rec.width, rec.height) / UiStore::grid_size * 0.9f;
  BeginScissorMode(int(rec.x), int(rec.y), int(rec.width), int(rec.height));
  DrawCircleV(center, size / 2, Fade(ui.robot, fresh ? 1.0f : 0.4f));
  draw_arrow(center,
             size * 0.75f,
             screen_degrees(link.telemetry->heading_rad),
             Fade(ui.background, fresh ? 1.0f : 0.6f));
  EndScissorMode();
}

void
Map::draw_reset() const
{
  if (!reset_cell) {
    return;
  }
  const Rectangle rec = cell_rec(*reset_cell);
  DrawRectangleRoundedLinesEx(rec, ui.roundness, 0, 3, ui.selection);
  if (reset_heading) {
    draw_arrow({ rec.x + rec.width / 2, rec.y + rec.height / 2 },
               std::min(rec.width, rec.height) * 0.9f,
               screen_degrees(std::uint8_t(*reset_heading) * PI / 2),
               ui.selection);
  }
}

void
Map::select(std::pair<float, float> position)
{
  const Rectangle rec = field_rec();
  if (!is_inside(position, rec)) {
    return;
  }
  const int column =
    int((position.first - rec.x) / rec.width * UiStore::grid_size);
  const int row =
    int((position.second - rec.y) / rec.height * UiStore::grid_size);
  reset_cell = Cell{ std::clamp(column, 0, UiStore::grid_size - 1),
                     std::clamp(UiStore::grid_size - 1 - row,
                                0,
                                UiStore::grid_size - 1) };
  reset_heading.reset();
  update_prompt();
}

void
Map::send_reset()
{
  if (!reset_cell || !reset_heading) {
    return;
  }
  const auto [x, y] = mines.center_of(*reset_cell);
  wire.send_reset(x, y, *reset_heading);
  log.add(fmt::format("Pose reset sent: ({:.2f}, {:.2f}) m, heading {}",
                      x,
                      y,
                      heading_name(*reset_heading)));
  cancel_reset();
}

void
Map::cancel_reset()
{
  reset_cell.reset();
  reset_heading.reset();
  update_prompt();
}

void
Map::update_prompt()
{
  if (!reset_cell) {
    log.set_prompt({});
    return;
  }
  const auto [x, y] = mines.center_of(*reset_cell);
  log.set_prompt(
    reset_heading
      ? fmt::format("Reset pose to ({:.2f}, {:.2f}) m facing {}: Enter sends, "
                    "Esc cancels",
                    x,
                    y,
                    heading_name(*reset_heading))
      : fmt::format("Reset pose to ({:.2f}, {:.2f}) m: choose a heading with "
                    "the arrow keys, Esc cancels",
                    x,
                    y));
}

CameraFeed::CameraFeed(const UiStore& ui, GstCamera& camera, Rectangle feed_rec)
  : Drawable(ui, feed_rec)
  , camera(camera)
{
}

void
CameraFeed::self_draw()
{
  const Rectangle rec = get_rec();
  const Texture2D& texture = camera.get_texture();
  const float scale =
    std::min(rec.width / texture.width, rec.height / texture.height);
  const Vector2 position{ rec.x + (rec.width - texture.width * scale) / 2,
                          rec.y + (rec.height - texture.height * scale) / 2 };
  DrawTextureEx(texture, position, 0, scale, WHITE);
}

Feed::Feed(const UiStore& ui, GstCamera& camera)
  : Section(ui,
            ui.feed.bounds.resolve(ui.content_rect()),
            "Camera Feed",
            ui.feed.color)
{
  children.push_back(
    std::make_unique<CameraFeed>(ui, camera, get_rec(ui.section_padding)));
}

StatusTable::StatusTable(const UiStore& ui,
                         Rectangle rec,
                         Wire& wire,
                         const MineMap& mines)
  : Drawable(ui, rec)
  , wire(wire)
  , mines(mines)
{
}

namespace {
std::string
link_status(const LinkHealth& health)
{
  if (!health.bridge) {
    return "Bridge down";
  }
  if (!health.localization && !health.hardware) {
    return "Robot down";
  }
  if (!health.localization) {
    return "Localization stale";
  }
  if (!health.hardware) {
    return "Hardware stale";
  }
  return health.vision ? "OK" : "OK (no vision)";
}
}

void
StatusTable::self_draw()
{
  const auto link = wire.link();
  const auto health = link_health(link);
  const std::array<const char*, 8> labels{ "Link",  "Arm",   "Metal",
                                           "Heading", "Pos X", "Pos Y",
                                           "Surface", "Buried" };
  // Stale values stay visible but faded; missing values are never shown as
  // zeros.
  std::array<std::string, 8> values{
    link_status(health),
    "--",
    "--",
    "--",
    "--",
    "--",
    fmt::format("{}", mines.count(MineState::surface_mine)),
    fmt::format("{}", mines.count(MineState::buried_mine))
  };
  const std::array<bool, 8> fresh{
    health.bridge && health.localization && health.hardware,
    health.hardware,
    health.hardware,
    health.localization,
    health.localization,
    health.localization,
    true,
    true
  };
  if (const auto& reading = link.telemetry) {
    if (reading->hardware_age_ms != msg::never) {
      values[1] = reading->arm_upright ? "Upright" : "Lowered";
      values[2] = reading->metal_detected ? "Detected" : "Clear";
    }
    if (reading->localization_age_ms != msg::never) {
      values[3] = fmt::format("{:.0f} deg", reading->heading_rad * 180 / PI);
      values[4] = fmt::format("{:.2f} m", reading->x);
      values[5] = fmt::format("{:.2f} m", reading->y);
    }
  }
  const auto rec = get_rec();
  const int font = ui.status_font;
  const float column_gap = 14;
  const float column_width = (rec.width - column_gap) / 2;
  const float row_height = rec.height / 4;
  for (std::size_t index = 0; index < labels.size(); ++index) {
    const float x = rec.x + (index % 2) * (column_width + column_gap);
    const float y = rec.y + (index / 2) * row_height + (row_height - font) / 2;
    draw_text(labels[index], x, y, font, ui.text);
    draw_text(values[index].c_str(),
              x + column_width - measure_text(values[index].c_str(), font),
              y,
              font,
              fresh[index] ? ui.text : Fade(ui.text, 0.4f));
  }
}

Status::Status(const UiStore& ui, Wire& wire, const MineMap& mines)
  : Section(ui,
            ui.status.bounds.resolve(ui.content_rect()),
            "Status",
            ui.status.color)
{
  children.push_back(std::make_unique<StatusTable>(
    ui, get_rec(ui.section_padding), wire, mines));
}

MessageList::MessageList(const UiStore& ui,
                         Rectangle rec,
                         const MessageLog& log)
  : Drawable(ui, rec)
  , log(log)
{
}

void
MessageList::self_draw()
{
  const auto rec = get_rec();
  const int font = ui.status_font;
  const float line_height = font + 4.0f;
  float y = rec.y;
  BeginScissorMode(int(rec.x), int(rec.y), int(rec.width), int(rec.height));
  if (!log.prompt().empty()) {
    draw_text(log.prompt().c_str(), rec.x, y, font, ui.selection);
    y += line_height;
  }
  for (const auto& line : log.lines()) {
    if (y + font > rec.y + rec.height) {
      break;
    }
    draw_text(line.c_str(), rec.x, y, font, ui.text);
    y += line_height;
  }
  EndScissorMode();
}

Messages::Messages(const UiStore& ui, const MessageLog& log)
  : Section(ui,
            ui.messages.bounds.resolve(ui.content_rect()),
            "Messages",
            ui.messages.color)
{
  children.push_back(
    std::make_unique<MessageList>(ui, get_rec(ui.section_padding), log));
}
