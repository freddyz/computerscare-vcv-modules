#pragma once
#include "Types.hpp"
namespace windchimes {
struct Quad {
  std::array<float, 4> channel{};  // FL, FR, RL, RR
};
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
inline std::array<float, 4> spatialGains(float x, float y, unsigned mask) {
  std::array<float, 4> g{};
  x = clamp(x, 0.f, 1.f);
  y = clamp(y, 0.f, 1.f);
  mask &= 15u;
  int count = 0, first = -1, second = -1;
  for (int i = 0; i < 4; ++i)
    if (mask & (1u << i)) {
      if (first < 0)
        first = i;
      else
        second = i;
      ++count;
    }
  if (!count) return g;
  if (count == 1) {
    g[first] = 1.f;
    return g;
  }
  const float sx[4] = {-1.f, 1.f, -1.f, 1.f};
  const float sy[4] = {-1.f, -1.f, 1.f, 1.f};
  if (count == 2) {
    // Project onto the pair's axis: front/rear pairs preserve left/right,
    // side pairs preserve front/back, diagonals use both coordinates.
    float dx = sx[second] - sx[first], dy = sy[second] - sy[first];
    float t = clamp(0.5f + ((2.f * x - 1.f) * dx + (2.f * y - 1.f) * dy) /
                               (dx * dx + dy * dy),
                    0.f, 1.f);
    g[first] = t == 1.f ? 0.f : std::cos(t * pi * 0.5f);
    g[second] = t == 0.f ? 0.f : std::sin(t * pi * 0.5f);
    return g;
  }
  // Speaker azimuths: FL -45°, FR +45°, RR +135°, RL +225°.
  // Only the two speakers surrounding the source angle receive direct sound.
  float px = 2.f * x - 1.f, py = 2.f * y - 1.f;
  float radius = std::hypot(px, py);
  if (radius > 1e-7f) {
    float angle = std::atan2(px, -py) + pi * 0.25f;
    if (angle < 0.f) angle += 2.f * pi;
    float sector = angle / (pi * 0.5f);
    int index = std::min(static_cast<int>(sector), 3);
    float t = clamp(sector - index, 0.f, 1.f);
    const int speakers[4] = {0, 1, 3, 2};
    g[speakers[index]] = t == 1.f ? 0.f : std::cos(t * pi * 0.5f);
    g[speakers[(index + 1) % 4]] = t == 0.f ? 0.f : std::sin(t * pi * 0.5f);
  }
  // Within 6% of the stage width, smoothly approach a centered image instead
  // of making the direction jump as a source crosses the listener.
  float blend = clamp(radius / 0.12f, 0.f, 1.f);
  blend = blend * blend * (3.f - 2.f * blend);
  for (float& value : g) value = 0.5f * (1.f - blend) + value * blend;
  for (int i = 0; i < 4; ++i)
    if (!(mask & (1u << i))) {
      float missing = g[i];
      g[i] = 0.f;
      int neighbors = 0;
      for (int j = 0; j < 4; ++j)
        if ((mask & (1u << j)) && (sx[i] == sx[j] || sy[i] == sy[j]))
          ++neighbors;
      for (int j = 0; j < 4; ++j)
        if ((mask & (1u << j)) && (sx[i] == sx[j] || sy[i] == sy[j]))
          g[j] += missing / std::sqrt(float(neighbors));
    }
  float power = 0.f;
  for (float v : g) power += v * v;
  float norm = 1.f / std::sqrt(std::max(power, 1e-12f));
  for (float& v : g) v *= norm;
  return g;
}
}  // namespace windchimes
