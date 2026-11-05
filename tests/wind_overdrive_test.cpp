#include "Wind/Generator.hpp"
#include "Wind/Overdrive.hpp"
#include <cstdio>
#include <cstdlib>
#include <limits>
static void check(bool ok,const char* why) {if(!ok){std::fprintf(stderr,"FAIL: %s\n",why);std::exit(1);}}
int main() {
  namespace air=computerscare::wind;
  for(float rate:{8000.f,44100.f,48000.f,96000.f}) {
    air::Overdrive shaper;
    for(int i=0;i<int(rate);++i) {
      float x=1.5f*std::sin(2.f*air::pi*137.f*i/rate);
      float amount=i<int(rate/2)?0.f:1.f;
      float y=shaper.process(x,amount,rate);
      check(std::isfinite(y)&&std::abs(y)<=1.f,"Overdrive stays bounded during full-range control changes");
    }
    check(shaper.process(std::numeric_limits<float>::infinity(),1,rate)==0,"Bad upstream samples do not poison the overdrive state");
    for(int i=0;i<int(rate);++i)check(shaper.process(0,1,rate)==0,"Reset/silent overdrive stays silent");
  }
  // Compare folded fifth/seventh harmonics against naive waveshaping at 7 kHz.
  air::Overdrive antialiased;
  double real[3]={},imag[3]={},naiveReal[3]={},naiveImag[3]={};
  const int frequency[]={7000,13000,1000};
  for(int i=-4800;i<48000;++i) {
    float x=.4f*std::sin(2.*air::pi*7000.*i/48000.);
    float y=antialiased.process(x,1,48000);
    float naive=std::tanh(air::Overdrive::curve(x*9.)/3.);
    if(i<0)continue;
    for(int k=0;k<3;++k) {
      double phase=2.*air::pi*frequency[k]*i/48000.;
      real[k]+=y*std::cos(phase);imag[k]+=y*std::sin(phase);
      naiveReal[k]+=naive*std::cos(phase);naiveImag[k]+=naive*std::sin(phase);
    }
  }
  auto energy=[](double x,double y){return x*x+y*y;};
  double aliases=(energy(real[1],imag[1])+energy(real[2],imag[2]))/energy(real[0],imag[0]);
  double naiveAliases=(energy(naiveReal[1],naiveImag[1])+energy(naiveReal[2],naiveImag[2]))/energy(naiveReal[0],naiveImag[0]);
  check(aliases<naiveAliases*.25,"Antiderivative shaping substantially reduces aliasing");
  // Wind controls at all 64 endpoint combinations, including repeated gusts.
  for(int corner=0;corner<64;++corner) {
    air::Generator generator;
    air::Overdrive clean[2],driven[2];
    generator.configure(48000,(corner>>3)&1,(corner>>4)&1,(corner>>5)&1);
    for(int frame=0;frame<96000;++frame) {
      if(frame%24000==0)generator.gust();
      auto audio=generator.process(corner&1,(corner>>1)&1,(corner>>2)&1,3);
      check(std::isfinite(generator.strength())&&generator.strength()>=0&&generator.strength()<=10,"Wind CV stays in range at parameter extremes");
      for(int ch=0;ch<2;++ch) {
        float x=audio.channel[ch]*3.f;
        check(std::isfinite(x),"Shared wind stays finite at all parameter endpoints");
        float a=clean[ch].process(x,0,48000),b=driven[ch].process(x,1,48000);
        check(std::isfinite(a)&&std::isfinite(b)&&std::abs(a)<=1&&std::abs(b)<=1,"Clean and driven wind remain stable at every corner");
      }
    }
  }
  std::puts("Wind overdrive alias suppression, smoothing, silence, sample rates and 64 parameter-corner checks passed");
}
