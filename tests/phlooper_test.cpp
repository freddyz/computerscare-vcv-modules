#include "Phlooper/Engine.hpp"
#include "Phlooper/Wav.hpp"
#include <cstdio>
#include <cstdlib>
#include <cmath>
void check(bool ok,const char* message){if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
void wavFixture(const char* path, int format, int bits, int channels, bool extensible) {
  std::ofstream out(path,std::ios::binary);
  auto put=[&](uint64_t value,int n){for(int i=0;i<n;++i)out.put(char(value>>(8*i)));};
  int fmtSize=extensible?40:16,bytes=bits/8,dataSize=2*channels*bytes;
  out.write("RIFF",4);put(20+fmtSize+dataSize,4);out.write("WAVEfmt ",8);put(fmtSize,4);
  put(extensible?65534:format,2);put(channels,2);put(48000,4);put(48000*channels*bytes,4);put(channels*bytes,2);put(bits,2);
  if(extensible){put(22,2);put(bits,2);put(0,4);put(format,4);put(0,2);put(16,2);put(128,1);put(0,1);put(0,1);put(170,1);put(0,1);put(56,1);put(155,1);put(113,1);}
  out.write("data",4);put(dataSize,4);
  for(int i=0;i<2*channels;++i){
    uint64_t raw;
    if(format==3&&bits==64){double x=.5;std::memcpy(&raw,&x,8);}
    else if(format==3){float x=.5;uint32_t value;std::memcpy(&value,&x,4);raw=value;}
    else raw=bits==8?192:((uint64_t(1)<<(bits-1))/2);
    put(raw,bytes);
  }
}
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
  s.mode=0;s.offset=0;s.speed=4;s.count=16;s.length=1;s.start=0;
  e.resetHeads();
  for(int i=0;i<249;++i) {
    e.process(in,48000,s,0,0,0,0,0,false);
    check(e.completed==0,"EOC remains low before loop boundary");
  }
  e.process(in,48000,s,0,0,0,65535,0,false);
  check(e.completed==65535,"all sixteen muted heads still emit EOC at 4x");
  e.process(in,48000,s,0,0,0,0,0,false);
  check(e.completed==0,"EOC event lasts one DSP frame");
  s.speed=.25f;e.resetHeads();
  for(int i=0;i<3999;++i)e.process(in,48000,s,0,0,0,0,0,false);
  check(e.completed==0,"quarter speed extends period fourfold");
  e.process(in,48000,s,0,0,0,0,1,false);
  check(e.completed==65534,"stopped head emits no EOC while other heads wrap");
  e.process(in,48000,s,0,0,65535,0,0,false);
  check(e.completed==0,"restart does not emit EOC");
  s.speed=1;s.count=2;s.offset=1;s.start=.2f;e.resetHeads();
  e.process(in,48000,s,0,0,0,0,0,false);
  check(e.activeStart[0]==200&&e.activeStart[1]==200,"heads start at requested source location");
  s.start=.6f;
  for(int i=0;i<951;++i)e.process(in,48000,s,0,0,0,0,0,false);
  check(e.activeStart[0]==200&&e.activeStart[1]==600,"start changes latch separately at each loop wrap");
  for(int i=0;i<48;++i)e.process(in,48000,s,0,0,0,0,0,false);
  check(e.activeStart[0]==600,"base head adopts start on its next wrap");
  s.start=.4f;e.process(in,48000,s,0,0,1,0,0,false);
  check(e.activeStart[0]==400&&e.activeStart[1]==600,"retrigger adopts pending start only for addressed head");
  s.start=0;s.count=3;s.mode=0;s.offset=1;s.speed=1;
  s.offsetMask=3;s.offsets[0]=2;s.offsets[1]=-2;e.resetHeads();
  e.process(in,48000,s,0,0,0,0,0,false);
  check(e.periods[0]==904&&e.periods[1]==1096&&e.periods[2]==904,
    "explicit offsets replace index spacing, missing CV channels retain knob spacing");
  s.mode=1;s.offsets[0]=10;s.offsets[1]=-10;e.resetHeads();
  e.process(in,48000,s,0,0,0,0,0,false);
  check(std::fabs(e.periods[0]-1100)<.001&&std::fabs(e.periods[1]-900)<.001&&std::fabs(e.periods[2]-1020)<.001,
    "positive length offsets extend periods; negative offsets shorten them, including knob spacing");
  s.mode=2;e.resetHeads();e.process(in,48000,s,0,0,0,0,0,false);
  check(std::fabs(e.head[0]-1.1)<.001&&std::fabs(e.head[1]-.9)<.001,
    "explicit speed offsets map to percent of nominal speed");
  s.offsetMask=0;s.mode=2;s.offset=10;s.count=3;s.speed=1;s.hold=false;e.resetHeads();
  for(int i=0;i<100;++i)e.process(in,48000,s,0,0,0,0,0,false);
  double difference=e.head[2]-e.head[0];
  s.hold=true;
  for(int i=0;i<100;++i)e.process(in,48000,s,0,0,0,0,0,false);
  check(std::fabs(e.head[2]-e.head[0]-difference)<.001,"hold preserves phase difference without retriggering");
  s.hold=false;e.process(in,48000,s,0,0,0,0,0,false);
  check(e.head[2]-e.head[0]>difference,"release resumes normal drift");
  s.mode=0;s.offsetMask=1;s.offsets[0]=-2;s.hold=true;
  e.process(in,48000,s,0,0,0,0,0,false);
  check(e.periods[0]==e.periods[1]&&e.periods[1]==e.periods[2]&&e.periods[0]==1096,
    "hold uses first loop period including its explicit offset");
  for(int variant=0;variant<4;++variant){
    int format=variant==1||variant==3?3:1;
    int bits=variant==0?8:variant==1?64:variant==2?24:32;
    int channels=variant==3?4:variant==2?2:1;
    wavFixture("/tmp/phlooper-format-test.wav",format,bits,channels,variant>=2);
    auto decoded=phlooper::loadWav("/tmp/phlooper-format-test.wav");
    check(decoded.channels==channels&&decoded.samples.size()==size_t(2*channels),"broader WAV formats preserve frame and channel counts");
    for(float value:decoded.samples)check(std::fabs(value-.5f)<1e-6,"PCM8, float64 and extensible WAV decode correctly");
  }
  std::remove("/tmp/phlooper-format-test.wav");
  phlooper::Engine resumed(e.limit);resumed.size=e.size;
  resumed.head=e.head;resumed.activeStart=e.activeStart;
  for(int v=0;v<16;++v){
    std::copy(e.audio[v].get(),e.audio[v].get()+e.size*2,resumed.audio[v].get());
    resumed.stereo[v]=e.stereo[v];
  }
  s.hold=false;s.start=.8f;
  e.process(in,48000,s,0,0,0,0,0,false);
  resumed.process(in,48000,s,0,0,0,0,0,false);
  for(int v=0;v<s.count;++v)
    check(e.head[v]==resumed.head[v]&&e.activeStart[v]==resumed.activeStart[v],
      "restored playheads continue from saved offsets and pending starts remain pending");
  s.count=3;s.mode=0;s.offset=0;s.offsetMask=0;s.hold=false;
  s.start=.2f;s.length=.8f;s.startMask=3;s.lengthMask=3;
  s.starts[0]=.1f;s.starts[1]=.7f;s.lengths[0]=.3f;s.lengths[1]=.6f;
  e.resetHeads();e.process(in,48000,s,0,0,0,0,0,false);
  check(e.activeStart[0]==int(e.size*.1f)&&e.activeStart[1]==int(e.size*.7f)&&e.activeStart[2]==int(e.size*.2f),"per-loop start CV regions and missing-channel fallback");
  check(e.periods[0]==int(e.size*.3f)&&e.periods[1]==int(e.size*.6f)&&e.periods[2]==int(e.size*.8f),"per-loop length CV regions and missing-channel fallback");
  s.hold=true;e.process(in,48000,s,0,0,0,0,0,false);
  check(e.periods[0]==e.periods[1]&&e.periods[1]==e.periods[2],"hold uses loop one's CV-controlled length for all loops");
  std::puts("Phlooper DSP and WAV tests passed");
}
