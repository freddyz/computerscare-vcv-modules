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
  owner->appendSequenceStep();
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
