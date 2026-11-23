#pragma once
// Exercise presentation changes against native engine state, including a
// mapped toggle and non-default quantity policies. No audio device is opened.
template<class Add, class Require>
void testBeekExposureState(const std::string& output,float scale,Add add,Require require) {
  auto* rack=APP->scene->rack;
  rack->clear();APP->history->clear();
  auto* model=plugin::getModel("computerscare","demo-gain");
  auto* child=add(model,Vec());auto* source=add(model,Vec(90,0));
  auto* outside=add(model,Vec(900,0));
  auto wire=[&](app::ModuleWidget* from,int out,app::ModuleWidget* to,int in) {
    auto* c=new engine::Cable;c->outputModule=from->module;c->outputId=out;
    c->inputModule=to->module;c->inputId=in;APP->engine->addCable(c);
    auto* cw=new app::CableWidget;cw->setCable(c);rack->addCable(cw);
  };
  wire(outside,0,child,0);wire(outside,0,child,0);wire(source,0,child,0);
  wire(child,0,source,1); // Internal feedback must remain exactly the same wire.
  child->module->params[0].setValue(.63f);
  child->module->params[2].setValue(1.f);
  child->module->params[3].setValue(2.f);
  child->module->getParamQuantity(0)->smoothEnabled=false;
  child->module->getParamQuantity(2)->snapEnabled=false;
  child->module->getParamQuantity(2)->smoothEnabled=true;
  child->module->getParamQuantity(2)->randomizeEnabled=false;
  auto* owner=static_cast<beekapoo::Beekapoo*>(add(modelBeekapooPreview,Vec(300,0)));
  engine::ParamHandle handle;APP->engine->addParamHandle(&handle);
  APP->engine->updateParamHandle(&handle,child->module->id,2,true);
  struct QuantityState {float value;bool snap,smooth,reset,randomize;};
  std::vector<QuantityState> before;
  for(auto* q:child->module->paramQuantities)
    before.push_back({q->getValue(),q->snapEnabled,q->smoothEnabled,q->resetEnabled,q->randomizeEnabled});
  using Ends=std::tuple<int64_t,int,int64_t,int>;
  auto wires=[&] {std::map<int64_t,Ends> result;
    for(auto* w:rack->getCompleteCables()){auto* c=w->cable;result[c->id]=Ends(c->outputModule->id,c->outputId,c->inputModule->id,c->inputId);}return result;
  };
  auto originalWires=wires();
  auto* originalState=child->module->toJson();
  APP->engine->stepBlock(64);float signal=child->module->outputs[0].getVoltage(3);
  auto verify=[&] {
    owner->step();
    for(size_t i=0;i<before.size();++i){auto* q=child->module->getParamQuantity(i);auto b=before[i];
      require(q->getValue()==b.value,"Exposure must never write a native parameter, including toggles");
      require(q->snapEnabled==b.snap&&q->smoothEnabled==b.smooth&&q->resetEnabled==b.reset&&q->randomizeEnabled==b.randomize,"Exposure must preserve the source quantity's policies");
    }
    auto* state=child->module->toJson();
    for(auto* key:{"params","data","bypassed"})
      require((!json_object_get(state,key)&&!json_object_get(originalState,key))||json_equal(json_object_get(state,key),json_object_get(originalState,key)),"Exposure must not mutate inner module data");
    json_decref(state);

    require(wires()==originalWires,"Creating/exposing must not add, remove, retarget or delay native cables");
    require(handle.module==child->module&&handle.paramId==2,"Existing MIDI mapping must still target the original parameter");
    APP->engine->stepBlock(1);
    require(child->module->outputs[0].getChannels()==4&&child->module->outputs[0].getVoltage(3)==signal,"Exposure must preserve the signal immediately, without settling a new bridge");
  };
  rack->deselectAll();rack->select(child,true);rack->select(source,true);
  owner->groupAction(true,true);verify();
  for(auto* p:child->getParams()){owner->toggleParameter(p);verify();owner->toggleParameter(p);verify();}
  for(int repeat=0;repeat<12;++repeat)for(int kind=0;kind<3;++kind) {
    owner->addExposedControl(kind);verify();
    owner->removeExposedControl(kind);verify();
    owner->rerollExposedControls(kind);verify();
  }
  for(const auto& p:owner->ports)if(p.input)
    require(p.route<0&&owner->findPort(p)==owner->originalPort(p),"Ordinary inputs must expose the actual native jack");
  APP->engine->removeParamHandle(&handle);json_decref(originalState);
  for(int example=0;example<2;++example) {
    owner->requestedWidth=example?180:120;
    owner->getModule<beekapoo::Module>()->panelTheme.store(example?2:1);
    owner->layoutControls();owner->step();
    auto* parent=owner->parent;auto pos=owner->box.pos;parent->removeChild(owner);owner->box.pos=Vec();
    capture(owner,output+"/exposure-state-"+std::to_string(example)+".png",scale,false);owner->box.pos=pos;parent->addChild(owner);
  }
  rack->clear();APP->history->clear();
  std::puts("Exposure state: values, toggle/knob policies, inner data, MIDI target, immediate poly signal and exact stacked/feedback cable graph passed");
}
