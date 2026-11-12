#pragma once
inline void checkBlunchSettings(rack::plugin::Model* model,const std::string& output,float scale) {
  using B=ComputerscareBlunch;
  auto require=[](bool ok,const char* why){if(!ok)throw std::runtime_error(why);};
  auto* m=static_cast<B*>(model->createModule());APP->engine->addModule(m);
  auto w=std::unique_ptr<ComputerscareBlunchWidget>(static_cast<ComputerscareBlunchWidget*>(model->createModuleWidget(m)));
  m->sequenceEditorStates[0].text="120bpm\n[300 450]ms\n(2 (3 1))hz@9";
  m->commitLineForChannel(0,0,true);m->params[B::RUN_PARAM].setValue(1);
  w->step();
  auto clickMode=[&](){
    widget::EventContext context;widget::Widget::ButtonEvent e;
    e.context=&context;e.button=GLFW_MOUSE_BUTTON_LEFT;e.action=GLFW_PRESS;
    w->editSettingsToggle->onButton(e);
  };
  auto key=[&](int code){widget::EventContext context;event::SelectKey e;e.context=&context;e.key=code;e.mods=0;e.action=GLFW_PRESS;w->settings->onSelectKey(e);};
  w->editor->cursor=2;w->editor->selection=2;w->step();
  const auto source=w->editor->text;int cursor=w->editor->cursor,selection=w->editor->selection;
  clickMode();require(w->settings->opened&&w->settings->transitioning(),"Edit opens Settings with a transition");
  require(APP->event->getSelectedWidget()==w->settings,"Settings receives keyboard focus");
  APP->event->setSelectedWidget(w->editSettingsToggle);
  w->step();
  require(APP->event->getSelectedWidget()==w->settings,"Settings restores keyboard focus after the mouse click completes");
  auto* s=w->settings;
  key(GLFW_KEY_ENTER);require(s->screens.size()==1,"Transition consumes keys without applying them");
  s->previewProgress=.5f;w->step();
  capture(w.get(),output+"/settings-splash.png",scale);
  s->previewProgress=1;w->step();
  require(!w->editor->visible,"Formula renderer is hidden behind Settings");
  capture(w.get(),output+"/settings-root.png",scale);
  key(GLFW_KEY_ENTER);require(s->screens.size()==2,"Enter opens Playback");
  capture(w.get(),output+"/settings-playback.png",scale);
  s->screens.back().selected=5;key(GLFW_KEY_ENTER);
  key(GLFW_KEY_DOWN);key(GLFW_KEY_DOWN);key(GLFW_KEY_SPACE);
  require(m->params[B::LINE_SCAN_PARAM].getValue()==2,"Line scan X is selectable");
  capture(w.get(),output+"/settings-line-scan.png",scale);
  key(GLFW_KEY_ESCAPE);s->screens.back().selected=6;key(GLFW_KEY_ENTER);
  key(GLFW_KEY_DOWN);key(GLFW_KEY_DOWN);key(GLFW_KEY_DOWN);key(GLFW_KEY_SPACE);
  require(m->params[B::BLOCK_SCAN_PARAM].getValue()==3,"Block scan Y is selectable");
  capture(w.get(),output+"/settings-block-scan.png",scale);
  key(GLFW_KEY_ESCAPE);s->screens.back().selected=0;
  key(GLFW_KEY_DOWN);key(GLFW_KEY_DOWN);key(GLFW_KEY_ENTER);
  require(s->screens.back().name=="Wait","Nested settings navigation");
  key(GLFW_KEY_DOWN);key(GLFW_KEY_DOWN);key(GLFW_KEY_SPACE);
  require(m->params[B::WAIT_MODE_PARAM].getValue()==2,"Space immediately selects Block wait");
  auto registry=blunchSettings(m);
  require(registry[0].children[2].children[2].checked(),"Context menu sees TUI changes");
  registry[0].children[2].children[3].action();
  require(s->screens.back().entries[3].checked(),"TUI sees context-menu changes");
  capture(w.get(),output+"/settings-wait.png",scale);
  key(GLFW_KEY_ESCAPE);require(s->screens.back().name=="Playback","Escape backs up one screen");
  s->screens.back().selected=4;key(GLFW_KEY_SPACE);
  require(m->params[B::LIVE_APPLY_PARAM].getValue()==1,"Checkbox toggles immediately");
  key(GLFW_KEY_SPACE);require(m->params[B::LIVE_APPLY_PARAM].getValue()==0,"Checkbox toggles off");
  key(GLFW_KEY_ESCAPE);
  s->screens.back().selected=1;key(GLFW_KEY_ENTER);s->screens.back().selected=1;key(GLFW_KEY_ENTER);
  for(int i=0;i<15;++i)key(GLFW_KEY_DOWN);
  require(s->firstRow>0,"Long choice lists keep the focused row visible");
  key(GLFW_KEY_ENTER);require(m->syncSource==15,"Source channel 16 is reachable");
  key(GLFW_KEY_ESCAPE);key(GLFW_KEY_ESCAPE);
  s->screens.back().selected=2;key(GLFW_KEY_ENTER);s->screens.back().selected=2;key(GLFW_KEY_ENTER);
  require(m->defaultNumberUnit==blunch::language::parseClockUnit("ms"),"Unit applies immediately");
  key(GLFW_KEY_ESCAPE);
  int staged=m->pageSubmitCount;s->screens.back().selected=3;key(GLFW_KEY_ENTER);
  require(m->pageSubmitCount==staged+1,"Stage page action is available");
  // Settings does not change the running state or formula text.
  require(m->channelPlaying(0),"Playback continues in Settings");
  engine::Module::ProcessArgs args;args.sampleRate=48000;args.sampleTime=1.f/48000;
  int high=0;
  for(int i=0;i<48000;++i){args.frame=i;m->process(args);high+=m->outputs[B::CLOCK_OUTPUT].getVoltage()>1.f;}
  require(high>0,"Settings does not stop real clock output");
  require(w->editor->text==source&&w->editor->cursor==cursor&&w->editor->selection==selection,"Settings preserves formula text and caret/selection");
  key(GLFW_KEY_ESCAPE);require(s->leaving,"Root Escape transitions back");
  w->step();require(!s->opened&&w->editor->visible,"Escape returns to Edit");
  require(APP->event->getSelectedWidget()==w->editor,"Formula focus restored");
  clickMode();s->previewProgress=1;w->step();clickMode();w->step();
  require(!s->opened,"Mode button returns to Edit");
  {
    widget::EventContext context;event::SelectKey f8;
    f8.context=&context;f8.key=GLFW_KEY_F8;f8.mods=0;f8.action=GLFW_PRESS;
    static_cast<BlunchTextEditor*>(w->editor)->onSelectKey(f8);s->previewProgress=1;w->step();
    require(s->opened,"F8 opens Settings from the focused editor");
    key(GLFW_KEY_F8);w->step();require(!s->opened,"F8 returns to Edit");
  }
  // Pointer activity must not change keyboard selection or activate entries.
  s->open(blunchSettings(m));s->previewProgress=1;w->step();
  widget::EventContext context;event::Button e;e.context=&context;e.button=GLFW_MOUSE_BUTTON_LEFT;e.action=GLFW_PRESS;e.pos=Vec(30,34);s->onButton(e);
  require(s->screens.size()==1&&s->screens.back().selected==0,"Mouse clicks do not activate settings");
  event::Hover hover;hover.context=&context;hover.pos=Vec(30,60);s->onHover(hover);
  event::HoverScroll scroll;scroll.context=&context;scroll.scrollDelta=Vec(0,-1);s->onHoverScroll(scroll);
  require(s->screens.back().selected==0,"Hover and scroll do not move keyboard selection");
  key(GLFW_KEY_DOWN);require(s->screens.back().selected==1,"Keyboard navigation still moves selection");
  capture(w.get(),output+"/settings-keyboard-only.png",scale);
  require(s->textStyle==&w->editor->style && bool(s->frame),"Settings shares editor typography and preserves its perspective frame");
  require(std::abs(s->metrics().lineHeight*s->textScale().y-w->editor->box.size.y/16.f)<.001f,"Settings rows match the editor's steady row height");
  m->params[B::EDITOR_FONT_SIZE_PARAM].setValue(16.f);
  m->params[B::EDITOR_FONT_WIDTH_PARAM].setValue(.03f);
  m->params[B::EDITOR_FONT_HEIGHT_PARAM].setValue(.12f);
  w->step();
  require(s->style().fontSize==16.f && s->style().fontWidthOffset==w->editor->style.fontWidthOffset,"Settings follows live editor typography changes");
  capture(w.get(),output+"/settings-matching-font.png",scale);
  m->params[B::EDITOR_FONT_SIZE_PARAM].setValue(14.f);
  m->params[B::EDITOR_FONT_WIDTH_PARAM].setValue(0.f);
  m->params[B::EDITOR_FONT_HEIGHT_PARAM].setValue(0.f);
  w->step();
  // Help is read-only; example activation copies exact reference formulas.
  s->screens.back().selected=4;key(GLFW_KEY_ENTER);
  require(s->screens.back().name=="Help" && s->screens.back().entries.size()==3,"Help has Overview, Controls, Language");
  capture(w.get(),output+"/help-menu.png",scale);
  key(GLFW_KEY_ENTER);require(!s->screens.back().body.empty(),"Overview opens a reader page");
  key(GLFW_KEY_DOWN);require(s->screens.back().scroll==1,"Help reader scrolls with arrows");
  key(GLFW_KEY_UP);
  capture(w.get(),output+"/help-overview.png",scale);
  key(GLFW_KEY_ESCAPE);s->screens.back().selected=1;key(GLFW_KEY_ENTER);key(GLFW_KEY_ENTER);
  require(s->screens.back().name=="Panel","Controls includes panel descriptions");
  capture(w.get(),output+"/help-controls.png",scale);
  key(GLFW_KEY_ESCAPE);key(GLFW_KEY_ESCAPE);
  s->screens.back().selected=2;key(GLFW_KEY_ENTER);
  require(s->screens.back().entries.size()==11,"Language includes every reference feature");
  for(const auto& topic:s->screens.back().entries)for(const auto& example:topic.children) {
    std::vector<BlunchProgramStep> program;
    if(!blunch::program::compileLineProgram(example.copyText,0,B::externalClockInputIndex,program))
      throw std::runtime_error("Help example does not compile: "+example.copyText);
  }
  capture(w.get(),output+"/help-language.png",scale);
  s->screens.back().selected=7;key(GLFW_KEY_ENTER);
  require(s->screens.back().entries[0].help.find("Randomly choose")!=std::string::npos,"Choice help explicitly says Randomly choose");
  capture(w.get(),output+"/help-random-choices.png",scale);
  key(GLFW_KEY_ESCAPE);
  s->screens.back().selected=3;key(GLFW_KEY_ENTER);
  require(s->screens.back().name=="Repeats","Language topics open examples");
  const char* prior=glfwGetClipboardString(APP->window->win);std::string clipboard=prior?prior:"";
  std::string expected=s->screens.back().entries[0].copyText;
  key(GLFW_KEY_ENTER);
  require(std::string(glfwGetClipboardString(APP->window->win))==expected,"Enter copies only the formula");
  require(w->editor->text==source && w->editor->cursor==cursor && w->editor->selection==selection,"Copying Help preserves the editor");
  capture(w.get(),output+"/help-copy-example.png",scale);
  for(int i=0;i<int(s->screens.back().entries.size());++i)
    if(s->screens.back().entries[i].copyText=="[120 91 62]@[2 3]")s->screens.back().selected=i;
  capture(w.get(),output+"/help-bracket-repeats.png",scale);
  for(int i=0;i<int(s->screens.back().entries.size());++i)
    if(s->screens.back().entries[i].copyText=="120bpm@(2 (3 4))")s->screens.back().selected=i;
  capture(w.get(),output+"/help-nested-repeats.png",scale);
  glfwSetClipboardString(APP->window->win,clipboard.c_str());
  key(GLFW_KEY_ESCAPE);s->screens.back().selected=8;key(GLFW_KEY_ENTER);
  s->screens.back().selected=int(s->screens.back().entries.size())-1;
  capture(w.get(),output+"/help-random-mode.png",scale);
  key(GLFW_KEY_ESCAPE);key(GLFW_KEY_ESCAPE);key(GLFW_KEY_ESCAPE);
  require(s->screens.size()==1,"Escape returns from Help to Settings");
  require(computerscare::TerminalSettings::transitionSeconds==.3,"Transition is 300 milliseconds");
  // Freeze 60 fps transition samples for an optional development animation.
  if(std::getenv("BLUNCH_SETTINGS_ANIMATION")) {
    std::string frames=output+"/settings-animation";system::createDirectories(frames);
    s->screens.erase(s->screens.begin()+1,s->screens.end());s->leaving=false;
    for(int i=0;i<150;++i) {
      if(i<30)s->previewProgress=i/30.f;
      else if(i<90)s->previewProgress=1;
      else if(i<120){s->leaving=true;s->previewProgress=(i-90)/30.f;}
      else{s->visible=false;w->editor->visible=true;}
      capture(w.get(),frames+string::f("/frame-%03d.png",i),scale,false);
    }
  }
  std::puts("Blunch Settings navigation, immediate shared bindings, 300ms transition, formula/focus preservation and mode cycling checks passed");
}
