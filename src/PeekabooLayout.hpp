#pragma once
#include <algorithm>
#include <cmath>

namespace peekaboo {
// All responsive thresholds and physical control budgets live here. Nothing
// in this planner depends on Rack, parameter values, or draw-time state.
struct ResponsiveLayout {
  float width = 0, ownWidth = 0, ownHeight = 0, paramHeight = 0, portHeight = 0;
  float paramWidth = 0, paramLeft = 6, paramTop = 40;
  bool parametersBeside = false, amountInTop = false;
  float paramCell = 0, portCell = 0, ownCell = 0;
  float paramRow = 0, portRow = 0, ownRow = 0;
  bool horizontalFits = true;
  int tier = 0, paramColumns = 1, inputColumns = 0, outputColumns = 0,
      ownColumns = 1;
  float totalHeight() const {
    if (!horizontalFits) return 100000;
    float upper = parametersBeside
                      ? std::max(ownHeight, paramHeight)
                      : ownHeight + (paramHeight ? 8 + paramHeight : 0);
    return 40 + upper + (portHeight ? 8 + portHeight : 0);
  }
};
inline int layoutRows(int count, int columns) {
  return count ? (count + columns - 1) / columns : 0;
}
inline ResponsiveLayout planLayout(float width, int tier, int params,
                                   int inputs, int outputs) {
  ResponsiveLayout p;
  p.width = width;
  p.tier = tier;
  float available = width - 25;
  p.ownWidth = std::min(width - 12, tier >= 2   ? 260.f
                                    : tier == 1 ? 200.f
                                                : 136.f);
  float ownAvailable = p.ownWidth - 12;
  float pair = tier >= 1 ? 42.f : 34.f;
  p.ownColumns = ownAvailable >= pair * 2 + 8 ? 2 : 1;
  p.amountInTop = ownAvailable >= 104;
  p.ownRow = 24;
  p.ownCell = p.ownColumns == 2 ? (ownAvailable - 8) * .5f : ownAvailable;
  int ownRows = p.ownColumns == 2 ? 3 : 6;
  p.ownHeight =
      28 + (tier ? 14 : 0) + ownRows * p.ownRow + (p.amountInTop ? 0 : 24) + 8;
  p.paramCell = tier >= 3 ? 110.f : tier >= 2 ? 90.f : tier == 1 ? 60.f : 28.f;
  p.paramRow = tier >= 3 ? 42.f : tier == 2 ? 34.f : tier == 1 ? 27.f : 28.f;
  p.paramColumns =
      std::max(1, std::min(std::max(1, params), int(available / p.paramCell)));
  float header = tier ? 14.f : 6.f;
  p.paramHeight =
      params ? header + layoutRows(params, p.paramColumns) * p.paramRow + 6 : 0;
  p.paramWidth = width - 12;
  p.paramTop = 40 + p.ownHeight + 8;
  float sideWidth = width - p.ownWidth - 20;
  int sideColumns = std::max(0, int((sideWidth - 13) / p.paramCell));
  if (params && sideColumns > 0) {
    sideColumns = std::min(params, sideColumns);
    float sideHeight =
        header + layoutRows(params, sideColumns) * p.paramRow + 6;
    if (std::max(p.ownHeight, sideHeight) < p.ownHeight + 8 + p.paramHeight) {
      p.parametersBeside = true;
      p.paramColumns = sideColumns;
      p.paramHeight = sideHeight;
      p.paramWidth = sideWidth;
      p.paramLeft = 6 + p.ownWidth + 8;
      p.paramTop = 40;
    }
  }
  p.portCell = tier >= 3 ? 100.f : tier >= 2 ? 84.f : tier == 1 ? 40.f : 22.f;
  p.portRow = tier >= 2 ? 32.f : tier == 1 ? 30.f : 22.f;
  int panes = (inputs ? 1 : 0) + (outputs ? 1 : 0);
  float portAvailable = width - 12 - panes * 13 - (panes == 2 ? 8 : 0);
  int columns = std::max(panes == 2 ? 2 : 1, int(portAvailable / p.portCell));
  p.horizontalFits =
      (!params || available >= p.paramCell) &&
      (!panes || portAvailable >= p.portCell * (panes == 2 ? 2 : 1));
  columns =
      std::min(columns, std::max(inputs && outputs ? 2 : 1, inputs + outputs));
  if (inputs && outputs) {
    int bestRows = inputs + outputs + 1;
    for (int c = 1; c < columns; ++c) {
      int rows =
          std::max(layoutRows(inputs, c), layoutRows(outputs, columns - c));
      if (rows < bestRows ||
          (rows == bestRows &&
           std::abs(float(c) / columns - float(inputs) / (inputs + outputs)) <
               std::abs(float(p.inputColumns) / columns -
                        float(inputs) / (inputs + outputs)))) {
        bestRows = rows;
        p.inputColumns = c;
        p.outputColumns = columns - c;
      }
    }
  } else {
    p.inputColumns = inputs ? columns : 0;
    p.outputColumns = outputs ? columns : 0;
  }
  int rows = std::max(layoutRows(inputs, std::max(1, p.inputColumns)),
                      layoutRows(outputs, std::max(1, p.outputColumns)));
  p.portHeight = rows ? header + rows * p.portRow + 6 : 0;
  return p;
}
inline float minimumLayoutWidth(int params, int inputs, int outputs) {
  for (int width = 75; width < 30000; width += 15)
    if (planLayout(width, 0, params, inputs, outputs).totalHeight() <= 368)
      return width;
  return 30000;
}
inline ResponsiveLayout responsiveLayout(float requested, int params,
                                         int inputs, int outputs) {
  float width = std::max(minimumLayoutWidth(params, inputs, outputs),
                         std::ceil(requested / 15) * 15);
  int tier = width >= 540 ? 3 : width >= 300 ? 2 : width >= 180 ? 1 : 0;
  auto p = planLayout(width, tier, params, inputs, outputs);
  while (tier > 0 && p.totalHeight() > 368)
    p = planLayout(width, --tier, params, inputs, outputs);
  return p;
}
}  // namespace peekaboo
