#pragma once
#include "rack.hpp"
using namespace rack;

// Recursive zoom-feedback trail: draws N ghost copies of the module, each
// progressively zoomed outward and slightly rotated, creating a spiral
// feedback loop visual. Copies draw from outermost (most transparent) inward.
inline void drawEchoEffect(NVGcontext* vg, app::ModuleWidget* mw,
                           float iterations) {
  widget::FramebufferWidget* fbw =
      mw->getFirstDescendantOfType<widget::FramebufferWidget>();
  if (!fbw) return;
  int img = fbw->getImageHandle();
  if (img <= 0) return;

  math::Vec pos = mw->box.pos;
  math::Vec sz = mw->box.size;
  float hw = sz.x * 0.5f, hh = sz.y * 0.5f;
  int n = (int)std::max(2.f, std::round(iterations));

  // Draw outermost copy first (back) through to innermost (front)
  for (int i = n; i >= 1; i--) {
    float t = (float)i / (float)n;  // 1 = outermost, 0 = innermost
    float sc = 1.f + t * 0.55f;    // 1.55× at outermost → 1.0× at center
    float alpha = (1.f - t) * 0.6f + 0.05f;
    float angle = t * 0.22f;        // slight CCW twist on outer copies

    nvgSave(vg);
    nvgTranslate(vg, pos.x + hw, pos.y + hh);
    nvgScale(vg, sc, sc);
    nvgRotate(vg, angle);
    NVGpaint p =
        nvgImagePattern(vg, -hw, -hh, sz.x, sz.y, 0.f, img, alpha);
    nvgBeginPath(vg);
    nvgRect(vg, -hw, -hh, sz.x, sz.y);
    nvgFillPaint(vg, p);
    nvgFill(vg);
    nvgRestore(vg);
  }
}
