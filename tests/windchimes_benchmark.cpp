#include "Windchimes/Engine.hpp"
#include <chrono>
#include <cstdio>
#include <memory>
using namespace windchimes;
volatile double sink=0;
int main(){
 for(int material=0;material<3;++material){
  for(unsigned mask : {1u,3u,12u,15u}) for(int scenario=0;scenario<3;++scenario){
   auto engine=std::unique_ptr<Engine>(new Engine);engine->setSampleRate(48000);engine->setOutputMask(mask);
   SetConfig c;c.material=material;c.enabled=true;c.tubes=12;c.decay=.8f;c.body=.95f;c.shape=1;
   for(int j=0;j<8;++j)engine->configure(j,c,0);
   engine->configureEffects(.4,.7,.5,.5);
   double sum=0;auto start=std::chrono::steady_clock::now();
   for(int i=0;i<960000;++i){
    if(scenario!=2 && i%24000==0){engine->gust();for(int j=0;j<8;++j)engine->strike(j);}
    if(i%240==0)for(int j=0;j<8;++j)engine->configure(j,c,0);
    auto out=engine->processQuad(scenario==0?1.f:0.f,.7f,.5f,.1f);for(float v:out.channel)sum+=v*v;
   }
   sink+=sum;printf("engine material %d mask %u scenario %d %.4f energy %.9f\n",material,mask,scenario,std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count(),sum);
  }
  for(int scenario=0;scenario<2;++scenario){
   std::array<Resonator,96> voices;SetConfig c;c.material=material;c.decay=.8f;c.body=.95f;c.shape=1;
   for(auto& v:voices){v.configure(220,48000,c);v.strike(.8f,ContactKind::Striker,.4f);}
   double sum=0;auto start=std::chrono::steady_clock::now();
   for(int i=0;i<480000;++i){if(scenario==0&&i%2400==0)for(auto& v:voices)v.strike(.8f,ContactKind::Tube,.4f);for(auto& v:voices){float x=v.process();sum+=x*x;}}
   sink+=sum;printf("voices material %d scenario %d %.4f energy %.9f\n",material,scenario,std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count(),sum);
  }
 }
}
