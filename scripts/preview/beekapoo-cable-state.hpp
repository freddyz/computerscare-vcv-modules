#pragma once
// In addition to the normalized graph, assert native widget identities,
// actual plug visibility/position and voltages at the original child inputs.
template<class Require>
void checkBeekCablePresentation(Require require) {
  auto* rack=APP->scene->rack;
  for(auto* w:rack->getModules()) if(auto* owner=dynamic_cast<beekapoo::Beekapoo*>(w)) {
    for(auto* cw:rack->getCompleteCables()) {
      auto* c=cw->cable;
      require(cw->inputPort && cw->outputPort && cw->inputPort->module==c->inputModule && cw->inputPort->portId==c->inputId && cw->outputPort->module==c->outputModule && cw->outputPort->portId==c->outputId,"CableWidget endpoints must match the native engine cable");
      auto role=owner->cableRole(c);
      if(role==beekapoo::Beekapoo::CableRole::Unrelated)continue;
      bool expected=role!=beekapoo::Beekapoo::CableRole::Bridge && (owner->expanded||role==beekapoo::Beekapoo::CableRole::Panel);
      require(cw->visible==expected,"Cable visibility must match logical membership and exposed endpoints");
      auto plugs=owner->plugWidgets.find(c->id);
      require(plugs!=owner->plugWidgets.end()&&plugs->second.input&&plugs->second.output,"Every managed cable must have its native plugs tracked");
      require(plugs->second.input->visible==expected&&plugs->second.output->visible==expected,"Cable and BOTH native plugs must agree on visibility");
      require(!(owner->panelCableIds.count(c->id)&&owner->internalCableIds.count(c->id)),"A cable cannot be classified both internal and panel");
      if(!expected)continue;
      auto position=[&](bool input) {
        auto* native=input?cw->inputPort:cw->outputPort;
        auto* route=input?owner->inputRoute(c):nullptr;
        if(!route&&!owner->contains(native->module))return;
        Vec actual=input?cw->getInputPos():cw->getOutputPos();
        if(owner->expanded) {
          auto* target=route?owner->originalPort(*route):native;
          require((actual-target->getRelativeOffset(target->box.size*.5f,rack)).norm()<.01f,"Edit cable endpoint must land on its original jack");
        } else {
          bool found=false;
          for(const auto& p:owner->ports)if(owner->findPort(p)==native) {
            auto center=p.face->getRelativeOffset(p.face->box.size*.5f,rack);
            found=p.face->visible&&(actual-center).norm()<.01f;
          }
          if(!found) std::fprintf(stderr,"Unmapped visible %s plug: cable %lld, owner %lld, endpoint %lld:%d at %.2f,%.2f\n",input?"input":"output",(long long)c->id,(long long)owner->module->id,(long long)native->module->id,native->portId,actual.x,actual.y);
          require(found,"Every visible cable end owned by a collapsed Beek must land on a visible panel jack");
        }
      };
      position(true);position(false);
    }
  }
}
template<class Add,class Require>
void testBeekCableState(const std::string& output,float scale,Add add,Require require) {
  using Owner=beekapoo::Beekapoo;
  auto* rack=APP->scene->rack;
  auto* model=plugin::getModel("computerscare","demo-gain");
  auto wire=[&](app::PortWidget* from,app::PortWidget* to) {
    auto* c=new engine::Cable;c->outputModule=from->module;c->outputId=from->portId;c->inputModule=to->module;c->inputId=to->portId;
    APP->engine->addCable(c);auto* cw=new app::CableWidget;cw->setCable(c);rack->addCable(cw);return c->id;
  };
  auto settle=[&] {for(int i=0;i<3;++i)rack->step();APP->engine->stepBlock(256);checkBeekCablePresentation(require);};
  auto reload=[&] {
    auto* saved=APP->engine->toJson();rack->mergeJson(saved);rack->clear();APP->history->clear();
    APP->engine->fromJson(saved);rack->fromJson(saved);json_decref(saved);settle();
  };
  auto gainInputs=[&] {
    std::map<int64_t,std::vector<float>> result;
    for(auto* w:rack->getModules())if(w->model==model) {
      auto& values=result[w->module->id];
      for(auto& input:w->module->inputs) {values.push_back(float(input.getChannels()));for(int ch=0;ch<4;++ch)values.push_back(input.getVoltage(ch));}
    }
    return result;
  };
  auto compare=[&](const std::map<int64_t,std::vector<float>>& before) {
    auto after=gainInputs();require(before.size()==after.size(),"Merge must retain all original processing modules");
    for(const auto& item:before)for(size_t i=0;i<item.second.size();++i)
      require(std::abs(item.second[i]-after.at(item.first)[i])<.0001f,"Merge/edit/reload/eject must preserve polyphony and attenuated stacked signal values");
  };
  // Both absorption directions, two/three owners, Create Beek, non-unity,
  // negative, zero and unity gains. Duplicate source cables remain distinct.
  for(int scenario=0;scenario<12;++scenario) {
    rack->clear();APP->history->clear();
    std::vector<Owner*> owners;std::vector<int64_t> ownerIds,sourceIds,childIds,publicWires,hiddenWires;
    for(int n=0;n<(scenario%2?3:2);++n) {
      auto* source=add(model,Vec(n*600,0));auto* child=add(model,Vec(n*600+90,0));
      source->module->params[0].setValue(.2f+float(n)*.1f);
      auto* owner=static_cast<Owner*>(add(modelBeekapooPreview,Vec(n*600+240,0)));
      rack->deselectAll();rack->select(source,true);rack->select(child,true);owner->collapse();
      owner->expose(child,child->getInput(1));owner->expose(source,source->getOutput(0));owner->exposeParameter(source->module->id,0);
      float gains[]={.37f,-.61f,0.f,1.f};owner->ports.front().gain->getParamQuantity()->setImmediateValue(gains[(scenario+n)%4]);
      owners.push_back(owner);ownerIds.push_back(owner->module->id);sourceIds.push_back(source->module->id);childIds.push_back(child->module->id);
    }
    auto* ordinary=add(model,Vec(2600,0));auto ordinaryId=ordinary->module->id;
    auto* outside=add(model,Vec(2800,0));outside->module->params[0].setValue(.25f);
    for(size_t n=0;n<owners.size();++n) {
      auto* proxy=owners[n]->findPort(owners[n]->ports.front());
      publicWires.push_back(wire(owners[n]->find(sourceIds[n])->getOutput(0),proxy));
      publicWires.push_back(wire(owners[(n+1)%owners.size()]->find(sourceIds[(n+1)%owners.size()])->getOutput(0),proxy));
      publicWires.push_back(wire(outside->getOutput(0),proxy));
      for(int i=0;i<2;++i)hiddenWires.push_back(wire(ordinary->getOutput(0),proxy));
    }
    settle();auto before=gainInputs();
    std::set<int64_t> realIds(publicWires.begin(),publicWires.end());realIds.insert(hiddenWires.begin(),hiddenWires.end());
    rack->deselectAll();for(auto* owner:owners)rack->select(owner,true);rack->select(ordinary,true);
    if(scenario%3==2)beekapoo::createSelectedGroup();
    else owners[scenario%3]->mergeSelection();
    Owner* combined=nullptr;for(auto* w:rack->getSelected())if(auto* o=dynamic_cast<Owner*>(w))combined=o;
    require(combined,"Merge must select the resulting Beek");auto id=combined->module->id;
    auto verify=[&] {
      settle();compare(before);combined=dynamic_cast<Owner*>(rack->getModule(id));require(combined,"Combined owner must remain present");
      require(combined->contains(APP->engine->getModule(ordinaryId)),"Ordinary upstream source must join the group");
      for(auto cable:realIds)require(rack->getCable(cable),"Merge must preserve each stacked cable ID, including repeated sources");
      for(auto cable:publicWires)require(rack->getCable(cable)->visible,"Exposed self/cross-Beek feedback and outside sources must stay visible");
      for(auto cable:hiddenWires) {
        // Create Beek may deliberately expose this source as a default;
        // hiding is required whenever it has no exposed output.
        auto* c=rack->getCable(cable)->cable;
        if(!combined->panelEndpoint(c->outputModule->id,c->outputId,false))require(!rack->getCable(cable)->visible,"Absorbed unexposed source must not leave a dangling plug");
      }
    };
    verify();
    for(int n=0;n<2;++n) {APP->history->undo();settle();compare(before);for(auto old:ownerIds)require(rack->getModule(old),"Undo restores all original owners");APP->history->redo();verify();}
    combined->editSubpatch();settle();compare(before);
    combined->moveEditedMembers(true,false);settle();compare(before);
    combined->moveEditedMembers(false,false);settle();compare(before);
    static_cast<beekapoo::RackEditSession*>(combined->editSession.get())->finish(true);verify();
    reload();verify();
    std::set<int64_t> internalBefore;
    for(auto* cw:rack->getCompleteCables())if(combined->cableRole(cw->cable)==Owner::CableRole::Internal)internalBefore.insert(cw->cable->id);
    combined->disconnectPanelAction();settle();
    for(auto cable:internalBefore)require(rack->getCable(cable),"Disconnect panel cables must keep hidden attenuated routes");
    for(auto cable:publicWires)require(!rack->getCable(cable),"Disconnect panel cables must remove visible feedback and boundary connections");
    APP->history->undo();verify();
    combined->ejectAction();settle();compare(before);
    APP->history->undo();verify();APP->history->redo();settle();compare(before);
  }
  rack->clear();APP->history->clear();
  // Ordinary-source absorption must not bypass the attenuverter, including
  // mixed stacks with sources that remain outside the group.
  for(float gain:{-.6f,0.f,.4f}) {
    auto* child=add(model,Vec());auto id=child->module->id;
    auto* first=add(model,Vec(500,0));auto* last=add(model,Vec(600,0));
    auto* owner=static_cast<Owner*>(add(modelBeekapooPreview,Vec(200,0)));
    rack->deselectAll();rack->select(child,true);owner->collapse();owner->expose(child,child->getInput(1));
    owner->ports.front().gain->getParamQuantity()->setImmediateValue(gain);
    wire(first->getOutput(0),owner->findPort(owner->ports.front()));wire(last->getOutput(0),owner->findPort(owner->ports.front()));settle();auto before=gainInputs();
    for(auto* source:{first,last}) {
      rack->deselectAll();rack->select(source,true);owner->addSelection();settle();compare(before);
      require(owner->exposed(owner->find(id)->getInput(1)),"Non-unity attenuation remains available until explicitly removed");
      APP->history->undo();settle();compare(before);APP->history->redo();settle();compare(before);
    }
    reload();compare(before);rack->clear();APP->history->clear();
  }
  {
    // Duplicate immediately after connecting, with no UI tick to refresh the
    // cached group JSON. The portable graph must carry its own gain intent.
    auto* source=add(model,Vec());source->module->params[0].setValue(.5f);
    auto* target=add(model,Vec(90,0));
    auto* owner=static_cast<Owner*>(add(modelBeekapooPreview,Vec(240,0)));
    rack->deselectAll();rack->select(source,true);rack->select(target,true);owner->collapse();
    owner->expose(target,target->getInput(1));owner->ports.front().gain->getParamQuantity()->setImmediateValue(.42f);
    wire(source->getOutput(0),owner->findPort(owner->ports.front()));
    owner->duplicateAction(false);settle();
    Owner* copy=nullptr;for(auto* w:rack->getModules())if(auto* o=dynamic_cast<Owner*>(w))if(o!=owner)copy=o;
    require(copy,"Immediate duplicate must create a group");
    for(auto* o:{owner,copy}) {
      const auto& p=o->ports.front();auto* child=o->find(p.id);
      for(int ch=0;ch<4;++ch)require(std::abs(child->module->inputs[1].getVoltage(ch)-.21f*(ch+1))<.0001f,"Immediate duplication must preserve hidden input gain routes");
    }
    APP->history->undo();settle();APP->history->redo();settle();
    rack->clear();APP->history->clear();
  }
  // Optional reproduction from a local copy of the reported saved patch.
  // Exclude audio/MIDI and unrelated plugins: this runner never opens devices.
  if(const char* path=std::getenv("BEEKAPOO_MERGE_PATCH")) {
    auto* patch=json_load_file(path,0,nullptr);require(patch,"Saved merge fixture must load");DEFER({json_decref(patch);});
    auto* modules=json_object_get(patch,"modules");std::set<int64_t> kept;
    for(size_t i=json_array_size(modules);i-->0;) {
      auto* m=json_array_get(modules,i);std::string maker=json_string_value(json_object_get(m,"plugin")),slug=json_string_value(json_object_get(m,"model"));
      if(maker=="Fundamental"||slug=="computerscare-beekapoo")kept.insert(json_integer_value(json_object_get(m,"id")));else json_array_remove(modules,i);
    }
    auto* cables=json_object_get(patch,"cables");std::set<int64_t> ids;
    for(size_t i=json_array_size(cables);i-->0;) {
      auto* c=json_array_get(cables,i);
      if(!kept.count(json_integer_value(json_object_get(c,"inputModuleId")))||!kept.count(json_integer_value(json_object_get(c,"outputModuleId"))))json_array_remove(cables,i);
      else ids.insert(json_integer_value(json_object_get(c,"id")));
    }
    APP->engine->fromJson(patch);rack->fromJson(patch);settle();
    require(rack->getCompleteCables().size()==ids.size(),"Saved phantom repair must not delete or add any real connection");
    for(auto id:ids)require(rack->getCable(id),"Saved patch repair must retain every native cable ID");
    for(auto* w:rack->getModules())if(auto* o=dynamic_cast<Owner*>(w)) {
      int hidden=0;for(auto* cw:rack->getCompleteCables())if(o->inputRoute(cw->cable)&&!cw->visible)++hidden;
      require(hidden>=2,"Reported sequencer CV 2/3 cables must become hidden internal gain routes");
      struct RackShot : widget::Widget {
        app::RackWidget* rack;Vec origin;
        void draw(const DrawArgs& args) override {
          nvgSave(args.vg);nvgTranslate(args.vg,-origin.x,-origin.y);
          auto shifted=args;shifted.clipBox.pos+=origin;
          rack->draw(shifted);rack->drawLayer(shifted,1);nvgRestore(args.vg);
        }
      } shot;
      shot.rack=rack;shot.origin=o->box.pos-Vec(15,15);
      shot.box.size=Vec(o->box.size.x+270,650);
      capture(&shot,output+"/merged-panel.png",scale,false);
      o->editSubpatch();settle();static_cast<beekapoo::RackEditSession*>(o->editSession.get())->finish(true);settle();
    }
    reload();
  }
  rack->clear();APP->history->clear();
  std::puts("Cable presentation: bidirectional/three-owner merges, self/stacked connections, native plugs/positions, signal gains, edit/near/far, eject, reload and Undo/Redo passed");
}
