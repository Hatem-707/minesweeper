#pragma once
#include <cstdint>

struct AppState
{
  float speed;
  float health;
  float heading;
  float pos_x;
  float pos_y;
  bool magnet;
  int16_t thrust;
};
