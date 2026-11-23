#pragma once
template<class Require> void checkBeekCablePresentation(Require require);
template<class Add,class Require>
void testBeekRegenerate(Add add,Require require) {
  auto* rack=APP->scene->rack;
  using Counts=beekapoo_patch::ExposureCounts;
  for(Counts wanted:{Counts{0,0,0},{1,1,1},{3,1,4},{5,4,10},{3,0,5}}) {
    rack->clear();APP->history->clear();
    auto* owner=static_cast<beekapoo::Beekapoo*>(add(modelBeekapooPreview,Vec()));
    auto* m=owner->getModule<beekapoo::Module>();
    auto* root=beekapoo::generated::plan(0,&wanted);require(root,"Exact generation must find enough useful controls");
    require(owner->importContents(root),"Exact generation must import");json_decref(root);
    m->setPatchName(beekapoo::generated::name());owner->refreshGeneratedName();
    auto counts=[&] {
      Counts result{0,0,int(owner->parameters.size())};
      for(const auto& p:owner->ports)++(p.input?result.inputs:result.outputs);
      return result;
    };
    auto verify=[&] {auto c=counts();require(c.inputs==wanted.inputs&&c.outputs==wanted.outputs&&c.parameters==wanted.parameters,"Double dice must preserve exact exposed counts, including zero");};
    auto* source=add(plugin::getModel("Fundamental","LFO"),Vec(4000,0));
    auto* sink=add(plugin::getModel("Fundamental","8vert"),Vec(4300,0));
    auto wire=[&](app::PortWidget* from,app::PortWidget* to) {
      auto* c=new engine::Cable;c->outputModule=from->module;c->outputId=from->portId;c->inputModule=to->module;c->inputId=to->portId;
      APP->engine->addCable(c);auto* cw=new app::CableWidget;cw->setCable(c);rack->addCable(cw);return cw;
    };
    std::vector<std::tuple<int,int,std::string>> selfWires;
    auto slot=[&](int index,bool input) -> app::PortWidget* {
      for(const auto& p:owner->ports)if(p.input==input && index--==0)return owner->findPort(p);
      return nullptr;
    };
    auto selfWire=[&](int out,int in) {
      auto* cw=wire(slot(out,false),slot(in,true));
      cw->color=nvgRGB(80+int(selfWires.size())*20,110,180);
      selfWires.push_back({out,in,color::toHexString(cw->color)});
    };
    if(wanted.inputs&&wanted.outputs) {
      selfWire(0,0);selfWire(0,0);
      selfWire(wanted.outputs-1,wanted.inputs-1);
    }
    int next=0;
    for(const auto& p:owner->ports)if(p.input){wire(source->getOutput(0),owner->findPort(p));wire(source->getOutput(0),owner->findPort(p));}
      else wire(owner->findPort(p),sink->getInput(next++));
    owner->step();owner->refreshGeneratedName();auto name=m->getPatchName();
    // Exterior patches and panel geometry must not rename a generated title.
    for(int i=0;i<20;++i)owner->step();
    require(m->getPatchName()==name,"Exterior cables must not rename the patch");
    auto exterior=[&] {
      int ins=0,outs=0;
      for(auto* cw:rack->getCompleteCables()) {
        ins+=cw->cable->outputModule==source->module && (cw->cable->inputModule==m || owner->contains(cw->cable->inputModule));
        outs+=cw->cable->inputModule==sink->module && owner->contains(cw->cable->outputModule);
      }
      require(ins==wanted.inputs*2&&outs==wanted.outputs,"Regeneration must retain every exterior cable, including stacked inputs");
      auto expected=selfWires;
      std::vector<std::tuple<int,int,std::string>> found;
      for(auto* cw:rack->getCompleteCables())
        if(owner->cableRole(cw->cable)==beekapoo::Beekapoo::CableRole::Panel)
          for(int out=0;out<wanted.outputs;++out)for(int in=0;in<wanted.inputs;++in)
            if(cw->outputPort==slot(out,false)&&cw->inputPort==slot(in,true)) {
              require(cw->visible,"Regenerated self-patch must remain visible");
              found.push_back({out,in,color::toHexString(cw->color)});
            }
      std::sort(expected.begin(),expected.end());std::sort(found.begin(),found.end());
      require(found==expected,"Regeneration must preserve both panel slot indices, colors and duplicate self-patch stacks");
      checkBeekCablePresentation(require);
    };
    verify();exterior();
    for(int i=0;i<3;++i) {
      name=m->getPatchName();
      if(i==0&&wanted.inputs&&wanted.outputs)selfWire(0,wanted.inputs-1);
      require(owner->regeneratePatch(),"Double dice regeneration must succeed");owner->step();verify();exterior();
      auto after=m->getPatchName();require(after!=name,"Generated names must change on regeneration");
      APP->history->undo();for(int tick=0;tick<20;++tick)owner->step();verify();exterior();require(m->getPatchName()==name,"Undo must restore the previous generated title");
      APP->history->redo();for(int tick=0;tick<20;++tick)owner->step();verify();exterior();require(m->getPatchName()==after,"Redo must restore the replacement title");
    }
    {
      auto ownerId=owner->module->id,sourceId=source->module->id,sinkId=sink->module->id;
      auto* saved=APP->engine->toJson();rack->mergeJson(saved);rack->clear();APP->history->clear();
      APP->engine->fromJson(saved);rack->fromJson(saved);json_decref(saved);
      owner=static_cast<beekapoo::Beekapoo*>(rack->getModule(ownerId));m=owner->getModule<beekapoo::Module>();
      source=rack->getModule(sourceId);sink=rack->getModule(sinkId);
      for(int i=0;i<3;++i)owner->step();verify();exterior();
    }
    name=m->getPatchName();owner->refreshGeneratedName();
    require(owner->growPatch(),"Grow must find an unlocked connection");
    require(m->getPatchName()!=name,"Grow must refresh a generated title");
    APP->history->undo();for(int tick=0;tick<20;++tick)owner->step();require(m->getPatchName()==name,"Grow Undo must restore the name");
    // A real internal cable edit also changes a generated name; resizing,
    // exposure and parameter edits do not.
    owner->refreshGeneratedName();name=m->getPatchName();
    app::CableWidget* internal=nullptr;
    for(auto* cw:rack->getCompleteCables())if(owner->cableRole(cw->cable)==beekapoo::Beekapoo::CableRole::Internal){internal=cw;break;}
    require(internal,"Generated patches need internal wiring");
    rack->removeCable(internal);delete internal;
    for(int tick=0;tick<20;++tick)owner->step();
    require(m->getPatchName()!=name,"Manual internal wiring changes must refresh generated names");
    name=m->getPatchName();owner->requestedWidth+=15;owner->layoutControls();owner->parameterAction(0,false);
    for(int tick=0;tick<20;++tick)owner->step();
    require(m->getPatchName()==name,"Resizing and changing knob values must keep the name");
    owner->rename("Keep this name");
    require(owner->regeneratePatch(),"Custom names must not prevent regeneration");verify();
    owner->growPatch();owner->step();require(m->getPatchName()=="Keep this name","A custom name must survive Grow and regeneration");
    owner->randomLocks.insert({owner->members.front().id,-1});
    auto old=counts();owner->regeneratePatch();auto now=counts();
    require(old.inputs==now.inputs&&old.outputs==now.outputs&&old.parameters==now.parameters,"Locked generation must not add or remove exposed controls");
  }
  rack->clear();APP->history->clear();
  std::puts("Beek regeneration: exact exposure counts, exterior/self-patch stacks, immediate patching, slot/color preservation, reload, names, locks and Undo/Redo passed");
}
