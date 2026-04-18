#pragma once
#include <cmath>
// Mirror effect: reflection across an axis at `axisDeg` degrees from horizontal.
//   axisDeg = 0   → horizontal flip (left ↔ right)
//   axisDeg = 90  → vertical flip   (top ↔ bottom)
//   axisDeg = 45  → diagonal flip
// Uses the 2D reflection matrix: [[cos2θ, sin2θ], [sin2θ, -cos2θ]]
inline void applyFlip(NVGcontext* vg, float axisDeg) {
  float theta = axisDeg * (float)M_PI / 180.f;
  float c2 = cosf(2.f * theta);
  float s2 = sinf(2.f * theta);
  nvgTransform(vg, c2, s2, s2, -c2, 0.f, 0.f);
}
