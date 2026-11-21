#pragma once
template <class Add, class Require>
void testBeekPlacement(const std::string& output,float scale,Add add,Require require) {
  auto* rack=APP->scene->rack;
  auto bounds=[](beekapoo::Beekapoo* owner,bool live) {
    Rect box;bool first=true;
    for(const auto& member:owner->members) {
      Rect next(live?member.widget->box.pos:member.position,member.widget->box.size);
      box=first?next:box.expand(next);first=false;
    }
    return box;
  };
  auto centered=[&](beekapoo::Beekapoo* owner) {
    auto* scroll=APP->scene->rackScroll;
    auto* session=static_cast<beekapoo::RackEditSession*>(owner->editSession.get());
    Vec topLeft=scroll->getGridOffset()*RACK_GRID_SIZE+RACK_OFFSET;
    Vec actual=(bounds(owner,true).getCenter()-topLeft)*scroll->getZoom();
    Vec expected(scroll->box.size.x*.5f,(scroll->box.size.y+session->toolbarHeight)*.5f);
    if(!((actual-expected).norm()<1.f))std::fprintf(stderr,"Center actual %.1f %.1f expected %.1f %.1f bounds %.1f %.1f / %.1f %.1f zoom %.2f offset %.1f %.1f\n",actual.x,actual.y,expected.x,expected.y,bounds(owner,true).pos.x,bounds(owner,true).pos.y,bounds(owner,true).size.x,bounds(owner,true).size.y,scroll->getZoom(),topLeft.x,topLeft.y);
    require((actual-expected).norm()<1.f,"Edit must center actual members, not an empty gap");
  };
  // Optional reproduction uses copies of contained Fundamental state only.
  // Never load the outer patch's Audio/Portaloof or modify the user's file.
  if(const char* file=std::getenv("BEEKAPOO_RECOVER_PATCH")) {
    auto* patch=json_load_file(file,0,nullptr);require(patch,"Reproduction patch must be readable");
    auto* list=json_object_get(patch,"modules");int number=0;
    for(size_t i=0;i<json_array_size(list);++i) {
      auto* item=json_array_get(list,i);auto* data=json_object_get(item,"data");
      auto* saved=json_object_get(data,"group");if(!json_is_object(saved))continue;
      rack->clear();APP->history->clear();
      auto* owner=static_cast<beekapoo::Beekapoo*>(add(modelBeekapooPreview,RACK_OFFSET));
      auto* root=json_deep_copy(saved);json_object_set(root,"contents",json_object_get(data,"contents"));
      require(owner->importContents(root),"Saved subpatch must import without its audio interface");json_decref(root);
      require(bounds(owner,false).size.y<=2*RACK_GRID_HEIGHT,"Broken saved rows must recover to a visible block");
      APP->scene->box.size=Vec(1200,740);APP->scene->rackScroll->box=Rect(Vec(0,30),Vec(1200,710));
      owner->requestedWidth=150;owner->layoutControls();owner->editSubpatch();centered(owner);
      auto* session=static_cast<beekapoo::RackEditSession*>(owner->editSession.get());
      for(int frame=0;frame<3;++frame){APP->scene->rackScroll->step();session->step();}
      capture(APP->scene,output+string::f("/recovered-edit-%d.png",++number),scale,false);
      session->finish(true);
      renderGeneratedBeekRow(owner,output+string::f("/recovered-patch-%d.png",number),scale);
      Vec origin=owner->box.pos;auto first=owner->members.front().id;
      owner->ejectAction();require((rack->getModule(first)->box.pos-origin).norm()<RACK_GRID_HEIGHT,"Recovered Eject must release beside the owner");
    }
    json_decref(patch);rack->clear();APP->history->clear();
    require(number>0,"Reproduction must contain a Beek");return;
  }
  rack->clear();APP->history->clear();
  APP->scene->box.size=Vec(1200,740);APP->scene->rackScroll->box=Rect(Vec(0,30),Vec(1200,710));
  auto* owner=static_cast<beekapoo::Beekapoo*>(add(modelBeekapooPreview,RACK_OFFSET+Vec(600,0)));
  require(owner->generateDefaultPatch(4),"Placement test needs a real generated patch");
  // Collapsing must not use the visible panel as a sibling anchor. Its view
  // can have a different parent while the actual modules stay in Rack.
  for(bool escape:{false,true}) {
    owner->editSubpatch();
    auto moduleIds=APP->engine->getModuleIds();
    std::set<int64_t> cableIds;
    for(auto* cable:rack->getCompleteCables())cableIds.insert(cable->cable->id);
    auto* container=rack->getModuleContainer();widget::Widget panelView;
    container->removeChild(owner);panelView.addChild(owner);
    auto* session=static_cast<beekapoo::RackEditSession*>(owner->editSession.get());
    if(escape) {
      widget::EventContext context;widget::Widget::HoverKeyEvent key;
      key.context=&context;key.key=GLFW_KEY_ESCAPE;key.action=GLFW_PRESS;
      session->onHoverKey(key);
      require(key.isConsumed(),"Escape must consume the edit-close key");
    } else session->done->action();
    panelView.removeChild(owner);container->addChild(owner);
    require(!owner->editSession&&!owner->expanded,"Done and Escape must finish with a temporarily reparented panel");
    require(APP->engine->getModuleIds()==moduleIds,"Finishing Edit must preserve every engine module");
    std::set<int64_t> after;
    for(auto* cable:rack->getCompleteCables())after.insert(cable->cable->id);
    require(after==cableIds,"Finishing Edit must preserve every cable");
    for(const auto& member:owner->members)
      require(member.holder&&member.holder->parent==container&&rack->getModule(member.id)==member.holder.get(),"Hidden members must stay registered directly in Rack");
  }
  // The actual bug: four members have absolute positions, a newly grown one
  // is 103 rows away. Save/reload must repair once without further movement.
  for(auto& m:owner->members)m.position+=RACK_OFFSET+Vec(0,103*RACK_GRID_HEIGHT);
  owner->members.back().position.y-=103*RACK_GRID_HEIGHT;owner->hasEditPlacement=true;
  owner->saveGroup();auto* saved=owner->module->dataToJson();owner->module->dataFromJson(saved);json_decref(saved);owner->step();
  require(bounds(owner,false).size.y==RACK_GRID_HEIGHT,"Saved separated rows must be compacted on restore");
  for(bool editing:{false,true}) {
    if(editing)owner->editSubpatch();
    size_t count=owner->members.size();bool added=false;
    for(int attempt=0;attempt<80&&!added;++attempt){require(owner->growPatch(),"Growth must be possible");added=owner->members.size()>count;}
    require(added&&bounds(owner,editing).size.y==RACK_GRID_HEIGHT,"Grow must inherit the same absolute row, including while editing");
    if(editing){require(bool(owner->editSession),"Grow must not close Edit");owner->focusEditedMembers();centered(owner);static_cast<beekapoo::RackEditSession*>(owner->editSession.get())->finish(true);}
  }
  auto previous=bounds(owner,false);owner->saveGroup();saved=owner->module->dataToJson();owner->module->dataFromJson(saved);json_decref(saved);owner->step();
  require(bounds(owner,false).pos==previous.pos&&bounds(owner,false).size==previous.size,"Good placement must stay stable on reload");
  owner->editSubpatch();centered(owner);static_cast<beekapoo::RackEditSession*>(owner->editSession.get())->finish(true);
  Vec position=owner->box.pos;auto first=owner->members.front().id;owner->ejectAction();
  require(rack->getModule(first)->box.pos==position,"Eject must start at the owner when space is available");
  rack->clear();APP->history->clear();
  std::puts("Saved far-row recovery, absolute-coordinate Grow/Edit, viewport centering, reload stability and close Eject passed");
}
