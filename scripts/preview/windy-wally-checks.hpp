#pragma once
inline void checkWindyWally(rack::plugin::Model* model) {
  using P=ComputerscareWindyWally;
  auto stereo=std::unique_ptr<P>(static_cast<P*>(model->createModule()));
  auto mono=std::unique_ptr<P>(static_cast<P*>(model->createModule()));
  auto loud=std::unique_ptr<P>(static_cast<P*>(model->createModule()));
  for(auto* m:{stereo.get(),mono.get(),loud.get()}) {
    m->outputs[P::LEFT].channels=1; m->outputs[P::RIGHT].channels=1;
    m->params[P::STRENGTH].setValue(.8f);
  }
  mono->params[P::WIDTH].setValue(0.f);
  loud->params[P::OVERDRIVE].setValue(1.f);
  rack::engine::Module::ProcessArgs args;args.sampleRate=48000;args.sampleTime=1.f/48000;
  double wide=0,narrow=0,quietEnergy=0,characterChange=0;
  for(int i=0;i<144000;++i) {
    args.frame=i;
    stereo->process(args);mono->process(args);loud->process(args);
    if(stereo->outputs[P::WIND].getVoltage()!=mono->outputs[P::WIND].getVoltage() ||
       stereo->outputs[P::WIND].getVoltage()!=loud->outputs[P::WIND].getVoltage())
      throw std::runtime_error("Windy Wally audio controls changed wind CV");
    for(auto* m:{stereo.get(),mono.get(),loud.get()})
      for(int out=0;out<P::OUTPUTS;++out) {
        float value=m->outputs[out].getVoltage();
        if(!std::isfinite(value)||std::abs(value)>(out==P::WIND?10.f:5.f))
          throw std::runtime_error("Windy Wally output exceeded its range");
      }
    if(i>48000) {
      wide+=std::abs(stereo->outputs[P::LEFT].getVoltage()-stereo->outputs[P::RIGHT].getVoltage());
      narrow+=std::abs(mono->outputs[P::LEFT].getVoltage()-mono->outputs[P::RIGHT].getVoltage());
      quietEnergy+=std::abs(stereo->outputs[P::LEFT].getVoltage());
      characterChange+=std::abs(loud->outputs[P::LEFT].getVoltage()-stereo->outputs[P::LEFT].getVoltage());
    }
  }
  if(wide<=0||narrow>wide*.001||characterChange<=quietEnergy*.05)
    throw std::runtime_error("Windy Wally stereo width or overdrive did not respond");
  auto cv=std::unique_ptr<P>(static_cast<P*>(model->createModule()));
  cv->params[P::STRENGTH].setValue(.2f);cv->inputs[P::STRENGTH_CV].channels=1;
  cv->inputs[P::STRENGTH_CV].setVoltage(5.f);
  cv->process(args);
  if(std::abs(cv->strength-.7f)>1e-6)
    throw std::runtime_error("Windy Wally strength CV must add to its knob");
  cv->inputs[P::STRENGTH_CV].setVoltage(-10.f);cv->refresh=0;cv->process(args);
  if(cv->strength!=0) throw std::runtime_error("Windy Wally negative CV must clamp at zero");
  stereo->params[P::GUST].setValue(1.f);
  loud->inputs[P::GUST_INPUT].channels=1;loud->inputs[P::GUST_INPUT].setVoltage(10.f);
  for(int i=0;i<48000;++i) {
    args.frame=144000+i;stereo->process(args);loud->process(args);
    if(stereo->outputs[P::WIND].getVoltage()!=loud->outputs[P::WIND].getVoltage())
      throw std::runtime_error("Windy Wally button and trigger must make the same gust");
  }
  cv->inputs[P::OVERDRIVE_CV].channels=1;
  cv->inputs[P::OVERDRIVE_CV].setVoltage(5.f);cv->refresh=0;cv->process(args);
  if(cv->targetDrive!=.5f)throw std::runtime_error("Overdrive CV must add 10% per volt");
  if(cv->drive<=0.f||cv->drive>.0021f)throw std::runtime_error("Overdrive CV changes must slew instead of jumping");
  cv->inputs[P::OVERDRIVE_CV].setVoltage(-10.f);cv->refresh=0;cv->process(args);
  if(cv->targetDrive!=0.f)throw std::runtime_error("Overdrive CV must clamp at zero");
  std::puts("Windy Wally module stereo/mono width, overdrive, CV isolation, input ranges and gust controls passed");
}

