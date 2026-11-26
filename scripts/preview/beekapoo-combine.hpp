#pragma once
template <class Add, class Require>
void testBeekCombine(Add add, Require require, plugin::Model* gainModel) {
  auto* rack = APP->scene->rack;
  for (int scenario = 0; scenario < 5; ++scenario) {
    bool absorb = scenario >= 3;
    int variant = absorb ? scenario - 2 : scenario;
    rack->clear(); APP->history->clear();
    auto make = [&](float x) {
      auto* a = add(gainModel, Vec(x,0));
      auto* b = add(gainModel, Vec(x+90,0));
      auto* owner = static_cast<beekapoo::Beekapoo*>(add(modelBeekapooPreview,Vec(x+240,0)));
      rack->deselectAll(); rack->select(a,true); rack->select(b,true); owner->collapse();
      owner->exposeParameter(a->module->id,3);
      owner->expose(a,a->getInput(0)); owner->expose(b,b->getOutput(0));
      for (auto& p:owner->ports) if(p.input) legacyBeekGain(owner,p)->setImmediateValue(.37f);
      owner->saveGroup(); return owner;
    };
    auto* first=make(0); auto firstId=first->module->id;
    auto* second=variant ? make(1200) : nullptr; auto secondId=second?second->module->id:-1;
    first->toggleRandomLock(first->members.front().id,0);
    if(second)second->toggleRandomLock(second->members.front().id,-1);
    auto expectedLocks=first->randomLocks;if(second)expectedLocks.insert(second->randomLocks.begin(),second->randomLocks.end());
    auto* ordinary=add(gainModel,Vec(2500,0)); auto ordinaryId=ordinary->module->id;
    auto* outside=add(gainModel,Vec(3000,0)); auto outsideId=outside->module->id;
    auto endpoint=[](beekapoo::Beekapoo* owner,bool input) {
      for(const auto& p:owner->ports) if(p.input==input)return owner->findPort(p);
      return static_cast<app::PortWidget*>(nullptr);
    };
    auto wire=[&](app::PortWidget* from,app::PortWidget* to) {
      auto* c=new engine::Cable;c->outputModule=from->module;c->outputId=from->portId;c->inputModule=to->module;c->inputId=to->portId;
      APP->engine->addCable(c);auto* cw=new app::CableWidget;cw->setCable(c);rack->addCable(cw);return c->id;
    };
    // Stacked exterior sources plus a loopback through the SAME exposed input.
    wire(outside->getOutput(0),endpoint(first,true));
    wire(ordinary->getOutput(0),endpoint(first,true));
    auto loop=wire(endpoint(first,false),endpoint(first,true));first->panelCableIds.insert(loop);
    wire(endpoint(first,false),outside->getInput(0));
    if(second) { wire(endpoint(first,false),endpoint(second,true)); wire(endpoint(second,false),endpoint(first,true)); }
    auto graph=[&] {
      std::map<int64_t,std::array<int64_t,4>> result;
      for(auto* cw:rack->getCompleteCables()) {
        auto* c=cw->cable;bool bridge=false;int64_t target=c->inputModule->id;int port=c->inputId;
        for(auto* w:rack->getModules()) if(auto* owner=dynamic_cast<beekapoo::Beekapoo*>(w)) {
          bridge|=owner->inputBridge(c);
          if(c->inputModule==owner->module) for(const auto& p:owner->ports)
            if(p.route>=0&&port==beekapoo::Module::inputBase+p.route) {target=p.id;port=p.portId;break;}
        }
        if(!bridge)result[c->id]={{c->outputModule->id,c->outputId,target,port}};
      }
      return result;
    };
    std::set<int64_t> expected;
    for(const auto& m:first->members)expected.insert(m.id);
    if(second)for(const auto& m:second->members)expected.insert(m.id);
    if(variant!=1)expected.insert(ordinaryId);
    auto before=graph();
    if(variant==2)first->editSubpatch();
    rack->deselectAll();rack->select(first,true);
    if(second)rack->select(second,true);
    if(variant!=1)rack->select(ordinary,true);
    if(variant==2)rack->select(rack->getModule(first->members.front().id),true);
    require(beekapoo::canCreateSelectedGroup(),"Selected Beeks and ordinary modules must be combinable");
    if (absorb) {
      require(first->canAddSelection() && second->canAddSelection(),"Both selected Beeks must offer absorbing the other");
      first->addSelectionButton->action();
    } else beekapoo::createSelectedGroup();
    beekapoo::Beekapoo* combined=nullptr;
    for(auto* w:rack->getSelected())if(auto* owner=dynamic_cast<beekapoo::Beekapoo*>(w))combined=owner;
    require(combined && (absorb ? combined->module->id==firstId : !rack->getModule(firstId)) && (!second||!rack->getModule(secondId)),"Combine must keep the chosen owner only when absorbing through its logo");
    auto combinedId=combined->module->id;
    for(int repeat=0;repeat<3;++repeat) {
      std::set<int64_t> actual;for(const auto& m:combined->members)actual.insert(m.id);
      float left=INFINITY,right=-INFINITY,width=0,y=combined->members.front().position.y;
      for(const auto& m:combined->members){left=std::min(left,m.position.x);right=std::max(right,m.position.x+m.widget->box.size.x);width+=m.widget->box.size.x;require(m.position.y==y,"Small combined subpatches must occupy one row");}
      require(std::abs(right-left-width)<.01f,"Combining distant groups must close gaps into a solid rectangular row");
      require(combined->randomLocks==expectedLocks,"Merges preserve sequence locks through Undo/Redo");
      require(actual==expected&&graph()==before,"Combine must preserve module IDs, stacked cables, cross-Beek cables, loopbacks and exterior wires");
      require(rack->getModule(outsideId)&&!combined->contains(outside->module),"Unselected modules must remain outside");
      int gains=0;for(const auto& p:combined->ports)if(p.input&&std::abs(combined->module->params[beekapoo::Module::INPUT_GAIN_BASE+p.route].getValue()-.37f)<.001f)++gains;
      require(gains>=(second?2:1),"Combine must retain exposed input gains");
      APP->history->undo();
      require((absorb || !rack->getModule(combinedId))&&rack->getModule(firstId)&&(!second||rack->getModule(secondId))&&graph()==before,"One Undo must restore every original Beek and connection");
      APP->history->redo();combined=dynamic_cast<beekapoo::Beekapoo*>(rack->getModule(combinedId));
      require(combined,"Redo must restore combined owner");combined->step();
    }
  }
  rack->clear();APP->history->clear();
  std::puts("Combine Beeks: ordinary/Beek mixtures, stacked cables, loopbacks, inter-Beek wiring, gains and repeated Undo/Redo passed");
}
