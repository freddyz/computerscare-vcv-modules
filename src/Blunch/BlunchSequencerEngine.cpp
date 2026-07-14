#include "BlunchSequencerEngine.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

namespace blunch {
namespace sequencer {

static const BlunchProgramStep* activeStep(const BlunchSequencerRuntime& seq) {
  if (seq.activeProgram.empty() || seq.activeProgramIndex < 0 ||
      seq.activeProgramIndex >= (int)seq.activeProgram.size()) {
    return NULL;
  }
  return &seq.activeProgram[seq.activeProgramIndex];
}

static BlunchProgramStep* activeStep(BlunchSequencerRuntime& seq) {
  if (seq.activeProgram.empty() || seq.activeProgramIndex < 0 ||
      seq.activeProgramIndex >= (int)seq.activeProgram.size()) {
    return NULL;
  }
  return &seq.activeProgram[seq.activeProgramIndex];
}

int activeExternalClockInput(const BlunchSequencerRuntime& seq) {
  const BlunchProgramStep* step = activeStep(seq);
  return step ? step->externalClockInput : -1;
}

int activeRepeatExternalClockInput(const BlunchSequencerRuntime& seq) {
  const BlunchProgramStep* step = activeStep(seq);
  if (!step) {
    return -1;
  }
  if (step->repeatExternalClockInput >= 0) {
    return step->repeatExternalClockInput;
  }
  return step->externalClockInput;
}

int activeTotalDurationExternalClockInput(const BlunchSequencerRuntime& seq) {
  const BlunchProgramStep* step = activeStep(seq);
  if (!step || !step->hasTotalDurationGroup ||
      !step->totalDurationIsTickCount) {
    return -1;
  }
  return step->totalDurationExternalClockInput;
}

bool activeStepUsesExternalClock(const BlunchSequencerRuntime& seq) {
  return activeExternalClockInput(seq) >= 0;
}

bool activeClockTickAdvancesStepRepeat(const BlunchSequencerRuntime& seq) {
  const BlunchProgramStep* step = activeStep(seq);
  return step && !step->hasTotalDurationGroup;
}

void syncActiveHighlightFromStep(BlunchSequencerRuntime& seq) {
  const BlunchProgramStep* step = activeStep(seq);
  if (!step) {
    return;
  }

  seq.activeHighlightBegin = step->highlightBegin;
  seq.activeHighlightEnd = step->highlightEnd;
}

static uint32_t mixSeed(uint32_t hash, uint32_t value) {
  hash ^= value + 0x9e3779b9u + (hash << 6) + (hash >> 2);
  hash ^= hash >> 16;
  hash *= 0x7feb352du;
  hash ^= hash >> 15;
  hash *= 0x846ca68bu;
  hash ^= hash >> 16;
  return hash;
}

float seededRandomFloat(float seed, int channel, int line, int stepIndex,
                        int role, int eventIndex, int drawIndex) {
  uint32_t seedBits = 0;
  std::memcpy(&seedBits, &seed, sizeof(seedBits));
  uint32_t hash = 0x811c9dc5u;
  hash = mixSeed(hash, seedBits);
  hash = mixSeed(hash, (uint32_t)channel);
  hash = mixSeed(hash, (uint32_t)line);
  hash = mixSeed(hash, (uint32_t)stepIndex);
  hash = mixSeed(hash, (uint32_t)role);
  hash = mixSeed(hash, (uint32_t)eventIndex);
  hash = mixSeed(hash, (uint32_t)drawIndex);
  return (hash >> 8) * (1.f / 16777216.f);
}

void chooseStepPlayback(BlunchSequencerRuntime& seq, float randomValue) {
  const BlunchProgramStep* step = activeStep(seq);
  if (!step) {
    seq.activeStepPlays = true;
    return;
  }

  int probability = step->probability;
  if (step->isRest) {
    seq.activeStepPlays = false;
  } else if (probability >= 100) {
    seq.activeStepPlays = true;
  } else if (probability <= 0) {
    seq.activeStepPlays = false;
  } else {
    seq.activeStepPlays = randomValue * 100.f < probability;
  }
}

bool nextClockGateHigh(BlunchSequencerRuntime& seq, bool usesExternalClock) {
  if (usesExternalClock) {
    seq.clockStartLowSamples = 0;
    seq.clockStartHighPending = false;
    return false;
  }

  if (seq.clockStartLowSamples > 0) {
    seq.clockStartLowSamples--;
    return false;
  }

  if (seq.clockStartHighPending) {
    seq.clockStartHighPending = false;
    seq.clockPhase = 0.f;
    seq.activeClockRamp = 0.f;
  }

  return seq.clockPhase < 0.5f;
}

void repeatProgramStep(BlunchSequencerRuntime& seq) {
  seq.activeProgramBeat = 0;
  seq.activeProgramElapsedSeconds = 0.0;
}

bool advanceProgramDuration(BlunchSequencerRuntime& seq, float sampleTime) {
  const BlunchProgramStep* step = activeStep(seq);
  if (!step || !step->hasDuration) {
    return false;
  }

  seq.activeProgramElapsedSeconds += sampleTime;
  if (seq.activeProgramElapsedSeconds < step->durationSeconds) {
    return false;
  }

  seq.activeProgramElapsedSeconds -= step->durationSeconds;
  return true;
}

bool advanceTotalDuration(BlunchSequencerRuntime& seq, float sampleTime) {
  const BlunchProgramStep* step = activeStep(seq);
  if (!step || !step->hasTotalDurationGroup || step->totalDurationIsTickCount) {
    return false;
  }

  seq.activeTotalDurationElapsedSeconds += sampleTime;
  if (seq.activeTotalDurationElapsedSeconds < step->totalDurationSeconds) {
    return false;
  }

  seq.activeTotalDurationElapsedSeconds -= step->totalDurationSeconds;
  int nextBeat = seq.activeProgramBeat + 1;
  if (nextBeat < step->repeat) {
    seq.activeProgramBeat = nextBeat;
    seq.activeProgramIndex = step->totalDurationGroupStart;
  } else {
    seq.activeProgramBeat = 0;
    seq.activeProgramIndex = step->totalDurationGroupEnd;
  }
  seq.activeTotalDurationGroupId = -1;
  seq.activeTotalDurationBranchLocal = false;
  seq.activeTotalDurationTicks = 0;
  return true;
}

bool advanceTotalTickCount(BlunchSequencerRuntime& seq) {
  const BlunchProgramStep* step = activeStep(seq);
  if (!step || !step->hasTotalDurationGroup ||
      !step->totalDurationIsTickCount) {
    return false;
  }

  seq.activeTotalDurationTicks++;
  if (seq.activeTotalDurationTicks < step->totalDurationTicks) {
    return false;
  }

  int nextBeat = seq.activeProgramBeat + 1;
  if (nextBeat < step->repeat) {
    seq.activeProgramBeat = nextBeat;
    seq.activeProgramIndex = step->totalDurationGroupStart;
  } else {
    seq.activeProgramBeat = 0;
    seq.activeProgramIndex = step->totalDurationGroupEnd;
  }
  seq.activeTotalDurationGroupId = -1;
  seq.activeTotalDurationBranchLocal = false;
  seq.activeTotalDurationElapsedSeconds = 0.0;
  seq.activeTotalDurationTicks = 0;
  return true;
}

bool advanceWithinTotalDurationGroup(BlunchSequencerRuntime& seq) {
  const BlunchProgramStep* step = activeStep(seq);
  if (!step || !step->hasTotalDurationGroup) {
    return false;
  }

  int begin = step->totalDurationGroupStart;
  int end = step->totalDurationGroupEnd;
  if (end - begin <= 1) {
    return false;
  }

  seq.activeProgramIndex++;
  if (seq.activeProgramIndex >= end) {
    seq.activeProgramIndex = begin;
  }
  return true;
}

static TimingScopeProgress makeProgress(TimingScopeKind kind, int begin,
                                        int end, float progress, int segments) {
  TimingScopeProgress highlight;
  highlight.kind = kind;
  highlight.begin = begin;
  highlight.end = end;
  highlight.progress = std::max(0.f, std::min(progress, 1.f));
  highlight.segments = segments;
  return highlight;
}

std::vector<TimingScopeProgress> getActiveTimingScopeProgressHighlights(
    const BlunchSequencerRuntime& seq) {
  std::vector<TimingScopeProgress> highlights;
  if (!seq.running) {
    return highlights;
  }

  const BlunchProgramStep* step = activeStep(seq);
  if (!step) {
    return highlights;
  }

  if (step->hasTotalDurationGroup) {
    if (step->totalDurationHighlightEnd > step->totalDurationHighlightBegin &&
        !(step->totalDurationIsTickCount &&
          step->totalDurationExternalClockInput >= 0)) {
      TimingScopeKind kind = step->totalDurationBranchLocal
                                 ? TimingScopeKind::BranchLocalTotalDuration
                                 : TimingScopeKind::TotalDuration;
      if (step->totalDurationIsTickCount) {
        float progress = step->totalDurationTicks > 0
                             ? (float)(seq.activeTotalDurationTicks + 1) /
                                   step->totalDurationTicks
                             : 0.f;
        highlights.push_back(
            makeProgress(kind, step->totalDurationHighlightBegin,
                         step->totalDurationHighlightEnd, progress,
                         std::max(1, step->totalDurationTicks)));
      } else {
        float progress = step->totalDurationSeconds > 0.f
                             ? seq.activeTotalDurationElapsedSeconds /
                                   step->totalDurationSeconds
                             : 0.f;
        highlights.push_back(
            makeProgress(kind, step->totalDurationHighlightBegin,
                         step->totalDurationHighlightEnd, progress, 0));
      }
    }

    if (step->repeat > 1 &&
        step->repeatHighlightEnd > step->repeatHighlightBegin) {
      highlights.push_back(makeProgress(
          TimingScopeKind::TotalDurationRepeat, step->repeatHighlightBegin,
          step->repeatHighlightEnd,
          step->repeat > 0 ? (float)(seq.activeProgramBeat + 1) / step->repeat
                           : 0.f,
          std::max(1, step->repeat)));
    }

    return highlights;
  }

  if (step->repeatHighlightEnd <= step->repeatHighlightBegin) {
    return highlights;
  }
  if (!step->hasDuration && step->repeatExternalClockInput >= 0) {
    return highlights;
  }

  if (step->hasDuration) {
    highlights.push_back(makeProgress(
        TimingScopeKind::StepDuration, step->repeatHighlightBegin,
        step->repeatHighlightEnd,
        step->durationSeconds > 0.f
            ? seq.activeProgramElapsedSeconds / step->durationSeconds
            : 0.f,
        0));
  } else {
    highlights.push_back(makeProgress(
        TimingScopeKind::StepRepeat, step->repeatHighlightBegin,
        step->repeatHighlightEnd,
        step->repeat > 0 ? (float)(seq.activeProgramBeat + 1) / step->repeat
                         : 0.f,
        std::max(1, step->repeat)));
  }

  return highlights;
}

bool getActiveRepeatProgressHighlight(const BlunchSequencerRuntime& seq,
                                      RepeatProgress& highlight) {
  std::vector<TimingScopeProgress> highlights =
      getActiveTimingScopeProgressHighlights(seq);
  if (highlights.empty()) {
    return false;
  }
  highlight = highlights[0];
  return true;
}

bool getActiveTotalDurationRepeatProgressHighlight(
    const BlunchSequencerRuntime& seq, RepeatProgress& highlight) {
  std::vector<TimingScopeProgress> highlights =
      getActiveTimingScopeProgressHighlights(seq);
  for (size_t i = 0; i < highlights.size(); i++) {
    if (highlights[i].kind == TimingScopeKind::TotalDurationRepeat) {
      highlight = highlights[i];
      return true;
    }
  }
  return false;
}

}  // namespace sequencer
}  // namespace blunch
