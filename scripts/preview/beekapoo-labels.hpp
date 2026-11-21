#pragma once
// Real Rack widget/text measurement at several compact widths, using long
// names found in the library checkout rather than short synthetic labels.
template<class Add,class Require>
void renderBeekLabels(const std::string& output,float scale,Add add,Require require) {
  auto* rack=APP->scene->rack;
  auto* model=plugin::getModel("computerscare","demo-gain");
  const char* inputs[]={"Frequency modulation","Channel 12 CV input","Left clock input","High-pass filter input","Probability","1V/Octave pitch","Reset","Cutoff frequency","Threshold","Retrigger","External oscillator frequency","Right channel 2"};
  const char* outputs[]={"Right audio output","End of cycle","Sample and hold","Triangle"};
  int widths[]={90,120,180,285};
  for(int example=0;example<4;++example) {
    rack->clear();APP->history->clear();
    auto* child=add(model,Vec());
    child->module->paramQuantities[0]->name="Cutoff frequency";
    child->module->paramQuantities[5]->name="Channel";
    auto* owner=static_cast<beekapoo::Beekapoo*>(add(modelBeekapooPreview,Vec(300,0)));
    rack->deselectAll();rack->select(child,true);owner->collapse();
    for(int i=0;i<3;++i) {
      child->module->inputInfos[i]->name=inputs[example*3+i];
      owner->expose(child,child->getInput(i));
    }
    child->module->outputInfos[0]->name=outputs[example];owner->expose(child,child->getOutput(0));
    owner->exposeParameter(child->module->id,0);owner->exposeParameter(child->module->id,5);
    auto* state=owner->getModule<beekapoo::Module>();
    state->setPatchName("label "+std::to_string(example+1),true);state->panelTheme.store(example%2?2:1);
    owner->requestedWidth=widths[example];owner->layoutControls();owner->step();
    auto* parent=owner->parent;auto position=owner->box.pos;parent->removeChild(owner);owner->box.pos=Vec();
    capture(owner,output+string::f("/labels-%d.png",example+1),scale,false);
    owner->box.pos=position;parent->addChild(owner);
    int labelled=0;
    for(const auto& label:owner->controlLabels)if(label.required) {
      require(label.prepared&&label.readable&&!label.lines.empty(),"Every exposed jack must have a measured, rendered label");
      require(label.preparedSize>=6.5f,"Common library names must fit without microscopic labels");
      ++labelled;
    }
    require(labelled==4,"Every exposed input/output must reserve caption space");
    for(const auto& p:owner->ports)if(p.gain) {
      auto* knob=dynamic_cast<beekapoo::ThemedSmallKnob*>(p.gain);
      require(knob&&knob->dark==bool(example%2),"Input attenuverters must match the panel theme");
    }
  }
  for(int width:{60,90,120,180,300})for(int count:{1,3,5,8,16}) {
    rack->clear();APP->history->clear();
    auto* child=add(model,Vec());
    auto* owner=static_cast<beekapoo::Beekapoo*>(add(modelBeekapooPreview,Vec(600,0)));
    rack->deselectAll();rack->select(child,true);owner->collapse();
    for(int i=0;i<count;++i)owner->expose(child,child->getInput(i));
    for(int i=0;i<3;++i)owner->expose(child,child->getOutput(i));
    for(int i=0;i<5;++i)owner->exposeParameter(child->module->id,i);
    owner->requestedWidth=width;owner->layoutControls();owner->step();
    int labelled=0;
    for(const auto& label:owner->controlLabels)if(label.required) {
      require(label.width>=14&&label.maxHeight>=10,"Dense grids must reserve usable label rectangles");
      ++labelled;
    }
    require(labelled==count+3,"Dense and smallest panels must retain every input/output caption");
  }
  rack->clear();APP->history->clear();
  std::puts("Beek labels: long library names, all exposed input/output captions, small widths and dark attenuverters passed");
}
