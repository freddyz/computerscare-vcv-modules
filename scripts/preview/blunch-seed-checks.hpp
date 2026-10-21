#pragma once

inline void checkBlunchSeedBoundaries(rack::plugin::Model* model) {
  using B = ComputerscareBlunch;
  auto require = [](bool ok, const char* why) {
    if (!ok) throw std::runtime_error(why);
  };
  auto make = [&](const std::string& formula, float seed) {
    auto m = std::unique_ptr<B>(static_cast<B*>(model->createModule()));
    m->runningLineTexts[0].clear();
    m->params[B::SEED_PARAM].setValue(seed);
    m->sequenceEditorStates[0].text = formula;
    if (!m->commitLine(0, true)) throw std::runtime_error("seed fixture failed to compile");
    return m;
  };
  auto timed = make("{80|160}@3s", 1.f);
  auto control = make("{80|160}@3s", 1.f);
  timed->params[B::SEED_PARAM].setValue(2.f);
  for (int beat = 0; beat < 8; ++beat) {
    timed->advanceActiveProgramBeat(timed->sequencers[0]);
    control->advanceActiveProgramBeat(control->sequencers[0]);
    require(timed->seedValue() == 1.f &&
                timed->sequencers[0].activeClockSpec.bpm == control->sequencers[0].activeClockSpec.bpm,
            "Seed knob changes do not alter an in-progress timed pattern");
  }
  timed->advanceActiveProgramDuration(timed->sequencers[0], 3.001f);
  auto nextSeed = make("{80|160}@3s", 2.f);
  require(timed->seedValue() == 2.f &&
              timed->sequencers[0].activeClockSpec.bpm == nextSeed->sequencers[0].activeClockSpec.bpm,
          "Timed pattern restart latches the latest seed and restarts its seeded sequence");
  auto counted = make("{80|160}@4", 1.f);
  counted->params[B::SEED_PARAM].setValue(2.f);
  for (int beat = 0; beat < 3; ++beat) {
    counted->advanceActiveProgramBeat(counted->sequencers[0]);
    require(counted->seedValue() == 1.f, "Counted pattern keeps its seed before repeat completion");
  }
  counted->advanceActiveProgramBeat(counted->sequencers[0]);
  require(counted->seedValue() == 2.f, "Counted pattern latches the seed at its repeat boundary");
  auto unseeded = make("{80|160}@3s", 0.f);
  unseeded->params[B::SEED_PARAM].setValue(3.f);
  require(!unseeded->seededRandomEnabled(), "Unseeded-to-seeded changes also wait for the boundary");
  unseeded->advanceActiveProgramDuration(unseeded->sequencers[0], 3.001f);
  require(unseeded->seedValue() == 3.f && unseeded->seededRandomEnabled(),
          "A previously unseeded pattern adopts a positive seed at restart");

  auto scoped = make("{120bpm|240#2}#4s", 1.f);
  bool held = false;
  for (int i = 1; i <= 32 && !held; ++i) {
    scoped->params[B::SEED_PARAM].setValue(i * .1f);
    scoped->commitLine(0, true);
    held = scoped->sequencers[0].nestedChoiceTicks == 2;
  }
  require(held, "Nested-scope fixture selects the two-pulse branch");
  auto& seq = scoped->sequencers[0];
  auto& step = seq.activeProgram[0];
  require(step.hasTotalDurationGroup && !step.totalDurationIsTickCount &&
              step.totalDurationSeconds == 4.f && seq.activeClockSpec.bpm == 240.,
          "Inner #2 does not replace the enclosing #4s timer");
  int serial = step.seededClockSerial;
  scoped->advanceActiveProgramBeat(seq);
  require(seq.nestedChoiceTicks == 1 && seq.activeClockSpec.bpm == 240. &&
              step.seededClockSerial == serial,
          "Selected 240#2 branch holds for its second pulse");
  scoped->advanceActiveProgramBeat(seq);
  require(step.seededClockSerial == serial + 1 && step.totalDurationSeconds == 4.f,
          "Completing inner #2 redraws the choice without restarting the outer timer");
  float oldSeed = scoped->seedValue();
  scoped->params[B::SEED_PARAM].setValue(9.f);
  require(!scoped->advanceActiveTotalDuration(seq, 3.9f) && scoped->seedValue() == oldSeed,
          "Nested branch does not shorten the outer four-second pattern or latch early");
  require(scoped->advanceActiveTotalDuration(seq, .101f) && scoped->seedValue() == 9.f,
          "Outer four-second completion is the seed boundary");
  auto plain = make("{80|160}#4s", 1.f);
  require(!plain->advanceActiveTotalDuration(plain->sequencers[0], 3.9f) &&
              plain->advanceActiveTotalDuration(plain->sequencers[0], .101f),
          "Both example formulas keep the same four-second total duration");

  auto poly = make("{80|160}@3s", 1.f);
  poly->runningLineTexts[1].clear();
  poly->sequenceEditorStates[1].text = "{80|160}@5s";
  poly->commitLineForChannel(1, 0, true);
  poly->inputs[B::SEED_CV_INPUT].channels = 1;
  poly->inputs[B::SEED_CV_INPUT].setVoltage(2.f);
  poly->advanceActiveProgramDuration(poly->sequencers[0], 3.001f);
  require(poly->sequencers[0].latchedSeed == 3.f && poly->sequencers[1].latchedSeed == 1.f,
          "Channels latch mono Seed CV independently at their own pattern boundaries");
  blunchProcessingChannel = 1;
  poly->advanceActiveProgramDuration(poly->sequencers[1], 5.001f);
  blunchProcessingChannel = -1;
  require(poly->sequencers[1].latchedSeed == 3.f, "Longer channel adopts Seed CV at its later restart");
  // Simulate the feedback path: the EOL edge reaches an external sequencer,
  // whose new CV arrives several audio samples after the boundary.
  auto feedback = make("{80|160}@1", 1.f);
  feedback->inputs[B::SEED_CV_INPUT].channels = 1;
  feedback->inputs[B::SEED_CV_INPUT].setVoltage(0.f);
  feedback->sequencers[0].running = true;
  feedback->allowSeedGrace = true;
  feedback->advanceActiveProgramBeat(feedback->sequencers[0]);
  feedback->allowSeedGrace = false;
  require(feedback->sequencers[0].seedGraceRemaining > 0.f &&
              feedback->sequencers[0].latchedSeed == 1.f &&
              feedback->sequencers[0].activeProgram[0].seededClockSerial == 0,
          "Boundary defers seed capture AND the first random draw");
  bool edges[4] = {};
  feedback->outputSampleTime = 1.f / 48000.f;
  for (int sample = 0; sample < 60; ++sample) {
    if (sample == 8) feedback->inputs[B::SEED_CV_INPUT].setVoltage(2.f);
    feedback->processSequencerChannel(0, feedback->outputSampleTime, edges,
                                     true, false, false, false, false, false);
  }
  require(feedback->sequencers[0].seedGraceRemaining == 0.f &&
              feedback->sequencers[0].latchedSeed == 3.f,
          "Post-EOL feedback CV is captured within the grace period");
  auto seedThree = make("{80|160}@1", 3.f);
  require(feedback->sequencers[0].activeClockSpec.bpm == seedThree->sequencers[0].activeClockSpec.bpm,
          "First random value uses the new feedback seed, not the previous seed");
  // Real sample-by-sample polyphony: Seed CV must not shorten the #4s
  // scope or accumulate one grace period per restart relative to a clock.
  auto aligned = make("{120|240#2}#4s", 1.f);
  aligned->sequenceEditorStates[1].text = "120bpm@8";
  aligned->sequenceEditorStates[2].text = "{80|160}#4s";
  aligned->commitLineForChannel(1, 0, true);
  aligned->commitLineForChannel(2, 0, true);
  aligned->inputs[B::SEED_CV_INPUT].channels = 1;
  aligned->outputSampleTime = 1.f / 48000.f;
  auto reference = make("120bpm@8", 1.f);
  reference->sequencers[0].running = true;
  reference->outputSampleTime = aligned->outputSampleTime;
  std::vector<int> referenceBoundaries, patchedBoundaries;
  bool referenceWasHigh = false;
  int boundaryCount[3] = {};
  bool wasHigh[3] = {};
  for (int ch = 0; ch < 3; ++ch) aligned->sequencers[ch].running = true;
  for (int sample = 1; sample <= 48000 * 24; ++sample) {
    reference->processSequencerChannel(0, reference->outputSampleTime, edges,
                                      true, false, false, false, false, false);
    bool referenceHigh = reference->lineCyclePulses[0].process(reference->outputSampleTime);
    if (referenceHigh && !referenceWasHigh) referenceBoundaries.push_back(sample);
    referenceWasHigh = referenceHigh;
    for (int ch = 0; ch < 3; ++ch) {
      aligned->processSequencerChannel(ch, aligned->outputSampleTime, edges,
                                       true, false, false, false, false, false);
      bool high = aligned->lineCyclePulses[ch].process(aligned->outputSampleTime);
      if (high && !wasHigh[ch]) {
        if (ch == 1) patchedBoundaries.push_back(sample);
        int expected = (boundaryCount[ch] + 1) * 192000;
        require(ch == 1 || std::abs(sample - expected) < 16,
                "Patched Seed CV preserves four-second polyphonic line boundaries without drift");
        ++boundaryCount[ch];
      }
      wasHigh[ch] = high;
    }
  }
  require(referenceBoundaries.size() == patchedBoundaries.size(),
          "Patched and unpatched counted clocks complete the same number of patterns");
  for (size_t i = 0; i < referenceBoundaries.size(); ++i)
    require(std::abs(referenceBoundaries[i] - patchedBoundaries[i]) < 16,
            "Seed grace does not add accumulated delay to counted patterns");
  require(boundaryCount[0] >= 5 && boundaryCount[1] >= 5 && boundaryCount[2] >= 5,
          "All three channels keep completing their full patterns");
  std::puts("Blunch seed-boundary and nested # scope checks passed.");
}
