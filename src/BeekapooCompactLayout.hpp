#pragma once
#include <algorithm>
#include <cmath>

namespace beekapoo {
// Pure geometry for the narrow panel. Labels are optional at the physical
// minimum; neither old attenuverter rows nor label budgets set that minimum.
struct CompactPanelLayout {
  int inputColumns = 1, outputColumns = 1;
  float inputWidth = 0, outputWidth = 0;
  float inputHeight = 0, outputHeight = 0, portHeight = 0;
  float parameterMinimum = 0;
  bool beside = false, fits = false;
};
inline CompactPanelLayout compactPanelLayout(float width, float height,
                                             int parameters, int inputs,
                                             int outputs) {
  CompactPanelLayout p;
  auto rows = [](int count, int cols) { return (count + cols - 1) / cols; };
  int paramCols = std::max(1, int((width - 23) / 18));
  p.parameterMinimum = parameters ? 10 + rows(parameters, paramCols) * 18 : 0;
  int columns = std::max(1, int((width - 14) / 20));
  p.inputColumns = std::max(1, std::min(inputs, columns));
  p.outputColumns = std::max(1, std::min(outputs, columns));
  p.inputWidth = p.outputWidth = width - 12;
  p.beside = inputs && outputs && width >= 150;
  if (p.beside) {
    int total = std::max(2, int((width - 46) / 20));
    p.inputColumns = std::max(1, std::min(inputs, total / 2));
    p.outputColumns = std::max(1, std::min(outputs, total - p.inputColumns));
    p.inputColumns = std::max(1, std::min(inputs, total - p.outputColumns));
    int side =
        std::max(rows(inputs, p.inputColumns), rows(outputs, p.outputColumns));
    int stacked = rows(inputs, columns) + rows(outputs, columns);
    p.beside = side <= stacked;
    if (p.beside) {
      float right =
          (width - 46) * p.outputColumns / (p.inputColumns + p.outputColumns);
      if (outputs == 1)
        right =
            std::min(width - 46 - 20 * p.inputColumns, std::max(right, 48.f));
      p.outputWidth = right + 13;
      p.inputWidth = width - 46 - right + 13;
    } else {
      p.inputColumns = std::max(1, std::min(inputs, columns));
      p.outputColumns = std::max(1, std::min(outputs, columns));
    }
  }
  float preferred = width >= 120 && inputs + outputs <= 4 ? 50.f : 40.f;
  for (float row : {50.f, 40.f, 32.f, 28.f, 20.f}) {
    if (row > preferred) continue;
    p.inputHeight = inputs ? 12 + rows(inputs, p.inputColumns) * row : 0;
    p.outputHeight = outputs ? 12 + rows(outputs, p.outputColumns) * row : 0;
    if (p.beside)
      p.inputHeight = p.outputHeight = std::max(p.inputHeight, p.outputHeight);
    p.portHeight =
        p.beside ? p.inputHeight
                 : p.inputHeight + p.outputHeight + (inputs && outputs ? 8 : 0);
    // Keep the same section gaps when captions are sacrificed for controls.
    p.fits = p.portHeight + p.parameterMinimum + 16 <= height;
    if (p.fits) break;
  }
  return p;
}
}  // namespace beekapoo
