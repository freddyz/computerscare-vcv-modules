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
  auto copy=saved.add(true,"fundamental");
  saved.find(copy)->name="A — two modules";
  saved.find(copy)->modules={{"Fundamental","VCO"},{"Fundamental","VCF"},{"NotInstalled","MissingModel"}};
  saved.defaultId=copy;require(saved.save(),"Set preferences must save atomically");
  ModuleSets reopened(path);
  require(reopened.defaultId==copy&&reopened.find(copy)->name=="A — two modules"&&reopened.find(copy)->modules.size()==3,"Names, IDs, default and unavailable models must survive reload");
  require(reopened.find(copy)->modules.count({"NotInstalled","MissingModel"}),"Missing models must not be discarded from saved sets");
  reopened.find("fundamental")->modules.clear();reopened.save();
  ModuleSets edited(path);require(edited.find("fundamental")->modules.empty(),"An intentionally emptied Fundamental set must remain empty");
  require(!edited.remove("fundamental")&&edited.remove(copy)&&edited.defaultId=="fundamental","Deleting the default must retain a usable default set ID");
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
  bool toggled=false;
  for(auto* w:editor->models->container->children)if(auto* row=dynamic_cast<ModuleSetEditor::Row*>(w))
    if(row->label=="Delay") {click(row);require(sets.find(subset)->modules.count({"Fundamental","Delay"}),"Click must include module");click(row);require(!sets.find(subset)->modules.count({"Fundamental","Delay"}),"Second click must exclude module");toggled=true;break;}
  require(toggled,"Model list must include all visible Fundamental models, not only defaults");
  editor->choose(other);editor->step();
  require(m->getGenerationSet()==other,"Choosing a set must change this Beek");
  APP->history->undo();require(m->getGenerationSet()==subset,"Set selection supports undo without changing subpatch");
  APP->history->redo();require(m->getGenerationSet()==other,"Set selection supports redo");
  click(editor->defaultRow);require(sets.defaultId==other,"Default checkbox must select the default for future Beeks");
  beekapoo::Module next;require(next.getGenerationSet()==other,"Changed default must apply to the next created module");
  editor->name->setText("B · wavetable + filter");
  require(sets.find(other)->name==editor->name->text,"Rename must preserve stable set identity");
  auto sceneBox=APP->scene->box;
  APP->scene->box.size=Vec(480,360);editor->step();
  require(editor->zoom->box.pos.x>=0 && editor->zoom->box.pos.y>=0 && editor->zoom->box.getRight()<=480 && editor->zoom->box.getBottom()<=360,"Set editor must fit a small Rack window");
  APP->scene->box=sceneBox;
  editor->choose("fundamental");editor->search->setText("");editor->step();
  require(editor->hint.find("No available")==std::string::npos,"Switching sets must clear stale generation errors");
  // A fresh capture of the actual editor, with the native scroll lists.
  editor->step();
  auto pos=editor->panel->box.pos;auto* panelParent=editor->panel->parent;panelParent->removeChild(editor->panel);editor->panel->box.pos=Vec();
  capture(editor->panel,output+"/module-sets.png",scale,false);editor->panel->box.pos=pos;panelParent->addChild(editor->panel);
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
  require(envelopeOwner->growPatch()&&envelopeOwner->members.size()==2,"Grow must support selected envelopes when a gate source is available");
  for(const auto& member:envelopeOwner->members)if(member.id!=lfo->module->id) {
    auto* w=envelopeOwner->find(member.id);
    require(w->model->slug=="ADSR","Envelope-only pool must add an envelope");
    auto wires=rack->getCompleteCablesOnPort(w->getInput(4));
    require(wires.size()==1&&wires.front()->cable->outputModule==lfo->module&&wires.front()->cable->outputId==3,"Added envelope must receive LFO square at its gate");
  }
  rack->clear();APP->history->clear();
  std::puts("Module sets: disk persistence, defaults, per-module selection, generation, inference, growth, undo and editor interactions passed");
}
