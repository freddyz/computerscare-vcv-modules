// Deliberately use the internal Module header only in this compatibility
// translation unit. rack.hpp marks its PRIVATE getters as deprecated under
// Clang, but as hard compile errors under GCC (Linux and Windows builds).
// Do not disable PRIVATE checks in the rest of the plugin or mirror Module's
// internal memory layout. Recheck these three getters with Rack SDK updates.
#include "BeekapooCpuMeter.hpp"

#include <algorithm>
#include <cmath>
#include <engine/Module.hpp>

namespace beekapoo {
float moduleCpuSeconds(rack::engine::Module* module) {
  if (!module) return 0.f;
  const float* samples = module->meterBuffer();
  int length = module->meterLength(), index = module->meterIndex();
  if (!samples || index < 0 || index >= length) return 0.f;
  float seconds = samples[index];
  return std::isfinite(seconds) ? std::max(0.f, seconds) : 0.f;
}
}  // namespace beekapoo
