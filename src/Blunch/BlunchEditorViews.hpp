#pragma once

#include <string>

#include "BlunchSequencerRuntime.hpp"

enum class BlunchEditorViewMode {
  Sequence,
  Channels,
};

std::string buildBlunchChannelsViewText(
    const BlunchSequencerRuntime* channels, int channelCount,
    const std::string* const* sourceTexts = nullptr,
    const bool* playing = nullptr, const int* cursors = nullptr);
int blunchChannelForChannelsViewLine(int line, int channelCount);
BlunchEditorViewMode switchBlunchEditorView(BlunchEditorViewMode mode);

constexpr float BLUNCH_FONT_HEIGHT_BASE = .069f;
int blunchFormulaLineCount(const std::string& text);
bool blunchChannelsRowEditable(bool playing, const std::string& text);
int blunchChannelsSourceLine(const std::string& text, int activeLine,
                             int cursor, bool playing);
std::string blunchChannelsRowText(const BlunchSequencerRuntime& sequencer,
                                  const std::string& text, int cursor,
                                  bool playing);
