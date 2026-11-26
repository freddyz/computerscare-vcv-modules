#pragma once
template<class Add,class Require>
void testBeekSequence(const std::string& output,float scale,Add add,Require require,plugin::Model* gainModel) {
  using beekapoo::Module;
  auto* rack=APP->scene->rack;
  auto* voice=add(gainModel,Vec());auto* child=voice->module;
  child->params[0].setValue(.25f);
  auto* owner=static_cast<beekapoo::Beekapoo*>(add(modelBeekapooPreview,Vec(300,0)));
  rack->deselectAll();rack->select(voice,true);owner->collapse();
  for(int p:{0,1,2,5})owner->exposeParameter(child->id,p);
  owner->expose(voice,voice->getInput(0));owner->expose(voice,voice->getOutput(0));
  auto* m=owner->getModule<Module>();
  auto saved=[&](int step,int param){for(const auto& v:owner->sequence.steps.at(step))if(v.param==param)return v.getValue();return -99.f;};
  auto select=[&](int step){owner->selectSequenceStep(step);m->process({});};
  require(owner->sequence.steps.size()==1&&!m->clockEnabled&&!m->scanEnabled,"One initial step, Clock/Scan Off");
  require(saved(0,1)==-99,"Momentaries excluded");
  child->params[0].setValue(.5f);owner->refreshSequenceModified();
  require(owner->sequenceModified&&owner->sequenceStatus()=="1/1 •","Unsaved live edits show modified marker");
  require(owner->sequenceButtons[3]->modified()&&owner->sequenceButtons[4]->modified(),"Capture and Update both mark unsaved changes");
  owner->appendSequenceStep();
  require(!owner->sequenceButtons[3]->modified()&&!owner->sequenceButtons[4]->modified(),"Capturing clears both change indicators");
  require(saved(0,0)==.25f&&saved(1,0)==.5f&&m->playingStep==1&&!owner->sequenceModified,"Capture New preserves A and adds live sound B");
  child->params[0].setValue(1.5f);m->manualClocks.fetch_add(1);m->process({});owner->refreshSequenceModified();
  require(saved(1,0)==.5f&&child->params[0].getValue()==.25f&&!owner->sequenceModified,"Advance discards live tweaks without overwriting either snapshot");
  child->params[0].setValue(.6f);select(1);select(0);
  require(child->params[0].getValue()==.25f,"Rapid recalls preserve saved steps");
  child->params[0].setValue(.75f);owner->sequenceButtons[4]->action();
  require(saved(0,0)==.75f&&saved(1,0)==.5f&&!owner->sequenceModified,"Update replaces only selected snapshot");
  APP->history->undo();require(saved(0,0)==.25f&&child->params[0].getValue()==.75f,"Undo Update restores snapshot without changing live knobs");
  APP->history->redo();require(saved(0,0)==.75f,"Redo restores updated snapshot");
  child->params[0].setValue(.8f);owner->sequenceButtons[5]->action();m->process({});owner->refreshSequenceModified();
  require(child->params[0].getValue()==.75f&&!owner->sequenceModified,"Recall discards tweaks to the same step");
  m->clockEnabled=true;m->inputs[Module::ACTIONS_INPUT].channels=9;
  auto pulse=[&](int ch){m->inputs[Module::ACTIONS_INPUT].setVoltage(0,ch);m->process({});m->inputs[Module::ACTIONS_INPUT].setVoltage(10,ch);m->process({});m->inputs[Module::ACTIONS_INPUT].setVoltage(0,ch);m->process({});};
  child->params[0].setValue(.7f);pulse(Module::CLOCK_CHANNEL);
  require(m->playingStep==1&&saved(0,0)==.75f&&child->params[0].getValue()==.5f,"CV clock recalls without saving outgoing tweaks");
  m->clockEnabled=false;child->params[0].setValue(.9f);pulse(Module::CLOCK_CHANNEL);
  require(m->playingStep==1&&child->params[0].getValue()==.9f,"Clock Off ignores incoming pulses while exploring");
  m->clockEnabled=true;m->scanEnabled=true;m->clockedScan=false;m->inputs[Module::ACTIONS_INPUT].setVoltage(0,Module::SCAN_CHANNEL);m->process({});
  require(m->playingStep==0,"Continuous Scan recalls");
  child->params[0].setValue(.8f);m->inputs[Module::ACTIONS_INPUT].setVoltage(10,Module::SCAN_CHANNEL);m->process({});
  require(m->playingStep==1&&saved(0,0)==.75f,"Scan preserves departing snapshot");
  m->clockedScan=true;m->inputs[Module::ACTIONS_INPUT].setVoltage(0,Module::SCAN_CHANNEL);m->process({});
  require(m->playingStep==1,"Clocked Scan waits");pulse(Module::CLOCK_CHANNEL);require(m->playingStep==0,"Clocked Scan recalls on pulse");
  m->clockEnabled=false;m->scanEnabled=false;m->inputs[Module::ACTIONS_INPUT].channels=0;
  owner->toggleRandomLock(child->id,0);child->params[0].setValue(.9f);select(1);
  require(child->params[0].getValue()==.9f&&saved(1,0)==.5f,"Lock holds live value without changing stored step");
  owner->refreshSequenceModified();require(!owner->sequenceModified,"Protected values do not falsely mark recalled steps modified");
  static_cast<beekapoofixture::Gain*>(child)->overwriteOnRandomize=true;
  for(int action:{0,1,3,4}){owner->parameterAction(action,false);require(child->params[0].getValue()==.9f,"Same lock protects all random/wiggle paths");}
  owner->toggleRandomLock(child->id,-1);child->params[6].setValue(.3f);select(0);require(child->params[6].getValue()==.3f,"Module lock protects all its parameters");
  owner->toggleRandomLock(child->id,-1);owner->toggleRandomLock(child->id,0);select(0);
  require(child->params[0].getValue()==.75f,"Unlock allows recall of preserved snapshot");
  engine::ParamHandle mapping;APP->engine->addParamHandle(&mapping);APP->engine->updateParamHandle(&mapping,child->id,0);
  child->params[0].setValue(.33f);select(1);require(child->params[0].getValue()==.33f,"MIDI mapping still protects recall");APP->engine->removeParamHandle(&mapping);
  select(0);child->params[0].setValue(.66f);
  for(int i=0;i<4096;++i)m->process({});require(saved(0,0)==.75f,"Idle audio processing never autosaves");
  auto* persisted=m->dataToJson();auto* data=json_object_get(json_object_get(persisted,"group"),"sequence");
  auto values=beekapoo::stepValuesFromJson(json_array_get(json_object_get(data,"steps"),0));bool found=false;
  for(const auto& v:values)if(v.param==0)found=v.getValue()==.75f;
  require(found,"Patch saving preserves stored snapshot separately from live edits");
  // Loading a saved Beek must leave the native live values alone.
  m->dataFromJson(persisted);owner->step();json_decref(persisted);owner->refreshSequenceModified();
  require(child->params[0].getValue()==.66f&&saved(0,0)==.75f&&owner->sequenceModified,"Reload keeps live tweaks and saved snapshot distinct");
  // The audio thread runs before, and during, the first UI restoration frame.
  // Simulate both schedules with a two-step bank and an already-high clock.
  select(1);m->clockEnabled=true;owner->saveGroup();
  auto* clockedSave=m->dataToJson();
  auto* face=static_cast<beekapoofixture::Face*>(owner->find(child->id));
  int restoreFrames=0;
  for(int reload=0;reload<3;++reload) {
    child->params[0].setValue(.5f);
    m->manualClocks=2;m->manualResets=1;m->requestedStep=0;m->pendingActions=1;
    m->dataFromJson(clockedSave);
    auto epoch=m->sequenceRecallEpoch.load();
    m->inputs[Module::ACTIONS_INPUT].channels=7;
    for(int sample=0;sample<128;++sample) {
      m->inputs[Module::ACTIONS_INPUT].setVoltage(sample%2?10.f:0.f,Module::CLOCK_CHANNEL);m->process({});
    }
    require(m->playingStep==1&&child->params[0].getValue()==.5f,"Audio before UI restore preserves saved playback position and live values");
    face->beforeStep=[&]{
      ++restoreFrames;
      require(m->restoringSequence,"Native source widgets restore under the playback barrier");
      m->inputs[Module::ACTIONS_INPUT].setVoltage(0,Module::CLOCK_CHANNEL);m->process({});
      m->inputs[Module::ACTIONS_INPUT].setVoltage(10,Module::CLOCK_CHANNEL);m->process({});
      require(m->sequenceRecallEpoch==epoch&&child->params[0].getValue()==.5f,"Intermediate restoration never recalls a partial bank");
    };
    owner->step();face->beforeStep={};
    require(!m->restoringSequence&&m->playingStep==1&&m->sequenceCount==2,"Restore publishes both steps and saved active index together");
    m->process({});
    require(m->playingStep==1&&child->params[0].getValue()==.5f&&!m->pendingActions,"Initial high clock and stale requests do not jump or randomize the restored step");
    pulse(Module::CLOCK_CHANNEL);require(m->playingStep==0&&child->params[0].getValue()==.75f,"First new clock edge recalls exactly the next saved step");
    pulse(Module::CLOCK_CHANNEL);require(m->playingStep==1&&child->params[0].getValue()==.5f,"Second new clock edge recalls the other saved step");
  }
  require(restoreFrames>=3,"Regression exercised native widgets during repeated reverts");
  json_decref(clockedSave);m->clockEnabled=false;m->inputs[Module::ACTIONS_INPUT].channels=0;
  select(1);float remaining=saved(0,0);owner->sequenceButtons[6]->action();
  auto* during=m->dataToJson();json_decref(during);m->process({});
  require(owner->sequence.steps.size()==1&&m->playingStep==0&&child->params[0].getValue()==remaining,"Delete active step recalls neighbour without overwriting it");
  owner->sequenceButtons[6]->action();require(owner->sequence.steps.size()==1,"Last remaining step is retained");
  APP->history->undo();require(owner->sequence.steps.size()==2,"Delete step is undoable");
  // Both legacy lock collections migrate to the same lock set.
  owner->saveGroup();auto* old=m->dataToJson();auto* group=json_object_get(old,"group");
  json_object_set_new(group,"randomLocks",json_pack("[{s:I,s:i}]","module",json_int_t(child->id),"param",0));
  json_object_set_new(group,"sequenceLocks",json_pack("[{s:I,s:i}]","module",json_int_t(child->id),"param",6));
  m->dataFromJson(old);owner->step();json_decref(old);
  require(owner->randomLocked(child->id,0)&&owner->randomLocked(child->id,6),"Old random and sequence locks migrate as a union");
  rack->deselectAll();rack->select(owner,true);rack->cloneSelectionAction(false);beekapoo::Beekapoo* clone=nullptr;
  for(auto* w:rack->getModules())if(w!=owner)if(auto* b=dynamic_cast<beekapoo::Beekapoo*>(w))clone=b;
  require(clone,"Native clone created");clone->step();require(clone->randomLocks.size()==2,"Clone keeps unified locks");
  for(const auto& lock:clone->randomLocks)require(lock.first!=child->id,"Clone remaps locks");
  float before=child->params[0].getValue();clone->getModule<Module>()->manualClocks.fetch_add(1);clone->getModule<Module>()->process({});require(child->params[0].getValue()==before,"Clone playback and snapshots stay isolated");
  for(const auto& p:owner->parameters)require(p.knob->getParamQuantity()==child->getParamQuantity(p.paramId),"Native quantity IDs remain unchanged");
  owner->unlockRandomization();owner->clearSequenceSteps();require(owner->sequence.steps.size()==1&&child->params[0].getValue()==before,"Clear keeps live sound");
  auto render=[&](const char* file){auto pos=owner->box.pos;auto* parent=owner->parent;parent->removeChild(owner);owner->box.pos=Vec();capture(owner,output+file,scale,false);owner->box.pos=pos;parent->addChild(owner);};
  for(float width:{60.f,120.f,225.f,255.f,285.f,330.f,480.f}){
    owner->requestedWidth=width;owner->layoutControls();owner->step();std::vector<widget::Widget*> occupied{owner->getInput(Module::ACTIONS_INPUT)};
    if(owner->stepSelector->visible){occupied.push_back(owner->stepSelector);require(owner->stepSelector->box.getRight()<owner->growButton->box.pos.x&&owner->stepSelector->box.getBottom()<=owner->titleFrame->box.getBottom(),"Selector stays in header before functions");}
    for(auto* b:owner->sequenceButtons)if(b->visible)occupied.push_back(b);for(auto* b:owner->actionButtons)if(b->visible)occupied.push_back(b);if(owner->regenerateButton->visible)occupied.push_back(owner->regenerateButton);
    for(int p:{Module::RANDOM_RANGE,Module::RANDOM_PROBABILITY,Module::WIGGLE_RANGE,Module::WIGGLE_PROBABILITY,Module::SEQUENCE_SCAN})if(owner->getParam(p)->visible)occupied.push_back(owner->getParam(p));
    for(size_t i=0;i<occupied.size();++i)for(size_t j=0;j<i;++j)require(!occupied[i]->box.intersects(occupied[j]->box),"Controls do not overlap");
  }
  owner->requestedWidth=330;m->panelTheme=1;owner->layoutControls();owner->step();
  child->params[0].setValue(.42f);owner->refreshSequenceModified();render("/snapshots-modified-light.png");
  owner->appendSequenceStep();child->params[0].setValue(1.2f);owner->appendSequenceStep();owner->exposeParameter(child->id,3);owner->exposeParameter(child->id,6);owner->requestedWidth=285;m->panelTheme=2;owner->layoutControls();owner->step();owner->refreshSequenceModified();render("/snapshots-captured-dark.png");
  child->params[0].setValue(.91f);owner->refreshSequenceModified();
  owner->showStepSelector();ui::Menu* stepMenu=nullptr;ui::MenuOverlay* stepOverlay=nullptr;
  for(auto* w:APP->scene->children)if(auto* overlay=dynamic_cast<ui::MenuOverlay*>(w))
    for(auto* c:overlay->children)if(auto* menu=dynamic_cast<ui::Menu*>(c)){stepMenu=menu;stepOverlay=overlay;}
  require(stepMenu&&stepOverlay,"Step selector opens native menu");stepMenu->step();
  std::vector<ui::MenuItem*> stepItems;for(auto* w:stepMenu->children)if(auto* item=dynamic_cast<ui::MenuItem*>(w))stepItems.push_back(item);
  require(stepItems.size()==3&&stepItems[2]->text=="3 •"&&stepItems[2]->rightText==CHECKMARK_STRING,"Active modified step has a dot and native checkmark");
  require(stepItems[0]->text=="1"&&stepItems[0]->rightText.empty(),"Inactive step has no marker");
  // Compose the live panel and native menu together, without changing the patch.
  auto* panelParent=owner->parent;Vec panelPos=owner->box.pos,menuPos=stepMenu->box.pos;
  panelParent->removeChild(owner);stepOverlay->removeChild(stepMenu);
  widget::Widget shot;shot.box.size=Vec(owner->box.size.x+stepMenu->box.size.x+18,owner->box.size.y);
  owner->box.pos=Vec();stepMenu->box.pos=Vec(owner->box.size.x+10,34);shot.addChild(owner);shot.addChild(stepMenu);
  capture(&shot,output+"/snapshots-active-menu-dark.png",scale,false);
  shot.removeChild(stepMenu);shot.removeChild(owner);owner->box.pos=panelPos;stepMenu->box.pos=menuPos;panelParent->addChild(owner);stepOverlay->addChild(stepMenu);
  select(0);owner->refreshSequenceModified();stepMenu->step();
  require(stepItems[0]->rightText==CHECKMARK_STRING&&stepItems[2]->rightText.empty()&&stepItems[2]->text=="3","Open menu tracks playback and removes stale modified marker");
  require(!owner->sequenceButtons[3]->modified()&&!owner->sequenceButtons[4]->modified(),"Recall clears both change indicators");
  APP->scene->removeChild(stepOverlay);delete stepOverlay;
  // Real Rack patch round-trip keeps working sound AND the distinct snapshots.
  select(0);float snapshot=saved(0,0);child->params[0].setValue(.37f);
  auto ownerId=m->id;auto childId=child->id;
  auto* patch=APP->engine->toJson();rack->mergeJson(patch);
  rack->clear();APP->engine->fromJson(patch);rack->fromJson(patch);json_decref(patch);
  owner=dynamic_cast<beekapoo::Beekapoo*>(rack->getModule(ownerId));
  require(owner,"Patch reload restores Beek");owner->step();m=owner->getModule<Module>();
  child=APP->engine->getModule(childId);require(child,"Patch reload restores live child");
  owner->refreshSequenceModified();
  require(child->params[0].getValue()==.37f&&saved(0,0)==snapshot&&owner->sequenceModified,"Full patch reload preserves unsaved live sound and saved snapshot");
  owner->recallSequenceStep();m->process({});require(child->params[0].getValue()==snapshot,"Reloaded snapshot remains recallable");
  rack->clear();APP->history->clear();
  std::puts("Beek snapshots: capture/update/recall, manual/CV/Scan playback, no autosave, locks, MIDI, save/reload, delete/undo, migration, clone isolation and layout passed");
}

