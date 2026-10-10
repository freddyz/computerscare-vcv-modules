#pragma once
#include "Types.hpp"

namespace windchimes {
// Mono broadcasts; poly channels offset matching chime slots. Missing
// channels leave the stored value intact, independent of editor selection.
template <typename Input>
float modelCvValue(float base, Input& input, int chime) {
  int count = input.getChannels();
  float voltage = count == 1      ? input.getVoltage(0)
                  : chime < count ? input.getVoltage(chime)
                                  : 0.f;
  return clamp(base + voltage / 10.f, 0.f, 1.f);
}
}  // namespace windchimes
