#pragma once
// Mirror effect: vertical-only scale (stretch/squeeze Y axis).
inline void applyScaleY(NVGcontext* vg, float scaleY) {
  nvgScale(vg, 1.f, scaleY);
}
