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
  for(float seed : {0.f, .1f, 1.f, 2.f, 7.f, 19.f}) {
    auto countedChoices=make("{2|3|4#2|5#3}hz@9",seed);
    auto& runtime=countedChoices->sequencers[0];
    int wraps=0,transitions=0;
    for(int tick=0;tick<90 && wraps<3;++tick) {
      int before=runtime.activeProgramBeat;
      countedChoices->advanceActiveProgramBeat(runtime);
      int after=runtime.activeProgramBeat;
      if(after!=before) {++transitions;if(after==0)++wraps;}
      require(after>=0 && after<9,"Outer @9 counter stays within nine choices");
    }
    require(wraps==3 && transitions==27,"Nested #2/#3 choices finish without trapping the seeded @9 pattern");
  }
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

template<typename Capture>
inline void checkBlunchRandomModes(rack::plugin::Model* model,Capture capture) {
  using B=ComputerscareBlunch;
  auto require=[](bool ok,const char* message){if(!ok)throw std::runtime_error(message);};
  auto make=[&](const std::string& formula,float seed){
    auto m=std::unique_ptr<B>(static_cast<B*>(model->createModule()));
    m->params[B::SEED_PARAM].setValue(0.f);
    m->inputs[B::SEED_CV_INPUT].channels=1;m->inputs[B::SEED_CV_INPUT].setVoltage(seed);
    m->sequenceEditorStates[0].text=formula;
    require(m->commitLine(0,true),"Random-mode fixture compiles");return m;
  };
  for(float seed:{-3.f,-1.f,0.f,2.f}) {
    auto a=make("${80-160}bpm?50@{2|4}#4s",seed);
    auto b=make("${80-160}bpm?50@{2|4}#4s",seed);
    require(a->seededRandomEnabled(),"Always-seeded accepts signed and zero seeds");
    for(int event=0;event<24;++event)for(int role=0;role<4;++role)
      require(a->randomFloatForEvent(a->sequencers[0],role,event,0)==b->randomFloatForEvent(b->sequencers[0],role,event,0),"Always-seeded draws repeat for all random roles");
    for(int beat=0;beat<16;++beat) {
      require(a->sequencers[0].activeClockSpec.bpm==b->sequencers[0].activeClockSpec.bpm&&a->sequencers[0].activeStepPlays==b->sequencers[0].activeStepPlays,"Actual clocks and probability replay with signed seed");
      a->advanceActiveProgramBeat(a->sequencers[0]);b->advanceActiveProgramBeat(b->sequencers[0]);
    }
    auto unforced=make("120bpm?50",seed);
    require(unforced->seededRandomEnabled()==(seed>0),"Unprefixed probability preserves existing seed behavior");
    auto fresh=make("!{80-160}bpm?50",seed);
    require(!fresh->seededRandomEnabled(),"Always-random ignores positive and negative seed");
    bool changed=false;float first=fresh->randomFloatForEvent(fresh->sequencers[0],0,0,0);
    for(int i=0;i<16;++i)changed |= first!=fresh->randomFloatForEvent(fresh->sequencers[0],0,0,0);
    require(changed,"Always-random does not replay a fixed event draw");
  }
  auto latched=make("${80|160}bpm@3s",-1.f);
  latched->inputs[B::SEED_CV_INPUT].setVoltage(-2.f);
  latched->advanceActiveProgramBeat(latched->sequencers[0]);
  require(latched->seedValue()==-1.f,"Forced negative seed remains latched inside a pattern");
  latched->advanceActiveProgramDuration(latched->sequencers[0],3.001f);
  require(latched->seedValue()==-2.f,"Forced seeded pattern adopts new negative seed at its normal boundary");
  auto negativeA=make("${80-160}bpm",-1.f),negativeB=make("${80-160}bpm",-2.f),positive=make("${80-160}bpm",1.f);
  require(negativeA->randomFloatForEvent(negativeA->sequencers[0],0,0,0)!=negativeB->randomFloatForEvent(negativeB->sequencers[0],0,0,0)&&negativeA->randomFloatForEvent(negativeA->sequencers[0],0,0,0)!=positive->randomFloatForEvent(positive->sequencers[0],0,0,0),"Negative seeds retain their value and sign");
  require(negativeA->paramQuantities[B::SEED_PARAM]->getDisplayValueString()=="Random","Seed zero displays Random");
  negativeA->paramQuantities[B::SEED_PARAM]->setDisplayValueString("2.5");
  require(negativeA->params[B::SEED_PARAM].getValue()==2.5f,"Seed direct numeric entry remains supported");
  auto* m=static_cast<B*>(model->createModule());APP->engine->addModule(m);
  auto widget=std::unique_ptr<ComputerscareBlunchWidget>(static_cast<ComputerscareBlunchWidget*>(model->createModuleWidget(m)));
  m->sequenceEditorStates[1].text="1s? 2s?\n1s 2s??\n1s $2s??75\n1s !2s??25";
  m->focusedChannel=1;m->channelsCursorChannel=1;m->openFocusedChannelEditPage();
  m->sequenceEditorStates[1].dirty=true;m->editorViewDirty=true;widget->step();
  capture(widget.get());
  std::puts("Blunch forced random modes, signed seeds, legacy probability and Seed display checks passed");
}

