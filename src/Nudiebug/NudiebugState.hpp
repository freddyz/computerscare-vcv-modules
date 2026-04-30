#pragma once

#include <array>

namespace nudiebug {

constexpr int kMaxChannels = 16;

enum DisplayType { DISPLAY_TYPE_POLY, DISPLAY_TYPE_COMPOLY };

enum TextMode {
  TEXT_OFF,
  TEXT_POLY,
  TEXT_COMPOLY_RECT,
  TEXT_COMPOLY_POLAR,
  TEXT_LEFT = TEXT_POLY,
  TEXT_MIDDLE,
  TEXT_RIGHT
};

enum BarsMode { BARS_OFF, BARS_UNI_EDGE, BARS_UNI_MID, BARS_BIPOLAR };

constexpr int BARS_UNIPOLAR = BARS_UNI_EDGE;

enum PlotMode { PLOT_OFF, PLOT_DOTS };

enum CompolyRepresentation { COMPOLY_REP_RECT, COMPOLY_REP_POLAR };

enum ChannelLabelsMode {
  CHANNEL_LABELS_OFF,
  CHANNEL_LABELS_LEFT,
  CHANNEL_LABELS_RIGHT,
  CHANNEL_LABELS_BOTH
};

enum ChannelLayoutMode { CHANNEL_LAYOUT_ALL, CHANNEL_LAYOUT_STRETCH };

enum DisplayOrientation { DISPLAY_VERTICAL, DISPLAY_HORIZONTAL };

struct DisplayOptions {
  int displayType = DISPLAY_TYPE_POLY;
  bool textEnabled = true;
  int textMode = TEXT_POLY;
  int compolyRepresentation = COMPOLY_REP_RECT;
  bool visualizationEnabled = true;
  int visualizationMode = BARS_UNI_EDGE;
  bool barBackgroundEnabled = true;
  bool plotEnabled = false;
  int plotMode = PLOT_OFF;
  bool clearPlotPerFrame = true;
  bool channelLabelsEnabled = true;
  int channelLabelsMode = CHANNEL_LABELS_BOTH;
  int channelLayoutMode = CHANNEL_LAYOUT_ALL;
  int displayOrientation = DISPLAY_VERTICAL;
};

struct Snapshot {
  std::array<float, kMaxChannels> leftVoltages = {};
  std::array<float, kMaxChannels> rightVoltages = {};
  std::array<float, kMaxChannels> compolyA = {};
  std::array<float, kMaxChannels> compolyB = {};
  std::array<float, kMaxChannels> rectX = {};
  std::array<float, kMaxChannels> rectY = {};
  std::array<float, kMaxChannels> polarR = {};
  std::array<float, kMaxChannels> polarTheta = {};
  int leftChannels = 0;
  int rightChannels = 0;
  int compolyChannels = 0;

  int maxChannels() const { return kMaxChannels; }
};

}  // namespace nudiebug
