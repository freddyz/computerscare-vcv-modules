#pragma once
template<class Add,class Require>
void testBeekInputRoutes(Add add,Require require) {
  auto* rack=APP->scene->rack;rack->clear();APP->history->clear();
  static_assert(beekapoo::Module::inputBase==6,"Saved exposed inputs start at port 6");
  static_assert(beekapoo::Module::GENERATE_INPUT>=beekapoo::Module::inputBase+beekapoo::Module::routeCapacity,"Actions must not reuse saved exposed-input IDs");
  auto* model=plugin::getModel("computerscare","demo-gain");
  auto wire=[&](app::ModuleWidget* from,int out,app::ModuleWidget* to,int in) {
    auto* c=new engine::Cable;c->outputModule=from->module;c->outputId=out;c->inputModule=to->module;c->inputId=in;
    APP->engine->addCable(c);auto* cw=new app::CableWidget;cw->setCable(c);rack->addCable(cw);return c->id;
  };
  auto* a=add(model,Vec());auto* b=add(model,Vec(90,0));
  auto* s1=add(model,Vec(300,0));auto* s2=add(model,Vec(390,0));auto* s3=add(model,Vec(480,0));
  int64_t aId=a->module->id,bId=b->module->id,s1Id=s1->module->id,s2Id=s2->module->id;
  wire(a,0,b,0);auto c1=wire(s1,0,a,0),c2=wire(s2,0,a,0),c3=wire(s3,0,a,0);
  auto* owner=static_cast<beekapoo::Beekapoo*>(add(modelBeekapooPreview,Vec(600,0)));
  rack->deselectAll();rack->select(a,true);rack->select(b,true);owner->groupAction(true);owner->step();
  for(auto& p:owner->ports)if(p.input)legacyBeekGain(owner,p);
  APP->engine->stepBlock(64);float expected=b->module->outputs[0].getVoltage();
  require(expected==3,"Three incoming native cables must sum before absorption");
  owner->expose(a,a->getInput(1));legacyBeekGain(owner,owner->ports.back());
  int routedInput=-1;for(const auto& p:owner->ports)if(p.id==aId&&p.input&&p.portId==1)routedInput=6+p.route;
  for(int repeat=0;repeat<2;++repeat) {
    APP->engine->stepBlock(64);
    require(a->module->inputs[1].getChannels()==0,"An unpatched exposed input must leave the original input normalized");
    auto id=wire(s1,0,owner,routedInput);owner->step();APP->engine->stepBlock(64);
    require(a->module->inputs[1].getChannels()==4&&a->module->inputs[1].getVoltage(3)==4,"Real cables must deliver polyphonic audio after an unpatched interval");
    auto* cw=rack->getCable(id);rack->removeCable(cw);delete cw;
  }
  APP->scene->box.size=Vec(1400,900);APP->scene->step();
  owner->editSubpatch();APP->scene->step();owner->focusEditedMembers();APP->scene->step();owner->step();
  auto* originalInput=owner->find(aId)->getInput(0);
  auto originalCenter=[&](app::PortWidget* port){return port->getRelativeOffset(port->box.size*.5f,rack);};
  for(auto id:{c1,c2,c3})require((rack->getCable(id)->getInputPos()-originalCenter(originalInput)).norm()<.01f,"Edit must draw all incoming cables at the original module jack");
  require(APP->engine->getCable(c1)->inputModule==owner->module,"Edit presentation must retain the native attenuator routing");
  APP->engine->stepBlock(64);require(owner->find(bId)->module->outputs[0].getVoltage()==expected,"Edit must preserve incoming signal and summing");
  {
    auto* session=static_cast<beekapoo::RackEditSession*>(owner->editSession.get());
    widget::EventContext context;widget::Widget::ButtonEvent press;press.context=&context;press.button=GLFW_MOUSE_BUTTON_LEFT;press.action=GLFW_PRESS;press.mods=0;
    press.pos=originalInput->getAbsoluteOffset(originalInput->box.size*.5f)-session->box.pos;
    session->onButton(press);
    require(rack->getIncompleteCables().size()==1,"Clicking the original input must start a native cable drag");
    require(!APP->engine->getCable(c3),"Clicking the original input must pick up its visible exterior cable, not the hidden bridge");
    widget::Widget::DragDropEvent drop;drop.origin=originalInput;drop.button=GLFW_MOUSE_BUTTON_LEFT;owner->find(aId)->getInput(1)->onDragDrop(drop);
    APP->event->setDraggedWidget(nullptr,GLFW_MOUSE_BUTTON_LEFT);owner->step();
    require(rack->getCable(c3)&&(rack->getCable(c3)->getInputPos()-originalCenter(owner->find(aId)->getInput(1))).norm()<.01f,"Moving an exterior cable in Edit must use the newly selected original jack");
    APP->history->undo();owner->step();
    require((rack->getCable(c3)->getInputPos()-originalCenter(originalInput)).norm()<.01f,"Undo must restore the original visible endpoint");
    widget::EventContext menuContext;press.context=&menuContext;press.button=GLFW_MOUSE_BUTTON_RIGHT;
    session->onButton(press);
    beekapoo::EditedInputDeleteItem* remove=nullptr;ui::MenuOverlay* overlay=nullptr;
    for(auto* child:APP->scene->children)if(auto* candidate=dynamic_cast<ui::MenuOverlay*>(child))for(auto* sub:candidate->children)if(auto* menu=dynamic_cast<ui::Menu*>(sub))for(auto* item:menu->children)if(auto* safe=dynamic_cast<beekapoo::EditedInputDeleteItem*>(item)){remove=safe;overlay=candidate;}
    require(remove&&!remove->disabled,"Original input context menu must include the exterior cables");
    widget::Widget::ActionEvent action;remove->onAction(action);owner->step();
    require(!APP->engine->getCable(c3),"Context-menu deletion must remove the exterior cable");
    APP->scene->removeChild(overlay);delete overlay;APP->history->undo();owner->step();
    APP->engine->stepBlock(64);require(owner->find(bId)->module->outputs[0].getVoltage()==expected,"Undo of context-menu deletion must restore the signal");
  }
  static_cast<beekapoo::RackEditSession*>(owner->editSession.get())->finish(true);owner->step();
  require(rack->getCable(c1)->inputPort->module==owner->module,"Done must return the cable endpoint to the exposed input");
  rack->deselectAll();rack->select(s1,true);rack->select(s2,true);owner->addSelection();owner->step();
  require(owner->members.size()==4,"Both incoming modules must join the subpatch");
  require(APP->engine->getCable(c1)->inputModule->id==aId && APP->engine->getCable(c2)->inputModule->id==aId,"Newly internal sources must route directly to their original destination");
  require(APP->engine->getCable(c3)->inputModule==owner->module,"The outside source must retain its exposed attenuated input");
  require(!rack->getCable(c1)->visible&&!rack->getCable(c2)->visible&&rack->getCable(c3)->visible,"Only the remaining exterior cable should be visible");
  APP->engine->stepBlock(64);require(b->module->outputs[0].getVoltage()==expected,"Absorption must preserve summed signal values");
  APP->history->undo();owner->step();
  require(owner->members.size()==2&&rack->getModule(s1Id)->visible&&rack->getModule(s2Id)->visible,"Undo absorption must release both incoming modules");
  APP->engine->stepBlock(64);require(owner->find(bId)->module->outputs[0].getVoltage()==expected,"Undo must preserve the sum");
  APP->history->redo();owner->step();
  // Adding the last outside source removes only the now-internal exposure.
  rack->deselectAll();rack->select(s3,true);owner->addSelection();owner->step();
  require(!owner->exposed(owner->find(aId)->getInput(0)),"Fully internalized input must disappear from the panel");
  // Native drag starting at an external Beek input and ending at its own
  // exposed output must produce a visible cable, not a hidden internal wire.
  owner->expose(owner->find(aId),owner->find(aId)->getInput(1));
  owner->expose(owner->find(bId),owner->find(bId)->getOutput(0));owner->step();
  beekapoo::PortFace *input=nullptr,*output=nullptr;
  for(const auto& p:owner->ports) {if(p.id==aId&&p.input&&p.portId==1)input=p.face;if(p.id==bId&&!p.input&&p.portId==0)output=p.face;}
  require(input&&output,"Self-patch endpoints must exist");
  widget::EventContext context;widget::Widget::ButtonEvent click;click.context=&context;click.button=GLFW_MOUSE_BUTTON_LEFT;click.action=GLFW_PRESS;click.mods=0;
  input->onButton(click);require(rack->getIncompleteCables().size()==1,"Input click must immediately start a visible cable");
  widget::Widget::DragDropEvent drop;drop.origin=input->source.get();drop.button=GLFW_MOUSE_BUTTON_LEFT;output->onDragDrop(drop);
  APP->event->setDraggedWidget(nullptr,GLFW_MOUSE_BUTTON_LEFT);owner->step();
  bool visible=false;for(auto* cw:rack->getCompleteCables())if(cw->inputPort==input->source.get()&&cw->outputPort==output->source.get())visible=cw->visible;
  require(visible,"A completed Beek output-to-input self-patch must remain visible on the main rack");
  auto* patch=APP->engine->toJson();rack->mergeJson(patch);
  int64_t ownerId=owner->module->id;rack->clear();APP->engine->fromJson(patch);rack->fromJson(patch);json_decref(patch);
  owner=static_cast<beekapoo::Beekapoo*>(rack->getModule(ownerId));owner->step();
  require(owner->members.size()==5,"Membership and input routes must survive native patch reload");
  auto* state=owner->getModule<beekapoo::Module>();
  int route=owner->ports.front().route;
  for(const auto& p:owner->ports)if(p.route>=0){route=p.route;break;}
  require(route>=0,"Reload fixture needs an attenuated input");
  state->inputs[6+route].channels=0;state->process({});
  require(state->outputs[route].getChannels()==0,"Unpatched attenuator must not defeat normalization");
  state->inputs[6+route].channels=2;state->inputs[6+route].setVoltage(3,0);state->inputs[6+route].setVoltage(-4,1);state->process({});
  require(state->outputs[route].getChannels()==2&&state->outputs[route].getVoltage(0)==3&&state->outputs[route].getVoltage(1)==-4,"Connecting after an unpatched interval must immediately restore all signal channels");
  std::set<int64_t> beforeIds;for(const auto& member:owner->members)beforeIds.insert(member.id);
  auto* firstFace=owner->ports.front().face;
  engine::Module::ProcessArgs args{};
  for(int i=0;i<200;++i) {
    state->inputs[6].channels=1;state->inputs[6].setVoltage(i%2?10.f:-10.f);
    state->process(args);
    require(state->pendingActions.load()==0,"Audio on saved input 6 must never regenerate the patch");
    owner->step();
    require(owner->ports.front().face==firstFace,"Reloaded input activity must not rebuild the panel");
  }
  std::set<int64_t> afterIds;for(const auto& member:owner->members)afterIds.insert(member.id);
  require(beforeIds==afterIds,"Reload must preserve members while an old input receives audio");
  rack->clear();APP->history->clear();
  std::puts("Beek input attenuverters, stacked-source absorption, Undo/Redo, native self-patching and patch reload passed");
}
