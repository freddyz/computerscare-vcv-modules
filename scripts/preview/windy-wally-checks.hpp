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
  loud->params[P::GAIN].setValue(18.f);
  rack::engine::Module::ProcessArgs args;args.sampleRate=48000;args.sampleTime=1.f/48000;
  double wide=0,narrow=0,quietEnergy=0,loudEnergy=0;
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
      loudEnergy+=std::abs(loud->outputs[P::LEFT].getVoltage());
    }
  }
  if(wide<=0||narrow>wide*.001||loudEnergy<=quietEnergy*2)
    throw std::runtime_error("Windy Wally stereo width or gain did not respond");
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
  std::puts("Windy Wally module stereo/mono width, gain, CV isolation, input ranges and gust controls passed");
}
