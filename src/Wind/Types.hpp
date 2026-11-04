#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace computerscare {
namespace wind {
constexpr int fieldSites = 16;
constexpr float pi = 3.14159265358979323846f;
inline float clamp(float x, float lo, float hi) {
  return std::max(lo, std::min(x, hi));
}
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
}  // namespace wind
}  // namespace computerscare
