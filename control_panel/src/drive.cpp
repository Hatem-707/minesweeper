#include "drive.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

DriveController::DriveController(float ramp_rate, float thrust_limit)
  : ramp_rate(ramp_rate)
  , thrust_limit(thrust_limit)
{
  if (!std::isfinite(ramp_rate) || ramp_rate <= 0 ||
      !std::isfinite(thrust_limit) || thrust_limit <= 0) {
    throw std::invalid_argument(
      "Drive ramp rate and thrust limit must be positive");
  }
}

void
DriveController::update(const DriveInput& input, float dt)
{
  if (!input.focused) {
    command = {};
    previous = {};
    locked_directions = {};
    return;
  }
  const bool new_direction =
    (input.up && !previous.up) || (input.down && !previous.down) ||
    (input.left && !previous.left) || (input.right && !previous.right);
  previous = input;
  const bool any_direction =
    input.up || input.down || input.left || input.right;
  const bool different_combination = input.up != locked_directions.up ||
                                     input.down != locked_directions.down ||
                                     input.left != locked_directions.left ||
                                     input.right != locked_directions.right;
  // Releases preserve the lock snapshot. Compare a newly pressed combination
  // with the directions captured at locking, not with the previous frame.
  if (new_direction && (!command.locked || different_combination)) {
    command.locked = false;
  } else if (input.toggle_lock && (any_direction || command.locked)) {
    command.locked = !command.locked;
    if (command.locked) {
      locked_directions = input;
    }
  }
  if (command.locked) {
    return;
  }
  if (!any_direction) {
    command = {};
    return;
  }
  const float elapsed = std::isfinite(dt) && dt > 0 ? dt : 0;
  const auto ramp = [&](float& thrust, int direction) {
    if (direction == 0) {
      thrust = 0;
      return;
    }
    if ((thrust > 0 && direction < 0) || (thrust < 0 && direction > 0)) {
      thrust = 0;
    }
    thrust = direction *
             std::min(thrust_limit, std::abs(thrust) + ramp_rate * elapsed);
  };
  ramp(command.linear, int(input.up) - int(input.down));
  ramp(command.angular, int(input.left) - int(input.right));
}
