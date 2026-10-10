#include "Phlooper/Engine.hpp"
#include "Phlooper/Waveform.hpp"
#include "Phlooper/Zoom.hpp"
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
  e.process({},48000,s,0,0,0,0,0,false);
  check(e.activeStart[0]==-1,"empty loops do not latch start points before audio loads");
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
  s.mode=0;s.offset=20;s.start=0;s.length=1;e.resetHeads();
  for(int i=0;i<1000;++i)e.process(in,48000,s,0,0,0,0,0,false);
  check(e.head[1]>=1000,"positive time offsets add silence beyond original length");
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
  s.speed=1;s.count=2;s.offset=-1;s.start=.2f;e.resetHeads();
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
  check(e.periods[0]==1096&&e.periods[1]==904&&e.periods[2]==1096,
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
  double difference=e.head[2]/e.periods[2]-e.head[0]/e.periods[0];
  s.hold=true;
  for(int i=0;i<100;++i)e.process(in,48000,s,0,0,0,0,0,false);
  check(std::fabs(e.head[2]/e.periods[2]-e.head[0]/e.periods[0]-difference)<1e-10,"speed hold preserves normalized phase without matching pitches");
  s.hold=false;e.process(in,48000,s,0,0,0,0,0,false);
  check(e.head[2]/e.periods[2]-e.head[0]/e.periods[0]>difference,"release resumes normal drift");
  s.mode=0;s.offsetMask=1;s.offsets[0]=-2;s.hold=true;
  e.process(in,48000,s,0,0,0,0,0,false);
  check(e.periods[0]==e.periods[1]&&e.periods[1]==e.periods[2]&&e.periods[0]==904,
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
  double restoredHead=resumed.head[0];
  resumed.activeStart.fill(-1);
  resumed.process(in,48000,s,0,0,0,0,65535u,false);
  check(resumed.activeStart[0]==int(resumed.size*s.start),
        "loaded regions adopt current start before the first wrap");
  check(resumed.head[0]==restoredHead,
        "initializing loaded regions preserves restored playhead progress");
  s.count=3;s.mode=0;s.offset=0;s.offsetMask=0;s.hold=false;
  s.start=.2f;s.length=.8f;s.startMask=3;s.lengthMask=3;
  s.starts[0]=.1f;s.starts[1]=.7f;s.lengths[0]=.3f;s.lengths[1]=.6f;
  e.resetHeads();e.process(in,48000,s,0,0,0,0,0,false);
  check(e.activeStart[0]==int(e.size*.1f)&&e.activeStart[1]==int(e.size*.7f)&&e.activeStart[2]==int(e.size*.2f),"per-loop start CV regions and missing-channel fallback");
  check(e.periods[0]==int(e.size*.3f)&&e.periods[1]==int(e.size*.6f)&&e.periods[2]==int(e.size*.8f),"per-loop length CV regions and missing-channel fallback");
  s.hold=true;e.process(in,48000,s,0,0,0,0,0,false);
  check(e.periods[0]==e.periods[1]&&e.periods[1]==e.periods[2],"hold uses loop one's CV-controlled length for all loops");
  s.count=1;s.mode=0;s.offset=0;s.offsetMask=0;s.startMask=0;s.lengthMask=0;
  s.start=0;s.length=1;s.speed=1;s.hold=false;e.resetHeads();
  for(int i=0;i<10;++i)e.process(in,48000,s,1,0,0,0,0,false);
  check(e.writeSpans[0].kind==1&&e.writeSpans[0].from==0&&e.writeSpans[0].extent==10,"record region grows from the action start");
  for(int i=0;i<10;++i)e.process(in,48000,s,0,0,0,0,0,false);
  check(e.writeSpans[0].extent==10,"record region stops growing when recording stops");
  for(int i=0;i<5;++i)e.process(in,48000,s,0,1,0,0,0,false);
  check(e.writeSpans[0].kind==2&&e.writeSpans[0].from==20&&e.writeSpans[0].extent==5,"erase region starts at its own action position");
  e.process(in,48000,s,0,0,0,0,0,false);e.head[0]=e.size-5;
  for(int i=0;i<10;++i)e.process(in,48000,s,1,0,0,0,0,false);
  check(e.writeSpans[0].from==e.size-5&&e.writeSpans[0].extent==10,"record region retains its origin across a loop wrap");
  s.count=4;s.mode=0;s.offset=0;s.mix=1;s.recordMix=.25f;s.overdub=0;
  auto monitor=[&](unsigned record,unsigned erase,unsigned stop){
    for(int v=0;v<s.count;++v){
      std::fill(e.audio[v].get(),e.audio[v].get()+e.size*2,2.f);
      e.head[v]=120;e.fade[v]=1;e.stereo[v]=true;
    }
    phlooper::Frame live;live.l=6;live.r=10;
    return e.process(live,48000,s,record,erase,0,0,stop,true);
  };
  auto monitored=monitor(15,0,0);
  check(std::fabs(monitored.l-3.f)<1e-5f&&std::fabs(monitored.r-4.f)<1e-5f,
        "blend monitoring mixes live input once at record mix across all loops");
  s.overdub=1;monitored=monitor(15,0,0);
  check(std::fabs(monitored.l-3.5f)<1e-5f&&std::fabs(monitored.r-4.5f)<1e-5f,
        "add monitoring retains loop audio and adds input at record mix");
  monitored=monitor(15,15,0);
  check(monitored.l==0&&monitored.r==0,"erase monitoring silences addressed loops and overrides recording");
  monitored=monitor(0,1,0);
  check(std::fabs(monitored.l-1.5f)<1e-5f,"erasing one loop leaves other loops audible at their existing gain");
  s.mix=.5f;monitored=monitor(0,15,0);
  check(monitored.l==3&&monitored.r==5,"output mix retains its dry input during erase");
  s=phlooper::Settings();s.count=3;s.offset=0;s.directions[1]=1;s.directions[2]=2;
  e.resetHeads();
  for(int v=0;v<3;++v){
    for(int frame=0;frame<e.size;++frame){
      e.audio[v][frame*2]=float(frame)/1000;
      e.audio[v][frame*2+1]=float(frame)/2000;
    }
    e.stereo[v]=true;e.head[v]=100;e.fade[v]=1;
  }
  auto directional=e.process({},48000,s,0,0,0,0,0,false);
  check(std::fabs(directional.l-(.1f+.899f+.1f)/3)<1e-5f &&
        std::fabs(directional.r-directional.l/2)<1e-5f,
        "each loop independently reads forward, reverse, or the forward bounce leg");
  check(e.playbackPosition(0,0)==101&&e.playbackPosition(1,1)==898,
        "reverse tracker follows the actual source position");
  e.head[2]=e.size-1;
  e.process({},48000,s,0,0,0,0,0,false);
  check(e.returning[2]&&e.head[2]==0&&(e.completed&4),
        "forward then reverse turns at the region end and emits EOC");
  e.process({},48000,s,0,0,0,0,4,false);
  check(e.returning[2]&&e.head[2]==0,"pause holds the reverse bounce leg");
  e.head[2]=e.size-1;
  e.process({},48000,s,0,0,0,0,0,false);
  check(!e.returning[2]&&e.head[2]==0,"bounce returns to forward at the region start");
  e.returning[2]=true;
  e.process({},48000,s,0,0,4,0,0,false);
  check(!e.returning[2]&&e.head[2]==1,"restart begins bounce in the forward direction");
  s.count=1;s.directions[0]=1;s.recordMix=1;
  e.head[0]=100;e.fade[0]=1;
  phlooper::Frame live;live.l=6;live.r=10;
  e.process(live,48000,s,1,0,0,0,0,true);
  check(e.audio[0][899*2]==6&&e.audio[0][899*2+1]==10&&e.audio[0][100*2]==.1f,
        "reverse overdub writes stereo audio beneath the reverse head");
  e.process(live,48000,s,0,1,0,0,0,true);
  check(e.audio[0][898*2]==0&&e.audio[0][898*2+1]==0,
        "reverse erase clears the reverse head location");
  e.head[0]=e.size-1;e.process({},48000,s,0,0,0,0,0,false);
  check(e.head[0]==0&&(e.completed&1)&&e.playbackPosition(0,1)==e.size-1,
        "reverse wraps from the start back to the region end");
  s.length=.5f;s.start=.8f;
  e.resetHeads();e.head[0]=100;e.fade[0]=1;
  e.process({},48000,s,0,0,0,0,0,false);
  check(e.activeStart[0]==800&&e.periods[0]==500&&e.playbackPosition(0,1)==398,
        "reverse respects a shortened region wrapping across the source boundary");
  std::vector<float> waveformSamples(1024*2);
  for(int frame=0;frame<1024;++frame){waveformSamples[frame*2]=2;waveformSamples[frame*2+1]=-3;}
  auto wave=phlooper::sampleWaveformBin(waveformSamples.data(),1024,true,128);
  check(wave.low==-3&&wave.high==2,"waveform retains opposite-phase stereo extrema");
  wave=phlooper::sampleWaveformBin(waveformSamples.data(),1024,false,255);
  check(wave.low==2&&wave.high==2,"mono waveform ignores the unused right channel");
  waveformSamples[1023*2]=7;
  wave=phlooper::sampleWaveformBin(waveformSamples.data(),1024,false,255);
  check(wave.high==7,"waveform includes the last source frame");
  wave=phlooper::sampleWaveformBin(nullptr,0,false,0);
  check(wave.low==0&&wave.high==0,"empty waveform is silent");
  check(phlooper::waveformAmplitude(0)==0&&phlooper::waveformAmplitude(.05f)>.05f/5,
        "waveform compression preserves silence and lifts quiet details");
  check(phlooper::waveformAmplitude(5)<1&&phlooper::waveformAmplitude(20)==1&&phlooper::waveformAmplitude(100)==1,
        "waveform compression keeps headroom and bounds large peaks");
  check(phlooper::waveformAmplitude(-1)==-phlooper::waveformAmplitude(1),
        "waveform compression treats positive and negative audio symmetrically");
  std::array<phlooper::TimeRange,16> zoomRegions{};
  zoomRegions[0]={.2f,.21f};zoomRegions[1]={.2f,.22f};
  auto fitted=phlooper::fitLoopZoom(zoomRegions,2);
  check(std::fabs(fitted.start-.198f)<1e-6f&&std::fabs(fitted.end-.222f)<1e-6f,
        "zoom fits all visible loops with ten percent padding");
  zoomRegions[0]={.98f,1.f};zoomRegions[1]={.02f,.04f};
  fitted=phlooper::fitLoopZoom(zoomRegions,2);
  check(fitted.start<.98f&&fitted.end>1.04f&&fitted.end-fitted.start<.08f,
        "zoom keeps nearby loops across the WAV boundary together");
  check(std::fabs(phlooper::zoomPoint(.03f,fitted)-1.03f)<1e-6f,
        "zoom maps wrapped playheads to the correct visible copy");
  zoomRegions[0]={0,1};fitted=phlooper::fitLoopZoom(zoomRegions,1);
  check(fitted.start==0&&fitted.end==1,"full length loops retain the full recording view");
  wave=phlooper::sampleWaveformBin(waveformSamples.data(),1024,false,0,1023.f/1024,1025.f/1024);
  check(wave.high==7,"zoom waveform samples the selected source tail");
  wave=phlooper::sampleWaveformBin(waveformSamples.data(),1024,false,255,1023.f/1024,1025.f/1024);
  check(wave.high==2&&wave.low==2,"zoom waveform wraps into the source beginning");
  s=phlooper::Settings();s.mode=2;s.count=3;s.offset=10;s.directions[1]=1;s.directions[2]=2;
  e.resetHeads();e.process({},48000,s,0,0,0,0,0,false);
  e.head[0]=200;e.head[1]=330;e.head[2]=450;e.returning[2]=true;
  std::array<double,3> beforePosition{},beforePhase{};
  for(int v=0;v<3;++v){
    beforePosition[v]=e.sourceStart(v)+e.playbackPosition(v,s.directions[v]);
    beforePhase[v]=e.head[v]/e.periods[v];
  }
  s.hold=true;e.process({},48000,s,0,0,0,0,0,false);
  check(e.speedHold.active,"speed hold captures a fitted loop region per channel");
  for(int v=0;v<3;++v){
    double speed=1.+v*.1;
    check(std::fabs(e.speedHold.speeds[v]-speed)<1e-12,"speed hold retains distinct channel pitches");
    check(std::fabs(e.periods[v]/speed-e.speedHold.duration)<1e-10,"fitted in/out points give every speed exactly the same repeat time");
    double moved=e.sourceStart(v)+e.playbackPosition(v,s.directions[v])-beforePosition[v];
    moved-=std::round(moved/e.size)*e.size;
    check(std::fabs(moved-(v==0?speed:-speed))<1e-9,"engaging speed hold preserves forward, reverse, and returning source positions");
  }
  auto heldPhaseDifference=[&](int v){return phlooper::Engine::wrapPhase(e.head[v]/e.periods[v]-e.head[0]/e.periods[0]);};
  double heldDifference=phlooper::Engine::wrapPhase(beforePhase[1]-beforePhase[0]);
  for(int frame=0;frame<20000;++frame)e.process({},frame%2?44100:96000,s,0,0,0,0,0,false);
  check(std::fabs(heldPhaseDifference(1)-heldDifference)<1e-10,"shared hold clock prevents phase drift through wraps and sample-rate changes");
  double pausedPosition=e.head[1];
  for(int frame=0;frame<100;++frame)e.process({},48000,s,0,0,0,0,2,false);
  check(std::fabs(e.head[1]-pausedPosition)<1e-9,"per-loop pause overrides the shared hold clock");
  e.process({},48000,s,0,0,2,0,0,false);
  check(std::fabs(e.head[1]-1.1)<1e-9,"restart resets one held channel without changing its pitch");
  double heldPeriod=e.periods[0];s.speed=2;
  double phaseBefore=e.head[0]/e.periods[0];
  e.process({},48000,s,0,0,0,0,0,false);
  check(e.periods[0]==heldPeriod&&std::fabs(phlooper::Engine::wrapPhase(e.head[0]/e.periods[0]-phaseBefore)-2/e.speedHold.duration)<1e-10,
        "overall speed changes scale all held pitches together without changing fitted regions");
  resumed.size=e.size;resumed.head=e.head;resumed.periods=e.periods;resumed.activeStart=e.activeStart;
  resumed.returning=e.returning;resumed.previousDirection=e.previousDirection;resumed.fade=e.fade;resumed.speedHold=e.speedHold;
  for(int v=0;v<3;++v){std::copy(e.audio[v].get(),e.audio[v].get()+e.size*2,resumed.audio[v].get());resumed.stereo[v]=e.stereo[v];}
  auto heldOutput=e.process({},48000,s,0,0,0,0,0,false);
  auto restoredOutput=resumed.process({},48000,s,0,0,0,0,0,false);
  check(heldOutput.l==restoredOutput.l&&e.head[1]==resumed.head[1]&&e.speedHold.clock==resumed.speedHold.clock,
        "restored speed hold resumes the exact fitted regions and shared phase clock");
  s.hold=false;e.process({},48000,s,0,0,0,0,0,false);
  check(!e.speedHold.active&&e.periods[0]==e.size&&e.periods[1]==e.size,"release restores the selected loop lengths");
  s=phlooper::Settings();s.count=16;s.mode=2;s.speed=4;s.hold=true;
  s.offsetMask=65535;s.lengthMask=65535;
  for(int v=0;v<16;++v){s.offsets[v]=v%2?700:-99;s.lengths[v]=.001f+.06f*v;s.directions[v]=v%3;}
  e.resetHeads();
  for(int frame=0;frame<200;++frame){
    auto extreme=e.process({},44100,s,frame%2?65535:0,frame%2?0:65535,0,0,0,true);
    check(std::isfinite(extreme.l)&&std::isfinite(extreme.r),"extreme held speeds and fractional regions remain finite while writing");
  }
  for(int v=0;v<16;++v)
    check(std::fabs(e.periods[v]/e.speedHold.speeds[v]-e.speedHold.duration)<1e-10,
          "all sixteen extreme-speed channels share the exact held repeat time");
  std::array<phlooper::Frame,phlooper::voices> inputs{};
  for(int v=0;v<phlooper::voices;++v){inputs[v].l=v+1;inputs[v].r=-(v+1);}
  s=phlooper::Settings();s.count=3;s.recordMix=1;
  e.begin(true);
  for(int i=0;i<64;++i)e.process({},48000,s,65535,0,0,0,0,true,&inputs);
  e.finish();
  for(int v=0;v<16;++v)
    check(e.audio[v][20]==v+1&&e.audio[v][21]==-(v+1),
          "initial recording captures separate stereo input channels");
  e.resetHeads();
  for(int v=0;v<3;++v){inputs[v].l=10+v;inputs[v].r=15+v;}
  e.process({},48000,s,7,0,0,0,0,true,&inputs);
  for(int v=0;v<3;++v)
    check(e.audio[v][0]==10+v&&e.audio[v][1]==15+v,
          "overdubbing uses each loop's corresponding stereo input");
  s.mix=0;
  e.process({},48000,s,0,0,0,0,0,true,&inputs);
  for(int v=0;v<3;++v)
    check(e.outputs[v].l==inputs[v].l&&e.outputs[v].r==inputs[v].r,
          "polyphonic dry output retains independent stereo channels");
  s.mix=1;s.offset=0;e.resetHeads();
  phlooper::Frame mixed;
  for(int frame=0;frame<24;++frame)
    mixed=e.process({},48000,s,0,0,0,0,0,true,&inputs);
  float channelLeft=0,channelRight=0;
  for(int v=0;v<3;++v){channelLeft+=e.outputs[v].l;channelRight+=e.outputs[v].r;}
  check(std::fabs(mixed.l-channelLeft/3)<1e-6&&std::fabs(mixed.r-channelRight/3)<1e-6,
        "mixed output equals the average of separate wet loop outputs");
  for(int frame=0;frame<1000;++frame)e.process({},48000,s,0,0,0,2,0,true,&inputs);
  check(e.outputs[1].l<e.outputs[0].l*.1f,"per-channel mute affects polyphonic output");
  e.begin(false);
  e.process({},96000,s,65535,0,0,0,0,false,&inputs);
  e.process({},96000,s,65535,0,0,0,0,true,&inputs);
  check(e.stereo[0]&&e.audio[0][0]==inputs[0].l&&e.audio[0][1]==inputs[0].r,
        "connecting stereo during initial recording retains the right channel");
  e.begin(false);
  e.process({},96000,s,65535,0,0,0,0,false);
  e.process({},96000,s,65535,0,0,0,0,false);
  check(e.audio[0][0]==0,"a new recording cannot interpolate stale previous input");
  e.finish();e.size=64;e.resetHeads();s=phlooper::Settings();s.count=2;s.speed=4;s.offset=0;
  for(int v=0;v<2;++v)for(int i=0;i<128;++i)e.audio[v][i]=1;
  e.process({},48000,s,0,0,0,0,0,false);
  double movingHead=e.head[1];
  for(int i=0;i<1000;++i)e.process({},48000,s,0,0,0,2,0,false);
  check(std::fabs(e.head[1]-std::fmod(movingHead+4000,64))<1e-9,
        "silent interpolation bypass preserves muted head timing");
  for(int i=0;i<100;++i)e.process({},48000,s,0,0,0,0,0,false);
  check(e.outputs[1].l>0,"muted voices resume audio after interpolation bypass");
  e.size=0;s.mix=.25f;
  phlooper::Frame dry;dry.l=4;dry.r=-2;
  auto emptyOutput=e.process(dry,48000,s,0,0,0,0,0,true);
  check(emptyOutput.l==3&&emptyOutput.r==-1.5f&&e.outputs[1].l==3,
        "empty fast path preserves mixed and polyphonic dry monitoring");
  std::puts("Phlooper DSP and WAV tests passed");
}
