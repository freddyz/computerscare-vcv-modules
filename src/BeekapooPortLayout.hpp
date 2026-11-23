#pragma once
#include <algorithm>
#include <utility>

namespace beekapoo {
// Allocate within the existing sections. A lone output needs enough width
// for its own name; it should not inherit a tiny input-column width.
inline std::pair<float, float> portPaneSpace(float available, int inputs,
                                             int outputs, int outputCount) {
  int columns = inputs + outputs;
  if (!columns) return {0, 0};
  float right = available * outputs / columns;
  if (inputs && outputCount == 1)
    right = std::min(available - 20 * inputs, std::max(right, 48.f));
  right = std::max(0.f, std::min(available, right));
  return {available - right, right};
}
struct PortRect {
  float x = 0, y = 0, w = 0, h = 0;
  PortRect() = default;
  PortRect(float x, float y, float w, float h) : x(x), y(y), w(w), h(h) {}
};
struct PortCell {
  PortRect jack, label;
  bool large = false;
  float fontSize = 0;
};
// Center the native jack and its caption together. Inputs no longer reserve
// an extra row for Beek-owned gain knobs. Reflow only on layout changes.
inline PortCell planPortCell(float width, float height, bool /*input*/) {
  PortCell cell;
  float w = std::max(0.f, width - 4), h = std::max(0.f, height - 4);
  float labelHeight = h >= 30   ? std::min(36.f, std::max(10.f, h * .3f))
                      : h >= 26 ? 8.f
                                : 0.f;
  float gap = labelHeight ? (h >= 30 ? 4.f : 2.f) : 0.f;
  cell.large = w >= 27 && h - labelHeight - gap >= 29;
  float jackW = cell.large ? 27.f : 16.f;
  float jackH = cell.large ? 29.f : 16.f;
  float top = (height - jackH - gap - labelHeight) * .5f;
  cell.jack = {(width - jackW) * .5f, top, jackW, jackH};
  if (labelHeight) {
    cell.label = {2, top + jackH + gap, w, labelHeight};
    cell.fontSize = std::max(8.f, std::min(18.f, labelHeight * .7f));
  }
  return cell;
}
}  // namespace beekapoo
