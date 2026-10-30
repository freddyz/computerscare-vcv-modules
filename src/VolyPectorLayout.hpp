#pragma once
#include <algorithm>
#include <array>
#include <cmath>

namespace computerscare {
namespace voly {
namespace layout {
struct Rect {
  float x = 0, y = 0, width = 0, height = 0;
  Rect() = default;
  Rect(float x, float y, float w, float h) : x(x), y(y), width(w), height(h) {}
};
struct Division {
  Rect bounds, title, offset, scale;
  std::array<Rect, 16> values{};
};
struct Result {
  bool valid = false, controls = false;
  int divisions = 0, count = 0, columns = 0;
  float diameter = 0, divisionHeight = 0, titleHeight = 0;
  std::array<Division, 5> groups{};
};
// Layout policy lives here: adjust the last count using 2 and 3 columns.
// The fifth row is only for the preview stress test (live selection limit: 4).
struct ColumnPolicy {
  int twoColumnsThrough, threeColumnsThrough;
};
constexpr ColumnPolicy columnPolicy[] = {
    {8, 12}, {8, 12}, {6, 12}, {4, 9}, {4, 9}};
constexpr float controlsTop = 42.f, titleTop = 18.f;
constexpr float labelHeightRatio = 11.f / 24.f, columnRiseRatio = 5.f / 24.f;
constexpr float bottomMargin = 2.f;
// Pure geometry: fixed columns, including incomplete rows; no DSP or widgets.
inline Result calculate(int divisions, int count, float width = 54,
                        float height = 264) {
  Result result;
  if (divisions < 1 || divisions > 5 || count < 1 || count > 16 || width <= 0 ||
      height <= 0)
    return result;
  result.divisions = divisions;
  result.count = count;
  const auto policy = columnPolicy[divisions - 1];
  result.columns = count <= policy.twoColumnsThrough
                       ? 2
                       : (count <= policy.threeColumnsThrough ? 3 : 4);
  result.divisionHeight = height / divisions;
  const int rows = (count + result.columns - 1) / result.columns;
  const float cellWidth = (width - 2.f) / result.columns;
  auto chooseSize = [&](bool controls) {
    const float top = controls ? controlsTop : titleTop;
    for (int size = 24; size >= 4; --size) {
      if (size > cellWidth - 1.f) continue;
      const float label = labelHeightRatio * size;
      const float rise = columnRiseRatio * size * (result.columns - 1);
      const float pitch = size + 2.f;
      if (top + label + rise + (rows - 1) * pitch + size + bottomMargin <=
          result.divisionHeight + .001f)
        return float(size);
    }
    return 0.f;
  };
  const float withControls = chooseSize(true);
  result.controls = withControls >= 12.f;
  result.diameter = result.controls ? withControls : chooseSize(false);
  if (result.diameter <= 0) return result;
  result.titleHeight = result.controls ? 18.f : 16.f;
  const float top = result.controls ? controlsTop : titleTop;
  const float rise = columnRiseRatio * result.diameter;
  const float first =
      top + labelHeightRatio * result.diameter + (result.columns - 1) * rise;
  const float last = result.divisionHeight - bottomMargin - result.diameter;
  // Keep neighboring rows together instead of stretching sparse grids.
  const float pitch =
      rows > 1 ? std::min((last - first) / (rows - 1), result.diameter + 6.f)
               : 0.f;
  for (int group = 0; group < divisions; ++group) {
    auto& division = result.groups[group];
    const float y = group * result.divisionHeight;
    division.bounds = Rect(0, y, width, result.divisionHeight);
    division.title = Rect(0, y, 52, result.titleHeight);
    division.offset = Rect(7, y + 16, 24, 24);
    division.scale = Rect(34, y + 12, 18, 18);
    for (int index = 0; index < count; ++index) {
      const int row = index / result.columns, column = index % result.columns;
      const float center = 1.f + (column + .5f) * cellWidth;
      division.values[index] = Rect(center - result.diameter * .5f,
                                    y + first + row * pitch - column * rise,
                                    result.diameter, result.diameter);
    }
  }
  result.valid = true;
  return result;
}
}  // namespace layout
}  // namespace voly
}  // namespace computerscare
