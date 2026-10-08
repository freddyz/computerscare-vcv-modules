#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace windchimes {
constexpr int maxSets = 16;
constexpr int maxTubes = 12;
constexpr int modesPerTube = 12;
constexpr float pi = 3.14159265358979323846f;
inline float clamp(float x, float lo, float hi) {
  return std::max(lo, std::min(x, hi));
}
struct SetConfig {
  bool enabled = false;
  int tubes = 6, material = 0, scale = 0, root = 0, divisions = 12, spread = 1;
  float octave = 0.f, fine = 0.f, decay = 0.55f, brightness = 0.55f;
  float strikerWeight = 0.5f, sailSize = 0.5f;
  float hardness = 0.5f, level = 0.7f, swing = 0.5f;
  float x = 0.5f, y = 0.5f;
  float shape = 0.35f, body = 0.65f, inharmonicity = 0.5f;
};
struct Stereo {
  float left, right;
  Stereo(float l = 0.f, float r = 0.f) : left(l), right(r) {}
};
class Random {
  uint32_t state = 1;

 public:
  explicit Random(uint32_t seed = 1) : state(seed ? seed : 1) {}
  float uniform() {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return static_cast<float>(state >> 8) * (1.f / 16777216.f);
  }
  float bipolar() { return 2.f * uniform() - 1.f; }
};
}  // namespace windchimes
