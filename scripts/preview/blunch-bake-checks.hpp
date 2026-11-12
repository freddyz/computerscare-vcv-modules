#pragma once
inline void checkBlunchTimeBake(rack::plugin::Model* model,const std::string& output,float scale) {
  using B=ComputerscareBlunch;
  auto require=[](bool ok,const char* why){if(!ok)throw std::runtime_error(why);};
  auto* m=static_cast<B*>(model->createModule());APP->engine->addModule(m);
  m->runningLineTexts[0].clear();m->runningLineTexts[1].clear();
  m->sequenceEditorStates[0].text="{100|200}bpm?75@3#8s\n[300 500]ms@(2s 4s)";
  m->sequenceEditorStates[1].text="4hz";
  m->commitLineForChannel(0,0,true);m->commitLineForChannel(1,0,true);
  m->params[B::POLY_CHANNELS_PARAM].setValue(2);m->params[B::SEED_PARAM].setValue(3);
  m->params[B::SPEED_OF_TIME_PARAM].setValue(1);
  m->inputs[B::TIME_CV_INPUT].channels=1;m->inputs[B::TIME_CV_INPUT].setVoltage(.5f);
  m->focusedChannel=0;m->params[B::WAIT_MODE_PARAM].setValue(B::WAIT_NEXT_VISIT);
  m->sequenceEditorStates[0].text="{100|200}bpm?75@3#8s\n[400 600]ms@(2s 4s)";
  require(m->executePage(),"Bake fixture stages Next");
  m->sequenceEditorStates[0].text="{100|200}bpm?75@3#8s\n[450 650]ms@(2s 4s)";
  rack::engine::Module::ProcessArgs args;args.sampleRate=48000;args.sampleTime=1.f/48000;args.frame=0;
  for(int i=0;i<137;++i){args.frame=i;m->process(args);}
  float phase=m->sequencers[0].clockPhase;double hz=m->sequencers[0].activeClockSpec.hz;
  double elapsed=m->sequencers[0].activeTotalDurationElapsedSeconds;
  bool chosen=m->sequencers[0].activeStepPlays;int serial=m->sequencers[0].activeProgram[0].seededClockSerial;
  auto before=std::unique_ptr<ComputerscareBlunchWidget>(static_cast<ComputerscareBlunchWidget*>(model->createModuleWidget(m)));
  m->openFocusedChannelEditPage();before->step();capture(before.get(),output+"/time-bake-before.png",scale);
  m->bakeTime();before->step();
  require(m->params[B::SPEED_OF_TIME_PARAM].getValue()==0 && m->inputs[B::TIME_CV_INPUT].getVoltage()==.5f,"Bake resets only the knob and ignores CV");
  require(m->sequenceEditorStates[0].text=="{200|400}bpm?75@3#4s\n[225 325]ms@(1s 2s)","Bake scales Draft timing without counts or probabilities");
  require(m->pendingFormulas[0].lines.at(1).text=="[200 300]ms@(1s 2s)" && m->runningLineTexts[0][1]=="[150 250]ms@(1s 2s)","Bake preserves separate Next and Now versions");
  require(m->runningLineTexts[1][0]=="8hz","Bake includes other channels");
  require(m->sequencers[0].clockPhase==phase && std::abs(m->sequencers[0].activeClockSpec.hz-hz*2)<1e-6 && std::abs(m->sequencers[0].activeTotalDurationElapsedSeconds-elapsed/2)<1e-9,"Bake preserves phase and elapsed timing progress");
  require(m->sequencers[0].activeStepPlays==chosen && m->sequencers[0].activeProgram[0].seededClockSerial==serial,"Bake preserves current random choices and counters");
  capture(before.get(),output+"/time-bake-after.png",scale);
  m->params[B::SPEED_OF_TIME_PARAM].setValue(1);
  m->sequenceEditorStates[1].text="{";
  std::string draft=m->sequenceEditorStates[0].text,now=m->runningLineTexts[0][0];
  bool rejected=false;try{m->bakeTime();}catch(const std::exception&){rejected=true;}
  require(rejected && m->sequenceEditorStates[0].text==draft && m->runningLineTexts[0][0]==now && m->params[B::SPEED_OF_TIME_PARAM].getValue()==1,"Invalid Draft makes baking atomic with no partial changes");
  auto baseline=std::unique_ptr<B>(static_cast<B*>(model->createModule()));
  auto baked=std::unique_ptr<B>(static_cast<B*>(model->createModule()));
  for(auto* test:{baseline.get(),baked.get()}) {
    test->runningLineTexts[0].clear();test->sequenceEditorStates[0].text="[120 180]bpm@2\n[300 400]ms@2";
    test->commitLineForChannel(0,0,true);test->params[B::POLY_CHANNELS_PARAM].setValue(1);test->params[B::SPEED_OF_TIME_PARAM].setValue(1);
    test->inputs[B::TIME_CV_INPUT].channels=1;test->inputs[B::TIME_CV_INPUT].setVoltage(.25f);
  }
  for(int i=0;i<211;++i){args.frame++;baseline->process(args);baked->process(args);}
  baked->bakeTime();
  int differences=0;
  for(int i=0;i<96000;++i){args.frame++;baseline->process(args);baked->process(args);differences+=(baseline->outputs[B::CLOCK_OUTPUT].getVoltage()>5)!=(baked->outputs[B::CLOCK_OUTPUT].getVoltage()>5);}
  require(differences<=4 && baseline->sequencers[0].activeLine==baked->sequencers[0].activeLine,"Baking preserves the actual clock output timeline with Time CV patched");
  std::puts("Blunch Time bake preserves layers, transport, random state, CV and atomic validation");
}
