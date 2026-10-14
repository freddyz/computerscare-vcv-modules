#include "Blunch/BlunchEditorViews.hpp"

#include <cstdio>
#include <cstdlib>

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    std::exit(1);
  }
}

} // namespace

int main() {
  BlunchSequencerRuntime channels[3];
  channels[0].activeLineText = "120bpm";
  channels[1].activeLineText = "33hz";

  require(buildBlunchChannelsViewText(channels, 3) ==
              "120bpm\n33hz\n",
          "channels view lists one active line per channel");
  channels[1].activeLineText = "48bpm";
  require(buildBlunchChannelsViewText(channels, 3) ==
              "120bpm\n48bpm\n",
          "channels view reflects each channel active line text");
  require(blunchChannelForChannelsViewLine(-1, 3) == 0,
          "negative channels view line clamps to first channel");
  require(blunchChannelForChannelsViewLine(2, 3) == 2,
          "channels view line maps directly to channel");
  require(blunchChannelForChannelsViewLine(99, 3) == 2,
          "large channels view line clamps to last channel");
  require(switchBlunchEditorView(BlunchEditorViewMode::Sequence) ==
              BlunchEditorViewMode::Channels,
          "tab from sequence view opens channels view");
  require(switchBlunchEditorView(BlunchEditorViewMode::Channels) ==
              BlunchEditorViewMode::Sequence,
          "tab from channels view opens sequence view");

  std::puts("blunch editor views tests passed");
  return 0;
}
