#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

namespace beekapoo {
struct ModuleGroupCount {
  int params = 0, inputs = 0, outputs = 0;
};
struct ModuleGroupRect {
  float x, y, width, height;
};
// Ordered shelves of module blocks. Try larger controls first, then add columns
// as width permits. Short final rows share the available width.
inline std::vector<ModuleGroupRect> moduleGroupLayout(
    const std::vector<ModuleGroupCount>& groups, float width, float height) {
  if (groups.empty() || width < 24 || height < 24) return {};
  const float gap = 3;
  for (float unit = 60; unit >= 16; unit -= 2) {
    int bestColumns = 0;
    float bestRequired = INFINITY;
    std::vector<float> bestRows;
    for (int columns = 1;
         columns <=
         std::min(int(groups.size()), int((width + gap) / (unit + 12 + gap)));
         ++columns) {
      std::vector<float> rows;
      bool fits = true;
      for (int first = 0; first < int(groups.size()); first += columns) {
        int count = std::min(columns, int(groups.size()) - first);
        float innerWidth = (width - gap * (count - 1)) / count - 12;
        if (innerWidth < std::max(16.f, unit)) {
          fits = false;
          break;
        }
        int pcols = std::max(1, int(innerWidth / (unit + 2)));
        int jcols = std::max(1, int(innerWidth / 20));
        float row = 0;
        for (int i = first; i < first + count; ++i) {
          auto g = groups[i];
          int pr = (g.params + pcols - 1) / pcols;
          int jr = (g.inputs + g.outputs + jcols - 1) / jcols;
          row = std::max(row, 24.f + pr * (unit + (unit >= 24 ? 14 : 2)) +
                                  jr * (unit >= 24 ? 32 : 20));
        }
        rows.push_back(row);
      }
      if (!fits) continue;
      float required = gap * (rows.size() - 1);
      for (float row : rows) required += row;
      if (required <= height && required < bestRequired) {
        bestRequired = required;
        bestColumns = columns;
        bestRows = rows;
      }
    }
    if (!bestColumns) continue;
    std::vector<ModuleGroupRect> result;
    float y = 0;
    float extra = (height - bestRequired) / bestRows.size();
    for (int first = 0, row = 0; first < int(groups.size());
         first += bestColumns, ++row) {
      int count = std::min(bestColumns, int(groups.size()) - first);
      float w = (width - gap * (count - 1)) / count, h = bestRows[row] + extra;
      for (int col = 0; col < count; ++col)
        result.push_back({col * (w + gap), y, w, h});
      y += h + gap;
    }
    return result;
  }
  return {};
}
}  // namespace beekapoo
