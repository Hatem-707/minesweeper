#pragma once
#include <cstdint>
#include <optional>

struct PickupInput
{
  bool requested = false;
  bool arm_upright = false;
  bool focused = true;
};

struct PickupState
{
  bool arm_upright = false;
  float rejected_remaining = 0; // Seconds of "press ignored" feedback left.
};

// Turns a pickup request into a single one-shot command, accepted only while
// the arm's contact switch reports it upright. The micro-controller runs the
// whole pickup sequence; the arm has no other state here.
class PickupController
{
  PickupState current;
  std::optional<std::uint8_t> pending;

public:
  static constexpr std::uint8_t command_value = 1;
  static constexpr float rejected_flash_seconds = 0.3f;

  void update(const PickupInput& input, float dt);
  // Returns the command once per accepted request, then nothing.
  std::optional<std::uint8_t> take_command();
  const PickupState& state() const { return current; }
};
