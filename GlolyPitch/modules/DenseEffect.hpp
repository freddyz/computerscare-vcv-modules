#pragma once
#include "rack.hpp"
using namespace rack;

// Dense smear: distributes many ghost copies in a circle around the module,
// creating a chromatic-smear / motion-blur / vibration look.
inline void drawDenseEffect(NVGcontext* vg, app::ModuleWidget* mw,
                            float copies) {
  widget::FramebufferWidget* fbw =
      mw->getFirstDescendantOfType<widget::FramebufferWidget>();
  if (!fbw) return;
  int img = fbw->getImageHandle();
  if (img <= 0) return;

  math::Vec pos = mw->box.pos;
  math::Vec sz = mw->box.size;
  int n = (int)std::max(3.f, std::round(copies));
  float spread = sz.x * 0.09f;
  float alpha = 0.45f / (float)n;

  for (int i = 0; i < n; i++) {
    float angle = (float)i / (float)n * 2.f * (float)M_PI;
    float dx = std::cos(angle) * spread;
    float dy = std::sin(angle) * spread;
    NVGpaint p =
        nvgImagePattern(vg, pos.x + dx, pos.y + dy, sz.x, sz.y, 0.f, img, alpha);
    nvgBeginPath(vg);
    nvgRect(vg, pos.x + dx, pos.y + dy, sz.x, sz.y);
    nvgFillPaint(vg, p);
    nvgFill(vg);
  }
}
