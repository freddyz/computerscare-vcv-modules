#pragma once

#include <cmath>

#include "../Computerscare.hpp"
#include "NudiebugState.hpp"

namespace nudiebug {

struct PlotDotsRenderer {
  static constexpr float kVoltageRange = 12.f;
  static constexpr float kDotRadius = 4.f;

  void draw(NVGcontext* vg, const Snapshot& snapshot, float width,
            float height) {
    for (int c = 0; c < snapshot.compolyChannels && c < kMaxChannels; c++) {
      const float x =
          width * 0.5f +
          clamp(snapshot.rectX[c] / kVoltageRange, -1.f, 1.f) * width * 0.5f;
      const float y =
          height * 0.5f -
          clamp(snapshot.rectY[c] / kVoltageRange, -1.f, 1.f) * height * 0.5f;

      nvgBeginPath(vg);
      nvgCircle(vg, x, y, kDotRadius);
      nvgFillColor(
          vg, nvgHSLA(std::fmod(c / 16.f + 0.03f, 1.f), 0.78f, 0.58f, 230));
      nvgFill(vg);
    }
  }
};

}  // namespace nudiebug
