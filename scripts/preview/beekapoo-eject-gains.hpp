#pragma once
template<class Add,class Require>
void testBeekEjectGains(Add add,Require require,plugin::Model* gainModel) {
  auto* rack=APP->scene->rack;
  auto* adapterModel=plugin::getModel("Fundamental","8vert");require(adapterModel,"8vert must be installed in the isolated test");
  for(int attenuated:{0,1,2,8,9,10,17}) {
    rack->clear();APP->history->clear();
    auto* source=add(gainModel,Vec(2400,0));
    auto* child=add(gainModel,Vec());auto childId=child->module->id;
    auto* owner=static_cast<beekapoo::Beekapoo*>(add(modelBeekapooPreview,Vec(240,0)));auto ownerId=owner->module->id;
    rack->deselectAll();rack->select(child,true);owner->collapse();
    auto wire=[&](app::PortWidget* from,app::PortWidget* to) {
      auto* c=new engine::Cable;c->outputModule=from->module;c->outputId=from->portId;c->inputModule=to->module;c->inputId=to->portId;
      APP->engine->addCable(c);auto* w=new app::CableWidget;w->setCable(c);rack->addCable(w);return c->id;
    };
    std::vector<int64_t> incoming;std::vector<float> gains;
    for(int i=0;i<attenuated+2;++i) {
      owner->expose(child,child->getInput(i));
      auto& port=owner->ports.back();float gain=i>=attenuated?1.f:(i%3==0?.5f:i%3==1?-.3f:0.f);
      gains.push_back(gain);legacyBeekGain(owner,port)->setImmediateValue(gain);
      if(i!=attenuated-1)incoming.push_back(wire(source->getOutput(0),owner->findPort(port)));
    }
    // Two sources stacked with feedback on the same exposed input.
    if(attenuated>1) {
      auto* proxy=owner->findPort(owner->ports.front());
      incoming.push_back(wire(source->getOutput(0),proxy));
      auto id=wire(child->getOutput(0),proxy);incoming.push_back(id);owner->panelCableIds.insert(id);
    }
    owner->saveGroup();APP->engine->stepBlock(2048);
    std::vector<float> voltages;std::vector<int> channels;
    for(int i=0;i<attenuated+2;++i){channels.push_back(child->module->inputs[i].getChannels());for(int c=0;c<4;++c)voltages.push_back(child->module->inputs[i].getVoltage(c));}
    owner->ejectAction();
    auto verify=[&] {
      std::vector<app::ModuleWidget*> adapters;
      for(auto* w:rack->getModules())if(w->model==adapterModel)adapters.push_back(w);
      require(adapters.size()==size_t((std::max(0,attenuated-1)+7)/8),"Eject must fill eight 8vert channels before adding another; unity gains need none");
      // Identical sources use one patched row per run; each new 8vert starts
      // with a real input even when the preceding one had the same sources.
      for(auto* adapter:adapters) {
        bool previous=false;
        for(int ch=0;ch<8;++ch) {
          if(!rack->getCompleteCablesOnPort(adapter->getInput(ch)).empty())previous=true;
          if(!rack->getCompleteCablesOnPort(adapter->getOutput(ch)).empty())require(previous,"Each normalized 8vert run must start with a patched input");
        }
      }
      APP->engine->stepBlock(2048);child=rack->getModule(childId);
      for(int i=0;i<attenuated+2;++i) {
        require(child->module->inputs[i].getChannels()==channels[i],"8vert must preserve polyphony and unpatched normalization");
        for(int c=0;c<4;++c)require(std::abs(child->module->inputs[i].getVoltage(c)-voltages[i*4+c])<.001f,"Eject must preserve attenuated, inverted, zero and unity signals with stacked feedback");
      }
      for(auto* adapter:adapters)require(rack->isSelected(adapter),"Ejected 8vert modules must be selected alongside the subpatch");
    };
    verify();
    for(int repeat=0;repeat<2;++repeat) {
      APP->history->undo();owner=dynamic_cast<beekapoo::Beekapoo*>(rack->getModule(ownerId));
      require(owner&&owner->members.size()==1,"Undo must restore the Beek and remove only its 8vert adapters");owner->step();
      for(auto* w:rack->getModules())require(w->model!=adapterModel,"Undo must remove generated 8vert modules");
      for(auto id:incoming)require(APP->engine->getCable(id),"Undo must restore every original cable, including redundant normalized sources");
      APP->history->redo();require(!rack->getModule(ownerId),"Redo must eject again");verify();
    }

  }
  rack->clear();APP->history->clear();
  std::puts("Eject 8vert: unity/unpatched bypass, eight-channel packing, inversion/zero, polyphony, stacking, feedback, normalization and Undo/Redo passed");
}
