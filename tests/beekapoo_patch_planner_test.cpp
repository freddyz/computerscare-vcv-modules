#include "BeekapooPatchPlanner.hpp"
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
    // A sparse output beside two input columns must get useful space and a
    // caption, not an anonymous miniature jack at the top of an empty box.
    auto panes=beekapoo::portPaneSpace(74,2,1,1);
    assert(panes.first>=40&&panes.second>=34&&panes.first+panes.second==74);
    auto out=beekapoo::planPortCell(panes.second+5,96,false);
    assert(out.large&&out.jack.w>=30&&out.label.h>=10);
    auto large=beekapoo::planPortCell(90,110,false);
    assert(large.jack.w>out.jack.w&&large.jack.w<=40.5f&&large.fontSize>out.fontSize-1);
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
        assert(c.labelWidth>0&&c.labelHeight>=10&&c.labelScale<1);
        assert(c.diameter>=32);
        assert(c.y+c.diameter*.5f+5+c.labelHeight<=170);
        for(const auto& next:cells)if(next.y>c.y)
          assert(c.y+c.diameter*.5f+5+c.labelHeight<=next.y-next.diameter*.5f);
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
  assert(beekapoo::labelAliases().size()==33);
  assert(beekapoo::abbreviatedLabel("Cutoff frequency")=="Cutoff");
  assert(beekapoo::abbreviatedLabel("Pulse width modulation")=="PW");
  assert(beekapoo::abbreviatedLabel("Channel 3 frequency")=="Channel 3 Freq");
  assert(beekapoo::abbreviatedLabel("High pass filter")=="HP");
  assert(beekapoo::abbreviatedLabel("Highpass filter")=="HP");
  assert(beekapoo::abbreviatedLabel("Lowpass filter")=="LP");
  assert(beekapoo::abbreviatedLabel("Unusual control")=="Unusual control");
  std::set<std::string> destinations;
  std::set<int> counts, mixerOutputs;
  for(unsigned seed=0;seed<10000;++seed) {
    Planner planner(seed);
    auto p=planner.make();
    assert(planner.validate(p).empty());
    assert(p.modules.size()>=3 && p.modules.size()<=11);
    assert(p.inputs.size()==3 && p.outputs.size()==1);
    counts.insert(int(p.modules.size()));
    for(auto e:p.inputs) destinations.insert(std::string(library()[p.modules[e.node]].slug)+":"+std::to_string(e.port));
    for(auto c:p.cables) if(p.modules[c.from.node]==7) mixerOutputs.insert(c.from.port);
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
  assert(counts.size()==9 && destinations.size()>15 && mixerOutputs.size()==5);
  Planner planner(1);
  Plan mixer; mixer.modules={0,7};mixer.outputs={{1,2}};mixer.cables={{{0,0},{1,5}}};
  assert(!planner.validate(mixer).empty()); // CV is not a signal input.
  mixer.cables={{{0,0},{1,1}}};assert(!planner.validate(mixer).empty()); // Wrong direct channel.
  mixer.cables={{{0,0},{1,2}}};assert(planner.validate(mixer).empty());
  mixer.outputs={{1,0}};assert(planner.validate(mixer).empty()); // Any signal channel feeds the sum.
  std::cout << "Beekapoo planner: 10000 patches, required inputs and output reachability passed\n";
}
