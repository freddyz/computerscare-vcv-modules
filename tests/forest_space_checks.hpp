#pragma once
#include <vector>

inline void checkForestSpace() {
  namespace wc=windchimes;
  auto response=[](std::array<float,4> settings, float frequency) {
    auto room=std::unique_ptr<wc::Reverb>(new wc::Reverb);
    std::array<float,8> pitches;
    for (int i=0;i<8;++i) pitches[i]=frequency*(1.f+i*.13f);
    room->configureForest(48000,settings[0],settings[1],settings[2],settings[3],pitches);
    std::vector<float> result(96000);
    for (int sample=0;sample<int(result.size());++sample) {
      auto out=room->processWetQuad({sample==0 ? .5f : 0.f, sample==0 ? .3f : 0.f});
      for (float value : out.channel) require(std::isfinite(value)&&std::abs(value)<2.f,"Forest feedback remains finite and bounded");
      result[sample]=out.channel[0];
    }
    return result;
  };
  std::array<float,4> baseline{{.5f,.5f,.6f,.4f}};
  auto reference=response(baseline,220.f);
  for (int parameter=0;parameter<4;++parameter) {
    auto changed=baseline; changed[parameter]=.95f;
    auto audio=response(changed,220.f); double difference=0;
    for (size_t i=0;i<audio.size();++i) difference+=std::abs(audio[i]-reference[i]);
    require(difference>.001,"Each Forest control independently changes the acoustic response");
  }
  auto retuned=response(baseline,293.6648f); double tuningDifference=0;
  for (size_t i=0;i<retuned.size();++i) tuningDifference+=std::abs(retuned[i]-reference[i]);
  require(tuningDifference>.001,"Forest resonances follow the chimes' tuning");
  {
    auto a=std::unique_ptr<wc::Propagation>(new wc::Propagation);
    auto b=std::unique_ptr<wc::Propagation>(new wc::Propagation);
    a->configure(48000,.4f); b->configure(48000,.4f);
    a->configureReflections(48000,.6f,.7f,.8f,.3f,220.f,.4f,0);
    b->configureReflections(48000,.6f,.7f,.8f,.3f,293.6648f,.4f,1);
    double difference=0,energy=0;
    for (int sample=0;sample<48000;++sample) {
      a->process(sample==0 ? 1.f : 0.f); b->process(sample==0 ? 1.f : 0.f);
      difference+=std::abs(a->reflection()-b->reflection()); energy+=std::abs(a->reflection());
    }
    require(energy>.01&&difference>.01,"Early reflections inherit source pitch, material and position");
  }
  // Maximum Expanse must retain long echoes even at high sample rates.
  for (float rate : {48000.f,192000.f}) {
    auto path=std::unique_ptr<wc::Propagation>(new wc::Propagation);
    path->configure(rate,0.f);
    path->configureReflections(rate,1.f,0.f,0.f,0.f,220.f,0.f,0);
    int arrival=-1;
    for (int sample=0;sample<int(rate);++sample) {
      path->process(sample==0 ? 1.f : 0.f);
      if (arrival<0 && std::abs(path->reflection())>1e-5f) arrival=sample;
    }
    require(arrival>int(rate*.65f) && arrival<int(rate*.85f),
            "Maximum Expanse produces long echoes without high-rate buffer truncation");
  }
  for (float rate : {44100.f,96000.f,192000.f}) {
    auto room=std::unique_ptr<wc::Reverb>(new wc::Reverb);
    std::array<float,8> pitches{{55,110,220,330,440,660,880,1760}};
    for (int sample=0;sample<48000;++sample) {
      if (sample%1000==0) {
        float control=(sample/1000)%2;
        room->configureForest(rate,control,1.f-control,1.f,control,pitches);
      }
      auto out=room->processWetQuad({sample%4096==0 ? 1.f : 0.f,0.f});
      for (float value : out.channel) require(std::isfinite(value)&&std::abs(value)<4.f,"Forest stays stable across rates and changing controls");
    }
  }
  {
    auto dry=std::unique_ptr<wc::Engine>(new wc::Engine);
    auto forest=std::unique_ptr<wc::Engine>(new wc::Engine);
    dry->setSampleRate(48000); forest->setSampleRate(48000);
    wc::SetConfig config; config.enabled=true; config.tubes=4;
    dry->configure(0,config,0); forest->configure(0,config,0);
    dry->configureEffects(0,.5f,.4f,.3f,0);
    forest->configureForestEffects(0,.8f,.7f,.9f,.2f,.4f,.3f,0);
    dry->strike(0); forest->strike(0);
    for (int sample=0;sample<24000;++sample) {
      auto a=dry->processQuad(0,0,0,0), b=forest->processQuad(0,0,0,0);
      for (int ch=0;ch<4;++ch) require(std::abs(a.channel[ch]-b.channel[ch])<1e-7f,"Zero Dry/wet preserves the original direct chime sound exactly");
    }
  }
  std::puts("Forest controls, pitch-linked reflections, dry endpoint, stability and sample-rate checks passed");
}
