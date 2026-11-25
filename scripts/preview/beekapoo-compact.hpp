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
  {
    beekapoo::Module old;old.model=modelBeekapooPreview;
    old.params[beekapoo::Module::WIGGLE_RANGE].setValue(.2f);
    old.params[beekapoo::Module::RANDOM_RANGE].setValue(.5f);
    old.params[beekapoo::Module::RANDOM_PROBABILITY].setValue(.3f);
    auto* saved=old.toJson();json_object_del(json_object_get(saved,"data"),"separateRandomControls");
    beekapoo::Module restored;restored.model=modelBeekapooPreview;restored.fromJson(saved);json_decref(saved);
    require(std::abs(restored.params[beekapoo::Module::WIGGLE_RANGE].getValue()-.1f)<.00001f&&restored.params[beekapoo::Module::WIGGLE_PROBABILITY].getValue()==.3f,"Old shared controls must migrate to the same effective wiggle range and chance");
    saved=restored.toJson();restored.fromJson(saved);json_decref(saved);
    require(std::abs(restored.params[beekapoo::Module::WIGGLE_RANGE].getValue()-.1f)<.00001f,"Separate controls must not be migrated twice");
  }
  auto* rack=APP->scene->rack;
  auto* model=plugin::getModel("computerscare","demo-gain");
  struct Case {int params,ins,outs,width;};
  const Case cases[]={{0,1,1,45},{3,1,1,45},{5,3,1,45},{8,2,2,75},{10,5,4,150},{6,3,2,240}};
  widget::Widget sheet;float left=0;
  std::vector<std::pair<beekapoo::Beekapoo*,Vec>> parked;
  for(int n=0;n<6;++n) {
    auto c=cases[n];
    auto* voice=add(model,Vec(n*1000,0));
    const char* labels[]={"Frequency modulation","Cutoff frequency","Channel 3 CV","Resonance","Pulse width","Delay time","Mix","Level","Attack","Release"};
    for(int i=0;i<10;++i)voice->module->getParamQuantity(i)->name=labels[i];
    if(n>=4){voice->module->getParamQuantity(0)->name="Channel 3 level";voice->module->getParamQuantity(4)->name="Channel 4 level";}
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
    int groupedNumbers=0;
    for(auto& p:owner->parameters)if(auto* knob=dynamic_cast<beekapoo::ParametricKnob*>(p.knob)) {
      groupedNumbers+=knob->groupedNumber;
      require(knob->groupedNumber==(n>=4&&(knob->paramId==0||knob->paramId==4)),"Only controls sharing a family caption may show extra numbers; standalone labels already contain theirs");
    }
    require(groupedNumbers==(n>=4?2:0),"A numbered pair retains its separate numbers at different widths");
    for(const auto& caption:owner->controlLabels)require(caption.text!="Cutoff frequency","Momentary names belong on the face, without a duplicate outside caption");
    if(n<2)require(owner->box.size.x==45,"Small Beeks must reach the physical three-HP width");
    if(n==2)require(owner->box.size.x<=60,"Five controls and 3/1 ports must fit at four HP or less");
    std::vector<widget::Widget*> occupied;
    for(auto& p:owner->parameters){require(p.knob->visible,"Narrow sizing must retain every exposed parameter");occupied.push_back(p.knob);}
    for(auto& p:owner->ports){require(p.face->visible,"Narrow sizing must retain every exposed port");occupied.push_back(p.face);}
    for(auto* b:owner->managementButtons)occupied.push_back(b);
    for(auto* b:{owner->actionButtons[0],owner->actionButtons[1],owner->actionButtons[3],owner->actionButtons[4],owner->regenerateButton,owner->growButton})if(b->visible)occupied.push_back(b);
    for(int id:{beekapoo::Module::RANDOM_RANGE,beekapoo::Module::RANDOM_PROBABILITY,beekapoo::Module::WIGGLE_RANGE,beekapoo::Module::WIGGLE_PROBABILITY}) {
      auto* knob=owner->getParam(id);if(knob->visible)occupied.push_back(knob);
    }
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
    auto plan=beekapoo::Beekapoo::ownControlLayout(owner->box.size.x);
    require(owner->getInput(beekapoo::Module::ACTIONS_INPUT)->visible,"CV jack is never hidden");
    require(owner->actionButtons[0]->visible&&owner->actionButtons[1]->visible,"Default narrow strip retains both exposed actions");
    int visible=owner->regenerateButton->visible;
    for(auto* b:owner->actionButtons)visible+=b->visible;
    require(visible==plan.columns,"Only the highest priority buttons that fit should render");
    const int randomKnobs[]={beekapoo::Module::RANDOM_RANGE,beekapoo::Module::RANDOM_PROBABILITY,beekapoo::Module::WIGGLE_RANGE,beekapoo::Module::WIGGLE_PROBABILITY};
    for(int i=0;i<4;++i)require(owner->getParam(randomKnobs[i])->visible==(i<plan.knobCount),"Action knobs show only when their space fits");
    if(n==5)require(plan.knobCount==4,"All four action knobs must fit on a wider panel");
    if(n==0) {
      auto count=owner->ports.size();owner->growButton->action();
      auto* overlay=dynamic_cast<ui::MenuOverlay*>(APP->scene->children.back());
      require(overlay,"Functions button must open a native menu");
      auto* menu=dynamic_cast<ui::Menu*>(overlay->children.front());
      require(menu&&menu->children.size()==2,"Functions menu must contain Grow and Compact");
      auto it=menu->children.begin();
      require(dynamic_cast<ui::MenuItem*>(*it++)->text=="Grow"&&dynamic_cast<ui::MenuItem*>(*it)->text=="Compact subpatch","Grow must be the first function");
      require(owner->ports.size()==count,"Opening the menu must not edit the patch");
      menu->step();overlay->removeChild(menu);menu->box.pos=Vec();
      capture(menu,output+"/functions-menu.png",scale,false);
      overlay->addChild(menu);
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
