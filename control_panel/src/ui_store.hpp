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

  int screen_width = 1280;
  int screen_height = 800;
  int target_fps = 60;
  int camera_resolution = 720;
  int app_padding = 4;
  int section_padding = 20;
  int cell_padding = 2;
  int text_padding = 8;
  int title_font = 24;
  int element_font = 20;
  float roundness = 0.025f;
  float table_fraction = 0.75f;
  float hover_scale = 1.2f;
  float spring_frequency = 22.0f; // Angular frequency, radians/second.
  float spring_damping_ratio = 0.65f;

  PanelStyle map{ { 0.3f, 0.0f, 0.7f, 0.9f }, { 42, 161, 152, 255 } };
  PanelStyle feed{ { 0.0f, 0.0f, 0.3f, 0.45f }, { 211, 54, 130, 255 } };
  PanelStyle controls{ { 0.0f, 0.45f, 0.3f, 0.45f }, { 38, 139, 210, 255 } };
  PanelStyle warnings{ { 0.0f, 0.9f, 1.0f, 0.1f }, { 203, 75, 22, 255 } };
  Color background{ 253, 246, 226, 255 };
  Color text{ 0, 0, 0, 255 };
  Color unchecked{ 88, 110, 117, 255 };
  Color clean{ 133, 153, 0, 255 };
  Color surface_mine{ 203, 75, 22, 255 };
  Color buried_mine{ 220, 50, 47, 255 };
  Color thrust{ 220, 50, 47, 255 };

  UiStore() = default;
  explicit UiStore(const std::filesystem::path& file);
  Rectangle screen_rect() const;
  Rectangle content_rect() const;

private:
  void validate_layout() const;
};
