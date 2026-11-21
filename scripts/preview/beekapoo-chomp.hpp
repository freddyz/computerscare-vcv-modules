#pragma once
template<class Add, class Require>
void renderBeekChomp(const std::string& output,float scale,Add add,Require require) {
  auto* rack=APP->scene->rack;
  std::vector<beekapoo::Beekapoo*> owners;
  for(int example=0;example<2;++example) {
    auto* owner=static_cast<beekapoo::Beekapoo*>(add(modelBeekapooPreview,Vec(example*2000,0)));
    require(owner->generateDefaultPatch(3+example),"Chomp fixture must use real Fundamental modules");
    owner->getModule<beekapoo::Module>()->panelTheme.store(example?2:1);
    owner->requestedWidth=example?210:150;owner->layoutControls();owner->step();
    owners.push_back(owner);
  }
  rack->deselectAll();rack->select(owners[1],true);
  require(!owners[1]->canAddSelection()&&owners[0]->canAddSelection(),"Only the selected Beek itself must stay idle; every other Beek can absorb it");
  rack->select(owners[0],true);
  for(auto* w:owners)require(w->canAddSelection(),"Both selected Beeks can absorb one another");
  auto* outside=add(plugin::getModel("Fundamental","LFO"),Vec(5000,0));
  auto outsideId=outside->module->id;
  for(auto* owner:owners) {
    for(float width:{60.f,75.f,90.f,120.f,180.f,360.f}) {
      rack->deselectAll();owner->requestedWidth=width;owner->layoutControls();owner->step();
      Rect edit=owner->managementButtons[0]->box,eject=owner->managementButtons[1]->box;
      float headerHeight=owner->titleFrame->box.size.y;
      rack->select(outside,true);rack->select(owner,true);owner->step();
      auto* logo=owner->addSelectionButton;
      require(logo->visible,"Selecting an outside module with its destination must enable the logo");
      require(edit.pos==owner->managementButtons[0]->box.pos&&eject.pos==owner->managementButtons[1]->box.pos&&headerHeight==owner->titleFrame->box.size.y,"Selection must not move the header controls or resize it");
      require(!logo->box.intersects(edit)&&!logo->box.intersects(eject),"Logo hit target must not overlap Edit or Eject");
      require(logo->box.pos==owner->logoTarget().pos&&logo->box.size==owner->logoTarget().size,"Click target must track the header logo at every width");
      widget::EventContext hoverContext;widget::Widget::HoverEvent hover;
      hover.context=&hoverContext;hover.pos=logo->box.getCenter();owner->onHover(hover);
      require(hover.getTarget()==logo,"Hover must reach the logo even when its owner is selected");
    }
    auto* logo=owner->addSelectionButton;
    logo->hovered=false;logo->phase=0;logo->advance(.05f);float idle=logo->phase;
    widget::Widget::EnterEvent enter;logo->onEnter(enter);
    logo->phase=0;logo->advance(.05f);
    require(logo->hovered&&logo->phase>idle*2,"Hover must speed up the chomp");
    rack->deselectAll();owner->step();
    require(!logo->visible&&!logo->hovered&&!logo->tooltip&&logo->pose()==0,"Clearing selection must stop animation and remove its tooltip");
    require(owner->growButton->icon==9,"Grow must retain its own wavy plus");
    auto* m=owner->getModule<beekapoo::Module>();auto name=m->getPatchName();
    owner->requestedWidth=75;owner->layoutControls();
    for(const auto& caption:{std::string(),name}) {
      m->setPatchName(caption,true);owner->step();
      require(owner->addSelectionButton->box.pos==owner->logoTarget().pos&&owner->addSelectionButton->box.size==owner->logoTarget().size,"Renaming must keep the hit target on the logo in the narrow header");
    }
  }
  // Exercise actual header mouse dispatch, including the owner in selection.
  auto* owner=owners[0];auto ownerId=owner->module->id;
  rack->select(owner,true);rack->select(outside,true);owner->step();
  size_t before=owner->members.size();
  widget::EventContext context;widget::Widget::ButtonEvent click;
  click.context=&context;click.action=GLFW_PRESS;click.button=GLFW_MOUSE_BUTTON_LEFT;click.mods=0;
  click.pos=owner->addSelectionButton->box.getCenter();owner->onButton(click);owner->step();
  require(click.isConsumed()&&owner->members.size()==before+1&&owner->contains(outside->module),"Clicking the header logo must adopt the selection into the existing Beek");
  require(owner->module->id==ownerId&&!owner->addSelectionButton->visible,"Adoption keeps the owner and stops chomping");
  APP->history->undo();owner->step();
  require(owner->members.size()==before&&rack->getModule(outsideId)->visible,"One Undo restores the outside selection");
  APP->history->redo();owner->step();
  require(owner->contains(APP->engine->getModule(outsideId)),"Redo must re-adopt the same module");
  APP->history->undo();owner->step();

  rack->deselectAll();rack->select(rack->getModule(outsideId),true);
  widget::Widget sheet;float x=0;std::vector<Vec> positions;
  for(size_t i=0;i<owners.size();++i) {
    auto* w=owners[i];w->requestedWidth=i?210:150;w->layoutControls();w->step();
    w->addSelectionButton->phase=0;w->addSelectionButton->hovered=i==1;
    positions.push_back(w->box.pos);rack->getModuleContainer()->removeChild(w);
    w->box.pos=Vec(x,0);sheet.addChild(w);x+=w->box.size.x+16;
  }
  sheet.box.size=Vec(x-16,380);
  for(int frame=0;frame<40;++frame) {
    capture(&sheet,output+string::f("/chomp-%02d.png",frame),scale,false);
    for(auto* w:owners)w->addSelectionButton->advance(.05f);
  }
  for(size_t i=0;i<owners.size();++i) {
    sheet.removeChild(owners[i]);owners[i]->box.pos=positions[i];rack->addModule(owners[i]);
  }
  std::puts("Chomping logo: real mouse hit testing, owner-in-selection, Undo/Redo, stable header, hover speed, tooltip cleanup, light/dark and unchanged Grow passed");
}