inline void renderWindyWallyAudio(rack::plugin::Model* model,const std::string& output) {
  using P=ComputerscareWindyWally;
  auto m=std::unique_ptr<P>(static_cast<P*>(model->createModule()));
  m->outputs[P::LEFT].channels=1;m->outputs[P::RIGHT].channels=1;
  m->params[P::STRENGTH].setValue(.75f);
  m->params[P::GUSTINESS].setValue(.7f);
  m->params[P::TURBULENCE].setValue(.6f);
  m->params[P::TONE].setValue(.6f);
  m->params[P::TEXTURE].setValue(.65f);
  m->params[P::PRESENCE].setValue(.6f);
  rack::engine::Module::ProcessArgs args;args.sampleRate=48000;args.sampleTime=1.f/48000;
  std::vector<float> audio(12*48000*2);
  for(int frame=-144000;frame<12*48000;++frame) {
    args.frame=frame+144000;
    if(frame==4*48000)m->params[P::OVERDRIVE].setValue(1.f);
    if(frame==8*48000) {
      m->params[P::STRENGTH].setValue(1.f);m->params[P::TONE].setValue(1.f);
      m->params[P::TEXTURE].setValue(1.f);m->params[P::TURBULENCE].setValue(1.f);
      m->params[P::GUST].setValue(1.f);
    }
    m->process(args);
    if(frame>=0) {
      audio[2*frame]=m->outputs[P::LEFT].getVoltage();
      audio[2*frame+1]=m->outputs[P::RIGHT].getVoltage();
    }
  }
  // Fade only the exported file boundaries so starting playback does not click.
  for(int frame=0;frame<12*48000;++frame) {
    float fade=std::min(1.f,std::min(frame,12*48000-1-frame)/960.f);
    audio[2*frame]*=fade;audio[2*frame+1]*=fade;
  }
  phlooper::saveWav(output+"/02-clean-overdrive-storm.wav",audio.data(),12*48000,true);
  struct Example { const char* name; float controls[8]; bool gusts; };
  const Example examples[] = {
    {"03-soft-breeze", {.4f,.2f,.15f,.25f,.2f,.5f,.7f,0.f}, false},
    {"04-textured-wind", {.65f,.6f,.5f,.6f,.7f,.6f,1.f,.2f}, false},
    {"05-gusty-overdrive", {1.f,.9f,.9f,.8f,.9f,.8f,1.f,.65f}, true}
  };
  for (const auto& example : examples) {
    auto voice=std::unique_ptr<P>(static_cast<P*>(model->createModule()));
    voice->outputs[P::LEFT].channels=1;voice->outputs[P::RIGHT].channels=1;
    for(int i=0;i<8;++i)voice->params[i].setValue(example.controls[i]);
    for(int frame=-4*48000;frame<12*48000;++frame) {
      args.frame=frame+4*48000;
      voice->params[P::GUST].setValue(example.gusts&&(frame==2*48000||frame==8*48000)?1.f:0.f);
      voice->process(args);
      if(frame>=0) {
        float fade=std::min(1.f,std::min(frame,12*48000-1-frame)/960.f);
        audio[2*frame]=voice->outputs[P::LEFT].getVoltage()*fade;
        audio[2*frame+1]=voice->outputs[P::RIGHT].getVoltage()*fade;
      }
    }
    phlooper::saveWav(output+"/"+example.name+".wav",audio.data(),12*48000,true);
  }

}
