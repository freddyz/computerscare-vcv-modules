#pragma once

#include <string>

#include "BlunchSequencerRuntime.hpp"

enum class BlunchEditorViewMode {
  Sequence,
  Channels,
};

std::string buildBlunchChannelsViewText(const BlunchSequencerRuntime* channels,
                                        int channelCount);
int blunchChannelForChannelsViewLine(int line, int channelCount);
BlunchEditorViewMode switchBlunchEditorView(BlunchEditorViewMode mode);
