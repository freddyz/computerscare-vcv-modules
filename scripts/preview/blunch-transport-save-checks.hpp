#pragma once
inline void checkBlunchTransportSave(rack::plugin::Model* model) {
  using B=ComputerscareBlunch;
  // Full Module::fromJson resolves the model through Rack's plugin registry.
  auto* owner=model->plugin;
  bool addPlugin=std::find(plugin::plugins.begin(),plugin::plugins.end(),owner)==plugin::plugins.end();
  bool addModel=std::find(owner->models.begin(),owner->models.end(),model)==owner->models.end();
  if(addPlugin)plugin::plugins.push_back(owner);
  if(addModel)owner->models.push_back(model);
  struct RestoreRegistry {
    plugin::Plugin* owner;plugin::Model* model;bool pluginAdded,modelAdded;
    ~RestoreRegistry(){
      if(modelAdded)owner->models.remove(model);
      if(pluginAdded)plugin::plugins.erase(std::find(plugin::plugins.begin(),plugin::plugins.end(),owner));
    }
  } restore{owner,model,addPlugin,addModel};
  auto require=[](bool ok,const char* why){if(!ok)throw std::runtime_error(why);};
  auto make=[&](){return std::unique_ptr<B>(static_cast<B*>(model->createModule()));};
  auto m=make();
  for(int ch=0;ch<4;++ch){m->sequenceEditorStates[ch].text="120bpm";m->commitLineForChannel(ch,0,true);m->startSequencer(ch);}
  m->params[B::POLY_CHANNELS_PARAM].setValue(4);
  m->stopSequencer(1,true);m->stopSequencer(3,true);
  engine::Module::ProcessArgs args;args.sampleRate=48000;args.sampleTime=1.f/48000;args.frame=0;
  auto roundTrip=[&](){
    auto saved=std::unique_ptr<json_t,void(*)(json_t*)>(m->toJson(),json_decref);
    auto restored=make();restored->fromJson(saved.get());return restored;
  };
  auto on=roundTrip();
  for(int frame=0;frame<32;++frame){args.frame=frame;on->process(args);}
  for(int ch=0;ch<4;++ch) {
    require(on->sequencers[ch].running==(ch%2==0),"Reload with Run on preserves each channel's transport state");
    if(ch%2)require(on->outputs[B::CLOCK_OUTPUT].getVoltage(ch)==0.f,"Stopped channels emit no trigger after reload");
  }
  // Save before the DSP has consumed the Run-off edge as well as after it.
  m->params[B::RUN_PARAM].setValue(0);
  auto pending=roundTrip();pending->process(args);
  require(pending->rememberedRunning[0]&&!pending->rememberedRunning[1],"Saving a pending Run-off edge preserves channel intent");
  m->process(args);auto off=roundTrip();off->process(args);
  for(int ch=0;ch<4;++ch)require(!off->sequencers[ch].running&&off->rememberedRunning[ch]==(ch%2==0),"Reload with Run off retains the channels to resume");
  off->params[B::RUN_PARAM].setValue(1);off->process(args);
  for(int ch=0;ch<4;++ch)require(off->sequencers[ch].running==(ch%2==0),"Run resumes only the channels that were started when saved");
  auto old=std::unique_ptr<json_t,void(*)(json_t*)>(m->toJson(),json_decref);
  json_object_del(json_object_get(old.get(),"data"),"channelStarted");
  auto legacy=make();legacy->fromJson(old.get());
  for(int ch=0;ch<4;++ch)require(legacy->rememberedRunning[ch],"Patches without transport state retain legacy behavior");
  std::puts("Blunch per-channel started/stopped persistence, Run-on/off reload, pending Run edge, output silence and legacy compatibility passed");
}
