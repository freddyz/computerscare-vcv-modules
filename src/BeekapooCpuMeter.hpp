#pragma once

namespace rack {
namespace engine {
struct Module;
}
}  // namespace rack

namespace beekapoo {
// Unsupported Rack 2 access is isolated in BeekapooCpuMeter.cpp. Public
// plugin translation units must not call the PRIVATE getters directly.
float moduleCpuSeconds(rack::engine::Module* module);
}  // namespace beekapoo
