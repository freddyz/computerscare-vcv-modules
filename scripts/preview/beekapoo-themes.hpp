#pragma once

template<class Add, class Require>
void renderBeekThemes(const std::string& output,float scale,Add add,Require require) {
  auto* rack=APP->scene->rack;
  auto& prefs=beekapoo::preferences();
  struct Restore {
    beekapoo::Preferences& prefs;
    int choice,saved;
    bool rackDark;
    std::string path;
    ~Restore(){prefs.defaultTheme=choice;prefs.savedDefaultTheme=saved;prefs.path=path;settings::preferDarkPanels=rackDark;}
  } restore{prefs,prefs.defaultTheme,prefs.savedDefaultTheme,settings::preferDarkPanels,prefs.path};
  prefs.path=output+"/runtime/theme-preferences.json";
  for(int local=0;local<3;++local)for(int shared=0;shared<3;++shared)for(bool dark:{false,true})
    require(beekapoo::resolveDarkTheme(local,shared,dark)==(local?local==2:shared?shared==2:dark),"Theme precedence must be local, shared default, then Rack");
  require(!beekapoo::resolveDarkTheme(-1,99,false),"Invalid themes must fall back to Rack");
  {
    beekapoo::Module saved,loaded;
    saved.panelTheme.store(2);auto* data=saved.dataToJson();loaded.dataFromJson(data);
    require(loaded.panelTheme.load()==2,"Explicit theme must survive patch/preset serialization");
    json_object_del(data,"panelTheme");loaded.dataFromJson(data);
    require(loaded.panelTheme.load()==0,"Legacy patches must follow the default theme");
    json_decref(data);
  }
  for(int sample=0;sample<3;++sample) {
    rack->clear();APP->history->clear();
    std::vector<app::ModuleWidget*> sources;float x=0;
    const std::vector<std::vector<std::string>> sets={{"VCO","VCO","VCF"},{"VCO","LFO","SEQ3"},{"VCF","Delay","ADSR","SEQ3"}};
    for(const auto& slug:sets[sample]){auto* w=add(plugin::getModel("Fundamental",slug),Vec(x,0));sources.push_back(w);x+=w->box.size.x;}
    auto* owner=static_cast<beekapoo::Beekapoo*>(add(modelBeekapooPreview,Vec(x+60,0)));
    rack->deselectAll();for(auto* w:sources)rack->select(w,true);owner->collapse();
    owner->getModule<beekapoo::Module>()->setPatchName(beekapoo::generated::name());
    int counts[]={3,5,8};
    if(sample)for(auto* w:sources) {
      for(auto* p:w->getParams())if(auto* button=dynamic_cast<app::Switch*>(p)) {
        if(button->momentary)continue;
        p->getParamQuantity()->setImmediateValue(p->getParamQuantity()->getMaxValue());
        owner->exposeParameter(w->module->id,p->paramId);break;
      }
      if(!owner->parameters.empty())break;
    }
    for(size_t row=0;owner->parameters.size()<size_t(counts[sample]);++row) {
      bool any=false;
      for(auto* w:sources)if(row<w->getParams().size()&&owner->parameters.size()<size_t(counts[sample])) {
        auto* p=w->getParams()[row];auto* q=p->getParamQuantity();if(!q||!q->isBounded())continue;
        q->setImmediateValue(q->getMinValue()+q->getRange()*(.2f+.15f*float(owner->parameters.size()%5)));
        owner->exposeParameter(w->module->id,p->paramId);any=true;
      }
      require(any||owner->parameters.size()==size_t(counts[sample]),"Theme fixture needs sufficient parameters");
    }
    for(int i=0;i<sample+1;++i)owner->expose(sources[i],sources[i]->getInputs()[0]);
    owner->expose(sources.back(),sources.back()->getOutputs()[0]);
    if(sample==2)owner->expose(sources.front(),sources.front()->getOutputs()[0]);
    owner->requestedWidth=(sample==0?6:sample==1?10:14)*15;owner->layoutControls();
    auto* m=owner->getModule<beekapoo::Module>();
    std::vector<Rect> positions;std::vector<float> values;
    for(const auto& p:owner->parameters){positions.push_back(p.knob->box);values.push_back(p.knob->getParamQuantity()->getValue());}
    for(int choice:{1,2}) {
      m->panelTheme.store(choice);owner->step();
      require(owner->darkPanel()==(choice==2),"Panel must immediately follow explicit theme");
      for(size_t i=0;i<owner->parameters.size();++i) {
        auto* p=owner->parameters[i].knob;
        require(p->box.pos.equals(positions[i].pos)&&p->box.size.equals(positions[i].size)&&p->getParamQuantity()->getValue()==values[i],"Theme must preserve control geometry and parameter values");
        if(auto* knob=dynamic_cast<beekapoo::ParametricKnob*>(p))require(knob->dark==(choice==2),"Knob cache must change with panel theme");
      }
      auto* parent=owner->parent;auto pos=owner->box.pos;parent->removeChild(owner);owner->box.pos=Vec();
      capture(owner,output+string::f("/theme-%d-%s.png",sample+1,choice==1?"light":"dark"),scale,false);
      owner->box.pos=pos;parent->addChild(owner);
    }
    auto* other=static_cast<beekapoo::Beekapoo*>(add(modelBeekapooPreview,Vec(x+600,0)));
    auto* otherModule=other->getModule<beekapoo::Module>();otherModule->freshDefault=false;otherModule->panelTheme.store(1);
    m->panelTheme.store(1);owner->setAllThemes(2);owner->step();other->step();
    require(m->panelTheme.load()==0&&owner->darkPanel(),"All Beekapoos must reset per-instance overrides");
    require(otherModule->panelTheme.load()==0&&other->darkPanel(),"All Beekapoos must update other live instances too");
    beekapoo::Preferences reloaded(prefs.path);
    require(reloaded.defaultTheme==2&&reloaded.outsideFade==prefs.outsideFade,"Global theme must persist alongside outside fade");
    prefs.defaultTheme=0;settings::preferDarkPanels=false;owner->step();require(!owner->darkPanel(),"Default must follow Rack light");
    settings::preferDarkPanels=true;owner->step();require(owner->darkPanel(),"Default must follow Rack dark live");
    m->panelTheme.store(1);owner->step();require(!owner->darkPanel(),"Explicit light must override Rack dark");
  }
  rack->clear();APP->history->clear();
  auto* browser=static_cast<beekapoo::Beekapoo*>(modelBeekapooPreview->createModuleWidget(nullptr));
  for(int choice:{1,2}) {
    prefs.defaultTheme=choice;browser->step();
    require(browser->darkPanel()==(choice==2),"Browser must follow the shared theme");
    for(auto* child:browser->controls->children)
      if(auto* knob=dynamic_cast<beekapoo::ParametricKnob*>(child))require(knob->dark==(choice==2),"Browser knobs must follow the theme without a module");
    capture(browser,output+std::string(choice==1?"/browser-light.png":"/browser-dark.png"),scale,false);
  }
  delete browser;
  std::puts("Beekapoo themes: persistence, global/default/override, browser, geometry and parameter preservation passed");
}
