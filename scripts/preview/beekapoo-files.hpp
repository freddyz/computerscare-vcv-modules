// Isolated renderer only: never opens or replaces the user's live patch.
template <class Require>
void testBeekAudioImportDialog(const std::string& path,const std::string& output,float scale,Require require) {
  beekapoo::BeekPatchFile file(path);
  auto* scene=APP->scene;auto* events=APP->event;
  auto* previousRoot=events->rootWidget;events->rootWidget=scene;
  auto size=scene->box.size;scene->box.size=Vec(1000,650);
  widget::Widget focus;events->setSelectedWidget(&focus);
  int imports=0;bool converted=true;
  auto create=[&] {
    auto* dialog=beekapoo::makeBeekImportDialog(file,path,[&](int64_t,bool audio){++imports;converted=audio;});
    require(dialog,"Audio choices must use a modal dialog");scene->addChild(dialog);dialog->step();return dialog;
  };
  auto key=[&](int code,int mods=0) {
    require(events->handleKey(Vec(2,2),code,0,GLFW_PRESS,mods),"Modal must consume keys even outside its panel");
    events->handleKey(Vec(2,2),code,0,GLFW_RELEASE,mods);
  };
  auto click=[&](Vec pos) {events->handleButton(pos,GLFW_MOUSE_BUTTON_LEFT,GLFW_PRESS,0);events->handleButton(pos,GLFW_MOUSE_BUTTON_LEFT,GLFW_RELEASE,0);};
  auto destroy=[&](beekapoo::BeekImportDialog* dialog){scene->removeChild(dialog);delete dialog;};
  auto* dialog=create();
  require(events->getSelectedWidget()==dialog && dialog->selected==0,"Modal must own keyboard focus with Expose initially selected");
  require((dialog->panel->box.getCenter()-scene->box.size*.5f).norm()<.1f,"Import dialog must be centered in the Rack window");
  click(Vec(2,2));require(!dialog->closed && imports==0,"Clicking the dimmed backdrop must not dismiss or load");
  key(GLFW_KEY_DOWN);require(dialog->selected==1 && imports==0,"Down must select Keep without importing or scrolling Rack");
  key(GLFW_KEY_R,RACK_MOD_CTRL);require(imports==0 && events->getSelectedWidget()==dialog,"Rack shortcuts must not escape modal keyboard focus");
  click(dialog->rows[0]->getAbsoluteOffset(dialog->rows[0]->box.size*.5f));
  require(dialog->selected==0 && events->getSelectedWidget()==dialog,"Mouse selection must retain dialog keyboard focus");
  capture(scene,output+"/38-audio-import-dialog.png",scale,false);
  for(float width:{400.f,640.f,1000.f}) {
    scene->box.size=Vec(width,500);dialog->step();
    require(dialog->panel->box.pos.x>=0 && dialog->panel->box.getRight()<=width,"Modal must fit narrower Rack windows");
    require(dialog->loadButton->box.getRight()<dialog->panel->box.size.x && dialog->cancelButton->box.pos.x>=0,"Dialog buttons must remain inside its panel");
  }
  key(GLFW_KEY_DOWN);key(GLFW_KEY_ENTER);
  require(imports==1 && !converted && dialog->closed && events->getSelectedWidget()==&focus,"Enter must load exactly the chosen option and restore focus");
  dialog->submit();require(imports==1,"Repeated input must not import twice");destroy(dialog);
  dialog=create();key(GLFW_KEY_ESCAPE);require(dialog->closed && imports==1,"Escape must cancel without importing");destroy(dialog);
  dialog=create();click(dialog->cancelButton->getAbsoluteOffset(dialog->cancelButton->box.size*.5f));require(dialog->closed && imports==1,"Cancel button must not import");destroy(dialog);
  events->setSelectedWidget(nullptr);events->rootWidget=previousRoot;scene->box.size=size;
  std::puts("Beek import modal: centering, focus, native mouse/keyboard routing, blocked backdrop/shortcuts, resize, confirm and cancel passed");
}

