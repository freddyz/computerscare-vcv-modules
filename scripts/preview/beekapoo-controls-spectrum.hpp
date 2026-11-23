#pragma once
template<class Add, class Require>
void renderBeekTitleCases(const std::string& output,float scale,Add add,Require require) {
  auto* rack=APP->scene->rack;
  const std::vector<std::string> models={"VCO","LFO","VCF","ADSR","8vert","Delay","Random","VCA-1","VCMixer","SEQ3"};
  std::vector<std::vector<std::string>> cases(4);
  for(size_t i=0;i<models.size();++i)for(size_t j=i;j<models.size();++j)for(size_t k=j;k<models.size();++k) {
    std::vector<std::string> names;
    for(size_t n:{i,j,k})names.push_back(beekapoo::moduleStyleName(plugin::getModel("Fundamental",models[n])));
    int mode=int(beekapoo::titleCapitalization(beekapoo::patchIdentity(names,{}).modules));
    if(cases[mode].empty())cases[mode]={models[i],models[j],models[k]};
  }
  const char* modes[]={"lower","upper","normal","mixed"};
  for(int example=0;example<4;++example) {
    require(!cases[example].empty(),"Real module combinations must exercise every title style");
    rack->clear();APP->history->clear();
    std::vector<app::ModuleWidget*> sources;float x=0;
    for(const auto& slug:cases[example]){auto* w=add(plugin::getModel("Fundamental",slug),Vec(x,0));sources.push_back(w);x+=w->box.size.x;}
    auto* owner=static_cast<beekapoo::Beekapoo*>(add(modelBeekapooPreview,Vec(x+90,0)));
    rack->deselectAll();for(auto* w:sources)rack->select(w,true);owner->collapse();
    owner->resetNameTopology();
    std::vector<std::vector<app::ParamWidget*>> candidates;
    for(auto* w:sources){candidates.emplace_back();for(auto* p:w->getParams())if(!dynamic_cast<app::Switch*>(p)&&p->getParamQuantity()->isBounded())candidates.back().push_back(p);}
    int wanted=3+example;
    for(size_t n=0;int(owner->parameters.size())<wanted;++n) {
      bool any=false;
      for(auto& list:candidates)if(n<list.size()&&int(owner->parameters.size())<wanted){auto* p=list[n];auto* q=p->getParamQuantity();q->setImmediateValue(q->getMinValue()+q->getRange()*float((example*3+int(owner->parameters.size())*7)%13)/12.f);owner->exposeParameter(p->module->id,p->paramId);any=true;}
      require(any||int(owner->parameters.size())==wanted,"Title sample must have enough real controls");
    }
    for(int i=0;i<1+example%3;++i)owner->expose(sources[i],sources[i]->getInputs()[0]);
    owner->expose(sources.back(),sources.back()->getOutputs()[0]);
    owner->requestedWidth=(8+example*2)*15;owner->layoutControls();owner->step();
    require(int(beekapoo::titleCapitalization(owner->appearance.modules))==example,"Rendered capitalization must come from the actual module hash");
    auto* parent=owner->parent;auto pos=owner->box.pos;parent->removeChild(owner);owner->box.pos=Vec();
    capture(owner,output+"/title-"+modes[example]+".png",scale,false);
    owner->box.pos=pos;parent->addChild(owner);
    require(owner->getModule<beekapoo::Module>()->getPatchName()==beekapoo::generated::topologyName(owner->appearance.wiring),"Generated title must come from the actual topology hash");
  }
  rack->clear();APP->history->clear();
}

// Families of three, mixed sizes, numbered controls, and module-first grouping.
template<class Add, class Require>
void renderBeekParamBlocks(const std::string& output, float scale, Add add, Require require) {
  auto* rack=APP->scene->rack;
  const int widths[]={8,12,16,10,14,18};
  for(int example=0;example<6;++example) {
    rack->clear();APP->history->clear();
    std::vector<app::ModuleWidget*> sources;
    float x=0;
    for(const char* slug:{"VCO","VCO","VCO","Delay","8vert"}) {
      auto* source=add(plugin::getModel("Fundamental",slug),Vec(x,0));
      x+=source->box.size.x;sources.push_back(source);
    }
    auto* owner=static_cast<beekapoo::Beekapoo*>(add(modelBeekapooPreview,Vec(x+90,0)));
    rack->deselectAll();for(auto* source:sources)rack->select(source,true);owner->collapse();
    owner->getModule<beekapoo::Module>()->setPatchName(beekapoo::generated::name(923+example*7919));
    auto expose=[&](int index,const std::string& name) {
      for(auto* p:sources[index]->getParams())if(p->getParamQuantity()->name==name) {
        auto* q=p->getParamQuantity();q->setImmediateValue(q->getMinValue()+q->getRange()*float((index*5+example*3)%11)/10.f);
        owner->exposeParameter(p->module->id,p->paramId);return;
      }
      require(false,"Block example must expose a real named control");
    };
    for(int i=0;i<3;++i)expose(i,"Frequency");
    expose(3,"Time");expose(3,"Mix");
    if(example>=3) {
      for(int i=0;i<3;++i)expose(i,"Pulse width");
      expose(0,"FM mode");expose(1,"FM mode");
      if(example==4)for(int i=0;i<4;++i)owner->exposeParameter(sources[4]->module->id,i);
    }
    owner->arrangeByModule=example==5;
    for(int i=0;i<3;++i)owner->expose(sources[i],sources[i]->getInputs()[0]);
    owner->expose(sources[3],sources[3]->getOutputs()[0]);
    owner->requestedWidth=widths[example]*15;owner->layoutControls();owner->step();
    for(const auto& p:owner->parameters)require(p.knob->visible,"Block preview must retain every control");
    auto* parent=owner->parent;auto pos=owner->box.pos;parent->removeChild(owner);owner->box.pos=Vec();
    capture(owner,output+string::f("/blocks-%02d.png",example+1),scale,false);
    owner->box.pos=pos;parent->addChild(owner);
  }
  rack->clear();APP->history->clear();
}

