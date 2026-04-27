#pragma once
#include <cmath>
// Mirror effect: rotation around the panel center.
// angleDeg: -180 to +180 degrees.
inline void applyRotation(NVGcontext* vg, float angleDeg) {
  nvgRotate(vg, angleDeg * (float)M_PI / 180.f);
}
