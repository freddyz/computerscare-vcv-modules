#include "Blunch/BlunchSequencerRuntime.hpp"

#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    std::exit(1);
  }
}

} // namespace

int main() {
  BlunchSequencerRuntime runtime;
  require(runtime.activeLine == 0, "runtime starts on line zero");
  require(runtime.activeHighlightBegin == 0,
          "runtime default highlight begins at zero");
  require(runtime.activeHighlightEnd == 6,
          "runtime default highlight matches initial token width");
  require(runtime.activeProgramIndex == 0,
          "runtime starts at first program step");
  require(runtime.activeTotalDurationGroupId == -1,
          "runtime starts outside total-duration group");
  require(!runtime.activeTotalDurationBranchLocal,
          "runtime starts outside branch-local total-duration group");
  require(runtime.activeStepPlays, "runtime starts with playable step");
  require(!runtime.activeClockOutputHigh, "runtime clock output starts low");
  require(runtime.running, "runtime starts armed to run");

  runtime.clockPhase = 0.75f;
  runtime.activeClockRamp = 0.5f;
  runtime.activeProgramIndex = 3;
  runtime.activeProgramBeat = 2;
  runtime.activeProgramElapsedSeconds = 1.25f;
  runtime.activeTotalDurationGroupId = 7;
  runtime.activeTotalDurationBranchLocal = true;
  runtime.activeTotalDurationElapsedSeconds = 3.5f;
  runtime.activeTotalDurationTicks = 9;
  runtime.activeStepPlays = false;
  runtime.resetActiveProgramState(false);
  require(runtime.activeProgramIndex == 0,
          "reset clears active program index");
  require(runtime.activeProgramBeat == 0, "reset clears active program beat");
  require(runtime.activeProgramElapsedSeconds == 0.f,
          "reset clears elapsed step duration");
  require(runtime.activeTotalDurationGroupId == -1,
          "reset leaves total-duration group");
  require(!runtime.activeTotalDurationBranchLocal,
          "reset clears branch-local total-duration group");
  require(runtime.activeTotalDurationElapsedSeconds == 0.f,
          "reset clears total-duration seconds");
  require(runtime.activeTotalDurationTicks == 0,
          "reset clears total-duration ticks");
  require(runtime.activeStepPlays, "reset restores playable step");
  require(runtime.clockPhase == 0.75f,
          "reset without phase reset preserves clock phase");
  require(runtime.activeClockRamp == 0.5f,
          "reset without phase reset preserves clock ramp");

  runtime.resetActiveProgramState(true);
  require(runtime.clockPhase == 0.f, "phase reset clears clock phase");
  require(runtime.activeClockRamp == 0.f, "phase reset clears clock ramp");

  runtime.running = true;
  runtime.clockHigh = true;
  runtime.activeClockOutputHigh = true;
  runtime.clockStartLowSamples = 2;
  runtime.clockStartHighPending = true;
  runtime.activeHighlightBegin = 3;
  runtime.activeHighlightEnd = 9;
  runtime.activeProgramIndex = 4;
  runtime.activeProgramBeat = 3;
  runtime.activeProgramElapsedSeconds = 1.f;
  runtime.stopPlayback();
  require(!runtime.running, "stop clears running state");
  require(!runtime.clockHigh, "stop clears internal clock gate");
  require(!runtime.activeClockOutputHigh, "stop clears output gate state");
  require(runtime.clockStartLowSamples == 0,
          "stop clears pending low samples");
  require(!runtime.clockStartHighPending,
          "stop clears pending high edge");
  require(runtime.activeHighlightBegin == 0 && runtime.activeHighlightEnd == 0,
          "stop clears active highlight range");
  require(runtime.activeProgramIndex == 0,
          "stop resets active program index");
  require(runtime.activeProgramBeat == 0, "stop resets active program beat");
  require(runtime.activeProgramElapsedSeconds == 0.f,
          "stop resets active program elapsed time");

  std::vector<BlunchProgramStep> program(2);
  program[0].repeat = 4;
  program[1].probability = 25;
  runtime.activeProgramIndex = 8;
  runtime.activeProgramBeat = 6;
  runtime.activeTotalDurationStepBeat = 5;
  runtime.loadActiveProgram(5, "135bpm", program, true);
  require(runtime.activeLine == 5, "load sets active line");
  require(runtime.activeLineText == "135bpm", "load sets active line text");
  require(runtime.activeProgram.size() == 2, "load stores program steps");
  require(runtime.activeProgram[0].repeat == 4,
          "load preserves first program step");
  require(runtime.activeProgram[1].probability == 25,
          "load preserves second program step");
  require(runtime.activeProgramIndex == 0,
          "load resets active program index");
  require(runtime.activeProgramBeat == 0, "load resets active program beat");
  require(runtime.activeTotalDurationStepBeat == 0,
          "load resets total-duration step beat");

  runtime.clockPhase = 0.33f;
  runtime.activeClockRamp = 0.66f;
  runtime.clockStartLowSamples = 0;
  runtime.clockStartHighPending = false;
  runtime.scheduleTokenStartTick();
  require(runtime.clockPhase == 0.f, "token start clears clock phase");
  require(runtime.activeClockRamp == 0.f, "token start clears clock ramp");
  require(runtime.clockStartLowSamples == 1,
          "token start schedules one low sample");
  require(runtime.clockStartHighPending,
          "token start schedules following high edge");

  std::puts("blunch sequencer runtime tests passed");
  return 0;
}
