#pragma once
inline void checkVolyKeyboard(rack::plugin::Model* model, std::function<void(rack::app::ModuleWidget*)> screenshot = {}){
  using namespace rack;
  auto require=[](bool ok,const char* why){if(!ok)throw std::runtime_error(why);};
  auto* m=model->createModule();APP->engine->addModule(m);
  auto w=std::unique_ptr<app::ModuleWidget>(model->createModuleWidget(m));
  auto param=[&](const char* name){for(int i=0;i<int(m->params.size());++i)if(m->getParamQuantity(i)->name==name || (m->getParamQuantity(i)->name.size()>std::strlen(name) && m->getParamQuantity(i)->name.compare(m->getParamQuantity(i)->name.size()-std::strlen(name),std::strlen(name),name)==0))return i;throw std::runtime_error("Missing Voly selector");};
  int band=param("Output"),channel=param("Channel"),scale=param("Scale"),offset=param("Offset");
  auto key=[&](int code,int mods=0,int action=GLFW_PRESS){widget::EventContext context;widget::Widget::HoverKeyEvent event;event.context=&context;event.key=code;event.mods=mods;event.action=action;w->onHoverKey(event);};
  for(int i=0;i<16;++i){key(GLFW_KEY_A+i);require(m->params[band].getValue()==i+1&&m->params[channel].getValue()==0,"Band keyboard focus matches button selection");}
  const char* keys="1234567890QWRTYU";
  for(int i=0;i<16;++i){key(keys[i]);require(m->params[channel].getValue()==i+1&&m->params[band].getValue()==0,"Channel keyboard focus matches button selection");}
  key(GLFW_KEY_B);m->params[0].setValue(3.25f);key(GLFW_KEY_3);key(GLFW_KEY_B);
  require(m->params[0].getValue()==3.25f,"Switching focus via keyboard preserves hidden knob values");
  key(GLFW_KEY_C,GLFW_MOD_CONTROL);require(m->params[band].getValue()==2,"Modified shortcuts do not change focus");
  key(GLFW_KEY_C,0,GLFW_RELEASE);require(m->params[band].getValue()==2,"Key release does not select a new focus");
  ui::TextField field;APP->event->setSelectedWidget(&field);key(GLFW_KEY_C);APP->event->setSelectedWidget(nullptr);
  require(m->params[band].getValue()==2,"Value text editing suppresses shortcuts");
  m->params[scale].setValue(.7f);m->params[offset].setValue(2.f);
  key(GLFW_KEY_Z);key(GLFW_KEY_X);
  require(m->params[scale].getValue()==.7f&&m->params[offset].getValue()==2.f,"View randomize and wiggle preserve scale and offset");
  auto* beforeW=m->toJson();key(GLFW_KEY_W,RACK_MOD_CTRL);auto* afterW=m->toJson();
  require(!json_equal(beforeW,afterW),"Mod+W wiggles current view");json_decref(beforeW);json_decref(afterW);
  require(m->params[scale].getValue()==.7f&&m->params[offset].getValue()==2.f,"Mod+W preserves scale/offset");
  require(m->params[band].getValue()==2,"Action keys preserve focus");
  ui::TextField editField;APP->event->setSelectedWidget(&editField);beforeW=m->toJson();key(GLFW_KEY_W,RACK_MOD_CTRL);afterW=m->toJson();
  require(json_equal(beforeW,afterW),"Mod+W leaves text editing alone");json_decref(beforeW);json_decref(afterW);APP->event->setSelectedWidget(nullptr);
  if(screenshot){key(GLFW_KEY_GRAVE_ACCENT);screenshot(w.get());key(GLFW_KEY_GRAVE_ACCENT,0,GLFW_RELEASE);}
  for(int i=0;i<int(m->inputs.size());++i)if(m->getInputInfo(i)->name=="Focus CV")m->inputs[i].channels=1;
  key(GLFW_KEY_C);require(m->params[band].getValue()==2,"Focus CV takes precedence over keyboard");
  std::puts("Voly Pector native keyboard selection, stored values, modifier/text guards and Focus CV precedence passed");
}
