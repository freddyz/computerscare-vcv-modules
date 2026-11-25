#pragma once
template<class Add,class Require>
void renderBeekPolyphony(const std::string& output,float scale,Add add,Require require) {
  using beekapoo::Module;
  Module state; state.model=modelBeekapooPreview;
  static_assert(Module::INPUT_GAIN_BASE==2 && Module::RANDOM_RANGE==514,"Removing Poly/Spread must preserve the input gain IDs");
  state.enableInput(0,true);
  auto& input=state.inputs[Module::inputBase];auto& routed=state.outputs[0];
  for(int incoming:{0,1,2,3,7,16,1,0})for(float atten:{-1.f,0.f,.7f,1.f}) {
    state.params[Module::INPUT_GAIN_BASE].setValue(atten);
    input.channels=incoming;
    for(int c=0;c<16;++c)input.setVoltage(c<incoming?float(c+1):0,c);
    state.process({});
    require(routed.getChannels()==incoming,"Exposed inputs must preserve native channel counts and normalization");
    for(int c=0;c<16;++c)
      require(std::abs(routed.getVoltage(c)-(c<incoming?(c+1)*atten:0.f))<.00001f,"Attenuation must preserve every native lane and clear inactive ones");
  }
  // A polyphonic source in the real engine, through an exposed input, a
  // Fundamental 8vert, an exposed output, and an outside consumer.
  struct Source : engine::Module {
    int count=1;
    Source(){config(0,0,1,0);}
    void process(const ProcessArgs&) override {
      outputs[0].setChannels(count);
      for(int c=0;c<count;++c)outputs[0].setVoltage(float(c+1),c);
    }
  };
  struct SourceWidget : app::ModuleWidget {
    explicit SourceWidget(Source* m){setModule(m);box.size=Vec(30,380);addOutput(createOutput<JackSmallPointingDown>(Vec(10,100),m,0));}
  };
  std::unique_ptr<plugin::Model> sourceModel(createModel<Source,SourceWidget>("beek-native-poly-source"));
  sourceModel->plugin=pluginInstance;
  auto* rack=APP->scene->rack;
  auto* utility=plugin::getModel("Fundamental","8vert");
  require(utility,"Native poly fixture needs Fundamental 8vert");
  for(int count:{1,7,16}) {
    rack->clear();APP->history->clear();
    auto* source=add(sourceModel.get(),Vec(1800,0));static_cast<Source*>(source->module)->count=count;
    auto* child=add(utility,Vec());auto childId=child->module->id;
    auto* sink=add(utility,Vec(2100,0));
    for(int i=0;i<3;++i)child->module->params[i].setValue(1.f);
    for(int i=0;i<2;++i) {
      auto* c=new engine::Cable;c->outputModule=child->module;c->outputId=i;c->inputModule=sink->module;c->inputId=i;
      APP->engine->addCable(c);auto* cw=new app::CableWidget;cw->setCable(c);rack->addCable(cw);
    }
    auto* owner=static_cast<beekapoo::Beekapoo*>(add(modelBeekapooPreview,Vec(300,0)));
    auto ownerId=owner->module->id;
    rack->deselectAll();rack->select(child,true);owner->collapse();
    for(int i=0;i<3;++i)owner->expose(child,child->getInput(i));
    for(auto& p:owner->ports)if(p.input) {
      legacyBeekGain(owner,p)->setImmediateValue(p.portId==1?-.5f:1.f);
      if(p.portId==2)continue;
      auto* endpoint=owner->findPort(p);auto* c=new engine::Cable;
      c->outputModule=source->module;c->outputId=0;c->inputModule=endpoint->module;c->inputId=endpoint->portId;
      APP->engine->addCable(c);auto* cw=new app::CableWidget;cw->setCable(c);rack->addCable(cw);
    }
    owner->saveGroup();
    auto verify=[&] {
      APP->engine->stepBlock(64);child=rack->getModule(childId);
      for(int i=0;i<3;++i) {
        require(child->module->inputs[i].getChannels()==(i<2?count:0),"Native cables must retain polyphony and unpatched normalization");
        if(i==2)continue;
        require(sink->module->inputs[i].getChannels()==count,"The exposed output must really deliver polyphony to an outside module");
        for(int c=0;c<count;++c)
          require(std::abs(sink->module->inputs[i].getVoltage(c)-(c+1)*(i==1?-.5f:1.f))<.0001f,"Every output lane must retain its voltage and input attenuation");
      }
    };
    verify();owner->ejectAction();verify();
    for(auto* w:rack->getModules())require(w->model!=modelComputerscareBeekapooAudioPorts,"Eject must not leave input-spread helpers");
    APP->history->undo();owner=dynamic_cast<beekapoo::Beekapoo*>(rack->getModule(ownerId));
    require(owner,"Undo must restore the container");owner->step();verify();
    APP->history->redo();verify();
  }
  rack->clear();APP->history->clear();
  for(int example=0;example<4;++example) {
    rack->clear();APP->history->clear();
    auto* owner=static_cast<beekapoo::Beekapoo*>(add(modelBeekapooPreview,Vec()));
    require(owner->generateDefaultPatch(3+example),"Native poly fixture must use real modules");
    auto* m=owner->getModule<Module>();m->panelTheme.store(example%2?2:1);
    float widths[]={90,120,180,285};owner->requestedWidth=widths[example];owner->layoutControls();owner->step();
    for(const auto& label:owner->controlLabels)require(label.text!="Spread"&&label.text!="Poly","Removed controls must not reserve labels on the panel");
    auto* block=example%3==0?owner->frame:example%3==1?owner->inputFrame:owner->outputFrame;
    owner->updateSectionAddButtons(block->box.getCenter());
    auto* parent=owner->parent;auto position=owner->box.pos;parent->removeChild(owner);owner->box.pos=Vec();
    capture(owner,output+string::f("/native-%d.png",example+1),scale,false);
    owner->box.pos=position;parent->addChild(owner);
  }
  rack->clear();APP->history->clear();
  std::puts("Beek native polyphony: mono/7/16 channels through real exposed input/output cables, attenuation, normalization and Eject Undo/Redo passed");
}