inline void checkBlunchSkipProbability(rack::plugin::Model* model) {
  using B=ComputerscareBlunch;
  auto require=[](bool ok,const char* why){if(!ok)throw std::runtime_error(why);};
  auto make=[&](const std::string& formula){
    auto m=std::unique_ptr<B>(static_cast<B*>(model->createModule()));
    m->runningLineTexts[0].clear();m->runningLineUnits[0].clear();m->pendingFormulas[0]=B::PendingFormula{};
    m->sequenceEditorStates[0].text=formula;
    require(m->commitLine(0,true),"Skip fixture compiles");
    m->sequencers[0].running=true;return m;
  };
  for(const char* formula:{"1s 2s??0","1s !2s??0","1s $2s??0","1s 2s??0@4","1s 2s??0@4s","1s 2s??0#4s","1s {2s??0#4s}"}) {
    auto m=make(formula);m->advanceActiveProgramBeat(m->sequencers[0]);
    require(m->sequencers[0].activeProgramIndex==0&&m->sequencers[0].activeStepPlays,"Miss skips block and remaining repeats immediately");
  }
  auto hit=make("1s 2s??100");hit->advanceActiveProgramBeat(hit->sequencers[0]);
  require(hit->sequencers[0].activeProgramIndex==1,"Probability 100 retains the step");
  auto choices=make("1s {2s??0}");choices->advanceActiveProgramBeat(choices->sequencers[0]);
  require(choices->sequencers[0].activeProgramIndex==0,"Choice-local skip mode reaches playback");
  auto all=make("1ms??0 2ms??0");
  for(int i=0;i<50;++i){all->advanceActiveProgramBeat(all->sequencers[0]);require(!all->sequencers[0].activeStepPlays,"All-skipped pattern stays silent and traversal remains bounded");}
  auto times=[&](const std::string& formula){
    auto m=make(formula);m->outputSampleTime=.001f;bool edges[4]={};bool was=false;std::vector<int> ticks;
    for(int sample=0;sample<8000;++sample) {
      m->processSequencerChannel(0,.001f,edges,true,false,false,false,false,false);
      bool high=m->sequencers[0].clockHigh&&m->sequencers[0].activeStepPlays;
      if(high&&!was)ticks.push_back(sample);was=high;
    }
    return ticks;
  };
  auto skipped=times("1s 2s??0"),rested=times("1s 2s?0");
  require(skipped.size()>=7&&rested.size()>=2,"Both timing fixtures produce clocks");
  for(size_t i=1;i<skipped.size();++i)require(std::abs(skipped[i]-skipped[i-1]-1000)<10,"Skipped two-second block adds no time");
  for(size_t i=1;i<rested.size();++i)require(std::abs(rested[i]-rested[i-1]-3000)<10,"Single ? retains the full three-second period");
  std::puts("Blunch skip/rest timing, probability bounds, repeats, choices and all-skipped safety passed");
}
