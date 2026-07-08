#pragma once

#include <string>
#include <vector>

#include "Computerscare.hpp"

struct ComputerscareTextHighlight {
  int begin = 0;
  int end = 0;
  NVGcolor foreground = nvgRGB(0xd8, 0xf3, 0xec);
  NVGcolor background = nvgRGBA(0x00, 0x00, 0x00, 0x00);
  NVGcolor border = nvgRGBA(0x00, 0x00, 0x00, 0x00);
  NVGcolor progressColor = nvgRGBA(0x00, 0x00, 0x00, 0x00);
  bool hasForeground = false;
  bool hasBackground = true;
  bool hasBorder = false;
  bool hasProgress = false;
  bool fullLine = false;
  float progress = 0.f;
  int progressSegments = 0;
};

struct ComputerscareTextEditorCommands {
  int submitCount = 0;
  int cancelCount = 0;
  int openCount = 0;
  int stopCount = 0;
  int switchViewForwardCount = 0;
  int switchViewBackwardCount = 0;
  int navigateChannelForwardCount = 0;
  int navigateChannelBackwardCount = 0;

  int switchViewCount() const {
    return switchViewForwardCount + switchViewBackwardCount;
  }

  int navigateChannelCount() const {
    return navigateChannelForwardCount + navigateChannelBackwardCount;
  }
};

struct ComputerscareTextEditorState {
  std::string text;
  bool dirty = false;
  int cursor = 0;
  int selection = 0;
  std::vector<ComputerscareTextHighlight> highlights;
};

struct ComputerscareTextEditorStyle {
  std::string fontPath = "res/fonts/ShareTechMono-Regular.ttf";
  NVGcolor backgroundColor = nvgRGB(0x07, 0x09, 0x0a);
  NVGcolor borderColor = nvgRGB(0x24, 0xc9, 0xa6);
  NVGcolor textColor = nvgRGB(0xd8, 0xf3, 0xec);
  NVGcolor selectionColor = nvgRGBA(0x24, 0xc9, 0xa6, 0x85);
  NVGcolor placeholderColor = nvgRGBA(0xd8, 0xf3, 0xec, 0x61);
  float cornerRadius = 3.f;
  float fontSize = BND_LABEL_FONT_SIZE;
  float fontWidthOffset = 0.f;
  float fontHeightOffset = 0.f;
  float letterSpacing = 0.f;
  bool lineWrapping = true;
};

struct ComputerscareTextEditorSnapshot {
  std::string text;
  int cursor = 0;
  int selection = 0;
};
