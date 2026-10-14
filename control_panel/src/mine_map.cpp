#include "mine_map.hpp"
#include "link.hpp"
#include <algorithm>
#include <cmath>
#include <fmt/format.h>

MineMap::MineMap(const MapConfig& config)
  : settings(config)
  , cells(std::size_t(config.grid_size * config.grid_size), MineState::unchecked)
{
}

MineState&
MineMap::state(Cell cell)
{
  return cells[std::size_t(cell.y * settings.grid_size + cell.x)];
}

MineState
MineMap::at(Cell cell) const
{
  if (cell.x < 0 || cell.y < 0 || cell.x >= settings.grid_size ||
      cell.y >= settings.grid_size) {
    return MineState::unchecked;
  }
  return cells[std::size_t(cell.y * settings.grid_size + cell.x)];
}

std::optional<Cell>
MineMap::cell_of(double x, double y) const
{
  const double size = settings.field_size_m / settings.grid_size;
  const double column = std::floor(x / size);
  const double row = std::floor(y / size);
  if (!std::isfinite(column) || !std::isfinite(row) || column < 0 || row < 0 ||
      column >= settings.grid_size || row >= settings.grid_size) {
    return std::nullopt;
  }
  return Cell{ int(column), int(row) };
}

std::pair<double, double>
MineMap::center_of(Cell cell) const
{
  const double size = settings.field_size_m / settings.grid_size;
  return { (cell.x + 0.5) * size, (cell.y + 0.5) * size };
}

int
MineMap::count(MineState wanted) const
{
  return int(std::count(cells.begin(), cells.end(), wanted));
}

std::vector<std::string>
MineMap::take_messages()
{
  return std::exchange(messages, {});
}

void
MineMap::update(const msg::Telemetry& telemetry, std::int64_t received_ns)
{
  const bool localized = telemetry.localization_age_ms <= stale_after_ms;
  const bool hardware = telemetry.hardware_age_ms <= stale_after_ms;
  const double heading = telemetry.heading_rad;
  const double x = telemetry.x + settings.detector_dx * std::cos(heading) -
                   settings.detector_dy * std::sin(heading);
  const double y = telemetry.y + settings.detector_dx * std::sin(heading) +
                   settings.detector_dy * std::cos(heading);
  const auto cell = localized ? cell_of(x, y) : std::nullopt;
  if (cell && state(*cell) == MineState::unchecked) {
    state(*cell) = MineState::clean;
  }

  // Stale hardware reads as no metal, so a fresh positive is a new edge.
  const bool metal = hardware && telemetry.metal_detected == 1;
  const bool rising = metal && !metal_previous;
  metal_previous = metal;
  if (rising) {
    if (!localized) {
      messages.push_back("Metal detected, but localization is stale: not mapped");
    } else if (!cell) {
      messages.push_back(
        fmt::format("Metal detected outside the field at ({:.2f}, {:.2f})", x, y));
    } else if (!pending || pending->cell != *cell) {
      if (pending) {
        resolve(false);
      }
      start(*cell, x, y, telemetry, received_ns);
    }
  }

  if (!pending) {
    return;
  }
  if (hardware && telemetry.arm_upright == 0) {
    resolve(true);
  } else if (localized &&
             std::hypot(telemetry.x - pending->robot_x,
                        telemetry.y - pending->robot_y) >
               settings.classify_distance_m) {
    resolve(false);
  }
}

void
MineMap::tick(std::int64_t now_ns)
{
  if (pending &&
      now_ns - pending->started_ns > settings.classify_seconds * 1e9) {
    resolve(false);
  }
}

void
MineMap::start(Cell cell,
               double x,
               double y,
               const msg::Telemetry& telemetry,
               std::int64_t received_ns)
{
  pending = Candidate{ .id = next_id++,
                       .cell = cell,
                       .x = x,
                       .y = y,
                       .robot_x = telemetry.x,
                       .robot_y = telemetry.y,
                       .started_ns = received_ns,
                       .previous = state(cell) };
  state(cell) = MineState::pending;
  messages.push_back(fmt::format(
    "Mine {}: metal at ({:.2f}, {:.2f}), waiting for the arm", pending->id, x, y));
}

void
MineMap::resolve(bool surface)
{
  auto& cell = state(pending->cell);
  // A repeat hit never downgrades a surface mine to buried.
  cell = surface || pending->previous == MineState::surface_mine
           ? MineState::surface_mine
           : MineState::buried_mine;
  messages.push_back(fmt::format("Mine {}: {} at ({:.2f}, {:.2f})",
                                 pending->id,
                                 cell == MineState::surface_mine ? "SURFACE"
                                                                 : "BURIED",
                                 pending->x,
                                 pending->y));
  pending.reset();
}
