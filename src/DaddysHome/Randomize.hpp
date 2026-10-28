#pragma once

#include <algorithm>
#include <cmath>

#include "Routing.hpp"

namespace daddys_home {
enum class RandomMode { Full, Shuffle, KeepJacks, KeepChannels };
struct RandomOptions {
  RandomMode mode = RandomMode::Full;
  bool connectedSources = false;
  bool unique = false;
  bool activeDestinations = false;
  bool allowBlank = false;
};
// Fixed-size working sets keep CV-triggered edits off the heap.
template <typename Pick>
Routing randomize(const Routing& before, const Filter& filter,
                  const Availability& available, const RandomOptions& options,
                  Pick pick, float probability = 1.f, bool wiggle = false) {
  Routing result = before;
  std::array<int, routes> targets{}, values{}, pool{};
  std::array<int, routes + 1> candidates{};
  int targetCount = 0, poolCount = 0;
  for (int dest = 0; dest < routes; ++dest)
    if (matches(filter, dest, before[dest]) &&
        (!options.activeDestinations || available.destination(dest)) &&
        (probability >= 1.f ||
         (probability > 0.f && pick(1000000) < probability * 1000000.f)))
      targets[targetCount++] = dest;
  if (options.mode == RandomMode::Shuffle) {
    for (int i = 0; i < targetCount; ++i) values[i] = before[targets[i]];
    if (wiggle) {
      for (int i = 0; i + 1 < targetCount; i += 2)
        std::swap(values[i], values[i + 1]);
    } else
      for (int i = targetCount - 1; i > 0; --i)
        std::swap(values[i], values[pick(i + 1)]);
    for (int i = 0; i < targetCount; ++i) result[targets[i]] = values[i];
    return result;
  }
  for (int source = 0; source < routes; ++source) {
    if (filter.inputPort >= 0 && source / ports != filter.inputPort) continue;
    if (filter.inputChannel >= 0 && source % ports != filter.inputChannel)
      continue;
    if (options.connectedSources && !available.source(source)) continue;
    pool[poolCount++] = source;
  }
  if (poolCount == 0 && !options.allowBlank) return result;
  std::array<bool, routes> used{};
  for (int i = targetCount - 1; i > 0; --i)
    std::swap(targets[i], targets[pick(i + 1)]);
  for (int i = 0; i < targetCount; ++i) {
    int dest = targets[i], old = before[dest], count = 0;
    if (old < 0 && (wiggle || options.mode != RandomMode::Full)) continue;
    for (int j = 0; j < poolCount; ++j) {
      int source = pool[j];
      if (options.unique && used[source]) continue;
      if (options.mode == RandomMode::KeepJacks &&
          source / ports != old / ports)
        continue;
      if (options.mode == RandomMode::KeepChannels &&
          source % ports != old % ports)
        continue;
      if (wiggle &&
          (std::abs(source / ports - old / ports) > 1 ||
           std::abs(source % ports - old % ports) > 1 || source == old))
        continue;
      candidates[count++] = source;
    }
    if (options.allowBlank && !wiggle) candidates[count++] = -1;
    if (count == 0) {
      if (!wiggle) result[dest] = -1;
      continue;
    }
    int source = candidates[pick(count)];
    result[dest] = source;
    if (source >= 0) used[source] = true;
  }
  return result;
}
}  // namespace daddys_home
