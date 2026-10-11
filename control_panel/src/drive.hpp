#pragma once

struct DriveInput
{
  bool up = false;
  bool down = false;
  bool left = false;
  bool right = false;
  bool toggle_lock = false;
  bool focused = true;
};

struct DriveState
{
  float linear = 0;
  float angular = 0;
  bool locked = false;
  // Signed motor commands: positive linear is forward; positive angular is
  // a left (counterclockwise) turn, as in ROS. These are not calibrated
  // velocities or an absolute robot
  // orientation.
};

class DriveController
{
  DriveState command;
  DriveInput previous;
  DriveInput locked_directions;
  float ramp_rate;
  float thrust_limit;

public:
  explicit DriveController(float ramp_rate = 85, float thrust_limit = 255);
  void update(const DriveInput& input, float dt);
  const DriveState& state() const { return command; }
  float limit() const { return thrust_limit; }
};
