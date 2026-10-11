#pragma once
#include <filesystem>
#include <raylib.h>

struct RelativeRect
{
  float x;
  float y;
  float width;
  float height;

  Rectangle resolve(Rectangle parent) const;
};

struct PanelStyle
{
  RelativeRect bounds;
  Color color;
};

class UiStore
{
public:
  // Grid topology is fixed; cell rectangles follow the configured map size.
  static constexpr int grid_size = 20;
  static constexpr float lock_height = 44;
  static constexpr float arrow_gap = 4;
  static constexpr float pad_meter_gap = 22;
  static constexpr float meter_gap = 14;
  static constexpr float minimum_arrow_size = 16;

  int screen_width = 1600;
  int screen_height = 900;
  int target_fps = 60;
  int camera_resolution = 720;
  int app_padding = 4;
  int section_padding = 20;
  int cell_padding = 2;
  int text_padding = 8;
  int title_font = 24;
  float roundness = 0.025f;
  float table_fraction = 0.19f;
  int control_font = 20;
  int pickup_button_height = 64;
  int arrow_size = 96;
  int control_gap = 10;
  float drive_ramp_rate = 85;
  float maximum_thrust = 255;
  float hover_scale = 1.2f;
  float spring_frequency = 22.0f; // Angular frequency, radians/second.
  float spring_damping_ratio = 0.65f;

  PanelStyle map{ { 0.325f, 0.0f, 0.675f, 0.9f }, { 42, 161, 152, 255 } };
  PanelStyle feed{ { 0.0f, 0.0f, 0.325f, 0.38f }, { 211, 54, 130, 255 } };
  PanelStyle controls{ { 0.0f, 0.38f, 0.325f, 0.52f }, { 38, 139, 210, 255 } };
  PanelStyle warnings{ { 0.0f, 0.9f, 1.0f, 0.1f }, { 203, 75, 22, 255 } };
  Color background{ 253, 246, 226, 255 };
  Color text{ 0, 0, 0, 255 };
  Color unchecked{ 88, 110, 117, 255 };
  Color clean{ 133, 153, 0, 255 };
  Color surface_mine{ 203, 75, 22, 255 };
  Color buried_mine{ 220, 50, 47, 255 };
  Color thrust{ 220, 50, 47, 255 };
  Color thrust_locked{ 133, 153, 0, 255 };
  Color control_surface{ 238, 232, 213, 255 };

  UiStore() = default;
  explicit UiStore(const std::filesystem::path& file);
  Rectangle screen_rect() const;
  Rectangle content_rect() const;
  float drive_meter_width() const;
  float pickup_height() const;

private:
  void validate_layout() const;
};
