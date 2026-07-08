#include "Blunch/BlunchSequencerEngine.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    std::exit(1);
  }
}

void requireNear(float actual, float expected, const char* message) {
  if (std::fabs(actual - expected) > 0.000001f) {
    std::fprintf(stderr, "FAIL: %s expected %.6f got %.6f\n", message,
                 expected, actual);
    std::exit(1);
  }
}

BlunchSequencerRuntime runtimeWithSteps(int count) {
  BlunchSequencerRuntime seq;
  seq.activeProgram.resize(count);
  return seq;
}

} // namespace

int main() {
  namespace engine = blunch::sequencer;

  BlunchSequencerRuntime empty;
  require(engine::activeExternalClockInput(empty) == -1,
          "empty runtime has no external clock");
  require(engine::activeRepeatExternalClockInput(empty) == -1,
          "empty runtime has no repeat clock");
  require(engine::activeTotalDurationExternalClockInput(empty) == -1,
          "empty runtime has no total-duration clock");

  BlunchSequencerRuntime clocks = runtimeWithSteps(1);
  clocks.activeProgram[0].externalClockInput = 2;
  clocks.activeProgram[0].repeatExternalClockInput = 1;
  clocks.activeProgram[0].hasTotalDurationGroup = true;
  clocks.activeProgram[0].totalDurationIsTickCount = true;
  clocks.activeProgram[0].totalDurationExternalClockInput = 3;
  require(engine::activeExternalClockInput(clocks) == 2,
          "active external clock comes from active step");
  require(engine::activeRepeatExternalClockInput(clocks) == 1,
          "repeat external clock overrides active clock");
  require(engine::activeTotalDurationExternalClockInput(clocks) == 3,
          "total-duration tick clock comes from active step");
  clocks.activeProgram[0].repeatExternalClockInput = -1;
  require(engine::activeRepeatExternalClockInput(clocks) == 2,
          "repeat external clock falls back to active external clock");

  BlunchSequencerRuntime playback = runtimeWithSteps(1);
  playback.activeProgram[0].probability = 25;
  engine::chooseStepPlayback(playback, 0.24f);
  require(playback.activeStepPlays, "probability plays below threshold");
  engine::chooseStepPlayback(playback, 0.25f);
  require(!playback.activeStepPlays,
          "probability rests at threshold and above");
  playback.activeProgram[0].probability = 100;
  engine::chooseStepPlayback(playback, 0.99f);
  require(playback.activeStepPlays, "100 percent probability always plays");
  playback.activeProgram[0].isRest = true;
  engine::chooseStepPlayback(playback, 0.f);
  require(!playback.activeStepPlays, "rest step does not play");

  BlunchSequencerRuntime gate;
  gate.clockPhase = 0.25f;
  gate.clockStartLowSamples = 1;
  gate.clockStartHighPending = true;
  require(!engine::nextClockGateHigh(gate, false),
          "token start emits one low sample first");
  require(engine::nextClockGateHigh(gate, false),
          "token start goes high after low sample");
  require(gate.clockPhase == 0.f, "token start high resets phase");
  gate.clockPhase = 0.75f;
  require(!engine::nextClockGateHigh(gate, false),
          "gate low after half phase");
  gate.clockStartLowSamples = 1;
  gate.clockStartHighPending = true;
  require(!engine::nextClockGateHigh(gate, true),
          "external clock step never produces internal start gate");
  require(gate.clockStartLowSamples == 0,
          "external clock clears pending low sample");
  require(!gate.clockStartHighPending,
          "external clock clears pending high edge");

  BlunchSequencerRuntime duration = runtimeWithSteps(1);
  duration.activeProgram[0].hasDuration = true;
  duration.activeProgram[0].durationSeconds = 0.5f;
  require(!engine::advanceProgramDuration(duration, 0.25f),
          "duration does not complete before threshold");
  require(engine::advanceProgramDuration(duration, 0.25f),
          "duration completes at threshold");

  BlunchSequencerRuntime totalSeconds = runtimeWithSteps(2);
  totalSeconds.activeProgram[0].hasTotalDurationGroup = true;
  totalSeconds.activeProgram[0].totalDurationSeconds = 1.f;
  totalSeconds.activeProgram[0].totalDurationGroupEnd = 2;
  require(!engine::advanceTotalDuration(totalSeconds, 0.5f),
          "total duration waits before threshold");
  require(engine::advanceTotalDuration(totalSeconds, 0.5f),
          "total duration completes at threshold");
  require(totalSeconds.activeProgramIndex == 2,
          "total duration jumps to group end");
  require(totalSeconds.activeTotalDurationGroupId == -1,
          "total duration clears active group");

  BlunchSequencerRuntime totalTicks = runtimeWithSteps(2);
  totalTicks.activeProgram[0].hasTotalDurationGroup = true;
  totalTicks.activeProgram[0].totalDurationIsTickCount = true;
  totalTicks.activeProgram[0].totalDurationTicks = 2;
  totalTicks.activeProgram[0].totalDurationGroupEnd = 2;
  require(!engine::advanceTotalTickCount(totalTicks),
          "total tick count waits before threshold");
  require(engine::advanceTotalTickCount(totalTicks),
          "total tick count completes at threshold");
  require(totalTicks.activeProgramIndex == 2,
          "total tick count jumps to group end");

  BlunchSequencerRuntime progress = runtimeWithSteps(1);
  progress.activeProgram[0].repeat = 4;
  progress.activeProgram[0].repeatHighlightBegin = 10;
  progress.activeProgram[0].repeatHighlightEnd = 12;
  progress.activeProgramBeat = 1;
  engine::RepeatProgress highlight;
  require(engine::getActiveRepeatProgressHighlight(progress, highlight),
          "repeat progress exists with repeat highlight");
  require(highlight.begin == 10 && highlight.end == 12,
          "repeat progress exposes source range");
  require(highlight.segments == 4, "repeat progress segments by repeat count");
  requireNear(highlight.progress, 0.5f,
              "repeat progress uses one-based current beat");

  std::puts("blunch sequencer engine tests passed");
  return 0;
}
