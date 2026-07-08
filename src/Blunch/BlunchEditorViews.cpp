#include "BlunchEditorViews.hpp"

#include <algorithm>

std::string buildBlunchChannelsViewText(const BlunchSequencerRuntime* channels,
                                        int channelCount) {
  std::string text;
  for (int channel = 0; channel < channelCount; channel++) {
    if (channel > 0) {
      text += "\n";
    }
    if (channels) {
      text += channels[channel].activeLineText;
    }
  }
  return text;
}

int blunchChannelForChannelsViewLine(int line, int channelCount) {
  if (channelCount <= 0) {
    return 0;
  }
  return std::max(0, std::min(line, channelCount - 1));
}

BlunchEditorViewMode switchBlunchEditorView(BlunchEditorViewMode mode) {
  return mode == BlunchEditorViewMode::Sequence
             ? BlunchEditorViewMode::Channels
             : BlunchEditorViewMode::Sequence;
}
