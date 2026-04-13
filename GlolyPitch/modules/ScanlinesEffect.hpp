#pragma once
#include "rack.hpp"
using namespace rack;

// Horizontal black scanlines drawn over each module.
// spacing: pixels between lines (2 = very dense, 20 = sparse).
inline void drawScanlinesEffect(NVGcontext* vg, app::ModuleWidget* mw,
                                float spacing) {
  math::Vec pos = mw->box.pos;
  math::Vec sz = mw->box.size;
  float sp = std::fmax(2.f, spacing);

  nvgSave(vg);
  nvgScissor(vg, pos.x, pos.y, sz.x, sz.y);
  nvgBeginPath(vg);
  for (float y = pos.y; y < pos.y + sz.y; y += sp) {
    nvgMoveTo(vg, pos.x, y);
    nvgLineTo(vg, pos.x + sz.x, y);
  }
  nvgStrokeColor(vg, nvgRGBAf(0.f, 0.f, 0.f, 0.55f));
  nvgStrokeWidth(vg, 1.0f);
  nvgStroke(vg);
  nvgResetScissor(vg);
  nvgRestore(vg);
}
