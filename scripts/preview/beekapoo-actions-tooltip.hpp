#pragma once
template<class Add, class Require>
void testBeekActionsTooltip(const std::string& output,float scale,Add add,Require require,plugin::Model* gainModel) {
  using beekapoo::Module;
  auto* rack=APP->scene->rack;
  auto* source=add(gainModel,Vec());
  auto* voice=add(gainModel,Vec(90,0));
  auto* owner=static_cast<beekapoo::Beekapoo*>(add(modelBeekapooPreview,Vec(300,0)));
  rack->deselectAll();rack->select(voice,true);owner->collapse();
  for(int p:{0,1,2})owner->exposeParameter(voice->module->id,p);
  owner->expose(voice,voice->getOutput(0));owner->requestedWidth=255;owner->layoutControls();owner->step();
  auto* m=owner->getModule<Module>();
  auto* jack=dynamic_cast<beekapoo::ActionsCvJack*>(owner->getInput(Module::ACTIONS_INPUT));
  require(jack,"Actions input uses decorated native jack");
  settings::tooltips=true;APP->event->setHoveredWidget(jack);
  require(jack->tip&&jack->tip->native,"Native tooltip is retained");jack->tip->step();
  for(int i=0;i<16;++i)require(jack->tip->text.find(std::to_string(i+1)+" ["+beekapoo::ActionsTooltip::channelLabel(i)+"]: no connection")!=std::string::npos,"Unpatched jack must list every possible channel as disconnected");
  auto* cable=new engine::Cable;cable->outputModule=source->module;cable->outputId=0;cable->inputModule=m;cable->inputId=Module::ACTIONS_INPUT;APP->engine->addCable(cable);
  auto* cw=new app::CableWidget;cw->setCable(cable);rack->addCable(cw);
  auto& input=m->inputs[Module::ACTIONS_INPUT];
  input.channels=1;input.setVoltage(3.125f);jack->tip->step();
  require(jack->tip->text.find("1 [wig exp]:  3.125V")!=std::string::npos,"Mono shows numbered function and native voltage precision");
  require(jack->tip->text.find("2 [rnd exp]: no connection")!=std::string::npos&&jack->tip->text.find("16 [unused]: no connection")!=std::string::npos,"Mono must explicitly mark missing lanes without inventing voltages");
  require(jack->tip->text.find(source->module->model->getFullName())!=std::string::npos,"Native source module details are retained");
  auto render=[&](const char* name){
    auto* tip=jack->tip.get();auto* parent=owner->parent;Vec pos=owner->box.pos,tipPos=tip->box.pos;
    parent->removeChild(owner);APP->scene->removeChild(tip);
    widget::Widget shot;shot.box.size=Vec(owner->box.size.x+tip->box.size.x+18,std::max(owner->box.size.y,tip->box.size.y+85));
    owner->box.pos=Vec();tip->box.pos=Vec(owner->box.size.x+10,85);shot.addChild(owner);shot.addChild(tip);
    capture(&shot,output+name,scale,false);
    shot.removeChild(tip);shot.removeChild(owner);owner->box.pos=pos;tip->box.pos=tipPos;parent->addChild(owner);APP->scene->addChild(tip);
  };
  require(!owner->getParam(Module::SEQUENCE_SCAN),"Manual Scan widget must not exist");
  m->panelTheme=1;owner->applyTheme();render("/actions-mono-light.png");
  input.channels=16;for(int i=0;i<16;++i)input.setVoltage(i%2?0.f:5.f,i);input.setVoltage(-2.25f,8);jack->tip->step();
  for(int i=0;i<16;++i)require(jack->tip->text.find(std::to_string(i+1)+" ["+beekapoo::ActionsTooltip::channelLabel(i)+"]:")!=std::string::npos,"Every present lane has its function, including unused lanes");
  require(jack->tip->text.find("9 [scan]: -2.250V")!=std::string::npos,"Signed voltages retained");
  input.channels=9;jack->tip->step();require(jack->tip->text.find("10 [unused]: no connection")!=std::string::npos,"Reduced polyphony replaces stale voltages with no connection");
  m->panelTheme=2;owner->exposeParameter(voice->module->id,5);owner->layoutControls();owner->step();jack->tip->step();render("/actions-poly-dark.png");
  input.setVoltage(1.25f,0);jack->tip->step();require(jack->tip->text.find("1.250V")!=std::string::npos,"Live values refresh");
  APP->event->setHoveredWidget(nullptr);require(!jack->tip,"Tooltip closes on leave");
  APP->event->setHoveredWidget(jack);settings::tooltips=false;jack->step();require(!jack->tip,"Tooltip setting clears existing overlay");APP->event->setHoveredWidget(nullptr);settings::tooltips=true;
  widget::Widget::DragEnterEvent drag;drag.origin=source->getOutput(0);drag.button=GLFW_MOUSE_BUTTON_LEFT;jack->onDragEnter(drag);
  require(jack->tip,"Native cable drag hover is decorated");jack->step();require(!jack->tip,"Lost drag hover clears the overlay");
  APP->event->setHoveredWidget(jack);WeakPtr<ui::Tooltip> stale=jack->tip.get();APP->event->setHoveredWidget(nullptr);rack->clear();APP->history->clear();
  require(!stale||!stale->visible,"Deleting module leaves no visible tooltip");
  std::puts("Actions tooltip: mono/9/16 lanes, live/signed/zero voltages, native source details, channel changes and hover lifecycle passed");
}
