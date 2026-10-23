#pragma once
inline void checkBlunchGates(rack::plugin::Model* model) {
  using B=ComputerscareBlunch;
  auto require=[](bool ok,const char* why){if(!ok)throw std::runtime_error(why);};
  rack::engine::Module::ProcessArgs args;args.sampleRate=48000;args.sampleTime=1.f/48000;args.frame=0;
  auto make=[&](std::string formula,float percent=50.f){
    auto m=std::unique_ptr<B>(static_cast<B*>(model->createModule()));m->runningLineTexts[0].clear();m->sequenceEditorStates[0].text=formula;
    require(m->commitLine(0,true),"Gate fixture compiles");m->params[B::POLY_CHANNELS_PARAM].setValue(1);m->params[B::CLOCK_OUTPUT_MODE_PARAM].setValue(1);m->params[B::GATE_LENGTH_PARAM].setValue(percent);return m;
  };
  auto tick=[&](B& m){++args.frame;m.process(args);return m.outputs[B::CLOCK_OUTPUT].getVoltage()>5;};
  for(float speed : {0.f,1.f}) {
    auto m=make("120bpm");m->params[B::SPEED_OF_TIME_PARAM].setValue(speed);
    int high=0,complete=0;bool previous=false;
    for(int i=0;i<60000;++i){bool now=tick(*m);if(now)++high;else if(previous){require(std::abs(high-int(12000/std::pow(2.f,speed)))<=2,"Gate percentage follows resolved duration and Speed of time");++complete;high=0;}previous=now;}
    require(complete>=2,"Timed gate fixture emitted multiple gates");
  }
  auto live=make("120bpm");
  live->params[B::LIVE_APPLY_PARAM].setValue(1);
  live->sequenceEditorStates[0].text="90bpm";
  live->inspectFocusedLine(0,false);
  require(live->runningLineTexts[0][0]=="90bpm","Valid typing applies in immediate mode");
  live->sequenceEditorStates[0].text="{";
  live->inspectFocusedLine(0,false);
  require(live->runningLineTexts[0][0]=="90bpm","Invalid typing preserves running formula");
  auto cv=make("120bpm",0);
  cv->inputs[B::GATE_LENGTH_CV_INPUT].channels=1;
  cv->inputs[B::GATE_LENGTH_CV_INPUT].setVoltage(5.f);
  int cvHigh=0;bool cvWas=false;bool cvDone=false;
  for(int i=0;i<30000;++i){bool on=tick(*cv);if(on)++cvHigh;else if(cvWas){require(std::abs(cvHigh-12000)<=2,"Gate length CV adds to knob");cvDone=true;break;}cvWas=on;}
  require(cvDone,"Gate CV emitted a measured gate");
  auto random=make("{100-200}ms");random->params[B::SEED_PARAM].setValue(1);
  int high=0,expected=0,complete=0;bool previous=false;
  for(int i=0;i<96000;++i){bool now=tick(*random);if(now&&!previous)expected=int(24000/random->sequencers[0].activeClockSpec.hz);if(now)++high;else if(previous){require(std::abs(high-expected)<=2,"Random duration determines its own gate length");++complete;high=0;}previous=now;}
  require(complete>5,"Random gates exercised several selected intervals");
  auto muted=make("120bpm",0);for(int i=0;i<30000;++i)require(!tick(*muted),"Zero percent produces no output gate");
  auto tied=make("120bpm",100);int lows=0;for(int i=0;i<60000;++i){bool high=tick(*tied);if(i>100&&!high)++lows;}require(lows<=2,"Full length gates join across consecutive steps");
  tied->sequencers[0].running=false;require(!tick(*tied),"Stop immediately ends a long gate");
  auto rest=make("~120bpm",100);for(int i=0;i<30000;++i)require(!tick(*rest),"Rests do not emit gates");
  auto trigger=make("120bpm");trigger->params[B::CLOCK_OUTPUT_MODE_PARAM].setValue(0);high=0;previous=false;complete=0;
  for(int i=0;i<30000;++i){bool now=tick(*trigger);if(now)++high;else if(previous){require(std::abs(high-48)<=1,"Trigger mode retains one millisecond pulses");high=0;++complete;}previous=now;}require(complete>0,"Trigger fixture emitted a pulse");
  auto ext=make("w");ext->inputs[B::EXTERNAL_CLOCK_W_INPUT].channels=1;
  for(int i=0;i<10000;++i){ext->inputs[B::EXTERNAL_CLOCK_W_INPUT].setVoltage(i%4800<48?10:0);bool now=tick(*ext);if(i==6000)require(now,"External gate remains high after its incoming trigger falls");if(i==7500)require(!now,"External gate ends at half the measured interval");}
  auto poly=make("120bpm");poly->params[B::POLY_CHANNELS_PARAM].setValue(2);poly->sequenceEditorStates[1].text="240bpm";require(poly->commitLineForChannel(1,0,true),"Poly gate fixture compiles");
  int widths[2]={},done[2]={};bool was[2]={};
  for(int i=0;i<36000;++i){tick(*poly);for(int ch=0;ch<2;++ch){bool on=poly->outputs[B::CLOCK_OUTPUT].getVoltage(ch)>5;if(on)++widths[ch];else if(was[ch]){require(std::abs(widths[ch]-(ch?6000:12000))<=2,"Each polyphonic channel uses its own resolved interval");++done[ch];widths[ch]=0;}was[ch]=on;}}
  require(done[0]>0&&done[1]>1,"Independent polyphonic gates were exercised");
  auto four=make("181 122");four->params[B::POLY_CHANNELS_PARAM].setValue(4);four->params[B::SPEED_OF_TIME_PARAM].setValue(-.41119158f);
  const char* formulas[]={"181 122","77 82","66","55"};
  for(int ch=1;ch<4;++ch){four->sequenceEditorStates[ch].text=formulas[ch];require(four->commitLineForChannel(ch,0,true),"Four-channel pattern compiles");}
  int edges[4]={};bool previousHigh[4]={};
  for(int i=0;i<960000;++i){tick(*four);for(int ch=0;ch<4;++ch){bool on=four->outputs[B::CLOCK_OUTPUT].getVoltage(ch)>5;if(on&&!previousHigh[ch])++edges[ch];previousHigh[ch]=on;}}
  for(int ch=0;ch<4;++ch)require(edges[ch]>0,"All four multi-step-pattern channels emit gates independently");
  std::puts("Blunch gate duration, random choices, Speed of time, zero/full length, rests, stop, trigger compatibility and external interval checks passed");
}