// Replay the saved Beek and its own children only: no audio/MIDI interfaces.
// The supplied file is an extracted patch.json; it is never changed.
template<class Require>
void testBeekSavedSequence(const std::string& output,float scale,Require require) {
  auto* patch=json_load_file(std::getenv("BEEKAPOO_SEQUENCE_PATCH"),0,nullptr);
  require(patch,"Saved sequence fixture must load");
  DEFER({json_decref(patch);});
  json_t* ownerJson=nullptr;auto* modules=json_object_get(patch,"modules");
  for(size_t i=0;i<json_array_size(modules);++i) {
    auto* m=json_array_get(modules,i);const char* slug=json_string_value(json_object_get(m,"model"));
    if(slug&&std::string(slug)=="computerscare-beekapoo"){ownerJson=m;break;}
  }
  require(ownerJson,"Saved fixture has a Beek");
  auto* group=json_object_get(json_object_get(ownerJson,"data"),"group");
  auto* seq=json_object_get(group,"sequence");int active=json_integer_value(json_object_get(seq,"playing"));
  auto id=json_integer_value(json_object_get(ownerJson,"id"));std::set<int64_t> ids{id};
  auto* members=json_object_get(group,"members");
  for(size_t i=0;i<json_array_size(members);++i)ids.insert(json_integer_value(json_object_get(json_array_get(members,i),"id")));
  for(size_t i=json_array_size(modules);i-->0;)if(!ids.count(json_integer_value(json_object_get(json_array_get(modules,i),"id"))))json_array_remove(modules,i);
  auto* cables=json_object_get(patch,"cables");
  for(size_t i=json_array_size(cables);i-->0;) {
    auto* c=json_array_get(cables,i);
    if(!ids.count(json_integer_value(json_object_get(c,"inputModuleId")))||!ids.count(json_integer_value(json_object_get(c,"outputModuleId"))))json_array_remove(cables,i);
  }
  auto* rack=APP->scene->rack;
  for(int reload=0;reload<3;++reload) {
    rack->clear();APP->engine->fromJson(patch);
    auto* m=dynamic_cast<beekapoo::Module*>(APP->engine->getModule(id));require(m,"Restored sequence engine");
    std::vector<std::pair<engine::Param*,float>> live;
    for(auto childId:ids)if(childId!=id)for(auto& p:APP->engine->getModule(childId)->params)live.push_back({&p,p.getValue()});
    m->inputs[beekapoo::Module::ACTIONS_INPUT].channels=7;
    for(int sample=0;sample<256;++sample){m->inputs[beekapoo::Module::ACTIONS_INPUT].setVoltage(sample%2?10:0,6);m->process({});}
    rack->fromJson(patch);
    auto* owner=dynamic_cast<beekapoo::Beekapoo*>(rack->getModule(id));require(owner,"Restored sequence panel");owner->step();m->process({});
    require(m->playingStep==active,"Saved patch keeps the active step through startup");
    for(auto p:live)require(p.first->getValue()==p.second,"Saved patch keeps every native live parameter through restoration");
    for(int step=0;step<4;++step) {
      m->inputs[beekapoo::Module::ACTIONS_INPUT].setVoltage(0,6);m->process({});
      m->inputs[beekapoo::Module::ACTIONS_INPUT].setVoltage(10,6);m->process({});
      int expected=(active+step+1)%owner->sequence.steps.size();require(m->playingStep==expected,"Saved patch clock advances exactly once");
      for(const auto& v:owner->sequence.steps[expected]) {
        auto* q=APP->engine->getModule(v.module)->getParamQuantity(v.param);
        if(!owner->randomLocked(v.module,v.param))require(std::abs(q->getValue()-clamp(v.getValue(),q->getMinValue(),q->getMaxValue()))<1e-5f,"Saved patch recalls every eligible value accurately");
      }
    }
    if(reload==2){owner->step();auto* parent=owner->parent;auto pos=owner->box.pos;parent->removeChild(owner);owner->box.pos=Vec();capture(owner,output+"/saved-sequence-reloaded.png",scale,false);owner->box.pos=pos;parent->addChild(owner);}
  }
  rack->clear();APP->history->clear();std::puts("Saved sequence: three cold reloads, startup clocks, native live values and four recalls per reload passed");
}
