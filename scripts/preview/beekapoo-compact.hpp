#pragma once
template<class Add,class Require>
void testBeekCompact(const std::string& output,float scale,Add add,Require require) {
  {
    struct Frames : widget::Widget {
      void draw(const DrawArgs& args) override {
        for(int theme=0;theme<2;++theme)for(unsigned profile=0;profile<5;++profile) {
          beekapoo::SectionSkin skin;skin.profile=profile;skin.seed=12345;
          skin.rounding=profile==1?5.f:0.f;
          std::array<Vec,4> corners{{Vec(4,3),Vec(85,5),Vec(87,65),Vec(3,64)}};
          nvgSave(args.vg);nvgTranslate(args.vg,profile*100+3,theme*82+3);
          for(const auto& points:beekapoo::sectionPolygons(corners,skin))
            beekapoo::SectionGeometry(points,Vec(6,3.9f),skin.rounding).draw(args.vg,theme?nvgRGB(48,48,48):nvgRGB(205,205,205));
          nvgRestore(args.vg);
        }
      }
    } frames;
    for(float depth:{0.f,1.f,4.f,8.f})for(unsigned profile=0;profile<5;++profile) {
      beekapoo::SectionSkin skin;skin.profile=profile;skin.seed=12345;skin.rounding=profile==1?5.f:0.f;
      std::array<Vec,4> corners{{Vec(4,3),Vec(85,5),Vec(87,65),Vec(3,64)}};
      for(const auto& points:beekapoo::sectionPolygons(corners,skin)) {
        beekapoo::SectionGeometry shape(points,Vec(depth,depth*.65f),skin.rounding);
        require(shape.front.size()>=4&&shape.silhouette.size()>=4,"Perspective frame has closed front and outer contours");
        if(skin.rounding>0)require(shape.front.size()>points.size()&&shape.creases.empty(),"Rounded sides use the rounded front contour without square-corner seams");
        if(depth==0)require(shape.sides.empty()&&shape.creases.empty(),"Flat frames have no extruded edges");
        for(const auto& side:shape.sides) {
          require(beekapoo::SectionGeometry::cross(side.points[1]-side.points[0],Vec(depth,depth*.65f))<0,"Only visible walls are generated");
          for(Vec p:side.points)for(size_t i=0;i<shape.silhouette.size();++i) {
            Vec a=shape.silhouette[i],b=shape.silhouette[(i+1)%shape.silhouette.size()];
            require(beekapoo::SectionGeometry::cross(b-a,p-a)>=-.01f,"Every wall lies within the one outer silhouette");
          }
        }
      }
    }
    frames.box.size=Vec(500,164);capture(&frames,output+"/section-perspective.png",scale,false);
  }
  {
    bool sawEjectOnly=false;
    for(int count:{1,10,100,1000})for(int width=45;width<=900;width+=15) {
      auto h=beekapoo::Beekapoo::headerLayout(width,count,12);
      require(std::abs(h.logo.getRight()-(width-8))<.01f,"Header logo stays on the right");
      if(h.buttons[3].pos.y<h.logo.getBottom())require(h.buttons[3].getRight()<h.logo.pos.x,"Inline menu stays inside the logo");
      require(!h.buttons[3].intersects(h.logo),"Menu must not overlap the right-aligned logo");
      require(h.visible[0]&&h.visible[3],"Edit and menu always remain in header");
      require(!h.visible[2]||h.visible[1],"Save hides before Eject");
      if(h.visible[1]&&!h.visible[2])sawEjectOnly=true;
      for(int i=0;i<4;++i)if(h.visible[i]) {
        require(h.buttons[i].pos.y==h.buttons[0].pos.y,"Header buttons never wrap");
        require(h.buttons[i].pos.x>=5&&h.buttons[i].getRight()<=width-5,"Header buttons stay within bounds");
        for(int j=0;j<i;++j)if(h.visible[j])require(!h.buttons[i].intersects(h.buttons[j]),"Header buttons do not overlap");
      }
    }
    require(sawEjectOnly,"Intermediate header sizes retain Eject after hiding Save");
    auto minimum=beekapoo::Beekapoo::headerLayout(45,10);
    require(!minimum.visible[1]&&!minimum.visible[2],"Smallest header shows only Edit and menu");
  }
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
  {
    beekapoo::Beekapoo browser(nullptr);
    browser.step();
    require(browser.browserName=="Beekapoo"&&browser.box.size.x==120,"Browser preview title must identify the module, not use a generated patch name");
    capture(&browser,output+"/browser-title.png",scale,false);
  }
  auto* rack=APP->scene->rack;
  auto* model=plugin::getModel("computerscare","demo-gain");
  struct Case {int params,ins,outs,width;};
  const Case cases[]={{0,1,1,45},{3,1,1,45},{5,3,1,45},{8,2,2,75},{10,5,4,150},{6,3,2,360}};
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
    for(auto* b:owner->managementButtons)if(b->visible)occupied.push_back(b);
    for(auto* b:{owner->actionButtons[0],owner->actionButtons[1],owner->actionButtons[3],owner->actionButtons[4],owner->regenerateButton,owner->growButton})if(b->visible)occupied.push_back(b);
    for(int id:{beekapoo::Module::RANDOM_RANGE,beekapoo::Module::RANDOM_PROBABILITY,beekapoo::Module::WIGGLE_RANGE,beekapoo::Module::WIGGLE_PROBABILITY}) {
      auto* knob=owner->getParam(id);if(knob->visible)occupied.push_back(knob);
    }
    occupied.push_back(owner->getInput(beekapoo::Module::ACTIONS_INPUT));
    if(owner->stepSelector->visible)occupied.push_back(owner->stepSelector);
    for(auto* b:owner->sequenceButtons)if(b->visible)occupied.push_back(b);
    require(!owner->getParam(beekapoo::Module::SEQUENCE_SCAN),"Manual Scan knob is absent at every width");
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
    require(std::abs(plan.positions[5].x+16-(owner->box.size.x-(owner->box.size.x<60?5:6)))<.01f,"Actions CV stays right-aligned even when its row wraps");
    require(owner->getInput(beekapoo::Module::ACTIONS_INPUT)->visible,"CV jack is never hidden");
    require(owner->actionButtons[0]->visible&&owner->actionButtons[1]->visible,"Default narrow strip retains both exposed actions");
    require(owner->actionButtons[0]->box.size.x==18&&owner->actionButtons[1]->box.size.x==18,"Wiggle and random buttons keep their normal size even at minimum width");
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
      require(menu&&menu->children.size()==7,"Functions menu must include Save and Eject alongside patch functions");
      auto it=menu->children.begin();
      require(dynamic_cast<ui::MenuItem*>(*it++)->text=="Grow"&&dynamic_cast<ui::MenuItem*>(*it)->text=="Compact subpatch","Grow must be the first function");
      bool save=false,eject=false,unlock=false;
      for(auto* child:menu->children)if(auto* item=dynamic_cast<ui::MenuItem*>(child)) {
        unlock|=item->text=="Unlock all";
        save|=item->text=="Save Beek";eject|=item->text=="Eject";
      }
      require(save&&eject&&unlock,"Hidden header actions stay accessible from the menu");
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
  {
    auto* voice=add(model,Vec(0,0));
    auto* owner=static_cast<beekapoo::Beekapoo*>(add(modelBeekapooPreview,Vec(240,0)));
    rack->deselectAll();rack->select(voice,true);owner->collapse();
    owner->getModule<beekapoo::Module>()->setPatchName("Munkle Poors",true);
    for(int i=0;i<7;++i)owner->exposeParameter(voice->module->id,i);
    const char* inputs[]={"Mult","Clock","Gate","Gate"};
    for(int i=0;i<4;++i){voice->module->getInputInfo(i)->name=inputs[i];owner->expose(voice,voice->getInput(i));}
    for(int i=0;i<2;++i){voice->module->getOutputInfo(i)->name=i?"Right":"Left";owner->expose(voice,voice->getOutput(i));}
    for(float width:{90.f,120.f}) {
      owner->getModule<beekapoo::Module>()->panelTheme.store(width==90?1:2);
      owner->requestedWidth=width;owner->layoutControls();
      auto* container=owner->parent;auto pos=owner->box.pos;container->removeChild(owner);owner->box.pos=Vec();
      capture(owner,output+"/stereo-labels-"+std::to_string(int(width))+".png",scale,false);
      int checked=0;
      for(const auto& label:owner->controlLabels)if(label.output&&(label.text=="Left"||label.text=="Right")) {
        auto full=computerscare::type::alternates(beekapoo::labelCase(label.text,owner->appearance.modules),owner->appearance.modules);
        require(label.readable&&label.lines==std::vector<std::string>{full},"Stereo outputs keep full Left/Right labels when they fit");
        ++checked;
      }
      require(checked==2,"Stereo fixture checks both output labels");
      owner->box.pos=pos;container->addChild(owner);
    }
  }
  rack->clear();APP->history->clear();
  {
    std::vector<app::ModuleWidget*> voices;
    for(int i=0;i<10;++i) voices.push_back(add(model,Vec(i*200,0)));
    auto* owner=static_cast<beekapoo::Beekapoo*>(add(modelBeekapooPreview,Vec(2400,0)));
    rack->deselectAll();for(auto* voice:voices)rack->select(voice,true);owner->collapse();
    owner->getModule<beekapoo::Module>()->setPatchName("Rasp Hazel",true);
    owner->getModule<beekapoo::Module>()->panelTheme.store(1);
    auto* voice=voices.front();
    for(int i=0;i<6;++i)owner->exposeParameter(voice->module->id,i);
    const char* names[]={"Pitch","Clock","FM"};
    for(int i=0;i<3;++i){voice->module->getInputInfo(i)->name=names[i];owner->expose(voice,voice->getInput(i));}
    for(int i=0;i<2;++i){voice->module->getOutputInfo(i)->name=i?"Channel":"Saw";owner->expose(voice,voice->getOutput(i));}
    for(float width:{60.f,90.f,180.f}) {
      owner->requestedWidth=width;owner->layoutControls();
      require(owner->managementButtons[0]->box.pos.y==owner->growButton->box.pos.y,"Reference layout keeps Edit and menu beside each other");
      if(width==60) {
        require(!owner->managementButtons[1]->visible&&!owner->managementButtons[2]->visible,"Reference narrow header hides Save then Eject");
        require(owner->inputFrame->box.size.y<85,"Three inputs do not consume excess height");
        require(owner->frame->box.size.y>owner->inputFrame->box.size.y,"Parameters get more space than inputs");
      }
      auto* container=owner->parent;auto pos=owner->box.pos;container->removeChild(owner);owner->box.pos=Vec();
      capture(owner,output+"/rasp-hazel-"+std::to_string(int(width))+".png",scale,false);
      owner->box.pos=pos;container->addChild(owner);
    }
    const char* titles[]={"Honeydip","Rasp Hazel","Low Moss","Crankshaft","Tin Hinge","Windmill"};
    const float widths[]={45,60,90,75,120,180};
    for(int i=0;i<6;++i) {
      owner->getModule<beekapoo::Module>()->setPatchName(titles[i],true);
      owner->getModule<beekapoo::Module>()->panelTheme.store(i%2?2:1);
      owner->requestedWidth=widths[i];owner->layoutControls();
      auto title=owner->headerArtworkArea,logo=owner->headerLogoArea;
      require(!title.intersects(logo),"Adaptive header title and logo never overlap");
      require(title.getBottom()<owner->managementButtons[0]->box.pos.y,"Adaptive artwork stays above header controls");
      if(i==0)require(title.pos.y>logo.getBottom(),"Narrow single word title sits below logo");
      if(title.pos.y>logo.getBottom())require(std::abs(logo.getCenter().x-owner->box.size.x*.5f)<.01f,"Logo is centered when the title sits below it");
      if(i==2)require(title.getRight()<logo.pos.x,"Short wrapped title fits beside logo");
      auto* container=owner->parent;auto pos=owner->box.pos;container->removeChild(owner);owner->box.pos=Vec();
      capture(owner,output+"/adaptive-header-"+std::to_string(i+1)+".png",scale,false);
      owner->box.pos=pos;container->addChild(owner);
    }

  }
  rack->clear();APP->history->clear();
  std::puts("Compact Beek: physical minimum, labels, all exposed controls, non-overlap, dark action contrast and source-state preservation passed");
}
