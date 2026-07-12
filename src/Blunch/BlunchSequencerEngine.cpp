#include "BlunchSequencerEngine.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>

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
  seq.activeProgramBeat = 0;
  seq.activeProgramIndex = step->totalDurationGroupEnd;
  seq.activeTotalDurationGroupId = -1;
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

  seq.activeProgramBeat = 0;
  seq.activeProgramIndex = step->totalDurationGroupEnd;
  seq.activeTotalDurationGroupId = -1;
  seq.activeTotalDurationElapsedSeconds = 0.0;
  seq.activeTotalDurationTicks = 0;
  return true;
}

bool getActiveRepeatProgressHighlight(const BlunchSequencerRuntime& seq,
                                      RepeatProgress& highlight) {
  if (!seq.running) {
    return false;
  }

  const BlunchProgramStep* step = activeStep(seq);
  if (!step) {
    return false;
  }

  if (step->hasTotalDurationGroup) {
    if (step->totalDurationHighlightEnd <= step->totalDurationHighlightBegin) {
      return false;
    }
    if (step->totalDurationIsTickCount && step->totalDurationTicks <= 1 &&
        step->totalDurationExternalClockInput >= 0) {
      return false;
    }

    highlight.begin = step->totalDurationHighlightBegin;
    highlight.end = step->totalDurationHighlightEnd;
    if (step->totalDurationIsTickCount) {
      highlight.segments = std::max(1, step->totalDurationTicks);
      highlight.progress = step->totalDurationTicks > 0
                               ? (float)(seq.activeTotalDurationTicks + 1) /
                                     step->totalDurationTicks
                               : 0.f;
    } else {
      highlight.segments = 0;
      highlight.progress = step->totalDurationSeconds > 0.f
                               ? seq.activeTotalDurationElapsedSeconds /
                                     step->totalDurationSeconds
                               : 0.f;
    }
    highlight.progress = std::max(0.f, std::min(highlight.progress, 1.f));
    return true;
  }

  if (step->repeatHighlightEnd <= step->repeatHighlightBegin) {
    return false;
  }
  if (!step->hasDuration && step->repeat <= 1 &&
      step->repeatExternalClockInput >= 0) {
    return false;
  }

  highlight.begin = step->repeatHighlightBegin;
  highlight.end = step->repeatHighlightEnd;
  if (step->hasDuration) {
    highlight.segments = 0;
    highlight.progress =
        step->durationSeconds > 0.f
            ? seq.activeProgramElapsedSeconds / step->durationSeconds
            : 0.f;
  } else {
    highlight.segments = std::max(1, step->repeat);
    highlight.progress = step->repeat > 0
                             ? (float)(seq.activeProgramBeat + 1) / step->repeat
                             : 0.f;
  }
  highlight.progress = std::max(0.f, std::min(highlight.progress, 1.f));
  return true;
}

}  // namespace sequencer
}  // namespace blunch
