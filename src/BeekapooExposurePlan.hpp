#pragma once
#include <algorithm>
#include <random>

namespace beekapoo {
struct ExposureCounts {
  int inputs, parameters, outputs;
};
// Targets, not limits: existing boundary connections must always remain
// exposed.
inline ExposureCounts defaultExposureCounts(int modules, std::mt19937& rng) {
  if (modules <= 0) return {0, 0, 0};
  auto between = [&](int low, int high) {
    return std::uniform_int_distribution<int>(low, high)(rng);
  };
  return {between(2, std::min(8, 2 + (modules + 1) / 2)),
          between(3, std::min(16, 3 + modules)),
          between(1, std::min(6, (modules + 1) / 2))};
}
}  // namespace beekapoo
