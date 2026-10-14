#include "ui_store.hpp"
#include "toml.hpp"
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

namespace {
[[noreturn]] void
invalid(const std::string& key, const std::string& reason)
{
  throw std::runtime_error("Invalid UI parameter '" + key + "': " + reason);
}

const toml::value&
table(const toml::value& root, const std::string& key)
{
  static const toml::value empty(toml::table{});
  if (!root.contains(key)) {
    return empty;
  }
  const auto& value = root.at(key);
  if (!value.is_table()) {
    invalid(key, "expected a table");
  }
  return value;
}

int
integer(const toml::value& section,
        const std::string& key,
        int fallback,
        int minimum = 0)
{
  if (!section.contains(key)) {
    return fallback;
  }
  const auto& value = section.at(key);
  if (!value.is_integer()) {
    invalid(key, "expected an integer");
  }
  const auto number = value.as_integer();
  if (number < minimum || number > std::numeric_limits<int>::max()) {
    invalid(key, "integer is outside the allowed range");
  }
  return static_cast<int>(number);
}

float
number(const toml::value& section,
       const std::string& key,
       float fallback,
       float minimum,
       float maximum)
{
  if (!section.contains(key)) {
    return fallback;
  }
  const auto& value = section.at(key);
  if (!value.is_integer() && !value.is_floating()) {
    invalid(key, "expected a number");
  }
  const double result = value.is_integer()
                          ? static_cast<double>(value.as_integer())
                          : value.as_floating();
  if (!std::isfinite(result) || result < minimum || result > maximum) {
    invalid(key, "number is outside the allowed range");
  }
  return static_cast<float>(result);
}

Color
color(const toml::value& section, const std::string& key, Color fallback)
{
  if (!section.contains(key)) {
    return fallback;
  }
  const auto& value = section.at(key);
  if (!value.is_array() || (value.size() != 3 && value.size() != 4)) {
    invalid(key, "expected an RGB or RGBA array");
  }
  unsigned char channels[4]{ 0, 0, 0, 255 };
  for (std::size_t index = 0; index < value.size(); ++index) {
    const auto& channel = value.at(index);
    if (!channel.is_integer() || channel.as_integer() < 0 ||
        channel.as_integer() > 255) {
      invalid(key, "color channels must be integers from 0 to 255");
    }
    channels[index] = static_cast<unsigned char>(channel.as_integer());
  }
  return { channels[0], channels[1], channels[2], channels[3] };
}

PanelStyle
panel(const toml::value& root, const std::string& key, PanelStyle fallback)
{
  const auto& section = table(root, key);
  auto& bounds = fallback.bounds;
  bounds.x = number(section, "x", bounds.x, 0.0f, 1.0f);
  bounds.y = number(section, "y", bounds.y, 0.0f, 1.0f);
  bounds.width = number(section, "width", bounds.width, 0.001f, 1.0f);
  bounds.height = number(section, "height", bounds.height, 0.001f, 1.0f);
  if (bounds.x + bounds.width > 1.00001f ||
      bounds.y + bounds.height > 1.00001f) {
    invalid(key, "panel must fit within its parent");
  }
  fallback.color = color(section, "color", fallback.color);
  return fallback;
}
}

Rectangle
RelativeRect::resolve(Rectangle parent) const
{
  return { parent.x + parent.width * x,
           parent.y + parent.height * y,
           parent.width * width,
           parent.height * height };
}

UiStore::UiStore(const std::filesystem::path& file)
{
  const auto root = toml::parse(file.string());
  const auto& screen = table(root, "Screen");
  screen_width = integer(screen, "width", screen_width, 1);
  screen_height = integer(screen, "height", screen_height, 1);
  target_fps = integer(screen, "fps", target_fps, 1);
  camera_resolution =
    integer(table(root, "Camera"), "resolution", camera_resolution, 1);

  const auto& style = table(root, "Style");
  app_padding = integer(style, "app_padding", app_padding);
  section_padding = integer(style, "section_padding", section_padding);
  cell_padding = integer(style, "cell_padding", cell_padding);
  text_padding = integer(style, "text_padding", text_padding);
  title_font = integer(style, "title_font", title_font, 1);
  roundness = number(style, "roundness", roundness, 0.0f, 1.0f);
  status_font = integer(table(root, "Status"), "font", status_font, 1);

  const auto& hover = table(root, "Hover");
  hover_scale = number(hover, "scale", hover_scale, 1.0f, 3.0f);
  spring_frequency =
    number(hover, "frequency", spring_frequency, 0.001f, 1000.0f);
  spring_damping_ratio =
    number(hover, "damping_ratio", spring_damping_ratio, 0.001f, 0.999f);

  mapping.field_size_m = number(table(root, "Map"),
                                "field_size",
                                float(mapping.field_size_m),
                                0.1f,
                                1000.0f);
  const auto& detector = table(root, "Detector");
  mapping.detector_dx =
    number(detector, "dx", float(mapping.detector_dx), -10.0f, 10.0f);
  mapping.detector_dy =
    number(detector, "dy", float(mapping.detector_dy), -10.0f, 10.0f);
  const auto& classification = table(root, "Classification");
  mapping.classify_seconds = number(
    classification, "seconds", float(mapping.classify_seconds), 0.1f, 600.0f);
  mapping.classify_distance_m = number(classification,
                                       "distance",
                                       float(mapping.classify_distance_m),
                                       0.01f,
                                       100.0f);

  map = panel(root, "Map", map);
  feed = panel(root, "Camera view", feed);
  status = panel(root, "Status", status);
  messages = panel(root, "Messages", messages);

  const auto& colors = table(root, "Colors");
  background = color(colors, "background", background);
  text = color(colors, "text", text);
  unchecked = color(colors, "unchecked", unchecked);
  clean = color(colors, "clean", clean);
  pending = color(colors, "pending", pending);
  surface_mine = color(colors, "surface_mine", surface_mine);
  buried_mine = color(colors, "buried_mine", buried_mine);
  robot = color(colors, "robot", robot);
  selection = color(colors, "selection", selection);
  validate_layout();
}

Rectangle
UiStore::screen_rect() const
{
  return {
    0, 0, static_cast<float>(screen_width), static_cast<float>(screen_height)
  };
}

Rectangle
UiStore::content_rect() const
{
  const float padding = static_cast<float>(app_padding);
  return { padding,
           padding,
           screen_width - 2.0f * padding,
           screen_height - 2.0f * padding };
}

void
UiStore::validate_layout() const
{
  const auto parent = content_rect();
  if (parent.width <= 0 || parent.height <= 0) {
    invalid("Style.app_padding", "padding consumes the entire window");
  }
  for (const auto& style : { map, feed, status, messages }) {
    const auto rec = style.bounds.resolve(parent);
    if (rec.width <= 2.0f * section_padding ||
        rec.height <= 2.0f * section_padding) {
      invalid("Style.section_padding", "padding consumes a panel");
    }
  }
  const auto map_rect = map.bounds.resolve(parent);
  if ((map_rect.width - 2.0f * section_padding) / grid_size <=
        2.0f * cell_padding ||
      (map_rect.height - 2.0f * section_padding) / grid_size <=
        2.0f * cell_padding) {
    invalid("Style.cell_padding", "map cells have no drawable area");
  }
  const auto status_rect = status.bounds.resolve(parent);
  if (status_rect.height - 2.0f * section_padding < 4.0f * (status_font + 2)) {
    invalid("Status", "not enough room for the sensor readings");
  }
}
