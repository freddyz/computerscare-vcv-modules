#include "Blunch/BlunchSequencerEngine.hpp"

#include <cmath>
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

void requireNear(float actual, float expected, const char* message) {
  if (std::fabs(actual - expected) > 0.000001f) {
    std::fprintf(stderr, "FAIL: %s expected %.6f got %.6f\n", message,
                 expected, actual);
    std::exit(1);
  }
}

void requireNearDouble(double actual, double expected, const char* message) {
  if (std::fabs(actual - expected) > 0.000001) {
    std::fprintf(stderr, "FAIL: %s expected %.6f got %.6f\n", message,
                 expected, actual);
    std::exit(1);
  }
}

void requireDifferent(float left, float right, const char* message) {
  if (std::fabs(left - right) <= 0.000001f) {
    std::fprintf(stderr, "FAIL: %s expected different values got %.6f\n",
                 message, left);
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

  float seededA = engine::seededRandomFloat(1.0f, 2, 3, 4, 5, 6, 7);
  float seededB = engine::seededRandomFloat(1.0f, 2, 3, 4, 5, 6, 7);
  requireNear(seededA, seededB, "seeded random is stable");
  require(seededA >= 0.f && seededA < 1.f, "seeded random is in range");
  requireDifferent(seededA,
                   engine::seededRandomFloat(1.001f, 2, 3, 4, 5, 6, 7),
                   "small seed changes produce different patterns");
  requireDifferent(seededA,
                   engine::seededRandomFloat(1.0f, 2, 3, 4, 5, 6, 8),
                   "seeded random draw index changes output");

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
  requireNearDouble(duration.activeProgramElapsedSeconds, 0.0,
                    "exact duration boundary has no elapsed carry");

  BlunchSequencerRuntime durationCarry = runtimeWithSteps(1);
  durationCarry.activeProgram[0].hasDuration = true;
  durationCarry.activeProgram[0].durationSeconds = 0.5f;
  require(engine::advanceProgramDuration(durationCarry, 0.75f),
          "duration completes with oversized sample");
  requireNearDouble(durationCarry.activeProgramElapsedSeconds, 0.25,
                    "duration preserves elapsed carry at boundary");

  BlunchSequencerRuntime totalSeconds = runtimeWithSteps(2);
  totalSeconds.activeProgram[0].hasTotalDurationGroup = true;
  totalSeconds.activeProgram[0].totalDurationSeconds = 1.f;
  totalSeconds.activeProgram[0].totalDurationGroupEnd = 2;
  totalSeconds.activeTotalDurationBranchLocal = true;
  require(!engine::advanceTotalDuration(totalSeconds, 0.5f),
          "total duration waits before threshold");
  require(engine::advanceTotalDuration(totalSeconds, 0.5f),
          "total duration completes at threshold");
  require(totalSeconds.activeProgramIndex == 2,
          "total duration jumps to group end");
  require(totalSeconds.activeTotalDurationGroupId == -1,
          "total duration clears active group");
  require(!totalSeconds.activeTotalDurationBranchLocal,
          "total duration clears active branch-local group");
  requireNearDouble(totalSeconds.activeTotalDurationElapsedSeconds, 0.0,
                    "exact total duration boundary has no elapsed carry");

  BlunchSequencerRuntime totalSecondsCarry = runtimeWithSteps(2);
  totalSecondsCarry.activeProgram[0].hasTotalDurationGroup = true;
  totalSecondsCarry.activeProgram[0].totalDurationSeconds = 1.f;
  totalSecondsCarry.activeProgram[0].totalDurationGroupEnd = 2;
  require(engine::advanceTotalDuration(totalSecondsCarry, 1.25f),
          "total duration completes with oversized sample");
  requireNearDouble(totalSecondsCarry.activeTotalDurationElapsedSeconds, 0.25,
                    "total duration preserves elapsed carry at boundary");

  BlunchSequencerRuntime repeatedTotalSeconds = runtimeWithSteps(2);
  repeatedTotalSeconds.activeProgram[0].hasTotalDurationGroup = true;
  repeatedTotalSeconds.activeProgram[0].totalDurationSeconds = 1.f;
  repeatedTotalSeconds.activeProgram[0].totalDurationGroupStart = 0;
  repeatedTotalSeconds.activeProgram[0].totalDurationGroupEnd = 2;
  repeatedTotalSeconds.activeProgram[0].repeat = 2;
  require(engine::advanceTotalDuration(repeatedTotalSeconds, 1.f),
          "repeated total duration first cycle completes");
  require(repeatedTotalSeconds.activeProgramIndex == 0,
          "repeated total duration loops to group start");
  require(repeatedTotalSeconds.activeProgramBeat == 1,
          "repeated total duration increments cycle counter");
  require(engine::advanceTotalDuration(repeatedTotalSeconds, 1.f),
          "repeated total duration second cycle completes");
  require(repeatedTotalSeconds.activeProgramIndex == 2,
          "repeated total duration advances after final cycle");
  require(repeatedTotalSeconds.activeProgramBeat == 0,
          "repeated total duration clears cycle counter after final cycle");

  BlunchSequencerRuntime totalTicks = runtimeWithSteps(2);
  totalTicks.activeProgram[0].hasTotalDurationGroup = true;
  totalTicks.activeProgram[0].totalDurationIsTickCount = true;
  totalTicks.activeProgram[0].totalDurationTicks = 2;
  totalTicks.activeProgram[0].totalDurationGroupEnd = 2;
  totalTicks.activeTotalDurationBranchLocal = true;
  require(!engine::activeClockTickAdvancesStepRepeat(totalTicks),
          "clock tick does not advance repeat inside total duration");
  require(!engine::advanceTotalTickCount(totalTicks),
          "total tick count waits before threshold");
  require(engine::advanceTotalTickCount(totalTicks),
          "total tick count completes at threshold");
  require(totalTicks.activeProgramIndex == 2,
          "total tick count jumps to group end");
  require(!totalTicks.activeTotalDurationBranchLocal,
          "total tick count clears active branch-local group");

  BlunchSequencerRuntime repeatedTotalTicks = runtimeWithSteps(2);
  repeatedTotalTicks.activeProgram[0].hasTotalDurationGroup = true;
  repeatedTotalTicks.activeProgram[0].totalDurationIsTickCount = true;
  repeatedTotalTicks.activeProgram[0].totalDurationTicks = 2;
  repeatedTotalTicks.activeProgram[0].totalDurationGroupStart = 0;
  repeatedTotalTicks.activeProgram[0].totalDurationGroupEnd = 2;
  repeatedTotalTicks.activeProgram[0].repeat = 2;
  require(!engine::advanceTotalTickCount(repeatedTotalTicks),
          "repeated total tick count first tick waits");
  require(engine::advanceTotalTickCount(repeatedTotalTicks),
          "repeated total tick count first cycle completes");
  require(repeatedTotalTicks.activeProgramIndex == 0,
          "repeated total tick count loops to group start");
  require(repeatedTotalTicks.activeProgramBeat == 1,
          "repeated total tick count increments cycle counter");
  require(!engine::advanceTotalTickCount(repeatedTotalTicks),
          "repeated total tick count second cycle first tick waits");
  require(engine::advanceTotalTickCount(repeatedTotalTicks),
          "repeated total tick count second cycle completes");
  require(repeatedTotalTicks.activeProgramIndex == 2,
          "repeated total tick count advances after final cycle");
  require(repeatedTotalTicks.activeProgramBeat == 0,
          "repeated total tick count clears cycle counter");

  BlunchSequencerRuntime totalGroupStep = runtimeWithSteps(3);
  totalGroupStep.activeProgramIndex = 1;
  totalGroupStep.activeProgramBeat = 2;
  totalGroupStep.activeTotalDurationTicks = 5;
  for (int i = 0; i < 3; i++) {
    totalGroupStep.activeProgram[i].hasTotalDurationGroup = true;
    totalGroupStep.activeProgram[i].totalDurationGroupStart = 1;
    totalGroupStep.activeProgram[i].totalDurationGroupEnd = 3;
  }
  require(engine::advanceWithinTotalDurationGroup(totalGroupStep),
          "multi-step total duration advances within its group");
  require(totalGroupStep.activeProgramIndex == 2,
          "multi-step total duration moves to next child");
  require(totalGroupStep.activeProgramBeat == 2,
          "multi-step total duration preserves outer cycle counter");
  require(totalGroupStep.activeTotalDurationTicks == 5,
          "multi-step total duration preserves tick counter");
  require(engine::advanceWithinTotalDurationGroup(totalGroupStep),
          "multi-step total duration wraps within its group");
  require(totalGroupStep.activeProgramIndex == 1,
          "multi-step total duration wraps to group start");

  BlunchSequencerRuntime singleTotalGroupStep = runtimeWithSteps(1);
  singleTotalGroupStep.activeProgram[0].hasTotalDurationGroup = true;
  singleTotalGroupStep.activeProgram[0].totalDurationGroupStart = 0;
  singleTotalGroupStep.activeProgram[0].totalDurationGroupEnd = 1;
  require(!engine::advanceWithinTotalDurationGroup(singleTotalGroupStep),
          "single-step total duration does not advance within group");

  BlunchSequencerRuntime progress = runtimeWithSteps(1);
  progress.activeProgram[0].repeat = 4;
  progress.activeProgram[0].repeatHighlightBegin = 10;
  progress.activeProgram[0].repeatHighlightEnd = 12;
  progress.activeProgramBeat = 1;
  engine::TimingScopeProgress highlight;
  require(engine::activeClockTickAdvancesStepRepeat(progress),
          "clock tick advances ordinary step repeat");
  progress.activeProgram[0].highlightBegin = 14;
  progress.activeProgram[0].highlightEnd = 18;
  engine::syncActiveHighlightFromStep(progress);
  require(progress.activeHighlightBegin == 14 && progress.activeHighlightEnd == 18,
          "active highlight syncs from refreshed step");
  require(engine::getActiveRepeatProgressHighlight(progress, highlight),
          "repeat progress exists with repeat highlight");
  std::vector<engine::TimingScopeProgress> scopeHighlights =
      engine::getActiveTimingScopeProgressHighlights(progress);
  require(scopeHighlights.size() == 1,
          "repeat progress has one active timing scope");
  require(scopeHighlights[0].kind == engine::TimingScopeKind::StepRepeat,
          "repeat progress scope kind is step repeat");
  require(highlight.begin == 10 && highlight.end == 12,
          "repeat progress exposes source range");
  require(highlight.segments == 4, "repeat progress segments by repeat count");
  requireNear(highlight.progress, 0.5f,
              "repeat progress uses one-based current beat");
  progress.running = false;
  require(!engine::getActiveRepeatProgressHighlight(progress, highlight),
          "stopped runtime does not expose repeat progress");

  BlunchSequencerRuntime singleExternalRepeat = runtimeWithSteps(1);
  singleExternalRepeat.activeProgram[0].repeat = 1;
  singleExternalRepeat.activeProgram[0].repeatExternalClockInput = 1;
  singleExternalRepeat.activeProgram[0].repeatHighlightBegin = 10;
  singleExternalRepeat.activeProgram[0].repeatHighlightEnd = 12;
  require(!engine::getActiveRepeatProgressHighlight(singleExternalRepeat,
                                                    highlight),
          "single external repeat does not show repeat progress");

  BlunchSequencerRuntime multiExternalRepeat = runtimeWithSteps(1);
  multiExternalRepeat.activeProgram[0].repeat = 2;
  multiExternalRepeat.activeProgram[0].repeatExternalClockInput = 1;
  multiExternalRepeat.activeProgram[0].repeatHighlightBegin = 10;
  multiExternalRepeat.activeProgram[0].repeatHighlightEnd = 12;
  require(engine::getActiveRepeatProgressHighlight(multiExternalRepeat,
                                                   highlight),
          "multi external repeat shows repeat progress");

  BlunchSequencerRuntime singleExternalTotal = runtimeWithSteps(1);
  singleExternalTotal.activeProgram[0].hasTotalDurationGroup = true;
  singleExternalTotal.activeProgram[0].totalDurationIsTickCount = true;
  singleExternalTotal.activeProgram[0].totalDurationTicks = 1;
  singleExternalTotal.activeProgram[0].totalDurationExternalClockInput = 2;
  singleExternalTotal.activeProgram[0].totalDurationHighlightBegin = 20;
  singleExternalTotal.activeProgram[0].totalDurationHighlightEnd = 22;
  require(!engine::getActiveRepeatProgressHighlight(singleExternalTotal,
                                                    highlight),
          "single external total duration does not show repeat progress");

  BlunchSequencerRuntime multiExternalTotal = runtimeWithSteps(1);
  multiExternalTotal.activeProgram[0].hasTotalDurationGroup = true;
  multiExternalTotal.activeProgram[0].totalDurationIsTickCount = true;
  multiExternalTotal.activeProgram[0].totalDurationTicks = 2;
  multiExternalTotal.activeProgram[0].totalDurationExternalClockInput = 2;
  multiExternalTotal.activeProgram[0].totalDurationHighlightBegin = 20;
  multiExternalTotal.activeProgram[0].totalDurationHighlightEnd = 22;
  require(engine::getActiveRepeatProgressHighlight(multiExternalTotal,
                                                   highlight),
          "multi external total duration shows repeat progress");
  scopeHighlights =
      engine::getActiveTimingScopeProgressHighlights(multiExternalTotal);
  require(scopeHighlights.size() == 1,
          "external total duration has one active timing scope");
  require(scopeHighlights[0].kind == engine::TimingScopeKind::TotalDuration,
          "external total duration scope kind is total duration");

  BlunchSequencerRuntime repeatedTotalProgress = runtimeWithSteps(1);
  repeatedTotalProgress.activeProgram[0].hasTotalDurationGroup = true;
  repeatedTotalProgress.activeProgram[0].totalDurationIsTickCount = true;
  repeatedTotalProgress.activeProgram[0].totalDurationTicks = 8;
  repeatedTotalProgress.activeProgram[0].totalDurationHighlightBegin = 20;
  repeatedTotalProgress.activeProgram[0].totalDurationHighlightEnd = 22;
  repeatedTotalProgress.activeProgram[0].repeat = 3;
  repeatedTotalProgress.activeProgram[0].repeatHighlightBegin = 30;
  repeatedTotalProgress.activeProgram[0].repeatHighlightEnd = 31;
  repeatedTotalProgress.activeProgramBeat = 1;
  repeatedTotalProgress.activeTotalDurationTicks = 3;
  require(engine::getActiveRepeatProgressHighlight(repeatedTotalProgress,
                                                   highlight),
          "repeated total duration shows total-duration progress");
  scopeHighlights =
      engine::getActiveTimingScopeProgressHighlights(repeatedTotalProgress);
  require(scopeHighlights.size() == 2,
          "repeated total duration exposes both active timing scopes");
  require(scopeHighlights[0].kind == engine::TimingScopeKind::TotalDuration,
          "repeated total duration first scope is total duration");
  require(scopeHighlights[1].kind ==
              engine::TimingScopeKind::TotalDurationRepeat,
          "repeated total duration second scope is outer repeat");
  require(highlight.begin == 20 && highlight.end == 22,
          "repeated total duration highlights total-duration range");
  require(highlight.segments == 8,
          "repeated total duration progress segments by total tick count");
  requireNear(highlight.progress, 0.5f,
              "repeated total duration progress uses total tick counter");

  require(engine::getActiveTotalDurationRepeatProgressHighlight(
              repeatedTotalProgress, highlight),
          "repeated total duration also shows outer repeat progress");
  require(highlight.begin == 30 && highlight.end == 31,
          "repeated total duration repeat highlights repeat range");
  require(highlight.segments == 3,
          "repeated total duration repeat segments by repeat count");
  requireNear(highlight.progress, 2.f / 3.f,
              "repeated total duration repeat progress uses current cycle");

  repeatedTotalProgress.activeTotalDurationTicks = 4;
  require(engine::getActiveTotalDurationRepeatProgressHighlight(
              repeatedTotalProgress, highlight),
          "repeated total duration repeat progress remains available");
  requireNear(highlight.progress, 2.f / 3.f,
              "repeated total duration repeat progress ignores clock ticks");

  repeatedTotalProgress.activeProgramBeat = 0;
  require(engine::getActiveTotalDurationRepeatProgressHighlight(
              repeatedTotalProgress, highlight),
          "repeated total duration repeat progress exists on first cycle");
  requireNear(highlight.progress, 1.f / 3.f,
              "repeated total duration repeat progress starts at one");

  BlunchSequencerRuntime branchLocalTotalProgress = runtimeWithSteps(1);
  branchLocalTotalProgress.activeProgram[0].hasTotalDurationGroup = true;
  branchLocalTotalProgress.activeProgram[0].totalDurationBranchLocal = true;
  branchLocalTotalProgress.activeProgram[0].totalDurationIsTickCount = true;
  branchLocalTotalProgress.activeProgram[0].totalDurationTicks = 3;
  branchLocalTotalProgress.activeProgram[0].totalDurationHighlightBegin = 40;
  branchLocalTotalProgress.activeProgram[0].totalDurationHighlightEnd = 42;
  scopeHighlights =
      engine::getActiveTimingScopeProgressHighlights(branchLocalTotalProgress);
  require(scopeHighlights.size() == 1,
          "branch-local total duration has one active timing scope");
  require(scopeHighlights[0].kind ==
              engine::TimingScopeKind::BranchLocalTotalDuration,
          "branch-local total duration scope kind is explicit");
  require(scopeHighlights[0].segments == 3,
          "branch-local total duration preserves segment count");

  std::puts("blunch sequencer engine tests passed");
  return 0;
}
