#include "pickup.hpp"
#include <algorithm>
#include <utility>

void
PickupController::update(const PickupInput& input, float dt)
{
  current.arm_upright = input.arm_upright;
  current.rejected_remaining = std::max(0.f, current.rejected_remaining - dt);
  if (!input.requested || !input.focused) {
    return;
  }
  if (input.arm_upright) {
    pending = command_value;
  } else {
    current.rejected_remaining = rejected_flash_seconds;
  }
}

std::optional<std::uint8_t>
PickupController::take_command()
{
  return std::exchange(pending, std::nullopt);
}
