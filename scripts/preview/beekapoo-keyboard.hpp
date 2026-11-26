#pragma once
// Exercise the same native handlers and painter used in the running Rack UI.
struct BeekKeyboardCapture : widget::Widget {
  beekapoo::Beekapoo* owner = nullptr;
  void drawLayer(const DrawArgs& args, int layer) override {
    widget::Widget::drawLayer(args, layer);
    if (layer == 1) {
      nvgSave(args.vg);
      nvgTranslate(args.vg, owner->box.pos.x, owner->box.pos.y);
      owner->paintKeyboard(args, false);
      nvgRestore(args.vg);
    }
  }
};
template<class Add, class Require>
void testBeekKeyboard(const std::string& output, float scale, Add add, Require require, plugin::Model* gainModel) {
  using beekapoo::KeyAction;
  using beekapoo::Module;
  auto* rack = APP->scene->rack;
  std::vector<app::ModuleWidget*> voices;
  for (int i = 0; i < 4; ++i) voices.push_back(add(gainModel, Vec(i * 90, 0)));
  auto* owner = static_cast<beekapoo::Beekapoo*>(add(modelBeekapooPreview, Vec(600, 0)));
  rack->deselectAll();
  for (auto* voice : voices) rack->select(voice, true);
  owner->collapse();
  for (auto* voice : voices)
    for (int p : {1, 2, 3}) owner->exposeParameter(voice->module->id, p);
  owner->requestedWidth = 330;
  owner->layoutControls();owner->step();
  auto* m = owner->getModule<Module>();
  auto key = [&](int code, int action = GLFW_PRESS, int mods = 0) {
    widget::EventContext context;
    widget::Widget::HoverKeyEvent e;
    e.context = &context; e.key = code; e.action = action; e.mods = mods;
    owner->onHoverKey(e);
  };
  require(!owner->keyboardBlocked(), "Keyboard is available over Beek");
  require(owner->shortcutButton(KeyAction::WiggleAll)==owner->actionButtons[3] && owner->shortcutButton(KeyAction::RandomAll)==owner->actionButtons[4], "Q/T address all controls");
  require(owner->shortcutButton(KeyAction::WiggleExposed)==owner->actionButtons[0] && owner->shortcutButton(KeyAction::RandomExposed)==owner->actionButtons[1], "W/R address exposed controls");
  for(auto action : {KeyAction::WiggleAll, KeyAction::WiggleExposed, KeyAction::RandomAll, KeyAction::RandomExposed}) {
    auto* button=owner->shortcutButton(action);auto original=button->action;int calls=0;
    button->action=[&]{++calls;};
    for(const auto& binding:beekapoo::keyBindings())if(binding.action==action){key(binding.key);key(binding.key,GLFW_REPEAT);}
    require(calls==1,"Random/wiggle keys dispatch once to the correct button");button->action=original;
  }
  auto before = owner->sequence.steps.size();
  key(GLFW_KEY_N);key(GLFW_KEY_N, GLFW_REPEAT);key(GLFW_KEY_N, GLFW_RELEASE);
  require(owner->sequence.steps.size()==before+1, "Save New fires once per key press");
  key(GLFW_KEY_SPACE);require(m->clockEnabled.load(), "Space enables clock");
  key(GLFW_KEY_SPACE,GLFW_REPEAT);require(m->clockEnabled.load(), "Space repeat does not toggle");
  key(GLFW_KEY_SPACE);require(!m->clockEnabled.load(), "Space disables clock");
  key(GLFW_KEY_HOME);m->process({});require(m->playingStep==0, "Home resets");
  key(GLFW_KEY_RIGHT);m->process({});require(m->playingStep==1, "Right advances");
  key(GLFW_KEY_LEFT);m->process({});require(m->playingStep==0, "Left recalls previous step");
  auto stepCount=owner->sequence.steps.size();
  key(GLFW_KEY_D);key(GLFW_KEY_D,GLFW_REPEAT);m->process({});
  require(owner->sequence.steps.size()==stepCount-1&&rack->getModule(m->id)==owner,"D deletes one step, not the Beek module");
  key(GLFW_KEY_D);require(owner->sequence.steps.size()==1,"D keeps the final step");key(GLFW_KEY_N);
  auto* text = new ui::TextField;rack->addChild(text);APP->event->setSelectedWidget(text);
  before = owner->sequence.steps.size();key(GLFW_KEY_N);key(GLFW_KEY_SPACE);
  require(owner->sequence.steps.size()==before&&!m->clockEnabled.load(), "Text entry does not trigger shortcuts");
  APP->event->setSelectedWidget(nullptr);rack->removeChild(text);delete text;
  APP->scene->browser->visible=true;key(GLFW_KEY_N);require(owner->sequence.steps.size()==before, "Browser typing is ignored");APP->scene->browser->visible=false;
  auto* menu=createMenu();key(GLFW_KEY_N);require(owner->sequence.steps.size()==before, "Modal menu blocks shortcuts");auto* overlay=menu->parent;overlay->parent->removeChild(overlay);delete overlay;
  auto buttons = owner->keyboardButtons();require(buttons.size()==12, "All exposed switches get numbered");
  int momentary=-1,toggle=-1;
  for(int i=0;i<10;++i) {if(buttons[i]->momentary)momentary=i;else if(buttons[i]->getParamQuantity()->getMaxValue()==1)toggle=i;}
  require(momentary>=0&&toggle>=0,"Test includes native momentary and toggle");
  auto number=[](int i){return i==9?GLFW_KEY_0:GLFW_KEY_1+i;};
  auto* pq=buttons[momentary]->getParamQuantity();
  key(number(momentary));require(pq->getValue()==1,"Number press holds native momentary");
  key(GLFW_KEY_RIGHT_BRACKET);require(owner->buttonKeyBank==1,"Next button bank");
  key(number(momentary),GLFW_RELEASE);
  rack->getModule(pq->module->id)->step();rack->getModule(pq->module->id)->step();
  require(pq->getValue()==0&&owner->heldButtonKeys.empty(),"Release reaches original button after bank change");
  key(GLFW_KEY_LEFT_BRACKET);require(owner->buttonKeyBank==0,"Previous button bank");
  pq=buttons[toggle]->getParamQuantity();float previous=pq->getValue();
  key(number(toggle));require(pq->getValue()!=previous,"Number toggles original switch");float toggled=pq->getValue();key(number(toggle),GLFW_REPEAT);key(number(toggle),GLFW_RELEASE);require(pq->getValue()==toggled,"Toggle fires once, not on repeat or release");
  key(number(momentary));owner->releaseButtonKeys();
  for(auto* v:voices){rack->getModule(v->module->id)->step();rack->getModule(v->module->id)->step();}
  require(buttons[momentary]->getParamQuantity()->getValue()==0,"Focus loss cleanup releases momentaries");
  for(const auto& p:owner->parameters)require(p.knob->getParamQuantity()==APP->engine->getModule(p.id)->getParamQuantity(p.paramId),"Keyboard preserves native mapping targets");
  owner->updateSectionAddButtons(owner->frame->box.getCenter());
  auto exposed=owner->parameters.size();key(GLFW_KEY_EQUAL);
  require(owner->parameters.size()==exposed+1,"Plus exposes a control in the hovered section");
  owner->updateSectionAddButtons(owner->frame->box.getCenter());key(GLFW_KEY_MINUS);
  require(owner->parameters.size()==exposed,"Minus removes one from the hovered section");
  owner->updateSectionAddButtons(owner->frame->box.getCenter());key(GLFW_KEY_R,GLFW_PRESS,GLFW_MOD_ALT);
  require(owner->parameters.size()==exposed,"Alt R rerolls the hovered section, preserving count");
  auto render=[&](const char* filename) {
    auto position=owner->box.pos;auto* parent=owner->parent;parent->removeChild(owner);
    auto* preview=new BeekKeyboardCapture;preview->owner=owner;owner->box.pos=Vec(8,8);preview->box.size=Vec(owner->box.size.x+244,396);preview->addChild(owner);
    capture(preview,output+filename,scale,false);
    preview->removeChild(owner);delete preview;owner->box.pos=position;parent->addChild(owner);
  };
  // A readable mix of momentary, toggle, snapped and continuous controls.
  while(!owner->parameters.empty())owner->removeExposedControl(0);
  for(int p:{0,1,2,3,5})owner->exposeParameter(voices[0]->module->id,p);
  owner->expose(voices[0],voices[0]->getInput(0));owner->expose(voices[0],voices[0]->getOutput(0));
  m->panelTheme.store(1);owner->layoutControls();owner->step();
  owner->updateSectionAddButtons(owner->frame->box.getCenter());
  render("/keyboard-wide-light.png");
  owner->exposeParameter(voices[1]->module->id,0);owner->exposeParameter(voices[1]->module->id,2);
  owner->requestedWidth=120;m->panelTheme.store(2);owner->layoutControls();owner->step();
  render("/keyboard-narrow-dark.png");
  rack->clear();APP->history->clear();
  std::puts("Beek keyboard: native momentary/toggle and bank releases, transport, no auto-repeat, text/browser/menu guards, mapping identity and fresh overlay renders passed");
}
