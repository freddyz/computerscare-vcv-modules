#pragma once
template<class Add, class Require>
void testBeekPanelGroups(const std::string& output,float scale,Add add,Require require,plugin::Model* model) {
  auto* rack=APP->scene->rack;
  std::vector<app::ModuleWidget*> voices;
  for(int i=0;i<3;++i)voices.push_back(add(model,Vec(i*100,0)));
  auto* owner=static_cast<beekapoo::Beekapoo*>(add(modelBeekapooPreview,Vec(500,0)));
  rack->deselectAll();for(auto* voice:voices)rack->select(voice,true);owner->collapse();
  // Keep this topology explicit, independent of automatic initial exposure.
  while(!owner->parameters.empty()) {
    auto p=owner->parameters.back();owner->controls->removeChild(p.knob);delete p.knob;owner->parameters.pop_back();
  }
  while(!owner->ports.empty()) {
    auto& p=owner->ports.back();owner->detachInput(p);owner->controls->removeChild(p.face);delete p.face;owner->ports.pop_back();
  }
  for(int i=0;i<3;++i) {
    owner->exposeParameter(voices[i]->module->id,0);
    owner->exposeParameter(voices[i]->module->id,i==1?1:2);
    owner->expose(voices[i],voices[i]->getInput(0));
    owner->expose(voices[i],voices[i]->getOutput(0));
  }
  auto* m=owner->getModule<beekapoo::Module>();m->setPatchName("Rowan kettle",true);
  std::vector<float> values;
  for(auto* voice:voices)for(auto& p:voice->module->params)values.push_back(p.getValue());
  auto unchanged=[&] {
    int at=0;for(auto* voice:voices)for(auto& p:voice->module->params)
      require(p.getValue()==values[at++],"Layout/lock changes never alter source parameter values");
  };
  auto key=[&](int code,int action=GLFW_PRESS) {
    widget::EventContext context;widget::Widget::HoverKeyEvent e;
    e.context=&context;e.key=code;e.action=action;e.mods=0;owner->onHoverKey(e);
  };
  auto click=[&](app::ParamWidget* p) {
    widget::EventContext context;widget::Widget::ButtonEvent e;
    e.context=&context;e.pos=p->box.getCenter();e.action=GLFW_PRESS;e.button=GLFW_MOUSE_BUTTON_LEFT;e.mods=0;owner->onButton(e);
    require(e.isConsumed(),"Lock click is consumed before knob/button handling");
    require(e.getTarget()==nullptr,"Lock clicks must not arm Rack's module dragging");
  };
  owner->layoutControls();key(GLFW_KEY_L);key(GLFW_KEY_L,GLFW_REPEAT);
  require(owner->frontLockMode,"L toggles front locks once, ignoring repeats");
  auto first=owner->parameters.front();
  Vec lockedPosition=owner->box.pos;
  rack->select(owner,true); // Selected modules must not bypass lock handling either.
  click(first.knob);owner->step();
  require(owner->box.pos==lockedPosition,"Locking leaves module position unchanged");
  require(owner->randomLocked(first.id,first.paramId),"Front click locks original parameter");unchanged();
  click(first.knob);require(!owner->randomLocked(first.id,first.paramId),"Front click unlocks original parameter");
  owner->toggleRandomLock(first.id,-1);click(first.knob);
  require(!owner->randomLocked(first.id,first.paramId)&&owner->randomLocked(first.id,23),"Front unlock splits module lock without unlocking other parameters");
  auto momentary=owner->parameters[3];click(momentary.knob);unchanged();
  key(GLFW_KEY_ESCAPE);require(!owner->frontLockMode,"Escape leaves front lock mode");
  for(int mode=1;mode<=2;++mode)for(float width:{60.f,90.f,150.f,240.f,360.f}) {
    owner->arrangeByModule=true;owner->groupPortsByModule=mode==2;owner->requestedWidth=width;
    owner->layoutControls();unchanged();
    int shown=0;for(auto* b:owner->memberFrames)shown+=b->visible;
    require(shown==3,"Every represented module gets a separate block");
    std::vector<Rect> occupied;
    for(auto& p:owner->parameters) {
      require(p.knob->visible,"Every grouped parameter remains visible");
      bool inside=false;for(auto* b:owner->memberFrames)if(b->visible)
        inside|=b->box.contains(p.knob->box.pos)&&b->box.contains(p.knob->box.getBottomRight());
      require(inside,"Grouped parameters stay within module blocks");occupied.push_back(p.knob->box);
    }
    for(auto& p:owner->ports) {require(p.face->visible,"Every grouped jack remains visible");occupied.push_back(p.face->box);}
    for(size_t i=0;i<occupied.size();++i)for(size_t j=i+1;j<occupied.size();++j)
      require(!occupied[i].intersects(occupied[j]),"Grouped controls never overlap");
    if(width==150||width==240) {
      m->panelTheme.store((mode==1)==(width==240)?1:2);owner->applyTheme();
      owner->setFrontLockMode(mode==2);owner->saveGroup();
      auto* parent=owner->parent;auto pos=owner->box.pos;parent->removeChild(owner);owner->box.pos=Vec();
      capture(owner,output+"/module-groups-"+std::to_string(mode)+"-"+std::to_string(int(width))+".png",scale,false);
      owner->box.pos=pos;parent->addChild(owner);
    }
  }
  owner->saveGroup();auto* data=m->dataToJson();
  m->dataFromJson(data);json_decref(data);owner->step();
  require(owner->arrangeByModule&&owner->groupPortsByModule,"Reload retains all-controls-by-module arrangement");
  require(owner->randomLocked(first.id,23),"Reload retains locks made from the front");
  auto savedLocks=owner->randomLocks;
  Vec unlockPosition=owner->box.pos;
  owner->growButton->action();
  auto* overlay=dynamic_cast<ui::MenuOverlay*>(APP->scene->children.back());
  require(overlay,"Main functions menu opens");
  auto* menu=dynamic_cast<ui::Menu*>(overlay->children.front());
  ui::MenuItem* unlock=nullptr;
  for(auto* child:menu->children)if(auto* item=dynamic_cast<ui::MenuItem*>(child))if(item->text=="Unlock all")unlock=item;
  require(unlock&&!unlock->disabled,"Main menu exposes Unlock all when locks exist");
  widget::Widget::ActionEvent action;unlock->onAction(action);
  require(owner->randomLocks.empty()&&owner->box.pos==unlockPosition,"Unlock all clears both parameter and module locks without moving the panel");
  overlay->parent->removeChild(overlay);delete overlay;
  APP->history->undo();owner->step();require(owner->randomLocks==savedLocks,"Unlock all is undoable");
  owner->toggleFrontLock(first.id,first.paramId);
  APP->history->undo();owner->step();
  require(owner->randomLocks==savedLocks,"Undo restores front-panel locks");
  APP->history->redo();owner->step();
  require(owner->randomLocked(first.id,first.paramId),"Redo restores front-panel lock changes");
  owner->arrangeByModule=false;owner->layoutControls();
  for(auto* b:owner->memberFrames)require(!b->visible,"Switching back hides module blocks");
  rack->clear();APP->history->clear();
  std::puts("Beek front locks and module blocks: native clicks, module-lock exceptions, source-state preservation, dense sizing, reload passed");
}
