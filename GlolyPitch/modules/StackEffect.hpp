#pragma once
#include "rack.hpp"
using namespace rack;

// Stack: draws 6 ghost copies of the module displaced diagonally,
// like a fanned / dropped deck of cards behind the original.
// offset: pixels per layer (1-15).
inline void drawStackEffect(NVGcontext* vg, app::ModuleWidget* mw,
                            float offset) {
  widget::FramebufferWidget* fbw =
      mw->getFirstDescendantOfType<widget::FramebufferWidget>();
  if (!fbw) return;
  int img = fbw->getImageHandle();
  if (img <= 0) return;

  math::Vec pos = mw->box.pos;
  math::Vec sz = mw->box.size;
  const int layers = 6;

  // Draw from back (most offset) to front (least offset)
  for (int i = layers; i >= 1; i--) {
    float dx = i * offset;
    float dy = i * offset * 0.55f;
    float alpha = 0.20f;
    NVGpaint p =
        nvgImagePattern(vg, pos.x + dx, pos.y + dy, sz.x, sz.y, 0.f, img, alpha);
    nvgBeginPath(vg);
    nvgRect(vg, pos.x + dx, pos.y + dy, sz.x, sz.y);
    nvgFillPaint(vg, p);
    nvgFill(vg);
  }
}
