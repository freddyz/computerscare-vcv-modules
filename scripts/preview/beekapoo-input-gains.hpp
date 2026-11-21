#pragma once
template<class Add,class Require>
void testBeekInputGains(Add add,Require require) {
  auto* rack=APP->scene->rack;
  auto* utility=plugin::getModel("Fundamental","8vert");
  require(utility,"Input-gain regression needs Fundamental 8vert");
  for(int scenario=0;scenario<5;++scenario) {
    rack->clear();APP->history->clear();
    auto* child=add(utility,Vec());auto childId=child->module->id;
    auto* owner=static_cast<beekapoo::Beekapoo*>(add(modelBeekapooPreview,Vec(240,0)));
    auto ownerId=owner->module->id;
    rack->deselectAll();rack->select(child,true);owner->collapse();
    for(int i=0;i<3;++i)owner->expose(child,child->getInput(i));
    owner->exposeParameter(childId,0);owner->expose(child,child->getOutput(0));
    struct Gain {int input,route,param;float value;};std::vector<Gain> gains;
    float values[]={-.37f,.23f,0.f};
    for(auto& p:owner->ports)if(p.input) {
      auto* q=p.gain->getParamQuantity();q->setValue(values[p.portId]);
      gains.push_back({p.portId,p.route,q->paramId,values[p.portId]});
    }
    // Deliberately do not saveGroup after turning a knob: normal knob edits
    // don't rebuild the group's structural JSON.
    auto verify=[&](const char* phase) {
      owner=dynamic_cast<beekapoo::Beekapoo*>(rack->getModule(ownerId));require(owner,"Original owner must survive");
      for(const auto& saved:gains) {
        bool found=false;
        for(const auto& p:owner->ports)if(p.id==childId&&p.input&&p.portId==saved.input) {
          found=true;auto* q=p.gain->getParamQuantity();
          require(p.route==saved.route&&q->paramId==saved.param,"Existing gain/port IDs must not change when adding controls or modules");
          if(std::abs(q->getValue()-saved.value)>.00001f)
            throw std::runtime_error(std::string("Input gain changed during ")+phase+": expected "+std::to_string(saved.value)+", got "+std::to_string(q->getValue()));
        }
        require(found,"Existing exposed input must remain mapped");
      }
    };
    verify("setup");
    if(scenario==0) {
      auto* saved=owner->toJson();owner->fromJson(saved);json_decref(saved);owner->step();
      verify("save/reload after a knob edit");
    } else if(scenario==1) {
      auto* added=add(utility,Vec(600,0));rack->deselectAll();rack->select(added,true);owner->addSelection();owner->step();
      verify("adding a module");APP->history->undo();owner->step();verify("Undo add");APP->history->redo();owner->step();verify("Redo add");
    } else if(scenario==2) {
      for(int kind=0;kind<3;++kind) {
        require(owner->addExposedControl(kind),"Section plus must expose a new control");verify("exposure add");
        APP->history->undo();owner->step();verify("Undo exposure");APP->history->redo();owner->step();verify("Redo exposure");
      }
    } else if(scenario==3) {
      owner->editSubpatch();auto* added=add(utility,Vec(900,380));auto* session=static_cast<beekapoo::RackEditSession*>(owner->editSession.get());
      session->adoptNewModules();verify("adding inside Edit");require(owner->contains(added->module),"New module must join the subpatch");
      session->finish(true);owner->step();verify("Done editing");
      APP->history->undo();owner->step();verify("Undo Edit add");APP->history->redo();owner->step();verify("Redo Edit add");
    } else {
      auto* otherChild=add(utility,Vec(1000,0));auto* other=static_cast<beekapoo::Beekapoo*>(add(modelBeekapooPreview,Vec(1200,0)));
      rack->deselectAll();rack->select(otherChild,true);other->collapse();other->expose(otherChild,otherChild->getInput(0));
      auto otherId=otherChild->module->id;
      other->ports.front().gain->getParamQuantity()->setValue(-.61f);
      rack->deselectAll();rack->select(other,true);owner->addSelection();owner->step();verify("merging a Beek");
      bool copied=false;for(const auto& p:owner->ports)if(p.id==otherId&&p.input)copied=std::abs(p.gain->getParamQuantity()->getValue()+.61f)<.00001f;
      require(copied,"Merge must retain the incoming knob's smoothing target, not its old instantaneous value");
      APP->history->undo();owner->step();verify("Undo merge");APP->history->redo();owner->step();verify("Redo merge");
    }
  }
  rack->clear();APP->history->clear();
  std::puts("Input attenuverter values/IDs: unsaved knob edits, reload, add, section exposure, Edit adoption, merge and Undo/Redo passed");
}
