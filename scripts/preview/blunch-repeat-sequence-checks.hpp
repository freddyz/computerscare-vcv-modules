#pragma once
inline void checkBlunchRepeatSequences(rack::plugin::Model* model) {
  using B=ComputerscareBlunch;
  auto require=[](bool ok,const char* why){if(!ok)throw std::runtime_error(why);};
  auto make=[&](const std::string& formula,float seed=0.f) {
    auto m=std::unique_ptr<B>(static_cast<B*>(model->createModule()));
    m->runningLineTexts[0].clear();m->params[B::SEED_PARAM].setValue(seed);
    m->sequenceEditorStates[0].text=formula;
    require(m->commitLine(0,true),"Repeat sequence fixture compiles");return m;
  };
  for(float seed:{0.f,1.f,7.f})for(int lineMode:{0,1}) {
    auto m=make("120bpm@(2 4)",seed);m->params[B::AUTO_ADVANCE_PARAM].setValue(lineMode);
    auto& seq=m->sequencers[0];
    for(int count:{2,4,2,4}) {
      require(seq.activeProgram[0].repeat==count,"Repeat sequence alternates 2/4 across line cycles with or without a seed");
      auto& step=seq.activeProgram[0];
      require(seq.activeLineText.substr(step.repeatHighlightBegin,step.repeatHighlightEnd-step.repeatHighlightBegin)==std::to_string(count),"Repeat argument highlight follows the selected count");
      for(int beat=0;beat<count;++beat)m->advanceActiveProgramBeat(seq);
    }
    m->resetActiveBlock(true);require(seq.activeProgram[0].repeat==2,"Block reset restarts repeat argument sequence");
    m->advanceActiveProgramBeat(seq);m->advanceActiveProgramBeat(seq);
    require(seq.activeProgram[0].repeat==4,"Repeat sequence progresses after reset");
    m->resetActiveProgram(true);require(seq.activeProgram[0].repeat==2,"Line reset restarts repeat argument sequence");
  }
  auto brackets=make("[120 91 62]@[2 3]");
  for(int count:{2,3})for(int index=0;index<3;++index) {
    auto& seq=brackets->sequencers[0];
    require(seq.activeProgramIndex==index && seq.activeProgram[index].repeat==count,"Group repeats each clock twice on its first pass and three times on its next");
    for(int beat=0;beat<count;++beat)brackets->advanceActiveProgramBeat(seq);
  }
  brackets=make("[120 91 62]@[2 3]");
  auto parens=make("[120 91 62]@(2 3)");
  for(int tick=0;tick<60;++tick) {
    auto& a=brackets->sequencers[0];auto& b=parens->sequencers[0];
    require(a.activeProgramIndex==b.activeProgramIndex && a.activeProgramBeat==b.activeProgramBeat && a.activeProgram[a.activeProgramIndex].repeat==b.activeProgram[b.activeProgramIndex].repeat,"Bracket and parenthesis repeat sequences have identical playback");
    brackets->advanceActiveProgramBeat(a);parens->advanceActiveProgramBeat(b);
  }
  auto nested=make("[120 91 162]@(2 (3 4))");
  for(int count:{2,3,2,4,2})for(int index=0;index<3;++index) {
    auto& seq=nested->sequencers[0];
    require(seq.activeProgramIndex==index && seq.activeProgram[index].repeat==count,"Nested repeat arguments alternate 2,3,2,4 for every clock in the group");
    for(int beat=0;beat<count;++beat)nested->advanceActiveProgramBeat(seq);
  }
  auto page=make("120bpm@(2 4)\n240bpm");
  auto& seq=page->sequencers[0];
  page->advanceActiveProgramBeat(seq);page->advanceActiveProgramBeat(seq);
  require(seq.activeLine==1,"Page advances to the second line");
  page->advanceActiveProgramBeat(seq);
  require(seq.activeLine==0 && seq.activeProgram[0].repeat==4,"Repeat argument position survives visits to another line");
  page->resetActiveSequence(true);require(seq.activeProgram[0].repeat==2,"Page reset restarts repeat sequences");
  auto timed=make("120bpm@(.5s 1s)");
  require(std::abs(timed->sequencers[0].activeProgram[0].durationSeconds-.5f)<1e-6,"Timed repeat sequence starts at .5 seconds");
  timed->advanceActiveProgramDuration(timed->sequencers[0],.501f);
  require(std::abs(timed->sequencers[0].activeProgram[0].durationSeconds-1.f)<1e-6,"Timed repeat sequence advances to one second");
  std::puts("Blunch repeat count/duration sequence progression, seed independence, highlighting, page visits and explicit resets passed");
}
