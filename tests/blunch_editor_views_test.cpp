#include "Blunch/BlunchEditorViews.hpp"

#include <cstdio>
#include <cmath>
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

  require(blunchChannelsRowEditable(false, "120bpm\n90bpm"),
          "paused multi-line channels can be edited in All view");
  require(blunchChannelsRowEditable(true, "120bpm\n\n"),
          "a running single formula remains editable despite trailing blank lines");
  require(!blunchChannelsRowEditable(true, "120bpm\n90bpm"),
          "running multi-line channels are read-only");
  require(blunchChannelsRowEditable(false, "") && blunchChannelsRowEditable(true, ""),
          "empty channels accept their first formula");
  require(blunchFormulaLineCount("\n  \n120bpm\n\t\n90bpm\n") == 2,
          "only nonblank formula lines count toward multi-line editing locks");
  BlunchSequencerRuntime idle;
  idle.activeLineText = "stale";
  idle.activeLine = 0;
  require(blunchChannelsRowText(idle, "133bpm", 0, false) == "133bpm",
          "inactive authored text appears without waiting for playback");
  require(blunchChannelsRowText(idle, "120bpm\n90bpm", 8, false) == "90bpm",
          "paused channels show the last selected authored line");
  require(blunchChannelsRowText(idle, "120bpm\n90bpm", 8, true) == "stale",
          "running multi-line channels display the active runtime version");
  require(blunchChannelsRowText(idle, "133bpm\n", 7, false) == "133bpm",
          "a trailing blank cursor line does not hide the sole formula");
  require(blunchChannelsRowText(idle, "133bpm", 0, true) == "133bpm",
          "running single-line edits remain visible before submit");
  BlunchSequencerRuntime sixteen[16];
  std::string sources[16];
  const std::string* pointers[16];
  bool playing[16] = {};
  int cursors[16] = {};
  for (int i = 0; i < 16; ++i) pointers[i] = &sources[i];
  sources[12] = "[300 400]ms";
  std::string overview = buildBlunchChannelsViewText(sixteen, 16, pointers, playing, cursors);
  require(overview == std::string(12, '\n') + "[300 400]ms\n\n\n",
          "channel 13 appears in All view while outside the playing polyphony");
  require(std::fabs(BLUNCH_FONT_HEIGHT_BASE - .069f) < 1e-6f,
          "zero font-height offset uses the requested baseline");
  std::puts("blunch editor views tests passed");
  return 0;
}
