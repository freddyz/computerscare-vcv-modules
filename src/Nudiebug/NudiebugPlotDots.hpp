#pragma once

#include <algorithm>
#include <cmath>

#include "../Computerscare.hpp"
#include "NudiebugState.hpp"

namespace nudiebug {

struct PlotDotsRenderer {
  static constexpr float kVoltageRange = 12.f;
  static constexpr float kDotRadius = 3.f;
  static constexpr float kDotPadding = kDotRadius + 1.f;

  void draw(NVGcontext* vg, const Snapshot& snapshot, float width,
            float height) {
    for (int c = 0; c < snapshot.compolyChannels && c < kMaxChannels; c++) {
      const float plotWidth = std::max(1.f, width - kDotPadding * 2.f);
      const float plotHeight = std::max(1.f, height - kDotPadding * 2.f);
      const float x = kDotPadding + plotWidth * 0.5f +
                      clamp(snapshot.rectX[c] / kVoltageRange, -1.f, 1.f) *
                          plotWidth * 0.5f;
      const float y = kDotPadding + plotHeight * 0.5f -
                      clamp(snapshot.rectY[c] / kVoltageRange, -1.f, 1.f) *
                          plotHeight * 0.5f;
      const NVGcolor color =
          nvgHSLA(std::fmod(c / 16.f + 0.03f, 1.f), 0.78f, 0.58f, 245);

      nvgBeginPath(vg);
      nvgCircle(vg, x, y, kDotRadius);
      nvgFillColor(vg, color);
      nvgFill(vg);
    }
  }
};

}  // namespace nudiebug
