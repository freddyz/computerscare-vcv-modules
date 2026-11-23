// Isolated Rack-engine integration test, not connected to a live Rack process.
namespace beekapoofixture {
struct ProtectedQuantity : engine::ParamQuantity {
  int randomCalls=0;
  void randomize() override {++randomCalls;setImmediateValue(getMaxValue());}
};
struct Gain : engine::Module {
  void dataFromJson(json_t* data) override {
    if(json_is_true(json_object_get(data,"failLoad")))throw Exception("Fixture load failure");
  }
  int randomCalls=0,resetCalls=0,saveCalls=0;
  bool overwriteOnRandomize=false;
  void onSave(const SaveEvent&) override { ++saveCalls; }
  void onRandomize(const RandomizeEvent& e) override {++randomCalls;engine::Module::onRandomize(e);if(overwriteOnRandomize)params[0].setValue(2.f);}
  void onReset(const ResetEvent& e) override {++resetCalls;engine::Module::onReset(e);}
  Gain() {
    config(24, 24, 24, 0);
    for(int i=0;i<24;++i) {
      configParam(i,0.f,2.f,1.f,i==0?"Gain":"Gain "+std::to_string(i+1));
      configInput(i,i==0?"Signal":"Input "+std::to_string(i+1));
      configOutput(i,i==0?"Signal":"Output "+std::to_string(i+1));
    }
    configParam<ProtectedQuantity>(4,0.f,2.f,1.f,"Protected gain");
    configButton(1,"Momentary");
    configSwitch(2,0.f,1.f,0.f,"Toggle");
    configSwitch(3,0.f,3.f,0.f,"Position");
    configParam(5,0.f,4.f,2.f,"Steps")->snapEnabled=true;
  }
  void process(const ProcessArgs&) override {
    outputs[0].setChannels(4);
    for(int ch=0;ch<4;++ch)
      outputs[0].setVoltage((inputs[0].isConnected() ? inputs[0].getVoltage(ch) : (ch+1.f)) * params[0].getValue(), ch);
  }
};
struct Mapper : Gain {
  engine::ParamHandle handle;
  Mapper(){APP->engine->addParamHandle(&handle);}
  ~Mapper() override {APP->engine->removeParamHandle(&handle);}
  json_t* dataToJson() override {
    auto* data=json_object();auto* maps=json_array();
    json_array_append_new(maps,json_pack("{s:I,s:i}","moduleId",(json_int_t)handle.moduleId,"paramId",handle.paramId));
    json_object_set_new(data,"maps",maps);return data;
  }
  void dataFromJson(json_t* data) override {
    Gain::dataFromJson(data);
    auto* target=json_array_get(json_object_get(data,"maps"),0);
    APP->engine->updateParamHandle_NoLock(&handle,json_integer_value(json_object_get(target,"moduleId")),0,false);
  }
  void process(const ProcessArgs& args) override {
    if(handle.module)handle.module->params[0].setValue(params[0].getValue());
    Gain::process(args);
  }
};
struct Face : app::ModuleWidget {
  Face(Gain* m) {
    setModule(m); box.size = Vec(90,380);
    for(int i=0;i<24;++i) {
      if(i==23)addInput(createInputCentered<ThemedPJ301MPort>(Vec(25,20+i*14),m,i));
      else addInput(createInputCentered<JackLargePointingDown>(Vec(25,20+i*14),m,i));
      addOutput(createOutputCentered<JackLargePointingUp>(Vec(65,20+i*14),m,i));
    }
    addParam(createParamCentered<SmallKnob>(Vec(45,180),m,0));
    addParam(createParamCentered<SmallSnapKnob>(Vec(45,160),m,5));
    for(int i=1;i<=3;++i) {
      auto* sw=createParamCentered<app::Switch>(Vec(45,200+i*20),m,i);
      sw->box=Rect(Vec(35,200+i*20),Vec(20,20));
      sw->momentary=i==1;addParam(sw);
    }
  }
};
}
// Compatibility fixtures deliberately recreate pre-removal gain routes.
// New ordinary exposures must never allocate one.
inline engine::ParamQuantity* legacyBeekGain(beekapoo::Beekapoo* owner, beekapoo::Beekapoo::Port& p) {
  if(p.route<0) {owner->attachInput(p,0,-1,1.f);owner->saveGroup();}
  return owner->module->getParamQuantity(beekapoo::Module::INPUT_GAIN_BASE+p.route);
}
#include "beekapoo-print.hpp"
#include "beekapoo-knobs.hpp"
#include "beekapoo-buttons.hpp"
#include "beekapoo-fonts.hpp"
#include "beekapoo-files.hpp"
#include "beekapoo-audio-import.hpp"
#include "beekapoo-generator.hpp"
#include "beekapoo-combine.hpp"
#include "beekapoo-inputs.hpp"
#include "beekapoo-context.hpp"
#include "beekapoo-menus.hpp"
#include "beekapoo-compact.hpp"
void renderBeekapooPreview(plugin::Plugin& plugin,const std::string& output,float scale) {
  auto require=[](bool pass,const char* why){if(!pass)throw std::runtime_error(why);};
  testBeekMenuStartup(require);
  testBeekPortContext(require);
  if(std::getenv("BEEKAPOO_FONTS")){renderBeekFonts(output,scale,require);return;}
  testBeekKnobRendering(output,scale,require);
  if(std::getenv("BEEKAPOO_BUTTONS_ONLY")){renderBeekButtonSamples(output,scale);return;}
  if(std::getenv("BEEKAPOO_KNOBS_ONLY")){renderBeekKnobSpectrum(output,scale);return;}
  {
    auto mono=beekapoo::monochromeJack("jack-small-pointing-down");
    auto original=APP->window->loadSvg(asset::plugin(pluginInstance,"res/components/jack-small-pointing-down.svg"));
    require(mono!=original,"Grayscale jack artwork must not mutate the shared Rack asset");
    require(mono==beekapoo::monochromeJack("jack-small-pointing-down"),"Grayscale jack copies must be cached");
    for(auto* shape=mono->handle->shapes;shape;shape=shape->next)
      for(auto* paint:{&shape->fill,&shape->stroke})if(paint->type==NSVG_PAINT_COLOR){
        unsigned c=paint->color;
        require((c&255)==((c>>8)&255)&&((c>>8)&255)==((c>>16)&255),"Grayscale jack fills and strokes must be neutral");
      }
  }
  {
    using beekapoo::StyleCable;
    std::vector<std::string> types{"Maker A/VCO","Maker B/VCF","Maker A/VCO"};
    std::vector<StyleCable> cables{{0,0,1,2},{2,1,1,3}};
    auto a=beekapoo::patchIdentity(types,cables);
    auto permuted=beekapoo::patchIdentity({types[2],types[0],types[1]},{{1,0,2,2},{0,1,2,3}});
    require(a==permuted,"All appearance hashes must be independent of module/cable insertion order and IDs");
    auto single=beekapoo::patchIdentity({"Maker A/VCO","Maker B/VCF"},{});
    std::vector<std::string> many(15,"Maker A/VCO");many.push_back("Maker B/VCF");auto duplicate=beekapoo::patchIdentity(many,{});
    require(single.types==duplicate.types&&single.modules!=duplicate.modules,"Unique-type hash must collapse duplicates while counted hash retains multiplicity");
    require(single.manufacturerTypes==duplicate.manufacturerTypes&&single.manufacturers!=duplicate.manufacturers,"Manufacturer set collapses duplicate makers while counted manufacturer hash retains their module counts");
    auto otherModels=beekapoo::patchIdentity({"Maker A/LFO","Maker B/VCA"},{});
    require(single.manufacturerTypes==otherModels.manufacturerTypes&&single.manufacturers==otherModels.manufacturers,"Manufacturer hashes must ignore model names");
    auto anotherMaker=beekapoo::patchIdentity({"Maker C/VCO","Maker B/VCF"},{});
    require(single.manufacturerTypes!=anotherMaker.manufacturerTypes&&single.manufacturers!=anotherMaker.manufacturers,"Changing a manufacturer must change both manufacturer hashes");
    auto makerDescriptor=beekapoo::patchIdentity({std::string("Maker A")+'\x1f'+"VCO"+'\x1f'+"a/vco"},{});
    auto slashDescriptor=beekapoo::patchIdentity({"Maker A/VCO"},{});
    require(makerDescriptor.manufacturerTypes==slashDescriptor.manufacturerTypes,"Runtime descriptors must hash manufacturer independently of model and plugin slug");
    auto repatched=beekapoo::patchIdentity(types,{{0,0,1,1},{2,1,1,3}});
    require(a.modules==repatched.modules&&a.types==repatched.types&&a.wiring!=repatched.wiring,"Changing a cable endpoint must alter only the wiring hash");
    auto external=beekapoo::patchIdentity(types,{{-1,0,0,0,"External/MIDI",""}});
    require(external==beekapoo::patchIdentity(types,{}),"Exterior patching must not affect any appearance hash");
    auto self=beekapoo::patchIdentity({"VCO","VCO"},{{0,0,0,0},{1,0,1,0}});
    auto cycle=beekapoo::patchIdentity({"VCO","VCO"},{{0,0,1,0},{1,0,0,0}});
    require(self.wiring!=cycle.wiring,"Self connections must differ from connections between identical modules");
    auto connected=beekapoo::patchIdentity({"VCO","VCO","VCO","VCO"},{{0,0,1,0},{1,0,2,0},{2,0,3,0},{3,0,0,0}});
    auto separate=beekapoo::patchIdentity({"VCO","VCO","VCO","VCO"},{{0,0,1,0},{1,0,0,0},{2,0,3,0},{3,0,2,0}});
    require(connected.wiring!=separate.wiring,"Separate identical components must differ from a single connected cycle");
    for(const auto& family:{"Frequency","Level","Attack","Resonance"}) {
      auto style=beekapoo::controlStyle(family,"VCO",a);
      auto base=beekapoo::controlStyle("Frequency","LFO",a);
      auto gray=style.color();
      require(gray.r==gray.g&&gray.g==gray.b,"Generated control skin must be grayscale");
      auto cable=beekapoo::patchCableColor(a,3);
      require(cable.r==cable.g&&cable.g==cable.b,"Generated cable colors must be grayscale");
      require(std::abs(style.hue-base.hue)<=.026f,"One subpatch must use one narrow discrete palette across all parameter families");
    }
  }
  {
    auto a=beekapoo::controlStyle("Channel 1 CV","VCO"), b=beekapoo::controlStyle("Channel 2 CV","VCO");
    auto same=beekapoo::controlStyle("Channel 1 CV","VCO"), other=beekapoo::controlStyle("Channel 1 CV","LFO");
    require(a.hue==b.hue&&a.exponent==b.exponent&&a.ripple==b.ripple&&std::abs(a.lightness-b.lightness)<.04f,"Numbered controls must share family shape and palette with only subtle shade variation");
    require(a.hue==same.hue&&a.rotation==same.rotation&&a.pointer==same.pointer,"Procedural styles must be stable for parameter/module names");
    require(std::abs(a.hue-other.hue)<.04f&&a.exponent!=other.exponent,"Other source modules must keep the family palette while changing the profile");
  }
  {
    std::array<Vec,4> corners{Vec(0,0),Vec(100,0),Vec(100,80),Vec(0,80)};
    for(unsigned profile=0;profile<5;++profile) {
      beekapoo::SectionSkin skin;skin.profile=profile;skin.seed=12345;
      auto polygons=beekapoo::sectionPolygons(corners,skin);
      require(polygons.size()==1,"Every section must be a single solid silhouette without a centre seam");
      for(const auto& polygon:polygons)for(Vec p:polygon)
        require(p.x>=0&&p.x<=100&&p.y>=0&&p.y<=80,"Section polygons must stay inside their content envelope");
    }
    auto a=beekapoo::patchIdentity({"Fundamental/VCO"},{});
    auto first=beekapoo::sectionSkin(a,2),again=beekapoo::sectionSkin(a,2);
    require(first.profile==again.profile&&first.seed==again.seed&&first.rounding==again.rounding,"Section shapes must be stable for the same structural hashes and role");
    require(first.seed!=beekapoo::sectionSkin(a,3).seed,"Section roles must have distinct derived geometry");
  }
  {
    auto measure=[](const std::string& text){return float(text.size());};
    require(beekapoo::wholeWordLines("Frequency",3,measure)==std::vector<std::string>{"Frequency"},"An oversized word must never be split into fragments");
    require(beekapoo::wholeWordLines("Cutoff frequency CV",10,measure)==std::vector<std::string>({"Cutoff","frequency","CV"}),"Labels may wrap only at whitespace");
  }
  require(beekapoo::paramFamily("Channel 1 CV")==beekapoo::paramFamily("Channel 12 CV"),"Numeric channel labels must share a family");
  require(beekapoo::paramFamily("Frequency")==beekapoo::paramFamily("frequency"),"Identical labels must group regardless of case");
  {
    std::vector<beekapoo::ParamLayoutItem> labels{{"Channel 10 CV",0},{"Frequency",1},{"Channel 2 CV",0},{"Frequency",2},{"Level",0}};
    auto groups=beekapoo::paramGroups(labels,false);
    require(groups.size()==3&&groups[0][0]==2&&groups[0][1]==0&&groups[1][0]==1&&groups[1][1]==3,"Families must remain adjacent and numbered variants sort numerically");
  }
  for(float width:{35.f,65.f,95.f,185.f,420.f})for(float height:{40.f,100.f,160.f,240.f})for(int count:{1,3,4,8,16,32}) {
    std::vector<beekapoo::ParamLayoutItem> labels;
    for(int i=0;i<count;++i)labels.push_back({i%3==0?"Frequency":"Channel "+std::to_string(i)+" CV",i/4});
    auto cells=beekapoo::layoutParams(labels,width,height,false);
    std::set<int> placed;
    for(const auto& cell:cells) {
      require(placed.insert(cell.index).second&&cell.diameter>0&&cell.diameter<=64,"Adaptive controls must be unique and within the supported size range");
      require(cell.x-cell.diameter*.5f>=-.01f&&cell.x+cell.diameter*.5f<=width+.01f&&cell.y-cell.diameter*.5f>=-.01f&&cell.y+cell.diameter*.5f<=height+.01f,"Adaptive controls must stay inside the available frame");
      for(const auto& other:cells)if(other.index!=cell.index)require(std::abs(cell.x-other.x)>=(cell.diameter+other.diameter)*.5f- .01f||std::abs(cell.y-other.y)>=(cell.diameter+other.diameter)*.5f-.01f,"Adaptive controls must not overlap");
    }
  }
  {
    std::vector<beekapoo::ParamLayoutItem> varied{{"Frequency",0,.7f},{"Pulse width",0,1.4f},{"Mix",1,.7f},{"Time",1,1.4f}};
    auto cells=beekapoo::layoutParams(varied,155,190,false);
    require(cells.size()==4,"Spacious four-control layout must retain all controls");
    for(const auto& cell:cells) require(cell.diameter>=40,"Spare space must produce usable controls rather than tiny hash-scaled controls");
    auto narrow=beekapoo::layoutParams(varied,65,138,false);
    require(narrow.size()==4,"Narrow layout must retain every parameter");
    for(const auto& cell:narrow) require(cell.diameter>=30,"Compact narrow controls must use the available area");
    std::vector<beekapoo::ParamLayoutItem> mixed{{"Frequency",0},{"Frequency",1},{"Pulse width",0},{"Level",1},{"Mix",2},{"Time",2},{"Attack",3},{"Release",3}};
    auto roomy=beekapoo::layoutParams(mixed,215,230,false);
    require(roomy.size()==mixed.size(),"Mixed groups must share rows to fit all eight controls");
    for(const auto& cell:roomy) require(cell.diameter>=32,"Roomy mixed-group layout must not shrink to mini controls");
  }
  {
    std::vector<beekapoo::ParamLayoutItem> repeated;
    for(int i=0;i<3;++i)repeated.push_back({"Frequency",i});
    for(int i=0;i<3;++i)repeated.push_back({"Pulse width",i});
    auto cells=beekapoo::layoutParams(repeated,155,270,false);
    require(cells.size()==6,"Spacious matching-family layout must show all six controls");
    for(int family=0;family<2;++family){int labels=0,block=-1;for(const auto& cell:cells)if(cell.index/3==family){if(block<0)block=cell.block;require(block==cell.block,"Matching family must remain in its own region");labels+=cell.labelWidth>0;}require(labels==1,"Each family has one shared caption");}
  }
  for(int count:{0,1,4,8,24,64,128}) for(float width:{75.f,90.f,120.f,180.f,225.f,300.f,450.f,600.f,900.f,1500.f}) {
    auto layout=beekapoo::responsiveLayout(width,count,count,count/2);
    require(layout.width>=width && layout.totalHeight()<=368.01f,"Responsive planner must preserve requested width or enforce physical minimum without hiding controls");
  }
  // Only this renderer process gets a fake watcher control path. Never send
  // commands to the user's watcher or running Rack session.
  unsetenv("VCVMON_CONTROL");installDevWatcherToolbar();
  const auto isolatedControl=output+"/isolated-watcher";
  setenv("VCVMON_CONTROL",isolatedControl.c_str(),1);
  auto findToolbar=[]() -> widget::Widget* {
    for(auto* child:APP->scene->children)for(auto* control:child->children)
      if(auto* button=dynamic_cast<ui::Button*>(control))if(button->text=="Save version")return child;
    return nullptr;
  };
  {
    auto* scene=APP->scene;APP->scene=nullptr;installDevWatcherToolbar();APP->scene=scene;
    // Service native UI events only: no module step or manual installer call.
    // An empty patch must get its toolbar after deferred plugin startup too.
    for(int i=0;i<100 && !findToolbar();++i){glfwPollEvents();system::sleep(.005);}
    auto* bar=findToolbar();
    require(bar,"Toolbar must attach after UI startup without a computerscare module callback");
    for(int i=0;i<4;++i)installDevWatcherToolbar();
    require(findToolbar()==bar,"Repeated installation must keep the same toolbar");
    std::ofstream(isolatedControl+".status")<<"paused "<<std::time(nullptr)<<'\n';
    bar->step();ui::Button* resume=nullptr;
    for(auto* control:bar->children)if(auto* button=dynamic_cast<ui::Button*>(control))if(button->text=="Resume")resume=button;
    require(resume,"Paused watcher must show Resume");
    widget::Widget::ActionEvent resumeAction;resume->onAction(resumeAction);
    std::ifstream command(isolatedControl+".command");std::string action;command>>action;
    require(action=="resume" && bar->parent==scene && bar->visible,"Resume must write only a command and leave the toolbar attached");
    command.close();system::remove(isolatedControl+".command");
    std::ofstream(isolatedControl+".status")<<"watching "<<std::time(nullptr)<<'\n';
    system::sleep(.26);bar->step();bool pause=false;
    for(auto* control:bar->children)if(auto* button=dynamic_cast<ui::Button*>(control))pause|=button->text=="Pause";
    require(pause && findToolbar()==bar,"Resume heartbeat must retain the toolbar and show Pause");
    scene->removeChild(bar);bar->hide();installDevWatcherToolbar();
    require(bar->parent==scene && bar->visible,"Detached or hidden toolbar must reattach and become visible");
    widget::Widget other;scene->removeChild(bar);other.addChild(bar);installDevWatcherToolbar();
    require(findToolbar() && findToolbar()!=bar,"A toolbar attached elsewhere must not block the current scene");
    other.removeChild(bar);delete bar;
    bar=findToolbar();scene->removeChild(bar);delete bar;installDevWatcherToolbar();
    require(findToolbar(),"Deleted toolbar must be recreated");
    std::puts("Dev toolbar late environment/UI, resume, detach, hidden, wrong-parent and deletion recovery passed");
  }
  auto* rack=APP->scene->rack;
  {
    beekapoo::OutsideFadeQuantity first,second;
    float previous=first.getValue();first.setDisplayValue(72);
    require(std::abs(second.getValue()-.72f)<.0001f,"Edit fade must be shared across menus");
    first.setDisplayValue(100);require(second.getValue()==.9f,"Edit fade must stop at 90 percent");
    first.setDisplayValue(-10);require(second.getValue()==0,"Edit fade must allow no fading");
    first.setValue(previous);
    auto path=output+"/runtime/fade-preferences-test.json";system::remove(path);
    beekapoo::Preferences fresh(path);
    require(fresh.outsideFade==.45f,"Missing preferences must default to 45 percent");
    fresh.outsideFade=.72f;fresh.save();
    beekapoo::Preferences reopened(path);
    require(std::abs(reopened.outsideFade-.72f)<.0001f,"Edit fade must survive a new Rack session");
    system::remove(path);
  }
  auto* gainModel=createModel<beekapoofixture::Gain,beekapoofixture::Face>("demo-gain");
  auto* mapperModel=createModel<beekapoofixture::Mapper,beekapoofixture::Face>("demo-mapper");
  plugin.addModel(mapperModel);
  gainModel->name="Demo gain";
  plugin.addModel(gainModel); plugin.addModel(modelBeekapooPreview); plugin.addModel(modelBeekapooLegacyPreview); plugin.addModel(modelPeekabooOldNamePreview);plugin.addModel(modelComputerscareBeekapooAudioPorts);
  require(modelBeekapooLegacyPreview->hidden,"Legacy model must stay hidden from the browser");
  plugin::plugins.push_back(&plugin);
  auto* browserWidget=modelBeekapooPreview->createModuleWidget(nullptr);
  browserWidget->step();capture(browserWidget,output+"/00-browser.png",scale,false);delete browserWidget;
  struct Cleanup {
    plugin::Plugin& plugin;plugin::Model *gain,*mapper;
    ~Cleanup() {
      APP->scene->rack->clear();
      plugin::plugins.erase(std::find(plugin::plugins.begin(),plugin::plugins.end(),&plugin));
      plugin.models.remove(gain);plugin.models.remove(mapper);plugin.models.remove(modelBeekapooPreview);plugin.models.remove(modelBeekapooLegacyPreview);plugin.models.remove(modelPeekabooOldNamePreview);plugin.models.remove(modelComputerscareBeekapooAudioPorts);delete gain;delete mapper;
    }
  } cleanup{plugin,gainModel,mapperModel};
  auto add=[&](plugin::Model* model,Vec pos){
    auto* m=model->createModule();APP->engine->addModule(m);
    auto* w=model->createModuleWidget(m);w->box.pos=pos;rack->addModule(w);return w;
  };
  testBeekGroupShutdown(add,require);
  testBeekCompact(output,scale,add,require);
  if(std::getenv("BEEKAPOO_COMPACT"))return;
  if(std::getenv("BEEKAPOO_CONTEXT_CHECKS"))return;
  if(std::getenv("BEEKAPOO_LABELS")){testBeekGenerator(output,scale,add,require);return;}
  if(std::getenv("BEEKAPOO_EXPOSURE_PATCH")||std::getenv("BEEKAPOO_EXPOSURE_STATE")||std::getenv("BEEKAPOO_SHARED_INPUTS")||std::getenv("BEEKAPOO_CABLE_STATE")||std::getenv("BEEKAPOO_MODULE_SETS")||std::getenv("BEEKAPOO_COPY_CABLES")||std::getenv("BEEKAPOO_SECTION_CONTROLS")||std::getenv("BEEKAPOO_INPUT_GAINS")||std::getenv("BEEKAPOO_REGENERATE")||std::getenv("BEEKAPOO_POLYPHONY")||std::getenv("BEEKAPOO_CHOMP")||std::getenv("BEEKAPOO_NAME_EDITOR")||std::getenv("BEEKAPOO_THEMES")||std::getenv("BEEKAPOO_TITLE_CASES")||std::getenv("BEEKAPOO_PARAM_BLOCKS")||std::getenv("BEEKAPOO_CONTROL_SPECTRUM")||std::getenv("BEEKAPOO_GENERATED_SPECTRUM")||std::getenv("BEEKAPOO_THREE_EXAMPLES")||std::getenv("BEEKAPOO_RECOVER_PATCH")){testBeekGenerator(output,scale,add,require);return;}
  if(std::getenv("BEEKAPOO_PRINT_SPECTRUM")) {
    renderBeekapooPrintSpectrum(gainModel,output,scale);
    return;
  }
  {
    auto* first=add(gainModel,Vec(0,0));auto* second=add(gainModel,Vec(90,0));auto* outside=add(gainModel,Vec(600,0));
    auto wire=[&](app::ModuleWidget* from,app::ModuleWidget* to) {
      auto* c=new engine::Cable;c->outputModule=from->module;c->outputId=0;c->inputModule=to->module;c->inputId=0;APP->engine->addCable(c);
      auto* w=new app::CableWidget;w->setCable(c);rack->addCable(w);return c->id;
    };
    auto internal=wire(first,second),boundary=wire(second,outside);
    auto firstId=first->module->id,secondId=second->module->id;
    Vec firstPosition=first->box.pos,secondPosition=second->box.pos;
    rack->select(first,true);rack->select(second,true);
    beekapoo::BeekapooRackHooks* hooks=nullptr;
    for(auto* child:APP->scene->children)if(auto* candidate=dynamic_cast<beekapoo::BeekapooRackHooks*>(child))hooks=candidate;
    require(hooks,"Selection support must remain installed before any Beekapoo instance exists");
    testBeekEditMenu(output+"/27-create-edit-menu.png",scale,false,require);
    auto* overlay=new ui::MenuOverlay;auto* menu=new ui::Menu;overlay->addChild(menu);APP->scene->addChild(overlay);
    rack->appendSelectionContextMenu(menu);hooks->step();
    beekapoo::CreateBeekapooMenuItem* create=nullptr;
    for(auto* child:menu->children)if(auto* item=dynamic_cast<beekapoo::CreateBeekapooMenuItem*>(child))create=item;
    require(create && !create->disabled,"Native multi-selection menu must offer Create Beek-A-Poo without an existing instance");
    menu->step();auto menuPosition=menu->box.pos;overlay->removeChild(menu);menu->box.pos=Vec();
    capture(menu,output+"/27-create-selection-menu.png",scale,false);menu->box.pos=menuPosition;overlay->addChild(menu);
    widget::Widget::ActionEvent action;create->onAction(action);
    APP->scene->removeChild(overlay);delete overlay;
    beekapoo::Beekapoo* group=nullptr;for(auto* w:rack->getModules())if(auto* candidate=dynamic_cast<beekapoo::Beekapoo*>(w))group=candidate;
    require(group && group->members.size()==2 && rack->isSelected(group),"Create must group exactly the selection and select its new owner");
    auto groupName=group->getModule<beekapoo::Module>()->getPatchName();
    require(!groupName.empty(),"Create from selection must assign a generated name too");
    int defaultIns=0,defaultOuts=0;
    for(const auto& p:group->ports)(p.input?defaultIns:defaultOuts)++;
    require(defaultIns>=2&&defaultIns<=3&&defaultOuts==1&&group->parameters.size()>=3&&group->parameters.size()<=5,"Create must expose a module-count-based set of inputs, outputs and parameters");
    std::set<std::pair<int64_t,int>> defaultParams;
    for(const auto& p:group->parameters){require(p.paramId!=1,"Momentary buttons should not be chosen by default");defaultParams.insert({p.id,p.paramId});}
    auto ownerId=group->module->id;
    require(APP->engine->getCable(internal) && APP->engine->getCable(boundary) && rack->getCable(boundary)->visible && !rack->getCable(internal)->visible,"Create must preserve both internal and boundary cable identities");
    for(int repeat=0;repeat<2;++repeat) {
      APP->history->undo();
      require(!rack->getModule(ownerId) && rack->getModule(firstId)->visible && rack->getModule(secondId)->visible,"One Undo must remove the owner and reveal both originals");
      require(rack->getModule(firstId)->box.pos==firstPosition && rack->getModule(secondId)->box.pos==secondPosition && rack->isSelected(rack->getModule(firstId)) && rack->isSelected(rack->getModule(secondId)),"Undo must restore the original positions and selection");
      require(beekapoo::canCreateSelectedGroup(),"Released modules must remain eligible for grouping again");
      APP->history->redo();group=dynamic_cast<beekapoo::Beekapoo*>(rack->getModule(ownerId));group->step();
      require(group->members.size()==2 && APP->engine->getCable(internal) && APP->engine->getCable(boundary),"Redo must restore the complete group without adding or losing cables");
      std::set<std::pair<int64_t,int>> redone;for(const auto& p:group->parameters)redone.insert({p.id,p.paramId});
      require(redone==defaultParams,"Redo must retain the selected defaults, not randomize them again");
      require(group->getModule<beekapoo::Module>()->getPatchName()==groupName,"Redo must retain the generated selection name");
    }
    rack->deselectAll();rack->select(group,true);rack->select(outside,true);
    require(beekapoo::canCreateSelectedGroup(),"Creation must allow combining a Beek with ordinary modules");
    testBeekEditMenu("",scale,true,require);
    group->editSubpatch();rack->select(first,true);rack->select(second,true);rack->select(group,false);rack->select(outside,false);
    require(!beekapoo::canCreateSelectedGroup(),"Creation must reject modules already owned by an editing group");
    static_cast<beekapoo::RackEditSession*>(group->editSession.get())->finish(true);
    rack->clear();
    std::puts("Create Beek-A-Poo native selection menu, one-step Undo/Redo, wiring, placement and ownership checks passed");
  }
  {
    std::vector<beekapoo::Beekapoo*> targets;
    const float widths[]={75,180,360};
    for(int i=0;i<3;++i) {
      auto* target=static_cast<beekapoo::Beekapoo*>(add(modelBeekapooPreview,Vec(i*1500+300,0)));
      if(i) {
        auto* source=add(gainModel,Vec(i*1500,0));
        rack->deselectAll();rack->select(source,true);target->collapse();
        target->exposeParameter(source->module->id,0);
        target->expose(source,source->getInput(0));target->expose(source,source->getOutput(0));
      }
      rack->deselectAll();target->requestedWidth=widths[i];target->layoutControls();
      targets.push_back(target);
    }
    auto* outside=add(gainModel,Vec(6000,0));auto outsideId=outside->module->id;
    auto refresh=[&] {for(auto* target:targets)target->step();};
    auto selectOutside=[&] {rack->deselectAll();rack->select(rack->getModule(outsideId),true);refresh();};
    selectOutside();
    for(auto* target:targets) {
      require(target->canAddSelection() && target->addSelectionButton->visible,"Every Beekapoo must offer the same unowned outside selection");
      auto* plus=target->addSelectionButton;auto face=target->frameContent(target->titleFrame);
      require(face.contains(plus->box.pos) && face.contains(plus->box.getBottomRight()),"Selection changes must position Add inside the header face");
      for(auto* control:target->managementButtons)
        require(!plus->box.intersects(control->box),"Add must not overlap Edit or Eject after selection changes");
      auto* amount=target->getParam(beekapoo::Module::WIGGLE_AMOUNT);
      require(!amount->visible || !plus->box.intersects(amount->box),"Add must not overlap Amount after selection changes");
    }
    widget::Widget sheet;float x=0;
    std::vector<Vec> positions;
    for(auto* target:targets) {
      positions.push_back(target->box.pos);rack->getModuleContainer()->removeChild(target);
      target->box.pos=Vec(x,0);sheet.addChild(target);x+=target->box.size.x+15;
    }
    sheet.box.size=Vec(x-15,380);capture(&sheet,output+"/33-add-to-any-beekapoo.png",scale,false);
    for(int i=0;i<3;++i) {sheet.removeChild(targets[i]);targets[i]->box.pos=positions[i];rack->addModule(targets[i]);}
    rack->deselectAll();refresh();
    for(auto* target:targets)require(!target->addSelectionButton->visible,"Add must disappear when the outside selection is cleared");
    for(auto* target:targets) {
      selectOutside();size_t count=target->members.size();
      target->addSelectionButton->action();refresh();
      require(target->members.size()==count+1 && target->contains(APP->engine->getModule(outsideId)),"The selected outside module must be addable to any of three Beekapoos");
      APP->history->undo();refresh();
      require(target->members.size()==count && rack->getModule(outsideId)->visible,"Undo must return the outside module for another Beekapoo to adopt");
    }
    rack->clear();
    std::puts("Three Beekapoos: selection-driven Add layout, empty/populated adoption and Undo passed");
  }
  testBeekInputRoutes(add,require);
  auto* a=add(gainModel,Vec(0,0));
  auto* sink=add(gainModel,Vec(90,0));
  auto* b=add(gainModel,Vec(180,0));
  auto* guard=add(gainModel,Vec(915,0));
  const int64_t guardId=guard->module->id;
  auto* host=static_cast<beekapoo::Beekapoo*>(add(modelBeekapooPreview,Vec(300,0)));
  const int64_t aId=a->module->id,bId=b->module->id,sinkId=sink->module->id,hostId=host->module->id;
  auto connect=[&](app::ModuleWidget* from,app::ModuleWidget* to){
    auto* c=new engine::Cable;c->outputModule=from->module;c->outputId=0;c->inputModule=to->module;c->inputId=0;
    APP->engine->addCable(c);auto* cw=new app::CableWidget;cw->setCable(c);cw->color=nvgRGB(231,183,71);rack->addCable(cw);return cw;
  };
  auto* internal=connect(a,b);auto* external=connect(b,sink);
  const int64_t internalId=internal->cable->id,externalId=external->cable->id;
  APP->engine->stepBlock(64);
  std::vector<float> reference;
  for(int ch=0;ch<4;++ch)reference.push_back(sink->module->outputs[0].getVoltage(ch));
  // These assertions track musical wiring; attenuverter bridge cables are
  // implementation-owned and are checked separately below.
  auto cableIds=[] {
    std::vector<int64_t> ids;
    for (auto* cw : APP->scene->rack->getCompleteCables()) {
      bool bridge=false;
      for(auto* w:APP->scene->rack->getModules())
        if(auto* beek=dynamic_cast<beekapoo::Beekapoo*>(w))bridge|=beek->inputBridge(cw->cable);
      if(!bridge)ids.push_back(cw->cable->id);
    }
    std::sort(ids.begin(),ids.end());return ids;
  };
  auto ids=cableIds();
  Vec originalInput=a->getInput(0)->box.pos,originalOutput=b->getOutput(0)->box.pos;
  rack->select(a,true);rack->select(b,true);host->collapse();
  require(host->members.size()==2 && !a->visible && !b->visible && sink->visible,"Selection must use membership rather than a rectangle");
  require(rack->getModule(aId)!=a && rack->getModule(aId)->getInput(0)==a->getInput(0),"Registered holder must preserve original port lookup");
  require(rack->getModule(aId)->box.size==Vec() && a->box.size==Vec(90,380),"Holder must have no footprint without resizing the real widget");
  require(!internal->visible && external->visible,"Internal cable must hide; external cable must stay visible");
  require(cableIds()==ids,"Collapse changed cable identities");
  {
    beekapoo::PortFace frontOutput;frontOutput.source=a->getOutput(0);frontOutput.input=false;
    auto click=[&](int mods=0) {
      widget::EventContext context;widget::Widget::ButtonEvent e;
      e.context=&context;e.action=GLFW_PRESS;e.button=GLFW_MOUSE_BUTTON_LEFT;e.mods=mods;
      frontOutput.onButton(e);
    };
    auto menuDelete=[&](bool expectVisible) {
      std::set<widget::Widget*> before(APP->scene->children.begin(),APP->scene->children.end());
      widget::EventContext context;widget::Widget::ButtonEvent e;e.context=&context;e.button=GLFW_MOUSE_BUTTON_RIGHT;e.action=GLFW_PRESS;
      frontOutput.onButton(e);
      beekapoo::VisiblePortDeleteItem* remove=nullptr;
      std::vector<widget::Widget*> overlays;
      for(auto* child:APP->scene->children)if(!before.count(child)) {
        overlays.push_back(child);
        for(auto* sub:child->children)if(auto* menu=dynamic_cast<ui::Menu*>(sub)) {
          for(auto* entry:menu->children)if(auto* item=dynamic_cast<beekapoo::VisiblePortDeleteItem*>(entry))remove=item;
          auto clone=menu->children.begin();std::advance(clone,2);
          require(dynamic_cast<ui::MenuItem*>(*clone)->disabled==!expectVisible,"Clone top cable must only consider visible connections");
        }
      }
      require(remove && remove->disabled==!expectVisible,"Jack menu Delete must only consider visible cables");
      widget::Widget::ActionEvent action;remove->onAction(action);
      for(auto* overlay:overlays){APP->scene->removeChild(overlay);delete overlay;}
    };
    menuDelete(false);require(cableIds()==ids,"Jack menu must not delete hidden-only wiring");
    for(int attempt=0;attempt<2;++attempt) {
      click();auto loose=rack->getIncompleteCables();
      require(loose.size()==1 && loose[0]->visible && loose[0]->outputPort==a->getOutput(0),"First exposed-output click must immediately start a visible native cable");
      require(cableIds()==ids && internal->cable && !internal->visible,"Starting a cable must not detach a hidden internal route");
      APP->event->setDraggedWidget(nullptr,GLFW_MOUSE_BUTTON_LEFT);
      require(cableIds()==ids && rack->getIncompleteCables().empty(),"Cancelling a front-panel drag must preserve all existing wiring");
    }
    click(GLFW_MOD_SHIFT);
    require(cableIds()==ids,"Shift-click must not delete a hidden internal cable");
    click();widget::Widget::DragDropEvent drop;drop.origin=a->getOutput(0);drop.button=GLFW_MOUSE_BUTTON_LEFT;
    guard->getInput(0)->onDragDrop(drop);APP->event->setDraggedWidget(nullptr,GLFW_MOUSE_BUTTON_LEFT);
    require(cableIds().size()==ids.size()+1 && internal->cable,"An exposed output must patch externally while keeping its internal route");
    menuDelete(true);require(cableIds()==ids,"Mixed jack menu must only delete its visible cable");
    APP->history->undo();require(cableIds().size()==ids.size()+1,"Undo jack-menu deletion must restore the visible cable");
    click();require(internal->cable && APP->engine->getCable(internalId),"Grabbing a visible external cable must leave the hidden route intact");
    APP->event->setDraggedWidget(nullptr,GLFW_MOUSE_BUTTON_LEFT);
    require(cableIds()==ids,"Cancelling a visible cable move must only remove that visible cable");
    beekapoo::PortFace frontInput;frontInput.source=b->getInput(0);frontInput.input=true;
    widget::EventContext context;widget::Widget::ButtonEvent inputClick;inputClick.context=&context;inputClick.button=GLFW_MOUSE_BUTTON_LEFT;inputClick.action=GLFW_PRESS;inputClick.mods=0;
    frontInput.onButton(inputClick);
    require(cableIds()==ids && rack->getIncompleteCables().size()==1,"An exposed input must not pick up its hidden internal cable");
    APP->event->setDraggedWidget(nullptr,GLFW_MOUSE_BUTTON_LEFT);
  }
  APP->engine->stepBlock(64);
  for(int ch=0;ch<4;++ch)require(sink->module->outputs[0].getVoltage(ch)==reference[ch],"Collapse changed polyphonic output");
  require(rack->requestModulePos(sink,Vec(0,0)),"Freed rack space must accept another module");
  require(rack->requestModulePos(host,Vec(315,0)),"Zero-size holders must not block container movement");
  host->expose(a,a->getInput(0));host->exposeParameter(bId,0);
  auto* mapped=dynamic_cast<app::ParamWidget*>(host->controls->children.back());
  require(mapped && mapped->getParamQuantity()==b->module->getParamQuantity(0),"Exposed parameter must target original module");
  mapped->getParamQuantity()->setImmediateValue(.5f);APP->engine->stepBlock(64);
  require(sink->module->outputs[0].getVoltage(3)==2.f,"Mapped parameter must affect original DSP");
  for(int i=1;i<24;++i) {host->expose(a,a->getInput(i));host->exposeParameter(bId,i);}
  host->expose(a,a->getInput(0));host->exposeParameter(bId,0);
  require(host->ports.size()==25 && host->parameters.size()==24,"Controls must support more than 16 ports and one parameter without duplicates");
  require(host->box.size.x>=beekapoo::Beekapoo::minimumJackWidth(30,1),"Minimum panel width must fit all jacks");
  host->requestedWidth=540;host->layoutControls();
  auto* moment=dynamic_cast<beekapoo::ExposedSwitch*>(host->parameters[1].knob);
  auto* toggle=dynamic_cast<beekapoo::ExposedSwitch*>(host->parameters[2].knob);
  auto* positions=dynamic_cast<beekapoo::ExposedSwitch*>(host->parameters[3].knob);
  require(moment && moment->momentary && toggle && !toggle->momentary && positions,"Exposed switch kinds must match original widgets");
  auto* discrete=dynamic_cast<beekapoo::MappedParam<beekapoo::ParametricKnob>*>(host->parameters[5].knob);
  require(discrete && discrete->snap && discrete->getParamQuantity()==b->module->getParamQuantity(5),"Discrete knobs must use the generated dot selector and original quantity");
  APP->event->setHoveredWidget(discrete);
  require(discrete->tip && discrete->tip->caption().find("Steps")!=std::string::npos,"Dot selector tooltip must keep source module and parameter details");
  APP->event->setHoveredWidget(nullptr);
  event::DragStart press;press.button=GLFW_MOUSE_BUTTON_LEFT;
  event::DragEnd release;release.button=GLFW_MOUSE_BUTTON_LEFT;
  moment->onDragStart(press);b->step();
  require(b->module->params[1].getValue()==1.f,"Momentary must press");
  moment->onDragEnd(release);b->step();b->step();
  require(b->module->params[1].getValue()==0.f,"Momentary must release");
  toggle->onDragStart(press);toggle->onDragEnd(release);
  require(b->module->params[2].getValue()==1.f,"Toggle must latch");
  positions->onDragStart(press);positions->onDragEnd(release);
  positions->onDragStart(press);positions->onDragEnd(release);
  require(b->module->params[3].getValue()==2.f,"Multi-position switch must cycle");
  beekapoo::PortFace* outputFace=nullptr;
  for(auto& port:host->ports)if(port.id==bId && !port.input && port.portId==0)outputFace=port.face;
  require(outputFace,"Tooltip fixture must expose the connected output");
  widget::EventContext portHoverContext;widget::Widget::HoverEvent portHover;portHover.context=&portHoverContext;portHover.pos=outputFace->box.size*.5f;
  outputFace->onHover(portHover);require(portHoverContext.target==b->getOutput(0),"Mapped jack hover must target its actual cable endpoint for native cable highlighting");
  APP->event->setHoveredWidget(portHoverContext.target);
  require(outputFace->tip,"Real source-targeted mouse hover must create the module-headed tooltip");outputFace->tip->step();
  auto hoveredCaption=outputFace->caption();
  require(outputFace->tip->text.rfind(hoveredCaption.substr(0,hoveredCaption.find('\n'))+"\n",0)==0,"Real mouse hover tooltip must put the source module above the native port details");
  auto* hoveredPort=dynamic_cast<app::PortWidget*>(APP->event->getHoveredWidget());
  require(hoveredPort && rack->getCompleteCablesOnPort(hoveredPort).size()==1,"Native hovered endpoint must expose its connected cable");APP->event->setHoveredWidget(nullptr);
  APP->event->setHoveredWidget(outputFace);require(outputFace->nativeTooltip && outputFace->tip,"Mapped port must create the native Rack tooltip");
  outputFace->tip->step();require(outputFace->tip->text.find("4: ")!=std::string::npos && outputFace->tip->text.find("V")!=std::string::npos,"Native mapped-port tooltip must report every polyphonic voltage");
  auto portCaption=outputFace->caption();
  auto moduleHeading=portCaption.substr(0,portCaption.find('\n'));
  require(outputFace->tip->text.rfind(moduleHeading+"\n",0)==0,"Mapped port tooltip must put the source module on its first line");
  b->module->outputs[0].setVoltage(3.125f,3);outputFace->tip->step();
  require(outputFace->tip->text.find("3.125V")!=std::string::npos,"Port tooltip must update live voltage values");
  APP->event->setHoveredWidget(nullptr);require(!outputFace->tip,"Native mapped-port tooltip must close on leave");APP->engine->stepBlock(64);
  widget::Widget::DragEnterEvent tipDrag;tipDrag.origin=a->getInput(1);tipDrag.button=GLFW_MOUSE_BUTTON_LEFT;
  outputFace->onDragEnter(tipDrag);require(outputFace->tip,"Drag-hover must track the native tooltip it creates");
  WeakPtr<ui::Tooltip> staleDragTip=outputFace->tip;outputFace->step();
  require(!outputFace->tip && (!staleDragTip || !staleDragTip->visible),"A tooltip must disappear after losing actual hover, even without a Leave event");
  APP->event->setHoveredWidget(outputFace);settings::tooltips=false;outputFace->step();require(!outputFace->tip,"Disabling tooltips must clear an already visible tooltip");settings::tooltips=true;APP->event->setHoveredWidget(nullptr);
  auto* nativeJack=dynamic_cast<app::SvgPort*>(a->getInput(23));require(nativeJack,"Fixture must use Fundamental's themed jack widget");
  beekapoo::PortFace separate;separate.source=nativeJack;separate.input=true;
  auto* independent=dynamic_cast<JackLargePointingDown*>(separate.artwork());
  require(independent && independent->sw->svg!=nativeJack->sw->svg,"Mapped inputs must use computerscare artwork even for Fundamental source jacks");
  require(independent->type==engine::Port::INPUT,"Mapped input artwork must have input polarity");
  separate.input=false;
  require(dynamic_cast<JackLargePointingUp*>(separate.artwork()) && separate.artwork()->type==engine::Port::OUTPUT,"Mapped outputs must use computerscare artwork with output polarity");
  Vec savedGuardPosition=guard->box.pos;
  host->step();
  auto& proxy=host->ports.front();
  Vec expected=host->controls->getRelativeOffset(proxy.face->box.getCenter(),rack);
  require((external->getOutputPos()-expected).norm()<.01f,"Boundary cable must follow container movement");
  auto checkDisconnect=[&](std::function<void()> action) {
    action();
    require(APP->engine->getCable(internalId) && !APP->engine->getCable(externalId),"Disconnect must remove boundary wiring and preserve hidden internal wiring");
    APP->history->undo();host->step();
    external=rack->getCable(externalId);
    require(cableIds()==ids,"Disconnect Undo must restore precisely the prior cables");
  };
  checkDisconnect([&]{host->disconnectPanelAction();});
  checkDisconnect([&]{
    ui::Menu menu;menu.addChild(createMenuItem("Native disconnect","Ctrl+U",[&]{host->disconnectAction();}));
    host->appendContextMenu(&menu);widget::Widget::ActionEvent action;
    for(auto* child:menu.children)if(auto* item=dynamic_cast<ui::MenuItem*>(child))if(item->rightText=="Ctrl+U"){item->onAction(action);return;}
    require(false,"Module menu must retain its native Disconnect command");
  });
  checkDisconnect([&]{
    rack->deselectAll();rack->select(host,true);
    ui::MenuOverlay overlay;auto* menu=new ui::Menu;overlay.addChild(menu);rack->appendSelectionContextMenu(menu);
    beekapoo::BeekapooRackHooks* hooks=nullptr;for(auto* child:APP->scene->children)if(auto* h=dynamic_cast<beekapoo::BeekapooRackHooks*>(child))hooks=h;
    require(hooks,"Selection hooks must exist");hooks->wrapMenus(menu);widget::Widget::ActionEvent action;
    for(auto* child:menu->children)if(auto* item=dynamic_cast<beekapoo::GroupDisconnectMenuItem*>(child)){item->onAction(action);return;}
    require(false,"Selection menu must use the same safe Disconnect policy");
  });
  checkDisconnect([&]{widget::EventContext context;widget::Widget::HoverKeyEvent e;e.context=&context;e.key=GLFW_KEY_U;e.mods=RACK_MOD_CTRL;e.action=GLFW_PRESS;host->onHoverKey(e);});
  rack->deselectAll();
  if(std::getenv("VCVMON_CONTROL")) {
    bool pause=false, save=false;
    widget::Widget* toolbar=nullptr;
    for(auto* child:APP->scene->children)for(auto* control:child->children)
      if(auto* button=dynamic_cast<ui::Button*>(control)) {
        pause |= button->text=="Pause" || button->text=="Resume";
        save |= button->text=="Save version";
        if(button->text=="Save version") toolbar=child;
      }
    require(toolbar && save,"Beekapoo's standard panel must attach the dev toolbar");
    toolbar->step();
    for(auto* control:toolbar->children)
      if(auto* button=dynamic_cast<ui::Button*>(control)) pause |= button->text=="Pause" || button->text=="Resume";
    require(pause,"Dev toolbar must include Pause/Resume");
    auto* parent=toolbar->parent;auto position=toolbar->box.pos;
    parent->removeChild(toolbar);toolbar->box.pos=Vec();
    capture(toolbar,output+"/03-dev-toolbar.png",scale,false);
    toolbar->box.pos=position;parent->addChild(toolbar);
  }
  {
    auto* own=static_cast<beekapoo::Module*>(host->module);
    auto* gainA=static_cast<beekapoofixture::Gain*>(a->module);auto* gainB=static_cast<beekapoofixture::Gain*>(b->module);
    std::vector<float> savedA,savedB;
    for(int i=0;i<24;++i){savedA.push_back(gainA->getParamQuantity(i)->getValue());savedB.push_back(gainB->getParamQuantity(i)->getValue());}
    host->parameterAction(2,true);
    require(gainB->getParamQuantity(0)->getValue()==1 && gainA->getParamQuantity(0)->getValue()==savedA[0],"Initialize visible must affect only exposed params");
    APP->history->undo();require(gainB->getParamQuantity(0)->getValue()==savedB[0],"Initialize visible undo failed");
    widget::EventContext ctx;widget::Widget::HoverKeyEvent key;key.context=&ctx;key.key=GLFW_KEY_R;key.mods=RACK_MOD_CTRL;key.action=GLFW_PRESS;host->onHoverKey(key);
    require(ctx.consumed && gainA->randomCalls==0 && gainB->randomCalls==0 && gainA->getParamQuantity(0)->getValue()==savedA[0],"Mod+R must randomize exposed params without whole-module callbacks");
    APP->history->undo();
    own->params[beekapoo::Module::WIGGLE_AMOUNT].setValue(.1f);
    host->parameterAction(0,true);
    for(int i=0;i<24;++i)require(std::abs(gainB->getParamQuantity(i)->getValue()-savedB[i])<=.20001f,"Wiggle exceeded selected amount");
    APP->history->undo();
    host->parameterAction(4,true);require(gainA->randomCalls==1 && gainB->randomCalls==1,"Randomize all must call each module's native callback");
    APP->history->undo();host->parameterAction(5,true);require(gainA->resetCalls==1 && gainB->resetCalls==1,"Initialize all must call each module's native callback");
    APP->history->undo();
    size_t historySize=APP->history->actions.size();
    engine::Module::ProcessArgs args;args.sampleRate=48000;args.sampleTime=1.f/48000;args.frame=0;
    own->inputs[1].setVoltage(5);own->process(args);host->step();
    require(APP->history->actions.size()==historySize && gainA->getParamQuantity(0)->getValue()==savedA[0],"Visible randomize CV must not fill undo history or affect hidden parameters");
    own->inputs[1].setVoltage(0);own->process(args);
    for(int index=0;index<6;++index) {
      int beforeRandom=gainA->randomCalls,beforeReset=gainA->resetCalls;
      host->actionPulse[index]=0;own->inputs[index].setVoltage(0);own->process(args);
      own->inputs[index].setVoltage(5);own->process(args);host->step();
      require(host->actionPulse[index]>0 && APP->history->actions.size()==historySize,"Every action CV must dispatch its cell without adding history");
      require(gainA->randomCalls==beforeRandom+(index==4) && gainA->resetCalls==beforeReset+(index==5),"Action CV routed to the wrong module callback");
      host->actionPulse[index]=0;own->process(args);host->step();require(host->actionPulse[index]==0,"Held action CV must not repeatedly fire");
      own->inputs[index].setVoltage(0);own->process(args);
    }

    auto* protectedQ=static_cast<beekapoofixture::ProtectedQuantity*>(gainB->getParamQuantity(4));
    protectedQ->randomizeEnabled=false;protectedQ->setImmediateValue(2.f);
    gainB->getParamQuantity(1)->setImmediateValue(1.f);
    int customRandomBefore=protectedQ->randomCalls;
    host->parameterAction(1,true);
    require(protectedQ->getValue()<2.f && protectedQ->randomCalls==customRandomBefore,"Exposed Randomize must ignore exclusion flags and custom randomize callbacks");
    require(gainB->getParamQuantity(1)->getValue()==1.f,"Exposed Randomize must leave momentary buttons untouched");
    APP->history->undo();require(protectedQ->getValue()==2.f,"Undo must restore random-excluded exposed parameters");
    bool moved=false;own->params[beekapoo::Module::WIGGLE_AMOUNT].setValue(.1f);
    for(int i=0;i<4;++i){protectedQ->setImmediateValue(1.f);host->parameterAction(0,false);moved|=protectedQ->getValue()!=1.f;}
    require(moved && gainB->getParamQuantity(1)->getValue()==1.f,"Exposed Wiggle must ignore exclusion flags and skip momentary buttons");
    protectedQ->setImmediateValue(1.f);host->parameterAction(3,false);
    require(protectedQ->getValue()==1.f,"All Wiggle must retain parameter exclusions");
    host->parameterAction(4,false);
    require(protectedQ->getValue()==1.f && protectedQ->randomCalls==customRandomBefore,"All Randomize must retain the module's exclusions");
    protectedQ->randomizeEnabled=true;
    for(int i=0;i<24;++i){gainA->getParamQuantity(i)->setImmediateValue(savedA[i]);gainB->getParamQuantity(i)->setImmediateValue(savedB[i]);}
  }
  {
    auto* gainA=static_cast<beekapoofixture::Gain*>(a->module);
    auto* gainB=static_cast<beekapoofixture::Gain*>(b->module);
    std::vector<float> savedA,savedB;
    for(int i=0;i<24;++i){savedA.push_back(gainA->params[i].getValue());savedB.push_back(gainB->params[i].getValue());}
    host->toggleRandomLock(aId,-1);host->toggleRandomLock(bId,0);
    gainB->overwriteOnRandomize=true;
    int callbacks=gainA->randomCalls;
    for(int action:{0,1,3,4}) {
      gainA->params[0].setValue(.35f);gainB->params[0].setValue(.65f);
      host->parameterAction(action,false);
      require(gainA->params[0].getValue()==.35f && gainB->params[0].getValue()==.65f,
              "Module and parameter locks must protect exposed/all randomize and wiggle, including custom callbacks");
    }
    require(gainA->randomCalls==callbacks,"A locked module's custom randomizer must not run");
    auto* own=static_cast<beekapoo::Module*>(host->module);
    engine::Module::ProcessArgs args;args.sampleRate=48000;args.sampleTime=1.f/48000;args.frame=0;
    for(int input:{0,1,3,4}) {
      own->inputs[input].setVoltage(5);own->process(args);host->step();
      require(gainB->params[0].getValue()==.65f,"Action CV must obey randomization locks");
      own->inputs[input].setVoltage(0);own->process(args);
    }
    host->parameterAction(5,false);
    require(gainA->params[0].getValue()==1.f && gainB->params[0].getValue()==1.f,"Rnd Lock must leave Initialize available");
    gainB->overwriteOnRandomize=false;
    for(int i=0;i<24;++i){gainA->params[i].setValue(savedA[i]);gainB->params[i].setValue(savedB[i]);}
    host->unlockRandomization();require(host->randomLocks.empty(),"Unlock all must clear both lock kinds");
    APP->history->undo();host->step();require(host->randomLocks.size()==2,"Unlock all must support Undo");
    APP->history->redo();host->step();require(host->randomLocks.empty(),"Unlock all must support Redo");
    APP->history->undo();host->step();
  }
  // Real Rack save/load: serialize engine state and native widget positions,
  // destroy every widget and module, then recreate them through their models.
  unsigned savedOutputShape=host->outputFrame->shapeSeed;
  json_t* patch=APP->engine->toJson();rack->mergeJson(patch);
  char* encoded=json_dumps(patch,JSON_INDENT(2));require(encoded,"Patch must serialize as valid JSON");
  std::ofstream(output+"/roundtrip.json")<<encoded;std::free(encoded);
  rack->clear();require(APP->engine->getModuleIds().empty(),"Rack clear must release held modules exactly once");
  APP->engine->fromJson(patch);rack->fromJson(patch);
  host=dynamic_cast<beekapoo::Beekapoo*>(rack->getModule(hostId));require(host,"Reload lost container");
  host->step();
  require(host->outputFrame->shapeSeed==savedOutputShape,"Frame shapes must stay stable across patch reload");
  require(host->randomLocked(aId,0) && host->randomLocks.count({bId,0}),"Randomization locks must survive native patch reload");
  require(rack->getModule(guardId)->box.pos==savedGuardPosition,"Reload shoved an unrelated module");
  a=host->find(aId);b=host->find(bId);sink=rack->getModule(sinkId);
  require(a && b && sink && host->members.size()==2 && !a->visible && !b->visible,"Saved grouping must restore on patch load");
  require(host->ports.size()==25 && host->parameters.size()==24 && host->parameters[0].id==bId && host->parameters[0].paramId==0,"Reload lost manual port or exposed parameter");
  require(rack->getModule(aId)->box.size==Vec() && rack->getModule(bId)->box.size==Vec(),"Reload restored unwanted rack footprints");
  require(!rack->getCable(internalId)->visible && rack->getCable(externalId)->visible,"Reload lost cable visibility");
  require(cableIds()==ids,"Reload changed cable identities");
  APP->engine->stepBlock(64);
  for(int ch=0;ch<4;++ch)require(sink->module->outputs[0].getVoltage(ch)==reference[ch]*.5f,"Reload changed polyphonic audio");
  APP->scene->box.size=Vec(1100,650);
  host->showView();auto* view=dynamic_cast<beekapoo::SubpatchView*>(host->subview.get());
  require(view,"Show must open a subpatch view");
  auto* originalRackParent=rack->parent;auto rackBox=rack->box;
  auto registeredModules=rack->getModules();auto selectedModules=rack->getSelected();
  auto* touchedBefore=rack->getTouchedParam();
  view->step();
  require(rack->parent==originalRackParent && rack->box.pos==rackBox.pos && rack->box.size==rackBox.size && rack->getModules()==registeredModules && rack->getSelected()==selectedModules && rack->getTouchedParam()==touchedBefore,"Subview callbacks must restore main Rack geometry, registry, selection and touched parameter");
  widget::EventContext blankContext;widget::Widget::HoverEvent blankHover;
  blankHover.context=&blankContext;blankHover.pos=Vec(-50,-50);
  view->nativeRack->onHover(blankHover);
  require(blankHover.getTarget()!=rack,"Subview background must never target the live Rack");
  require(!a->visible && !b->visible && rack->getModule(aId)->box.size==Vec(),"Show must not expand or ungroup members");
  require(view->canvas->box.size.isFinite() && view->scroll->containerBox.size.isFinite(),"Subview canvas and scroll bounds must be finite");
  auto* farFace=static_cast<beekapoo::SubpatchModuleFace*>(view->nativeRack->getModules().back());
  Vec savedSourceSize=farFace->source->box.size;
  farFace->source->box.size.x=1800;APP->scene->box.size=Vec(600,480);view->step();view->step();
  require(view->scroll->horizontalScrollbar->visible && view->scroll->verticalScrollbar->visible,"Large subpatch must scroll on both axes");
  for(auto* bar:{view->scroll->horizontalScrollbar,view->scroll->verticalScrollbar}) {
    widget::EventContext context;widget::Widget::ButtonEvent e;e.context=&context;e.action=GLFW_PRESS;e.button=GLFW_MOUSE_BUTTON_LEFT;
    e.pos=bar->box.size*.85f;bar->onButton(e);
    widget::Widget::DragMoveEvent drag;drag.mouseDelta=Vec(150,150);bar->onDragMove(drag);view->step();
    require(view->scroll->offset.isFinite() && view->scroll->container->box.pos.isFinite(),"Scrollbar click or drag made the subpatch disappear");
    auto bounds=view->scroll->getContainerOffsetBound();
    require(view->scroll->offset.x>=bounds.pos.x && view->scroll->offset.x<=bounds.getRight() && view->scroll->offset.y>=bounds.pos.y && view->scroll->offset.y<=bounds.getBottom(),"Scroll position must stay inside content bounds");
  }
  view->scroll->offset=Vec(INFINITY,INFINITY);view->step();require(view->scroll->offset.isFinite(),"Invalid scroll offsets must recover");
  farFace->source->box.size=savedSourceSize;view->scroll->offset=Vec();APP->scene->box.size=Vec(1100,650);view->step();
  require(host->members.size()==2 && cableIds()==ids,"Scrolling must not change membership or cables");

  auto* locked=b->getOutput(0);
  require(!view->connect(locked,a->getInput(1)),"Subview must reject external-jack repatching");
  view->disconnect(locked);require(rack->getCable(externalId),"Subview removed external cable");
  auto hitFor=[&](widget::Widget* source) -> beekapoo::SubpatchHit* {
    for(auto* hit:view->hits)if(hit->param.get()==source)return hit;
    throw std::runtime_error("Subview omitted a real parameter");
  };
  auto clickHit=[&](widget::Widget* source) {
    widget::Widget* target=nullptr;
    if(auto* port=dynamic_cast<app::PortWidget*>(source))target=view->portFor(port);
    else target=hitFor(source);
    require(target,"Subview omitted a real port");
    widget::EventContext ctx;widget::Widget::ButtonEvent e;
    e.context=&ctx;e.action=GLFW_PRESS;e.button=GLFW_MOUSE_BUTTON_LEFT;e.pos=target->box.size*.5f;
    target->onButton(e);return ctx.target;
  };
  view->setMode(true);
  size_t count=host->ports.size();clickHit(locked);
  require(host->ports.size()==count,"External jack exposure must stay locked");
  clickHit(a->getInput(1));require(host->ports.size()==count-1,"Expose mode must remove a jack");
  clickHit(a->getInput(1));require(host->ports.size()==count,"Expose mode must add a jack");
  count=host->parameters.size();clickHit(b->getParam(0));
  require(host->parameters.size()==count-1,"Expose mode must remove a parameter");
  clickHit(b->getParam(0));require(host->parameters.size()==count,"Expose mode must add a parameter");
  view->setMode(false);
  auto* knobHit=hitFor(b->getParam(0));
  require(clickHit(b->getParam(0))==knobHit && knobHit->dragTarget.get()==b->getParam(0),"Adjust mode must forward knob input through its zoom-aware drag target");
  view->setViewZoom(.5f,view->scroll->box.size*.5f);view->step();
  require(view->zoom->getZoom()==.5f && view->scroll->containerBox.size.isFinite(),"Subview zoom must preserve finite scroll bounds");
  Vec adjusted=knobHit->adjustedDragDelta(Vec(10,0));
  require(std::abs(adjusted.x/knobHit->dragTarget->getAbsoluteZoom()-10.f/knobHit->getAbsoluteZoom())<.001f,"Knob drag sensitivity must follow subview zoom");
  view->fitView();
  {
    settings::tooltips=true;widget::Widget::EnterEvent enter;widget::Widget::LeaveEvent leave;
    auto* portHit=view->portFor(a->getInput(0));APP->event->setHoveredWidget(portHit);require(portHit->tooltip,"Port tooltip must use original widget");
    portHit->tooltip->step();portHit->tooltipAnchor->step();
    require(portHit->tooltip->text.find("Signal")!=std::string::npos,"Port tooltip lost its native label");
    require(portHit->tooltip->box.pos.isFinite(),"Tooltip position must remain valid at subview zoom");
    APP->event->setHoveredWidget(nullptr);require(!portHit->tooltip,"Port tooltip must close on leave");
    APP->event->setHoveredWidget(knobHit);require(knobHit->tooltip,"Parameter tooltip must use original widget");knobHit->tooltip->step();knobHit->tooltipAnchor->step();
    require(knobHit->tooltip->text.find("Gain")!=std::string::npos,"Parameter tooltip lost value/label");APP->event->setHoveredWidget(nullptr);
    settings::tooltips=false;APP->event->setHoveredWidget(portHit);require(!portHit->tooltip,"Subview must obey disabled tooltips");APP->event->setHoveredWidget(nullptr);settings::tooltips=true;
  }

  auto* dragSource=view->portFor(a->getOutput(1));
  widget::Widget::DragStartEvent start;start.button=GLFW_MOUSE_BUTTON_LEFT;dragSource->onDragStart(start);
  widget::Widget::DragDropEvent drop;drop.button=GLFW_MOUSE_BUTTON_LEFT;drop.origin=dragSource;
  view->portFor(b->getInput(1))->onDragDrop(drop);
  widget::Widget::DragEndEvent end;end.button=GLFW_MOUSE_BUTTON_LEFT;dragSource->onDragEnd(end);
  require(cableIds().size()==ids.size()+1 && rack->getIncompleteCables().empty(),"Subview drag must create only a complete internal cable");
  APP->history->undo();host->step();require(cableIds()==ids,"Internal repatch undo changed external cables");
  APP->history->redo();host->step();require(cableIds().size()==ids.size()+1,"Internal repatch redo failed");
  view->disconnect(a->getOutput(1));host->step();require(cableIds()==ids,"Internal disconnect failed");
  require(rack->getCable(externalId) && rack->getCable(internalId),"Subview altered protected or unrelated cables");
  {
    auto* source=view->portFor(b->getInput(0));auto* target=view->portFor(b->getInput(2));
    widget::Widget::DragStartEvent start;start.button=GLFW_MOUSE_BUTTON_LEFT;source->onDragStart(start);
    widget::Widget::DragDropEvent drop;drop.button=GLFW_MOUSE_BUTTON_LEFT;drop.origin=source;target->onDragDrop(drop);
    widget::Widget::DragEndEvent end;end.button=GLFW_MOUSE_BUTTON_LEFT;source->onDragEnd(end);
    require(cableIds()==ids && APP->engine->getCable(internalId)->inputId==2,"Dragging an existing plug must move that cable, not add another");
    APP->history->undo();host->step();view->step();require(APP->engine->getCable(internalId)->inputId==0,"Native cable move undo failed");
    APP->history->redo();host->step();view->step();require(APP->engine->getCable(internalId)->inputId==2,"Native cable move redo failed");
    APP->history->undo();host->step();view->step();
  }

  host->closeView();
  // A copied/pasted container must not take ownership of the original group.
  auto* copy=static_cast<beekapoo::Beekapoo*>(add(modelBeekapooPreview,Vec(645,0)));
  json_t* state=host->module->dataToJson();copy->module->dataFromJson(state);json_decref(state);copy->step();
  require(copy->members.size()==2 && host->members.size()==2 && !a->visible,"Copied container must clone rather than steal its group");
  std::vector<int64_t> copiedIds;for(auto& member:copy->members){require(member.id!=aId && member.id!=bId,"Copied members must have new engine IDs");copiedIds.push_back(member.id);}
  require(copy->parameters.size()==host->parameters.size() && copy->ports.size()==host->ports.size(),"Copy must preserve mappings");
  require(copy->randomLocks.size()==2,"Copy must retain randomization locks");
  for(const auto& lock:copy->randomLocks)require(lock.first!=aId && lock.first!=bId && copy->find(lock.first),"Copy must remap locks to its own new modules");
  rack->removeModule(copy);delete copy;
  for(auto id:copiedIds)if(auto* w=rack->getModule(id)){rack->removeModule(w);delete w;}
  for(const auto& port:host->ports){auto area=host->frameContent(port.input?host->inputFrame:host->outputFrame);require(port.face->box.getRight()<=area.getRight()&&port.face->box.getBottom()<=area.getBottom(),"Exposed port clipped outside its section face");}
  for(const auto& param:host->parameters)if(param.knob->visible)require(param.knob->box.getRight()<host->box.size.x && param.knob->box.getBottom()<=368,"Exposed knob clipped outside panel");
  host->arrangeByModule=true;
  for(float width:{75.f,180.f,300.f,540.f,900.f}) {
    host->requestedWidth=width;host->layoutControls();
    if(host->jacksOnly){for(auto& p:host->parameters)if(p.knob->visible)require(p.knob->box.getBottom()<host->inputFrame->box.pos.y,"Small parameter knobs must fit above I/O");continue;}
    auto bounds=beekapoo::Beekapoo::frameContent(host->frame);
    std::vector<Rect> occupied;
    auto check=[&](widget::Widget* control) {
      require(control->box.pos.x>=bounds.pos.x-.1f && control->box.getRight()<=bounds.getRight()+.1f && control->box.pos.y>=bounds.pos.y-.1f && control->box.getBottom()<=bounds.getBottom()+.1f,"Module-first controls must stay inside their frame face");
      for(auto r:occupied)require(!control->box.intersects(r),"Module-first controls must not overlap");occupied.push_back(control->box);
    };
    for(auto& p:host->parameters)if(p.knob->visible)check(p.knob);

  }
  host->arrangeByModule=false;
  for(float width:{75.f,90.f,105.f,120.f,150.f,180.f,300.f,450.f,900.f}) {
    host->requestedWidth=width;host->layoutControls();
    for(int i=0;i<6;++i){require(host->getInput(i)&&!host->getInput(i)->visible,"Legacy single-action CV jacks must stay hidden");require(host->actionButtons[i]->visible==(i%3!=2),"Exactly four manual action buttons must be visible");}
    require(host->growButton->visible&&host->regenerateButton->visible&&host->getInput(beekapoo::Module::ACTIONS_INPUT)->visible,"Generate and Grow buttons and shared poly CV must be visible");
    require(host->getParam(beekapoo::Module::WIGGLE_AMOUNT)&&!host->getParam(beekapoo::Module::WIGGLE_AMOUNT)->visible,"Wiggle quantity must remain available without occupying the panel");
    std::vector<widget::Widget*> visibleControls;
    for(auto* w:host->actionButtons)visibleControls.push_back(w);
    for(auto* w:host->managementButtons)visibleControls.push_back(w);
    visibleControls.push_back(host->growButton);visibleControls.push_back(host->regenerateButton);
    visibleControls.push_back(host->getInput(beekapoo::Module::ACTIONS_INPUT));
    for(int i=0;i<6;++i)visibleControls.push_back(host->getInput(i));
    visibleControls.push_back(host->getParam(beekapoo::Module::WIGGLE_AMOUNT));
    for(const auto& p:host->ports)visibleControls.push_back(p.face);
    for(const auto& p:host->parameters)visibleControls.push_back(p.knob);
    visibleControls.erase(std::remove_if(visibleControls.begin(),visibleControls.end(),[](widget::Widget* w){return !w->visible;}),visibleControls.end());
    require(host->ownFrame->box.size.x==host->box.size.x && host->titleFrame->box.size.x==host->box.size.x,"Header and module controls frames must fill the panel width");
    auto jack=host->getInput(beekapoo::Module::ACTIONS_INPUT)->box;
    require(jack.getCenter().x>host->regenerateButton->box.getCenter().x&&std::abs(jack.getCenter().y-host->regenerateButton->box.getCenter().y)<.01f,"Shared action CV must follow New subpatch on the same row");
    require(host->growButton->box.getBottom()<host->ownFrame->box.pos.y,"Functions menu must stay in the header");
    require(host->actionButtons[3]->faceColor.r < host->actionButtons[0]->faceColor.r-.3f && host->actionButtons[4]->faceColor.r < host->actionButtons[1]->faceColor.r-.3f,"All and Exposed actions must remain visually distinct");
    for(size_t i=0;i<visibleControls.size();++i) {
      auto bounds=visibleControls[i]->box;
      require(bounds.pos.x>=5 && bounds.pos.y>=5 && bounds.getRight()<=host->box.size.x-5 && bounds.getBottom()<=host->box.size.y-6,("Responsive control outside module at width "+std::to_string(int(host->box.size.x))+": "+std::to_string(bounds.pos.x)+","+std::to_string(bounds.pos.y)+" to "+std::to_string(bounds.getRight())+","+std::to_string(bounds.getBottom())).c_str());
      for(size_t j=0;j<i;++j)require(!bounds.intersects(visibleControls[j]->box),("Responsive controls overlap width "+std::to_string(int(host->box.size.x))+" controls "+std::to_string(i)+"/"+std::to_string(j)+" y "+std::to_string(bounds.pos.y)+"/"+std::to_string(visibleControls[j]->box.pos.y)).c_str());
    }
  }
  host->requestedWidth=450;host->layoutControls();
  auto* widthState=host->module->dataToJson();
  host->requestedWidth=900;host->layoutControls();host->module->dataFromJson(widthState);json_decref(widthState);host->step();
  require(host->requestedWidth==450 && host->box.size.x>=450,"User resize width must survive module JSON reload");
  auto* parent=host->parent;parent->removeChild(host);auto pos=host->box.pos;host->box.pos=Vec();
  host->requestedWidth=900;host->layoutControls();
  capture(host,output+"/02-reloaded.png",scale,false);
  for(int mode=1;mode<=2;++mode) {
    host->requestedWidth=mode==1?450.f:75.f;host->layoutControls();
    capture(host,output+(mode==1?"/10-medium.png":"/11-small.png"),scale,false);
  }
  host->requestedWidth=900;host->layoutControls();
  auto* mappedButton=dynamic_cast<beekapoo::MappedParam<beekapoo::ExposedSwitch>*>(host->parameters[1].knob);
  require(mappedButton,"Mapped button must have tooltip support");
  APP->event->setHoveredWidget(mappedButton);
  require(mappedButton->tip && mappedButton->tip->caption().find(host->memberName(bId))!=std::string::npos && host->memberName(bId).find("#")!=std::string::npos,"Mapped button tooltip must identify duplicate module instance");
  APP->event->setHoveredWidget(nullptr);
  require(!mappedButton->tip,"Mapped button tooltip must clear on leave");
  host->box.pos=pos;parent->addChild(host);
  host->groupAction(false);
  APP->history->undo();host->step();
  require(host->members.size()==2 && host->ports.size()==25 && host->parameters.size()==24,"Expand undo lost group or exposed controls");
  APP->history->redo();host->step();require(host->expanded && host->members.size()==2,"Expand redo must retain abstraction");
  require(a->getInput(0)->box.pos==originalInput && b->getOutput(0)->box.pos==originalOutput,"Expand did not restore jack positions");
  auto* registeredA=rack->getModule(aId);auto* registeredB=rack->getModule(bId);
  require(registeredA->visible && registeredB->visible && a->visible && b->visible && rack->getCable(internalId)->visible,"Expand failed");
  require(!registeredA->box.intersects(sink->box) && !registeredB->box.intersects(sink->box),"Expansion overlaps a module occupying freed space");
  require(registeredA->box.pos.y==registeredB->box.pos.y && registeredA->box.getRight()==registeredB->box.pos.x,"Expanded members must form one continuous block");
  require(rack->isSelected(registeredA) && rack->isSelected(registeredB),"Expanded members must remain selected");
  rack->deselectAll();rack->select(rack->getModule(guardId),true);Vec outsidePosition=rack->getModule(guardId)->box.pos;
  host->addSelection();require(host->expanded && host->members.size()==3,"Add selected outside module failed");
  APP->history->undo();host->step();require(host->members.size()==2 && !host->contains(APP->engine->getModule(guardId)) && rack->getModule(guardId)->box.pos==outsidePosition,"Undo add lost outside-module membership or position");
  APP->history->redo();host->step();require(host->members.size()==3,"Redo add failed");
  rack->deselectAll();rack->select(rack->getModule(guardId),true);host->removeSelection();
  require(host->members.size()==2 && rack->getModule(guardId)->visible,"Remove selected member must leave it in rack");
  APP->history->undo();host->step();require(host->members.size()==3,"Undo remove failed");
  APP->history->redo();host->step();require(host->members.size()==2,"Redo remove failed");
  rack->deselectAll();rack->select(rack->getModule(aId),true);host->removeSelection();
  require(host->members.size()==1 && rack->getModule(aId)->visible && host->boundaryPort(b->getInput(0)) && host->exposed(b->getInput(0)) && cableIds()==ids,"Removing a wired member must preserve cables and expose the new boundary");
  APP->history->undo();host->step();require(host->expanded && host->members.size()==2 && !host->boundaryPort(b->getInput(0)),"Undo wired removal must restore internal membership");
  host->endAction();require(host->members.empty() && rack->getModule(aId)->visible && rack->getModule(bId)->visible,"End expanded abstraction failed");
  APP->history->undo();host->step();require(host->expanded && host->members.size()==2,"Undo end must restore expanded group");
  APP->history->redo();host->step();require(host->members.empty(),"Redo end failed");
  APP->history->undo();host->step();
  auto* expandedPatch=APP->engine->toJson();rack->mergeJson(expandedPatch);
  Vec expandedA=rack->getModule(aId)->box.pos,expandedB=rack->getModule(bId)->box.pos;
  rack->clear();APP->engine->fromJson(expandedPatch);rack->fromJson(expandedPatch);json_decref(expandedPatch);
  host=dynamic_cast<beekapoo::Beekapoo*>(rack->getModule(hostId));host->step();
  a=host->find(aId);b=host->find(bId);sink=rack->getModule(sinkId);registeredA=rack->getModule(aId);registeredB=rack->getModule(bId);
  require(host->expanded && host->members.size()==2 && registeredA->box.pos==expandedA && registeredB->box.pos==expandedB,"Expanded grouping and block positions must survive reload");

  widget::EventContext eventContext;widget::Widget::ButtonEvent click;
  click.context=&eventContext;click.button=GLFW_MOUSE_BUTTON_LEFT;click.action=GLFW_PRESS;click.mods=0;click.pos=Vec(5,20);
  registeredA->onButton(click);
  require(eventContext.target==registeredA,"Expanded module background must target registered wrapper");
  rack->select(registeredA,true);rack->select(registeredB,true);host->groupAction(true);
  APP->history->undo();host->step();require(host->expanded && host->members.size()==2 && registeredA->visible,"Collapse undo must keep expanded grouping");
  APP->history->redo();host->step();require(host->members.size()==2 && !registeredA->visible,"Abstract redo failed");
  rack->deselectAll();rack->select(rack->getModule(sinkId),true);Vec sinkPosition=rack->getModule(sinkId)->box.pos;
  host->addSelection();require(!host->expanded && host->members.size()==3,"Add while collapsed must stay collapsed");
  APP->history->undo();host->step();require(host->members.size()==2 && rack->getModule(sinkId)->visible && rack->getModule(sinkId)->box.pos==sinkPosition,"Undo collapsed add must release outside module at its original position");
  host->endAction();require(host->members.empty() && registeredA->visible && registeredB->visible,"End collapsed abstraction must release its block");
  APP->history->undo();host->step();require(!host->expanded && host->members.size()==2,"Undo end must restore collapsed grouping");
  rack->removeModule(host);delete host;
  for(auto* child:APP->scene->children)if(auto* cleanup=dynamic_cast<beekapoo::BeekapooRackHooks*>(child))cleanup->step();
  require(!rack->getModule(aId) && !rack->getModule(bId) && cableIds().empty(),"Removing container must remove contained modules and cables");
  rack->clear();require(APP->engine->getModuleIds().empty(),"Expanded wrappers must release modules exactly once");
  json_t* intact=json_deep_copy(patch);
  // Exercise a missing member on load: recover the available module, don't
  // leave it hidden or attach the group to some unrelated replacement.
  auto* modules=json_object_get(patch,"modules");
  for(size_t i=0;i<json_array_size(modules);++i)
    if(json_integer_value(json_object_get(json_array_get(modules,i),"id"))==aId) {json_array_remove(modules,i);break;}
  APP->engine->fromJson(patch);rack->fromJson(patch);
  host=dynamic_cast<beekapoo::Beekapoo*>(rack->getModule(hostId));host->step();
  require(host->members.empty() && rack->getModule(bId)->visible,"Missing member must recover available modules");
  rack->clear();json_decref(patch);
  auto* legacyModules=json_object_get(intact,"modules");
  for(size_t i=0;i<json_array_size(legacyModules);++i) {
    auto* entry=json_array_get(legacyModules,i);
    if(json_integer_value(json_object_get(entry,"id"))==hostId) json_object_set_new(entry,"model",json_string("computerscare-patch-abstractor"));
  }
  APP->engine->fromJson(intact);rack->fromJson(intact);
  host=dynamic_cast<beekapoo::Beekapoo*>(rack->getModule(hostId));host->step();
  require(host && host->module->model==modelBeekapooLegacyPreview && host->members.size()==2,"Old module slug must restore the complete Beekapoo group");
  rack->clear();
  for(size_t i=0;i<json_array_size(legacyModules);++i) {
    auto* entry=json_array_get(legacyModules,i);
    if(json_integer_value(json_object_get(entry,"id"))==hostId) json_object_set_new(entry,"model",json_string("computerscare-peekaboo"));
  }
  APP->engine->fromJson(intact);rack->fromJson(intact);
  host=dynamic_cast<beekapoo::Beekapoo*>(rack->getModule(hostId));host->step();
  require(host->module->model==modelPeekabooOldNamePreview && host->members.size()==2,"Former Peekaboo slug must load the complete group");
  rack->touchedParam=host->find(aId)->getParam(0);
  rack->selectAll();rack->deleteSelectionAction();
  require(APP->engine->getModuleIds().empty() && rack->touchedParam==nullptr,"Delete-all left a held module or dangling touched control");
  json_decref(intact);
  // Editable subpatch layout, browser adoption, and selection removal.
  rack->clear();
  auto* editA=add(gainModel,Vec());auto* editB=add(gainModel,Vec(105,0));
  auto* editOut=add(gainModel,Vec(210,0));
  auto* editHost=static_cast<beekapoo::Beekapoo*>(add(modelBeekapooPreview,Vec(360,0)));
  int64_t editAId=editA->module->id,editBId=editB->module->id;
  connect(editA,editB);connect(editB,editOut);auto editCableIds=cableIds();
  rack->deselectAll();rack->select(editA,true);rack->select(editB,true);editHost->collapse();
  editHost->showView();auto* editView=dynamic_cast<beekapoo::SubpatchView*>(editHost->subview.get());editView->step();
  editView->selectModule(editAId,0);editView->selectModule(editBId,GLFW_MOD_SHIFT);
  require(editView->selected.size()==2,"Shift-click must select multiple subpatch modules");
  auto* editFace=static_cast<beekapoo::SubpatchModuleFace*>(editView->nativeRack->getModules().front());
  widget::EventContext selectionContext;widget::Widget::ButtonEvent selectionClick;
  selectionClick.context=&selectionContext;selectionClick.pos=Vec(2,2);selectionClick.action=GLFW_PRESS;selectionClick.button=GLFW_MOUSE_BUTTON_LEFT;selectionClick.mods=GLFW_MOD_SHIFT;
  editFace->onButton(selectionClick);
  widget::Widget::DragStartEvent selectionStart;selectionStart.button=GLFW_MOUSE_BUTTON_LEFT;editFace->onDragStart(selectionStart);
  require(!editView->selected.count(editFace->module->id) && !editView->moveHistory,"Shift-toggle must not immediately reselect a module through drag start");
  editView->selected.clear();editView->nativeRack->pointer=Vec();
  editView->nativeRack->onDragStart(selectionStart);
  widget::Widget::DragHoverEvent marqueeHover;marqueeHover.pos=Vec(240,379);editView->nativeRack->onDragHover(marqueeHover);
  widget::Widget::DragMoveEvent marqueeMove;marqueeMove.mouseDelta=Vec(240,379);editView->nativeRack->onDragMove(marqueeMove);
  require(editView->selected.size()==2,"Rectangle selection must select both modules using the current pointer, without double-counting delta");
  widget::Widget::DragEndEvent marqueeEnd;editView->nativeRack->onDragEnd(marqueeEnd);
  editView->selected={editAId,editBId};
  auto initialPositions=editHost->subpatchPositions;
  editView->beginMove(editAId);editView->moveSelection(Vec(30,RACK_GRID_HEIGHT));editView->endMove();
  for(auto id:{editAId,editBId})require(editHost->subpatchPositions[id]==initialPositions[id]+Vec(30,RACK_GRID_HEIGHT),"Dragging must move all selected modules together on Rack grid");
  require(cableIds()==editCableIds,"Moving subpatch modules changed audio connections");
  auto movedPositions=editHost->subpatchPositions;
  APP->history->undo();editHost->step();require(editHost->subpatchPositions==initialPositions,"Subview move undo must restore layout");
  APP->history->redo();editHost->step();require(editHost->subpatchPositions==movedPositions,"Subview move redo must restore layout");
  auto* editSaved=editHost->toJson();editHost->fromJson(editSaved);json_decref(editSaved);editHost->step();
  require(editHost->subpatchPositions==movedPositions,"Subview positions must survive patch JSON reload");
  editHost->showView();editView=dynamic_cast<beekapoo::SubpatchView*>(editHost->subview.get());editView->step();
  widget::Widget::EnterEvent addEnter;editView->addButton->onEnter(addEnter);
  require(editView->addButton->tooltip,"Browser return fixture needs an open control tooltip");
  WeakPtr<ui::Tooltip> browserTip=editView->addButton->tooltip;
  editView->openBrowser(Vec(450,0));require(APP->scene->browser->visible,"Add module must open Rack's existing browser");
  auto* browserAdded=add(gainModel,editHost->parkingPosition()+Vec(60,RACK_GRID_HEIGHT));int64_t browserId=browserAdded->module->id;
  APP->scene->browser->hide();
  // Exercise Scene's real child traversal, including a tooltip adjacent to the
  // view being replaced. Direct view->step() calls miss sibling invalidation.
  APP->scene->widget::Widget::step();
  APP->scene->widget::Widget::step();
  require(!browserTip,"Replacing the browser view must also retire its tooltip");
  editView=dynamic_cast<beekapoo::SubpatchView*>(editHost->subview.get());require(editView && editHost->members.size()==3 && rack->getModule(browserId)->box.size==Vec(),"Browser-created module must be absorbed and hidden in the main Rack");editView->step();
  require(editView->nativeRack->getModules().size()==3,"Browser-created module must appear in subpatch workspace");
  editView->selected={editAId,browserId};editView->queueRemoval(editAId);editView->step();
  require(editHost->members.size()==1 && !editHost->expanded && rack->getModule(editAId)->visible && rack->getModule(browserId)->visible && !rack->getModule(editBId)->visible,"Removing selection must return both modules to main Rack and keep remaining module abstracted");
  require(cableIds()==editCableIds,"Removing members must preserve all engine connections");
  APP->history->undo();editHost->step();require(editHost->members.size()==3 && !rack->getModule(browserId)->visible,"Selection removal must support undo");
  APP->history->redo();editHost->step();require(editHost->members.size()==1 && rack->getModule(browserId)->visible,"Selection removal must support redo");
  editHost->removeMembers({editBId});require(editHost->members.empty() && rack->getModule(editBId)->visible && cableIds()==editCableIds,"Removing final member must leave the audio patch intact");
  editHost->showView();editView=dynamic_cast<beekapoo::SubpatchView*>(editHost->subview.get());editView->step();
  require(editView && !editView->closing && editView->nativeRack->getModules().empty(),"Empty abstraction must keep an editable workspace available");
  rack->clear();

  // Scene erases the owner before destroying it. Tooltip cleanup must not
  // erase the sibling iterator that Scene is about to visit next.
  for(int kind=0;kind<3;++kind) {
    auto* owner=new widget::Widget;APP->scene->addChild(owner);
    widget::Widget::EnterEvent enter;WeakPtr<ui::Tooltip> tip;
    if(kind==0) {
      auto* control=new beekapoo::ActionButton;owner->addChild(control);
      control->onEnter(enter);tip=control->tooltip;
    } else if(kind==1) {
      auto* control=new beekapoo::MappedParam<SmallKnob>;owner->addChild(control);
      control->caption=[] {return "Mapped parameter";};control->onEnter(enter);tip=control->tip.get();
    } else {
      auto* control=new beekapoo::PortFace;owner->addChild(control);
      control->caption=[] {return "Mapped port";};control->onEnter(enter);tip=control->tip.get();
    }
    require(tip,"Tooltip deletion fixture must create a tooltip");
    owner->requestDelete();APP->scene->widget::Widget::step();
    require(!tip,"Deleting a tooltip owner must safely retire its scene sibling");
  }
  // Real-rack editing uses original widgets, never subview proxies.
  rack->clear();
  auto* realA=add(gainModel,Vec(0,0));auto* realB=add(gainModel,Vec(90,0));
  auto* realHost=static_cast<beekapoo::Beekapoo*>(add(modelBeekapooPreview,Vec(240,0)));
  auto realAId=realA->module->id;auto realBId=realB->module->id;
  connect(realA,realB);auto realCableIds=cableIds();
  rack->deselectAll();rack->select(realA,true);rack->select(realB,true);realHost->collapse();
  realA->module->params[0].setValue(.73f);
  auto initialModuleCount=APP->engine->getModuleIds().size();
  int beforeDuplicateSave=static_cast<beekapoofixture::Gain*>(realA->module)->saveCalls;
  realHost->duplicateAction(false);
  require(static_cast<beekapoofixture::Gain*>(realA->module)->saveCalls==beforeDuplicateSave+1,"Duplicate must prepare contained patch storage once");
  beekapoo::Beekapoo* cloned=nullptr;
  for(auto* w:rack->getModules())if(auto* candidate=dynamic_cast<beekapoo::Beekapoo*>(w))if(candidate!=realHost)cloned=candidate;
  require(cloned,"Native duplicate must create a container");cloned->step();
  require(cloned->members.size()==2 && APP->engine->getModuleIds().size()==initialModuleCount+3,"Native duplicate must clone its hidden contents");
  require(cableIds().size()==realCableIds.size()+1,"Duplicate without external cables must retain internal wiring");
  auto cloneId=cloned->module->id;
  APP->history->undo();require(APP->engine->getModuleIds().size()==initialModuleCount && cableIds()==realCableIds,"Duplicate undo must remove container and all hidden contents");
  APP->history->redo();cloned=dynamic_cast<beekapoo::Beekapoo*>(rack->getModule(cloneId));cloned->step();
  require(cloned->members.size()==2 && APP->engine->getModuleIds().size()==initialModuleCount+3,"Duplicate redo must restore one complete independent group");
  APP->history->undo();
  // An incoming cable is copied only by the with-cables command.
  auto* externalInput=add(gainModel,Vec(600,0));connect(externalInput,realA);realCableIds=cableIds();
  realHost->duplicateAction(true);
  cloned=nullptr;for(auto* w:rack->getModules())if(auto* candidate=dynamic_cast<beekapoo::Beekapoo*>(w))if(candidate!=realHost)cloned=candidate;
  require(cloned,"With-cables duplicate must create a container");cloned->step();
  require(cableIds().size()==realCableIds.size()+2,"With-cables duplicate must copy incoming and internal connections");
  APP->history->undo();require(APP->engine->getModuleIds().size()==initialModuleCount+1 && cableIds()==realCableIds,"With-cables duplicate undo must remove its complete wiring");
  // Rack's selection format contains the complete subpatch, not just live IDs.
  rack->deselectAll();rack->select(realHost,true);auto* fragment=rack->selectionToJson();
  require(json_object_get(json_object_get(json_array_get(json_object_get(fragment,"modules"),0),"data"),"contents"),"Saved selection must embed the subpatch");
  rack->pasteJsonAction(fragment);json_decref(fragment);APP->event->setDraggedWidget(nullptr,GLFW_MOUSE_BUTTON_LEFT);
  cloned=nullptr;for(auto* w:rack->getModules())if(auto* candidate=dynamic_cast<beekapoo::Beekapoo*>(w))if(candidate!=realHost)cloned=candidate;
  require(cloned,"Selection paste must create a container");cloned->step();
  require(cloned->members.size()==2 && cableIds().size()==realCableIds.size()+1,"Selection paste must restore independent modules and internal cables");
  bool preservedValue=false;for(auto& member:cloned->members)if(std::abs(member.widget->module->params[0].getValue()-.73f)<.0001f)preservedValue=true;
  require(preservedValue,"Selection paste must retain contained parameter values");
  APP->history->undo();require(cableIds()==realCableIds,"Selection paste undo must remove all copied wiring");
  rack->deselectAll();rack->select(realHost,true);
  auto* duplicateOverlay=new ui::MenuOverlay;auto* duplicateMenu=new ui::Menu;duplicateOverlay->addChild(duplicateMenu);APP->scene->addChild(duplicateOverlay);
  rack->appendSelectionContextMenu(duplicateMenu);
  beekapoo::BeekapooRackHooks* hooks=nullptr;for(auto* child:APP->scene->children)if(auto* h=dynamic_cast<beekapoo::BeekapooRackHooks*>(child))hooks=h;
  require(hooks,"Native rack copy policy hook must be installed");hooks->step();
  beekapoo::GroupDuplicateMenuItem* duplicateItem=nullptr;
  for(auto* child:duplicateMenu->children)
    if(auto* item=dynamic_cast<beekapoo::GroupDuplicateMenuItem*>(child))
      if(item->cables)duplicateItem=item;
  require(duplicateItem,"Native selection menu must retain contained-cable policy");
  widget::Widget::ActionEvent duplicateEvent;duplicateItem->onAction(duplicateEvent);
  cloned=nullptr;for(auto* w:rack->getModules())if(auto* candidate=dynamic_cast<beekapoo::Beekapoo*>(w))if(candidate!=realHost)cloned=candidate;
  cloned->step();require(cableIds().size()==realCableIds.size()+2,"Native selection Duplicate with cables must preserve incoming hidden endpoints");
  APP->history->undo();duplicateOverlay->hide();duplicateOverlay->requestDelete();
  auto savedOffset=APP->scene->rackScroll->getGridOffset();auto savedZoom=APP->scene->rackScroll->getZoom();
  realHost->editSubpatch();
  auto* session=dynamic_cast<beekapoo::RackEditSession*>(realHost->editSession.get());
  require(session && realHost->expanded && rack->getModule(realAId)==realA && rack->getModule(realBId)==realB,"Editing must restore original widgets directly to the native Rack");
  require(realA->box.pos.y==4*RACK_GRID_HEIGHT && realB->box.pos.y==realA->box.pos.y,"Editing must use a clear row four rows below Beekapoo");
  require(cableIds()==realCableIds,"Entering real-rack edit must preserve cable identities");
  rack->setModulePosNearest(realA,realA->box.pos+Vec(0,RACK_GRID_HEIGHT));
  require(realA->box.pos.y==5*RACK_GRID_HEIGHT,"Original module must use native Rack placement");
  auto* joined=add(gainModel,realB->box.pos+Vec(120,0));auto joinedId=joined->module->id;
  session->step();require(realHost->contains(joined->module) && rack->getModule(joinedId)==joined,"New modules must join editing group without replacing native widgets");
  rack->deselectAll();session->step();require(!session->removeButton->visible,"Remove must hide when no member is selected");
  rack->select(realA,true);rack->select(joined,true);session->step();
  require(session->removeButton->visible,"Remove must appear for a multi-module member selection");
  Vec removeOffset=APP->scene->rackScroll->getGridOffset();
  session->removeButton->action();realHost->step();session->step();session->step();
  require(realHost->members.size()==1 && realHost->editSession.get()==session && realHost->expanded,"Remove must leave the editor open and not readopt removed members");
  require(realA->visible && joined->visible && !realHost->contains(realA->module) && cableIds()==realCableIds,"Remove must preserve released modules and every cable");
  require(realA->box.pos.y<2*RACK_GRID_HEIGHT && joined->box.pos.y<2*RACK_GRID_HEIGHT,"Removed modules must return near Beekapoo instead of remaining in its editing rows");
  require(APP->scene->rackScroll->getGridOffset()==removeOffset,"Remove must preserve the edit viewport");
  APP->history->undo();realHost->step();
  session=dynamic_cast<beekapoo::RackEditSession*>(realHost->editSession.get());
  require(session && realHost->members.size()==3 && rack->getModule(realAId)==realA && cableIds()==realCableIds,"Undo Remove must restore editing membership and original widgets");
  APP->history->redo();realHost->step();
  session=dynamic_cast<beekapoo::RackEditSession*>(realHost->editSession.get());session->step();
  require(realHost->members.size()==1 && !realHost->contains(realA->module),"Redo Remove must not readopt released modules");
  APP->history->undo();realHost->step();
  session=dynamic_cast<beekapoo::RackEditSession*>(realHost->editSession.get());
  auto* editingState=realHost->module->dataToJson();realHost->module->dataFromJson(editingState);json_decref(editingState);realHost->step();
  session=dynamic_cast<beekapoo::RackEditSession*>(realHost->editSession.get());
  require(session && rack->getModule(realAId)==realA && rack->getModule(joinedId)==joined,"Reloading an editing state must resume with original native widgets");
  session->panelButton->action();session->step();require(!session->showPanel && !session->preview->visible && realHost->controls->visible,"Floating preview must be optional without hiding the actual Beekapoo panel");session->panelButton->action();
  session->exposeMode=true;
  APP->scene->box.size=Vec(1000,720);APP->scene->rackScroll->box.size=Vec(1000,690);
  APP->scene->rackScroll->step();session->step();
  realHost->focusEditedMembers();APP->scene->rackScroll->step();session->step();
  widget::EventContext exposeContext;widget::Widget::ButtonEvent exposeClick;
  exposeClick.context=&exposeContext;exposeClick.button=GLFW_MOUSE_BUTTON_LEFT;exposeClick.action=GLFW_PRESS;
  auto* exposeParam=realB->getParam(0);
  exposeClick.pos=exposeParam->getAbsoluteOffset(exposeParam->box.size*.5f)-session->box.pos;
  session->onButton(exposeClick);require(realHost->exposed(exposeParam),"Expose must toggle the actual source control in Rack");
  session->finish(true);
  require(!realHost->expanded && !realHost->editSession && realHost->members.size()==3 && rack->getModule(joinedId)->box.size==Vec(),"Done must collapse all members including newly added modules");
  require(APP->scene->rackScroll->getGridOffset()==savedOffset && APP->scene->rackScroll->getZoom()==savedZoom,"Done must restore the previous viewport");
  require(cableIds()==realCableIds,"Done must preserve original cables");
  // Cables patched through the compact front stay on that front.
  auto* frontA=realHost->find(realAId);auto* frontB=realHost->find(realBId);
  realHost->expose(frontA,frontA->getOutput(3));realHost->expose(frontB,frontB->getInput(3));
  widget::Widget::DragStartEvent panelStart;panelStart.button=GLFW_MOUSE_BUTTON_LEFT;frontA->getOutput(3)->onDragStart(panelStart);
  widget::Widget::DragDropEvent panelDrop;panelDrop.button=GLFW_MOUSE_BUTTON_LEFT;panelDrop.origin=frontA->getOutput(3);
  beekapoo::PortFace* targetFace=nullptr;for(auto& port:realHost->ports)if(port.id==realBId && port.portId==3 && port.input)targetFace=port.face;
  require(targetFace,"Front-patching fixture needs its input face");targetFace->onDragDrop(panelDrop);
  widget::Widget::DragEndEvent panelEnd;panelEnd.button=GLFW_MOUSE_BUTTON_LEFT;frontA->getOutput(3)->onDragEnd(panelEnd);
  auto* frontCable=rack->getTopCable(targetFace->source.get());auto frontCableId=frontCable->cable->id;
  require(realHost->panelCableIds.count(frontCableId),"Front-panel patch intent must be saved before the next UI frame");
  realHost->step();require(frontCable->visible && realHost->panelCableIds.count(frontCableId),"Panel-patched self connections must remain visible outside Beekapoo");
  auto frontState=realHost->module->dataToJson();realHost->module->dataFromJson(frontState);json_decref(frontState);realHost->step();
  require(frontCable->visible,"Panel cable presentation must survive module state reload");
  realHost->duplicateAction(false);cloned=nullptr;
  for(auto* w:rack->getModules())if(auto* candidate=dynamic_cast<beekapoo::Beekapoo*>(w))if(candidate!=realHost)cloned=candidate;
  cloned->step();require(cloned->panelCableIds.size()==1,"Duplicate must remap front-panel cable IDs");
  for(auto* cw:rack->getCompleteCables())if(cloned->panelCableIds.count(cw->cable->id))require(cw->visible,"Duplicated front cable must remain visible");
  APP->history->undo();
  frontA=realHost->find(realAId);frontB=realHost->find(realBId);
  auto panelInput=[&]()->app::PortWidget* {for(const auto& p:realHost->ports)if(p.id==realBId&&p.input&&p.portId==3)return realHost->findPort(p);return nullptr;};
  auto* dragPort=panelInput();require(dragPort,"Mapped native input must survive duplicate Undo");widget::Widget::DragStartEvent frontStart;frontStart.button=GLFW_MOUSE_BUTTON_LEFT;dragPort->onDragStart(frontStart);
  widget::Widget::DragDropEvent frontDrop;frontDrop.button=GLFW_MOUSE_BUTTON_LEFT;frontDrop.origin=dragPort;externalInput->getInput(3)->onDragDrop(frontDrop);
  widget::Widget::DragEndEvent frontEnd;frontEnd.button=GLFW_MOUSE_BUTTON_LEFT;dragPort->onDragEnd(frontEnd);realHost->step();
  auto* movedFront=rack->getTopCable(externalInput->getInput(3));require(movedFront && movedFront->visible,"Moving a panel cable outside must preserve its visible cable");
  APP->history->undo();realHost->step();
  auto* restoredFront=rack->getTopCable(panelInput());require(restoredFront && restoredFront->visible,"Undo of external repatching must retain front-panel presentation");
  rack->removeCable(restoredFront);delete restoredFront;realHost->step();
  require(cableIds()==realCableIds,"Front-panel cable regression must preserve the original patch");
  // Container deletion is a single native undo transaction for all contents.
  int64_t deletingHostId=realHost->module->id;
  realHost->deleteGroupAction();
  require(APP->engine->getModuleIds().size()==1 && cableIds().empty(),"Deleting Beekapoo must delete all contained modules and their internal/external cables");
  APP->history->undo();realHost=dynamic_cast<beekapoo::Beekapoo*>(rack->getModule(deletingHostId));realHost->step();
  require(realHost->members.size()==3 && rack->getModule(realAId) && rack->getModule(joinedId) && cableIds()==realCableIds,"One undo must restore the complete subpatch and exact cable IDs");
  require(std::abs(rack->getModule(realAId)->module->params[0].getValue()-.73f)<.0001f,"Delete undo must preserve contained parameter values");
  APP->history->redo();hooks->step();
  require(APP->engine->getModuleIds().size()==1 && cableIds().empty(),"Delete redo must remove the complete group again");
  APP->history->undo();realHost=dynamic_cast<beekapoo::Beekapoo*>(rack->getModule(deletingHostId));realHost->step();
  // Undo before queued sibling cleanup must cancel that cleanup.
  realHost->removeAction();APP->history->undo();realHost=dynamic_cast<beekapoo::Beekapoo*>(rack->getModule(deletingHostId));realHost->step();hooks->step();
  require(realHost->members.size()==3 && cableIds()==realCableIds,"Immediate undo must not let deferred cleanup delete restored members");
  // A mixed native selection can include the parent and a real child while editing.
  realHost->editSubpatch();rack->deselectAll();rack->select(realHost,true);rack->select(rack->getModule(realAId),true);
  rack->deleteSelectionAction();hooks->step();
  require(APP->engine->getModuleIds().size()==1 && cableIds().empty(),"Mixed parent/child deletion must delete the remaining subpatch without double deletion");
  APP->history->undo();realHost=dynamic_cast<beekapoo::Beekapoo*>(rack->getModule(deletingHostId));realHost->step();
  require(realHost->members.size()==3 && realHost->editSession && cableIds()==realCableIds,"Mixed selection undo must restore editing, all children and all cables");
  auto* deletedMember=realHost->find(realAId);deletedMember->removeAction();realHost->step();
  require(realHost->members.size()==2 && realHost->editSession && APP->engine->getModule(joinedId),"Deleting a member in Edit must keep the rest of the group and session");
  for(auto& p:realHost->ports)require(p.id!=realAId,"Member deletion must remove its jack mappings");
  APP->history->undo();realHost->step();
  require(realHost->members.size()==3 && realHost->editSession && cableIds()==realCableIds,"Member deletion undo must restore membership and connections");
  APP->history->redo();realHost->step();require(realHost->members.size()==2 && realHost->editSession,"Member deletion redo must leave the editor open");
  APP->history->undo();realHost->step();
  realHost->find(realAId)->removeAction();APP->history->undo();realHost->step();
  require(realHost->members.size()==3 && realHost->editSession,"Immediate member Undo must rebind its recreated widget and mappings");
  rack->deselectAll();for(auto& member:realHost->members)rack->select(rack->getModule(member.id),true);
  rack->deleteSelectionAction();realHost->step();
  require(realHost->members.empty() && realHost->editSession,"Deleting the last member must keep an empty editing session usable");
  APP->history->undo();realHost->step();require(realHost->members.size()==3 && realHost->editSession,"Undo last-member deletion must restore the editing group");
  APP->history->redo();realHost->step();require(realHost->members.empty() && realHost->editSession,"Redo last-member deletion must keep the empty editor open");
  auto* emptyState=realHost->module->dataToJson();realHost->module->dataFromJson(emptyState);json_decref(emptyState);realHost->step();
  require(realHost->members.empty() && realHost->editSession,"An empty editing session must survive state reload");
  auto* emptySession=static_cast<beekapoo::RackEditSession*>(realHost->editSession.get());
  widget::Widget::HoverKeyEvent emptyEscape;widget::EventContext emptyEscapeContext;emptyEscape.context=&emptyEscapeContext;emptyEscape.key=GLFW_KEY_ESCAPE;emptyEscape.action=GLFW_PRESS;
  emptySession->onHoverKey(emptyEscape);require(!realHost->editSession && !realHost->expanded,"Escape must also close an empty editing session");
  APP->history->undo();realHost->step();
  session=static_cast<beekapoo::RackEditSession*>(realHost->editSession.get());
  widget::Widget::HoverKeyEvent escape;widget::EventContext escapeContext;escape.context=&escapeContext;escape.key=GLFW_KEY_ESCAPE;escape.action=GLFW_PRESS;
  session->onHoverKey(escape);require(!realHost->editSession && !realHost->expanded,"Escape must finish editing and collapse the group");
  realHost->endAbstraction();realHost->removeAction();hooks->step();
  require(APP->engine->getModuleIds().size()==4 && cableIds()==realCableIds,"End abstraction must still release modules without deleting them");
  rack->clear();
  // Show actual computerscare modules in both subview modes for visual QA.
  modelVolyMultiPreview->plugin=&plugin;modelComputerscareMelyPorge->plugin=&plugin;
  modelVolyMultiPreview->name="Voly Pector";modelComputerscareMelyPorge->name="Mely Porge";
  // Reproduce first-step caching after port relocation, as seen with Voly.
  {
    auto* native=add(modelVolyMultiPreview,Vec(0,0));
    auto* other=add(gainModel,Vec(900,0));
    auto* owner=static_cast<beekapoo::Beekapoo*>(add(modelBeekapooPreview,Vec(1200,0)));
    std::vector<std::pair<int,Vec>> positions;
    for(auto* port:native->getOutputs())positions.push_back({port->portId,port->box.pos});
    rack->deselectAll();rack->select(native,true);rack->select(other,true);owner->collapse();
    owner->expose(native,native->getOutput(7));
    auto* c=new engine::Cable;c->outputModule=native->module;c->outputId=7;c->inputModule=other->module;c->inputId=0;
    APP->engine->addCable(c);auto* cw=new app::CableWidget;cw->setCable(c);rack->addCable(cw);
    for(int f=0;f<3;++f)rack->step();
    owner->editSubpatch();
    for(int f=0;f<3;++f)rack->step();
    for(const auto& position:positions)require(native->getOutput(position.first)->box.pos==position.second,"Edit must restore native Voly jacks, not first-step cached panel coordinates");
    require(cw->outputPort==native->getOutput(7),"Cable must remain attached to the real restored jack");
    APP->scene->box.size=Vec(1100,760);APP->scene->rackScroll->box=Rect(Vec(0,30),Vec(1100,730));owner->focusEditedMembers();
    for(int f=0;f<3;++f)APP->scene->rackScroll->step();
    capture(APP->scene,output+"/restored-voly-jacks.png",scale,false);
    static_cast<beekapoo::RackEditSession*>(owner->editSession.get())->finish(true);
    // Hover buttons change only their section, and support Undo/Redo.
    owner->exposeParameter(native->module->id,0);owner->expose(other,other->getInput(1));
    for(int kind=0;kind<3;++kind) {
      auto counts=[&] {std::array<size_t,3> result{{owner->parameters.size(),0,0}};for(const auto& p:owner->ports)++result[p.input?1:2];return result;};
      auto before=counts();auto cables=cableIds();
      auto* block=kind==0?owner->frame:kind==1?owner->inputFrame:owner->outputFrame;
      owner->updateSectionAddButtons(block->box.getCenter());
      auto* corner=owner->sectionAddButtons[kind];
      require(corner->visible,"Section plus must appear on hover");
      require(corner->box.pos.y<block->box.pos.y && corner->box.getRight()<=owner->box.size.x,"Section plus must hang above the frame without leaving the module");
      owner->updateSectionAddButtons(corner->box.pos+Vec(2,2));
      require(corner->visible,"Hover must stay active on the overhanging part of the button");
      owner->sectionAddButtons[kind]->action();auto after=before;++after[kind];
      require(counts()==after,"Section plus must add exactly one matching control");
      APP->history->undo();owner->step();require(counts()==before&&cableIds()==cables,"Undo must restore exposure and bridge cables");
      APP->history->redo();owner->step();require(counts()==after,"Redo must restore section exposure");
    }
    owner->requestedWidth=210;owner->layoutControls();owner->step();
    owner->updateSectionAddButtons(owner->frame->box.getCenter());
    auto* parent=owner->parent;auto position=owner->box.pos;parent->removeChild(owner);owner->box.pos=Vec();capture(owner,output+"/section-plus.png",scale,false);owner->box.pos=position;parent->addChild(owner);
    owner->updateSectionAddButtons(Vec(-1,-1));for(auto* b:owner->sectionAddButtons)require(!b->visible,"Section buttons must hide after leaving");
    rack->clear();APP->history->clear();
    std::puts("First-step Voly jack restoration and section-specific expose buttons/Undo/Redo passed");
  }
  auto* voly=add(modelVolyMultiPreview,Vec(0,0));
  auto* mely=add(modelComputerscareMelyPorge,Vec(voly->box.size.x+30,0));
  auto* outside=add(gainModel,Vec(mely->box.getRight()+30,0));
  auto* group=static_cast<beekapoo::Beekapoo*>(add(modelBeekapooPreview,Vec(outside->box.getRight()+30,0)));
  connect(voly,mely);connect(mely,outside);APP->engine->stepBlock(64);voly->step();mely->step();
  rack->select(voly,true);rack->select(mely,true);group->groupAction(true);
  group->exposeParameter(voly->module->id,0);group->exposeParameter(mely->module->id,0);
  if(auto* input=mely->getInput(1))group->expose(mely,input);
  group->showView();auto* realView=dynamic_cast<beekapoo::SubpatchView*>(group->subview.get());
  APP->scene->box.size=Vec(std::max(900.f,mely->box.getRight()+90),620);
  realView->step();
  auto captureView=[&](const std::string& path){
    auto idsBefore=cableIds();
    float rackOpacity=settings::cableOpacity;settings::cableOpacity=.13f;
    std::vector<std::pair<app::CableWidget*,std::pair<app::PortWidget*,app::PortWidget*>>> endpoints;
    for(auto* cable:rack->getCompleteCables())endpoints.push_back({cable,{cable->inputPort,cable->outputPort}});
    auto* parent=realView->parent;parent->removeChild(realView);capture(realView,path,scale,false);parent->addChild(realView);
    require(settings::cableOpacity==.13f,"Subview opacity leaked into main rack setting");settings::cableOpacity=rackOpacity;
    require(realView->canvas->renderedCables>0,"Normal modal draw must render internal native cables without an external layer pass");
    require(cableIds()==idsBefore,"Rendering must not change engine cables");
    for(const auto& p:endpoints)require(p.first->inputPort==p.second.first && p.first->outputPort==p.second.second,"Rendering must restore native rack endpoints");
  };
  captureView(output+"/04-adjust-subpatch.png");
  realView->selected={voly->module->id,mely->module->id};
  realView->beginMove(voly->module->id);realView->moveSelection(Vec(60,0));realView->endMove();
  realView->fitView();realView->step();captureView(output+"/12-editable-rack.png");
  realView->setMode(true);realView->setViewZoom(.75f,Vec());captureView(output+"/05-expose-subpatch.png");
  {
    realView->fitView();realView->step();
    beekapoo::SubpatchHit* hit=nullptr;
    for(auto* candidate:realView->hits)if(candidate->param && candidate->visible){hit=candidate;break;}
    require(hit,"Actual subpatch needs a parameter tooltip target");
    widget::Widget::EnterEvent enter;APP->event->setHoveredWidget(hit);require(hit->tooltip,"Actual module's native tooltip was not created");
    hit->tooltip->step();hit->tooltipAnchor->step();
    struct TooltipShot : widget::Widget {
      beekapoo::SubpatchView* view;ui::Tooltip* tip;
      void draw(const DrawArgs& args) override {
        view->draw(args);nvgSave(args.vg);nvgTranslate(args.vg,tip->box.pos.x,tip->box.pos.y);
        auto local=args;local.clipBox=tip->box.zeroPos();tip->draw(local);nvgRestore(args.vg);
      }
    } shot;
    shot.view=realView;shot.tip=hit->tooltip.get();shot.box.size=realView->box.size;
    capture(&shot,output+"/09-native-tooltip.png",scale,false);
    widget::Widget::LeaveEvent leave;APP->event->setHoveredWidget(nullptr);
  }
  realView->setViewZoom(1.8f,Vec());realView->scroll->offset=Vec(90,80);realView->step();captureView(output+"/07-zoomed-scrolled.png");
  std::vector<Rect> faceBounds;
  for(auto* child:realView->nativeRack->getModuleContainer()->children) {
    for(const auto& prior:faceBounds)require(!prior.intersects(child->box),"Subview module panels must never overlap");
    faceBounds.push_back(child->box);
  }
  group->closeView();
  group->arrangeByModule=true;group->requestedWidth=540;group->layoutControls();group->saveGroup();
  require(group->frame->visible && group->inputFrame->visible && group->outputFrame->visible && group->outputFrame->box.getBottom()>=367,"I/O must remain at the bottom in module-first arrangement");
  group->hiddenKinds=2;group->layoutControls();
  for(auto& port:group->ports)if(port.input)require(!port.face->visible,"Input visibility filter must hide all input faces without removing mappings");
  group->hiddenKinds=0;
  group->hiddenModuleKinds[voly->module->id]=7;group->layoutControls();
  for(auto& param:group->parameters)if(param.id==voly->module->id)require(!param.knob->visible,"Module visibility filter must hide that module's controls");
  group->hiddenModuleKinds.clear();group->layoutControls();group->saveGroup();
  APP->scene->rack->getModuleContainer()->removeChild(group);
  auto groupedShotPosition=group->box.pos;group->box.pos=Vec();
  capture(group,output+"/19-by-module.png",scale,false);group->box.pos=groupedShotPosition;
  APP->scene->rack->getModuleContainer()->addChild(group);
  group->requestedWidth=45;group->layoutControls();
  require(group->jacksOnly && !group->getParam(beekapoo::Module::WIGGLE_AMOUNT)->visible,"Minimum view must hide parameter knobs");
  for(auto* button:group->managementButtons)require(button->visible,"Edit and Eject must always stay visible");
  for(int i=0;i<6;++i)require(group->actionButtons[i]->visible==(i%3!=2),"All layouts must retain the four manual Wiggle/Randomize actions");
  int visibleKnobs=0;for(auto& p:group->parameters)if(p.knob->visible)++visibleKnobs;
  for(auto& p:group->parameters)require(p.knob->visible,"Small layouts must retain every exposed parameter");
  require(group->titleFrame->visible && group->ownFrame->visible,"Minimum layout must keep the logo and its separate module-controls block");
  auto* tinyParent=group->parent;auto tinyPosition=group->box.pos;tinyParent->removeChild(group);group->box.pos=Vec();
  capture(group,output+"/20-jacks-only.png",scale,false);group->box.pos=tinyPosition;tinyParent->addChild(group);
  auto* patchInput=mely->getInput(1);APP->event->setDraggedWidget(patchInput,GLFW_MOUSE_BUTTON_LEFT);
  for(auto& port:group->ports)if(!port.input)require(port.face->tinyUp.type==engine::Port::OUTPUT,"Compact output artwork must preserve output type for native drag fading");
  tinyParent->removeChild(group);group->box.pos=Vec();capture(group,output+"/21-panel-patching.png",scale,false);group->box.pos=tinyPosition;tinyParent->addChild(group);
  APP->event->setDraggedWidget(nullptr,GLFW_MOUSE_BUTTON_LEFT);
  group->requestedWidth=540;group->layoutControls();
  require(group->managementButtons[0]->icon==3 && group->managementButtons[1]->icon==7 && group->editShowsText==(group->box.size.x>=150),"Header management controls must retain icons and add text when there is room");
  {
    auto* parent=group->parent;auto position=group->box.pos;parent->removeChild(group);group->box.pos=Vec();
    capture(group,output+"/25-edit-eject-panel.png",scale,false);
    group->box.pos=position;parent->addChild(group);
  }
  APP->event->setSelectedWidget(nullptr);APP->scene->browser->hide();
  widget::EventContext editContext,kbdExposeContext,adjustContext;
  widget::Widget::HoverKeyEvent editKey;editKey.context=&editContext;editKey.action=GLFW_PRESS;editKey.key=GLFW_KEY_E;editKey.mods=0;group->onHoverKey(editKey);
  require(group->editSession&&editKey.isConsumed(),"E on hovered Beekapoo must open subpatch editing");
  APP->scene->box.size=Vec(1100,740);APP->scene->rackScroll->box=Rect(Vec(0,30),Vec(1100,710));
  group->focusEditedMembers();
  auto* realSession=static_cast<beekapoo::RackEditSession*>(group->editSession.get());
  widget::Widget::HoverKeyEvent exposeKey;exposeKey.context=&kbdExposeContext;exposeKey.action=GLFW_PRESS;exposeKey.key=GLFW_KEY_E;exposeKey.mods=0;realSession->onHoverKey(exposeKey);
  require(realSession->exposeMode&&exposeKey.isConsumed(),"E must enter Expose mode");
  widget::Widget::HoverKeyEvent adjustKey;adjustKey.context=&adjustContext;adjustKey.action=GLFW_PRESS;adjustKey.key=GLFW_KEY_X;adjustKey.mods=0;realSession->onHoverKey(adjustKey);require(!realSession->exposeMode,"X again must return to Adjust mode");

  {
    widget::EventContext context;widget::Widget::HoverKeyEvent key;
    key.context=&context;key.action=GLFW_PRESS;key.key=GLFW_KEY_E;key.mods=0;
    group->onHoverKey(key);
    require(realSession->exposeMode&&key.isConsumed(),"E over the owner panel must toggle Expose during editing");
    key.action=GLFW_REPEAT;realSession->onHoverKey(key);
    require(realSession->exposeMode,"Holding E must not repeatedly toggle Expose");
    key.action=GLFW_PRESS;realSession->onHoverKey(key);
    require(!realSession->exposeMode,"E again must return to Adjust mode");
    ui::TextField field;APP->event->setSelectedWidget(&field);
    realSession->onHoverKey(key);
    require(!realSession->exposeMode,"Typing E in a text field must not toggle Expose");
    APP->event->setSelectedWidget(nullptr);
  }
  group->moveEditedMembers(true,false);
  require(group->controls->visible && !group->parameters.empty(),"Near editing must retain the owner's mapped control layer");
  for(auto& p:group->parameters)require(p.knob->visible,"Mapped parameters must remain drawn on Beekapoo itself in Near edit mode");
  realSession->panelButton->action();
  require(!realSession->showPanel && group->controls->visible && group->frame->visible,"Hiding the floating preview must not hide the actual panel controls or their frame");
  realSession->panelButton->action();
  {
    auto* parent=group->parent;auto position=group->box.pos;parent->removeChild(group);group->box.pos=Vec();
    capture(group,output+"/35-near-edit-panel.png",scale,false);
    group->box.pos=position;parent->addChild(group);
  }
  group->moveEditedMembers(false,false);
  for(int frame=0;frame<3;++frame){APP->scene->rackScroll->step();realSession->step();}
  rack->deselectAll();rack->select(voly,true);realSession->step();
  auto editOutsidePosition=outside->box.pos;
  outside->box.pos=Vec(mely->box.getRight()+15,mely->box.pos.y);
  capture(APP->scene,output+"/17-real-rack-edit.png",scale,false);
  outside->box.pos=editOutsidePosition;
  {
    rack->deselectAll();rack->select(outside,true);rack->select(group,true);
    widget::EventContext context;widget::Widget::HoverKeyEvent key;
    key.context=&context;key.action=GLFW_PRESS;key.key=GLFW_KEY_A;key.mods=RACK_MOD_CTRL;
    realSession->onHoverKey(key);
    require(context.consumed && rack->getSelected().size()==2 && rack->isSelected(voly) && rack->isSelected(mely) && !rack->isSelected(group) && !rack->isSelected(outside),"Edit Cmd/Ctrl+A must select only subpatch members");
    realSession->randomLockButton->action();
    auto click=[&](Vec pos) {
      widget::EventContext ctx;widget::Widget::ButtonEvent e;
      e.context=&ctx;e.action=GLFW_PRESS;e.button=GLFW_MOUSE_BUTTON_LEFT;e.mods=0;e.pos=pos;
      realSession->onButton(e);require(ctx.consumed,"Rnd Lock clicks must not reach native patch controls");
    };
    auto* parameter=voly->getParam(0);
    click(parameter->getAbsoluteOffset(parameter->box.size*.5f)-realSession->box.pos);
    click(mely->getAbsoluteOffset(Vec(3,200))-realSession->box.pos);
    require(group->randomLocks.count({voly->module->id,0}) && group->randomLocks.count({mely->module->id,-1}),"Rnd Lock must toggle individual parameters and module backgrounds");
    rack->deselectAll();realSession->step();
    APP->event->setHoveredWidget(nullptr);
    capture(APP->scene,output+"/34-randomization-locks.png",scale,false);
    APP->event->setHoveredWidget(nullptr);
    realSession->optionsButton->action();
    ui::MenuOverlay* lockMenuOverlay=nullptr;ui::MenuItem* unlockItem=nullptr;
    for(auto* child:APP->scene->children)if(auto* overlay=dynamic_cast<ui::MenuOverlay*>(child))
      for(auto* nested:overlay->children)if(auto* menu=dynamic_cast<ui::Menu*>(nested))
        for(auto* item:menu->children)if(auto* choice=dynamic_cast<ui::MenuItem*>(item))
          if(choice->text=="Unlock all"){unlockItem=choice;lockMenuOverlay=overlay;}
    require(unlockItem && !unlockItem->disabled,"Edit options must offer Unlock all while locks exist");
    widget::Widget::ActionEvent unlock;unlockItem->onAction(unlock);
    require(group->randomLocks.empty(),"Unlock all in the toolbar menu must remove both lock kinds");
    APP->scene->removeChild(lockMenuOverlay);delete lockMenuOverlay;
    realSession->randomLockMode=false;
  }
  {
    auto* overlay=new ui::MenuOverlay;auto* menu=new ui::Menu;
    overlay->addChild(menu);APP->scene->addChild(overlay);
    group->appendContextMenu(menu);menu->step();
    overlay->removeChild(menu);menu->box.pos=Vec();
    capture(menu,output+"/30-edit-fade-menu.png",scale,false);
    delete menu;APP->scene->removeChild(overlay);delete overlay;
  }
  realSession->exposeMode=true;realSession->showPanel=false;realSession->preview->visible=false;
  require(!realSession->highlights.empty(),"Expose must install native-module background highlight layers");
  for(auto& layer:realSession->highlights)if(layer)require(layer->parent==layer->sourceModule.get(),"Expose highlights must live behind the source module's controls");
  APP->event->setHoveredWidget(voly->getParam(0));
  capture(APP->scene,output+"/18-real-rack-expose.png",scale,false);APP->event->setHoveredWidget(nullptr);
  realSession->finish(true);
  require(group->returnHighlight>.79f && !group->editSession,"Leaving Edit must start the 800 ms return highlight");
  group->requestedWidth=300;group->layoutControls();group->step();
  APP->scene->box.size=Vec(520,740);APP->scene->rackScroll->box=Rect(Vec(0,30),Vec(520,710));
  APP->scene->rackScroll->zoomToBound(group->box);APP->scene->rackScroll->step();group->step();
  group->returnHighlight=.72f;
  capture(APP->scene,output+"/26-return-to-patch.png",scale,false);
  group->returnHighlight=0;
  beekapoo::PortFace* visibleOutput=nullptr;for(auto& port:group->ports)if(!port.input)visibleOutput=port.face;
  require(visibleOutput,"Screenshot needs a mapped output");
  widget::EventContext outputHoverContext;widget::Widget::HoverEvent outputHover;outputHover.context=&outputHoverContext;outputHover.pos=visibleOutput->box.size*.5f;
  visibleOutput->onHover(outputHover);APP->event->setHoveredWidget(outputHoverContext.target);
  require(visibleOutput->nativeTooltip && visibleOutput->tip,"Screenshot must use the native output tooltip");visibleOutput->tip->step();
  capture(APP->scene,output+"/22-output-voltage-tooltip.png",scale,false);APP->event->setHoveredWidget(nullptr);
  visibleOutput->step();
  beekapoo::PortFace* visibleInput=nullptr;for(auto& port:group->ports)if(port.input)visibleInput=port.face;
  require(visibleInput,"Screenshot needs a mapped input");
  widget::EventContext inputHoverContext;widget::Widget::HoverEvent inputHover;inputHover.context=&inputHoverContext;inputHover.pos=visibleInput->box.size*.5f;
  visibleInput->onHover(inputHover);APP->event->setHoveredWidget(inputHoverContext.target);visibleInput->tip->step();
  auto inputCaption=visibleInput->caption();
  require(visibleInput->tip->text.rfind(inputCaption.substr(0,inputCaption.find('\n'))+"\n",0)==0,"Real input hover must also include its source module heading");
  capture(APP->scene,output+"/29-input-module-tooltip.png",scale,false);APP->event->setHoveredWidget(nullptr);visibleInput->step();

  auto* groupParent=group->parent;auto groupPosition=group->box.pos;
  groupParent->removeChild(group);group->box.pos=Vec();capture(group,output+"/08-action-grid.png",scale,false);
  const float resizeWidths[]={75,180,360,600};
  const char* resizeFiles[]={"/13-minimum-width.png","/14-compact-width.png","/15-readable-width.png","/16-roomy-width.png"};
  for(int i=0;i<4;++i) {
    group->requestedWidth=resizeWidths[i];group->layoutControls();
    capture(group,output+resizeFiles[i],scale,false);
  }
  {
    bool meterWasEnabled=settings::cpuMeter;
    settings::cpuMeter=true;
    APP->engine->stepBlock(96000);
    double expected=0;
    for(const auto& member:group->members)
      expected+=beekapoo::moduleCpuSeconds(APP->engine->getModule(member.id));
    require(std::abs(group->subpatchCpuPercent()-expected*APP->engine->getSampleRate()*100)<.001,
            "Subpatch CPU must sum native member readings without including the host");
    group->requestedWidth=360;group->layoutControls();
    capture(group,output+"/31-subpatch-cpu.png",scale,false);
    group->requestedWidth=45;group->layoutControls();
    capture(group,output+"/32-subpatch-cpu-small.png",scale,false);
    require(settings::cpuMeter,"Drawing the custom CPU meter must not change Rack's global switch");
    settings::cpuMeter=meterWasEnabled;
  }
  group->requestedWidth=180;group->layoutControls();group->box.pos=groupPosition;groupParent->addChild(group);
  // The external fixture only demonstrates a locked boundary in the subview;
  // omit that unstyled test module from the rack photograph.
  rack->removeModule(outside);delete outside;
  group->box.pos=Vec(15,0);group->step();group->groupAction(false);
  group->step();
  auto* rackParent=rack->parent;auto rackSize=rack->box.size;auto rackPosition=rack->box.pos;
  float right=0;for(auto* w:rack->getModules())if(w->visible){w->box.pos.y+=12;right=std::max(right,w->box.getRight());}
  rackParent->removeChild(rack);rack->box.pos=Vec();rack->box.size=Vec(right+20,404);
  capture(rack,output+"/06-expanded-selected.png",scale,false);
  rack->box.pos=rackPosition;rack->box.size=rackSize;rackParent->addChild(rack);
  group->ejectAction();
  rackParent->removeChild(rack);rack->box.pos=Vec();rack->box.size=Vec(right+20,404);
  capture(rack,output+"/28-ejected-nearby.png",scale,false);
  rack->box.pos=rackPosition;rack->box.size=rackSize;rackParent->addChild(rack);
  rack->clear();


  // The narrow breakpoint is common for small abstractions: all eight
  // parameters must remain accessible with one input and one output.
  struct NarrowTile : widget::Widget {
    int hp=0;
    void draw(const DrawArgs& args) override {
      nvgBeginPath(args.vg);nvgRect(args.vg,0,0,box.size.x,box.size.y);nvgFillColor(args.vg,nvgRGB(222,226,223));nvgFill(args.vg);
      auto font=APP->window->loadFont(asset::system("res/fonts/DejaVuSans.ttf"));nvgFontFaceId(args.vg,font->handle);nvgFontSize(args.vg,12);nvgFillColor(args.vg,nvgRGB(32,48,40));
      std::string label=std::to_string(hp)+" HP";nvgText(args.vg,10,18,label.c_str(),nullptr);widget::Widget::draw(args);
    }
  };
  auto* narrowSelection=add(gainModel,Vec(30000,0));
  widget::Widget narrowSheet;narrowSheet.box.size=Vec(1000,414);
  std::vector<beekapoo::Beekapoo*> narrowHosts;
  for(int hp=8;hp<=12;++hp){
    float base=hp*2000.f;auto* voice=add(gainModel,Vec(base,0));
    auto* wrapper=static_cast<beekapoo::Beekapoo*>(add(modelBeekapooPreview,Vec(base+300,0)));
    rack->deselectAll();rack->select(voice,true);wrapper->collapse();
    for(int i=0;i<8;++i){wrapper->exposeParameter(voice->module->id,i+4);voice->module->params[i+4].setValue((i+1)*.2f);}
    wrapper->expose(voice,voice->getInput(0));wrapper->expose(voice,voice->getOutput(0));
    rack->select(narrowSelection,true);
    wrapper->requestedWidth=hp*15;wrapper->layoutControls();wrapper->step();
    require(wrapper->addSelectionButton->visible,"Add button must appear for an eligible outside selection");
    require(wrapper->box.size.x==hp*15,"Narrow 8-param layout must preserve requested width");
    // Two wrapped rows, each including its own 18 px button and 11 px CV jack.
    require(wrapper->ownFrame->box.size.y<=beekapoo::Beekapoo::ownControlLayout(wrapper->box.size.x).height+14,"Top controls must fit their compact action layout");
    for(auto* action:wrapper->actionButtons)if(action->visible)require(!action->iconAndText&&action->box.size.x==18&&action->box.size.y==18&&action->faceColor.r==action->faceColor.g&&action->faceColor.g==action->faceColor.b,"Random actions must be small neutral icon buttons");
    int visible=0;for(auto& param:wrapper->parameters)visible+=param.knob->visible;
    require(visible==8&&wrapper->parameters.size()==8,"Narrow layout must retain all eight mappings, with readable controls shown as space permits");
    std::set<std::string> families;
    for(const auto& param:wrapper->parameters)if(param.knob->visible)families.insert(beekapoo::paramFamily(param.knob->getParamQuantity()->getLabel()));
    require(wrapper->controlLabels.size()>=2,"Narrow layout must retain the jack labels");
    for(const auto& family:families){int matches=0;for(const auto& label:wrapper->controlLabels)if(beekapoo::paramFamily(label.text)==family)++matches;require(matches<=1,"A visible family must not duplicate its caption");}
    auto within=[](Rect child,Rect parent){return child.pos.x>=parent.pos.x && child.pos.y>=parent.pos.y && child.getRight()<=parent.getRight() && child.getBottom()<=parent.getBottom();};
    for(auto& param:wrapper->parameters)if(param.knob->visible)require(within(param.knob->box,wrapper->frameContent(wrapper->frame)),"Narrow knob must stay inside its frame");
    for(auto& port:wrapper->ports)require(within(port.face->box,wrapper->frameContent(port.input?wrapper->inputFrame:wrapper->outputFrame)),"Narrow jack must stay inside its frame");
    require(wrapper->frame->box.getBottom()<=wrapper->inputFrame->box.pos.y,"Narrow parameter and I/O blocks must meet without overlap");
    for(auto* action:wrapper->actionButtons)if(action->visible)require(action->icon>=0,"Narrow action buttons must use icons rather than clipped labels");
    auto* tile=new NarrowTile;tile->hp=hp;tile->box=Rect(Vec((hp-8)*200,0),Vec(200,414));
    rack->getModuleContainer()->removeChild(wrapper);wrapper->box.pos=Vec((200-wrapper->box.size.x)*.5f,28);tile->addChild(wrapper);narrowHosts.push_back(wrapper);
    capture(tile,output+"/narrow8-"+std::to_string(hp)+"hp.png",scale,false);
    narrowSheet.addChild(tile);
  }
  capture(&narrowSheet,output+"/narrow8-strip.png",scale,false);
  for(int i=0;i<5;++i){
    auto* wrapper=narrowHosts[i];wrapper->requestedWidth=(i+4)*15;wrapper->layoutControls();
    static_cast<NarrowTile*>(wrapper->parent)->hp=i+4;
    wrapper->box.pos=Vec((200-wrapper->box.size.x)*.5f,28);
  }
  capture(&narrowSheet,output+"/thin-title-strip.png",scale,false);
  for(auto* wrapper:narrowHosts){wrapper->parent->removeChild(wrapper);wrapper->box.pos=Vec();rack->addModule(wrapper);}
  rack->clear();

  // Sixteen real module renders at a common scale, covering sparse, skewed,
  // medium and crowded mappings. Captures are development-only artifacts.
  struct SpectrumCase {int p,in,out;float width;const char* name;};
  const SpectrumCase cases[]={
    {0,1,1,75,"Sparse / minimum"},{2,2,2,105,"Sparse / narrow"},{3,1,4,180,"Sparse / compact"},{4,4,1,300,"Sparse / readable"},
    {0,8,1,90,"Mostly inputs"},{2,1,8,150,"Mostly outputs"},{8,0,4,225,"No inputs"},{8,4,0,360,"No outputs"},
    {8,8,8,120,"Medium / minimum"},{12,4,8,210,"Medium / compact"},{16,8,4,300,"Medium / readable"},{12,12,12,480,"Medium / wide"},
    {24,16,16,150,"Large / minimum"},{24,2,24,300,"Large / output-heavy"},{24,24,2,450,"Large / input-heavy"},{24,24,24,600,"Large / wide"}
  };
  struct SpectrumTile : widget::Widget {
    std::string caption;
    void draw(const DrawArgs& args) override {
      nvgBeginPath(args.vg);nvgRect(args.vg,0,0,box.size.x,box.size.y);nvgFillColor(args.vg,nvgRGB(222,226,223));nvgFill(args.vg);
      auto font=APP->window->loadFont(asset::system("res/fonts/DejaVuSans.ttf"));nvgFontFaceId(args.vg,font->handle);nvgFontSize(args.vg,10);nvgFillColor(args.vg,nvgRGB(32,48,40));
      auto separator=caption.find(" | ");
      nvgText(args.vg,8,12,caption.substr(0,separator).c_str(),nullptr);
      if(separator!=std::string::npos)nvgText(args.vg,8,25,caption.substr(separator+3).c_str(),nullptr);
      widget::Widget::draw(args);
    }
  } sheet;
  sheet.box.size=Vec(2480,1680);
  struct SpectrumEntry {SpectrumTile* tile;beekapoo::Beekapoo* host;Vec position;};
  std::vector<SpectrumEntry> entries;
  std::string previousName=gainModel->name;gainModel->name="Voice";
  auto inside=[](Rect b,Rect a) {return b.pos.x>=a.pos.x-.01f && b.pos.y>=a.pos.y-.01f && b.getRight()<=a.getRight()+.01f && b.getBottom()<=a.getBottom()+.01f;};
  for(int index=0;index<16;++index) {
    const auto& c=cases[index];float base=index*2000.f;
    auto* left=add(gainModel,Vec(base,0));auto* right=add(gainModel,Vec(base+105,0));
    auto* wrapper=static_cast<beekapoo::Beekapoo*>(add(modelBeekapooPreview,Vec(base+300,0)));
    rack->deselectAll();rack->select(left,true);rack->select(right,true);wrapper->collapse();
    for(int n=0;n<c.p;++n) {auto* source=n%2?right:left;wrapper->exposeParameter(source->module->id,n/2);auto* q=source->module->getParamQuantity(n/2);float value=q->getMinValue()+(q->getMaxValue()-q->getMinValue())*((n*7+index*3)%17)/16.f;if(q->snapEnabled)value=std::round(value);q->setImmediateValue(value);}
    for(int n=0;n<c.in;++n)wrapper->expose(left,left->getInput(n));
    for(int n=0;n<c.out;++n)wrapper->expose(right,right->getOutput(n));
    wrapper->requestedWidth=c.width;wrapper->layoutControls();wrapper->step();
    for(auto* button:wrapper->actionButtons)if(button->visible)require(inside(button->box,wrapper->frameContent(wrapper->ownFrame)),("Spectrum "+std::to_string(index+1)+": action outside controls face").c_str());
    for(auto* button:wrapper->managementButtons)if(button->visible)require(inside(button->box,wrapper->frameContent(wrapper->titleFrame)),("Spectrum "+std::to_string(index+1)+": management outside header face").c_str());
    for(int i:{0,1,2,3,4,5,beekapoo::Module::GENERATE_INPUT,beekapoo::Module::GROW_INPUT})if(wrapper->getInput(i)->visible)require(inside(wrapper->getInput(i)->box,wrapper->frameContent(wrapper->ownFrame)),("Spectrum "+std::to_string(index+1)+": CV outside controls face").c_str());
    for(const auto& p:wrapper->parameters)if(p.knob->visible)require(inside(p.knob->box,wrapper->frameContent(wrapper->frame)),("Spectrum "+std::to_string(index+1)+": parameter extends into a perspective wall").c_str());
    for(const auto& p:wrapper->ports)require(inside(p.face->box,wrapper->frameContent(p.input?wrapper->inputFrame:wrapper->outputFrame)),("Spectrum "+std::to_string(index+1)+": jack extends into a perspective wall").c_str());
    auto* tile=new SpectrumTile;tile->box=Rect(Vec((index%4)*620,(index/4)*420),Vec(620,420));
    tile->caption=std::to_string(index+1)+". "+c.name+" | "+std::to_string(c.p)+" params / "+std::to_string(c.in)+" in / "+std::to_string(c.out)+" out | width "+std::to_string(int(wrapper->box.size.x));
    auto position=wrapper->box.pos;rack->getModuleContainer()->removeChild(wrapper);wrapper->box.pos=Vec((620-wrapper->box.size.x)*.5f,30);tile->addChild(wrapper);entries.push_back({tile,wrapper,position});
    capture(tile,output+"/spectrum-"+std::to_string(index+1)+".png",scale,false);
    sheet.addChild(tile);
  }
  float columnWidths[]={340,340,340,340};
  for(size_t i=0;i<entries.size();++i)columnWidths[i%4]=std::max(columnWidths[i%4],entries[i].host->box.size.x+24);
  float columnOffsets[]={0,columnWidths[0],columnWidths[0]+columnWidths[1],columnWidths[0]+columnWidths[1]+columnWidths[2]};
  sheet.box.size.x=columnOffsets[3]+columnWidths[3];
  for(size_t i=0;i<entries.size();++i) {
    entries[i].tile->box.pos.x=columnOffsets[i%4];entries[i].tile->box.size.x=columnWidths[i%4];
    entries[i].host->box.pos.x=(columnWidths[i%4]-entries[i].host->box.size.x)*.5f;
  }
  capture(&sheet,output+"/spectrum-16.png",scale,false);
  struct SpectrumRow : widget::Widget {
    widget::Widget* sheet;int row;
    void draw(const DrawArgs& args) override {nvgSave(args.vg);nvgTranslate(args.vg,0,-row*420);auto local=args;local.clipBox.pos.y+=row*420;sheet->draw(local);nvgRestore(args.vg);}
  } row;
  row.sheet=&sheet;row.box.size=Vec(sheet.box.size.x,420);
  for(int r=0;r<4;++r) {row.row=r;capture(&row,output+"/spectrum-row-"+std::to_string(r+1)+".png",scale,false);}
  for(auto& entry:entries) {entry.tile->removeChild(entry.host);entry.host->box.pos=entry.position;rack->addModule(entry.host);}
  rack->clear();gainModel->name=previousName;

  // Regression probes for exposure history, presets, native menu isolation,
  // and save lifecycle. These use the same original engine objects as Rack.
  auto* reviewA=add(gainModel,Vec(0,0));
  auto* reviewB=add(gainModel,Vec(90,0));
  auto* reviewHost=static_cast<beekapoo::Beekapoo*>(add(modelBeekapooPreview,Vec(300,0)));
  connect(reviewA,reviewB);
  rack->deselectAll();rack->select(reviewA,true);rack->select(reviewB,true);reviewHost->collapse();reviewHost->step();
  APP->engine->prepareSave();
  require(static_cast<beekapoofixture::Gain*>(reviewA->module)->saveCalls==1 && static_cast<beekapoofixture::Gain*>(reviewB->module)->saveCalls==1,"Each child must receive exactly one callback per patch save");
  int exposureHistory=APP->history->actionIndex;
  reviewHost->toggleParameter(reviewA->getParam(0));
  require(APP->history->actionIndex==exposureHistory+1 && reviewHost->exposed(reviewA->getParam(0)),"Expose parameter must create one undo action");
  APP->history->undo();require(!reviewHost->exposed(reviewA->getParam(0)),"Undo exposing parameter failed");
  APP->history->redo();require(reviewHost->exposed(reviewA->getParam(0)),"Redo exposing parameter failed");
  reviewHost->toggleParameter(reviewA->getParam(0));APP->history->undo();
  require(reviewHost->exposed(reviewA->getParam(0)),"Undo unexposing parameter failed");
  reviewHost->togglePort(reviewA->getInput(4));
  APP->history->undo();require(!reviewHost->exposed(reviewA->getInput(4)),"Undo exposing jack failed");
  APP->history->redo();require(reviewHost->exposed(reviewA->getInput(4)),"Redo exposing jack failed");
  reviewHost->exposeParameterAction(reviewA->module->id,3);APP->history->undo();
  require(!reviewHost->exposed(reviewA->getParam(3)),"Context-menu exposure must also support undo");
  reviewHost->exposeParameterAction(reviewA->module->id,8);
  bool hiddenParameterMapped=false;for(auto& param:reviewHost->parameters)hiddenParameterMapped|=param.id==reviewA->module->id && param.paramId==8;
  require(hiddenParameterMapped,"Context-menu exposure must support parameters without their own source widget");
  APP->history->undo();
  reviewHost->exposePortAction(reviewA->module->id,7,true);APP->history->undo();
  require(!reviewHost->exposed(reviewA->getInput(7)),"Context-menu port exposure must also support undo");
  rack->deselectAll();rack->select(reviewHost,true);
  {
    ui::Menu unrelated;
    bool ran=false;auto* customDelete=createMenuItem("Delete","",[&] {ran=true;});unrelated.addChild(customDelete);
    auto* submenu=new ui::Menu;unrelated.addChild(submenu);submenu->addChild(createMenuItem("Duplicate","",[] {}));
    hooks->wrapMenus(&unrelated);
    require(unrelated.children.front()==customDelete,"Custom Delete menu must not be intercepted");
    widget::Widget::ActionEvent action;customDelete->onAction(action);require(ran,"Custom Delete action must remain intact");
    ui::Menu native;
    auto* prefix=createMenuItem("Before selection","",[] {});native.addChild(prefix);
    rack->appendSelectionContextMenu(&native);
    auto* suffix=createMenuItem("After selection","",[] {});native.addChild(suffix);
    hooks->wrapMenus(&native);
    require(native.children.front()==prefix && native.children.back()==suffix,"Menu wrapping must leave unrelated prefix and suffix actions alone");
    int removes=0,duplicates=0;
    for(auto* child:native.children){removes+=dynamic_cast<beekapoo::GroupDeleteMenuItem*>(child)!=nullptr;duplicates+=dynamic_cast<beekapoo::GroupDuplicateMenuItem*>(child)!=nullptr;}
    require(removes==1 && duplicates==2,"Only Rack's native selection actions should be wrapped");
  }
  rack->deselectAll();reviewHost->step();
  require(!reviewHost->addSelectionButton->visible,"Add button must hide without an eligible selection");
  auto* selectedExtra=add(gainModel,Vec(900,0));auto selectedExtraId=selectedExtra->module->id;
  rack->select(selectedExtra,true);reviewHost->step();
  require(reviewHost->addSelectionButton->visible,"Add button must react to selection changes without resizing");
  reviewHost->addSelectionButton->action();reviewHost->step();
  require(reviewHost->members.size()==3 && !reviewHost->addSelectionButton->visible,"Add button must add the selection and then hide");
  APP->history->undo();reviewHost->step();
  require(reviewHost->members.size()==2 && rack->getModule(selectedExtraId)->visible,"Add button must preserve native group undo");
  auto* preset=reviewHost->toJson();
  auto* presetTarget=static_cast<beekapoo::Beekapoo*>(add(modelBeekapooPreview,Vec(600,0)));
  rack->deselectAll();rack->select(rack->getModule(selectedExtraId),true);presetTarget->step();
  require(presetTarget->addSelectionButton->visible,"An empty Beekapoo must offer adding selected modules");
  presetTarget->addSelectionButton->action();presetTarget->step();
  require(presetTarget->members.size()==1,"Add button must also create an initial group");
  APP->history->undo();presetTarget->step();
  require(presetTarget->members.empty() && rack->getModule(selectedExtraId)->visible,"Undo adding to an empty group must release the selection");
  size_t beforePreset=APP->engine->getModuleIds().size();auto beforePresetCables=cableIds();
  auto applyPreset=[&] {auto* copy=json_deep_copy(preset);presetTarget->pasteJsonAction(copy);json_decref(copy);presetTarget->step();};
  applyPreset();
  require(presetTarget->members.size()==2 && APP->engine->getModuleIds().size()==beforePreset+2,"Preset must import the complete group");
  std::set<int64_t> firstImport;for(auto& member:presetTarget->members)firstImport.insert(member.id);
  for(int repeat=0;repeat<3;++repeat){
    APP->history->undo();presetTarget->step();
    require(presetTarget->members.empty() && APP->engine->getModuleIds().size()==beforePreset && cableIds()==beforePresetCables,"Preset Undo must remove imported children and wiring");
    APP->history->redo();presetTarget->step();
    std::set<int64_t> restored;for(auto& member:presetTarget->members)restored.insert(member.id);
    require(restored==firstImport && APP->engine->getModuleIds().size()==beforePreset+2,"Preset Redo must restore the same child IDs without duplication");
  }
  // Replacement of an existing group must be one reversible transaction too.
  auto* oldChild=presetTarget->members.front().widget.get();auto oldChildId=oldChild->module->id;
  oldChild->module->params[0].setValue(.42f);
  auto* incoming=connect(reviewA,oldChild);auto incomingId=incoming->cable->id;
  presetTarget->step();applyPreset();
  require(!rack->getModule(oldChildId) && !APP->engine->getCable(incomingId) && APP->engine->getModuleIds().size()==beforePreset+2,"Replacing a preset must remove its former contents");
  for(int repeat=0;repeat<3;++repeat){
    APP->history->undo();presetTarget->step();
    require(presetTarget->members.size()==2 && rack->getModule(oldChildId) && APP->engine->getCable(incomingId),"Preset Undo must restore old members and boundary wiring");
    require(std::abs(rack->getModule(oldChildId)->module->params[0].getValue()-.42f)<.0001f,"Preset Undo must restore old parameter values");
    APP->history->redo();presetTarget->step();
    require(!rack->getModule(oldChildId) && APP->engine->getModuleIds().size()==beforePreset+2,"Preset Redo must not leak the former group");
  }
  for(int failure=0;failure<3;++failure) {
    auto* broken=json_deep_copy(preset);
    auto* children=json_object_get(json_object_get(broken,"data"),"contents");
    auto* child=json_array_get(json_object_get(children,"modules"),1);
    if(failure==0)json_object_set_new(child,"model",json_string("__missing_audit_model__"));
    if(failure==1){auto* data=json_object();json_object_set_new(data,"failLoad",json_true());json_object_set_new(child,"data",data);}
    if(failure==2)json_object_set(child,"sourceId",json_object_get(json_array_get(json_object_get(children,"modules"),0),"sourceId"));
    auto beforeCount=APP->engine->getModuleIds().size();auto beforeWires=cableIds();
    std::set<int64_t> beforeChildren;for(auto& member:presetTarget->members)beforeChildren.insert(member.id);
    presetTarget->pasteJsonAction(broken);json_decref(broken);presetTarget->step();
    std::set<int64_t> afterChildren;for(auto& member:presetTarget->members)afterChildren.insert(member.id);
    require(afterChildren==beforeChildren && cableIds()==beforeWires && APP->engine->getModuleIds().size()==beforeCount,"Failed preset loads must preserve the original group, wires and engine modules");
    APP->history->undo();presetTarget->step();
    APP->history->redo();presetTarget->step();
    require(presetTarget->members.size()==beforeChildren.size() && cableIds()==beforeWires,"Undo/Redo of a failed preset must not orphan or remove the original group");
  }
  json_decref(preset);rack->clear();
  {
    auto* target=add(gainModel,Vec(0,0));auto* map=add(mapperModel,Vec(90,0));
    auto* mapper=static_cast<beekapoofixture::Mapper*>(map->module);
    APP->engine->updateParamHandle(&mapper->handle,target->module->id,0);
    auto* owner=static_cast<beekapoo::Beekapoo*>(add(modelBeekapooPreview,Vec(240,0)));
    rack->select(target,true);rack->select(map,true);owner->collapse();owner->duplicateAction(false);
    beekapoo::Beekapoo* copied=nullptr;for(auto* w:rack->getModules())if(auto* group=dynamic_cast<beekapoo::Beekapoo*>(w))if(group!=owner)copied=group;
    require(copied,"Mapper duplicate must create a complete group");copied->step();
    beekapoofixture::Mapper* copiedMapper=nullptr;engine::Module* copiedTarget=nullptr;
    for(auto& member:copied->members)if(auto* m=dynamic_cast<beekapoofixture::Mapper*>(member.widget->module))copiedMapper=m;else copiedTarget=member.widget->module;
    require(copiedMapper && copiedMapper->handle.module==copiedTarget && mapper->handle.module==target->module,"Copied mapper must bind only to the copied target without stealing the original mapping");
    mapper->params[0].setValue(.25f);copiedMapper->params[0].setValue(.75f);APP->engine->stepBlock(64);
    require(std::abs(target->module->params[0].getValue()-.25f)<.001f && std::abs(copiedTarget->params[0].getValue()-.75f)<.001f,"Copied mapper must drive its own parameters independently");
    auto copyId=copied->module->id;APP->history->undo();APP->history->redo();copied=dynamic_cast<beekapoo::Beekapoo*>(rack->getModule(copyId));copied->step();
    for(auto& member:copied->members)if(auto* m=dynamic_cast<beekapoofixture::Mapper*>(member.widget->module))require(copied->contains(m->handle.module),"Mapper targets must stay internal after duplicate Undo/Redo");
    rack->clear();
  }

  // Native bypass must affect the contained engine modules, including
  {
    auto* original=add(gainModel,Vec(0,0));auto originalId=original->module->id;
    auto* owner=static_cast<beekapoo::Beekapoo*>(add(modelBeekapooPreview,Vec(180,0)));
    rack->select(original,true);owner->collapse();auto* saved=owner->toJson();
    original->module->params[0].setValue(.19f);
    owner->pasteJsonAction(saved);owner->step();
    require(owner->members.size()==1 && owner->members[0].id!=originalId && !rack->getModule(originalId),"Same-instance preset recall must replace children instead of reusing live settings");
    require(std::abs(owner->members[0].widget->module->params[0].getValue()-1.f)<.001f,"Same-instance preset must restore saved child parameters");
    APP->history->undo();owner->step();
    require(owner->members[0].id==originalId && std::abs(owner->members[0].widget->module->params[0].getValue()-.19f)<.001f,"Same-instance preset Undo must recover the prior live child and its settings");
    APP->history->redo();owner->step();
    require(owner->members.size()==1 && owner->members[0].id!=originalId,"Same-instance preset Redo must restore the replacement without orphaning originals");
    json_decref(saved);rack->clear();
  }
  // Native bypass must affect the contained engine modules, including
  // native routing, callbacks, mixed child states and one-step history.
  auto* bypassDriver=add(gainModel,Vec(0,0));
  auto* bypassA=add(gainModel,Vec(90,0));auto* bypassB=add(gainModel,Vec(180,0));
  auto* bypassHost=static_cast<beekapoo::Beekapoo*>(add(modelBeekapooPreview,Vec(330,0)));
  bypassA->module->configBypass(0,0);bypassB->module->configBypass(0,0);
  bypassA->module->params[0].setValue(2.f);bypassB->module->params[0].setValue(2.f);
  connect(bypassDriver,bypassA);connect(bypassA,bypassB);auto bypassCables=cableIds();
  rack->deselectAll();rack->select(bypassA,true);rack->select(bypassB,true);bypassHost->collapse();
  APP->engine->stepBlock(64);
  require(bypassB->module->outputs[0].getVoltage(3)==16.f,"Bypass fixture must start with processed polyphonic audio");
  APP->engine->bypassModule(bypassA->module,true);
  bypassHost->bypassAction(true);bypassHost->step();
  require(bypassA->module->isBypassed() && bypassB->module->isBypassed() && !bypassDriver->module->isBypassed(),"Bypass must affect every child and no outside modules");
  APP->engine->stepBlock(64);
  require(bypassB->module->outputs[0].getVoltage(3)==4.f && cableIds()==bypassCables,"Group bypass must use native polyphonic bypass routing without changing cables");
  bypassHost->bypassAction(false);bypassHost->step();
  require(!bypassA->module->isBypassed() && !bypassB->module->isBypassed(),"Un-bypass must enable all children, including a previously bypassed child");
  APP->history->undo();bypassHost->step();require(bypassA->module->isBypassed() && bypassB->module->isBypassed(),"Native bypass Undo must affect the full group");
  APP->history->redo();bypassHost->step();require(!bypassA->module->isBypassed() && !bypassB->module->isBypassed(),"Native bypass Redo must affect the full group");
  bypassHost->bypassAction(true);bypassHost->step();
  auto* bypassJson=bypassHost->toJson();
  APP->engine->bypassModule(bypassB->module,false);
  bypassHost->fromJson(bypassJson);json_decref(bypassJson);bypassHost->step();
  require(bypassHost->module->isBypassed() && bypassB->module->isBypassed(),"Reloaded group bypass must synchronize its children");
  auto* addedBypassed=add(gainModel,Vec(600,0));rack->deselectAll();rack->select(addedBypassed,true);bypassHost->addSelection();bypassHost->step();
  require(addedBypassed->module->isBypassed(),"New members of a bypassed group must also be bypassed");
  auto bypassPosition=bypassHost->box.pos;rack->getModuleContainer()->removeChild(bypassHost);bypassHost->box.pos=Vec();
  capture(bypassHost,output+"/24-bypassed-group.png",scale,false);bypassHost->box.pos=bypassPosition;rack->addModule(bypassHost);
  bypassHost->editSubpatch();bypassHost->bypassAction(false);bypassHost->step();
  require(!bypassA->module->isBypassed() && !bypassB->module->isBypassed() && !addedBypassed->module->isBypassed(),"Bypass must also work with the subpatch open in Rack");
  rack->clear();
  std::puts("Beekapoo native group bypass, un-bypass, undo/redo, reload, new members and four-channel routing passed");

  // Unified editing: additions can stay outside; placement preserves a 2D
  // layout; Eject releases the whole instrument and removes only its owner.
  auto* jointA=add(gainModel,Vec(0,0));auto* jointB=add(gainModel,Vec(90,0));
  auto* jointHost=static_cast<beekapoo::Beekapoo*>(add(modelBeekapooPreview,Vec(330,0)));
  auto jointHostId=jointHost->module->id;
  connect(jointA,jointB);auto jointCables=cableIds();
  rack->deselectAll();rack->select(jointA,true);rack->select(jointB,true);jointHost->collapse();jointHost->editSubpatch();
  auto* joint=static_cast<beekapoo::RackEditSession*>(jointHost->editSession.get());
  auto requireCentered=[&] {
    Rect bounds;bool first=true;
    for(auto& member:jointHost->members) {
      auto box=member.widget->box;
      if(first){bounds=box;first=false;}else bounds=bounds.expand(box);
    }
    auto* scroll=APP->scene->rackScroll;
    Vec topLeft=scroll->getGridOffset()*RACK_GRID_SIZE+RACK_OFFSET;
    Vec screenCenter=(bounds.getCenter()-topLeft)*scroll->getZoom();
    auto* editing=static_cast<beekapoo::RackEditSession*>(jointHost->editSession.get());
    Vec expected(scroll->box.size.x*.5f,(scroll->box.size.y+editing->toolbarHeight)*.5f);
    require((screenCenter-expected).norm()<1.f,"Edit and Near/Far must center the complete subpatch below the toolbar");
  };
  requireCentered();
  require(joint->showMembers && joint->membersButton->pressed(),"Subpatch member logos must be enabled by default");
  joint->membersButton->action();
  auto* markerState=jointHost->editSessionState();
  require(!joint->showMembers && json_is_false(json_object_get(markerState,"members")),"Member logo toggle must be saved with the edit session");
  json_decref(markerState);joint->membersButton->action();
  require(joint->panelButton->help()=="Show Beekapoo panel" && joint->done->help().find("[esc]")!=std::string::npos,"Edit toolbar tooltips must explain Panel and Done's shortcut");
  joint->layoutToolbar(520);
  require(joint->toolbarHeight>52,"Larger edit controls must wrap in narrow windows");
  joint->layoutToolbar(APP->scene->rackScroll->box.size.x);
  require(jointHost->managementButtons.size()==2 && jointHost->managementButtons[1]->text=="Eject","Panel must have only Edit and Eject management buttons");
  joint->newModulesButton->action();require(!jointHost->captureNewModules,"New module destination toggle must select Outside");
  auto* stayOutside=add(gainModel,Vec(1200,4*RACK_GRID_HEIGHT));joint->step();
  require(!jointHost->contains(stayOutside->module),"Modules added in Outside mode must remain in the main patch");
  joint->newModulesButton->action();joint->step();
  require(!jointHost->contains(stayOutside->module),"Switching Inside must not capture existing outside modules");
  auto* jointC=add(gainModel,jointB->box.pos+Vec(150,0));joint->step();
  require(jointHost->contains(jointC->module),"New modules must join when Inside is selected");
  rack->setModulePosNearest(jointB,jointB->box.pos+Vec(0,RACK_GRID_HEIGHT));
  Vec relativeB=jointB->box.pos-jointA->box.pos,relativeC=jointC->box.pos-jointA->box.pos;
  joint->nearButton->action();
  requireCentered();
  require(jointA->box.pos.y<2*RACK_GRID_HEIGHT && jointB->box.pos-jointA->box.pos==relativeB && jointC->box.pos-jointA->box.pos==relativeC,"Near must preserve the group's relative arrangement");
  APP->history->undo();jointHost->step();
  joint=static_cast<beekapoo::RackEditSession*>(jointHost->editSession.get());
  require(joint && jointA->box.pos.y>=4*RACK_GRID_HEIGHT,"Near Undo must restore the previous position and keep editing");
  auto* leftBlock=add(gainModel,jointHost->box.pos-Vec(6000,0));
  leftBlock->box.size=Vec(6000,2*RACK_GRID_HEIGHT);
  auto* rightBlock=add(gainModel,Vec(jointHost->box.getRight(),jointHost->box.pos.y));
  rightBlock->box.size=Vec(6000,2*RACK_GRID_HEIGHT);
  jointHost->moveEditedMembers(true,false);
  require(jointA->box.pos.y<jointHost->box.pos.y,"Near must use available space above instead of pushing far to the side");
  auto* aboveBlock=add(gainModel,jointHost->box.pos-Vec(6000,2*RACK_GRID_HEIGHT));
  aboveBlock->box.size=Vec(12000,2*RACK_GRID_HEIGHT);
  jointHost->moveEditedMembers(true,false);
  require(jointA->box.pos.y>=jointHost->box.getBottom() && jointA->box.pos.x==jointHost->box.pos.x,"Near must use space below when the other sides are occupied");
  require(jointB->box.pos-jointA->box.pos==relativeB && jointC->box.pos-jointA->box.pos==relativeC,"Vertical Near placement must preserve the group's arrangement");
  for(auto* blocker:{leftBlock,rightBlock,aboveBlock}) {rack->removeModule(blocker);delete blocker;}
  joint->farButton->action();
  requireCentered();
  require(jointB->box.pos-jointA->box.pos==relativeB && jointC->box.pos-jointA->box.pos==relativeC,"Far must preserve the group's relative arrangement");
  joint->newModulesButton->action();require(!jointHost->captureNewModules,"Outside preference selected before closing");
  std::map<int64_t,Vec> kept;for(auto& member:jointHost->members)kept[member.id]=member.widget->box.pos;
  joint->done->action();jointHost->editSubpatch();
  joint=static_cast<beekapoo::RackEditSession*>(jointHost->editSession.get());
  for(auto& member:jointHost->members)require(member.widget->box.pos==kept.at(member.id),"Reopening must retain the native subpatch layout");
  require(!jointHost->captureNewModules,"New-module destination must persist across editing sessions");
  size_t beforeEject=APP->engine->getModuleIds().size();
  WeakPtr<beekapoo::Beekapoo> ejected=jointHost;
  widget::EventContext ejectContext;widget::Widget::ButtonEvent ejectClick;
  ejectClick.context=&ejectContext;ejectClick.action=GLFW_PRESS;ejectClick.button=GLFW_MOUSE_BUTTON_LEFT;
  jointHost->managementButtons[1]->onButton(ejectClick);
  require(!ejected && !rack->getModule(jointHostId) && APP->engine->getModuleIds().size()==beforeEject-1,"Eject button must delete only Beekapoo without a dangling button event");
  require(jointA->visible && jointB->visible && jointC->visible && cableIds()==jointCables,"Eject must release all modules and preserve their wiring");
  for(int repeat=0;repeat<2;++repeat){
    APP->history->undo();jointHost=dynamic_cast<beekapoo::Beekapoo*>(rack->getModule(jointHostId));jointHost->step();
    require(jointHost && jointHost->members.size()==3 && jointHost->editSession && cableIds()==jointCables,"One Eject Undo must restore the complete editing group");
    APP->history->redo();
    require(!rack->getModule(jointHostId) && APP->engine->getModuleIds().size()==beforeEject-1 && cableIds()==jointCables,"Eject Redo must keep released contents alive");
  }
  APP->history->undo();jointHost=dynamic_cast<beekapoo::Beekapoo*>(rack->getModule(jointHostId));jointHost->step();
  static_cast<beekapoo::RackEditSession*>(jointHost->editSession.get())->done->action();
  jointHost->ejectAction();APP->history->undo();jointHost=dynamic_cast<beekapoo::Beekapoo*>(rack->getModule(jointHostId));jointHost->step();
  require(jointHost && jointHost->members.size()==3 && !jointHost->expanded && cableIds()==jointCables,"Collapsed Eject Undo must restore the collapsed group");
  rack->clear();
  std::puts("Unified Edit, Inside/Outside adoption, Near/Far placement, saved layout, Eject button and atomic Undo/Redo passed");

  for(int variant=0;variant<6;++variant) {
    auto* first=add(gainModel,Vec(0,0));auto* second=add(gainModel,Vec(90,variant==5?RACK_GRID_HEIGHT:0));
    auto firstId=first->module->id,secondId=second->module->id;
    Vec relative=second->box.pos-first->box.pos;
    auto* owner=static_cast<beekapoo::Beekapoo*>(add(modelBeekapooPreview,Vec(600,2*RACK_GRID_HEIGHT)));
    rack->select(first,true);rack->select(second,true);owner->collapse();
    // Preserve the deliberately edited two-row fixture after initial packing.
    if(variant==5){for(auto& member:owner->members)if(member.id==secondId)member.position=Vec(90,RACK_GRID_HEIGHT);owner->saveGroup();}
    owner->requestedWidth=variant==0||variant==5?300:75;owner->layoutControls();
    auto ownerId=owner->module->id;Rect oldSpace=owner->box;
    std::map<int64_t,Rect> obstacles;
    auto wall=[&](Vec pos,Vec size) {auto* w=add(gainModel,pos);w->box.size=size;obstacles[w->module->id]=w->box;};
    if(variant==2||variant==3)wall(oldSpace.pos-Vec(6000,0),Vec(6000,RACK_GRID_HEIGHT));
    if(variant>=2 && variant<=4)wall(Vec(oldSpace.getRight(),oldSpace.pos.y),Vec(6000,RACK_GRID_HEIGHT));
    if(variant==3||variant==4)wall(oldSpace.pos-Vec(6000,RACK_GRID_HEIGHT),Vec(12000,RACK_GRID_HEIGHT));
    if(variant==4)wall(oldSpace.pos+Vec(-6000,RACK_GRID_HEIGHT),Vec(12000,RACK_GRID_HEIGHT));
    int64_t actionCable=-1;
    if(variant==0) {auto* driver=add(gainModel,Vec(5000,8*RACK_GRID_HEIGHT));actionCable=connect(driver,owner)->cable->id;}
    owner->ejectAction();
    Rect a=rack->getModule(firstId)->box,b=rack->getModule(secondId)->box;
    require(!rack->getModule(ownerId) && b.pos-a.pos==relative,"Eject must release one cohesive block and preserve its arrangement");
    if(actionCable>=0)require(!APP->engine->getCable(actionCable),"Eject must remove cables connected to the deleted owner's action inputs");
    if(variant<=1||variant==5)require(a.pos==oldSpace.pos,"Eject must reuse its own position when the complete group fits there");
    if(variant==2)require(a.pos.y==oldSpace.pos.y-RACK_GRID_HEIGHT,"Eject must use the closest free row above");
    if(variant==3)require(a.pos.y==oldSpace.getBottom(),"Eject must use the closest free row below when above is occupied");
    if(variant==4)require(a.pos.y==oldSpace.pos.y && a.pos.x<oldSpace.pos.x,"Eject must use nearby space to the left when other directions are blocked");
    for(auto& obstacle:obstacles) {
      auto current=rack->getModule(obstacle.first)->box;
      require(current.pos==obstacle.second.pos && current.size==obstacle.second.size && !a.intersects(current) && !b.intersects(current),"Eject must not overlap or move unrelated modules");
    }
    APP->history->undo();owner=dynamic_cast<beekapoo::Beekapoo*>(rack->getModule(ownerId));owner->step();
    require(owner->members.size()==2,"Placement Undo must recover the complete abstraction");
    if(actionCable>=0)require(APP->engine->getCable(actionCable),"Eject Undo must also recover cables to the owner's own inputs");
    APP->history->redo();
    require(rack->getModule(firstId)->box.pos==a.pos && rack->getModule(secondId)->box.pos==b.pos,"Placement Redo must restore the same nearby positions");
    rack->clear();
  }
  std::puts("Eject current-space reuse, above/below/left placement, intact layout, obstacle preservation and Undo/Redo passed");

  {
    auto* source=add(gainModel,Vec());
    auto* panel=static_cast<beekapoo::Beekapoo*>(add(modelBeekapooPreview,Vec(150,0)));
    rack->deselectAll();rack->select(source,true);panel->collapse();
    for(int id:{0,5,1,2})panel->exposeParameter(source->module->id,id);
    panel->expose(source,source->getInput(0));panel->expose(source,source->getOutput(0));
    panel->requestedWidth=240;panel->layoutControls();
    auto* parent=panel->parent;parent->removeChild(panel);panel->box.pos=Vec();
    capture(panel,output+"/36-discrete-dot-knob.png",scale,false);
    parent->addChild(panel);rack->clear();
  }

  testBeekCombine(add,require,gainModel);
  testBeekPatchFiles(output,scale,add,require,gainModel);
  testBeekGenerator(output,scale,add,require);
  std::printf("Beekapoo: zero footprint, occupied-space expansion, native save/load, four-channel DSP, exposed controls, copy isolation, missing-member recovery and deletion checks passed.\n");
}
