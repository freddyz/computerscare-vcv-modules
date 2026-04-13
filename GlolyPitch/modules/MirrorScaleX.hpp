#pragma once
// Mirror effect: horizontal-only scale (stretch/squeeze X axis).
inline void applyScaleX(NVGcontext* vg, float scaleX) {
  nvgScale(vg, scaleX, 1.f);
}
