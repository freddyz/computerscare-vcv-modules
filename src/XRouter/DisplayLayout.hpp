#pragma once
#include <algorithm>
namespace xrouter {
constexpr float sourceLeft = 5.f;
constexpr float sourceGap = 1.5f;
inline float sourceColumnWidth(float width) {
  return (width - 2.f * sourceLeft) / 16.f;
}
inline float sourceFontSize(float width, float longestAddressWidthAt8) {
  return std::min(11.f, 8.f * (sourceColumnWidth(width) - sourceGap) /
                            longestAddressWidthAt8);
}
inline float sourceColumnX(float width, int column) {
  return sourceLeft + (column + .5f) * sourceColumnWidth(width);
}
}  // namespace xrouter
