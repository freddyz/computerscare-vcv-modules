#pragma once
#include "rack.hpp"
using namespace rack;

// Ghost copy of each module rotated around its center by angleDeg degrees.
inline void drawTiltEffect(NVGcontext* vg, app::ModuleWidget* mw, float angleDeg) {
  widget::FramebufferWidget* fbw =
      mw->getFirstDescendantOfType<widget::FramebufferWidget>();
  if (!fbw) return;
  int img = fbw->getImageHandle();
  if (img <= 0) return;

  math::Vec pos = mw->box.pos;
  math::Vec sz = mw->box.size;
  float hw = sz.x * 0.5f;
  float hh = sz.y * 0.5f;
  float angle = angleDeg * (float)M_PI / 180.f;

  nvgSave(vg);
  nvgTranslate(vg, pos.x + hw, pos.y + hh);
  nvgRotate(vg, angle);
  NVGpaint p = nvgImagePattern(vg, -hw, -hh, sz.x, sz.y, 0.f, img, 0.72f);
  nvgBeginPath(vg);
  nvgRect(vg, -hw, -hh, sz.x, sz.y);
  nvgFillPaint(vg, p);
  nvgFill(vg);
  nvgRestore(vg);
}
