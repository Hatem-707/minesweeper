#include "link.hpp"

LinkHealth
link_health(const RobotLink& link, std::int64_t now_ns)
{
  LinkHealth health;
  if (link.telemetry &&
      now_ns - link.telemetry_received_ns <= stale_after_ns) {
    health.bridge = true;
    health.localization =
      link.telemetry->localization_age_ms <= stale_after_ms;
    health.hardware = link.telemetry->hardware_age_ms <= stale_after_ms;
  }
  health.vision =
    link.vision && now_ns - link.vision_received_ns <= stale_after_ns;
  return health;
}

bool
arm_upright(const RobotLink& link, const LinkHealth& health)
{
  return health.hardware && link.telemetry->arm_upright == 1;
}
