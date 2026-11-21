#include "BeekapooPatchPlanner.hpp"
#include "BeekapooMemberLayout.hpp"
#include "BeekapooLabels.hpp"
#include "BeekapooExposurePlan.hpp"
#include "BeekapooParamLayout.hpp"
#include "BeekapooPortLayout.hpp"
#include <cassert>
#include <iostream>
#include <set>
using namespace beekapoo_patch;
int main() {
  {
    std::vector<Spec> sourcesOnly{library()[0], library()[1]};
    std::vector<Spec> empty;
    assert(Planner(1, empty).make().modules.empty());
    for(unsigned seed=0;seed<50;++seed) {
      Planner planner(seed,sourcesOnly);
      auto p=planner.make(4);
      assert(p.modules.size()==4 && planner.validate(p).empty());
      for(int type:p.modules) assert(type==0 || type==1);
    }
  }

  for (int count=1;count<=64;++count) {
    std::vector<float> widths;
    for(int i=0;i<count;++i)widths.push_back(15.f*(3+(i*7)%18));
    auto cells=beekapoo::compactMemberCells(widths);
    assert(cells.size()==widths.size());
    int row=0;float right=0;
    for(int i=0;i<count;++i) {
      if(cells[i].row!=row){assert(cells[i].row==row+1);++row;right=0;}
      assert(cells[i].x==right);right+=widths[i];
    }
    auto again=beekapoo::compactMemberCells(widths);
    for(int i=0;i<count;++i)assert(cells[i].x==again[i].x&&cells[i].row==again[i].row);
  }
  {
    auto cells=beekapoo::compactMemberCells(std::vector<float>(24,100));
    assert(cells.back().row==1&&cells[12].x==0&&cells.back().x==1100);
  }

  {
    // A sparse output beside two input columns must get useful space and a
    // caption, not an anonymous miniature jack at the top of an empty box.
    auto panes=beekapoo::portPaneSpace(74,2,1,1);
    assert(panes.first>=40&&panes.second>=34&&panes.first+panes.second==74);
    auto out=beekapoo::planPortCell(panes.second+5,96,false);
    assert(out.large&&out.jack.w==27&&out.label.h>=10);
    auto large=beekapoo::planPortCell(90,110,false);
    assert(large.jack.w==27&&large.jack.h==29&&large.fontSize>out.fontSize-1);
    auto overlap=[](beekapoo::PortRect a,beekapoo::PortRect b) {
      return a.w>0&&b.w>0&&a.x<b.x+b.w-.01f&&b.x<a.x+a.w-.01f&&a.y<b.y+b.h-.01f&&b.y<a.y+a.h-.01f;
    };
    for(float width:{24.f,30.f,42.f,60.f,100.f})for(float height:{45.f,65.f,100.f,160.f})for(bool input:{false,true}) {
      auto cell=beekapoo::planPortCell(width,height,input);
      for(auto r:{cell.jack,cell.gain,cell.label})if(r.w>0) {
        assert(r.x>=-.01f&&r.y>=-.01f&&r.x+r.w<=width+.01f&&r.y+r.h<=height+.01f);
      }
      assert(!overlap(cell.jack,cell.gain)&&!overlap(cell.jack,cell.label)&&!overlap(cell.gain,cell.label));
    }
  }
  {
    std::vector<beekapoo::ParamLayoutItem> labels{{"Frequency",0},{"Pulse width",0},{"Mix",1},{"Time",1},{"Feedback",1},{"Resonance",2},{"Cutoff",2},{"Drive",2}};
    // These dense arrangements previously discarded every caption. Smaller
    // whole-word captions must fit without shrinking the main controls.
    for(int count:{5,8}) {
      auto subset=labels;subset.erase(subset.begin()+count,subset.end());
      auto cells=beekapoo::layoutParams(subset,count==5?95:125,170,false);
      assert(cells.size()==subset.size());
      for(const auto& c:cells) {
        assert(c.labelWidth>0&&c.labelHeight>=10);
        assert(c.diameter>=32);
        float bottom=c.block>=0?c.captionY+c.labelHeight:c.y+c.diameter*.5f+5+c.labelHeight;
        assert(bottom<=170.01f);
      }
    }
    labels.erase(labels.begin()+5,labels.end());
    auto dense=beekapoo::compactParams(labels,95,142,false);
    assert(dense.size()==5);
    for(const auto& c:dense) {
      assert(c.diameter>=28&&c.labelWidth>0&&c.labelHeight>=24);
      assert(c.y-c.diameter*.5f>=0);
      assert(c.y+c.diameter*.5f+5+c.labelHeight<=142);
    }
  }
  {
    // Families occupy disjoint regions, with one caption and independently
    // sized knobs. Check every visible hitbox/caption, not global grid rows.
    using beekapoo::ParamLayoutItem;
    auto overlap=[](float x,float y,float w,float h,float xx,float yy,float ww,float hh) {
      return x<xx+ww-.02f&&xx<x+w-.02f&&y<yy+hh-.02f&&yy<y+h-.02f;
    };
    for(float width:{35.f,65.f,95.f,155.f,215.f,320.f})
      for(float height:{40.f,100.f,170.f,240.f})for(int count:{1,3,5,8,16,32,64,128})for(bool byModule:{false,true}) {
        std::vector<ParamLayoutItem> items;
        for(int i=0;i<count;++i)items.emplace_back(i%3==0?"Frequency":"Channel "+std::to_string(i)+" CV",i/4);
        auto cells=beekapoo::layoutParams(items,width,height,byModule);
        auto again=beekapoo::layoutParams(items,width,height,byModule);
        assert(cells.size()==items.size()&&again.size()==cells.size());
        std::set<int> indices;
        for(size_t i=0;i<cells.size();++i) {
          const auto& c=cells[i];
          assert(indices.insert(c.index).second);
          assert(c.diameter>0&&c.diameter<=64);
          assert(c.x-c.diameter*.5f>=-.01f&&c.x+c.diameter*.5f<=width+.01f);
          assert(c.y-c.diameter*.5f>=-.01f&&c.y+c.diameter*.5f<=height+.01f);
          assert(c.x==again[i].x&&c.y==again[i].y&&c.diameter==again[i].diameter);
          for(const auto& b:cells)if(c.index!=b.index)
            assert(!overlap(c.x-c.diameter*.5f,c.y-c.diameter*.5f,c.diameter,c.diameter,b.x-b.diameter*.5f,b.y-b.diameter*.5f,b.diameter,b.diameter));
          if(c.block<0||c.labelWidth<=0)continue;
          float left=c.captionX-c.labelWidth*.5f;
          assert(left>=-.01f&&left+c.labelWidth<=width+.01f);
          assert(c.captionY>=0&&c.captionY+c.labelHeight<=height+.01f);
          for(const auto& b:cells) {
            assert(!overlap(left,c.captionY,c.labelWidth,c.labelHeight,b.x-b.diameter*.5f,b.y-b.diameter*.5f,b.diameter,b.diameter));
            if(c.index!=b.index&&b.labelWidth>0)
              assert(!overlap(left,c.captionY,c.labelWidth,c.labelHeight,b.captionX-b.labelWidth*.5f,b.captionY,b.labelWidth,b.labelHeight));
          }
        }
      }
    std::vector<ParamLayoutItem> items{{"Frequency",0},{"Frequency",1},{"Frequency",2},{"Mix",3},{"Time",3}};
    auto cells=beekapoo::layoutParams(items,155,190,false);
    int shared=0;float smallest=64,largest=0;std::set<float> familyRows;
    for(const auto& c:cells){if(c.index<3){familyRows.insert(c.y);shared+=c.labelWidth>0;}smallest=std::min(smallest,c.diameter);largest=std::max(largest,c.diameter);}
    assert(shared==1&&familyRows.size()>1&&largest-smallest>2&&smallest>=40);
    std::vector<ParamLayoutItem> sparse{{"Frequency modulation",0},{"Pulse width modulation",0},{"Cutoff frequency",1},{"Resonance",1}};
    for(float height:{140.f,170.f,200.f}) {
      auto fitted=beekapoo::layoutParams(sparse,95,height,false);
      assert(fitted.size()==sparse.size());
      for(const auto& c:fitted)assert(c.diameter>=30);
    }
  }
  for(unsigned seed=0;seed<1000;++seed) {
    std::mt19937 rng(seed);
    auto empty=beekapoo::defaultExposureCounts(0,rng);
    assert(empty.inputs==0&&empty.parameters==0&&empty.outputs==0);
    for(int n=1;n<=100;++n) {
      auto c=beekapoo::defaultExposureCounts(n,rng);
      assert(c.inputs>=2&&c.inputs<=std::min(8,2+(n+1)/2));
      assert(c.parameters>=3&&c.parameters<=std::min(16,3+n));
      assert(c.outputs>=1&&c.outputs<=std::min(6,(n+1)/2));
    }
  }
  assert(beekapoo::abbreviatedLabel("Cutoff frequency")=="Cutoff Freq");
  assert(beekapoo::abbreviatedLabel("Pulse width modulation")=="PW");
  assert(beekapoo::abbreviatedLabel("Channel 3 frequency")=="Ch3 Freq");
  assert(beekapoo::abbreviatedLabel("High pass filter")=="HP");
  assert(beekapoo::abbreviatedLabel("Highpass filter")=="HP");
  assert(beekapoo::abbreviatedLabel("Lowpass filter")=="LP");
  assert(beekapoo::abbreviatedLabel("Frequency modulation")=="FM");
  assert(beekapoo::abbreviatedLabel("Channel 2 frequency modulation")=="Ch2 FM");
  assert(beekapoo::abbreviatedLabel("Resonance")=="Res");
  assert(beekapoo::labelCase("Cutoff Freq", 0)=="cutoff freq");
  assert(beekapoo::labelCase("Cutoff Freq", 1)=="CUTOFF FREQ");
  {
    using beekapoo::TitleCapitalization;
    int counts[4]{};uint64_t seeds[4]{};
    for(uint64_t seed=0;seed<10000;++seed) {
      int mode=int(beekapoo::titleCapitalization(seed));++counts[mode];seeds[mode]=seed;
      auto name=beekapoo::titleCase("moss / reed-7",seed);
      assert(name==beekapoo::titleCase("moss / reed-7",seed));
      assert(beekapoo::labelCase(name,0)=="moss / reed-7");
    }
    for(int i=0;i<3;++i)assert(counts[i]>2800&&counts[i]<3800);
    assert(counts[3]>100&&counts[3]<300);
    assert(beekapoo::titleCase("Moss / REED-7",seeds[int(TitleCapitalization::Lower)])=="moss / reed-7");
    assert(beekapoo::titleCase("Moss / REED-7",seeds[int(TitleCapitalization::Upper)])=="MOSS / REED-7");
    assert(beekapoo::titleCase("moss / REED-7, fox's den",seeds[int(TitleCapitalization::Title)])=="Moss / Reed-7, Fox's Den");
    assert(beekapoo::titleCase("éclair / λ 42",seeds[int(TitleCapitalization::Upper)])=="éCLAIR / λ 42");
  }
  assert(beekapoo::abbreviatedLabel("CHANNEL 3 CV")=="Ch3 CV");
  {
    auto choices=beekapoo::labelAlternatives("Channel 3 CV");
    assert(std::find(choices.begin(),choices.end(),"CV3")!=choices.end());
  }
  assert(beekapoo::abbreviatedLabel("1V/OCTAVE PITCH")=="Pitch");
  assert(beekapoo::abbreviatedLabel("Unusual control")=="Unusual control");
  for(auto example:std::vector<std::pair<std::string,std::string>>{
      {"Linear frequency modulation","Lin FM"}, {"End of cycle","EOC"},
      {"Right channel 12","R Ch12"}, {"Sample & hold","S&H"},
      {"Threshold","Thresh"},{"Low frequency oscillator","LFO"},
      {"Stereo width","Width"},{"Trigger","Trig"},{"Probability","Prob"}})
    assert(beekapoo::abbreviatedLabel(example.first)==example.second);
  auto has=[](const std::string& name,const std::string& target) {
    auto choices=beekapoo::labelAlternatives(name,true);
    return std::find(choices.begin(),choices.end(),target)!=choices.end();
  };
  assert(has("Channel 12 CV input","CV12"));
  assert(has("FREQUENCY_MODULATION","FM"));
  assert(has("High-pass filter input","HP"));
  assert(has("Left clock input","L Clk"));
  assert(has("Hold - overrides control when patched","Hold"));
  assert(has("Channel #12 frequency","Ch12 Freq"));
  assert(has("Cutoff frequency","Cutoff"));
  for(const auto& label:beekapoo::labelAliases()) {
    assert(!beekapoo::abbreviatedLabel(label.full).empty());
    // Do not replace dictionary fragments inside an unrelated word.
    assert(beekapoo::abbreviatedLabel(std::string("z")+label.full+"z").front()=='z');
  }
  for(float width:{20.f,22.f,24.f,30.f,42.f,80.f})
    for(float height:{64.f,80.f,120.f}) {
      auto cell=beekapoo::planPortCell(width,height,true);
      assert(cell.label.h>=10&&cell.label.w>=14);
      assert(cell.fontSize>=8&&cell.gain.w==20);
    }

  std::set<std::string> destinations;
  std::set<int> counts, mixerOutputs, mixerChannels;
  for(unsigned seed=0;seed<10000;++seed) {
    Planner planner(seed);
    auto p=planner.make();
    assert(planner.validate(p).empty());
    assert(p.modules.size()>=3 && p.modules.size()<=11);
    assert(p.inputs.size()==3 && p.outputs.size()==1);
    counts.insert(int(p.modules.size()));
    for(auto e:p.inputs) destinations.insert(std::string(library()[p.modules[e.node]].slug)+":"+std::to_string(e.port));
    for(auto c:p.cables) {
      if(p.modules[c.from.node]==7) mixerOutputs.insert(c.from.port);
      if(p.modules[c.to.node]==7&&c.to.port>=1&&c.to.port<=4)mixerChannels.insert(c.to.port);
    }
    // Independent checks of the musically relevant channels and waveforms.
    auto inputUsed=[&](int node,int port){for(auto c:p.cables)if(c.to==Endpoint{node,port})return true;return false;};
    auto outputUsed=[&](int node,int port){for(auto e:p.outputs)if(e==Endpoint{node,port})return true;for(auto c:p.cables)if(c.from==Endpoint{node,port})return true;return false;};
    for(auto e:p.parameters) {
      auto type=p.modules[e.node];
      if((type==0||type==1)&&e.port==5)assert(outputUsed(e.node,3));
      if(type==7&&e.port>0)assert(inputUsed(e.node,e.port));
    }
    auto checkInput=[&](Endpoint e) {
      auto type=p.modules[e.node];
      if((type==0||type==1)&&e.port==3)assert(outputUsed(e.node,3));
      if(type==7&&e.port>=5)assert(inputUsed(e.node,e.port-4));
    };
    for(auto e:p.inputs)checkInput(e);
    for(auto c:p.cables)checkInput(c.to);
    if(p.modules[p.outputs[0].node]==7)mixerOutputs.insert(p.outputs[0].port);
    // Removing a required input must fail even though the module still has
    // an output cable and can have CV patched into it.
    for(size_t i=0;i<p.cables.size();++i) {
      auto c=p.cables[i]; bool required=false;
      for(const auto& o:library()[p.modules[c.to.node]].outputs)
        for(const auto& r:o.requirements)
          required |= std::find(r.anyOf.begin(),r.anyOf.end(),c.to.port)!=r.anyOf.end();
      if(required) { auto broken=p;broken.cables.erase(broken.cables.begin()+i);assert(!planner.validate(broken).empty()); }
    }
  }
  assert(counts.size()==9 && destinations.size()>15 && mixerOutputs==std::set<int>{0} && mixerChannels.size()==4);
  Planner planner(1);
  Plan mixer; mixer.modules={0,7};mixer.outputs={{1,2}};mixer.cables={{{0,0},{1,5}}};
  assert(!planner.validate(mixer).empty()); // CV is not a signal input.
  mixer.cables={{{0,0},{1,1}}};assert(!planner.validate(mixer).empty()); // Wrong direct channel.
  mixer.cables={{{0,0},{1,2}}};assert(planner.validate(mixer).empty());
  mixer.outputs={{1,0}};assert(planner.validate(mixer).empty()); // Any signal channel feeds the sum.
  mixer.parameters={{1,1}};assert(!planner.validate(mixer).empty());
  mixer.parameters={{1,2}};assert(planner.validate(mixer).empty());
  mixer.inputs={{1,5}};assert(!planner.validate(mixer).empty());
  mixer.inputs={{1,6}};assert(planner.validate(mixer).empty());
  // PWM input/parameter are both conditional on the square output.
  for(int type:{0,1}) {
    Plan oscillator;oscillator.modules={type};oscillator.outputs={{0,0}};
    oscillator.parameters={{0,5}};assert(!planner.validate(oscillator).empty());
    oscillator.outputs={{0,3}};assert(planner.validate(oscillator).empty());
    oscillator.inputs={{0,3}};assert(planner.validate(oscillator).empty());
    oscillator.parameters.clear();oscillator.outputs={{0,1}};
    assert(!planner.validate(oscillator).empty());
  }
  for (ExposureCounts counts : {ExposureCounts{0,0,0}, {1,1,1}, {3,1,4}, {5,4,10}, {16,8,32}, {3,0,5}})
    for (unsigned seed=1;seed<=60;++seed) {
      Planner exact(seed);
      auto p=exact.make(0, &counts);
      assert(!p.modules.empty() && exact.validate(p).empty());
      assert(int(p.inputs.size())==counts.inputs && int(p.outputs.size())==counts.outputs && int(p.parameters.size())==counts.parameters);
      for (auto* endpoints : {&p.inputs, &p.outputs, &p.parameters}) {
        std::set<std::pair<int,int>> unique;
        for (auto e:*endpoints) assert(unique.insert({e.node,e.port}).second);
      }
    }
  std::cout << "Beekapoo planner: 10000 patches, required inputs and output reachability passed\n";
}
