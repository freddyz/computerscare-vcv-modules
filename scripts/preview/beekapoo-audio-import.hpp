namespace beekapoofixture {
// Format-compatible endpoints for Keep tests, without opening audio devices.
struct AudioDevice : engine::Module {AudioDevice(){config(0,64,64,0);}};
struct AudioDeviceFace : app::ModuleWidget {
  AudioDeviceFace(AudioDevice* m){setModule(m);box.size=Vec(90,380);for(int i=0;i<64;++i){addInput(createInputCentered<JackSmallPointingDown>(Vec(20,20+i*5),m,i));addOutput(createOutputCentered<JackSmallPointingUp>(Vec(60,20+i*5),m,i));}}
};
}
template<class Add,class Require>
void testBeekAllAudioInterfaces(const std::string& output,float scale,Add add,Require require,plugin::Model* gainModel) {
  auto* rack=APP->scene->rack;
  plugin::Plugin core,stoer;core.slug="Core";stoer.slug="Stoermelder-P1";
  std::vector<plugin::Model*> hardware;
  for(const char* slug:{"AudioInterface2","AudioInterface","AudioInterface16"}){auto* m=createModel<beekapoofixture::AudioDevice,beekapoofixture::AudioDeviceFace>(slug);core.addModel(m);hardware.push_back(m);}
  auto* sm=createModel<beekapoofixture::AudioDevice,beekapoofixture::AudioDeviceFace>("AudioInterface64");stoer.addModel(sm);hardware.push_back(sm);
  plugin::plugins.push_back(&core);plugin::plugins.push_back(&stoer);
  DEFER({rack->clear();plugin::plugins.erase(std::find(plugin::plugins.begin(),plugin::plugins.end(),&core));plugin::plugins.erase(std::find(plugin::plugins.begin(),plugin::plugins.end(),&stoer));core.models.clear();stoer.models.clear();for(auto* m:hardware)delete m;});
  auto* seed=add(gainModel,Vec());
  auto* target=static_cast<beekapoo::Beekapoo*>(add(modelBeekapooPreview,Vec(150,0)));
  auto* sink=add(gainModel,Vec(900,0));
  auto wire=[&](app::ModuleWidget* from,int out,app::ModuleWidget* to,int in){auto* c=new engine::Cable;c->outputModule=from->module;c->outputId=out;c->inputModule=to->module;c->inputId=in;APP->engine->addCable(c);auto* cw=new app::CableWidget;cw->setCable(c);rack->addCable(cw);return cw;};
  auto gain=[&](json_int_t id,float level,int x,int y){auto* n=seed->toJson();json_object_set_new(n,"id",json_integer(id));json_object_set_new(n,"pos",json_pack("[i,i]",x,y));auto* ps=json_object_get(n,"params");for(size_t i=0;i<json_array_size(ps);++i)if(json_integer_value(json_object_get(json_array_get(ps,i),"id"))==0)json_object_set_new(json_array_get(ps,i),"value",json_real(level));return n;};
  auto device=[](json_int_t id,const char* plugin,const char* model,int x,int y){auto* n=json_pack("{s:I,s:s,s:s}","id",id,"plugin",plugin,"model",model);json_object_set_new(n,"pos",json_pack("[i,i]",x,y));return n;};
  auto cable=[](json_int_t id,json_int_t from,int out,json_int_t to,int in){return json_pack("{s:I,s:I,s:i,s:I,s:i}","id",id,"outputModuleId",from,"outputId",out,"inputModuleId",to,"inputId",in);};
  auto root=[](){return json_pack("{s:[],s:[]}","modules","cables");};
  auto label=[](const beekapoo::Beekapoo::Port& p){return p.face->source->getPortInfo()->getName();};
  for(int family=0;family<4;++family)for(int count:{1,2}) {
    int cap=family==0?2:family==1?8:family==2?16:64;
    const char* model=family==0?"AudioInterface2":family==1?"AudioInterface":family==2?"AudioInterface16":"AudioInterface64";
    const char* plugin=family==3?"Stoermelder-P1":"Core";
    auto* r=root();auto* nodes=json_object_get(r,"modules");auto* cables=json_object_get(r,"cables");
    json_array_append_new(nodes,gain(1001,1.f,0,0));json_array_append_new(nodes,device(17,plugin,model,0,1));
    // Two hardware channels fed from the same signal share its native jack.
    for(int i=0;i<count;++i)json_array_append_new(cables,cable(100+i,1001,0,17,i?cap-1:0));
    auto path=output+"/audio-family-"+std::to_string(family)+"-"+std::to_string(count)+".vcvs";json_dump_file(r,path.c_str(),0);json_decref(r);
    require(target->loadBeek(path,-1,false),"Keep must import each supported Audio model");
    bool kept=false;for(auto& member:target->members)if(auto* m=APP->engine->getModule(member.id))kept|=m->model->slug==model && m->model->plugin->slug==plugin;
    require(kept && target->ports.empty(),"Keep must retain hardware modules without replacing/exposing them");
    require(target->loadBeek(path,-1,true) && target->ports.size()==1,"Expose must support mono/stereo on Core 2/8/16 and stoermelder 64");
    int outputIndex=0;for(auto& p:target->ports)wire(target->find(p.id),p.portId,sink,outputIndex++);
    APP->engine->stepBlock(64);
    for(auto& port:target->ports){auto* m=APP->engine->getModule(port.id);require(!port.input && m->model==gainModel,"Imported outputs must expose the actual source, without any relay");require(m->outputs[port.portId].getChannels()==4 && std::abs(m->outputs[port.portId].getVoltage(3)-4.f)<.0001f,"Output conversion must preserve polyphonic audio");}
    require(target->members.size()==1,"A simple mono/stereo import adds no helper modules; duplicate sources share one jack");
  }
  {
    plugin::plugins.erase(std::find(plugin::plugins.begin(),plugin::plugins.end(),&stoer));
    DEFER({plugin::plugins.push_back(&stoer);});
    auto path=output+"/audio-family-3-2.vcvs";auto ids=APP->engine->getModuleIds();bool missing=false;
    try{target->loadBeek(path,-1,false);}catch(const Exception&){missing=true;}
    require(missing && APP->engine->getModuleIds()==ids,"Keep must report missing stoermelder without touching the patch");
    require(target->loadBeek(path,-1,true),"Expose must not require the removed stoermelder plugin to be installed");
  }
  auto* r=root();auto* nodes=json_object_get(r,"modules");auto* cables=json_object_get(r,"cables");
  // Reverse file order intentionally; interface order must follow rack layout.
  json_array_append_new(nodes,device(18,"Stoermelder-P1","AudioInterface64",20,1));json_array_append_new(nodes,gain(1002,2.f,6,0));
  json_array_append_new(nodes,device(17,"Core","AudioInterface",0,1));json_array_append_new(nodes,gain(1001,1.f,0,0));
  json_array_append_new(cables,cable(201,17,3,18,63));
  json_array_append_new(cables,cable(202,1001,0,18,8));
  json_array_append_new(cables,cable(203,1002,0,17,1));json_array_append_new(cables,cable(204,1001,0,17,1));
  json_array_append_new(cables,cable(205,17,3,1001,2));json_array_append_new(cables,cable(206,17,3,1002,3));
  json_array_append_new(cables,cable(207,18,7,1001,4));
  auto mixed=output+"/mixed-audio-interfaces.vcvs";json_dump_file(r,mixed.c_str(),0);json_decref(r);
  require(target->loadBeek(mixed,-1,true) && target->members.size()==3 && target->ports.size()==5,"Multiple mixed Audio interfaces must expose every used input/output");
  std::vector<std::string> ins,outs;
  for(auto& p:target->ports)(p.input?ins:outs).push_back(label(p));
  require(ins.size()==2 && ins[0].find("Audio 8 #1 input 4")!=std::string::npos && ins[1].find("Audio 64 #2 input 8")!=std::string::npos,"Input jacks must be ordered by interface then channel");
  require(outs.size()==3,"Every distinct playback signal must remain exposed");
  auto* driver1=add(gainModel,Vec(1200,0));auto* driver2=add(gainModel,Vec(1290,0));driver1->module->params[0].setValue(3.f);driver2->module->params[0].setValue(4.f);
  int incoming=0;app::CableWidget* connection=nullptr;
  for(auto& p:target->ports)if(p.input){auto* cw=([&]{auto* endpoint=target->findPort(p);return wire(incoming++?driver2:driver1,0,endpoint->getAncestorOfType<app::ModuleWidget>(),endpoint->portId);}());if(!connection)connection=cw;}
  int outputIndex=0;for(auto& p:target->ports)if(!p.input)wire(target->find(p.id),p.portId,sink,outputIndex++);
  APP->engine->stepBlock(64);
  for(auto& member:target->members)if(auto* m=APP->engine->getModule(member.id))if(m->model==gainModel){int port=m->params[0].getValue()==1.f?2:3;require(m->inputs[port].getChannels()==4 && m->inputs[port].getVoltage(3)==12.f,"Hardware input fan-out must preserve every destination and poly channel");if(port==2)require(m->inputs[4].getVoltage(3)==16.f,"The second interface input must remain separate");}
  for(auto& p:target->ports)if(!p.input){auto* m=APP->engine->getModule(p.id);float expected=m->model==gainModel?4.f:12.f;require(std::abs(m->outputs[p.portId].getVoltage(3)-expected)<.0001f,"Output sums and connections between interfaces must be preserved");}
  rack->removeCable(connection);delete connection;APP->engine->stepBlock(64);
  for(auto& p:target->ports)if(!p.input && APP->engine->getModule(p.id)->model->slug=="8vert" && p.portId==1)require(APP->engine->getModule(p.id)->outputs[p.portId].getVoltage()==0.f,"Unpatching an imported input must silence its routes");
  target->requestedWidth=360;target->layoutControls();target->step();
  auto* parent=target->parent;auto pos=target->box.pos;parent->removeChild(target);target->box.pos=Vec();capture(target,output+"/39-all-audio-ports.png",scale,false);target->box.pos=pos;parent->addChild(target);
  auto saved=output+"/converted-audio-beek.vcv";target->saveBeek(saved);require(target->loadBeek(saved),"Converted routing must survive Save/Load Beek");
  std::vector<std::string> loaded;for(auto& p:target->ports)loaded.push_back(label(p));
  std::vector<std::string> expected={outs[0],outs[1],outs[2],ins[0],ins[1]};require(loaded==expected,"Converted port names and interface/channel ordering must survive reload");
  APP->history->undo();target->step();require(target->ports.size()==5,"Converted import Undo must restore every exposed port");APP->history->redo();target->step();require(target->ports.size()==5,"Converted import Redo must restore every exposed port");
  r=root();nodes=json_object_get(r,"modules");cables=json_object_get(r,"cables");
  json_array_append_new(nodes,gain(1001,1.f,0,0));json_array_append_new(nodes,device(17,"Stoermelder-P1","AudioInterface64",0,1));
  for(int channel=0;channel<64;++channel){json_array_append_new(cables,cable(100+channel,1001,0,17,channel));json_array_append_new(cables,cable(200+channel,17,channel,1001,2));}
  auto all=output+"/all-64-audio-channels.vcvs";json_dump_file(r,all.c_str(),0);json_decref(r);
  require(target->loadBeek(all,-1,true) && target->ports.size()==65,"All 64 capture and 64 playback channels must be exposable together");
  int inputs=0,outputs=0;engine::Module* routing=nullptr;for(auto& p:target->ports){p.input?++inputs:++outputs;routing=APP->engine->getModule(p.id);}
  require(inputs==64 && outputs==1 && routing,"Full AUDIO-64 conversion must preserve both directions");
  for(const auto& member:target->members) require(APP->engine->getModule(member.id)->model!=modelComputerscareBeekapooAudioPorts,"New imports must never create the legacy audio relay");
  std::puts("Audio import: Core 2/8/16 and stoermelder 64, Keep/Expose, mono/stereo, duplicate sources, mixed interfaces, channel order, input fan-out, output sums, inter-interface routing, polyphony, unpatch silence and Save/Load/Undo passed");
}
