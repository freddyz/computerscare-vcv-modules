#include "Phlooper/Engine.hpp"
#include "Phlooper/Wav.hpp"
#include <cstdio>
#include <cstdlib>
#include <cmath>
void check(bool ok,const char* message){if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
int main(){
  phlooper::Engine e(4800);phlooper::Settings s;s.count=16;s.offset=1;
  e.begin(false);
  phlooper::Frame in;in.l=1;in.r=1;
  for(int i=0;i<1000;++i)e.process(in,48000,s,0,0,0,0,0,false);
  e.finish();check(e.size==1000,"first recording defines length");
  for(int i=0;i<16;++i)check(e.audio[i][100]==1&&!e.stereo[i],"initial mono recording seeds every independent loop");
  s.recordMix=1;s.offset=0;
  phlooper::Frame replacement;replacement.l=2;replacement.r=3;
  e.process(replacement,48000,s,1u<<15,0,0,0,0,true);
  check(e.audio[15][0]==2&&e.audio[15][1]==3&&e.stereo[15],"poly record updates selected stereo loop");
  check(e.audio[0][0]==1&&!e.stereo[0],"other loop remains independent");
  e.resetHeads();e.process(in,48000,s,0,1u<<15,0,0,0,false);
  check(e.audio[15][0]==0&&e.audio[0][0]==1,"erase addresses individual head");
  e.head[0]=200;e.head[15]=300;e.process(in,48000,s,0,0,1u<<15,0,1,false);
  check(e.head[0]==200&&e.head[15]==1,"stop holds one head while retrigger resets another");
  e.resetHeads();s.offset=1;
  for(int i=0;i<1000;++i)e.process(in,48000,s,0,0,0,0,0,false);
  check(e.head[0]<.001&&e.head[1]>0,"time offsets cause phase drift");
  s.mode=2;s.offset=-20;s.start=1;s.length=.001f;
  for(int i=0;i<10000;++i){auto out=e.process(in,192000,s,0,0,0,0,0,false);check(std::isfinite(out.l)&&std::fabs(out.l)<=3,"extreme settings remain bounded");}
  s.mode=0;s.offset=-20;s.start=0;s.length=1;e.resetHeads();
  for(int i=0;i<1000;++i)e.process(in,48000,s,0,0,0,0,0,false);
  check(e.head[1]>=1000,"negative time offsets add silence beyond original length");
  phlooper::saveWav("/tmp/phlooper-test.wav",e.audio[0].get(),e.size,false);
  auto wav=phlooper::loadWav("/tmp/phlooper-test.wav");
  check(wav.channels==1&&wav.samples.size()==1000&&std::fabs(wav.samples[50]-.2f)<1e-6,"mono float WAV roundtrip preserves audio scale");
  std::remove("/tmp/phlooper-test.wav");
  phlooper::Engine highRate(4800);highRate.begin(true);
  for(int i=0;i<4000;++i)highRate.process(in,192000,s,0,0,0,0,0,true);
  highRate.finish();check(highRate.size==1000,"recording duration is independent of Rack sample rate");
  for(int i=0;i<e.size;++i) e.audio[0][2*i]=std::sin(float(i)*2.f*3.14159265f*.4f);
  double raw=0,filtered=0;
  for(int i=100;i<900;++i) {
    auto a=e.read(0,i,0,e.size);auto b=e.filteredRead(0,i,0,e.size,2);
    raw+=a.l*a.l;filtered+=b.l*b.l;
  }
  check(filtered<raw*.01,"accelerated read suppresses frequencies above its Nyquist limit");
  s.mode=2;s.offset=20;s.count=16;s.length=1;
  for(int i=0;i<100000;++i) {
    auto out=e.process(in,48000,s,65535,0,0,0,0,true);
    check(std::isfinite(out.l)&&std::isfinite(out.r),"sixteen accelerated overdubs remain finite");
  }
  std::puts("Phlooper DSP and WAV tests passed");
}
