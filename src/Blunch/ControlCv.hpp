#pragma once
#include <algorithm>
#include <cmath>
namespace computerscare {
namespace blunch {
inline float timeScaleWithCv(float knob, float voltage) {
  return std::pow(2.f, std::max(-5.f, std::min(5.f, knob + voltage)));
}
inline float seedWithCv(float knob, float voltage) { return knob + voltage; }
inline bool usesSeed(float seed) { return seed > 0.f; }
}  // namespace blunch
}  // namespace computerscare
