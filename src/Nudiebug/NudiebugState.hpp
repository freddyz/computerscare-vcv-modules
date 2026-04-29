#pragma once

#include <array>

namespace nudiebug {

constexpr int kMaxChannels = 16;

enum TextMode {
  TEXT_OFF,
  TEXT_POLY,
  TEXT_COMPOLY_RECT,
  TEXT_COMPOLY_POLAR
};

enum BarsMode {
  BARS_OFF,
  BARS_UNIPOLAR,
  BARS_BIPOLAR
};

enum PlotMode {
  PLOT_OFF
};

struct DisplayOptions {
  bool textEnabled = true;
  int textMode = TEXT_POLY;
  bool visualizationEnabled = true;
  int visualizationMode = BARS_UNIPOLAR;
  bool plotEnabled = false;
  int plotMode = PLOT_OFF;
};

struct Snapshot {
  std::array<float, kMaxChannels> leftVoltages = {};
  std::array<float, kMaxChannels> rightVoltages = {};
  std::array<float, kMaxChannels> rectX = {};
  std::array<float, kMaxChannels> rectY = {};
  std::array<float, kMaxChannels> polarR = {};
  std::array<float, kMaxChannels> polarTheta = {};
  int leftChannels = 0;
  int rightChannels = 0;
  int compolyChannels = 0;

  int maxChannels() const {
    return kMaxChannels;
  }
};

}  // namespace nudiebug
