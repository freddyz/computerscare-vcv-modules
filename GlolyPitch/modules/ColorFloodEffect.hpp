#pragma once
#include "rack.hpp"
using namespace rack;

// Additive color wash over each module using NVG_LIGHTER blend mode.
// intensity 0-1: 0 = no effect, 1 = fully saturated teal flood.
inline void drawColorFloodEffect(NVGcontext* vg, app::ModuleWidget* mw,
                                 float intensity) {
  if (intensity <= 0.f) return;
  math::Vec pos = mw->box.pos;
  math::Vec sz = mw->box.size;

  nvgSave(vg);
  nvgGlobalCompositeOperation(vg, NVG_LIGHTER);
  nvgBeginPath(vg);
  nvgRect(vg, pos.x, pos.y, sz.x, sz.y);
  // Teal/green in the computerscare palette
  nvgFillColor(vg, nvgRGBAf(0.f, intensity * 0.55f, intensity * 0.38f,
                             intensity * 0.45f));
  nvgFill(vg);
  nvgRestore(vg);
}
