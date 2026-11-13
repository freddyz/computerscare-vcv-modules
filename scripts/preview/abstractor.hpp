// Isolated Rack-engine integration test, not connected to a live Rack process.
namespace abstractorfixture {
struct Gain : engine::Module {
  int randomCalls=0,resetCalls=0;
  void onRandomize(const RandomizeEvent& e) override {++randomCalls;engine::Module::onRandomize(e);}
  void onReset(const ResetEvent& e) override {++resetCalls;engine::Module::onReset(e);}
  Gain() {
    config(24, 24, 24, 0);
    for(int i=0;i<24;++i) {
      configParam(i,0.f,2.f,1.f,i==0?"Gain":"Gain "+std::to_string(i+1));
      configInput(i,i==0?"Signal":"Input "+std::to_string(i+1));
      configOutput(i,i==0?"Signal":"Output "+std::to_string(i+1));
    }
    configButton(1,"Momentary");
    configSwitch(2,0.f,1.f,0.f,"Toggle");
    configSwitch(3,0.f,3.f,0.f,"Position");
  }
  void process(const ProcessArgs&) override {
    outputs[0].setChannels(4);
    for(int ch=0;ch<4;++ch)
      outputs[0].setVoltage((inputs[0].isConnected() ? inputs[0].getVoltage(ch) : (ch+1.f)) * params[0].getValue(), ch);
  }
};
struct Face : app::ModuleWidget {
  Face(Gain* m) {
    setModule(m); box.size = Vec(90,380);
    for(int i=0;i<24;++i) {
      addInput(createInputCentered<JackLargePointingDown>(Vec(25,20+i*14),m,i));
      addOutput(createOutputCentered<JackLargePointingUp>(Vec(65,20+i*14),m,i));
    }
    addParam(createParamCentered<SmallKnob>(Vec(45,180),m,0));
    for(int i=1;i<=3;++i) {
      auto* sw=createParamCentered<app::Switch>(Vec(45,200+i*20),m,i);
      sw->momentary=i==1;addParam(sw);
    }
  }
};
}
void renderAbstractorDemo(plugin::Plugin& plugin,const std::string& output,float scale) {
  auto require=[](bool pass,const char* why){if(!pass)throw std::runtime_error(why);};
  // Only this renderer process gets a fake watcher control path. Never send
  // commands to the user's watcher or running Rack session.
  setenv("VCVMON_CONTROL",(output+"/isolated-watcher").c_str(),1);
  auto* rack=APP->scene->rack;
  auto* gainModel=createModel<abstractorfixture::Gain,abstractorfixture::Face>("demo-gain");
  gainModel->name="Demo gain";
  plugin.addModel(gainModel); plugin.addModel(modelAbstractorPreview);
  plugin::plugins.push_back(&plugin);
  struct Cleanup {
    plugin::Plugin& plugin;plugin::Model* gain;
    ~Cleanup() {
      APP->scene->rack->clear();
      plugin::plugins.erase(std::find(plugin::plugins.begin(),plugin::plugins.end(),&plugin));
      plugin.models.remove(gain);plugin.models.remove(modelAbstractorPreview);delete gain;
    }
  } cleanup{plugin,gainModel};
  auto add=[&](plugin::Model* model,Vec pos){
    auto* m=model->createModule();APP->engine->addModule(m);
    auto* w=model->createModuleWidget(m);w->box.pos=pos;rack->addModule(w);return w;
  };
  auto* a=add(gainModel,Vec(0,0));
  auto* sink=add(gainModel,Vec(90,0));
  auto* b=add(gainModel,Vec(180,0));
  auto* guard=add(gainModel,Vec(915,0));
  const int64_t guardId=guard->module->id;
  auto* host=static_cast<patchabstractor::Abstractor*>(add(modelAbstractorPreview,Vec(300,0)));
  const int64_t aId=a->module->id,bId=b->module->id,sinkId=sink->module->id,hostId=host->module->id;
  auto connect=[&](app::ModuleWidget* from,app::ModuleWidget* to){
    auto* c=new engine::Cable;c->outputModule=from->module;c->outputId=0;c->inputModule=to->module;c->inputId=0;
    APP->engine->addCable(c);auto* cw=new app::CableWidget;cw->setCable(c);cw->color=nvgRGB(231,183,71);rack->addCable(cw);return cw;
  };
  auto* internal=connect(a,b);auto* external=connect(b,sink);
  const int64_t internalId=internal->cable->id,externalId=external->cable->id;
  APP->engine->stepBlock(64);
  std::vector<float> reference;
  for(int ch=0;ch<4;++ch)reference.push_back(sink->module->outputs[0].getVoltage(ch));
  auto cableIds=[] { auto ids=APP->engine->getCableIds();std::sort(ids.begin(),ids.end());return ids; };
  auto ids=cableIds();
  Vec originalInput=a->getInput(0)->box.pos,originalOutput=b->getOutput(0)->box.pos;
  rack->select(a,true);rack->select(b,true);host->collapse();
  require(host->members.size()==2 && !a->visible && !b->visible && sink->visible,"Selection must use membership rather than a rectangle");
  require(rack->getModule(aId)!=a && rack->getModule(aId)->getInput(0)==a->getInput(0),"Registered holder must preserve original port lookup");
  require(rack->getModule(aId)->box.size==Vec() && a->box.size==Vec(90,380),"Holder must have no footprint without resizing the real widget");
  require(!internal->visible && external->visible,"Internal cable must hide; external cable must stay visible");
  require(cableIds()==ids,"Collapse changed cable identities");
  APP->engine->stepBlock(64);
  for(int ch=0;ch<4;++ch)require(sink->module->outputs[0].getVoltage(ch)==reference[ch],"Collapse changed polyphonic output");
  require(rack->requestModulePos(sink,Vec(0,0)),"Freed rack space must accept another module");
  require(rack->requestModulePos(host,Vec(315,0)),"Zero-size holders must not block container movement");
  host->expose(a,a->getInput(0));host->exposeParameter(bId,0);
  auto* mapped=dynamic_cast<app::ParamWidget*>(host->controls->children.back());
  require(mapped && mapped->getParamQuantity()==b->module->getParamQuantity(0),"Exposed parameter must target original module");
  mapped->getParamQuantity()->setImmediateValue(.5f);APP->engine->stepBlock(64);
  require(sink->module->outputs[0].getVoltage(3)==2.f,"Mapped parameter must affect original DSP");
  for(int i=1;i<24;++i) {host->expose(a,a->getInput(i));host->exposeParameter(bId,i);}
  host->expose(a,a->getInput(0));host->exposeParameter(bId,0);
  require(host->ports.size()==25 && host->parameters.size()==24,"Controls must support more than 16 ports and one parameter without duplicates");
  require(host->box.size.x>300,"Panel must grow to fit all exposed controls");
  auto* moment=dynamic_cast<patchabstractor::ExposedSwitch*>(host->parameters[1].knob);
  auto* toggle=dynamic_cast<patchabstractor::ExposedSwitch*>(host->parameters[2].knob);
  auto* positions=dynamic_cast<patchabstractor::ExposedSwitch*>(host->parameters[3].knob);
  require(moment && moment->momentary && toggle && !toggle->momentary && positions,"Exposed switch kinds must match original widgets");
  event::DragStart press;press.button=GLFW_MOUSE_BUTTON_LEFT;
  event::DragEnd release;release.button=GLFW_MOUSE_BUTTON_LEFT;
  moment->onDragStart(press);b->step();
  require(b->module->params[1].getValue()==1.f,"Momentary must press");
  moment->onDragEnd(release);b->step();b->step();
  require(b->module->params[1].getValue()==0.f,"Momentary must release");
  toggle->onDragStart(press);toggle->onDragEnd(release);
  require(b->module->params[2].getValue()==1.f,"Toggle must latch");
  positions->onDragStart(press);positions->onDragEnd(release);
  positions->onDragStart(press);positions->onDragEnd(release);
  require(b->module->params[3].getValue()==2.f,"Multi-position switch must cycle");
  Vec savedGuardPosition=guard->box.pos;
  host->step();
  auto& proxy=host->ports.front();
  Vec expected=host->controls->getRelativeOffset(proxy.face->box.getCenter(),rack);
  require((external->getOutputPos()-expected).norm()<.01f,"Boundary cable must follow container movement");
  if(std::getenv("VCVMON_CONTROL")) {
    bool pause=false, save=false;
    widget::Widget* toolbar=nullptr;
    for(auto* child:APP->scene->children)for(auto* control:child->children)
      if(auto* button=dynamic_cast<ui::Button*>(control)) {
        pause |= button->text=="Pause" || button->text=="Resume";
        save |= button->text=="Save version";
        if(button->text=="Save version") toolbar=child;
      }
    require(toolbar && save,"Abstractor's standard panel must attach the dev toolbar");
    toolbar->step();
    for(auto* control:toolbar->children)
      if(auto* button=dynamic_cast<ui::Button*>(control)) pause |= button->text=="Pause" || button->text=="Resume";
    require(pause,"Dev toolbar must include Pause/Resume");
    auto* parent=toolbar->parent;auto position=toolbar->box.pos;
    parent->removeChild(toolbar);toolbar->box.pos=Vec();
    capture(toolbar,output+"/03-dev-toolbar.png",scale,false);
    toolbar->box.pos=position;parent->addChild(toolbar);
  }
  {
    auto* own=static_cast<patchabstractor::Module*>(host->module);
    auto* gainA=static_cast<abstractorfixture::Gain*>(a->module);auto* gainB=static_cast<abstractorfixture::Gain*>(b->module);
    std::vector<float> savedA,savedB;
    for(int i=0;i<24;++i){savedA.push_back(gainA->getParamQuantity(i)->getValue());savedB.push_back(gainB->getParamQuantity(i)->getValue());}
    host->parameterAction(2,true);
    require(gainB->getParamQuantity(0)->getValue()==1 && gainA->getParamQuantity(0)->getValue()==savedA[0],"Initialize visible must affect only exposed params");
    APP->history->undo();require(gainB->getParamQuantity(0)->getValue()==savedB[0],"Initialize visible undo failed");
    widget::EventContext ctx;widget::Widget::HoverKeyEvent key;key.context=&ctx;key.key=GLFW_KEY_R;key.mods=RACK_MOD_CTRL;key.action=GLFW_PRESS;host->onHoverKey(key);
    require(ctx.consumed && gainA->randomCalls==0 && gainB->randomCalls==0 && gainA->getParamQuantity(0)->getValue()==savedA[0],"Mod+R must randomize exposed params without whole-module callbacks");
    APP->history->undo();
    own->params[patchabstractor::Module::WIGGLE_AMOUNT].setValue(.1f);
    host->parameterAction(0,true);
    for(int i=0;i<24;++i)require(std::abs(gainB->getParamQuantity(i)->getValue()-savedB[i])<=.20001f,"Wiggle exceeded selected amount");
    APP->history->undo();
    host->parameterAction(4,true);require(gainA->randomCalls==1 && gainB->randomCalls==1,"Randomize all must call each module's native callback");
    APP->history->undo();host->parameterAction(5,true);require(gainA->resetCalls==1 && gainB->resetCalls==1,"Initialize all must call each module's native callback");
    APP->history->undo();
    size_t historySize=APP->history->actions.size();
    engine::Module::ProcessArgs args;args.sampleRate=48000;args.sampleTime=1.f/48000;args.frame=0;
    own->inputs[1].setVoltage(5);own->process(args);host->step();
    require(APP->history->actions.size()==historySize && gainA->getParamQuantity(0)->getValue()==savedA[0],"Visible randomize CV must not fill undo history or affect hidden parameters");
    own->inputs[1].setVoltage(0);own->process(args);
    for(int index=0;index<6;++index) {
      int beforeRandom=gainA->randomCalls,beforeReset=gainA->resetCalls;
      host->actionPulse[index]=0;own->inputs[index].setVoltage(0);own->process(args);
      own->inputs[index].setVoltage(5);own->process(args);host->step();
      require(host->actionPulse[index]>0 && APP->history->actions.size()==historySize,"Every action CV must dispatch its cell without adding history");
      require(gainA->randomCalls==beforeRandom+(index==4) && gainA->resetCalls==beforeReset+(index==5),"Action CV routed to the wrong module callback");
      host->actionPulse[index]=0;own->process(args);host->step();require(host->actionPulse[index]==0,"Held action CV must not repeatedly fire");
      own->inputs[index].setVoltage(0);own->process(args);
    }

    for(int i=0;i<24;++i){gainA->getParamQuantity(i)->setImmediateValue(savedA[i]);gainB->getParamQuantity(i)->setImmediateValue(savedB[i]);}
  }
  // Real Rack save/load: serialize engine state and native widget positions,
  // destroy every widget and module, then recreate them through their models.
  json_t* patch=APP->engine->toJson();rack->mergeJson(patch);
  char* encoded=json_dumps(patch,JSON_INDENT(2));require(encoded,"Patch must serialize as valid JSON");
  std::ofstream(output+"/roundtrip.json")<<encoded;std::free(encoded);
  rack->clear();require(APP->engine->getModuleIds().empty(),"Rack clear must release held modules exactly once");
  APP->engine->fromJson(patch);rack->fromJson(patch);
  host=dynamic_cast<patchabstractor::Abstractor*>(rack->getModule(hostId));require(host,"Reload lost container");
  host->step();
  require(rack->getModule(guardId)->box.pos==savedGuardPosition,"Reload shoved an unrelated module");
  a=host->find(aId);b=host->find(bId);sink=rack->getModule(sinkId);
  require(a && b && sink && host->members.size()==2 && !a->visible && !b->visible,"Saved grouping must restore on patch load");
  require(host->ports.size()==25 && host->parameters.size()==24 && host->parameters[0].id==bId && host->parameters[0].paramId==0,"Reload lost manual port or exposed parameter");
  require(rack->getModule(aId)->box.size==Vec() && rack->getModule(bId)->box.size==Vec(),"Reload restored unwanted rack footprints");
  require(!rack->getCable(internalId)->visible && rack->getCable(externalId)->visible,"Reload lost cable visibility");
  require(cableIds()==ids,"Reload changed cable identities");
  APP->engine->stepBlock(64);
  for(int ch=0;ch<4;++ch)require(sink->module->outputs[0].getVoltage(ch)==reference[ch]*.5f,"Reload changed polyphonic audio");
  APP->scene->box.size=Vec(1100,650);
  host->showView();auto* view=dynamic_cast<patchabstractor::SubpatchView*>(host->subview.get());
  require(view,"Show must open a subpatch view");
  auto* originalRackParent=rack->parent;auto rackBox=rack->box;
  auto registeredModules=rack->getModules();auto selectedModules=rack->getSelected();
  auto* touchedBefore=rack->getTouchedParam();
  view->step();
  require(rack->parent==originalRackParent && rack->box.pos==rackBox.pos && rack->box.size==rackBox.size && rack->getModules()==registeredModules && rack->getSelected()==selectedModules && rack->getTouchedParam()==touchedBefore,"Subview callbacks must restore main Rack geometry, registry, selection and touched parameter");
  widget::EventContext blankContext;widget::Widget::HoverEvent blankHover;
  blankHover.context=&blankContext;blankHover.pos=Vec(-50,-50);
  view->nativeRack->onHover(blankHover);
  require(blankHover.getTarget()!=rack,"Subview background must never target the live Rack");
  require(!a->visible && !b->visible && rack->getModule(aId)->box.size==Vec(),"Show must not expand or ungroup members");
  require(view->canvas->box.size.isFinite() && view->scroll->containerBox.size.isFinite(),"Subview canvas and scroll bounds must be finite");
  auto* farFace=static_cast<patchabstractor::SubpatchModuleFace*>(view->nativeRack->getModules().back());
  Vec savedSourceSize=farFace->source->box.size;
  farFace->source->box.size.x=1800;APP->scene->box.size=Vec(600,480);view->step();view->step();
  require(view->scroll->horizontalScrollbar->visible && view->scroll->verticalScrollbar->visible,"Large subpatch must scroll on both axes");
  for(auto* bar:{view->scroll->horizontalScrollbar,view->scroll->verticalScrollbar}) {
    widget::EventContext context;widget::Widget::ButtonEvent e;e.context=&context;e.action=GLFW_PRESS;e.button=GLFW_MOUSE_BUTTON_LEFT;
    e.pos=bar->box.size*.85f;bar->onButton(e);
    widget::Widget::DragMoveEvent drag;drag.mouseDelta=Vec(150,150);bar->onDragMove(drag);view->step();
    require(view->scroll->offset.isFinite() && view->scroll->container->box.pos.isFinite(),"Scrollbar click or drag made the subpatch disappear");
    auto bounds=view->scroll->getContainerOffsetBound();
    require(view->scroll->offset.x>=bounds.pos.x && view->scroll->offset.x<=bounds.getRight() && view->scroll->offset.y>=bounds.pos.y && view->scroll->offset.y<=bounds.getBottom(),"Scroll position must stay inside content bounds");
  }
  view->scroll->offset=Vec(INFINITY,INFINITY);view->step();require(view->scroll->offset.isFinite(),"Invalid scroll offsets must recover");
  farFace->source->box.size=savedSourceSize;view->scroll->offset=Vec();APP->scene->box.size=Vec(1100,650);view->step();
  require(host->members.size()==2 && cableIds()==ids,"Scrolling must not change membership or cables");

  auto* locked=b->getOutput(0);
  require(!view->connect(locked,a->getInput(1)),"Subview must reject external-jack repatching");
  view->disconnect(locked);require(rack->getCable(externalId),"Subview removed external cable");
  auto hitFor=[&](widget::Widget* source) -> patchabstractor::SubpatchHit* {
    for(auto* hit:view->hits)if(hit->param.get()==source)return hit;
    throw std::runtime_error("Subview omitted a real parameter");
  };
  auto clickHit=[&](widget::Widget* source) {
    widget::Widget* target=nullptr;
    if(auto* port=dynamic_cast<app::PortWidget*>(source))target=view->portFor(port);
    else target=hitFor(source);
    require(target,"Subview omitted a real port");
    widget::EventContext ctx;widget::Widget::ButtonEvent e;
    e.context=&ctx;e.action=GLFW_PRESS;e.button=GLFW_MOUSE_BUTTON_LEFT;e.pos=target->box.size*.5f;
    target->onButton(e);return ctx.target;
  };
  view->setMode(true);
  size_t count=host->ports.size();clickHit(locked);
  require(host->ports.size()==count,"External jack exposure must stay locked");
  clickHit(a->getInput(1));require(host->ports.size()==count-1,"Expose mode must remove a jack");
  clickHit(a->getInput(1));require(host->ports.size()==count,"Expose mode must add a jack");
  count=host->parameters.size();clickHit(b->getParam(0));
  require(host->parameters.size()==count-1,"Expose mode must remove a parameter");
  clickHit(b->getParam(0));require(host->parameters.size()==count,"Expose mode must add a parameter");
  view->setMode(false);
  auto* knobHit=hitFor(b->getParam(0));
  require(clickHit(b->getParam(0))==knobHit && knobHit->dragTarget.get()==b->getParam(0),"Adjust mode must forward knob input through its zoom-aware drag target");
  view->setViewZoom(.5f,view->scroll->box.size*.5f);view->step();
  require(view->zoom->getZoom()==.5f && view->scroll->containerBox.size.isFinite(),"Subview zoom must preserve finite scroll bounds");
  Vec adjusted=knobHit->adjustedDragDelta(Vec(10,0));
  require(std::abs(adjusted.x/knobHit->dragTarget->getAbsoluteZoom()-10.f/knobHit->getAbsoluteZoom())<.001f,"Knob drag sensitivity must follow subview zoom");
  view->fitView();
  {
    settings::tooltips=true;widget::Widget::EnterEvent enter;widget::Widget::LeaveEvent leave;
    auto* portHit=view->portFor(a->getInput(0));APP->event->setHoveredWidget(portHit);require(portHit->tooltip,"Port tooltip must use original widget");
    portHit->tooltip->step();portHit->tooltipAnchor->step();
    require(portHit->tooltip->text.find("Signal")!=std::string::npos,"Port tooltip lost its native label");
    require(portHit->tooltip->box.pos.isFinite(),"Tooltip position must remain valid at subview zoom");
    APP->event->setHoveredWidget(nullptr);require(!portHit->tooltip,"Port tooltip must close on leave");
    APP->event->setHoveredWidget(knobHit);require(knobHit->tooltip,"Parameter tooltip must use original widget");knobHit->tooltip->step();knobHit->tooltipAnchor->step();
    require(knobHit->tooltip->text.find("Gain")!=std::string::npos,"Parameter tooltip lost value/label");APP->event->setHoveredWidget(nullptr);
    settings::tooltips=false;APP->event->setHoveredWidget(portHit);require(!portHit->tooltip,"Subview must obey disabled tooltips");APP->event->setHoveredWidget(nullptr);settings::tooltips=true;
  }

  auto* dragSource=view->portFor(a->getOutput(1));
  widget::Widget::DragStartEvent start;start.button=GLFW_MOUSE_BUTTON_LEFT;dragSource->onDragStart(start);
  widget::Widget::DragDropEvent drop;drop.button=GLFW_MOUSE_BUTTON_LEFT;drop.origin=dragSource;
  view->portFor(b->getInput(1))->onDragDrop(drop);
  widget::Widget::DragEndEvent end;end.button=GLFW_MOUSE_BUTTON_LEFT;dragSource->onDragEnd(end);
  require(cableIds().size()==ids.size()+1 && rack->getIncompleteCables().empty(),"Subview drag must create only a complete internal cable");
  APP->history->undo();host->step();require(cableIds()==ids,"Internal repatch undo changed external cables");
  APP->history->redo();host->step();require(cableIds().size()==ids.size()+1,"Internal repatch redo failed");
  view->disconnect(a->getOutput(1));host->step();require(cableIds()==ids,"Internal disconnect failed");
  require(rack->getCable(externalId) && rack->getCable(internalId),"Subview altered protected or unrelated cables");
  {
    auto* source=view->portFor(b->getInput(0));auto* target=view->portFor(b->getInput(2));
    widget::Widget::DragStartEvent start;start.button=GLFW_MOUSE_BUTTON_LEFT;source->onDragStart(start);
    widget::Widget::DragDropEvent drop;drop.button=GLFW_MOUSE_BUTTON_LEFT;drop.origin=source;target->onDragDrop(drop);
    widget::Widget::DragEndEvent end;end.button=GLFW_MOUSE_BUTTON_LEFT;source->onDragEnd(end);
    require(cableIds()==ids && APP->engine->getCable(internalId)->inputId==2,"Dragging an existing plug must move that cable, not add another");
    APP->history->undo();host->step();view->step();require(APP->engine->getCable(internalId)->inputId==0,"Native cable move undo failed");
    APP->history->redo();host->step();view->step();require(APP->engine->getCable(internalId)->inputId==2,"Native cable move redo failed");
    APP->history->undo();host->step();view->step();
  }

  host->closeView();
  // A copied/pasted container must not take ownership of the original group.
  auto* copy=static_cast<patchabstractor::Abstractor*>(add(modelAbstractorPreview,Vec(645,0)));
  json_t* state=host->module->dataToJson();copy->module->dataFromJson(state);json_decref(state);copy->step();
  require(copy->members.empty() && host->members.size()==2 && !a->visible,"Copied container stole a group");
  rack->removeModule(copy);delete copy;
  for(const auto& port:host->ports)require(port.face->box.getRight()<host->box.size.x && port.face->box.getBottom()<350,"Exposed port clipped outside panel");
  for(const auto& param:host->parameters)require(param.knob->box.getRight()<host->box.size.x && param.knob->box.getBottom()<350,"Exposed knob clipped outside panel");
  auto* parent=host->parent;parent->removeChild(host);auto pos=host->box.pos;host->box.pos=Vec();
  capture(host,output+"/02-reloaded.png",scale,false);
  for(int mode=1;mode<=2;++mode) {
    host->viewMode=mode;host->layoutControls();
    capture(host,output+(mode==1?"/10-medium.png":"/11-small.png"),scale,false);
  }
  host->viewMode=0;host->layoutControls();
  auto* mappedButton=dynamic_cast<patchabstractor::MappedParam<patchabstractor::ExposedSwitch>*>(host->parameters[1].knob);
  require(mappedButton,"Mapped button must have tooltip support");
  APP->event->setHoveredWidget(mappedButton);
  require(mappedButton->tip && mappedButton->tip->caption().find("#2")!=std::string::npos,"Mapped button tooltip must identify duplicate module instance");
  APP->event->setHoveredWidget(nullptr);
  require(!mappedButton->tip,"Mapped button tooltip must clear on leave");
  host->box.pos=pos;parent->addChild(host);
  host->groupAction(false);
  APP->history->undo();host->step();
  require(host->members.size()==2 && host->ports.size()==25 && host->parameters.size()==24,"Expand undo lost group or exposed controls");
  APP->history->redo();host->step();require(host->expanded && host->members.size()==2,"Expand redo must retain abstraction");
  require(a->getInput(0)->box.pos==originalInput && b->getOutput(0)->box.pos==originalOutput,"Expand did not restore jack positions");
  auto* registeredA=rack->getModule(aId);auto* registeredB=rack->getModule(bId);
  require(registeredA->visible && registeredB->visible && a->visible && b->visible && rack->getCable(internalId)->visible,"Expand failed");
  require(!registeredA->box.intersects(sink->box) && !registeredB->box.intersects(sink->box),"Expansion overlaps a module occupying freed space");
  require(registeredA->box.pos.y==registeredB->box.pos.y && registeredA->box.getRight()==registeredB->box.pos.x,"Expanded members must form one continuous block");
  require(rack->isSelected(registeredA) && rack->isSelected(registeredB),"Expanded members must remain selected");
  rack->deselectAll();rack->select(rack->getModule(guardId),true);Vec outsidePosition=rack->getModule(guardId)->box.pos;
  host->addSelection();require(host->expanded && host->members.size()==3,"Add selected outside module failed");
  APP->history->undo();host->step();require(host->members.size()==2 && !host->contains(APP->engine->getModule(guardId)) && rack->getModule(guardId)->box.pos==outsidePosition,"Undo add lost outside-module membership or position");
  APP->history->redo();host->step();require(host->members.size()==3,"Redo add failed");
  rack->deselectAll();rack->select(rack->getModule(guardId),true);host->removeSelection();
  require(host->members.size()==2 && rack->getModule(guardId)->visible,"Remove selected member must leave it in rack");
  APP->history->undo();host->step();require(host->members.size()==3,"Undo remove failed");
  APP->history->redo();host->step();require(host->members.size()==2,"Redo remove failed");
  rack->deselectAll();rack->select(rack->getModule(aId),true);host->removeSelection();
  require(host->members.size()==1 && rack->getModule(aId)->visible && host->boundaryPort(b->getInput(0)) && host->exposed(b->getInput(0)) && cableIds()==ids,"Removing a wired member must preserve cables and expose the new boundary");
  APP->history->undo();host->step();require(host->expanded && host->members.size()==2 && !host->boundaryPort(b->getInput(0)),"Undo wired removal must restore internal membership");
  host->endAction();require(host->members.empty() && rack->getModule(aId)->visible && rack->getModule(bId)->visible,"End expanded abstraction failed");
  APP->history->undo();host->step();require(host->expanded && host->members.size()==2,"Undo end must restore expanded group");
  APP->history->redo();host->step();require(host->members.empty(),"Redo end failed");
  APP->history->undo();host->step();
  auto* expandedPatch=APP->engine->toJson();rack->mergeJson(expandedPatch);
  Vec expandedA=rack->getModule(aId)->box.pos,expandedB=rack->getModule(bId)->box.pos;
  rack->clear();APP->engine->fromJson(expandedPatch);rack->fromJson(expandedPatch);json_decref(expandedPatch);
  host=dynamic_cast<patchabstractor::Abstractor*>(rack->getModule(hostId));host->step();
  a=host->find(aId);b=host->find(bId);sink=rack->getModule(sinkId);registeredA=rack->getModule(aId);registeredB=rack->getModule(bId);
  require(host->expanded && host->members.size()==2 && registeredA->box.pos==expandedA && registeredB->box.pos==expandedB,"Expanded grouping and block positions must survive reload");

  widget::EventContext eventContext;widget::Widget::ButtonEvent click;
  click.context=&eventContext;click.button=GLFW_MOUSE_BUTTON_LEFT;click.action=GLFW_PRESS;click.pos=Vec(5,20);
  registeredA->onButton(click);
  require(eventContext.target==registeredA,"Expanded module background must target registered wrapper");
  rack->select(registeredA,true);rack->select(registeredB,true);host->groupAction(true);
  APP->history->undo();host->step();require(host->expanded && host->members.size()==2 && registeredA->visible,"Collapse undo must keep expanded grouping");
  APP->history->redo();host->step();require(host->members.size()==2 && !registeredA->visible,"Abstract redo failed");
  rack->deselectAll();rack->select(rack->getModule(sinkId),true);Vec sinkPosition=rack->getModule(sinkId)->box.pos;
  host->addSelection();require(!host->expanded && host->members.size()==3,"Add while collapsed must stay collapsed");
  APP->history->undo();host->step();require(host->members.size()==2 && rack->getModule(sinkId)->visible && rack->getModule(sinkId)->box.pos==sinkPosition,"Undo collapsed add must release outside module at its original position");
  host->endAction();require(host->members.empty() && registeredA->visible && registeredB->visible,"End collapsed abstraction must release its block");
  APP->history->undo();host->step();require(!host->expanded && host->members.size()==2,"Undo end must restore collapsed grouping");
  rack->removeModule(host);delete host;
  require(rack->getModule(aId)->visible && rack->getModule(bId)->visible && cableIds()==ids,"Removing container must expand without losing audio graph");
  rack->clear();require(APP->engine->getModuleIds().empty(),"Expanded wrappers must release modules exactly once");
  json_t* intact=json_deep_copy(patch);
  // Exercise a missing member on load: recover the available module, don't
  // leave it hidden or attach the group to some unrelated replacement.
  auto* modules=json_object_get(patch,"modules");
  for(size_t i=0;i<json_array_size(modules);++i)
    if(json_integer_value(json_object_get(json_array_get(modules,i),"id"))==aId) {json_array_remove(modules,i);break;}
  APP->engine->fromJson(patch);rack->fromJson(patch);
  host=dynamic_cast<patchabstractor::Abstractor*>(rack->getModule(hostId));host->step();
  require(host->members.empty() && rack->getModule(bId)->visible,"Missing member must recover available modules");
  rack->clear();json_decref(patch);
  APP->engine->fromJson(intact);rack->fromJson(intact);
  host=dynamic_cast<patchabstractor::Abstractor*>(rack->getModule(hostId));host->step();
  rack->touchedParam=host->find(aId)->getParam(0);
  rack->selectAll();rack->deleteSelectionAction();
  require(APP->engine->getModuleIds().empty() && rack->touchedParam==nullptr,"Delete-all left a held module or dangling touched control");
  json_decref(intact);
  // Show actual computerscare modules in both subview modes for visual QA.
  modelVolyMultiPreview->plugin=&plugin;modelComputerscareMelyPorge->plugin=&plugin;
  modelVolyMultiPreview->name="Voly Pector";modelComputerscareMelyPorge->name="Mely Porge";
  auto* voly=add(modelVolyMultiPreview,Vec(0,0));
  auto* mely=add(modelComputerscareMelyPorge,Vec(voly->box.size.x+30,0));
  auto* outside=add(gainModel,Vec(mely->box.getRight()+30,0));
  auto* group=static_cast<patchabstractor::Abstractor*>(add(modelAbstractorPreview,Vec(outside->box.getRight()+30,0)));
  connect(voly,mely);connect(mely,outside);APP->engine->stepBlock(64);voly->step();mely->step();
  rack->select(voly,true);rack->select(mely,true);group->groupAction(true);
  group->exposeParameter(voly->module->id,0);group->exposeParameter(mely->module->id,0);
  group->showView();auto* realView=dynamic_cast<patchabstractor::SubpatchView*>(group->subview.get());
  APP->scene->box.size=Vec(std::max(900.f,mely->box.getRight()+90),620);
  realView->step();
  auto captureView=[&](const std::string& path){
    auto idsBefore=cableIds();
    float rackOpacity=settings::cableOpacity;settings::cableOpacity=.13f;
    std::vector<std::pair<app::CableWidget*,std::pair<app::PortWidget*,app::PortWidget*>>> endpoints;
    for(auto* cable:rack->getCompleteCables())endpoints.push_back({cable,{cable->inputPort,cable->outputPort}});
    auto* parent=realView->parent;parent->removeChild(realView);capture(realView,path,scale,false);parent->addChild(realView);
    require(settings::cableOpacity==.13f,"Subview opacity leaked into main rack setting");settings::cableOpacity=rackOpacity;
    require(realView->canvas->renderedCables>0,"Normal modal draw must render internal native cables without an external layer pass");
    require(cableIds()==idsBefore,"Rendering must not change engine cables");
    for(const auto& p:endpoints)require(p.first->inputPort==p.second.first && p.first->outputPort==p.second.second,"Rendering must restore native rack endpoints");
  };
  captureView(output+"/04-adjust-subpatch.png");
  realView->setMode(true);realView->setViewZoom(.75f,Vec());captureView(output+"/05-expose-subpatch.png");
  {
    realView->fitView();realView->step();
    patchabstractor::SubpatchHit* hit=nullptr;
    for(auto* candidate:realView->hits)if(candidate->param && candidate->visible){hit=candidate;break;}
    require(hit,"Actual subpatch needs a parameter tooltip target");
    widget::Widget::EnterEvent enter;APP->event->setHoveredWidget(hit);require(hit->tooltip,"Actual module's native tooltip was not created");
    hit->tooltip->step();hit->tooltipAnchor->step();
    struct TooltipShot : widget::Widget {
      patchabstractor::SubpatchView* view;ui::Tooltip* tip;
      void draw(const DrawArgs& args) override {
        view->draw(args);nvgSave(args.vg);nvgTranslate(args.vg,tip->box.pos.x,tip->box.pos.y);
        auto local=args;local.clipBox=tip->box.zeroPos();tip->draw(local);nvgRestore(args.vg);
      }
    } shot;
    shot.view=realView;shot.tip=hit->tooltip.get();shot.box.size=realView->box.size;
    capture(&shot,output+"/09-native-tooltip.png",scale,false);
    widget::Widget::LeaveEvent leave;APP->event->setHoveredWidget(nullptr);
  }
  realView->setViewZoom(1.8f,Vec());realView->scroll->offset=Vec(90,80);realView->step();captureView(output+"/07-zoomed-scrolled.png");
  std::vector<Rect> faceBounds;
  for(auto* child:realView->nativeRack->getModuleContainer()->children) {
    for(const auto& prior:faceBounds)require(!prior.intersects(child->box),"Subview module panels must never overlap");
    faceBounds.push_back(child->box);
  }
  group->closeView();
  auto* groupParent=group->parent;auto groupPosition=group->box.pos;
  groupParent->removeChild(group);group->box.pos=Vec();capture(group,output+"/08-action-grid.png",scale,false);group->box.pos=groupPosition;groupParent->addChild(group);
  // The external fixture only demonstrates a locked boundary in the subview;
  // omit that unstyled test module from the rack photograph.
  rack->removeModule(outside);delete outside;
  group->box.pos=Vec(15,0);group->step();group->groupAction(false);
  group->step();
  auto* rackParent=rack->parent;auto rackSize=rack->box.size;auto rackPosition=rack->box.pos;
  float right=0;for(auto* w:rack->getModules())if(w->visible){w->box.pos.y+=12;right=std::max(right,w->box.getRight());}
  rackParent->removeChild(rack);rack->box.pos=Vec();rack->box.size=Vec(right+20,404);
  capture(rack,output+"/06-expanded-selected.png",scale,false);
  rack->box.pos=rackPosition;rack->box.size=rackSize;rackParent->addChild(rack);
  rack->clear();


  std::printf("Abstractor: zero footprint, occupied-space expansion, native save/load, four-channel DSP, exposed controls, copy isolation, missing-member recovery and deletion checks passed.\n");
}
