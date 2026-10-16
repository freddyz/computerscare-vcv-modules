#include "BlunchEditorViews.hpp"

#include <algorithm>
#include <vector>

std::string buildBlunchChannelsViewText(const BlunchSequencerRuntime* channels,
                                        int channelCount,
                                        const std::string* const* sourceTexts,
                                        const bool* playing,
                                        const int* cursors) {
  std::string text;
  for (int channel = 0; channel < channelCount; channel++) {
    if (channel > 0) {
      text += "\n";
    }
    if (channels) {
      text += sourceTexts ? blunchChannelsRowText(
                                channels[channel], *sourceTexts[channel],
                                cursors ? cursors[channel] : 0,
                                playing && playing[channel])
                          : channels[channel].activeLineText;
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

namespace {
std::vector<std::string> formulaLines(const std::string& text) {
  std::vector<std::string> result;
  size_t begin = 0;
  do {
    size_t end = text.find('\n', begin);
    result.push_back(
        text.substr(begin, end == std::string::npos ? end : end - begin));
    if (end == std::string::npos) break;
    begin = end + 1;
  } while (true);
  return result;
}
bool blank(const std::string& line) {
  return line.find_first_not_of(" \t\r") == std::string::npos;
}
}  // namespace
int blunchFormulaLineCount(const std::string& text) {
  int count = 0;
  for (const auto& line : formulaLines(text))
    if (!blank(line)) ++count;
  return count;
}
bool blunchChannelsRowEditable(bool playing, const std::string& text) {
  return !playing || blunchFormulaLineCount(text) <= 1;
}
int blunchChannelsSourceLine(const std::string& text, int activeLine,
                             int cursor, bool playing) {
  auto lines = formulaLines(text);
  activeLine = std::max(0, std::min(activeLine, int(lines.size()) - 1));
  if (playing) return activeLine;
  cursor = std::max(0, std::min(cursor, int(text.size())));
  int selected = int(std::count(text.begin(), text.begin() + cursor, '\n'));
  if (!blank(lines[selected])) return selected;
  if (!blank(lines[activeLine])) return activeLine;
  for (size_t i = 0; i < lines.size(); ++i)
    if (!blank(lines[i])) return int(i);
  return selected;
}
std::string blunchChannelsRowText(const BlunchSequencerRuntime& sequencer,
                                  const std::string& text, int cursor,
                                  bool playing) {
  if (playing && blunchFormulaLineCount(text) > 1)
    return sequencer.activeLineText;
  return formulaLines(text)[blunchChannelsSourceLine(text, sequencer.activeLine,
                                                     cursor, playing)];
}
