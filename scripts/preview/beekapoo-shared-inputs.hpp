#pragma once
template<class Add, class Require>
void testBeekSharedInputs(const std::string& output, float scale, Add add, Require require) {
  using Owner=beekapoo::Beekapoo;
  auto* rack=APP->scene->rack;
  auto* model=plugin::getModel("computerscare","demo-gain");
  auto wire=[&](app::PortWidget* from,app::PortWidget* to) {
    auto* c=new engine::Cable;c->outputModule=from->module;c->outputId=from->portId;c->inputModule=to->module;c->inputId=to->portId;
    APP->engine->addCable(c);auto* cw=new app::CableWidget;cw->setCable(c);rack->addCable(cw);return c->id;
  };
  for(float gain:{1.f,-.6f,0.f}) for(int merge:{0,1,2}) {
    rack->clear();APP->history->clear();
    auto* seed=add(model,Vec());
    auto* nodes=json_array();auto* cables=json_array();
    for(int i=0;i<2;++i) {
      auto* n=seed->toJson();json_object_set_new(n,"id",json_integer(100+i));json_array_append_new(nodes,n);
    }
    json_array_append_new(nodes,json_pack("{s:I,s:s,s:s}","id",json_int_t(17),"plugin","Core","model","AudioInterface2"));
    auto connect=[&](int id,int from,int out,int to,int in) {
      json_array_append_new(cables,json_pack("{s:i,s:i,s:i,s:i,s:i}","id",id,"outputModuleId",from,"outputId",out,"inputModuleId",to,"inputId",in));
    };
    // Input 1 fans out, with a duplicate destination cable. Input 2 shares
    // the first destination but must remain an independent gain/control.
    connect(1,17,0,100,1);connect(2,17,0,101,1);connect(3,17,0,101,1);
    connect(4,17,1,100,1);
    connect(5,100,0,17,0);connect(6,101,0,17,1);
    auto* root=json_pack("{s:o,s:o}","modules",nodes,"cables",cables);
    auto path=output+"/shared-input-fixture.vcvs";json_dump_file(root,path.c_str(),0);json_decref(root);
    rack->clear();
    auto* owner=static_cast<Owner*>(add(modelBeekapooPreview,Vec(400,0)));
    require(owner->loadBeek(path,-1,true),"Shared-input fixture must import without Core audio");
    auto ownerId=owner->module->id;
    require(owner->members.size()==2&&owner->ports.size()==4,"Import must expose two shared inputs and actual outputs, with no relay");
    std::vector<int64_t> children;for(const auto& m:owner->members)children.push_back(m.id);
    auto input=[&](int n)->Owner::Port* {
      for(auto& p:owner->ports)if(p.input&&p.shared.label.find("input "+std::to_string(n))!=std::string::npos)return &p;
      return nullptr;
    };
    require(input(1)&&input(2)&&input(1)->shared.extra.size()==2,"One input must retain every destination, including duplicate connections");
    auto firstTarget=input(1)->id;
    legacyBeekGain(owner,*input(1))->setImmediateValue(gain);
    legacyBeekGain(owner,*input(2))->setImmediateValue(.25f);
    auto* first=add(model,Vec(1200,0));first->module->params[0].setValue(.4f);
    auto* second=add(model,Vec(1300,0));second->module->params[0].setValue(.7f);
    auto* other=add(model,Vec(1400,0));other->module->params[0].setValue(.8f);
    wire(first->getOutput(0),owner->findPort(*input(1)));
    wire(first->getOutput(0),owner->findPort(*input(1))); // same source stacked twice
    wire(second->getOutput(0),owner->findPort(*input(1)));
    wire(other->getOutput(0),owner->findPort(*input(2)));
    auto settle=[&] {
      for(int i=0;i<3;++i)rack->step();APP->engine->stepBlock(256);
      owner=dynamic_cast<Owner*>(rack->getModule(ownerId));
      checkBeekCablePresentation(require);
    };
    float total=1.5f;std::string stage="original";
    auto signals=[&] {
      settle();
      for(auto id:children) {
        auto* m=APP->engine->getModule(id);require(m,"Every original processing module must survive");
        auto& in=m->inputs[1];require(in.getChannels()==4,"Shared input retains all four channels");
        float value=id==firstTarget?total*gain+.2f:2.f*total*gain;
        for(int ch=0;ch<4;++ch) {
          if(std::abs(in.getVoltage(ch)-value*(ch+1))>=.0001f) {
            std::fprintf(stderr,"shared signals gain=%g merge=%d stage=%s target=%lld actual=%g expected=%g\n",gain,merge,stage.c_str(),(long long)id,in.getVoltage(ch),value*(ch+1));
            auto* snapshot=APP->engine->toJson();rack->mergeJson(snapshot);json_dump_file(snapshot,(output+"/failed-shared.json").c_str(),JSON_INDENT(2));json_decref(snapshot);
          }
          require(std::abs(in.getVoltage(ch)-value*(ch+1))<.0001f,"Shared fan-out, duplicate stacks and independent overlapping mappings must preserve voltages");
        }
      }
    };
    signals();
    owner->editSubpatch();signals();
    static_cast<beekapoo::RackEditSession*>(owner->editSession.get())->finish(true);signals();
    // Duplicate with boundary cables; copies must not bind the original buses.
    owner->duplicateAction(true);settle();
    Owner* clone=nullptr;for(auto* w:rack->getModules())if(auto* o=dynamic_cast<Owner*>(w))if(o!=owner)clone=o;
    require(clone&&clone->members.size()==2,"Shared mappings duplicate without helper modules");
    for(const auto& p:clone->ports)if(p.input) {
      require(p.shared.key!=input(p.shared.extra.empty()?2:1)->shared.key,"Copies get independent shared mapping identities");
      if(!p.shared.extra.empty()) {
        for(const auto& to:p.shared.extra)require(std::abs(APP->engine->getModule(to.id)->inputs[1].getVoltage(3)-12.f*gain)<.0001f,"Duplicate restores all fan-out destinations and gain");
      }
    }
    APP->history->undo();signals();APP->history->redo();signals();APP->history->undo();signals();
    if(!merge)for(auto id:children) {
      stage="release";owner->removeMembers({id});signals();
      require(input(1),"Removing one shared destination keeps the other destination exposed");
      stage="release undo";APP->history->undo();signals();stage="release redo";APP->history->redo();signals();APP->history->undo();signals();
    }
    // A public self-patch must survive the same merge/save/eject paths.
    wire(owner->find(firstTarget)->getOutput(0),owner->findPort(*input(1)));
    total+=1.f;signals();
    if(merge) {
      auto* extra=add(model,Vec(1800,0));
      auto* eater=static_cast<Owner*>(add(modelBeekapooPreview,Vec(2000,0)));
      rack->deselectAll();rack->select(extra,true);eater->collapse();
      rack->deselectAll();rack->select(owner,true);rack->select(eater,true);
      auto oldOwner=ownerId;
      if(merge==1){eater->mergeSelection();ownerId=eater->module->id;}
      else {beekapoo::createSelectedGroup();for(auto* w:rack->getSelected())if(auto* o=dynamic_cast<Owner*>(w))ownerId=o->module->id;}
      signals();require(input(1)&&input(2)&&input(1)->shared.extra.size()==2,"Merge retains one shared input for the full fan-out");
      auto merged=ownerId;APP->history->undo();ownerId=oldOwner;signals();APP->history->redo();ownerId=merged;signals();
    }
    // Native patch save/reload retains native proxy slots and all bridges.
    auto* saved=APP->engine->toJson();rack->mergeJson(saved);rack->clear();APP->history->clear();APP->engine->fromJson(saved);rack->fromJson(saved);json_decref(saved);signals();
    auto sharedFile=output+"/shared-input-beek.vcv";owner->saveBeek(sharedFile);
    auto* imported=static_cast<Owner*>(add(modelBeekapooPreview,Vec(2300,0)));
    require(imported->loadBeek(sharedFile),"Save/Load Beek must retain shared mappings");
    int shared=0;for(const auto& p:imported->ports)if(p.input&&p.shared.key>=0) ++shared;
    require(shared==2,"Portable export keeps two independent shared inputs");
    // The exported Beek omits outside cables but retains the public self-patch.
    auto importedId=imported->module->id;
    std::vector<int64_t> importedChildren;
    int importedCheck=0;
    auto importedSignals=[&](float expected) {
      ++importedCheck;
      for(int i=0;i<3;++i)rack->step();APP->engine->stepBlock(256);
      imported=dynamic_cast<Owner*>(rack->getModule(importedId));
      checkBeekCablePresentation(require);
      if(importedChildren.empty())for(const auto& p:imported->ports)if(p.input&&!p.shared.extra.empty()) {
        importedChildren.push_back(p.id);importedChildren.push_back(p.shared.extra[0].id);
      }
      require(importedChildren.size()==2,"Portable shared mapping has both native targets");
      for(size_t i=0;i<importedChildren.size();++i) {
        auto& in=APP->engine->getModule(importedChildren[i])->inputs[1];
        if(!(in.getChannels()>0 && std::abs(in.getVoltage()-expected*(i+1))<.0001f)) {
          auto* snapshot=APP->engine->toJson();rack->mergeJson(snapshot);json_dump_file(snapshot,(output+"/failed-shared.json").c_str(),JSON_INDENT(2));json_decref(snapshot);
          std::fprintf(stderr,"shared check gain=%g merge=%d step=%d target=%zu channels=%d actual=%g expected=%g\n",gain,merge,importedCheck,i,in.getChannels(),in.getVoltage(),expected*(i+1));}
        require(in.getChannels()>0 && std::abs(in.getVoltage()-expected*(i+1))<.0001f,"Portable self-patch and shared silence preserve target signals");
      }
    };
    importedSignals(gain);
    require(imported->foldSelfPatchedInput(),"Shared self-only input supports minus/internalize");
    importedSignals(gain);APP->history->undo();importedSignals(gain);APP->history->redo();importedSignals(gain);APP->history->undo();importedSignals(gain);
    for(auto* cw:rack->getCompleteCables())if(cw->cable->inputModule==imported->module && imported->contains(cw->cable->outputModule)) {rack->removeCable(cw);delete cw;}
    importedSignals(0.f);
    auto* bm=static_cast<beekapoo::Module*>(imported->module);
    engine::Module::ProcessArgs args{48000.f,1.f/48000.f,0};bm->processBypass(args);
    for(const auto& p:imported->ports)if(p.input&&p.shared.key>=0)require(bm->outputs[p.route].getChannels()==1 && bm->outputs[p.route].getVoltage()==0.f,"Bypass preserves unpatched shared-input silence");
    imported->ejectAction();importedSignals(0.f);APP->history->undo();importedSignals(0.f);APP->history->redo();importedSignals(0.f);
    settle();
    owner->ejectAction();signals();
    require(!owner,"Eject removes the Beek while native cables retain every destination");
    APP->history->undo();signals();require(owner&&input(1)&&input(2),"Undo eject restores shared mappings");
    APP->history->redo();signals();
  }
  rack->clear();APP->history->clear();
  // Fresh visual example: one imported hardware input feeds both filters.
  auto* nodes=json_array();auto* cables=json_array();
  for(int i=0;i<3;++i) {
    auto* w=add(plugin::getModel("Fundamental",i?"VCF":"VCO"),Vec(i*90,0));
    if(i) {w->module->params[0].setValue(.38f+i*.15f);w->module->params[2].setValue(.15f+i*.22f);w->module->params[3].setValue(.6f);}
    else w->module->params[2].setValue(-38.f);
    auto* n=w->toJson();json_object_set_new(n,"id",json_integer(100+i));json_array_append_new(nodes,n);
  }
  json_array_append_new(nodes,json_pack("{s:i,s:s,s:s}","id",17,"plugin","Core","model","AudioInterface2"));
  int cid=1;
  auto connect=[&](int from,int out,int to,int in) {json_array_append_new(cables,json_pack("{s:i,s:i,s:i,s:i,s:i}","id",cid++,"outputModuleId",from,"outputId",out,"inputModuleId",to,"inputId",in));};
  connect(17,0,101,3);connect(17,0,102,3);
  connect(100,0,101,0);connect(100,1,102,0);
  connect(101,0,17,0);connect(102,0,17,1);
  auto* root=json_pack("{s:o,s:o}","modules",nodes,"cables",cables);
  auto path=output+"/shared-filter-example.vcvs";json_dump_file(root,path.c_str(),0);json_decref(root);rack->clear();
  auto* shown=static_cast<Owner*>(add(modelBeekapooPreview,Vec()));
  require(shown->loadBeek(path,-1,true)&&shown->members.size()==3,"The example retains just VCO and two VCFs");
  for(const auto& m:shown->members) {
    auto* child=shown->find(m.id);
    if(child->model->slug=="VCF"){shown->exposeParameter(m.id,0);shown->exposeParameter(m.id,2);}
    else shown->exposeParameter(m.id,2);
  }
  shown->getModule<beekapoo::Module>()->setPatchName("Shared input",true);
  shown->requestedWidth=240;shown->layoutControls();shown->saveGroup();
  renderGeneratedBeekRow(shown,output+"/shared-input-example.png",scale);
  rack->clear();APP->history->clear();
  std::puts("Shared inputs: fan-out, overlapping targets, duplicate stacks, polyphonic gains, self-patches, shared-input minus, silence/bypass, destination removal, Edit, native/portable save-load, duplication, both merges, eject and Undo/Redo passed");
}
