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

  }
  // Ten controls / one output: the narrow and medium panels formerly lost
  // every caption despite room between the knobs (user's "shim peak" case).
  for(int example=0;example<4;++example) {
    rack->clear();APP->history->clear();
    std::vector<app::ModuleWidget*> sources;float x=0;
    for(const char* slug:{"VCO","VCO","Delay","Delay","Random"}) {
      auto* sourceModel=plugin::getModel("Fundamental",slug);require(sourceModel,"Parameter label previews need Fundamental loaded");
      auto* w=add(sourceModel,Vec(x,0));x+=w->box.size.x;sources.push_back(w);
    }
    auto* owner=static_cast<beekapoo::Beekapoo*>(add(modelBeekapooPreview,Vec(x+90,0)));
    rack->deselectAll();for(auto* w:sources)rack->select(w,true);owner->collapse();
    const char* names[]={"Frequency","Frequency modulation","Time","Time CV","Internal trigger rate","Trigger probability"};
    for(int i=0;i<5;++i)for(int j=0;j<2;++j) {
      std::string name=names[(i/2)*2+j];
      if(example==3&&j==1&&i==1)name="FM mode";
      if(example==3&&j==1&&i==4)name="Offset";
      bool found=false;
      for(auto* p:sources[i]->getParams())if(p->getParamQuantity()->name==name) {
        auto* q=p->getParamQuantity();q->setImmediateValue(q->getMinValue()+q->getRange()*float((i*5+j*3+example)%11)/10.f);
        owner->exposeParameter(p->module->id,p->paramId);found=true;break;
      }
      require(found,"Dense label fixture needs its real named control");
    }
    owner->expose(sources[2],sources[2]->getOutputs()[0]);
    int widths[]={90,135,270,180};
    auto* state=owner->getModule<beekapoo::Module>();
    state->setPatchName(beekapoo::generated::name(20261123+example*157),true);state->panelTheme.store(example%2?2:1);
    owner->requestedWidth=widths[example];owner->layoutControls();owner->step();
    auto* parent=owner->parent;auto pos=owner->box.pos;parent->removeChild(owner);owner->box.pos=Vec();
    capture(owner,output+string::f("/param-labels-%d.png",example+1),scale,false);
    owner->box.pos=pos;parent->addChild(owner);
    int captions=0;
    for(const auto& label:owner->controlLabels)if(!label.required) {
      require(label.prepared&&label.readable&&!label.lines.empty(),"Dense parameter captions must actually render, not just reserve space");
      require(label.preparedSize>=8.f,"Dense parameter labels must remain readable");++captions;
    }
    require(captions==(example==3?7:6),"Every parameter family needs a caption at these widths");
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
      require(label.width>=14&&label.maxHeight>=8,"Dense grids must reserve usable label rectangles");
      ++labelled;
    }
    require(labelled<=count+3,"Each exposed port has at most one caption");
    for(const auto& p:owner->ports) {
      require(p.face->visible,"Physical minimum must retain every exposed port");
      auto* block=p.input?owner->inputFrame:owner->outputFrame;
      auto region=beekapoo::Beekapoo::frameContent(block);
      bool hasCaption=false;
      for(const auto& label:owner->controlLabels)if(label.required&&label.output==!p.input&&std::abs(label.pos.x-p.face->box.getCenter().x)<.01f&&label.pos.y>=p.face->box.getBottom()&&label.pos.y-p.face->box.getBottom()<10)hasCaption=true;
      // Missing captions are permitted only in physically packed rows.
      if(!hasCaption)require(region.size.y/float((p.input?count:3))<34,"A roomy port block must retain its labels");
    }
  }
  rack->clear();APP->history->clear();
  std::puts("Beek labels: long library names, all exposed input/output captions, small widths passed");
}
