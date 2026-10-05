#pragma once
#include <array>
#include <fmt/base.h>
#include <memory>
#include <raylib.h>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "camera.hpp"
#include "spin_lock.hpp"
#include "state.hpp"
#include "ui_store.hpp"

class Drawable
{
private:
  Rectangle rectangle;

protected:
  const UiStore& ui;
  virtual void draw_children(float dt);

public:
  std::vector<std::unique_ptr<Drawable>> children;
  virtual ~Drawable() = default;
  Drawable(const UiStore& ui, Rectangle rec);
  void draw(float dt);
  virtual void self_update(float) {}
  virtual void self_draw() = 0;
  Rectangle get_rec(float pad = 0) const;
};

class Section : public Drawable
{
  std::string name;
  Color color;

  Rectangle get_title_rec();

public:
  Section(const UiStore& ui, Rectangle rec, std::string name, Color color);
  virtual ~Section() = default;
  void self_draw() final;
};

enum class MineState
{
  unchecked,
  clean,
  surface_mine,
  buried_mine,
};

class CellState
{
  MineState mine_state = MineState::unchecked;
  float scale = 1.0f;
  float target_scale = 1.0f;
  float velocity = 0.0f;

public:
  Color get_color(const UiStore& ui) const;
  float get_scale() const { return scale; }
  void update_state(float dt, const UiStore& ui);
  void set_mine_state(MineState);
  void set_target_scale(float);
};

class MapCell final : public Drawable
{
  CellState state;
  bool hovered = false;
  InputConnection hover_connection;

public:
  MapCell(const UiStore& ui, Rectangle rec, StateHandler&);
  MapCell(const MapCell&) = delete;
  MapCell& operator=(const MapCell&) = delete;
  MapCell(MapCell&&) = delete;
  MapCell& operator=(MapCell&&) = delete;
  void self_update(float dt) override;
  void self_draw() override;
  bool is_hovered() const { return hovered; }
  Rectangle get_draw_rec() const;
  void set_state(CellState target_state);
};

class Map final : public Section
{
  std::array<MapCell*, UiStore::grid_size * UiStore::grid_size> cells;

protected:
  void draw_children(float dt) override;

public:
  Map(const UiStore& ui, StateHandler& state_handler);
  MapCell& cell_at(int x, int y);
};

class CameraFeed final : public Drawable
{
  GstCamera& camera;
  void self_draw() override;

public:
  CameraFeed(const UiStore& ui, GstCamera& camera, Rectangle feed_rec);
};

class Feed final : public Section
{

public:
  Feed(const UiStore& ui, GstCamera& camera);
};

class SliderThrust final : public Drawable
{
  SpinLock<Telemetry>& state;
  void self_draw() override;

public:
  SliderThrust(const UiStore& ui, Rectangle rec, SpinLock<Telemetry>& state);
};

class ThrustControls final : public Section
{

public:
  ThrustControls(const UiStore& ui, Rectangle rec, SpinLock<Telemetry>& state);
};

class TableControls final : public Drawable
{
  SpinLock<Telemetry>& state;
  void self_draw() override;
  std::array<std::string_view, 3> first_column = { "Health:",
                                                   "Pos_x :",
                                                   "Speed :" };
  std::array<std::string_view, 3> second_column = { "Magnet:",
                                                    "Pos_y :",
                                                    "Orien :" };
  std::pair<int, int> get_entry_pos(int x, int y);

public:
  TableControls(const UiStore& ui,
                Rectangle rec,
                SpinLock<Telemetry>& telemetry);
};

class Controls final : public Section
{

public:
  Controls(const UiStore& ui, SpinLock<Telemetry>& telemetry);
};

class Warnings final : public Section
{
public:
  Warnings(const UiStore& ui);
};
