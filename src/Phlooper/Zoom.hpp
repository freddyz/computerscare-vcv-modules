#pragma once
#include <algorithm>
#include <array>
#include <cmath>
namespace phlooper {
struct TimeRange {
  float start, end;
  TimeRange(float start = 0.f, float end = 1.f) : start(start), end(end) {}
};
inline TimeRange fitLoopZoom(const std::array<TimeRange, 16>& regions,
                             int count) {
  count = std::max(1, std::min(16, count));
  float anchor = regions[0].start, first = anchor, last = anchor;
  for (int i = 0; i < count; ++i) {
    float length = std::max(0.f, regions[i].end - regions[i].start);
    float start = regions[i].start - std::round(regions[i].start - anchor);
    first = std::min(first, start);
    last = std::max(last, start + length);
  }
  float length = std::max(.00001f, last - first);
  float padding = length * .1f;
  if (length + padding * 2 >= 1.f) return {};
  return {first - padding, last + padding};
}
inline float zoomPoint(float position, TimeRange range) {
  if (range.start == 0.f && range.end == 1.f) return position;
  return position + std::round((range.start + range.end) * .5f - position);
}
}  // namespace phlooper
