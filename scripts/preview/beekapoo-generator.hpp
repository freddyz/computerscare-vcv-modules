#pragma once
#include <dlfcn.h>
// Compact comparison of the real child widgets/cables and their front panel.
// Borrow engine cables only for drawing; no routing or live Rack is changed.
inline void renderGeneratedBeekRow(beekapoo::Beekapoo* owner,const std::string& path,float scale) {
  owner->step();
  // The collapsed panel relocates its native endpoints. Draw the originals
  // at their real panel positions, and restore those endpoints afterwards.
  std::vector<std::pair<app::PortWidget*,Vec>> portPositions;
  for(const auto& p:owner->ports)if(auto* source=owner->originalPort(p)) {
    portPositions.push_back({source,source->box.pos});source->box.pos=p.originalPosition;
  }
  struct Row : widget::Widget {
    float divider=0;
    void draw(const DrawArgs& args) override {
      nvgBeginPath(args.vg);nvgRect(args.vg,0,0,box.size.x,box.size.y);nvgFillColor(args.vg,nvgRGB(29,34,31));nvgFill(args.vg);
      nvgBeginPath(args.vg);nvgMoveTo(args.vg,divider,8);nvgLineTo(args.vg,divider,box.size.y-8);nvgStrokeColor(args.vg,nvgRGB(120,133,125));nvgStrokeWidth(args.vg,1);nvgStroke(args.vg);
      widget::Widget::draw(args);
    }
  } row;
  struct Saved {widget::Widget* widget;widget::Widget* parent;Rect box;bool visible;};
  std::vector<Saved> saved;
  std::vector<GroupCableWidget*> cables;
  // Collect native endpoints before temporarily lifting the original widgets.
  for(auto* source:owner->homeRack->getCompleteCables())
    if(owner->contains(source->cable->inputModule)&&owner->contains(source->cable->outputModule)) {
      auto* wire=new GroupCableWidget;
      wire->cable=source->cable;
      wire->inputPort=owner->find(source->cable->inputModule->id)->getInput(source->cable->inputId);
      wire->outputPort=owner->find(source->cable->outputModule->id)->getOutput(source->cable->outputId);
      wire->color=source->color;cables.push_back(wire);
    }
  auto move=[&](widget::Widget* w,Vec position) {
    saved.push_back({w,w->parent,w->box,w->visible});w->parent->removeChild(w);w->box.pos=position;w->visible=true;row.addChild(w);
  };
  move(owner,Vec(8,8));
  row.divider=owner->box.size.x+22;
  float x=owner->box.size.x+36;
  for(const auto& member:owner->members) {
    auto* w=owner->find(member.id);move(w,Vec(x,8));w->step();x+=w->box.size.x;
  }
  row.box.size=Vec(x+8,396);
  auto tension=settings::cableTension;settings::cableTension=.7f;
  for(auto* wire:cables) {
    auto a=wire->getInputPos(),b=wire->getOutputPos();
    row.box.size.y=std::max(row.box.size.y,std::max(a.y,b.y)+.5f*(1-settings::cableTension)*(150+(a-b).norm())+24);
    wire->box.size=row.box.size;row.addChild(wire);row.addChild(wire->outputPlug);row.addChild(wire->inputPlug);wire->step();
  }
  auto opacity=settings::cableOpacity;settings::cableOpacity=.85f;
  capture(&row,path,scale,false);
  settings::cableOpacity=opacity;settings::cableTension=tension;
  for(auto* wire:cables) {
    row.removeChild(wire->inputPlug);row.removeChild(wire->outputPlug);row.removeChild(wire);
    wire->cable=nullptr;delete wire;
  }
  for(const auto& item:saved) {
    row.removeChild(item.widget);item.widget->box=item.box;item.widget->visible=item.visible;item.parent->addChild(item.widget);
  }
  for(const auto& p:portPositions)p.first->box.pos=p.second;
}

