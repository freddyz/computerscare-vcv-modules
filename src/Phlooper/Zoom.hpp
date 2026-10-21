#pragma once
#include <algorithm>
#include <array>
#include <cmath>

#include "Beats.hpp"
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
// Keep the opposite point fixed and prevent crossing or exceeding storage.
inline TimeRange dragLoopPoint(TimeRange region, bool startPoint, float delta,
                               float minimumLength, float maximumLength) {
  float length =
      std::max(minimumLength,
               std::min(maximumLength, region.end - region.start +
                                           (startPoint ? -delta : delta)));
  return startPoint ? TimeRange(region.end - length, region.end)
                    : TimeRange(region.start, region.start + length);
}
inline TimeRange dragLoopPointOnGrid(TimeRange region, bool startPoint,
                                     float delta, float minimumLength,
                                     float maximumLength, float step,
                                     float origin) {
  if (step <= 0.f)
    return dragLoopPoint(region, startPoint, delta, minimumLength,
                         maximumLength);
  double low =
      startPoint ? region.end - maximumLength : region.start + minimumLength;
  double high =
      startPoint ? region.end - minimumLength : region.start + maximumLength;
  double first = std::ceil((low - origin) / step - 1e-6);
  double last = std::floor((high - origin) / step + 1e-6);
  if (first > last)
    return dragLoopPoint(region, startPoint, delta, minimumLength,
                         maximumLength);
  double point = (startPoint ? region.start : region.end) + delta;
  double index = std::floor((point - origin) / step + .500001);
  point = origin + std::max(first, std::min(last, index)) * step;
  return startPoint ? TimeRange(float(point), region.end)
                    : TimeRange(region.start, float(point));
}
inline float controlOffset(int voice, int mode, bool hold, float knob,
                           int channels, float voltage) {
  if (mode == 3) {
    float spacing = hold ? 0.f : voice * beatValue(knob);
    float cv = beatValue(std::max(-10.f, std::min(10.f, voltage)) * 2.f);
    if (channels == 1) return spacing + cv;
    return voice < channels ? cv : spacing;
  }
  float spacing = hold ? 0.f : voice * knob * (mode == 0 ? 5.f : 1.f);
  float cv = std::max(-1.f, std::min(1.f, voltage / 10.f)) *
             (mode == 0 ? 100.f : 20.f);
  if (channels == 1) return spacing + cv;
  return voice < channels ? cv : spacing;
}
// Shift a whole region together; never wrap its end independently of its start.
inline float loopRegionPoint(float start, float point, TimeRange range) {
  float shift =
      range.end - range.start < 1.f ? zoomPoint(start, range) - start : 0.f;
  return point + shift;
}
}  // namespace phlooper
