#pragma once
#include "Types.hpp"
namespace windchimes {
// Degrees in a twelve-step reference octave, mapped onto the selected EDO.
constexpr int scaleDegrees[5][7] = {{0, 2, 4, 7, 9, -1, -1},
                                    {0, 2, 3, 7, 10, -1, -1},
                                    {0, 2, 4, 5, 7, 9, 11},
                                    {0, 2, 3, 5, 7, 8, 10},
                                    {0, 2, 4, 6, 8, 10, -1}};
constexpr int scaleSizes[5] = {5, 5, 7, 7, 6};
inline float tubeFrequency(const SetConfig& c, int tube, float transpose) {
  int scale = std::max(0, std::min(c.scale, 4));
  int divisions = std::max(5, std::min(c.divisions, 24));
  int degree = scaleDegrees[scale][tube % scaleSizes[scale]];
  float step = std::round(degree * divisions / 12.f);
  float pitch = c.octave + c.root / 12.f + c.fine / 1200.f + transpose;
  pitch += tube / scaleSizes[scale] + step / divisions;
  return clamp(261.625565f * std::exp2(pitch), 20.f, 12000.f);
}
}  // namespace windchimes
