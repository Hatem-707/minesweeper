#pragma once
#include <cstdint>
#include <minesweeper/messages.hpp>
#include <optional>
#include <string>
#include <utility>
#include <vector>

struct MapConfig
{
  int grid_size = 20;
  double field_size_m = 20; // Square field; cells are field_size / grid_size.
  // Metal detector position in the robot frame (+x forward, +y left).
  double detector_dx = 0;
  double detector_dy = 0;
  // A metal hit becomes SURFACE when the arm leaves upright within this time
  // or before the robot moves this far; otherwise it becomes BURIED.
  double classify_seconds = 5;
  double classify_distance_m = 0.5;
};

enum class MineState : std::uint8_t
{
  unchecked,
  clean,   // The detector has passed over this cell.
  pending, // Metal hit waiting for the arm.
  surface_mine,
  buried_mine,
};

// Grid indices in the map frame: x right, y up, (0, 0) at the start corner.
struct Cell
{
  int x = 0;
  int y = 0;
  bool operator==(const Cell&) const = default;
};

// Builds the mine map from Telemetry. No raylib: tested on its own.
class MineMap
{
public:
  explicit MineMap(const MapConfig& config = {});

  // Feed every Telemetry in arrival order.
  void update(const msg::Telemetry& telemetry, std::int64_t received_ns);
  // Classifies a pending hit as BURIED once its time window has passed.
  void tick(std::int64_t now_ns);

  MineState at(Cell cell) const;
  std::optional<Cell> cell_of(double x, double y) const;
  std::pair<double, double> center_of(Cell cell) const;
  int count(MineState state) const;
  const MapConfig& config() const { return settings; }
  // Operator-facing notes about mapping decisions, oldest first.
  std::vector<std::string> take_messages();

private:
  struct Candidate
  {
    int id;
    Cell cell;
    double x, y;             // Mine position, map metres.
    double robot_x, robot_y; // Robot position at the hit.
    std::int64_t started_ns;
    MineState previous;
  };

  MapConfig settings;
  std::vector<MineState> cells;
  std::optional<Candidate> pending;
  bool metal_previous = false;
  int next_id = 1;
  std::vector<std::string> messages;

  MineState& state(Cell cell);
  void start(Cell cell, double x, double y, const msg::Telemetry& telemetry,
             std::int64_t received_ns);
  void resolve(bool surface);
};
