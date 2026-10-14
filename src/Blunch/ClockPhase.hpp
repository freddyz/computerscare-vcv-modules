#pragma once
#include <algorithm>
namespace computerscare {
namespace blunch {
struct ClockPhase {
  double elapsed = 0, period = 0;
  bool seen = false;
  void process(double dt, bool edge) {
    elapsed += dt;
    if (edge) {
      if (seen) period = elapsed;
      seen = true;
      elapsed = 0;
    }
  }
  float phase() const {
    return period > 0 ? float(std::min(1., elapsed / period)) : 0.f;
  }
};
}  // namespace blunch
}  // namespace computerscare
