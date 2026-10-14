#pragma once
#include <memory>

#include "Types.hpp"
namespace windchimes {
// One mono travel path per source, before speaker routing and room sends.
class Propagation {
  static constexpr unsigned capacity = 65536;
  std::unique_ptr<float[]> buffer{new float[capacity]{}};
  unsigned position = 0, valid = 0;
  float target = 0.f, tap = 0.f, previous = 0.f, blend = 1.f, blendStep = .001f;
  float read(float lag) const {
    if (valid <= std::ceil(lag)) return 0.f;
    double index = double(position) - lag;
    if (index < 0.) index += capacity;
    unsigned a = static_cast<unsigned>(index);
    float fraction = index - a;
    return buffer[a] * (1.f - fraction) +
           buffer[(a + 1) & (capacity - 1)] * fraction;
  }

 public:
  void reset() {
    valid = 0;
    blend = 1.f;
  }
  void configure(float rate, float distance) {
    target =
        std::min(float(capacity - 2), .08f * rate * clamp(distance, 0.f, 1.f));
    blendStep = 1.f / std::max(1.f, .02f * rate);
  }
  float process(float input) {
    if (!valid) {
      tap = previous = target;
      blend = 1.f;
    }
    if (blend >= 1.f && std::fabs(target - tap) > .1f) {
      previous = tap;
      tap = target;
      blend = 0.f;
    }
    buffer[position] = input;
    valid = std::min(valid + 1, capacity - 1);
    float out = read(tap);
    if (blend < 1.f) out = read(previous) * (1.f - blend) + out * blend;
    blend = std::min(1.f, blend + blendStep);
    position = (position + 1) & (capacity - 1);
    return out;
  }
};
}  // namespace windchimes
