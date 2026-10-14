#pragma once

#include <string>
#include <utility>
#include <vector>

#include "../BlunchLanguage/BlunchLanguage.hpp"

struct BlunchProgramStep {
  blunch::language::ClockLiteralAst literal;
  blunch::language::ClockSpec spec;
  bool isRest = false;
  bool baseIsRest = false;
  int externalClockInput = -1;
  int repeat = 1;
  int baseRepeat = 1;
  bool repeatIsRandom = false;
  bool repeatIsSequence = false;
  bool repeatRandomIsDuration = false;
  blunch::language::ClockLiteralAst repeatRandom;
  std::vector<blunch::language::ClockLiteralAst> repeatSequence;
  int repeatExternalClockInput = -1;
  int baseRepeatExternalClockInput = -1;
  bool hasDuration = false;
  bool baseHasDuration = false;
  float durationSeconds = 0.f;
  float baseDurationSeconds = 0.f;
  int probability = 100;
  int baseProbability = 100;
  int highlightBegin = 0;
  int highlightEnd = 0;
  int repeatHighlightBegin = 0;
  int baseRepeatHighlightBegin = 0;
  int repeatHighlightEnd = 0;
  int baseRepeatHighlightEnd = 0;
  bool repeatHighlightIsOwn = false;
  bool baseRepeatHighlightIsOwn = false;
  bool hasTotalDurationGroup = false;
  bool baseHasTotalDurationGroup = false;
  bool totalDurationBranchLocal = false;
  bool baseTotalDurationBranchLocal = false;
  int totalDurationGroupId = -1;
  int baseTotalDurationGroupId = -1;
  int totalDurationGroupStart = 0;
  int baseTotalDurationGroupStart = 0;
  int totalDurationGroupEnd = 0;
  int baseTotalDurationGroupEnd = 0;
  bool totalDurationIsRandom = false;
  bool baseTotalDurationIsRandom = false;
  blunch::language::ClockLiteralAst totalDurationRandom;
  bool totalDurationIsTickCount = false;
  bool baseTotalDurationIsTickCount = false;
  int totalDurationTicks = 0;
  int baseTotalDurationTicks = 0;
  int totalDurationExternalClockInput = -1;
  int baseTotalDurationExternalClockInput = -1;
  float totalDurationSeconds = 0.f;
  float baseTotalDurationSeconds = 0.f;
  int totalDurationHighlightBegin = 0;
  int baseTotalDurationHighlightBegin = 0;
  int totalDurationHighlightEnd = 0;
  int baseTotalDurationHighlightEnd = 0;
  bool hasRandomValue = false;
  int seededClockSerial = 0;
  int seededProbabilitySerial = 0;
  int seededRepeatSerial = 0;
  int seededTotalDurationSerial = 0;
  int sourceLineBegin = 0;
};

struct BlunchSequencerRuntime {
  float clockPhase = 0.f;
  float activeClockRamp = 0.f;
  bool clockHigh = false;
  int clockStartLowSamples = 0;
  bool clockStartHighPending = false;
  int activeLine = 0;
  std::string activeLineText;
  int activeHighlightBegin = 0;
  int activeHighlightEnd = 6;
  blunch::language::ClockSpec activeClockSpec;
  std::vector<BlunchProgramStep> activeProgram;
  int activeProgramIndex = 0;
  int activeProgramBeat = 0;
  double activeProgramElapsedSeconds = 0.0;
  int activeTotalDurationGroupId = -1;
  bool activeTotalDurationBranchLocal = false;
  double activeTotalDurationElapsedSeconds = 0.0;
  int activeTotalDurationTicks = 0;
  int activeTotalDurationStepBeat = 0;
  bool activeStepPlays = true;
  bool activeClockOutputHigh = false;
  float activeClockDisplayPulse = 0.f;
  float activeExternalTimingDisplayPulse = 0.f;
  bool running = true;

  void stopPlayback() {
    running = false;
    clockHigh = false;
    activeClockOutputHigh = false;
    activeClockDisplayPulse = 0.f;
    activeExternalTimingDisplayPulse = 0.f;
    clockStartLowSamples = 0;
    clockStartHighPending = false;
    activeHighlightBegin = 0;
    activeHighlightEnd = 0;
    resetActiveProgramState(true);
  }

  void resetActiveProgramState(bool resetPhase) {
    activeProgramIndex = 0;
    activeProgramBeat = 0;
    activeProgramElapsedSeconds = 0.0;
    activeTotalDurationGroupId = -1;
    activeTotalDurationBranchLocal = false;
    activeTotalDurationElapsedSeconds = 0.0;
    activeTotalDurationTicks = 0;
    activeTotalDurationStepBeat = 0;
    activeStepPlays = true;
    if (resetPhase) {
      clockPhase = 0.f;
      activeClockRamp = 0.f;
    }
  }

  void loadActiveProgram(int line, std::string lineText,
                         std::vector<BlunchProgramStep> program,
                         bool resetPhase) {
    activeLine = line;
    activeLineText = std::move(lineText);
    activeProgram = std::move(program);
    resetActiveProgramState(resetPhase);
  }

  void scheduleTokenStartTick() {
    clockPhase = 0.f;
    activeClockRamp = 0.f;
    clockStartLowSamples = 1;
    clockStartHighPending = true;
  }
};
