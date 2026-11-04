#pragma once
#include "../Wind/Spatial.hpp"
#include "Types.hpp"
namespace windchimes {
using computerscare::wind::Quad;
using computerscare::wind::spatialGains;
struct StagePosition {
  float x, y;
};
inline StagePosition migrateStagePosition(float pan, float nearness) {
  float radius = 1.f - clamp(nearness, 0.f, 1.f);
  float angle = (clamp(pan, 0.f, 1.f) - 0.5f) * pi;
  return {0.5f + 0.5f * radius * std::sin(angle),
          0.5f - 0.5f * radius * std::cos(angle)};
}
inline float stageDistance(float x, float y) {
  return clamp(std::hypot(2.f * x - 1.f, 2.f * y - 1.f), 0.f, 1.f);
}
}  // namespace windchimes
