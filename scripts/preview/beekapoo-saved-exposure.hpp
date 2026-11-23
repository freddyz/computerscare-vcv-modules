#pragma once
// Optional local reproduction. The caller supplies an extracted patch and
// uniquely named copies of its installed plugin libraries. Never loads Audio.
template<class Add,class Require>
void testBeekSavedExposure(const std::string& output,float scale,Add add,Require require) {
  const char* filename=std::getenv("BEEKAPOO_EXPOSURE_PATCH");
  const char* directory=std::getenv("BEEKAPOO_AUDIT_PLUGIN_DIR");
  require(filename&&directory,"Saved exposure audit needs a patch and plugin directory");
  auto* rack=APP->scene->rack;rack->clear();APP->history->clear();
  struct Loaded {plugin::Plugin* plugin;void* library;};std::vector<Loaded> loaded;
  auto* patch=json_load_file(filename,0,nullptr);require(patch,"Saved exposure patch must be readable");
  DEFER({rack->clear();APP->history->clear();json_decref(patch);for(auto& l:loaded){plugin::plugins.erase(std::find(plugin::plugins.begin(),plugin::plugins.end(),l.plugin));delete l.plugin;dlclose(l.library);}});
  if(!plugin::getModel("computerscare","computerscare-horse-a-doodle-doo"))pluginInstance->addModel(modelComputerscareHorseADoodleDoo);
  auto* nodes=json_object_get(patch,"modules");
  std::vector<app::ModuleWidget*> children;std::map<int64_t,app::ModuleWidget*> byId;
  for(size_t i=0;i<json_array_size(nodes);++i) {
    auto* node=json_array_get(nodes,i);std::string slug=json_string_value(json_object_get(node,"plugin"));
    std::string model=json_string_value(json_object_get(node,"model"));
    if(slug=="Core"||model=="computerscare-beekapoo")continue;
    auto* pm=plugin::getModel(slug,model);
    if(!pm) {
      auto file=asset::plugin(pluginInstance,"build/previews/"+slug+"-audit.dylib");
      void* lib=dlopen(file.c_str(),RTLD_NOW|RTLD_LOCAL|RTLD_FIRST);
      if(!lib)throw std::runtime_error(dlerror());
      auto* p=new plugin::Plugin;p->path=std::string(directory)+"/"+slug;
      auto* manifest=json_load_file((p->path+"/plugin.json").c_str(),0,nullptr);require(manifest,"Audit plugin manifest must exist");
      auto init=reinterpret_cast<void(*)(plugin::Plugin*)>(dlsym(lib,"init"));require(init,"Audit plugin init must exist");
      p->fromJson(manifest);init(p);p->modulesFromJson(json_object_get(manifest,"modules"));json_decref(manifest);
      plugin::plugins.push_back(p);loaded.push_back({p,lib});pm=plugin::getModel(slug,model);
    }
    require(pm,"Saved module must be installed");auto* w=add(pm,Vec(children.size()*150,0));
    auto* state=json_deep_copy(node);json_object_set_new(state,"id",json_integer(w->module->id));
    w->module->fromJson(state);json_decref(state);
    for(int j=0;j<4;++j)w->step();
    byId[json_integer_value(json_object_get(node,"id"))]=w;children.push_back(w);
  }
  auto* owner=static_cast<beekapoo::Beekapoo*>(add(modelBeekapooPreview,Vec(1800,0)));
  std::map<int64_t,json_t*> saved;
  using Flags=std::tuple<bool,bool,bool,bool>;
  std::map<std::pair<int64_t,int>,Flags> flags;
  for(auto* w:children){saved[w->module->id]=w->module->toJson();for(auto* q:w->module->paramQuantities)if(q)flags[{w->module->id,q->paramId}]=Flags(q->snapEnabled,q->smoothEnabled,q->resetEnabled,q->randomizeEnabled);}
  DEFER({for(auto& p:saved)json_decref(p.second);});
  std::string phase="create";
  auto verify=[&] {
    owner->step();
    for(auto* w:children) {
      auto* state=w->module->toJson();
      // Adjacency is Rack bookkeeping, not module parameter/custom state.
      for(auto* key:{"params","data","bypassed"})if((json_object_get(state,key)||json_object_get(saved[w->module->id],key))&&!json_equal(json_object_get(state,key),json_object_get(saved[w->module->id],key))) {
        std::fprintf(stderr,"Saved audit changed %s %s during %s\n",w->model->slug.c_str(),key,phase.c_str());
        json_dump_file(saved[w->module->id],(output+"/audit-before.json").c_str(),JSON_INDENT(2));json_dump_file(state,(output+"/audit-after.json").c_str(),JSON_INDENT(2));
        require(false,"Saved patch module values and private state must survive exposure");
      }
      json_decref(state);
      for(auto* q:w->module->paramQuantities)if(q)require(flags[{w->module->id,q->paramId}]==Flags(q->snapEnabled,q->smoothEnabled,q->resetEnabled,q->randomizeEnabled),"Saved patch quantity policies must survive exposure");
    }
  };
  rack->deselectAll();for(auto* w:children)rack->select(w,true);owner->groupAction(true,true);verify();
  for(auto* w:children)for(auto* param:w->getParams()) {
    phase=w->model->slug+" "+param->getParamQuantity()->getLabel();
    owner->toggleParameter(param);verify();owner->toggleParameter(param);verify();
  }
  for(int n=0;n<8;++n)for(int kind=0;kind<3;++kind) {
    phase="section + "+std::to_string(kind);owner->addExposedControl(kind);verify();
    phase="section - "+std::to_string(kind);owner->removeExposedControl(kind);verify();
    phase="section reroll "+std::to_string(kind);owner->rerollExposedControls(kind);verify();
  }
  owner->requestedWidth=210;owner->layoutControls();owner->step();auto* parent=owner->parent;auto pos=owner->box.pos;parent->removeChild(owner);owner->box.pos=Vec();capture(owner,output+"/saved-exposure.png",scale,false);owner->box.pos=pos;parent->addChild(owner);
  std::printf("Saved exposure audit passed: %s (%zu source modules)\n",filename,children.size());
}
