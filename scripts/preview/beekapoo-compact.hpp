#pragma once
template<class Add,class Require>
void testBeekCompact(const std::string& output,float scale,Add add,Require require) {
  {
    beekapoo::Module m;engine::Module::ProcessArgs args{};
    auto& cv=m.inputs[beekapoo::Module::ACTIONS_INPUT];cv.channels=16;
    m.process(args);
    for(int ch=0;ch<16;++ch) {
      cv.setVoltage(10,ch);m.process(args);
      unsigned expected=ch<6?1u<<beekapoo::Module::polyAction(ch):0;
      require(m.pendingActions.exchange(0)==expected,"Poly action channel must trigger only its assigned action");
      m.process(args);require(m.pendingActions.exchange(0)==0,"Held gate must not retrigger");
      cv.setVoltage(0,ch);m.process(args);
    }
    for(int ch=0;ch<6;++ch)cv.setVoltage(10,ch);
    m.process(args);require(m.pendingActions.exchange(0)==219,"Simultaneous action channels must retain every trigger");
    cv.channels=0;m.process(args);require(m.pendingActions.exchange(0)==0,"Disconnected poly CV must not trigger stale voices");
    cv.channels=1;m.process(args);
    require(m.pendingActions.exchange(0)==1,"Mono CV must only wiggle exposed parameters, never broadcast to other actions");
    cv.channels=0;m.process(args);
    m.inputs[1].setVoltage(10);m.process(args);
    require(m.pendingActions.exchange(0)==2,"Existing single-action CV ports must remain compatible");
  }
  auto* rack=APP->scene->rack;
  auto* model=plugin::getModel("computerscare","demo-gain");
  struct Case {int params,ins,outs,width;};
  const Case cases[]={{0,1,1,45},{3,1,1,45},{5,3,1,45},{8,2,2,75},{10,5,4,105},{6,3,2,180}};
  widget::Widget sheet;float left=0;
  std::vector<std::pair<beekapoo::Beekapoo*,Vec>> parked;
  for(int n=0;n<6;++n) {
    auto c=cases[n];
    auto* voice=add(model,Vec(n*1000,0));
    const char* labels[]={"Frequency modulation","Cutoff frequency","Channel 3 CV","Resonance","Pulse width","Delay time","Mix","Level","Attack","Release"};
    for(int i=0;i<10;++i)voice->module->getParamQuantity(i)->name=labels[i];
    auto* owner=static_cast<beekapoo::Beekapoo*>(add(modelBeekapooPreview,Vec(n*1000+240,0)));
    rack->deselectAll();rack->select(voice,true);owner->collapse();
    owner->getModule<beekapoo::Module>()->panelTheme.store(n%2?2:1);
    for(int i=0;i<c.params;++i) {
      auto* q=voice->module->getParamQuantity(i);
      q->setImmediateValue(q->getMinValue()+q->getRange()*float((i*7+n*3)%17)/16.f);
      owner->exposeParameter(voice->module->id,i);
    }
    for(int i=0;i<c.ins;++i)owner->expose(voice,voice->getInput(i));
    for(int i=0;i<c.outs;++i)owner->expose(voice,voice->getOutput(i));
    auto before=voice->module->toJson();
    owner->requestedWidth=c.width;owner->layoutControls();owner->step();
    if(n<2)require(owner->box.size.x==45,"Small Beeks must reach the physical three-HP width");
    if(n==2)require(owner->box.size.x<=60,"Five controls and 3/1 ports must fit at four HP or less");
    std::vector<widget::Widget*> occupied;
    for(auto& p:owner->parameters){require(p.knob->visible,"Narrow sizing must retain every exposed parameter");occupied.push_back(p.knob);}
    for(auto& p:owner->ports){require(p.face->visible,"Narrow sizing must retain every exposed port");occupied.push_back(p.face);}
    for(auto* b:owner->managementButtons)occupied.push_back(b);
    for(auto* b:{owner->actionButtons[0],owner->actionButtons[1],owner->actionButtons[3],owner->actionButtons[4],owner->regenerateButton,owner->growButton})occupied.push_back(b);
    occupied.push_back(owner->getInput(beekapoo::Module::ACTIONS_INPUT));
    for(size_t i=0;i<occupied.size();++i) {
      auto b=occupied[i]->box;
      require(b.pos.x>=0&&b.pos.y>=0&&b.getRight()<=owner->box.size.x&&b.getBottom()<=380,"Compact controls must stay on the panel");
      for(size_t j=0;j<i;++j)require(!b.intersects(occupied[j]->box),"Compact controls must not overlap");
    }
    auto after=voice->module->toJson();require(json_equal(before,after),"Resizing and changing theme must not write source parameters");json_decref(before);json_decref(after);
    if(n%2) {
      for(auto* b:{owner->actionButtons[0],owner->actionButtons[3],owner->regenerateButton,owner->growButton}) {
        require(b->faceColor.r<.3f && b->borderColor.r<=.571f,"Dark actions must use subdued dark faces and borders");
        require(b->textInks[phlooper::WarpedDropdown::UP]>.8f,"Dark action icons must remain clearly visible");
      }
      require(owner->actionButtons[3]->faceColor.r<owner->actionButtons[0]->faceColor.r,"All actions retain their darker face in dark mode");
    }
    if(n==0) {
      auto count=owner->ports.size();owner->growButton->action();
      auto* overlay=dynamic_cast<ui::MenuOverlay*>(APP->scene->children.back());
      require(overlay,"Functions button must open a native menu");
      auto* menu=dynamic_cast<ui::Menu*>(overlay->children.front());
      require(menu&&menu->children.size()==2,"Functions menu must contain Grow and Compact");
      auto it=menu->children.begin();
      require(dynamic_cast<ui::MenuItem*>(*it++)->text=="Grow"&&dynamic_cast<ui::MenuItem*>(*it)->text=="Compact subpatch","Grow must be the first function");
      require(owner->ports.size()==count,"Opening the menu must not edit the patch");
      APP->scene->removeChild(overlay);delete overlay;
    }
    auto pos=owner->box.pos;rack->getModuleContainer()->removeChild(owner);
    owner->box.pos=Vec();capture(owner,output+"/compact-panel-"+std::to_string(n+1)+".png",scale,false);
    owner->box.pos=Vec(left,0);sheet.addChild(owner);parked.push_back({owner,pos});left+=owner->box.size.x+15;
  }
  sheet.box.size=Vec(left-15,380);capture(&sheet,output+"/compact-sizing-six.png",scale,false);
  for(auto& p:parked){sheet.removeChild(p.first);p.first->box.pos=p.second;rack->getModuleContainer()->addChild(p.first);}
  rack->clear();APP->history->clear();
  std::puts("Compact Beek: physical minimum, labels, all exposed controls, non-overlap, dark action contrast and source-state preservation passed");
}
