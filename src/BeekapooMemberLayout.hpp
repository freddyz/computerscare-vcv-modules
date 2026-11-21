#pragma once
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace beekapoo {
struct MemberCell {
  float x;
  int row;
};
// Preserve reading order, remove gaps, and balance consecutive rows. This
// only runs when membership changes, never while the user drags a module.
inline std::vector<MemberCell> compactMemberCells(
    const std::vector<float>& widths, float rowWidth = 1200.f) {
  const int n = int(widths.size());
  if (!n) return {};
  std::vector<double> prefix(n + 1);
  for (int i = 0; i < n; ++i) {
    prefix[i + 1] = prefix[i] + widths[i];
    rowWidth = std::max(rowWidth, widths[i]);
  }
  int rows = std::min(n, int(std::ceil(prefix[n] / rowWidth)));
  std::vector<std::vector<double>> cost(
      rows + 1,
      std::vector<double>(n + 1, std::numeric_limits<double>::infinity()));
  std::vector<std::vector<int>> cut(rows + 1, std::vector<int>(n + 1));
  cost[0][0] = 0;
  for (int r = 1; r <= rows; ++r)
    for (int end = r; end <= n; ++end)
      for (int start = r - 1; start < end; ++start) {
        double w = prefix[end] - prefix[start];
        double score = cost[r - 1][start] + w * w;
        if (score < cost[r][end]) {
          cost[r][end] = score;
          cut[r][end] = start;
        }
      }
  std::vector<MemberCell> result(n);
  for (int r = rows, end = n; r > 0; --r) {
    int start = cut[r][end];
    for (int i = start; i < end; ++i)
      result[i] = {float(prefix[i] - prefix[start]), r - 1};
    end = start;
  }
  return result;
}
}  // namespace beekapoo