template <class Add, class Require>
void testBeekPatchFiles(const std::string& output,float scale,Add add,Require require,plugin::Model* gainModel) {
  auto* rack=APP->scene->rack;
  pluginInstance->addModel(modelPhlooperPreview);
  struct Cleanup {~Cleanup(){pluginInstance->models.remove(modelPhlooperPreview);}} cleanup;
  using P=ComputerscarePhlooper;
  auto* sample=add(modelPhlooperPreview,Vec());auto sampleId=sample->module->id;
  auto* audio=static_cast<P*>(sample->module);
  const int frames=512;
  audio->loop.size=audio->loop.captured=frames;audio->loop.initial=false;audio->frames=frames;audio->createdLoops=3;
  audio->params[P::COUNT].setValue(3);audio->params[P::LENGTH].setValue(.75f);
  for(int voice=0;voice<16;++voice) {
    audio->loop.stereo[voice]=true;
    for(int i=0;i<frames*2;++i)audio->loop.audio[voice][i]=std::sin(float(i)*.037f)*(voice==2?.4f:.2f);
  }
  auto* gain=add(gainModel,Vec(600,0));
  auto* outside=add(gainModel,Vec(2000,0));auto outsideId=outside->module->id;
  auto* owner=static_cast<beekapoo::Beekapoo*>(add(modelBeekapooPreview,Vec(900,0)));auto ownerId=owner->module->id;
  auto wire=[&](app::ModuleWidget* from,int out,app::ModuleWidget* to,int in) {
    auto* c=new engine::Cable;c->outputModule=from->module;c->outputId=out;c->inputModule=to->module;c->inputId=in;
    APP->engine->addCable(c);auto* cw=new app::CableWidget;cw->setCable(c);cw->color=nvgRGB(50,180,120);rack->addCable(cw);
  };
  wire(sample,P::OUT_L,gain,0);wire(gain,0,outside,0);
  rack->deselectAll();rack->select(sample,true);rack->select(gain,true);owner->collapse();
  owner->exposeParameter(sampleId,P::LENGTH);owner->toggleRandomLock(sampleId,P::LENGTH);owner->requestedWidth=240;owner->layoutControls();owner->saveGroup();
  owner->expose(gain,gain->getOutput(1));wire(gain,1,owner,5);
  auto path=output+"/shared-beek.vcv";
  auto patchPath=APP->patch->path;auto historyIndex=APP->history->actionIndex;
  owner->saveBeek(path);
  require(APP->patch->path==patchPath && APP->history->actionIndex==historyIndex,"Export must leave patch path and history alone");
  {
    beekapoo::BeekPatchFile saved(path);
    require(saved.modules.size()==3 && saved.beeks.size()==1 && !saved.modules.count(outsideId),"Export must contain only the Beek and its children");
    require(json_array_size(json_object_get(saved.root,"cables"))==2,"Export must retain child and wrapper CV wiring, excluding outside cables");
    require(system::exists(system::join(saved.temporary.path,"modules",std::to_string(sampleId),"audio-index.txt")),"Export must bundle Phlooper patch storage");
    require(!json_object_get(saved.root,"path"),"Export must omit the current patch path");
  }
  auto* target=static_cast<beekapoo::Beekapoo*>(add(modelBeekapooPreview,Vec(2600,0)));
  auto* previous=add(gainModel,Vec(3000,0));auto previousId=previous->module->id;
  rack->deselectAll();rack->select(previous,true);target->collapse();
  auto before=APP->history->actionIndex;
  require(target->loadBeek(path),"Exported Beek must import into an existing instance");
  require(APP->history->actionIndex==before+1 && target->members.size()==2 && !rack->getModule(previousId),"Import must replace contents as one Undo action");
  auto loadedAudio=[&](beekapoo::Beekapoo* host) -> P* {
    for(auto& member:host->members)if(auto* m=dynamic_cast<P*>(APP->engine->getModule(member.id)))return m;
    return nullptr;
  };
  auto checkAudio=[&](P* m) {
    require(m && m->loop.size==frames,"Imported Phlooper must read assets during onAdd");
    for(int voice:{0,2,15})for(int i:{0,39,321,900})
      require(std::abs(m->loop.audio[voice][i]-std::sin(float(i)*.037f)*(voice==2?.4f:.2f))<.00001f,"Shared WAV samples must preserve recorded audio");
    require(std::abs(m->params[P::LENGTH].getValue()-.75f)<.00001f,"Shared Phlooper controls must be restored");
  };
  auto* imported=loadedAudio(target);checkAudio(imported);
  require(imported->id!=sampleId && target->randomLocks.count({imported->id,P::LENGTH}),"Imports must use fresh IDs and remap locks");
  auto importedId=imported->id;
  APP->history->undo();target->step();
  require(target->members.size()==1 && target->contains(APP->engine->getModule(previousId)) && !APP->engine->getModule(importedId),"Undo must restore the previous group");
  APP->history->redo();target->step();checkAudio(loadedAudio(target));
  require(!APP->engine->getModule(previousId),"Redo must remove the former group again");
  // Exercise Rack's own loader, not Beek's importer.
  APP->patch->load(path);
  auto* opened=dynamic_cast<beekapoo::Beekapoo*>(rack->getModule(ownerId));
  require(opened,"Rack's native loader must open the exported Beek");opened->step();
  require(opened->members.size()==2 && APP->engine->getModuleIds().size()==3,"Native opening must restore exactly the exported subpatch");checkAudio(loadedAudio(opened));
  auto* secondChild=add(gainModel,Vec(1200,0));auto secondChildId=secondChild->module->id;
  auto* second=static_cast<beekapoo::Beekapoo*>(add(modelBeekapooPreview,Vec(1500,0)));auto secondId=second->module->id;
  rack->deselectAll();rack->select(secondChild,true);second->collapse();
  auto multiple=output+"/two-beeks.vcv";APP->patch->save(multiple);
  auto* receiver=static_cast<beekapoo::Beekapoo*>(add(modelBeekapooPreview,Vec(1800,0)));
  bool choose=false;try{receiver->loadBeek(multiple);}catch(const Exception&){choose=true;}
  require(choose && receiver->members.empty(),"Multi-Beek files must require a choice before changing the target");
  require(receiver->loadBeek(multiple,secondId) && receiver->members.size()==1 && !loadedAudio(receiver),"Choosing a Beek must import only its own children");
  {
    beekapoo::BeekPatchFile file(multiple);
    int64_t chosen=-1;
    auto* dialog=beekapoo::makeBeekImportDialog(file,multiple,[&](int64_t id,bool){chosen=id;});
    require(dialog&&dialog->choices.size()==3&&dialog->choices.back().title=="Entire patch","Every multi-Beek import must offer the entire patch");
    auto* scene=APP->scene;auto size=scene->box.size;scene->box.size=Vec(900,600);
    scene->addChild(dialog);dialog->step();dialog->choose(2);
    capture(scene,output+"/entire-patch-import.png",scale,false);
    dialog->submit();require(chosen==beekapoo::beekEntirePatch,"Entire patch choice must bypass automatic single-Beek selection");
    scene->removeChild(dialog);delete dialog;scene->box.size=size;
    // The original fixture has a CV connection to the wrapper's own actions.
    // Refuse unsupported control semantics before modifying the destination.
    auto before=receiver->toJson();auto history=APP->history->actionIndex;
    bool refused=false;try{receiver->loadBeek(multiple,beekapoo::beekEntirePatch);}catch(const Exception&){refused=true;}
    auto after=receiver->toJson();require(refused&&json_equal(before,after)&&APP->history->actionIndex==history,"Unsupported wrapper action wiring must not silently disappear or partially import");json_decref(before);json_decref(after);
    auto* wires=json_object_get(file.root,"cables");
    for(size_t i=json_array_size(wires);i-- >0;) {
      auto* c=json_array_get(wires,i);
      if(json_integer_value(json_object_get(c,"inputModuleId"))==ownerId&&json_integer_value(json_object_get(c,"inputId"))==5)json_array_remove(wires,i);
    }
    int64_t looseId=0;while(file.modules.count(looseId))++looseId;
    auto* loose=json_deep_copy(file.modules.at(secondChildId));json_object_set_new(loose,"id",json_integer(looseId));
    json_array_append_new(json_object_get(file.root,"modules"),loose);
    auto* firstGroup=json_object_get(json_object_get(file.modules.at(ownerId),"data"),"group");
    auto* secondGroup=json_object_get(json_object_get(file.modules.at(secondId),"data"),"group");
    int64_t firstGain=-1;
    auto* children=json_object_get(firstGroup,"members");
    for(size_t i=0;i<json_array_size(children);++i){auto id=json_integer_value(json_object_get(json_array_get(children,i),"id"));if(id!=sampleId)firstGain=id;}
    auto port=[&](json_t* group,int64_t child,int input,float gain,bool shared) {
      auto* p=json_pack("{s:I,s:i,s:b,s:i,s:f}","module",json_int_t(child),"port",input,"input",true,"route",0,"gain",double(gain));
      if(shared){json_object_set_new(p,"shared",json_integer(731));json_object_set_new(p,"targets",json_pack("[{s:I,s:i,s:I}]","module",json_int_t(child),"port",1,"bridge",json_int_t(-1)));}
      json_array_append_new(json_object_get(group,"ports"),p);
    };
    port(firstGroup,firstGain,2,.5f,false);port(secondGroup,secondChildId,0,-.5f,true);
    auto cable=[&](int64_t from,int out,int64_t to,int in) {
      json_array_append_new(wires,json_pack("{s:I,s:I,s:i,s:I,s:i,s:s}","id",json_int_t(900000+json_array_size(wires)),"outputModuleId",json_int_t(from),"outputId",out,"inputModuleId",json_int_t(to),"inputId",in,"color","#30a070"));
    };
    cable(looseId,0,ownerId,beekapoo::Module::inputBase);cable(looseId,0,ownerId,beekapoo::Module::inputBase);
    cable(secondChildId,0,ownerId,beekapoo::Module::inputBase);
    cable(firstGain,1,secondId,beekapoo::Module::inputBase);cable(secondChildId,0,secondId,beekapoo::Module::inputBase);
    cable(ownerId,0,firstGain,2);cable(secondId,0,secondChildId,0);cable(secondId,0,secondChildId,1);
    json_dump_file(file.root,system::join(file.temporary.path,"patch.json").c_str(),JSON_INDENT(2));
    auto whole=output+"/whole-patch.vcv";system::archiveDirectory(whole,file.temporary.path,1);
    auto oldChild=receiver->members.front().id;history=APP->history->actionIndex;
    require(receiver->loadBeek(whole,beekapoo::beekEntirePatch),"Entire patch must combine wrappers and loose modules");
    auto verify=[&] {
      for(int tick=0;tick<3;++tick)rack->step();
      require(receiver->members.size()==4,"Entire patch must import every child and loose module exactly once");checkAudio(loadedAudio(receiver));
      for(const auto& member:receiver->members)require(!dynamic_cast<beekapoo::Module*>(APP->engine->getModule(member.id)),"Whole patch must not create nested Beek wrappers");
      int routes=0;
      for(const auto& p:receiver->ports)if(p.route>=0) {
        ++routes;int incoming=0;
        for(auto* c:rack->getCompleteCables())if(c->cable->inputModule==receiver->module&&c->cable->inputId==beekapoo::Module::inputBase+p.route)++incoming;
        bool shared=p.shared.key>=0;
        require(incoming==(shared?2:3),"Whole-patch import must preserve duplicate stacks, self-patches and connections between Beeks");
        require(std::abs(receiver->inputGain(p)-(shared?-.5f:.5f))<.0001f,"Merged legacy input routes must retain independent gains");
        require(!shared||p.shared.extra.size()==1,"Shared input fanout must survive the merge");
      }
      require(routes==2,"Equal route numbers in separate Beeks must become distinct routes");
      checkBeekCablePresentation(require);
    };
    verify();require(APP->history->actionIndex==history+1,"Entire patch replacement must be one Undo action");
    APP->history->undo();receiver->step();require(receiver->members.size()==1&&receiver->members.front().id==oldChild,"Undo must restore the previous Beek");
    APP->history->redo();receiver->step();verify();
    auto roundtrip=output+"/whole-patch-roundtrip.vcv";receiver->saveBeek(roundtrip);require(receiver->loadBeek(roundtrip),"Combined patch must save and reload");verify();
    beekapoo::BeekPatchFile single(roundtrip);std::unique_ptr<beekapoo::BeekImportDialog> singleDialog(beekapoo::makeBeekImportDialog(single,roundtrip,[](int64_t,bool){}));
    require(singleDialog&&singleDialog->choices.size()==2&&singleDialog->choices.back().owner==beekapoo::beekEntirePatch,"Single-Beek files must also offer Entire patch");
  }
  rack->deselectAll();rack->select(second,true);
  auto selection=output+"/shared-beek.vcvs";rack->saveSelection(selection);
  require(receiver->loadBeek(selection) && receiver->members.size()==1,"Load Beek must accept native selections");
  require(receiver->loadBeek(selection,beekapoo::beekEntirePatch) && receiver->members.size()==1,"Entire patch must also load native selections with embedded Beek contents");
  {
    beekapoo::BeekPatchFile valid(multiple);
    auto* root=json_deep_copy(valid.root);auto* modules=json_object_get(root,"modules");
    for(size_t i=0;i<json_array_size(modules);++i) {
      auto* node=json_array_get(modules,i);
      if(json_integer_value(json_object_get(node,"id"))==secondChildId)json_object_set_new(node,"plugin",json_string("not-installed"));
    }
    auto missing=output+"/missing-plugin.vcvs";json_dump_file(root,missing.c_str(),JSON_INDENT(2));json_decref(root);
    auto ids=APP->engine->getModuleIds();auto index=APP->history->actionIndex;bool failed=false;
    try{receiver->loadBeek(missing,secondId);}catch(const Exception&){failed=true;}
    require(failed && APP->engine->getModuleIds()==ids && APP->history->actionIndex==index,"Missing plugins must fail before changing the rack or history");
  }
  {
    // An ordinary stereo patch, with a Core Audio descriptor removed before
    // dependency loading. No audio backend is created in this test.
    auto* root=json_object();auto* nodes=json_array();auto* cables=json_array();
    json_object_set_new(root,"version",json_string(APP_VERSION.c_str()));
    json_object_set_new(root,"modules",nodes);json_object_set_new(root,"cables",cables);
    json_array_append_new(nodes,secondChild->toJson());
    const json_int_t audioId=17;
    json_array_append_new(nodes,json_pack("{s:I,s:s,s:s}","id",audioId,"plugin","Core","model","AudioInterface2"));
    for(int channel=0;channel<2;++channel)
      json_array_append_new(cables,json_pack("{s:i,s:I,s:i,s:I,s:i}","id",100+channel,"inputModuleId",audioId,"inputId",channel,"outputModuleId",json_int_t(secondChildId),"outputId",channel));
    beekapoo::BeekTemporaryDirectory raw;
    json_dump_file(root,system::join(raw.path,"patch.json").c_str(),JSON_INDENT(2));
    auto stereo=output+"/ordinary-stereo.vcv";system::archiveDirectory(stereo,raw.path);
    require(receiver->loadBeek(stereo,-1,true) && receiver->members.size()==1 && receiver->ports.size()==2,"Ordinary patches must support exposing stereo outputs without opening an audio device");
    require(receiver->ports[0].portId!=receiver->ports[1].portId && !receiver->ports[0].input && !receiver->ports[1].input,"Stereo conversion must expose the original left/right source ports");
    testBeekAudioImportDialog(stereo,output,scale,require);
    auto ids=APP->engine->getModuleIds();
    json_object_set_new(json_array_get(cables,1),"inputId",json_integer(2));
    json_dump_file(root,system::join(raw.path,"patch.json").c_str(),JSON_INDENT(2));system::archiveDirectory(stereo,raw.path);
    bool refused=false;try{receiver->loadBeek(stereo,-1,true);}catch(const Exception&){refused=true;}
    require(refused && APP->engine->getModuleIds()==ids,"Multichannel Audio conversion must fail before touching the target");
    {beekapoo::BeekPatchFile invalid(stereo);std::unique_ptr<beekapoo::BeekImportDialog> choice(beekapoo::makeBeekImportDialog(invalid,stereo,[](int64_t,bool){}));require(choice && choice->choices[0].title=="Expose" && choice->choices[1].title=="Keep","Audio import must offer only Expose and Keep");}
    json_array_remove(nodes,1);json_array_clear(cables);
    auto plain=output+"/ordinary-selection.vcvs";json_dump_file(root,plain.c_str(),JSON_INDENT(2));json_decref(root);
    require(receiver->loadBeek(plain) && receiver->members.size()==1 && receiver->ports.size()==48,"Ordinary selections must wrap modules and expose unpatched ports");
  }
  auto* menu=createMenu();receiver->appendContextMenu(menu);menu->step();
  bool save=false,load=false;for(auto* child:menu->children)if(auto* item=dynamic_cast<ui::MenuItem*>(child)){save|=item->text=="Save Beek…";load|=item->text=="Load Beek…";}
  require(save && load,"Context menu must provide Save and Load Beek");
  auto* overlay=menu->parent;overlay->removeChild(menu);menu->box.pos=Vec();
  capture(menu,output+"/37-save-load-beek.png",scale,false);
  overlay->addChild(menu);APP->scene->removeChild(overlay);delete overlay;
  rack->clear();
  std::puts("Save/Load Beek: standard .vcv, Phlooper WAV assets, native Open, ID isolation, atomic Undo/Redo, multi-Beek choice, .vcvs and missing plugin checks passed");
}
