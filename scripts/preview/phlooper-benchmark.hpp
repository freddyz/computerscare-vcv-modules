#pragma once
// Opt-in DSP-only timing. The same live module path is used before/after changes.
inline void benchmarkPhlooper(rack::plugin::Model* model) {
  using P=ComputerscarePhlooper;
  volatile float checksum=0;
  for(int count:{3,16})for(bool stereo:{false,true}) {
    auto m=std::unique_ptr<P>(static_cast<P*>(model->createModule()));
    m->loop.size=48000;m->createdLoops=count;
    for(int voice=0;voice<16;++voice){m->loop.stereo[voice]=stereo;for(int i=0;i<48000;++i){m->loop.audio[voice][2*i]=std::sin(i*.031f);m->loop.audio[voice][2*i+1]=std::cos(i*.023f);}}
    m->params[P::COUNT].setValue(count);m->params[P::OFFSET].setValue(-1);m->params[P::MIX].setValue(1);
    m->outputs[P::OUT_L].channels=m->outputs[P::OUT_R].channels=1;
    rack::engine::Module::ProcessArgs args;args.sampleRate=48000;args.sampleTime=1.f/48000;
    for(int i=0;i<20000;++i){args.frame=i;m->process(args);}
    std::vector<double> timings;
    for(int repeat=0;repeat<7;++repeat){auto start=std::chrono::steady_clock::now();
      for(int i=0;i<240000;++i){args.frame++;m->process(args);}
      auto end=std::chrono::steady_clock::now();
      timings.push_back(std::chrono::duration<double,std::nano>(end-start).count()/240000);
      checksum+=m->outputs[P::OUT_L].getVoltage();
    }
    std::sort(timings.begin(),timings.end());
    std::printf("Phlooper full DSP: %d loops %s: %.1f ns/sample, %.3f%% of one core at 48kHz\n",count,stereo?"stereo":"mono",timings[3],timings[3]*.0048);
  }
  std::printf("Benchmark checksum: %.6f\n",float(checksum));
}
