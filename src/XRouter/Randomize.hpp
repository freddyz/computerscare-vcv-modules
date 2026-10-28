#pragma once

#include <algorithm>

#include "Routing.hpp"

namespace xrouter {
enum class RandomMode { Full, Shuffle, KeepJacks, KeepChannels };
struct RandomOptions {
  RandomMode mode = RandomMode::Full;
  bool connectedSources = false;
  bool unique = false;
  bool activeDestinations = false;
  bool allowBlank = false;
};
// UI-side only. Snapshot the target set before changing any source addresses.
template <typename Pick>
Routing randomize(const Routing& before, const Filter& filter,
                  const Availability& available, const RandomOptions& options,
                  Pick pick) {
  Routing result = before;
  std::vector<int> targets;
  for (int dest = 0; dest < routes; ++dest)
    if (matches(filter, dest, before[dest]) &&
        (!options.activeDestinations || available.destination(dest)))
      targets.push_back(dest);
  if (options.mode == RandomMode::Shuffle) {
    std::vector<int> values;
    for (int dest : targets) values.push_back(before[dest]);
    for (int i = int(values.size()) - 1; i > 0; --i)
      std::swap(values[i], values[pick(i + 1)]);
    for (size_t i = 0; i < targets.size(); ++i) result[targets[i]] = values[i];
    return result;
  }
  std::vector<int> pool;
  for (int source = 0; source < routes; ++source) {
    if (filter.inputPort >= 0 && source / ports != filter.inputPort) continue;
    if (filter.inputChannel >= 0 && source % ports != filter.inputChannel)
      continue;
    if (options.connectedSources && !available.source(source)) continue;
    pool.push_back(source);
  }
  if (pool.empty() && !options.allowBlank) return result;
  std::array<bool, routes> used{};
  // Randomize traversal too, so shortages do not always blank the last outputs.
  for (int i = int(targets.size()) - 1; i > 0; --i)
    std::swap(targets[i], targets[pick(i + 1)]);
  for (int dest : targets) {
    int old = before[dest];
    if (old < 0 && options.mode != RandomMode::Full) continue;
    std::vector<int> candidates;
    for (int source : pool) {
      if (options.unique && used[source]) continue;
      if (options.mode == RandomMode::KeepJacks &&
          source / ports != old / ports)
        continue;
      if (options.mode == RandomMode::KeepChannels &&
          source % ports != old % ports)
        continue;
      candidates.push_back(source);
    }
    if (options.allowBlank) candidates.push_back(-1);
    if (candidates.empty()) {
      result[dest] = -1;
      continue;
    }
    int source = candidates[pick(int(candidates.size()))];
    result[dest] = source;
    if (source >= 0) used[source] = true;
  }
  return result;
}
}  // namespace xrouter
