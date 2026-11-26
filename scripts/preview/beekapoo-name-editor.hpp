#pragma once
template<class Add,class Require>
void testBeekNameEditor(const std::string& output,float scale,Add add,Require require) {
  auto* rack=APP->scene->rack; rack->clear(); APP->history->clear();
  auto* a=add(plugin::getModel("Fundamental","VCO"),Vec());
  auto* b=add(plugin::getModel("Fundamental","LFO"),Vec(90,0));
  auto* c=add(plugin::getModel("Fundamental","VCF"),Vec(180,0));
  auto* owner=static_cast<beekapoo::Beekapoo*>(add(modelBeekapooPreview,Vec(300,0)));
  rack->deselectAll();for(auto* w:{a,b,c})rack->select(w,true);owner->collapse();
  for(auto* w:{a,b,c})owner->exposeParameter(w->module->id,w->getParams()[0]->paramId);
  owner->exposeParameter(a->module->id,a->getParams()[1]->paramId);
  owner->exposeParameter(b->module->id,b->getParams()[1]->paramId);
  owner->expose(a,a->getInputs()[0]);owner->expose(c,c->getOutputs()[0]);
  owner->requestedWidth=150;owner->layoutControls();owner->step();
  auto* m=owner->getModule<beekapoo::Module>();
  owner->resetNameTopology();
  auto generatedName=m->getPatchName();
  auto members=owner->members;
  std::string exact="MOSS / ReED 7";
  {
    std::set<widget::Widget*> before(APP->scene->children.begin(),APP->scene->children.end());
    widget::EventContext context;widget::Widget::ButtonEvent e;
    e.context=&context;e.action=GLFW_PRESS;e.button=GLFW_MOUSE_BUTTON_RIGHT;
    e.pos=owner->headerArtworkArea.getCenter();owner->onButton(e);
    require(!owner->nameEditor,"Title right-click must not open a rename editor");
    ui::MenuOverlay* overlay=nullptr;ui::Menu* menu=nullptr;
    for(auto* w:APP->scene->children)if(!before.count(w))if(auto* found=dynamic_cast<ui::MenuOverlay*>(w))overlay=found;
    if(overlay)for(auto* w:overlay->children)if(auto* found=dynamic_cast<ui::Menu*>(w))menu=found;
    require(menu,"Title right-click opens the standard module menu");menu->step();
    overlay->removeChild(menu);auto pos=menu->box.pos;menu->box.pos=Vec();
    capture(menu,output+"/title-context-menu.png",scale,false);menu->box.pos=pos;overlay->addChild(menu);
    APP->scene->removeChild(overlay);delete overlay;
  }
  auto open=[&] {
    owner->editName();
    auto* editor=dynamic_cast<beekapoo::BeekNameEditor*>(owner->nameEditor.get());
    require(editor,"Explicit Rename action opens the name field");
    require(APP->event->getSelectedWidget()==editor->field,"Name editor must focus immediately");
    require(editor->field->getSelectedText()==m->displayPatchName(owner->appearance.modules),"Existing displayed title must be selected for immediate replacement");
    return editor;
  };
  auto key=[&](beekapoo::BeekNameEditor* editor,int value) {
    widget::EventContext context;widget::Widget::SelectKeyEvent e;
    e.context=&context;e.action=GLFW_PRESS;e.key=value;editor->field->onSelectKey(e);
    require(e.isConsumed()&&editor->closed,"Enter/Escape must close the editor and consume the shortcut");
    auto* parent=editor->parent;parent->removeChild(editor);delete editor;
  };
  auto* editor=open();editor->field->insertText("do not save");key(editor,GLFW_KEY_ESCAPE);
  require(m->getPatchName()==generatedName&&!m->hasCustomPatchName(),"Escape must leave both title and generated/custom state alone");
  editor=open();editor->field->insertText(exact);
  {
    struct Stage:widget::Widget {
      void draw(const DrawArgs& args) override {
        nvgBeginPath(args.vg);nvgRect(args.vg,0,0,box.size.x,box.size.y);
        nvgFillColor(args.vg,nvgRGB(44,44,44));nvgFill(args.vg);widget::Widget::draw(args);
      }
    } stage;
    stage.box.size=Vec(780,430);
    auto* parent=owner->parent;auto pos=owner->box.pos;
    parent->removeChild(owner);owner->box.pos=Vec(24,24);stage.addChild(owner);
    auto* panel=editor->panel;auto popupPos=panel->box.pos;
    editor->removeChild(panel);panel->box.pos=Vec(190,50);stage.addChild(panel);
    capture(&stage,output+"/rename-editor.png",scale,false);
    stage.removeChild(panel);panel->box.pos=popupPos;editor->addChild(panel);
    stage.removeChild(owner);owner->box.pos=pos;parent->addChild(owner);
  }
  key(editor,GLFW_KEY_ENTER);
  require(m->getPatchName()==exact&&m->hasCustomPatchName(),"Enter must save a custom name");
  for(uint64_t hash:{0ull,1ull,432ull,9981ull})require(m->displayPatchName(hash)==exact,"Hash changes must never recase custom titles");
  APP->history->undo();require(m->getPatchName()==generatedName&&!m->hasCustomPatchName(),"Rename undo must restore generated-name state");
  APP->history->redo();require(m->getPatchName()==exact&&m->hasCustomPatchName(),"Rename redo must restore custom-name state");
  for(size_t i=0;i<members.size();++i)require(owner->members[i].id==members[i].id&&owner->find(members[i].id)==members[i].widget.get(),"Rename undo/redo must not recreate child modules");
  for(int i=0;i<6;++i){owner->parameterAction(i,false);require(m->getPatchName()==exact,"Wiggle, randomize and initialize must preserve custom names");}
  require(owner->regeneratePatch(),"Regeneration fixture needs Fundamental");
  require(m->getPatchName()==exact&&m->hasCustomPatchName(),"Whole-patch regeneration must preserve custom names");
  owner->growPatch();owner->step();require(m->getPatchName()==exact,"Grow must preserve custom names");
  {
    auto* data=m->dataToJson();beekapoo::Module restored;restored.dataFromJson(data);
    require(restored.hasCustomPatchName()&&restored.displayPatchName(5)==exact,"Patch/preset reload must preserve the exact custom title");
    restored.setPatchName("random replacement");require(restored.getPatchName()==exact,"Automatic names must never replace custom titles");
    json_object_del(data,"customName");restored.dataFromJson(data);
    require(restored.hasCustomPatchName(),"Legacy names must be preserved conservatively");json_decref(data);
  }
  owner->saveBeek(output+"/runtime/name-preservation.vcv");
  owner->rename("keep THIS name");
  require(owner->loadBeek(output+"/runtime/name-preservation.vcv"),"Saved Beek must reload");
  require(m->getPatchName()=="keep THIS name"&&m->hasCustomPatchName(),"Loading contents must preserve the destination's custom title");
  owner->rename(exact);owner->step();
  auto* parent=owner->parent;auto pos=owner->box.pos;parent->removeChild(owner);owner->box.pos=Vec();
  capture(owner,output+"/renamed-panel.png",scale,false);owner->box.pos=pos;parent->addChild(owner);
  rack->clear();APP->history->clear();
  std::puts("Beekapoo rename: ordinary title context menu, explicit rename, native field focus, save/cancel, exact casing, undo, randomization and import persistence passed");
}
