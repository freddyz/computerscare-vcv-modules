#pragma once
#include "rack.hpp"
using namespace rack;

// Overexposure glow: redraws the module's own bitmap with NVG_LIGHTER (additive)
// blending, creating a bloom / overexposure / burned-in effect.
// intensity 0-1: 0 = subtle glow, 1 = fully blown-out.
inline void drawOpacityEffect(NVGcontext* vg, app::ModuleWidget* mw,
                              float intensity) {
  if (intensity <= 0.f) return;
  widget::FramebufferWidget* fbw =
      mw->getFirstDescendantOfType<widget::FramebufferWidget>();
  if (!fbw) return;
  int img = fbw->getImageHandle();
  if (img <= 0) return;

  math::Vec pos = mw->box.pos;
  math::Vec sz = mw->box.size;

  nvgSave(vg);
  nvgGlobalCompositeOperation(vg, NVG_LIGHTER);
  NVGpaint p =
      nvgImagePattern(vg, pos.x, pos.y, sz.x, sz.y, 0.f, img, intensity * 0.85f);
  nvgBeginPath(vg);
  nvgRect(vg, pos.x, pos.y, sz.x, sz.y);
  nvgFillPaint(vg, p);
  nvgFill(vg);
  nvgRestore(vg);
}
