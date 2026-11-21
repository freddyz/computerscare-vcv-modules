#include "BeekapooPatchPlanner.hpp"
#include <cassert>
#include <iostream>
#include <set>
using namespace beekapoo_patch;
int main() {
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
        for(const auto& r:o.requires)
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
