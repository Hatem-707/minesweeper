#include "ui.hpp"
#include "camera.hpp"
#include "spin_lock.hpp"
#include "state.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
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

CommandMeter::CommandMeter(const UiStore& ui,
                           Rectangle rec,
                           const DriveController& drive,
                           DriveChannel channel)
  : Drawable(ui, rec)
  , drive(drive)
  , channel(channel)
{
}

void
CommandMeter::self_draw()
{
  const auto rec = get_rec();
  const auto& command = drive.state();
  const bool linear = channel == DriveChannel::linear;
  const float thrust = linear ? command.linear : command.angular;
  const int font = ui.control_font;
  // Turn signs follow ROS: positive is a left (counterclockwise) turn.
  const char* positive = linear ? "+ Forward" : "+ Left";
  const char* negative = linear ? "- Reverse" : "- Right";
  draw_text(positive,
            rec.x + (rec.width - measure_text(positive, font)) / 2,
            rec.y,
            font,
            ui.text);
  draw_text(negative,
            rec.x + (rec.width - measure_text(negative, font)) / 2,
            rec.y + rec.height - font,
            font,
            ui.text);
  const float track_top = rec.y + font + 6;
  const float track_height = rec.height - 2 * font - 12;
  const Rectangle track{ rec.x + 8, track_top, 26, track_height };
  DrawRectangleRounded(track, ui.roundness, 0, ui.control_surface);
  const float fill_height = track.height * std::abs(thrust) / drive.limit() / 2;
  if (fill_height > 0) {
    DrawRectangleRec(
      { track.x,
        track.y + track.height / 2 - (thrust > 0 ? fill_height : 0),
        track.width,
        fill_height },
      command.locked ? ui.thrust_locked : ui.thrust);
  }
  DrawLineEx({ track.x, track.y + track.height / 2 },
             { track.x + track.width, track.y + track.height / 2 },
             1,
             ui.unchecked);
  const auto value = fmt::format("{:+.0f}", thrust == 0 ? 0.0f : thrust);
  draw_text(value.c_str(),
            track.x + track.width + 10,
            track.y + (track.height - font) / 2,
            font,
            ui.text);
}

DriveControls::DriveControls(const UiStore& ui,
                             Rectangle rec,
                             const DriveController& drive,
                             const DriveInput& input)
  : Drawable(ui, rec)
  , drive(drive)
  , input(input)
  , positions(calculate_layout())
{
  children.push_back(std::make_unique<CommandMeter>(
    ui, positions.meters[0], drive, DriveChannel::linear));
  children.push_back(std::make_unique<CommandMeter>(
    ui, positions.meters[1], drive, DriveChannel::angular));
  const auto file = std::filesystem::path(GetApplicationDirectory()) /
                    "assets/icons/turn-right.png";
  Image icon = LoadImage(file.string().c_str());
  if (!icon.data) {
    throw std::runtime_error("Unable to load turn icon: " + file.string());
  }
  ImageColorInvert(&icon); // White pixels allow tinting with the UI text color.
  turn_arrow = LoadTextureFromImage(icon);
  UnloadImage(icon);
  if (turn_arrow.id == 0) {
    throw std::runtime_error("Unable to load turn icon: " + file.string());
  }
  SetTextureFilter(turn_arrow, TEXTURE_FILTER_BILINEAR);
}

DriveControls::~DriveControls()
{
  UnloadTexture(turn_arrow);
}

DriveLayout
DriveControls::calculate_layout() const
{
  const auto rec = get_rec();
  const float body_height = rec.height - ui.lock_height - ui.control_gap;
  const float meter_width = ui.drive_meter_width();
  const float arrow =
    std::min({ float(ui.arrow_size),
               (body_height - 2 * ui.arrow_gap) / 3,
               (rec.width - ui.pad_meter_gap - 2 * meter_width - ui.meter_gap -
                2 * ui.arrow_gap) /
                 3 });
  const float pad_width = 3 * arrow + 2 * ui.arrow_gap;
  const float group_width =
    pad_width + ui.pad_meter_gap + 2 * meter_width + ui.meter_gap;
  const float start_x = rec.x + (rec.width - group_width) / 2;
  const float start_y = rec.y + (body_height - pad_width) / 2;
  DriveLayout result;
  result.arrows = { { { start_x + arrow + ui.arrow_gap, start_y, arrow, arrow },
                      { start_x + arrow + ui.arrow_gap,
                        start_y + 2 * (arrow + ui.arrow_gap),
                        arrow,
                        arrow },
                      { start_x, start_y + arrow + ui.arrow_gap, arrow, arrow },
                      { start_x + 2 * (arrow + ui.arrow_gap),
                        start_y + arrow + ui.arrow_gap,
                        arrow,
                        arrow } } };
  const float meter_x = start_x + pad_width + ui.pad_meter_gap;
  result.meters = { { { meter_x, rec.y, meter_width, body_height },
                      { meter_x + meter_width + ui.meter_gap,
                        rec.y,
                        meter_width,
                        body_height } } };
  result.lock = {
    rec.x, rec.y + rec.height - ui.lock_height, rec.width, ui.lock_height
  };
  return result;
}

