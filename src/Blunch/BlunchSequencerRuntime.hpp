#pragma once

#include <string>
#include <utility>
#include <vector>

#include "../BlunchLanguage/BlunchLanguage.hpp"

struct BlunchProgramStep {
  blunch::language::ClockLiteralAst literal;
  blunch::language::ClockSpec spec;
  bool isRest = false;
  int externalClockInput = -1;
  int repeat = 1;
  bool repeatIsRandom = false;
  bool repeatRandomIsDuration = false;
  blunch::language::ClockLiteralAst repeatRandom;
  int repeatExternalClockInput = -1;
  bool hasDuration = false;
  float durationSeconds = 0.f;
  int probability = 100;
  int highlightBegin = 0;
  int highlightEnd = 0;
  int repeatHighlightBegin = 0;
  int repeatHighlightEnd = 0;
  bool repeatHighlightIsOwn = false;
  bool hasTotalDurationGroup = false;
  int totalDurationGroupId = -1;
  int totalDurationGroupStart = 0;
  int totalDurationGroupEnd = 0;
  bool totalDurationIsRandom = false;
  blunch::language::ClockLiteralAst totalDurationRandom;
  bool totalDurationIsTickCount = false;
  int totalDurationTicks = 0;
  int totalDurationExternalClockInput = -1;
  float totalDurationSeconds = 0.f;
  int totalDurationHighlightBegin = 0;
  int totalDurationHighlightEnd = 0;
  bool hasRandomValue = false;
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
  float activeProgramElapsedSeconds = 0.f;
  int activeTotalDurationGroupId = -1;
  float activeTotalDurationElapsedSeconds = 0.f;
  int activeTotalDurationTicks = 0;
  bool activeStepPlays = true;
  bool activeClockOutputHigh = false;
  bool running = true;

  void resetActiveProgramState(bool resetPhase) {
    activeProgramIndex = 0;
    activeProgramBeat = 0;
    activeProgramElapsedSeconds = 0.f;
    activeTotalDurationGroupId = -1;
    activeTotalDurationElapsedSeconds = 0.f;
    activeTotalDurationTicks = 0;
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
