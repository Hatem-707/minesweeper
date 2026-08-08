#pragma once
#include <cstdint>
#pragma pack(push, 1)
struct MineUpdate
{
  uint8_t surface;
  float pos_x;
  float pos_y;
  float confidence;
};

struct Telemetry
{
  float speed;
  float heading;
  float pos_x;
  float pos_y;
};

struct CmdVelocity
{
  int16_t x;
  int16_t y;
};

enum class CmdArm : uint8_t
{
  Extend,
  Retract,
};

enum class CmdMagnet : uint8_t
{
  On,
  Off,
};

struct CmdResetPos {
  float pos_x;
  float pos_y;
};

enum class MessageType : uint8_t
{
  MineUpdate,
  Telemetry,
  CmdVelocity,
  CmdResetPos,
  CmdArm,
  CmdMagnet,
};

struct MessageMeta
{
  MessageType type;
  int32_t seq_num;
  int64_t time_ns;
};

#pragma pack(pop)