void
DriveControls::self_draw()
{
  const std::array<bool, 4> held{
    input.up, input.down, input.left, input.right
  };
  const std::array<Vector2, 4> directions{
    { { 0, -1 }, { 0, 1 }, { -1, 0 }, { 1, 0 } }
  };
  for (std::size_t index = 0; index < held.size(); ++index) {
    const auto rec = positions.arrows[index];
    DrawRectangleRounded(rec,
                         ui.roundness,
                         0,
                         held[index] ? ui.controls.color : ui.control_surface);
    const Vector2 center{ rec.x + rec.width / 2, rec.y + rec.height / 2 };
    const float icon_scale = std::min(rec.width, rec.height) / 48;
    if (index >= 2) {
      const float size = 32 * icon_scale;
      DrawTexturePro(turn_arrow,
                     { 0,
                       0,
                       float(index == 2 ? -turn_arrow.width : turn_arrow.width),
                       float(turn_arrow.height) },
                     { center.x - size / 2, center.y - size / 2, size, size },
                     { 0, 0 },
                     0,
                     ui.text);
      continue;
    }
    const auto direction = directions[index];
    const Vector2 side{ -direction.y, direction.x };
    const Vector2 tip{ center.x + 10 * icon_scale * direction.x,
                       center.y + 10 * icon_scale * direction.y };
    const Vector2 base{ center.x + 2 * icon_scale * direction.x,
                        center.y + 2 * icon_scale * direction.y };
    DrawLineEx({ center.x - 8 * icon_scale * direction.x,
                 center.y - 8 * icon_scale * direction.y },
               base,
               3 * icon_scale,
               ui.text);
    DrawTriangle(
      tip,
      { base.x - 6 * icon_scale * side.x, base.y - 6 * icon_scale * side.y },
      { base.x + 6 * icon_scale * side.x, base.y + 6 * icon_scale * side.y },
      ui.text);
  }
  const auto lock = positions.lock;
  DrawRectangleRounded(lock,
                       ui.roundness,
                       0,
                       drive.state().locked ? ui.thrust_locked
                                            : ui.control_surface);
  const char* label =
    drive.state().locked ? "Speed lock: Locked" : "Speed lock: Unlocked";
  draw_text(label,
            lock.x + 10,
            lock.y + (lock.height - ui.control_font) / 2,
            ui.control_font,
            ui.text);
  draw_text("Space",
            lock.x + lock.width - measure_text("Space", ui.control_font) - 10,
            lock.y + (lock.height - ui.control_font) / 2,
            ui.control_font,
            ui.text);
}

TableControls::TableControls(const UiStore& ui, Rectangle rec, Wire& wire)
  : Drawable(ui, rec)
  , wire(wire)
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
TableControls::self_draw()
{
  const auto link = wire.link();
  const auto health = link_health(link);
  const std::array<const char*, 6> labels{ "Link",  "Arm",   "Pos X",
                                           "Pos Y", "Speed", "Heading" };
  // Stale values stay visible but faded; missing values are never shown as
  // zeros.
  std::array<std::string, 6> values{ link_status(health), "--", "--",
                                     "--",                "--", "--" };
  std::array<bool, 6> fresh{
    health.bridge && health.localization && health.hardware,
    health.hardware,
    health.localization,
    health.localization,
    health.localization,
    health.localization
  };
  if (const auto& reading = link.telemetry) {
    if (reading->hardware_age_ms != msg::never) {
      values[1] = reading->arm_upright ? "Upright" : "Not upright";
    }
    if (reading->localization_age_ms != msg::never) {
      values[2] = fmt::format("{:.2f} m", reading->x);
      values[3] = fmt::format("{:.2f} m", reading->y);
      values[4] = fmt::format("{:.2f} m/s", reading->speed_mps);
      values[5] =
        fmt::format("{:.0f} deg", reading->heading_rad * 180 / PI);
    }
  }
  const auto rec = get_rec();
  const float column_width = (rec.width - 14) / 2;
  const float row_height = rec.height / 3;
  for (std::size_t index = 0; index < labels.size(); ++index) {
    const float x = rec.x + (index % 2) * (column_width + 14);
    const float y =
      rec.y + (index / 2) * row_height + (row_height - ui.control_font) / 2;
    draw_text(labels[index], x, y, ui.control_font, ui.text);
    draw_text(values[index].c_str(),
              x + column_width -
                measure_text(values[index].c_str(), ui.control_font),
              y,
              ui.control_font,
              fresh[index] ? ui.text : Fade(ui.text, 0.4f));
  }
}

