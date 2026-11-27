#pragma once
template<class Add, class Require>
void testBeekModuleSets(const std::string& output, float scale, Add add, Require require) {
  using namespace beekapoo;
  auto* rack=APP->scene->rack;
  rack->clear();APP->history->clear();
  auto path=output+"/runtime/module-set-store-test.json";
  system::remove(path);
  ModuleSets saved(path);
  require(saved.find("fundamental")&&saved.find("fundamental")->modules.size()==8,"Fresh sets must contain the eight established Fundamental defaults");
  require(saved.find("default")&&saved.find("default")->name=="Default"&&saved.find("default")->modules==saved.find("fundamental")->modules&&saved.defaultId=="default","Read-only Default must contain the original eight modules and be the fresh installation default");
  require(!saved.remove("default"),"Built-in Default cannot be deleted");
  auto copy=saved.add(true,"fundamental");
  saved.find(copy)->name="A — two modules";
  saved.find(copy)->modules={{"Fundamental","VCO"},{"Fundamental","VCF"},{"NotInstalled","MissingModel"}};
  saved.find(copy)->positions[ {"Fundamental","VCO"} ]={120,380};
  saved.defaultId=copy;require(saved.save(),"Set preferences must save atomically");
  ModuleSets reopened(path);
  require(reopened.defaultId==copy&&reopened.find(copy)->name=="A — two modules"&&reopened.find(copy)->modules.size()==3,"Names, IDs, default and unavailable models must survive reload");
  require(reopened.find(copy)->positions.at({"Fundamental","VCO"})==std::make_pair(120.f,380.f),"Module positions must survive reload");
  auto positionedCopy=reopened.add(true,copy);
  require(reopened.find(positionedCopy)->positions==reopened.find(copy)->positions,"Duplicating a set must retain arrangement");
  require(reopened.find(copy)->modules.count({"NotInstalled","MissingModel"}),"Missing models must not be discarded from saved sets");
  reopened.find("fundamental")->modules.clear();reopened.save();
  ModuleSets edited(path);require(edited.find("fundamental")->modules.empty(),"An intentionally emptied Fundamental set must remain empty");
  require(!edited.remove("fundamental")&&edited.remove(copy)&&edited.defaultId=="default","Deleting the default must retain a usable default set ID");
  auto& sets=moduleSets();
  struct Restore { ModuleSets& target; ModuleSets previous; ~Restore(){target=previous;} } restore{sets,sets};
  system::remove(path+".session");
  sets=ModuleSets(path+".session");
  auto subset=sets.add(true,"fundamental");
  sets.find(subset)->name="A · VCO + VCF";
  sets.find(subset)->modules={{"Fundamental","VCO"},{"Fundamental","VCF"}};
  auto other=sets.add(true,subset);sets.find(other)->name="B · wavetable";
  sets.find(other)->modules={{"Fundamental","VCO2"},{"Fundamental","VCF"}};
  auto empty=sets.add(false);sets.find(empty)->name="Empty";
  sets.defaultId=subset;
  beekapoo::Module first,second;
  require(first.getGenerationSet()==subset&&second.getGenerationSet()==subset,"New modules use the global default");
  first.setGenerationSet(other);
  require(second.getGenerationSet()==subset,"Selecting a set is per module");
  auto* data=first.dataToJson();second.dataFromJson(data);json_decref(data);
  require(second.getGenerationSet()==other,"Selected set must survive patch/preset serialization");
  data=json_object();second.dataFromJson(data);json_decref(data);
  require(second.getGenerationSet()=="fundamental","Old patches retain the Fundamental selection, independent of the current default");
  for(const auto& id:{subset,other})for(int i=0;i<12;++i) {
    std::string error;
    auto* plan=generated::plan(5,nullptr,id,&error);
    require(plan,error.c_str());
    auto* modules=json_object_get(json_object_get(plan,"contents"),"modules");
    for(size_t n=0;n<json_array_size(modules);++n) {
      auto* node=json_array_get(modules,n);
      ModuleKey key{json_string_value(json_object_get(node,"plugin")),json_string_value(json_object_get(node,"model"))};
      require(sets.find(id)->modules.count(key),"Every generated model must be in the selected pool, including best-effort models");
    }
    json_decref(plan);
  }
  require(!generated::plan(4,nullptr,empty)&&!generated::plan(4,nullptr,"missing-id"),"Empty or deleted sets must fail without falling back to unrelated models");
  auto* owner=static_cast<Beekapoo*>(add(modelBeekapooPreview,Vec()));
  auto* m=owner->getModule<beekapoo::Module>();
  require(owner->generateDefaultPatch(4),"Selected default must generate on a new module");
  require(m->getGenerationSet()==subset,"Generating content must preserve selected set");
  m->setGenerationSet(empty);
  auto ids=APP->engine->getModuleIds();auto cables=APP->engine->getCableIds();
  require(!owner->regeneratePatch()&&ids==APP->engine->getModuleIds()&&cables==APP->engine->getCableIds(),"Failed generation must leave the running patch and cable identities intact");
  auto growth=sets.add(false);sets.find(growth)->name="Growth filter";sets.find(growth)->modules={{"Fundamental","Delay"}};
  m->setGenerationSet(growth);
  auto before=owner->members.size();
  std::set<int64_t> originals;for(const auto& member:owner->members)originals.insert(member.id);
  for(int i=0;i<24 && owner->members.size()==before;++i)owner->growPatch();
  require(owner->members.size()>before,"Growth must be able to insert a selected non-VCA processor");
  for(const auto& member:owner->members)if(!originals.count(member.id))
    require(owner->find(member.id)->model->slug=="Delay","Growth must never insert a module outside the selected set");
  APP->history->undo();require(owner->members.size()==before,"Growth undo must remove the inserted module");
  APP->history->redo();require(owner->members.size()==before+1,"Growth redo must restore membership");
  m->setGenerationSet(subset);
  owner->editSubpatch();
  static_cast<beekapoo::RackEditSession*>(owner->editSession.get())->setsButton->action();
  auto* editor=dynamic_cast<ModuleSetEditor*>(owner->moduleSetEditor.get());
  require(editor&&editor->brand=="Fundamental","Module sets must open from Edit with Fundamental selected");
  auto click=[&](ModuleSetEditor::Row* row) {
    widget::EventContext context;widget::Widget::ButtonEvent e;
    e.context=&context;e.action=GLFW_PRESS;e.button=GLFW_MOUSE_BUTTON_LEFT;row->onButton(e);
    require(e.isConsumed(),"Set row click must not reach Rack underneath");
  };
  auto engineIds=APP->engine->getModuleIds(), cableIds=APP->engine->getCableIds();
  std::string historyName=APP->history->getUndoName();
  std::map<int64_t,std::vector<float>> liveValues;
  for(auto id:engineIds) {auto* module=APP->engine->getModule(id);for(auto& p:module->params)liveValues[id].push_back(p.getValue());}
  auto isolation=[&] {
    require(APP->engine->getModuleIds()==engineIds&&APP->engine->getCableIds()==cableIds,"Editing a set must never add/remove live modules or cables");
    for(auto id:engineIds) {auto* module=APP->engine->getModule(id);for(size_t i=0;i<module->params.size();++i)require(module->params[i].getValue()==liveValues[id][i],"Preview interaction must not affect live parameters");}
  };
  ModuleKey delay{"Fundamental","Delay"};
  auto pickerFor=[&](const ModuleKey& key) -> ModuleSetEditor::Card* {
    for(auto* w:editor->models->container->children)if(auto* c=dynamic_cast<ModuleSetEditor::Card*>(w))if(c->key==key)return c;
    return nullptr;
  };
  auto clickCard=[&](ModuleSetEditor::Card* card) {
    require(card,"Module card must exist");
    widget::EventContext context;widget::Widget::ButtonEvent e;e.context=&context;e.button=GLFW_MOUSE_BUTTON_LEFT;e.action=GLFW_PRESS;e.mods=0;
    card->onButton(e);require(e.isConsumed(),"Card click must not reach the live patch");editor->step();
  };
  auto checkFit=[&] {
    for(auto* c:editor->cards) {
      require(c->box.pos.x>=0&&c->box.pos.y>=0&&c->box.getRight()*editor->rackScale<=editor->rackScroll->box.size.x+.1f&&c->box.getBottom()*editor->rackScale<=editor->rackScroll->box.size.y+.1f,"All chosen previews must fit on the right");
      for(auto* other:editor->cards)if(other!=c)require(!c->box.intersects(other->box),"Automatically placed previews must not overlap");
    }
  };
  clickCard(pickerFor(delay));require(editor->findCard(delay),"Click on the left must add a module immediately");
  auto* card=editor->findCard(delay);require(card->preview&&!card->preview->module,"Previews must remain null-module widgets");
  for(auto* port:card->preview->getPorts())require(port->visible,"Full browser previews must retain their ports");
  auto position=card->box.pos;
  widget::EventContext dragContext;widget::Widget::DragMoveEvent drag;drag.context=&dragContext;drag.mouseDelta=Vec(200,380);card->onDragMove(drag);
  require(card->box.pos==position,"Cards must not support manual reorganization");
  checkFit();
  clickCard(pickerFor(delay));require(!editor->findCard(delay),"Second left click must remove the module");
  editor->undo();editor->step();require(editor->findCard(delay),"Undo restores removed membership");
  editor->redo();editor->step();require(!editor->findCard(delay),"Redo repeats removal");
  clickCard(pickerFor(delay));clickCard(editor->findCard(delay));
  require(!editor->findCard(delay),"Click on the right must remove the module");
  widget::EventContext keyContext;widget::Widget::HoverKeyEvent undoKey;undoKey.context=&keyContext;undoKey.action=GLFW_PRESS;undoKey.key=GLFW_KEY_Z;undoKey.mods=RACK_MOD_CTRL;
  editor->onHoverKey(undoKey);editor->step();require(editor->findCard(delay),"Mod+Z restores right-side removal");
  widget::EventContext redoContext;undoKey.context=&redoContext;undoKey.mods=RACK_MOD_CTRL|GLFW_MOD_SHIFT;
  editor->onHoverKey(undoKey);editor->step();require(!editor->findCard(delay),"Mod+Shift+Z repeats removal");
  require(historyName==APP->history->getUndoName(),"Set edits must not enter patch history");
  editor->models->step();
  widget::EventContext wheelContext;widget::Widget::HoverScrollEvent wheel;wheel.context=&wheelContext;wheel.pos=Vec(50,50);wheel.scrollDelta=Vec(0,-100);
  editor->models->onHoverScroll(wheel);require(editor->models->offset.y>0,"Wheel over previews must scroll the picker");auto scrollBefore=editor->models->offset;
  clickCard(pickerFor(delay));require(editor->models->offset==scrollBefore,"Toggling membership must retain picker scroll");
  clickCard(pickerFor(delay));editor->models->offset=Vec();
  isolation();
  editor->choose(other);editor->step();
  require(m->getGenerationSet()==subset,"Browsing a different set must not change the Beek selection");
  editor->useSelected();require(m->getGenerationSet()==other,"Use this set must change this Beek");
  APP->history->undo();require(m->getGenerationSet()==subset,"Set selection supports undo without changing subpatch");
  APP->history->redo();require(m->getGenerationSet()==other,"Set selection supports redo");
  click(editor->defaultRow);require(sets.defaultId==other,"Default checkbox must select the default for future Beeks");
  beekapoo::Module next;require(next.getGenerationSet()==other,"Changed default must apply to the next created module");
  editor->name->setText("B · wavetable + filter");
  require(sets.find(other)->name==editor->name->text,"Rename must preserve stable set identity");
  editor->undo();editor->step();require(editor->name->text=="B · wavetable","Undo must refresh the displayed name");
  editor->redo();editor->step();require(editor->name->text=="B · wavetable + filter","Redo must restore the displayed name");
  editor->change([&]{editor->current()->modules.insert({"NotInstalled","MissingModel"});});editor->step();
  auto* missing=editor->findCard({"NotInstalled","MissingModel"});require(missing&&!missing->preview,"Unavailable models remain editable placeholders");
  editor->undo();editor->step();isolation();
  auto sceneBox=APP->scene->box;
  APP->scene->box.size=Vec(480,360);editor->step();
  require(editor->zoom->box.pos.x>=0 && editor->zoom->box.pos.y>=0 && editor->zoom->box.getRight()<=480 && editor->zoom->box.getBottom()<=360,"Set editor must fit a small Rack window");
  APP->scene->box=sceneBox;
  editor->choose("default");editor->search->setText("");editor->step();
  auto standard=editor->current()->modules;auto undoCount=editor->editCursor;
  clickCard(pickerFor({"Fundamental","Delay"}));
  clickCard(editor->findCard({"Fundamental","VCO"}));
  editor->addModel({"Fundamental","VCO2"});editor->deleteSet();editor->name->setText("Changed");
  require(editor->current()->modules==standard&&editor->current()->name=="Default"&&editor->editCursor==undoCount,"Default rejects adding, removing, renaming and deleting without pushing undo entries");
  require(!editor->name->visible&&!editor->deleteSetButton->visible,"Read-only set must not present rename/delete controls");
  editor->chooseSet(true);editor->step();require(!editor->readOnly()&&editor->current()->modules==standard,"Duplicate Default makes an editable copy");
  clickCard(editor->findCard({"Fundamental","VCO"}));require(editor->current()->modules.size()==standard.size()-1,"Duplicate membership is editable");
  editor->undo();editor->step();require(editor->current()->modules==standard,"Undo restores editable duplicate membership");
  require(sets.find("default")->modules==standard,"Editing a copy must leave Default intact");
  ModuleSets disk(sets.path);require(disk.find("default")->modules==standard,"Read-only Default survives saving and reloading other sets");
  editor->choose("default");editor->step();
  require(editor->hint.find("No available")==std::string::npos,"Switching sets must clear stale generation errors");
  editor->fitRack();
  checkFit();
  require(editor->panel->box.size.y>=900,"Editor must provide a taller collection area");
  require(editor->buttons.front()->text=="Done"&&editor->buttons.front()->box.pos.x==16,"Done belongs at the top left");
  struct DrawProbe : widget::Widget {
    int base=0,light=0;
    void draw(const DrawArgs&) override {++base;}
    void drawLayer(const DrawArgs&,int layer) override {if(layer==1)++light;}
  };
  auto* drawProbe=new DrawProbe;drawProbe->box.size=Vec(10,10);editor->cards.front()->preview->addChild(drawProbe);
  // A fresh capture of the complete browser previews, including display layers.
  editor->step();
  auto pos=editor->panel->box.pos;auto* panelParent=editor->panel->parent;panelParent->removeChild(editor->panel);editor->panel->box.pos=Vec();
  capture(editor->panel,output+"/module-sets.png",scale,false);editor->panel->box.pos=pos;panelParent->addChild(editor->panel);
  require(drawProbe->base>0&&drawProbe->light==drawProbe->base,"Browser display layer must render exactly once, not double-draw text or lights");
  // A larger collection must also fit, then render another set without changing the patch.
  editor->chooseSet(false);
  editor->name->setText("Oscillators + modulation");
  for(const auto& slug:{"VCO","VCO2","LFO","Noise","ADSR","Delay","VCF","VCA","8vert","Mixer","SequentialSwitch1","SequentialSwitch2"}) {
    auto* model=plugin::getModel("Fundamental",slug);
    if(model&&generated::unavailableReason(model).empty())editor->addModel({"Fundamental",slug});
  }
  bool previousDark=settings::preferDarkPanels;settings::preferDarkPanels=true;
  editor->rackDirty=editor->dirty=true;editor->step();checkFit();isolation();
  pos=editor->panel->box.pos;panelParent->removeChild(editor->panel);editor->panel->box.pos=Vec();
  capture(editor->panel,output+"/module-sets-expanded.png",scale,false);editor->panel->box.pos=pos;panelParent->addChild(editor->panel);
  settings::preferDarkPanels=previousDark;
  editor->search->setText("VC");editor->step();require(!editor->models->container->children.empty(),"Search should retain matching module names");
  widget::EventContext context;widget::Widget::HoverKeyEvent key;key.context=&context;key.action=GLFW_PRESS;key.key=GLFW_KEY_ESCAPE;key.mods=0;key.pos=Vec();
  editor->onHoverKey(key);
  require(editor->closed&&owner->editSession,"Escape must close sets without exiting subpatch editing");
  editor->parent->removeChild(editor);delete editor;
  static_cast<beekapoo::RackEditSession*>(owner->editSession.get())->finish(true);
  rack->clear();APP->history->clear();
  auto envelope=sets.add(false);sets.find(envelope)->modules={{"Fundamental","ADSR"}};
  auto* lfo=add(plugin::getModel("Fundamental","LFO"),Vec());
  auto* envelopeOwner=static_cast<Beekapoo*>(add(modelBeekapooPreview,Vec(300,0)));
  rack->select(lfo,true);envelopeOwner->collapse();
  envelopeOwner->getModule<beekapoo::Module>()->setGenerationSet(envelope);
  require(envelopeOwner->growPatch()&&envelopeOwner->members.size()==2,"Grow must connect required inputs when adding a module without an existing insertion cable");
  for(const auto& member:envelopeOwner->members)if(member.id!=lfo->module->id) {
    auto* w=envelopeOwner->find(member.id);
    require(w->model->slug=="ADSR","Envelope-only pool must add an envelope");
    auto wires=rack->getCompleteCablesOnPort(w->getInput(4));
    require(wires.size()==1&&wires.front()->cable->outputModule==lfo->module,"Added envelope must receive an existing output at its required input, without signal typing");
  }
  rack->clear();APP->history->clear();
  std::puts("Module sets: disk persistence, defaults, per-module selection, generation, inference, growth, undo and editor interactions passed");
}
