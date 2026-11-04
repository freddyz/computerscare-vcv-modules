#include "Wind/Generator.hpp"
#include "Windchimes/Wind.hpp"
#include <cstdio>
#include <cstdlib>
#include <type_traits>
static void check(bool ok,const char* text) { if(!ok) {std::fprintf(stderr,"FAIL: %s\n",text); std::exit(1);} }
int main() {
  namespace air=computerscare::wind;
  static_assert(std::is_same<air::Wind,windchimes::Wind>::value,"Chimes and standalone wind must use the same DSP type");
  for(float rate:{44100.f,48000.f,96000.f}) {
    air::Generator generator;
    air::Wind reference;
    generator.configure(rate,.72f,.64f,.4f);
    reference.configure(rate,.72f,.64f,.4f);
    int interval=std::max(1,int(rate/400.f));
    double energy=0,stereo=0;
    for(int i=0;i<int(rate*3);++i) {
      if(i==int(rate)) {generator.gust();reference.gust();}
      if((i+1)%interval==0) reference.step(interval/rate,.6f,.55f,.35f);
      auto expected=reference.processQuad(rate,3),actual=generator.process(.6f,.55f,.35f,3);
      for(int ch=0;ch<4;++ch)check(actual.channel[ch]==expected.channel[ch],"Standalone wind matches the chime wind sample for sample");
      check(generator.strength()==air::clamp(reference.strength*10.f,0.f,10.f),"Wind CV matches the shared field strength");
      for(float sample:actual.channel)check(std::isfinite(sample),"Wind samples remain finite");
      energy+=actual.channel[0]*actual.channel[0];
      stereo+=std::fabs(actual.channel[0]-actual.channel[1]);
    }
    check(energy>0 && stereo>0,"Wind produces stereo audio");
  }
  air::Generator silent;
  silent.configure(48000,.8f,.8f,1.f); silent.gust();
  for(int i=0;i<96000;++i) {
    auto out=silent.process(0,.8f,.8f,3);
    check(out.channel[0]==0 && out.channel[1]==0 && silent.strength()==0,"Gusts respect zero strength");
  }
  air::Generator cvOnly;
  cvOnly.configure(48000,.4f,.5f,.5f);
  float peak=0;
  for(int i=0;i<96000;++i) {cvOnly.process(.8f,.5f,.5f,0);peak=std::max(peak,cvOnly.strength());}
  check(peak>0 && peak<=10,"CV-only operation keeps weather running without audio outputs");
  std::puts("Shared wind DSP, exact standalone audio, stereo, CV, silence and gust checks passed");
}
