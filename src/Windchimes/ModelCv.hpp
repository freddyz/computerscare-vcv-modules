#pragma once
#include "Types.hpp"

namespace windchimes {
template <typename Input>
float chimeVoltage(Input& input, int chime) {
  int count = input.getChannels();
  return count == 1      ? input.getVoltage(0)
         : chime < count ? input.getVoltage(chime)
                         : 0.f;
}
template <typename Input>
bool chimeGate(bool manual, Input& input, int chime) {
  return input.getChannels() ? chimeVoltage(input, chime) >= 1.f : manual;
}
// Mono broadcasts; poly channels offset matching chime slots. Missing
// channels leave the stored value intact, independent of editor selection.
template <typename Input>
float modelCvValue(float base, Input& input, int chime, float minimum = 0.f,
                   float maximum = 1.f) {
  float voltage = chimeVoltage(input, chime);
  return clamp(base + voltage / 10.f * (maximum - minimum), minimum, maximum);
}
}  // namespace windchimes