PickupControl::PickupControl(const UiStore& ui,
                             Rectangle rec,
                             StateHandler& handler,
                             Wire& wire,
                             PickupController& controller)
  : Drawable(ui, rec)
  , controller(controller)
  , wire(wire)
  , handler(handler)
{
  connections[0] =
    handler.register_key_press(KEY_J, [this] { requested = true; });
  connections[1] = handler.register_mouse_click([this](auto position) {
    requested = requested || is_inside(position, get_rec());
  });
}

void
PickupControl::self_update(float dt)
{
  const auto link = wire.link();
  controller.update({ .requested = requested,
                      .arm_upright = arm_upright(link, link_health(link)),
                      .focused = handler.is_focused() },
                    dt);
  requested = false;
  if (controller.take_command()) {
    wire.send_pickup();
  }
}

void
PickupControl::self_draw()
{
  const auto rec = get_rec();
  const auto& state = controller.state();
  const Color fill = state.arm_upright ? ui.thrust_locked : ui.unchecked;
  const Color text = state.arm_upright ? ui.text : Fade(ui.text, 0.6f);
  DrawRectangleRounded(rec, ui.roundness, 0, fill);
  if (state.rejected_remaining > 0) {
    const float strength =
      state.rejected_remaining / PickupController::rejected_flash_seconds;
    DrawRectangleRoundedLinesEx(
      rec, ui.roundness, 0, 4, Fade(ui.thrust, strength));
  }
  const char* title = "Pick up mine";
  const char* shortcut = "J";
  const char* subtitle = state.arm_upright ? "Arm upright" : "Arm not upright";
  const float gap = 16;
  const float title_width = measure_text(title, ui.title_font);
  const float row_width = title_width + gap + measure_text(shortcut, ui.title_font);
  const float block_height = ui.title_font + 6 + ui.control_font;
  const float top = rec.y + (rec.height - block_height) / 2;
  const float x = rec.x + (rec.width - row_width) / 2;
  draw_text(title, x, top, ui.title_font, text);
  draw_text(shortcut, x + title_width + gap, top, ui.title_font, text);
  draw_text(subtitle,
            rec.x + (rec.width - measure_text(subtitle, ui.control_font)) / 2,
            top + ui.title_font + 6,
            ui.control_font,
            text);
}

Controls::Controls(const UiStore& ui,
                   Wire& wire,
                   StateHandler& handler,
                   DriveController& drive,
                   PickupController& pickup)
  : Section(ui,
            ui.controls.bounds.resolve(ui.content_rect()),
            "Controls",
            ui.controls.color)
  , drive(drive)
  , wire(wire)
  , handler(handler)
{
  const std::array<int, 8> keys{ KEY_UP,   KEY_W, KEY_DOWN,  KEY_S,
                                 KEY_LEFT, KEY_A, KEY_RIGHT, KEY_D };
  for (std::size_t index = 0; index < keys.size(); ++index) {
    key_connections[index] = handler.register_key_down(
      keys[index], [this, index](bool down) { held_keys[index] = down; });
  }
  key_connections[8] =
    handler.register_key_press(KEY_SPACE, [this] { lock_requested = true; });
  const auto rec = get_rec(ui.section_padding);
  const float table_height = rec.height * ui.table_fraction;
  const float pickup_height = ui.pickup_height();
  children.push_back(std::make_unique<TableControls>(
    ui, Rectangle{ rec.x, rec.y, rec.width, table_height }, wire));
  children.push_back(std::make_unique<DriveControls>(
    ui,
    Rectangle{ rec.x,
               rec.y + table_height + ui.control_gap,
               rec.width,
               rec.height - table_height - pickup_height -
                 2 * ui.control_gap },
    drive,
    input));
  children.push_back(std::make_unique<PickupControl>(
    ui,
    Rectangle{
      rec.x, rec.y + rec.height - pickup_height, rec.width, pickup_height },
    handler,
    wire,
    pickup));
}

void
Controls::self_update(float dt)
{
  input = { held_keys[0] || held_keys[1],
            held_keys[2] || held_keys[3],
            held_keys[4] || held_keys[5],
            held_keys[6] || held_keys[7],
            lock_requested,
            handler.is_focused() };
  lock_requested = false;
  drive.update(input, dt);
  wire.set_drive(drive.state().linear, drive.state().angular);
}

Warnings::Warnings(const UiStore& ui)
  : Section(ui,
            ui.warnings.bounds.resolve(ui.content_rect()),
            "Warnings & Messages",
            ui.warnings.color)
{
}
