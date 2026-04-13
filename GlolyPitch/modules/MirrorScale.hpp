#pragma once
// Mirror effect: uniform scale from center.
// scale < 1 = zoom out, scale > 1 = zoom in.
// Applied via nvgScale before drawing the image pattern.
inline void applyScale(NVGcontext* vg, float scale) {
  nvgScale(vg, scale, scale);
}
