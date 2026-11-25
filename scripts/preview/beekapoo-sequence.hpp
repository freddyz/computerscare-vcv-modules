#pragma once
template<class Add,class Require>
void testBeekSequence(const std::string& output,float scale,Add add,Require require,plugin::Model* gainModel) {
  using beekapoo::Module;
  auto* rack=APP->scene->rack;
  auto* voice=add(gainModel,Vec());auto* child=voice->module;
  child->params[0].setValue(.25f);child->params[2].setValue(0);child->params[5].setValue(1);child->params[6].setValue(.4f);
  auto* owner=static_cast<beekapoo::Beekapoo*>(add(modelBeekapooPreview,Vec(300,0)));
  rack->deselectAll();rack->select(voice,true);owner->collapse();
  for(int p:{0,1,2,5})owner->exposeParameter(child->id,p);
  for(int p:{0,1})owner->expose(voice,voice->getInput(p));owner->expose(voice,voice->getOutput(0));
  auto* m=owner->getModule<Module>();
  require(owner->sequence.steps.size()==1&&m->playingStep==0,"New Beek starts at step 1/1");
  require(!m->clockEnabled.load()&&!m->scanEnabled.load(),"Clock and Scan default Off");
  for(const auto& v:owner->sequence.steps[0])require(v.param!=1,"Momentary buttons excluded from steps");
  child->params[0].setValue(1.5f);child->params[6].setValue(1.7f);owner->storeSequence(true);
  require(owner->sequence.steps.size()==2&&m->playingStep==1&&child->params[0].getValue()==1.5f,"Save as New captures and selects the live sound");
  m->process({});m->inputs[Module::ACTIONS_INPUT].channels=9;
  auto pulse=[&](int channel){m->inputs[Module::ACTIONS_INPUT].setVoltage(0,channel);m->process({});m->inputs[Module::ACTIONS_INPUT].setVoltage(10,channel);m->process({});m->inputs[Module::ACTIONS_INPUT].setVoltage(0,channel);m->process({});};
  pulse(Module::CLOCK_CHANNEL);require(m->playingStep==1,"Clock Off ignores incoming pulses");
  owner->selectSequenceStep(0);m->process({});require(m->playingStep==0&&child->params[0].getValue()==.25f,"Step selection recalls immediately even with Clock Off");
  m->inputs[Module::ACTIONS_INPUT].setVoltage(10,Module::SCAN_CHANNEL);m->params[Module::SEQUENCE_SCAN].setValue(10);m->process({});require(m->playingStep==0,"Disabled Scan ignores voltage and knob");
  m->clockEnabled.store(true);pulse(Module::CLOCK_CHANNEL);require(m->playingStep==1&&child->params[6].getValue()==1.7f,"Clock recalls unexposed parameters too");
  pulse(Module::CLOCK_CHANNEL);require(m->playingStep==0,"Scan Off does not interfere with ordinary clock advancement");
  m->params[Module::SEQUENCE_SCAN].setValue(0);m->scanEnabled.store(true);m->clockedScan.store(true);m->process({});require(m->playingStep==0,"Clocked Scan waits");pulse(Module::CLOCK_CHANNEL);require(m->playingStep==1,"Clock samples Scan");
  m->inputs[Module::ACTIONS_INPUT].setVoltage(0,Module::SCAN_CHANNEL);m->process({});require(m->playingStep==1,"Clocked Scan holds between pulses");pulse(Module::CLOCK_CHANNEL);require(m->playingStep==0,"Zero volts selects step one");
  m->clockedScan.store(false);m->inputs[Module::ACTIONS_INPUT].setVoltage(10,Module::SCAN_CHANNEL);m->process({});require(m->playingStep==1,"Continuous Scan follows voltage");
  m->clockEnabled.store(false);m->inputs[Module::ACTIONS_INPUT].setVoltage(0,Module::SCAN_CHANNEL);m->process({});require(m->playingStep==1,"Clock Off freezes automatic Scan as well");
  m->manualClocks.fetch_add(1);m->process({});require(m->playingStep==0,"Manual advance still works with Clock Off");
  m->inputs[Module::ACTIONS_INPUT].channels=1;m->inputs[Module::ACTIONS_INPUT].setVoltage(10,0);m->process({});require(m->playingStep==0,"Mono Actions must not replicate into transport");m->pendingActions.store(0);m->inputs[Module::ACTIONS_INPUT].channels=0;m->scanEnabled.store(false);
  child->params[0].setValue(.5f);owner->storeSequence(false);child->params[0].setValue(.8f);m->manualResets.fetch_add(1);m->process({});require(child->params[0].getValue()==.5f,"Store updates the active snapshot");
  for(const auto& p:owner->parameters)require(p.knob->module==child&&p.knob->getParamQuantity()==child->getParamQuantity(p.paramId),"All panel controls use original quantities and mapping IDs");
  auto* knob=owner->parameters.front().knob;
  widget::Widget::DoubleClickEvent dbl;knob->onDoubleClick(dbl);require(child->params[0].getValue()==child->getParamQuantity(0)->getDefaultValue(),"Native double click changes the live parameter");
  APP->history->undo();require(child->params[0].getValue()==.5f,"Native parameter undo still works");
  auto* button=dynamic_cast<beekapoo::ExposedSwitch*>(owner->parameters[1].knob);require(button&&button->momentary,"Momentary exposed button retains its type");
  widget::Widget::DragStartEvent start;start.button=GLFW_MOUSE_BUTTON_LEFT;button->onDragStart(start);require(child->params[1].getValue()==1,"Momentary still presses the live source");widget::Widget::DragEndEvent end;end.button=GLFW_MOUSE_BUTTON_LEFT;button->onDragEnd(end);rack->getModule(child->id)->step();rack->getModule(child->id)->step();require(child->params[1].getValue()==0,"Momentary releases the live source");
  float reference=0;require(!owner->savedSequenceValue(child->id,0,reference),"Saved-value ghosts default hidden");owner->showSavedValues=true;require(owner->savedSequenceValue(child->id,0,reference)&&reference==.5f,"Optional reference reads snapshot without replacing quantity");
  engine::ParamHandle mapping;APP->engine->addParamHandle(&mapping);APP->engine->updateParamHandle(&mapping,child->id,0);
  child->params[0].setValue(.9f);m->manualClocks.fetch_add(1);m->process({});require(child->params[0].getValue()==.9f&&child->params[6].getValue()==1.7f,"MIDI protects mapped value while other parameters recall");
  static_cast<beekapoofixture::Gain*>(child)->overwriteOnRandomize=true;
  for(int action:{0,1,3,4}){owner->parameterAction(action,false);require(child->params[0].getValue()==.9f,"Random and wiggle protect MIDI including module callbacks");}
  require(knob->getParamQuantity()->getValue()==.9f,"MIDI is reflected by the actual panel quantity");APP->engine->removeParamHandle(&mapping);m->manualResets.fetch_add(1);m->process({});require(child->params[0].getValue()==.5f,"Unmapping restores sequencing");
  owner->deleteSequenceStep(1);require(owner->sequence.steps.size()==1,"Delete removes a step");owner->deleteSequenceStep(0);require(owner->sequence.steps.size()==1,"Last step is retained");APP->history->undo();require(owner->sequence.steps.size()==2,"Undo restores step");APP->history->redo();require(owner->sequence.steps.size()==1,"Redo deletes step");APP->history->undo();
  m->clockEnabled.store(true);m->scanEnabled.store(true);m->clockedScan.store(true);owner->saveGroup();auto* saved=m->toJson();m->fromJson(saved);owner->step();json_decref(saved);
  require(m->clockEnabled.load()&&m->scanEnabled.load()&&m->clockedScan.load()&&owner->showSavedValues,"Transport and display options survive reload");require(owner->sequence.steps.size()==2,"Snapshots survive reload");
  // Old draft fields must never change the live patch on reload.
  auto* legacy=owner->sequenceJson();json_object_set_new(legacy,"viewed",json_integer(1));json_object_set_new(legacy,"dirty",json_true());json_object_set_new(legacy,"draft",beekapoo::stepValuesToJson({{child->id,0,1.9f}}));float before=child->params[0].getValue();owner->loadSequence(legacy);json_decref(legacy);require(child->params[0].getValue()==before,"Legacy drafts are ignored rather than applied");
  m->clockEnabled.store(false);m->scanEnabled.store(false);owner->saveGroup();
  rack->deselectAll();rack->select(owner,true);rack->cloneSelectionAction(false);beekapoo::Beekapoo* clone=nullptr;for(auto* w:rack->getModules())if(w!=owner)if(auto* b=dynamic_cast<beekapoo::Beekapoo*>(w))clone=b;
  require(clone,"Native duplicate creates Beek");clone->step();require(clone->sequence.steps.size()==2,"Duplicate keeps steps");for(const auto& step:clone->sequence.steps)for(const auto& v:step)require(v.module!=child->id&&clone->contains(APP->engine->getModule(v.module)),"Duplicate remaps every snapshot");before=child->params[0].getValue();clone->getModule<Module>()->manualResets.fetch_add(1);clone->getModule<Module>()->process({});require(child->params[0].getValue()==before,"Duplicate playback stays isolated");
  auto render=[&](const char* name){auto pos=owner->box.pos;auto* parent=owner->parent;parent->removeChild(owner);owner->box.pos=Vec();capture(owner,output+name,scale,false);owner->box.pos=pos;parent->addChild(owner);};
  for(float width:{60.f,120.f,240.f,255.f,285.f,330.f,480.f}) {
    owner->requestedWidth=width;owner->layoutControls();owner->step();std::vector<widget::Widget*> occupied{owner->stepSelector,owner->getInput(Module::ACTIONS_INPUT)};
    for(auto* b:owner->sequenceButtons)if(b->visible)occupied.push_back(b);for(auto* b:owner->actionButtons)if(b->visible)occupied.push_back(b);if(owner->regenerateButton->visible)occupied.push_back(owner->regenerateButton);
    for(int p:{Module::RANDOM_RANGE,Module::RANDOM_PROBABILITY,Module::WIGGLE_RANGE,Module::WIGGLE_PROBABILITY,Module::SEQUENCE_SCAN})if(owner->getParam(p)->visible)occupied.push_back(owner->getParam(p));
    for(size_t i=0;i<occupied.size();++i){auto b=occupied[i]->box;require(b.pos.x>=0&&b.getRight()<=owner->box.size.x&&b.getBottom()<=owner->ownFrame->box.getBottom()+2,"Action controls fit the strip");for(size_t j=0;j<i;++j)require(!b.intersects(occupied[j]->box),"Action controls do not overlap");}
    if(owner->twoRowControls){require(owner->getParam(Module::WIGGLE_RANGE)->box.pos.x==owner->actionButtons[0]->box.pos.x&&owner->getParam(Module::RANDOM_RANGE)->box.pos.x==owner->actionButtons[1]->box.pos.x,"Ranges align beneath their action groups");require(dynamic_cast<ScrambleKnob*>(owner->getParam(Module::RANDOM_PROBABILITY))&&dynamic_cast<ScrambleKnob*>(owner->getParam(Module::WIGGLE_PROBABILITY)),"Probability uses Voly Pector ScrambleKnob");}
  }
  owner->requestedWidth=330;m->panelTheme.store(1);owner->showSavedValues=false;owner->layoutControls();owner->step();render("/always-live-light.png");
  owner->exposeParameter(child->id,3);owner->exposeParameter(child->id,6);child->params[0].setValue(1.65f);child->params[5].setValue(3);owner->requestedWidth=285;m->panelTheme.store(2);owner->showSavedValues=true;owner->layoutControls();owner->step();render("/saved-reference-dark.png");
  owner->requestedWidth=75;owner->layoutControls();owner->step();render("/always-live-narrow.png");
  owner->requestedWidth=300;owner->layoutControls();before=child->params[0].getValue();owner->editSubpatch();auto* editor=static_cast<beekapoo::RackEditSession*>(owner->editSession.get());editor->step();require(child->params[0].getValue()==before,"Subpatch Edit stays live without changing parameters");editor->finish(true);
  rack->clear();APP->history->clear();
  std::puts("Beek always-live sequence: native controls/undo/momentaries, 1/1 startup, Store/New, selection, Clock/Scan Off, MIDI, persistence, duplication, ghosts and two-row geometry passed");
}
