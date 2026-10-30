#pragma once
inline void checkVolyMulti(rack::plugin::Model* model, const std::string& output, float scale) {
  using M=ComputerscareVolyPector;
  auto require=[](bool ok,const char* why){if(!ok)throw std::runtime_error(why);};
  auto* m=static_cast<M*>(model->createModule());APP->engine->addModule(m);
  auto w=std::unique_ptr<ComputerscareVolyPectorWidget>(static_cast<ComputerscareVolyPectorWidget*>(model->createModuleWidget(m)));
  for(int out=0;out<16;++out)for(int ch=0;ch<16;++ch)m->setMultiValue(out*16+ch,float((out*17+ch*7)%101)*.17f-8.f);
  auto connect=[&](int count,int poly){for(int out=0;out<16;++out)m->outputs[out].channels=out<count?poly:0;m->params[M::POLY_CHANNELS].setValue(poly);m->checkPoly();w->step();};
  auto click=[&](int index,bool channel,int mods=0){
    VolyPectorLabelButton* button=nullptr;
    for(auto* child:w->children)if(auto* b=dynamic_cast<VolyPectorLabelButton*>(child))if(b->outputLabel!=channel&&(channel?b->channelIndex:b->outputIndex)==index)button=b;
    require(button&&button->visible,"Selection button exists and remains visible");
    widget::EventContext context;widget::Widget::ButtonEvent event;event.context=&context;event.button=GLFW_MOUSE_BUTTON_LEFT;event.mods=mods;event.action=GLFW_PRESS;button->onButton(event);event.action=GLFW_RELEASE;button->onButton(event);w->step();
  };
  auto visible=[&](){int count=0;for(auto* k:w->multiPanel->knobs)if(k&&k->visible)++count;return count;};
  auto bounds=[&](){
    for(auto* k:w->multiPanel->knobs)if(k&&k->visible)require(k->box.pos.x>=0&&k->box.pos.y>=0&&k->box.pos.x+k->box.size.x<=w->multiPanel->width&&k->box.pos.y+k->box.size.y<=w->multiPanel->box.size.y,"Multi knobs fit their panel");
    for(auto* a:w->multiPanel->knobs)if(a&&a->visible)for(auto* b:w->multiPanel->knobs)if(b&&b->visible&&a->address<b->address){bool overlap=a->box.pos.x<b->box.pos.x+b->box.size.x&&a->box.pos.x+a->box.size.x>b->box.pos.x&&a->box.pos.y<b->box.pos.y+b->box.size.y&&a->box.pos.y+a->box.size.y>b->box.pos.y;float dx=std::min(a->box.pos.x+a->box.size.x,b->box.pos.x+b->box.size.x)-std::max(a->box.pos.x,b->box.pos.x);float dy=std::min(a->box.pos.y+a->box.size.y,b->box.pos.y+b->box.size.y)-std::max(a->box.pos.y,b->box.pos.y);
      // The existing full view has a 1.3 x 1px overlap of transparent SVG
      // corners between Offset and the staggered second knob.
      bool nativeCorner=b->address>=256&&a->address<256&&dx<=1.51f&&dy<=1.01f;

      require(!overlap||nativeCorner,"Multi knobs preserve native corner spacing without face overlap");}
  };
  auto save=[&](const char* name){w->step();bounds();capture(w.get(),output+"/"+name,scale);};
  auto resetByDoubleClick=[&](int address,float value,float expected){
    auto* knob=w->multiPanel->knobs[address];require(knob&&knob->visible,"Reset target is visible");m->setMultiValue(address,value);
    widget::EventState events;events.rootWidget=w.get();
    Vec point=w->multiPanel->box.pos.plus(knob->box.getCenter());
    int before=APP->history->actionIndex;
    events.handleButton(point,GLFW_MOUSE_BUTTON_LEFT,GLFW_PRESS,0);
    events.handleHover(point,Vec());events.handleButton(point,GLFW_MOUSE_BUTTON_LEFT,GLFW_RELEASE,0);
    events.handleButton(point,GLFW_MOUSE_BUTTON_LEFT,GLFW_PRESS,0);
    require(m->multiValue(address)==expected,"Double-click resets the actual hit-tested knob");
    events.handleHover(point,Vec());events.handleHover(point,Vec());
    events.handleButton(point,GLFW_MOUSE_BUTTON_LEFT,GLFW_RELEASE,0);
    require(m->multiValue(address)==expected,"Post-double-click drag updates preserve reset");
    require(APP->history->actionIndex==before+1,"Double-click reset makes one undo action");
    APP->history->undo();require(m->multiValue(address)==value,"Reset undo restores previous value");
    APP->history->redo();require(m->multiValue(address)==expected,"Reset redo restores default");
    events.setSelectedWidget(nullptr);
  };

  connect(3,2);click(0,false);require(!m->multiView()&&w->box.size.x==120,"Single selection keeps original width");save("multi-01-single.png");
  click(1,false,RACK_MOD_CTRL);click(2,false,RACK_MOD_CTRL);
  require(m->selectedBands==7&&m->selectedChannels==0&&w->multiPanel->blocks.size()==3&&visible()==12,"ABC gives three 2-channel value blocks");
  require(!w->getParam(M::GLOBAL_SCALE)->visible&&!w->getParam(M::GLOBAL_OFFSET)->visible,"Single-view pair is replaced by independent group pairs");
  require(w->multiPanel->knobs[256]->visible&&w->multiPanel->knobs[256]->box.size==Vec(24,24),"Group offset keeps normal size");
  require(w->multiPanel->knobs[272]->visible&&w->multiPanel->knobs[272]->box.size==Vec(18,18),"Group scale keeps normal size");
  require(w->multiPanel->knobs[256]->box.pos.y<w->multiPanel->knobs[0]->box.pos.y,"Group controls precede values");

  {
    plugin::Plugin destinationPlugin;destinationPlugin.brand="Test";
    plugin::Model destinationModel;destinationModel.plugin=&destinationPlugin;destinationModel.name="Receiver";
    auto* target=new engine::Module;target->config(0,2,0,0);target->model=&destinationModel;target->configInput(0,"Pitch");target->configInput(1,"Level");APP->engine->addModule(target);
    auto destination=std::unique_ptr<app::ModuleWidget>(new app::ModuleWidget);destination->setModule(target);
    for(int i=0;i<2;++i)destination->addInput(createInput<JackSmallPointingDown>(Vec(10+i*20,10),target,i));
    struct CableScope { std::vector<app::CableWidget*> cables; ~CableScope(){for(auto* cable:cables){APP->scene->rack->removeCable(cable);delete cable;}} } patched;
    for(int i=0;i<2;++i){auto* cable=new app::CableWidget;cable->outputPort=w->getOutput(M::OUTPUT+1);cable->inputPort=destination->getInput(i);cable->updateCable();APP->scene->rack->addCable(cable);patched.cables.push_back(cable);}
    auto* value=w->multiPanel->knobs[17];
    std::string hint=value->tooltipText();
    require(hint.find("Bravo Band, Channel 2")!=std::string::npos&&hint.find("to Test Receiver: Pitch input")!=std::string::npos&&hint.find("to Test Receiver: Level input")!=std::string::npos,"Multi value tooltip lists its actual fan-out destinations");
    require(w->multiPanel->knobs[0]->tooltipText().find("Pitch input")==std::string::npos,"Destination help does not use the focused band's jack");
    require(w->multiPanel->knobs[273]->tooltipText().find("Pitch input")!=std::string::npos,"Band scale includes destination help");
    require(w->multiPanel->knobs[257]->tooltipText().find("Level input")!=std::string::npos,"Band offset includes destination help");
    require(w->multiPanel->knobs[304]->tooltipText().find("Pitch input")==std::string::npos,"Shared channel controls retain normal tooltip scope");
    bool wasEnabled=settings::tooltips;settings::tooltips=false;value->onEnter(widget::Widget::EnterEvent{});require(!value->tooltip,"Multi tooltips respect Rack setting");settings::tooltips=true;value->onEnter(widget::Widget::EnterEvent{});require(value->tooltip,"Hover creates destination tooltip");
    auto* tooltip=value->tooltip;APP->scene->removeChild(tooltip);
    widget::Widget composition;composition.box.size=Vec(300,380);composition.addChild(w.get());composition.addChild(tooltip);
    capture(&composition,output+"/multi-10-destination-tooltip.png",scale);
    composition.removeChild(w.get());composition.removeChild(tooltip);APP->scene->addChild(tooltip);
    auto* last=patched.cables.back();patched.cables.pop_back();APP->scene->rack->removeCable(last);delete last;
    tooltip->step();require(tooltip->text.find("Level input")==std::string::npos&&tooltip->text.find("Pitch input")!=std::string::npos,"Destination tooltip updates after unplugging");
    value->clearTooltip();settings::tooltips=wasEnabled;
  }
  connect(3,2); // Restore the synthetic connection fixture after real cables.
  resetByDoubleClick(0,4.75f,0.f);resetByDoubleClick(256,-2.5f,0.f);resetByDoubleClick(272,.375f,1.f);
  save("multi-11-double-click-band.png");
  float scaleA=m->multiValue(272),scaleB=m->multiValue(273);m->setMultiValue(273,.625f);
  require(m->multiValue(272)==scaleA&&m->multiValue(273)==.625f,"Group scale edits only its own band");m->setMultiValue(273,scaleB);
  float offsetA=m->multiValue(256),offsetB=m->multiValue(257);m->setMultiValue(257,1.75f);
  require(m->multiValue(256)==offsetA&&m->multiValue(257)==1.75f,"Group offset edits only its own band");m->setMultiValue(257,offsetB);

  require(w->multiPanel->knobs[0]->box.size.x==computerscare::voly::layout::calculate(3,2).diameter,"Small selection uses planned native-scaled size");
  auto* nativeValue=dynamic_cast<VolyPectorDisableableSmoothKnob*>(w->getParam(M::KNOB));
  auto* nativeOffset=dynamic_cast<VolyPectorNoRandomMediumSmallKnob*>(w->getParam(M::GLOBAL_OFFSET));
  auto* nativeScale=dynamic_cast<VolyPectorNoRandomSmallKnob*>(w->getParam(M::GLOBAL_SCALE));
  require(w->multiPanel->knobs[0]->appearance.sw->svg==nativeValue->sw->svg,"Multi value uses the original SVG artwork");
  require(w->multiPanel->knobs[256]->appearance.sw->svg==nativeOffset->sw->svg&&w->multiPanel->knobs[272]->appearance.sw->svg==nativeScale->sw->svg,"Multi pair uses the original offset/scale artwork");
  require(std::abs(w->multiPanel->knobs[1]->box.pos.y-w->multiPanel->knobs[0]->box.pos.y+5.f*w->multiPanel->knobs[0]->box.size.x/24.f)<.001f,"Multi columns retain the original proportional stagger");
save("multi-02-three-bands.png");
  require(w->getInput(M::FOCUS_CV_INPUT)->box.pos.y>=w->multiPanel->box.pos.y+w->multiPanel->box.size.y,"Multi panel leaves Focus CV fully accessible");
  m->syncView();require(m->selectedBands==7,"Audio view synchronization preserves multi selection");
  auto* edited=w->multiPanel->knobs[17];float dragBefore=m->multiValue(17);
  widget::EventContext dragContext;widget::Widget::DragStartEvent start;start.context=&dragContext;start.button=GLFW_MOUSE_BUTTON_LEFT;edited->onDragStart(start);
  widget::Widget::DragMoveEvent move;move.context=&dragContext;move.button=GLFW_MOUSE_BUTTON_LEFT;move.mouseDelta=Vec(0,-5);edited->onDragMove(move);
  widget::Widget::DragEndEvent finish;finish.context=&dragContext;finish.button=GLFW_MOUSE_BUTTON_LEFT;edited->onDragEnd(finish);
  require(m->multiValue(17)!=dragBefore,"Dragging a multi knob changes its addressed value");APP->history->undo();require(m->multiValue(17)==dragBefore,"Multi knob drag undo restores the value");
  widget::EventContext textContext;widget::Widget::ButtonEvent right;right.context=&textContext;right.button=GLFW_MOUSE_BUTTON_RIGHT;right.action=GLFW_PRESS;edited->onButton(right);
  auto* overlay=APP->scene->children.back();ui::TextField* field=nullptr;for(auto* child:overlay->children.front()->children)if(auto* f=dynamic_cast<ui::TextField*>(child))field=f;
  require(field,"Multi knob right-click offers numeric entry");field->text="3.5";widget::Widget::SelectKeyEvent enter;enter.context=&textContext;enter.key=GLFW_KEY_ENTER;enter.action=GLFW_PRESS;field->onSelectKey(enter);
  require(m->multiValue(17)==3.5f,"Multi numeric entry writes the addressed value");APP->scene->removeChild(overlay);delete overlay;APP->history->undo();require(m->multiValue(17)==dragBefore,"Numeric entry supports undo");

  float old=m->multiValue(17);m->setMultiValue(17,4.25f);m->selectOutputView(1);require(m->params[M::KNOB+1].getValue()==4.25f,"Multi edits survive single-view selection");m->setMultiValue(17,old);
  click(0,false);click(1,false,RACK_MOD_CTRL);click(2,false,RACK_MOD_CTRL);
  // Wiggle only visible selected values; hidden cells and scale/offset survive.
  float hidden=m->multiValue(15*16+15), hiddenChannel=m->multiValue(2), offset=m->multiValue(256);
  m->randomizeCurrentView(computerscare::volypector::RandomizeMode::WIGGLE);
  require(m->multiValue(255)==hidden&&m->multiValue(2)==hiddenChannel&&m->multiValue(256)==offset,"Multi wiggle leaves hidden cells and offset unchanged");
  click(0,true,RACK_MOD_CTRL);require(!m->multiView()&&m->selectedBands==0&&m->selectedChannels==1,"Opposite axis replaces the old selection");
  click(1,true,RACK_MOD_CTRL);require(m->selectedChannels==3&&w->multiPanel->blocks.size()==2&&visible()==10,"Two channels give two ABC blocks");save("multi-03-two-channels.png");
  resetByDoubleClick(16,3.25f,0.f);resetByDoubleClick(288,-1.75f,0.f);resetByDoubleClick(304,.625f,1.f);
  save("multi-12-double-click-channel.png");

  float channelScale=m->multiValue(304);m->setMultiValue(305,.75f);require(m->multiValue(304)==channelScale&&m->multiValue(305)==.75f,"Channel scale belongs only to its group");m->setMultiValue(305,1.f);

  click(1,true,RACK_MOD_CTRL);require(m->selectedChannels==1,"Modifier removes selection");click(0,true,RACK_MOD_CTRL);require(m->selectedChannels==1,"Cannot remove last selection");
  click(0,false);connect(4,8);for(int i=1;i<4;++i)click(i,false,RACK_MOD_CTRL);
  require(m->selectedBands==15&&visible()==32&&w->box.size.x==120,"Four bands stay at 8HP");click(4,false,RACK_MOD_CTRL);require(m->selectedBands==15,"Fifth band is ignored");save("multi-04-four-bands.png");
  connect(16,16);require(visible()==64&&w->box.size.x==120,"Maximum 64 value knobs stay at 8HP");save("multi-05-maximum.png");
  auto* data=m->dataToJson();auto clone=std::unique_ptr<M>(static_cast<M*>(model->createModule()));clone->params[M::OUTPUT_SELECTOR].setValue(m->params[M::OUTPUT_SELECTOR].getValue());clone->dataFromJson(data);json_decref(data);require(clone->selectedBands==15&&clone->selectedChannels==0,"Selection survives patch data reload");require(clone->multiValue(17)==m->multiValue(17),"Hidden values survive patch data reload");
  click(0,true);for(int i=1;i<4;++i)click(i,true,RACK_MOD_CTRL);click(4,true,RACK_MOD_CTRL);
  require(m->selectedChannels==15&&visible()==64&&w->box.size.x==120,"Four-channel maximum stays fixed and refuses fifth");save("multi-06-four-channels.png");
  connect(0,2);click(0,false);click(1,false,RACK_MOD_CTRL);click(2,false,RACK_MOD_CTRL);require(visible()==12,"No cables preserves show-all fallback");save("multi-07-unpatched.png");
  connect(1,1);require(visible()==3,"Connection and polyphony changes filter multi controls");connect(3,2);
  m->setMultiValue(17,0.f);m->setMainKnobRange(false);save("multi-08-unipolar.png");
  require(std::abs(w->multiPanel->knobs[17]->lastDrawnAngle-w->multiPanel->knobs[17]->appearance.minAngle)<.001f,"Native artwork updates on unipolar range changes");
  m->setMainKnobRange(true);save("multi-09-bipolar.png");
  require(std::abs(w->multiPanel->knobs[17]->lastDrawnAngle)<.001f,"Native artwork updates on bipolar range changes");

  // Undo is addressed to the value, not the view it happened to occupy.
  auto* knob=w->multiPanel->knobs[17];old=m->multiValue(17);m->setMultiValue(17,3.125f);knob->record(old);click(10,false);APP->history->undo();require(m->multiValue(17)==old,"Multi undo addresses hidden value");APP->history->redo();require(m->multiValue(17)==3.125f,"Multi redo addresses hidden value");
  require(w->box.size.x==120,"Normal click restores single width");
  // Automatic filtering remains; only groups chosen while inactive override it.
  connect(1,2);click(3,false);click(0,false,RACK_MOD_CTRL);
  require(w->multiPanel->blocks.size()==2&&w->multiPanel->knobs[48]->visible&&w->multiPanel->knobs[49]->visible,"Explicit inactive band remains visible with active band");
  require(!w->multiPanel->knobs[50]->visible&&!w->multiPanel->knobs[16]->visible,"Inactive opposite-axis members stay filtered");
  require(m->multiDestinationVisible(3,0)&&!m->multiDestinationVisible(3,2),"Randomization and reset use the displayed cells");
  require(w->multiPanel->knobs[48]->inactive&&!w->multiPanel->knobs[0]->inactive,"Only inactive band main knobs use grey artwork");
  require(!w->multiPanel->knobs[259]->inactive&&!w->multiPanel->knobs[275]->inactive,"Inactive band offset and scale retain normal artwork");
  resetByDoubleClick(48,3.5f,0.f);
  save("multi-13-explicit-inactive-band.png");
  click(3,false,RACK_MOD_CTRL);require(m->explicitInactiveBands==0,"Removing a group clears its visibility override");
  click(0,true);click(5,true,RACK_MOD_CTRL);
  require(w->multiPanel->blocks.size()==2&&w->multiPanel->knobs[5]->visible&&!w->multiPanel->knobs[21]->visible,"Explicit inactive channel shows connected bands only");
  require(m->multiDestinationVisible(0,5)&&!m->multiDestinationVisible(1,5),"Channel override does not unfilter unpatched bands");
  require(w->multiPanel->knobs[5]->inactive&&!w->multiPanel->knobs[0]->inactive,"Inactive channel main knobs use grey artwork");
  require(!w->multiPanel->knobs[293]->inactive&&!w->multiPanel->knobs[309]->inactive,"Inactive channel offset and scale remain colored");
  save("multi-14-explicit-inactive-channel.png");
  auto* explicitData=m->dataToJson();auto explicitClone=std::unique_ptr<M>(static_cast<M*>(model->createModule()));
  explicitClone->params[M::OUTPUT_SELECTOR].setValue(m->params[M::OUTPUT_SELECTOR].getValue());
  explicitClone->params[M::CHANNEL_SELECTOR].setValue(m->params[M::CHANNEL_SELECTOR].getValue());
  explicitClone->dataFromJson(explicitData);json_decref(explicitData);
  require(explicitClone->explicitInactiveChannels==(1u<<5),"Explicit inactive selection survives patch reload");
  connect(2,2);require(w->multiPanel->knobs[21]->visible,"Newly active opposite-axis members appear automatically");
  connect(1,2);require(!w->multiPanel->knobs[21]->visible&&w->multiPanel->knobs[5]->visible,"Automatic filtering survives explicit selection");
  click(0,false);click(1,false,RACK_MOD_CTRL);m->inputs[M::FOCUS_CV_INPUT].channels=1;w->step();require(!m->multiView()&&w->box.size.x==120,"Focus CV returns to single layout");
  std::puts("Voly multi-selection axes, filtering, edits, undo, persistence, layout bounds and fixed-width renders passed");
}
