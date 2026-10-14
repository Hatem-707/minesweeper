#pragma once
#include <array>
#include <deque>
#include <memory>
#include <optional>
#include <raylib.h>
#include <string>
#include <vector>

#include "camera.hpp"
#include "mine_map.hpp"
#include "state.hpp"
#include "ui_fonts.hpp"
#include "ui_store.hpp"
#include "wire.hpp"

class Drawable
{
private:
  Rectangle rectangle;
  const UiFonts* fonts = nullptr;

protected:
  const UiStore& ui;
  virtual void draw_children(float dt);
  void draw_text(const char* text,
                 float x,
                 float y,
                 int size,
                 Color color) const;
  float measure_text(const char* text, int size) const;

public:
  std::vector<std::unique_ptr<Drawable>> children;
  virtual ~Drawable() = default;
  Drawable(const UiStore& ui, Rectangle rec);
  void draw(float dt);
  void set_fonts(const UiFonts& fonts);
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

// Operator messages, newest first, plus one prompt line for the action in
// progress.
class MessageLog
{
  static constexpr std::size_t limit = 50;
  std::deque<std::string> entries;
  std::string current_prompt;

public:
  void add(const std::string& message);
  void set_prompt(std::string prompt) { current_prompt = std::move(prompt); }
  const std::string& prompt() const { return current_prompt; }
  const std::deque<std::string>& lines() const { return entries; }
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
  const MineMap& mines;
  Cell cell;
  CellState state;
  bool hovered = false;
  InputConnection hover_connection;

public:
  MapCell(const UiStore& ui,
          Rectangle rec,
          StateHandler&,
          const MineMap& mines,
          Cell cell);
  MapCell(const MapCell&) = delete;
  MapCell& operator=(const MapCell&) = delete;
  MapCell(MapCell&&) = delete;
  MapCell& operator=(MapCell&&) = delete;
  void self_update(float dt) override;
  void self_draw() override;
  bool is_hovered() const { return hovered; }
  Rectangle get_draw_rec() const;
};

// Field map: scanned cells, mines and the robot. Right-click a cell, choose a
// heading with the arrow keys and press Enter to reset the Kalman pose there.
class Map final : public Section
{
  const MineMap& mines;
  Wire& wire;
  MessageLog& log;
  std::array<MapCell*, UiStore::grid_size * UiStore::grid_size> cells;
  std::vector<InputConnection> connections;
  Texture2D arrow{};
  std::optional<Cell> reset_cell;
  std::optional<msg::Heading> reset_heading;

  Rectangle field_rec() const;
  Rectangle cell_rec(Cell cell) const;
  void draw_arrow(Vector2 center, float size, float degrees, Color tint) const;
  void draw_robot() const;
  void draw_reset() const;
  void select(std::pair<float, float> position);
  void send_reset();
  void cancel_reset();
  void update_prompt();

protected:
  void draw_children(float dt) override;

public:
  Map(const UiStore& ui,
      StateHandler& state_handler,
      const MineMap& mines,
      Wire& wire,
      MessageLog& log);
  ~Map() override;
  Map(const Map&) = delete;
  Map& operator=(const Map&) = delete;
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

class StatusTable final : public Drawable
{
  Wire& wire;
  const MineMap& mines;
  void self_draw() override;

public:
  StatusTable(const UiStore& ui,
              Rectangle rec,
              Wire& wire,
              const MineMap& mines);
};

class Status final : public Section
{
public:
  Status(const UiStore& ui, Wire& wire, const MineMap& mines);
};

class MessageList final : public Drawable
{
  const MessageLog& log;
  void self_draw() override;

public:
  MessageList(const UiStore& ui, Rectangle rec, const MessageLog& log);
};

class Messages final : public Section
{
public:
  Messages(const UiStore& ui, const MessageLog& log);
};
