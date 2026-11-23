#pragma once
template<class Add,class Require>
void testBeekCompact(const std::string& output,float scale,Add add,Require require) {
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
    if(n==2)require(owner->box.size.x==60,"Five controls and 3/1 ports must fit at four HP");
    std::vector<widget::Widget*> occupied;
    for(auto& p:owner->parameters){require(p.knob->visible,"Narrow sizing must retain every exposed parameter");occupied.push_back(p.knob);}
    for(auto& p:owner->ports){require(p.face->visible,"Narrow sizing must retain every exposed port");occupied.push_back(p.face);}
    for(auto* b:owner->managementButtons)occupied.push_back(b);
    for(auto* b:{owner->actionButtons[0],owner->actionButtons[1],owner->actionButtons[3],owner->actionButtons[4],owner->regenerateButton,owner->growButton})occupied.push_back(b);
    for(int id:{0,1,3,4,int(beekapoo::Module::GENERATE_INPUT),int(beekapoo::Module::GROW_INPUT)})occupied.push_back(owner->getInput(id));
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
    auto pos=owner->box.pos;rack->getModuleContainer()->removeChild(owner);
    owner->box.pos=Vec();capture(owner,output+"/compact-panel-"+std::to_string(n+1)+".png",scale,false);
    owner->box.pos=Vec(left,0);sheet.addChild(owner);parked.push_back({owner,pos});left+=owner->box.size.x+15;
  }
  sheet.box.size=Vec(left-15,380);capture(&sheet,output+"/compact-sizing-six.png",scale,false);
  for(auto& p:parked){sheet.removeChild(p.first);p.first->box.pos=p.second;rack->getModuleContainer()->addChild(p.first);}
  rack->clear();APP->history->clear();
  std::puts("Compact Beek: physical minimum, labels, all exposed controls, non-overlap, dark action contrast and source-state preservation passed");
}
