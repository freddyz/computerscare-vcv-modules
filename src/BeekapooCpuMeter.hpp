#pragma once

#include "Computerscare.hpp"

namespace beekapoo {
// Rack 2 exposes these getters but marks them PRIVATE/unsupported. Keep the
// dependency here; read the same latest sample that Rack's own meter uses.
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
inline float moduleCpuSeconds(engine::Module* module) {
  if (!module) return 0.f;
  const float* samples = module->meterBuffer();
  int length = module->meterLength(), index = module->meterIndex();
  if (!samples || index < 0 || index >= length) return 0.f;
  float seconds = samples[index];
  return std::isfinite(seconds) ? std::max(0.f, seconds) : 0.f;
}
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
}  // namespace beekapoo
