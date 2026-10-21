#pragma once
inline void renderTolyPensorChecks(rack::plugin::Model* model,const std::string& output,float scale){
  using T=ComputerscareTolyPensor;
  auto require=[](bool ok,const char* text){if(!ok)throw std::runtime_error(text);};
  auto* module=static_cast<T*>(model->createModule());
  APP->engine->addModule(module);
  auto widget=std::unique_ptr<ComputerscareTolyPensorWidget>(static_cast<ComputerscareTolyPensorWidget*>(model->createModuleWidget(module)));
  rack::engine::Module::ProcessArgs args;args.sampleRate=48000;args.sampleTime=1.f/48000;args.frame=0;
  auto run=[&](int samples=64){for(int i=0;i<samples;++i){args.frame++;module->process(args);}};
  module->inputs[0].channels=2;module->inputs[1].channels=1;
  module->outputs[0].channels=1;module->outputs[1].channels=1;
  module->inputs[0].setVoltage(4,0);module->inputs[0].setVoltage(6,1);module->inputs[1].setVoltage(3,0);
  module->matrix.set(toly::cell(0,0,0,0),.5f);module->matrix.set(toly::cell(1,0,0,0),-1);
  module->matrix.scale[0][0]=2;module->matrix.offset[0][0]=1;
  module->matrix.scale[1][0]=.5f;module->matrix.offset[1][0]=1;
  module->matrix.scale[3][0]=2;module->matrix.offset[3][0]=.5f;
  module->matrix.scale[2][0]=3;module->matrix.offset[2][0]=-1;
  module->matrix.changed();run();
  require(module->outputs[0].getChannels()==2,"Auto polyphony follows the largest input");
  require(std::abs(module->outputs[0].getVoltage(0)-2.5f)<1e-6 && std::abs(module->outputs[0].getVoltage(1)-38)<1e-6,"Real SIMD processing composes all four affine stages around mixing");
  require(module->inactive(toly::cell(0,3,0,0))&&!module->inactive(toly::cell(0,0,0,0)),"Missing input polyphony fades only unavailable routes");
  require(module->inactive(toly::cell(0,0,5,0)),"Unpatched outputs fade their routes");
  module->params[T::POLY].setValue(4);run();
  require(module->outputs[0].getChannels()==4&&module->outputs[0].getVoltage(2)==-1,"Manual output polyphony retains output offsets on silent channels");
  for(int scope=0;scope<4;++scope){
    module->action(4,2,0,false);
    module->params[T::RANDOM_RANGE].setValue(.5f);module->params[T::RANDOM_CHANCE].setValue(1);
    module->action(scope,0,5,false);bool changed=false;
    for(int index=0;index<toly::cells;++index){auto x=toly::indices(index);float v=module->matrix.get(index);if(x[scope]==5){require(v>=-.5f&&v<=.5f,"Scoped randomization respects its range");changed|=v!=(index/256==index%256?1:0);}else require(v==(index/256==index%256?1:0),"Scoped randomization leaves every other tensor plane unchanged");}
    require(changed,"Each dimension randomizes its selected plane");
    module->action(scope,2,5,false);
    for(int index=0;index<toly::cells;++index)require(module->matrix.get(index)==(index/256==index%256?1:0),"Scoped Initialize restores exact matching routes");
  }
  module->params[T::WIGGLE_RANGE].setValue(.1f);module->action(4,1,0,false);
  for(int index=0;index<toly::cells;++index)require(std::abs(module->matrix.get(index)-(index/256==index%256?1:0))<=.100001f,"Wiggle is bounded around existing values");
  auto saved=std::unique_ptr<json_t,void(*)(json_t*)>(module->dataToJson(),json_decref);
  std::array<float,256> savedRow;for(int i=0;i<256;++i)savedRow[i]=module->matrix.get(i);
  module->action(4,2,0,false);module->dataFromJson(saved.get());
  for(int i=0;i<256;++i)require(module->matrix.get(i)==savedRow[i],"Saved sparse routes round-trip without losing coefficients");
  module->params[T::SELECT].setValue(1);
  module->params[T::RANDOM_CHANCE].setValue(0);
  module->action(0,0,1,false);
  for(int i=0;i<256;++i)require(module->matrix.get(i)==savedRow[i],"Zero probability leaves all values untouched");
  module->params[T::RANDOM_CHANCE].setValue(1);
  module->action(4,2,0,true);APP->history->undo();
  for(int i=0;i<256;++i)require(module->matrix.get(i)==savedRow[i],"Button undo restores actual cells independent of the view");
  APP->history->redo();
  module->matrix.set(toly::cell(2,0,0,0),.75f);module->matrix.changed();
  int initPort=T::ACTION+4*3+2;
  module->inputs[initPort].channels=1;module->inputs[initPort].setVoltage(0);run();
  module->inputs[initPort].setVoltage(10);run();
  require(module->matrix.get(toly::cell(2,0,0,0))==0,"Init CV triggers the scoped action");
  module->matrix.set(toly::cell(2,0,0,0),.75f);run();
  require(module->matrix.get(toly::cell(2,0,0,0))==.75f,"A held trigger does not repeatedly initialize");
  module->inputs[initPort].channels=0;
  module->action(4,2,0,false);
  int scopedInit=T::ACTION+2;
  module->matrix.set(toly::cell(1,0,0,0),.75f);module->matrix.set(toly::cell(2,0,0,0),.5f);module->matrix.changed();
  module->params[T::SELECT].setValue(2);
  module->inputs[scopedInit].channels=4;
  for(int i=0;i<4;++i)module->inputs[scopedInit].setVoltage(0,i);run();
  module->inputs[scopedInit].setVoltage(10,1);run();
  require(module->matrix.get(toly::cell(1,0,0,0))==0&&module->matrix.get(toly::cell(2,0,0,0))==.5f,
          "Poly action channels address their matching indices instead of the selected one");
  module->inputs[scopedInit].channels=1;module->inputs[scopedInit].setVoltage(0);run();
  module->inputs[scopedInit].setVoltage(10);run();
  require(module->matrix.get(toly::cell(2,0,0,0))==0,"Mono action CV addresses the selected index");
  module->inputs[scopedInit].channels=0;
  module->params[T::SELECT].setValue(1);
  module->action(4,2,0,false);
  for(auto* child:widget->children) {
    if(auto* button=dynamic_cast<TolyAction*>(child)) {
      if(button->kind!=2)continue;
      rack::widget::EventContext hit;
      rack::event::Button click;click.context=&hit;click.button=GLFW_MOUSE_BUTTON_LEFT;click.action=GLFW_PRESS;
      click.pos=button->box.getCenter();widget->onButton(click);
      require(hit.target==button&&button->down,"Scope buttons receive mouse clicks through the panel");
      button->onDragEnd(rack::event::DragEnd());
    }
  }
  int focusButtons=0;
  for(auto* child:widget->children) if(auto* button=dynamic_cast<TolyFocusButton*>(child)) {
    rack::widget::EventContext hit; rack::event::Button click;
    click.context=&hit;click.button=GLFW_MOUSE_BUTTON_LEFT;click.action=GLFW_PRESS;
    click.pos=button->box.getCenter();widget->onButton(click);
    require(hit.target==button && int(module->params[T::SELECT+button->axis].getValue())==button->index,
            "Every port and channel focus button is clickable and selects its index");
    require(button->isPressed(),"Selected focus buttons stay depressed");
    ++focusButtons;
  }
  require(focusButtons==68,"Four columns expose sixteen indices plus All each");
  for(int axis=0;axis<4;++axis)module->params[T::SELECT+axis].setValue(0);
  for(auto* port:widget->getPorts()){
    rack::widget::EventContext hit;rack::event::Button click;
    click.context=&hit;click.button=GLFW_MOUSE_BUTTON_LEFT;click.action=GLFW_PRESS;
    click.pos=port->box.getCenter();widget->onButton(click);
    require(dynamic_cast<rack::app::PortWidget*>(hit.target),"All audio and action jacks remain clickable");
  }
  module->params[T::SELECT+1].setValue(-1);module->params[T::VIEW].setValue(1);widget->step();
  int visible=0;for(auto* knob:widget->grid->knobs)visible+=knob->visible;
  require(visible==16,"Minimum view exposes exactly sixteen knobs");
  capture(widget.get(),output+"/01-sixteen-knobs.png",scale);
  module->params[T::VIEW].setValue(2);
  module->params[T::SELECT].setValue(0);module->params[T::SELECT+2].setValue(0);
  module->params[T::SELECT+3].setValue(-1);module->params[T::ROW_AXIS].setValue(1);module->params[T::COLUMN_AXIS].setValue(3);widget->step();
  visible=0;for(auto* knob:widget->grid->knobs)visible+=knob->visible;
  require(visible==256,"Grid view exposes exactly 256 knobs");
  auto* knob=widget->grid->knobs[0];
  auto* history=new TolyChange;history->moduleId=module->id;history->name="test route edit";
  int edited=knob->index();history->values.push_back({edited,module->value(edited),.37f});module->setValue(edited,.37f);module->matrix.changed();APP->history->push(history);
  module->params[T::SELECT].setValue(3);APP->history->undo();require(module->value(edited)==1,"Changing slices before undo still restores the original connection");APP->history->redo();require(module->value(edited)==.37f,"Redo addresses the original connection");
  module->params[T::SELECT].setValue(0);
  TolyValueField field;field.moduleId=module->id;field.index=edited;field.text="1/3";
  rack::event::SelectKey enter;enter.action=GLFW_PRESS;enter.key=GLFW_KEY_ENTER;
  field.onSelectKey(enter);
  require(std::abs(module->value(edited)-1.f/3.f)<1e-6,"Exact-value editing accepts arithmetic fractions");
  APP->history->undo();require(module->value(edited)==.37f,"Exact-value editing supports undo");
  rack::widget::EventContext knobHit;rack::event::Button press;
  press.context=&knobHit;press.button=GLFW_MOUSE_BUTTON_LEFT;press.action=GLFW_PRESS;press.pos=knob->box.pos+widget->grid->box.pos+knob->box.size*.5f;
  widget->onButton(press);require(knobHit.target==knob,"Matrix knobs receive mouse clicks");
  rack::event::DragStart beginDrag;beginDrag.button=GLFW_MOUSE_BUTTON_LEFT;knob->onDragStart(beginDrag);
  rack::event::DragMove move;move.mouseDelta=Vec(0,-10);knob->onDragMove(move);knob->onDragEnd(rack::event::DragEnd());
  require(module->value(edited)>.37f,"Dragging a connection changes its actual coefficient");
  module->params[T::SELECT].setValue(5);APP->history->undo();require(module->value(edited)==.37f,"A real knob gesture undoes correctly after selecting another slice");
  module->params[T::SELECT].setValue(0);module->params[T::RANDOM_RANGE].setValue(.7f);module->action(2,0,0,false);run();widget->step();
  capture(widget.get(),output+"/02-grid-connected.png",scale);
  module->params[T::VIEW].setValue(0);widget->step();visible=0;for(auto* k:widget->grid->knobs)visible+=k->visible;require(visible==0,"None hides every connection control while audio remains active");
  capture(widget.get(),output+"/03-controls-hidden.png",scale);
  // Every All permutation selects a contextual slice without changing routes.
  for(int mask=0;mask<16;++mask){
    int count=0;for(int axis=0;axis<4;++axis){bool all=mask&(1<<axis);count+=all;module->params[T::SELECT+axis].setValue(all?-1:axis+1);}
    widget->step();int shown=0;for(auto* k:widget->grid->knobs)shown+=k->visible;
    // Restore display after the hidden-controls preview.
    module->params[T::VIEW].setValue(1);widget->step();shown=0;for(auto* k:widget->grid->knobs)shown+=k->visible;
    require(shown==(count>2?0:count==2?256:count==1?16:1),"All permutations expose only the defined contextual controls");
    if(count<=2){auto fixed=module->selected();if(module->rowAxis()>=0)fixed[module->rowAxis()]=4;if(module->columnAxis()>=0)fixed[module->columnAxis()]=7;
      require(module->indexFor(4,7)==toly::cell(fixed[0],fixed[1],fixed[2],fixed[3]),"Contextual knobs address the selected matrix slice");}
  }
  module->params[T::SELECT].setValue(-1);module->params[T::SELECT+1].setValue(3);
  module->params[T::SELECT+2].setValue(-1);module->params[T::SELECT+3].setValue(6);widget->step();
  capture(widget.get(),output+"/04-port-to-port.png",scale);
  module->params[T::SELECT].setValue(0);module->params[T::SELECT+1].setValue(-1);
  module->params[T::SELECT+2].setValue(0);module->params[T::SELECT+3].setValue(-1);widget->step();
  capture(widget.get(),output+"/05-channel-to-channel.png",scale);
  // Benchmark real Rack SIMD kernels, excluding rendering and plan compilation.
  auto bench=std::unique_ptr<toly::Matrix>(new toly::Matrix);
  std::array<int,16> widths;std::array<float,256> input{},mixed{};
  for(int i=0;i<256;++i)input[i]=std::sin(i*.13f);
  volatile float checksum=0;
  auto measure=[&](const char* name,int frames){
    auto begin=std::chrono::steady_clock::now();
    for(int sample=0;sample<frames;++sample){input[0]=sample*.00001f;bench->process<simd::float_4>(input,mixed);checksum+=mixed[0];}
    double elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count();
    std::printf("Toly Pensor %s: %.3f ms per second of 48 kHz audio (%zu active SIMD packs)\n",name,elapsed*1000.*48000/frames,bench->activePacks());
  };
  widths.fill(16);bench->prepare(widths,65535,16);measure("identity",4800);
  for(int i=0;i<toly::cells;++i)bench->set(i,.003f);bench->changed();bench->prepare(widths,65535,16);measure("fully dense",2400);
  widths.fill(0);widths[0]=4;bench->prepare(widths,1,4);measure("one four-channel port",4800);
  widths.fill(16);bench->prepare(widths,1,16);measure("sixteen inputs, one output, dense",4800);
  for(int i=0;i<toly::cells;++i)bench->set(i,i/256==i%256?1.f:0.f);
  bench->changed();bench->prepare(widths,1,16);measure("sixteen inputs, one output, identity",4800);
  module->action(4,2,0,false);
  for(int p=0;p<16;++p){module->inputs[p].channels=16;module->outputs[p].channels=p==0?1:0;}
  module->params[T::POLY].setValue(16);run();
  auto moduleMeasure=[&](const char* name){auto begin=std::chrono::steady_clock::now();run(48000);std::printf("Toly Pensor full module %s: %.3f ms per second at 48 kHz\n",name,std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count()*1000.);};
  moduleMeasure("sixteen inputs, one output, identity");
  for(int i=0;i<toly::cells;++i)module->matrix.set(i,.003f);
  module->matrix.changed();run();moduleMeasure("sixteen inputs, one output, dense");
  for(int p=0;p<16;++p)module->outputs[p].channels=p<5?1:0;
  run();moduleMeasure("sixteen inputs, five outputs, dense");
  module->action(4,2,0,false);run();moduleMeasure("sixteen inputs, five outputs, identity");
  (void)checksum;
  std::puts("Toly Pensor native SIMD, scope isolation, trigger edges, persistence, Undo/Redo and view checks passed");
}
