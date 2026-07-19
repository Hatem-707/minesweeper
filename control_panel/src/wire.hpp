#include <cstdint>
struct MineUpdate
{
  float pos_x;
  float pos_y;
  bool surface;
  float confidence;
};

struct Telemetry
{
  float speed;
  float angle;
  float pos_x;
  float pos_y;
};

struct CmdVelocity
{
  int16_t x;
  int16_t y;
};

using CmdArm = bool;