// Real Fundamental modules in this isolated renderer, never the live Rack.
template <class Add, class Require>
void testBeekGenerator(const std::string& output,float scale,Add add,Require require) {
  auto* rack=APP->scene->rack;
  rack->clear();APP->history->clear();
  require(!beekapoo::generated::plan(),"Missing Fundamental must not create a partial patch");
  auto path=asset::system("plugins/Fundamental");
  void* library=dlopen(asset::plugin(pluginInstance,"build/previews/fundamental-preview.dylib").c_str(),RTLD_NOW|RTLD_LOCAL|RTLD_FIRST);
  if(!library)throw std::runtime_error(dlerror());
  auto* fundamental=new plugin::Plugin;
  fundamental->path=path;
  auto init=reinterpret_cast<void(*)(plugin::Plugin*)>(dlsym(library,"init"));
  require(init,"Fundamental's init must be available");
  auto* manifest=json_load_file((path+"/plugin.json").c_str(),0,nullptr);
  fundamental->fromJson(manifest);init(fundamental);fundamental->modulesFromJson(json_object_get(manifest,"modules"));json_decref(manifest);
  plugin::plugins.push_back(fundamental);
  struct Cleanup {
    plugin::Plugin* plugin;void* library;
    ~Cleanup(){APP->scene->rack->clear();APP->history->clear();plugin::plugins.erase(std::find(plugin::plugins.begin(),plugin::plugins.end(),plugin));delete plugin;dlclose(library);}
  } cleanup{fundamental,library};
  if(std::getenv("BEEKAPOO_THREE_EXAMPLES")) {
    const int counts[]={3,5,7}, widths[]={120,150,240}, amounts[]={5,8,12};
    for(int example=0;example<3;++example) {
      rack->clear();APP->history->clear();
      auto* owner=static_cast<beekapoo::Beekapoo*>(add(modelBeekapooPreview,Vec()));
      require(owner->generateDefaultPatch(counts[example]),"Comparison must generate a real Fundamental patch");
      for(auto& p:owner->parameters){owner->controls->removeChild(p.knob);delete p.knob;}
      owner->parameters.clear();
      std::vector<std::vector<app::ParamWidget*>> candidates;
      for(const auto& member:owner->members) {
        candidates.emplace_back();
        for(auto* param:owner->find(member.id)->getParams()) {
          auto* q=param->getParamQuantity();auto* button=dynamic_cast<app::Switch*>(param);
          if(q&&q->isBounded()&&q->getRange()>0&&!(button&&button->momentary))candidates.back().push_back(param);
        }
      }
      for(size_t next=0;int(owner->parameters.size())<amounts[example];++next) {
        bool added=false;
        for(const auto& group:candidates)if(next<group.size()&&int(owner->parameters.size())<amounts[example]) {
          auto* p=group[next];owner->exposeParameter(p->module->id,p->paramId);added=true;
        }
        if(!added)break;
      }
      owner->requestedWidth=widths[example];owner->layoutControls();
      for(const auto& p:owner->ports) {
        if(example==2)require(!p.face->compact,"Roomy exposed ports must use full-size jacks");
        if(p.gain&&p.gain->visible)
          for(const auto& other:owner->ports)require(!p.gain->box.intersects(other.face->box),"Larger jacks must stay clear of input attenuverters");
        for(const auto& other:owner->ports)if(&p!=&other)require(!p.face->box.intersects(other.face->box),"Exposed jacks must not overlap");
      }
      renderGeneratedBeekRow(owner,output+string::f("/comparison-%d.png",example+1),scale);
      auto* parent=owner->parent;auto position=owner->box.pos;parent->removeChild(owner);owner->box.pos=Vec();
      capture(owner,output+string::f("/comparison-panel-%d.png",example+1),scale,false);
      owner->box.pos=position;parent->addChild(owner);
      int readable=0;for(const auto& label:owner->controlLabels)readable+=label.readable;
      require(std::any_of(owner->controlLabels.begin(),owner->controlLabels.end(),[](const beekapoo::Beekapoo::ControlLabel& label){return label.output&&label.readable;}),"A sparse output must retain its visible name");
      std::fprintf(stderr,"Comparison %d: %zu controls, %d/%zu readable captions, frame %.1f x %.1f\n",example+1,owner->parameters.size(),readable,owner->controlLabels.size(),owner->frame->box.size.x,owner->frame->box.size.y);
      for(const auto& label:owner->controlLabels)std::fprintf(stderr,"  %s: %.1f x %.1f @%.1f, readable %d\n",label.text.c_str(),label.width,label.maxHeight,label.fontSize,label.readable);
      for(const auto& p:owner->parameters)std::fprintf(stderr,"  knob: %s %.1f x %.1f\n",p.knob->getParamQuantity()->getLabel().c_str(),p.knob->box.size.x,p.knob->box.size.y);
    }
    return;
  }
  if(std::getenv("BEEKAPOO_GENERATED_SPECTRUM")) {
    auto* manifest=json_array();
    const int widths[]={4,5,6,7,8,9,10,12,14,16,20,6,7,8,9,10,6,5,8,12,6};
    const int amounts[]={0,1,2,3,4,5,6,8,10,12,4,2,3,5,6,8,1,4,7,3,5};
    for(int i=0;i<41;++i){
      rack->clear();APP->history->clear();
      auto* owner=static_cast<beekapoo::Beekapoo*>(add(modelBeekapooPreview,Vec()));
      require(owner->generateDefaultPatch(3+(i%9)),"Spectrum must generate real Fundamental modules");
      for(auto& p:owner->parameters){owner->controls->removeChild(p.knob);delete p.knob;}owner->parameters.clear();
      int wanted=amounts[i%21];
      for(const auto& member:owner->members){auto* w=owner->find(member.id);for(auto* q:w->module->paramQuantities)if(q&&q->getRange()>0&&w->getParam(q->paramId)&&int(owner->parameters.size())<wanted)owner->exposeParameter(member.id,q->paramId);}
      owner->requestedWidth=widths[i%21]*15;owner->layoutControls();
      for(const auto& p:owner->parameters)require(p.knob->visible,"Every spectrum parameter must remain visible");
      for(const auto& p:owner->ports)if(p.gain&&p.gain->visible) {
        for(const auto& other:owner->ports)require(!p.gain->box.intersects(other.face->box),"Input attenuverter must not cover a jack");
        for(int input:{0,1,3,4,beekapoo::Module::GENERATE_INPUT,beekapoo::Module::GROW_INPUT})require(!p.gain->box.intersects(owner->getInput(input)->box),"Action jacks must not overlap input attenuverters at narrow widths");
      }
      std::string modules;for(const auto& member:owner->members){if(!modules.empty())modules+=" / ";modules+=owner->find(member.id)->model->slug;}
      std::string image=string::f("spectrum-%02d.png",i+1);
      auto* parent=owner->parent;auto position=owner->box.pos;parent->removeChild(owner);owner->box.pos=Vec();capture(owner,output+"/"+image,scale,false);owner->box.pos=position;parent->addChild(owner);
      auto* entry=json_pack("{s:i,s:i,s:i,s:i,s:i,s:s,s:s}","index",i+1,"hp",int(owner->box.size.x/15),"parameters",int(owner->parameters.size()),"inputs",3,"outputs",1,"modules",modules.c_str(),"image",image.c_str());json_array_append_new(manifest,entry);
    }
    require(json_dump_file(manifest,(output+"/generated-spectrum-manifest.json").c_str(),JSON_INDENT(2))==0,"Spectrum manifest must save");json_decref(manifest);rack->clear();return;
  }
  {
    auto before=APP->engine->getModuleIds();
    auto* preview=modelBeekapooPreview->createModuleWidget(nullptr);preview->step();capture(preview,output+"/browser-generated.png",scale,false);
    auto* beek=static_cast<beekapoo::Beekapoo*>(preview);require(beek->browserMemberCount>=3&&beek->browserMemberCount<=11&&!beek->browserModules.empty(),"Browser preview must depict a real generated layout");delete preview;
    require(APP->engine->getModuleIds()==before,"Browser preview must not register any DSP modules");
  }
  {
    std::set<std::string> names;int numbered=0,plain=0,oneWord=0,threeWords=0,separated=0,prefixNumber=0;
    std::ofstream sample(output+"/generated-names.txt");
    for(unsigned seed=0;seed<256;++seed) {
      auto name=beekapoo::generated::name(seed);
      require(name==beekapoo::generated::name(seed),"Naming templates must be reproducible for a given entropy seed");
      require(!name.empty()&&name.size()<=96&&name.find('\n')==std::string::npos,"Generated names must remain usable as panel labels");
      names.insert(name);bool digits=std::any_of(name.begin(),name.end(),[](char c){return c>='0'&&c<='9';});
      if(digits)++numbered;else ++plain;
      if(name.find(' ')==std::string::npos)++oneWord;
      if(std::count(name.begin(),name.end(),' ')>=2)++threeWords;
      if(name.find_first_of("-/#.")!=std::string::npos)++separated;
      if(name.front()>='0'&&name.front()<='9')++prefixNumber;
      if(seed<32)sample<<name<<'\n';
    }
    require(names.size()>180&&numbered>30&&plain>70&&oneWord>20&&threeWords>5&&separated>20&&prefixNumber>3,"Naming recipes must vary structure, word counts, separators, number presence and number position");
  }
  std::set<int> counts;
  for(int trial=0;trial<40;++trial) {
    auto* plan=beekapoo::generated::plan();require(plan,"Generator must produce a plan");
    auto* contents=json_object_get(plan,"contents");auto* nodes=json_object_get(contents,"modules");auto* wires=json_object_get(contents,"cables");
    int count=int(json_array_size(nodes));counts.insert(count);require(count>=3&&count<=11,"Generated patch must contain 3-11 modules");
    std::set<std::pair<int,int>> occupied;std::set<int> involved;
    for(size_t i=0;i<json_array_size(nodes);++i) {
      auto* node=json_array_get(nodes,i);require(std::string(json_string_value(json_object_get(node,"plugin")))=="Fundamental","Only Fundamental modules may be generated");
      if(i){auto* members=json_object_get(plan,"members");auto* previous=json_array_get(nodes,i-1);auto* model=plugin::getModel("Fundamental",json_string_value(json_object_get(previous,"model")));double x=json_number_value(json_object_get(json_array_get(members,i),"x")),prev=json_number_value(json_object_get(json_array_get(members,i-1),"x"));require(std::abs(x-prev-beekapoo::generated::moduleWidth(model))<.01,"Generated modules must touch without gaps");}
    }
    for(size_t i=0;i<json_array_size(wires);++i) {
      auto* wire=json_array_get(wires,i);int from=json_integer_value(json_object_get(wire,"outputModuleId")),to=json_integer_value(json_object_get(wire,"inputModuleId")),in=json_integer_value(json_object_get(wire,"inputId"));
      require(from!=to&&occupied.insert({to,in}).second,"Generated cables must not self-patch or stack inputs");involved.insert(from);involved.insert(to);
    }
    require(int(involved.size())==count,"Every generated module must participate in patching");
    int inputs=0,outputs=0;auto* ports=json_object_get(plan,"ports");
    for(size_t i=0;i<json_array_size(ports);++i){auto* p=json_array_get(ports,i);bool input=json_is_true(json_object_get(p,"input"));if(input){++inputs;require(!occupied.count({int(json_integer_value(json_object_get(p,"module"))),int(json_integer_value(json_object_get(p,"port")))}),"Exposed input must be free of internal wiring");}else ++outputs;}
    require(inputs==3&&outputs==1,"Generator must expose three inputs and one output");
    require(json_array_size(json_object_get(plan,"parameters"))>=2,"Generator must expose some parameters");
    json_decref(plan);
  }
  {
    std::set<int> sizes;int threes=0,elevens=0;
    for(unsigned i=0;i<100;++i){int n=beekapoo::generated::moduleCount(i);sizes.insert(n);threes+=n==3;elevens+=n==11;}
    require(sizes.size()==9&&*sizes.begin()==3&&*sizes.rbegin()==11&&threes>elevens,"Weighted counts must cover 3 through 11 and favour small patches");
  }
  auto* owner=static_cast<beekapoo::Beekapoo*>(add(modelBeekapooPreview,Vec()));auto id=owner->module->id;
  auto* batch=new history::ComplexAction;batch->name="add module";
  auto* action=new history::ModuleAdd;action->setModule(owner);batch->push(action);APP->history->push(batch);
  owner->step();
  require(owner->members.size()>=3&&owner->members.size()<=11&&owner->box.size.x==90,"Browser add must generate at six HP");
  require(owner->ports.size()==4&&!owner->parameters.empty(),"Browser add must expose controls and ports");
  {
    auto* m=owner->getModule<beekapoo::Module>();
    int gains=0;
    for(const auto& p:owner->ports)if(p.input) {
      ++gains;require(p.route>=0&&p.gain&&p.gain->visible,"Every exposed input needs an adjacent attenuverter");
      int route=p.route,in=beekapoo::Module::inputBase+route,param=beekapoo::Module::INPUT_GAIN_BASE+route;
      engine::Module::ProcessArgs args;args.sampleRate=48000;args.sampleTime=1.f/48000;
      m->inputs[in].channels=2;m->inputs[in].setVoltage(4,0);m->inputs[in].setVoltage(-2,1);m->params[param].setValue(-.5f);m->outputs[route].channels=2;m->process(args);
      require(m->outputs[route].getChannels()==2&&std::abs(m->outputs[route].getVoltage(0)+2)<.001&&std::abs(m->outputs[route].getVoltage(1)-1)<.001,"Attenuverters must invert and preserve polyphonic signals");
      m->params[param].setValue(0);m->process(args);require(m->outputs[route].getVoltage(0)==0,"Zero gain must silence the input");
      m->params[param].setValue(1);m->inputs[in].channels=0;m->process(args);require(m->outputs[route].getChannels()==0,"Unpatched bridge must preserve child input normalization");
    }
    require(gains==3,"Generated patches need three adjacent input attenuverters");
  }
  auto name=owner->getModule<beekapoo::Module>()->getPatchName();require(!name.empty(),"New random subpatch needs a name");
  int count=int(owner->members.size());
  {
    std::vector<int64_t> original;for(const auto& m:owner->members)original.push_back(m.id);
    auto locked=original.front();owner->randomLocks.insert({locked,-1});owner->saveGroup();
    std::vector<int64_t> protectedCables;
    for(auto* cw:rack->getCompleteCables())if(cw->cable->inputModule->id==locked||cw->cable->outputModule->id==locked)protectedCables.push_back(cw->cable->id);
    auto* protectedState=owner->find(locked)->module->toJson();
    for(int attempt=0;attempt<6;++attempt) {
      require(owner->growPatch(),"Grow must find an unlocked cable or processor insertion");
      for(auto id:original)require(APP->engine->getModule(id),"Growth must retain original native module IDs");
      for(auto id:protectedCables)require(APP->engine->getCable(id),"Growth must preserve every locked module connection");
      APP->history->undo();owner->step();require(int(owner->members.size())==count,"Grow Undo must restore original membership");
      APP->history->redo();owner->step();for(auto id:original)require(APP->engine->getModule(id),"Grow Redo must retain original modules");
      APP->history->undo();owner->step();
    }
    require(owner->regeneratePatch(),"Generate with locks must vary the unlocked graph in place");
    for(auto id:original)require(APP->engine->getModule(id),"Locked regeneration must retain native identities");
    for(auto id:protectedCables)require(APP->engine->getCable(id),"Locked regeneration must preserve protected cables");
    auto* after=owner->find(locked)->module->toJson();
    // Rack updates spatial neighbours during Undo/Redo. They are not control
    // or module data state, and these fixtures do not implement expanders.
    for(auto* state:{protectedState,after}){json_object_del(state,"leftModuleId");json_object_del(state,"rightModuleId");}
    require(json_equal(protectedState,after),"A locked module must retain its complete saved state");json_decref(after);json_decref(protectedState);
    APP->history->undo();owner->step();require(int(owner->members.size())==count,"Locked regeneration must undo in one step");
    owner->randomLocks.clear();owner->saveGroup();
    auto* m=owner->getModule<beekapoo::Module>();m->inputs[beekapoo::Module::GROW_INPUT].channels=1;m->inputs[beekapoo::Module::GROW_INPUT].setVoltage(10);
    engine::Module::ProcessArgs args;args.sampleRate=48000;args.sampleTime=1.f/48000;m->process(args);owner->step();
    require(owner->status=="Subpatch grown","Grow CV must invoke the same native graph action");
    m->inputs[beekapoo::Module::GROW_INPUT].setVoltage(0);m->inputs[beekapoo::Module::GROW_INPUT].channels=0;m->process(args);APP->history->undo();owner->step();
  }
  auto* sink=add(plugin::getModel("computerscare","demo-gain"),Vec(2000,0));
  std::vector<app::CableWidget*> monitor;
  int destination=0;
  for(const auto& port:owner->ports)if(!port.input) {
    auto* source=owner->findPort(port);auto* c=new engine::Cable;
    c->outputModule=source->module;c->outputId=source->portId;c->inputModule=sink->module;c->inputId=destination++;
    APP->engine->addCable(c);auto* cw=new app::CableWidget;cw->setCable(c);rack->addCable(cw);monitor.push_back(cw);
  }
  APP->engine->stepBlock(4096);
  for(int i=0;i<destination;++i)require(sink->module->inputs[i].isConnected()&&std::isfinite(sink->module->inputs[i].getVoltage()),"Generated output must carry finite native DSP");
  for(auto* cw:monitor){rack->removeCable(cw);delete cw;}rack->removeModule(sink);delete sink;

  auto* parent=owner->parent;auto position=owner->box.pos;parent->removeChild(owner);owner->box.pos=Vec();
  capture(owner,output+"/41-generated-beek.png",scale,false);owner->box.pos=position;parent->addChild(owner);
  owner->editSubpatch();
  APP->scene->box.size=Vec(1200,740);APP->scene->rackScroll->box=Rect(Vec(0,30),Vec(1200,710));
  owner->focusEditedMembers();
  auto* session=static_cast<beekapoo::RackEditSession*>(owner->editSession.get());
  for(int frame=0;frame<3;++frame){APP->scene->rackScroll->step();session->step();}
  capture(APP->scene,output+"/42-generated-subpatch.png",scale,false);
  owner->closeEditSession();
  // Repeated UI steps must never regenerate; restore also clears the fresh flag.
  auto* saved=owner->module->dataToJson();owner->module->dataFromJson(saved);json_decref(saved);owner->step();
  require(int(owner->members.size())==count&&owner->getModule<beekapoo::Module>()->getPatchName()==name,"Reload must retain generated name and patch");
  APP->history->undo();
  require(!rack->getModule(id)&&APP->engine->getModuleIds().empty(),"Undo browser add must remove the entire generated patch");
  APP->history->redo();owner=dynamic_cast<beekapoo::Beekapoo*>(rack->getModule(id));require(owner,"Redo must restore Beekapoo");owner->step();
  require(int(owner->members.size())==count&&owner->getModule<beekapoo::Module>()->getPatchName()==name,"Redo must restore the same patch and name");
  auto* rename=owner->beginAction("rename Beekapoo");owner->getModule<beekapoo::Module>()->setPatchName("fold reed 27");owner->finishAction(rename);
  APP->history->undo();owner->step();require(owner->getModule<beekapoo::Module>()->getPatchName()==name,"Rename must undo");
  APP->history->redo();owner->step();require(owner->getModule<beekapoo::Module>()->getPatchName()=="fold reed 27","Rename must redo");
  auto exportPath=output+"/generated-beek.vcv";owner->saveBeek(exportPath);
  auto* target=static_cast<beekapoo::Beekapoo*>(add(modelBeekapooPreview,Vec(1000,0)));require(target->loadBeek(exportPath),"Generated Beek must export and import");
  require(int(target->members.size())==count&&target->getModule<beekapoo::Module>()->getPatchName()=="fold reed 27","Export/import must retain the user name and all modules");
  {
    rack->clear();APP->history->clear();
    auto* owner=static_cast<beekapoo::Beekapoo*>(add(modelBeekapooPreview,Vec()));require(owner->generateDefaultPatch(),"Reroll fixture must generate");
    auto* source=add(plugin::getModel("Fundamental","VCO"),Vec(1000,0));auto* sink=add(plugin::getModel("Fundamental","VCF"),Vec(1200,0));
    std::set<int64_t> oldIds;for(const auto& member:owner->members)oldIds.insert(member.id);
    auto oldName=owner->getModule<beekapoo::Module>()->getPatchName();auto style=owner->appearance;
    app::PortWidget *in=nullptr,*out=nullptr;for(const auto& port:owner->ports){if(port.input&&!in)in=owner->findPort(port);if(!port.input)out=owner->findPort(port);}
    auto wire=[&](app::PortWidget* from,app::PortWidget* to){auto* c=new engine::Cable;c->outputModule=from->module;c->outputId=from->portId;c->inputModule=to->module;c->inputId=to->portId;APP->engine->addCable(c);auto* cw=new app::CableWidget;cw->setCable(c);rack->addCable(cw);return c->id;};
    std::set<int64_t> boundaryIds{wire(source->getOutput(0),in),wire(out,sink->getInput(3))};
    owner->step();owner->refreshAppearance();require(owner->appearance==style,"Exterior cables must leave the drawing identity unchanged");
    int beforeHistory=APP->history->actionIndex;owner->regenerateButton->action();
    require(APP->history->actionIndex==beforeHistory+1,"Generate button must form one undo transaction");
    int inputs=0,outputs=0;for(const auto& p:owner->ports)(p.input?inputs:outputs)++;
    require(inputs==3&&outputs==1,"Regeneration must expose three inputs and one output");
    std::set<int64_t> nextIds;for(const auto& member:owner->members){nextIds.insert(member.id);require(!oldIds.count(member.id),"Regeneration must replace the old contents");}
    for(auto id:oldIds)require(!rack->getModule(id),"Regeneration must remove old modules");
    auto connected=[&]{int n=0;for(auto* cw:rack->getCompleteCables()){auto* c=cw->cable;bool routedInput=c->inputModule==owner->module&&c->inputId>=beekapoo::Module::inputBase;if((c->outputModule==source->module&&(owner->contains(c->inputModule)||routedInput))||(owner->contains(c->outputModule)&&c->inputModule==sink->module))++n;}return n;};
    require(connected()==2,"Regeneration must retain exterior input/output patching by jack index");
    APP->history->undo();owner->step();require(owner->getModule<beekapoo::Module>()->getPatchName()==oldName&&owner->members.size()==oldIds.size(),"Undo must restore the prior name and contents");for(auto id:oldIds)require(owner->contains(APP->engine->getModule(id)),"Undo must restore original module identities");
    for(auto id:boundaryIds)require(rack->getCable(id),"Undo must restore original exterior cable identities");
    APP->history->redo();owner->step();for(auto id:nextIds)require(owner->contains(APP->engine->getModule(id)),"Redo must restore regenerated modules");require(connected()==2,"Redo must retain exterior patching");
    rack->clear();APP->history->clear();
  }
  for(int count:{3,6,9}) {
    rack->clear();APP->history->clear();
    const char* slugs[]={"VCO","LFO","VCF","Delay","ADSR","VCA-1"};
    for(int i=0;i<count;++i) {
      auto* w=add(plugin::getModel("Fundamental",slugs[i%6]),Vec(i*180,0));
      rack->select(w,true);
    }
    beekapoo::createSelectedGroup();
    beekapoo::Beekapoo* owner=nullptr;
    for(auto* w:rack->getModules())if(auto* b=dynamic_cast<beekapoo::Beekapoo*>(w))owner=b;
    require(owner&&int(owner->members.size())==count,"Create from real modules must retain the complete selection");
    int ins=0,outs=0;for(const auto& p:owner->ports)(p.input?ins:outs)++;
    require(ins>=2&&ins<=std::min(8,2+(count+1)/2)&&outs>=1&&outs<=(count+1)/2&&owner->parameters.size()>=3&&owner->parameters.size()<=size_t(3+count),"Real selections must receive bounded, module-count-based default exposures");
    std::set<int64_t> represented;for(const auto& p:owner->parameters)represented.insert(p.id);
    require(represented.size()>=std::min(size_t(count),owner->parameters.size()),"Default parameters must be spread across selected modules");
    auto ownerId=owner->module->id;auto numParams=owner->parameters.size();
    auto* patch=APP->engine->toJson();rack->mergeJson(patch);rack->clear();APP->engine->fromJson(patch);rack->fromJson(patch);json_decref(patch);
    owner=static_cast<beekapoo::Beekapoo*>(rack->getModule(ownerId));owner->step();
    require(owner->parameters.size()==numParams&&owner->ports.size()==size_t(ins+outs),"Loading must retain the chosen defaults");
    owner->requestedWidth=count==3?90:count==6?150:210;owner->layoutControls();
    renderGeneratedBeekRow(owner,output+"/selection-defaults-"+std::to_string(count)+".png",scale);
    auto* parent=owner->parent;auto position=owner->box.pos;parent->removeChild(owner);owner->box.pos=Vec();
    capture(owner,output+"/selection-panel-"+std::to_string(count)+".png",scale,false);
    owner->box.pos=position;parent->addChild(owner);
  }
  std::vector<json_t*> examples;
  for(int example=1;example<=10;++example) {
    rack->clear();APP->history->clear();
    auto* fresh=static_cast<beekapoo::Beekapoo*>(add(modelBeekapooPreview,Vec()));
    require(fresh->generateDefaultPatch(),"Fresh comparison example must generate");
    const float widths[]={90,90,105,120,150,90,120,150,180,240};
    fresh->requestedWidth=widths[example-1];fresh->layoutControls();
    if(example>5) {
      for(const auto& member:fresh->members) {
        auto* w=fresh->find(member.id);
        for(auto* q:w->module->paramQuantities)if(q&&q->randomizeEnabled&&q->getRange()>0&&w->getParam(q->paramId)&&fresh->parameters.size()<size_t(example))fresh->exposeParameter(member.id,q->paramId);
      }
    }
    if(example==4 && fresh->parameters.size()<=4) {
      // Long captions can use the compact 30-32 px controls. The separate
      // roomy-layout regression still checks that spare space enlarges them.
      for(const auto& p:fresh->parameters)if(p.knob->visible)require(p.knob->box.size.x>=30,"Sparse panel must retain usable control sizes alongside its captions");
    }
    APP->engine->stepBlock(4096);
    renderGeneratedBeekRow(fresh,output+"/generated-example-"+std::to_string(example)+".png",scale);
    examples.push_back(fresh->toJson());
  }
  rack->clear();APP->history->clear();
  {
    widget::Widget sheet;std::vector<beekapoo::Beekapoo*> panels;
    for(int i=0;i<10;++i) {
      auto* panel=static_cast<beekapoo::Beekapoo*>(add(modelBeekapooPreview,Vec(i*400,0)));
      panel->fromJson(examples[i]);panel->step();json_decref(examples[i]);
      auto* parent=panel->parent;auto position=panel->box.pos;
      parent->removeChild(panel);panel->box.pos=Vec();
      capture(panel,output+"/generated-panel-"+std::to_string(i+1)+".png",scale,false);
      panel->box.pos=position;parent->addChild(panel);
      panels.push_back(panel);
    }
    float widths[5]{},lefts[5]{},total=10,cellHeight=404;
    for(int i=0;i<10;++i)widths[i%5]=std::max(widths[i%5],panels[i]->box.size.x);
    for(int col=0;col<5;++col){lefts[col]=total;total+=widths[col]+16;}
    for(int i=0;i<10;++i) {auto* panel=panels[i];panel->parent->removeChild(panel);panel->box.pos=Vec(lefts[i%5],10+(i/5)*cellHeight);sheet.addChild(panel);}
    sheet.box.size=Vec(total,cellHeight*2);capture(&sheet,output+"/ten-generated-beeks.png",scale,false);
    for(auto* panel:panels){sheet.removeChild(panel);rack->getModuleContainer()->addChild(panel);}
  }
  rack->clear();APP->history->clear();
  std::vector<app::ModuleWidget*> oscillators;
  for(int i=0;i<3;++i)oscillators.push_back(add(plugin::getModel("Fundamental","VCO"),Vec(i*150,0)));
  auto* grouped=static_cast<beekapoo::Beekapoo*>(add(modelBeekapooPreview,Vec(600,0)));
  rack->deselectAll();for(auto* w:oscillators)rack->select(w,true);grouped->collapse();
  for(auto* w:oscillators){grouped->exposeParameter(w->module->id,2);grouped->exposeParameter(w->module->id,5);}
  grouped->expose(oscillators[0],oscillators[0]->getInput(0));grouped->expose(oscillators[1],oscillators[1]->getInput(0));
  grouped->expose(oscillators[0],oscillators[0]->getOutput(0));grouped->expose(oscillators[2],oscillators[2]->getOutput(2));
  grouped->requestedWidth=180;grouped->layoutControls();
  for(int param:{2,5}) {
    float y=-1;std::vector<float> xs;
    for(const auto& mapped:grouped->parameters)if(mapped.paramId==param){require(mapped.knob->visible&&mapped.knob->box.size.x>=16,"Repeated parameters must grow when there is room");if(y<0)y=mapped.knob->box.getCenter().y;require(std::abs(y-mapped.knob->box.getCenter().y)<.01f,"Matching controls must occupy one symmetrical row");xs.push_back(mapped.knob->box.getCenter().x);}
    require(xs.size()==3&&std::abs((xs[1]-xs[0])-(xs[2]-xs[1]))<.01f,"Matching controls must be evenly spaced");
  }
  for(const char* label:{"Frequency","Pulse width"}) {
    int captions=0;for(const auto& caption:grouped->controlLabels)if(caption.text==label)++captions;
    require(captions==1,"Matching controls must share exactly one caption across their group");
  }
  auto* firstKnob=dynamic_cast<beekapoo::ParametricKnob*>(grouped->parameters[0].knob);
  auto* secondKnob=dynamic_cast<beekapoo::ParametricKnob*>(grouped->parameters[2].knob);
  require(firstKnob&&secondKnob&&firstKnob->style.hue==secondKnob->style.hue&&firstKnob->profile==secondKnob->profile,"Identical parameters from identical module types must share procedural appearance");
  renderGeneratedBeekRow(grouped,output+"/grouped-vco-controls.png",scale);
  std::puts("Generated Beek: real Fundamental 3-11 modules, required inputs, connected graph, exposure, six HP, browser add, Undo/Redo, rename and export/import passed");
}
