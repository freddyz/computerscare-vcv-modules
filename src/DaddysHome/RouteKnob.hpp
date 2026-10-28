#pragma once
#include <cmath>

#include "../Computerscare.hpp"
namespace daddys_home {
// Small computerscare-style knob: an irregular face, a single rotating dot,
// and a stationary source code. No per-knob framebuffer or audio parameters.
inline void drawRouteKnob(NVGcontext* vg, Vec center, float diameter,
                          int source, bool active, unsigned seed) {
  float radius = diameter * .48f;
  nvgBeginPath(vg);
  for (int i = 0; i < 11; ++i) {
    float angle = float(i) * 2.f * float(M_PI) / 11.f;
    float r = radius * (.94f + .06f * std::sin(float(seed * 17 + i * 23)));
    Vec point = center.plus(Vec(std::sin(angle), -std::cos(angle)).mult(r));
    if (i == 0)
      nvgMoveTo(vg, point.x, point.y);
    else
      nvgLineTo(vg, point.x, point.y);
  }
  nvgClosePath(vg);
  nvgFillColor(vg, active ? nvgRGB(32, 91, 88) : nvgRGB(47, 72, 68));
  nvgFill(vg);
  float angle = (-.75f + float(source + 1) / 256.f * 1.5f) * float(M_PI);
  Vec dot =
      center.plus(Vec(std::sin(angle), -std::cos(angle)).mult(radius * .93f));
  nvgBeginPath(vg);
  nvgCircle(vg, dot.x, dot.y, .65f);
  nvgFillColor(vg, active ? COLOR_COMPUTERSCARE_GREEN : nvgRGB(123, 160, 148));
  nvgFill(vg);
}
}  // namespace daddys_home