// Typical small panels: lean toward a few controls while covering the requested
// 3–10 parameters, 1–5 inputs and 1–4 outputs. Includes knobs and real switches.
template<class Add, class Require>
void renderBeekControlSpectrum(const std::string& output, float scale, Add add, Require require) {
  auto* rack=APP->scene->rack;
  const int widths[]={6,8,10,12,9,12,14,16};
  const int counts[]={3,4,5,6,3,5,8,10};
  const int inputCounts[]={1,2,3,1,4,5,2,3};
  const int outputCounts[]={1,1,2,3,2,1,4,2};
  const int buttonCounts[]={0,1,1,1,0,2,2,3};
  const std::vector<std::vector<std::string>> sets={
    {"VCO","VCO","VCF"},
    {"LFO","VCO","VCA-1"},
    {"SEQ3","VCO","VCF","ADSR"},
    {"Random","Delay","VCO","VCMixer"},
    {"8vert","VCMixer","ADSR"},
    {"VCO","VCO","VCO","LFO"},
    {"SEQ3","Delay","VCF","VCA-1"},
    {"VCO","LFO","Random","Delay","8vert"}
  };
  auto* manifest=json_array();
  for(int example=0;example<int(sets.size());++example) {
    rack->clear();APP->history->clear();
    std::vector<app::ModuleWidget*> sources;
    float x=0;
    for(const auto& slug:sets[example]) {
      auto* model=plugin::getModel("Fundamental",slug);require(model,"Spectrum source model must exist");
      auto* source=add(model,Vec(x,0));x+=source->box.size.x;sources.push_back(source);
    }
    auto* owner=static_cast<beekapoo::Beekapoo*>(add(modelBeekapooPreview,Vec(x+90,0)));
    rack->deselectAll();for(auto* source:sources)rack->select(source,true);owner->collapse();
    owner->getModule<beekapoo::Module>()->setPatchName(beekapoo::generated::name());
    std::vector<std::vector<app::ParamWidget*>> knobs(sources.size()),buttons(sources.size());
    for(size_t i=0;i<sources.size();++i)for(auto* p:sources[i]->getParams()) {
      auto* q=p->getParamQuantity();if(!q||!q->isBounded()||q->getRange()<=0)continue;
      (dynamic_cast<app::Switch*>(p)?buttons[i]:knobs[i]).push_back(p);
    }
    int wanted=counts[example];
    int wantedButtons=buttonCounts[example];
    auto expose=[&](const std::vector<std::vector<app::ParamWidget*>>& groups,int number) {
      int added=0;
      for(size_t index=0;added<number;++index) {
        bool any=false;
        for(const auto& group:groups)if(index<group.size()&&added<number) {
          auto* p=group[index];auto* q=p->getParamQuantity();
          q->setImmediateValue(q->getMinValue()+q->getRange()*float((example*5+added*11)%23)/22.f);
          owner->exposeParameter(p->module->id,p->paramId);++added;any=true;
        }
        if(!any)break;
      }
    };
    expose(buttons,wantedButtons);expose(knobs,std::max(0,wanted-wantedButtons));
    int ins=inputCounts[example],outs=outputCounts[example];
    auto exposePorts=[&](bool input,int number) {
      int added=0;
      for(size_t index=0;added<number;++index) {
        bool any=false;
        for(auto* w:sources) {
          auto ports=input?w->getInputs():w->getOutputs();
          if(index<ports.size()&&added<number){owner->expose(w,ports[index]);++added;any=true;}
        }
        require(any||added==number,"Preview must have enough distinct ports");
      }
    };
    exposePorts(true,ins);exposePorts(false,outs);
    require(owner->parameters.size()==size_t(wanted),"Preview must expose exactly its requested parameter count");
    require(owner->ports.size()==size_t(ins+outs),"Preview must expose exactly its requested port count");
    owner->requestedWidth=widths[example]*15;owner->layoutControls();owner->step();
    for(const auto& p:owner->parameters)require(p.knob->visible,"Small spectrum must keep every parameter visible");
    for(const auto& p:owner->ports) {
      require(p.face->visible,"Every requested jack must be visible");
    }
    std::string modules;for(auto* w:sources){if(!modules.empty())modules+=" / ";modules+=w->model->name;}
    std::string name=string::f("controls-%02d.png",example+1);
    auto* parent=owner->parent;auto pos=owner->box.pos;parent->removeChild(owner);owner->box.pos=Vec();capture(owner,output+"/"+name,scale,false);owner->box.pos=pos;parent->addChild(owner);
    json_array_append_new(manifest,json_pack("{s:i,s:s,s:s,s:i,s:i,s:i}","index",example+1,"image",name.c_str(),"modules",modules.c_str(),"parameters",wanted,"inputs",ins,"outputs",outs));
  }
  require(json_dump_file(manifest,(output+"/controls-manifest.json").c_str(),JSON_INDENT(2))==0,"Control spectrum manifest must save");json_decref(manifest);
  rack->clear();APP->history->clear();
}
