#pragma once

#include <algorithm>
#include <array>
#include <cmath>

#include "../Computerscare.hpp"
#include "NudiebugState.hpp"

namespace nudiebug {

struct PlotDotsRenderer {
  static constexpr float kVoltageRange = 12.f;
  static constexpr float kDotRadius = 3.f;
  static constexpr float kDotPadding = kDotRadius + 1.f;
  static constexpr float kLineWidth = 2.f;

  std::array<math::Vec, kMaxChannels> previousPositions = {};
  std::array<bool, kMaxChannels> hasPreviousPosition = {};
  int lastPlotMode = PLOT_OFF;
  float lastWidth = 0.f;
  float lastHeight = 0.f;

  NVGcolor channelColor(int channel) const {
    return nvgHSLA(std::fmod(channel / 16.f + 0.03f, 1.f), 0.78f, 0.58f, 245);
  }

  math::Vec plotPosition(const Snapshot& snapshot, int channel, float width,
                         float height) const {
    const float plotWidth = std::max(1.f, width - kDotPadding * 2.f);
    const float plotHeight = std::max(1.f, height - kDotPadding * 2.f);
    const float x = kDotPadding + plotWidth * 0.5f +
                    clamp(snapshot.rectX[channel] / kVoltageRange, -1.f, 1.f) *
                        plotWidth * 0.5f;
    const float y = kDotPadding + plotHeight * 0.5f -
                    clamp(snapshot.rectY[channel] / kVoltageRange, -1.f, 1.f) *
                        plotHeight * 0.5f;
    return math::Vec(x, y);
  }

  void drawDot(NVGcontext* vg, math::Vec position, NVGcolor color) const {
    nvgBeginPath(vg);
    nvgCircle(vg, position.x, position.y, kDotRadius);
    nvgFillColor(vg, color);
    nvgFill(vg);
  }

  void resetHistory() { hasPreviousPosition.fill(false); }

  void draw(NVGcontext* vg, const Snapshot& snapshot, float width, float height,
            int plotMode) {
    if (plotMode == PLOT_OFF) {
      resetHistory();
      lastPlotMode = plotMode;
      return;
    }

    if (plotMode != lastPlotMode || width != lastWidth ||
        height != lastHeight) {
      resetHistory();
    }
    lastPlotMode = plotMode;
    lastWidth = width;
    lastHeight = height;

    const int channels = std::min(snapshot.compolyChannels, kMaxChannels);
    for (int c = 0; c < channels; c++) {
      const math::Vec position = plotPosition(snapshot, c, width, height);
      const NVGcolor color = channelColor(c);

      if (plotMode == PLOT_LINES && hasPreviousPosition[c]) {
        nvgBeginPath(vg);
        nvgMoveTo(vg, previousPositions[c].x, previousPositions[c].y);
        nvgLineTo(vg, position.x, position.y);
        nvgStrokeColor(vg, color);
        nvgStrokeWidth(vg, kLineWidth);
        nvgLineCap(vg, NVG_ROUND);
        nvgStroke(vg);
      } else if (plotMode != PLOT_LINES) {
        drawDot(vg, position, color);
      }

      previousPositions[c] = position;
      hasPreviousPosition[c] = true;
    }

    for (int c = channels; c < kMaxChannels; c++) {
      hasPreviousPosition[c] = false;
    }
  }
};

}  // namespace nudiebug
