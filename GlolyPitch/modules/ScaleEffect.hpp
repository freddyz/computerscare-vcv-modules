#pragma once
#include "rack.hpp"
using namespace rack;

// Ghost copy of each module scaled from its center.
// scale < 1 = shrunken ghost; scale > 1 = enlarged ghost bleeding into neighbors.
inline void drawScaleEffect(NVGcontext* vg, app::ModuleWidget* mw, float scale) {
  widget::FramebufferWidget* fbw =
      mw->getFirstDescendantOfType<widget::FramebufferWidget>();
  if (!fbw) return;
  int img = fbw->getImageHandle();
  if (img <= 0) return;

  math::Vec pos = mw->box.pos;
  math::Vec sz = mw->box.size;
  float hw = sz.x * 0.5f;
  float hh = sz.y * 0.5f;

  nvgSave(vg);
  nvgTranslate(vg, pos.x + hw, pos.y + hh);
  nvgScale(vg, scale, scale);
  NVGpaint p = nvgImagePattern(vg, -hw, -hh, sz.x, sz.y, 0.f, img, 0.68f);
  nvgBeginPath(vg);
  nvgRect(vg, -hw, -hh, sz.x, sz.y);
  nvgFillPaint(vg, p);
  nvgFill(vg);
  nvgRestore(vg);
}
