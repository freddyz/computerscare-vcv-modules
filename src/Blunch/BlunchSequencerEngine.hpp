#pragma once

#include "BlunchSequencerRuntime.hpp"

namespace blunch {
namespace sequencer {

struct RepeatProgress {
  int begin = 0;
  int end = 0;
  float progress = 0.f;
  int segments = 0;
};

int activeExternalClockInput(const BlunchSequencerRuntime& seq);
int activeRepeatExternalClockInput(const BlunchSequencerRuntime& seq);
int activeTotalDurationExternalClockInput(const BlunchSequencerRuntime& seq);
bool activeStepUsesExternalClock(const BlunchSequencerRuntime& seq);

float seededRandomFloat(float seed, int channel, int line, int stepIndex,
                        int role, int eventIndex, int drawIndex);
void chooseStepPlayback(BlunchSequencerRuntime& seq, float randomValue);
bool nextClockGateHigh(BlunchSequencerRuntime& seq, bool usesExternalClock);

void repeatProgramStep(BlunchSequencerRuntime& seq);
bool advanceProgramDuration(BlunchSequencerRuntime& seq, float sampleTime);
bool advanceTotalDuration(BlunchSequencerRuntime& seq, float sampleTime);
bool advanceTotalTickCount(BlunchSequencerRuntime& seq);

bool getActiveRepeatProgressHighlight(const BlunchSequencerRuntime& seq,
                                      RepeatProgress& highlight);

}  // namespace sequencer
}  // namespace blunch
