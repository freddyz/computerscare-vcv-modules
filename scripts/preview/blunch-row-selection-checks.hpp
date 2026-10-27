#pragma once
inline void checkBlunchRowSelection(rack::plugin::Model* model) {
  using B=ComputerscareBlunch;
  auto require=[](bool ok,const char* why){if(!ok)throw std::runtime_error(why);};
  auto* m=static_cast<B*>(model->createModule());APP->engine->addModule(m);
  auto widget=std::unique_ptr<ComputerscareBlunchWidget>(static_cast<ComputerscareBlunchWidget*>(model->createModuleWidget(m)));
  for(int ch=0;ch<4;++ch){m->sequenceEditorStates[ch].text=ch==1?"120bpm\n90bpm":"120bpm";require(m->commitLineForChannel(ch,0,true),"Selection fixture compiles");m->stopSequencer(ch,true);}
  m->params[B::POLY_CHANNELS_PARAM].setValue(4);m->returnToChannelsView();widget->step();
  auto* editor=static_cast<BlunchTextEditor*>(widget->editor);
  auto point=[&](int row){return Vec(15.f,(row+.5f)*editor->box.size.y/16.f);};
  auto click=[&](int row,int mods=0){widget::EventContext context;widget::Widget::ButtonEvent e;e.context=&context;e.action=GLFW_PRESS;e.button=GLFW_MOUSE_BUTTON_LEFT;e.mods=mods;e.pos=point(row);editor->onButton(e);widget->step();};
  auto key=[&](int code,int mods){widget::EventContext context;widget::Widget::SelectKeyEvent e;e.context=&context;e.action=GLFW_PRESS;e.key=code;e.mods=mods;editor->onSelectKey(e);widget->step();};
  click(1);key(GLFW_KEY_ENTER,RACK_MOD_CTRL);
  require(m->sequencers[1].running&&!m->sequencers[0].running&&!m->sequencers[2].running,"Mod+Enter starts only the selected All-view channel");
  require(!m->channelsEditorEditingEnabled(1),"Playing multiline row is text-read-only");
  key(GLFW_KEY_DOWN,GLFW_MOD_SHIFT);key(GLFW_KEY_DOWN,GLFW_MOD_SHIFT);
  require(m->controlSelection.targets()==14u,"Shift navigation selects across a read-only multiline row");
  key(GLFW_KEY_ENTER,RACK_MOD_CTRL);
  for(int ch=1;ch<4;++ch)require(m->sequencers[ch].running,"Mod+Enter starts every selected channel");
  key(GLFW_KEY_COMMA,RACK_MOD_CTRL);m->processControlRequests();
  for(int ch=1;ch<4;++ch)require(!m->sequencers[ch].running,"Mod+Comma stops every selected channel");
  click(0);widget::EventContext context;widget::Widget::DragHoverEvent drag;drag.context=&context;drag.origin=editor;drag.pos=point(2);editor->onDragHover(drag);editor->onDragEnd(widget::Widget::DragEndEvent{});widget->step();
  require(m->controlSelection.targets()==7u,"Mouse drag selects every crossed All-view channel");
  auto button=[&](BlunchChannelTransport* b){widget::EventContext context;widget::Widget::ButtonEvent e;e.context=&context;e.action=GLFW_PRESS;e.button=GLFW_MOUSE_BUTTON_LEFT;b->onButton(e);e.action=GLFW_RELEASE;b->onButton(e);m->processControlRequests();widget->step();};
  button(widget->channelPlay);for(int ch=0;ch<3;++ch)require(m->sequencers[ch].running,"Panel Play applies to the selection");require(!m->sequencers[3].running,"Panel Play leaves unselected channel stopped");
  button(widget->channelStop);for(int ch=0;ch<3;++ch)require(!m->sequencers[ch].running,"Panel Stop applies to the selection");
  click(3,GLFW_MOD_SHIFT);require(m->controlSelection.targets()==15u,"Shift-click extends the existing range");
  const char* previousClipboard=glfwGetClipboardString(APP->window->win);
  std::string savedClipboard=previousClipboard?previousClipboard:"";
  struct RestoreClipboard { std::string value; ~RestoreClipboard(){glfwSetClipboardString(APP->window->win,value.c_str());} } restoreClipboard{savedClipboard};
  m->controlMode=false;
  for(int ch=0;ch<4;++ch){m->stopSequencer(ch,true);m->sequenceEditorStates[ch].text="120bpm";}
  m->refreshChannelsEditorState();widget->step();
  auto selectText=[&](int begin,int end){editor->selection=begin;editor->cursor=end;editor->state->selection=begin;editor->state->cursor=end;};
  auto row0=getLineInfo(editor->text,0);selectText(row0.begin+2,row0.begin+2);m->controlSelection.selectOnly(0);
  key(GLFW_KEY_DOWN,GLFW_MOD_SHIFT);
  require(editor->selection==row0.begin+2&&editor->cursor>getLineInfo(editor->text,1).begin,"Shift+Down preserves the text anchor across All-view rows");
  auto first=getLineInfo(editor->text,0), second=getLineInfo(editor->text,1);
  selectText(first.begin+1,second.begin+3);
  std::string selectedText=editor->text.substr(first.begin+1,second.begin+3-first.begin-1);
  key(GLFW_KEY_C,RACK_MOD_CTRL);
  require(std::string(glfwGetClipboardString(APP->window->win))==selectedText,"All-view Copy keeps exact text across rows");
  key(GLFW_KEY_X,RACK_MOD_CTRL);
  require(getLineCount(editor->text)==16&&m->sequenceEditorStates[0].text!="120bpm"&&m->sequenceEditorStates[1].text!="120bpm","All-view Cut updates affected channels without deleting rows");
  editor->undo();widget->step();require(m->sequenceEditorStates[0].text=="120bpm"&&m->sequenceEditorStates[1].text=="120bpm","Undo restores every channel changed by Cut");
  auto row2=getLineInfo(editor->text,2);selectText(row2.begin,row2.end);
  glfwSetClipboardString(APP->window->win,"80bpm\n160bpm");key(GLFW_KEY_V,RACK_MOD_CTRL);
  require(m->sequenceEditorStates[2].text=="80bpm"&&m->sequenceEditorStates[3].text=="160bpm","All-view Paste maps pasted lines to successive channels");
  m->sequenceEditorStates[1].text="120bpm\n90bpm";m->commitLineForChannel(1,0,true);m->startSequencer(1);m->refreshChannelsEditorState();widget->step();
  auto readonly=getLineInfo(editor->text,1);selectText(readonly.begin,readonly.end);key(GLFW_KEY_C,RACK_MOD_CTRL);
  require(std::string(glfwGetClipboardString(APP->window->win))==readonly.text,"Playing multiline All-view rows remain copyable");
  std::string page=m->sequenceEditorStates[1].text;selectText(readonly.begin,readonly.end);key(GLFW_KEY_X,RACK_MOD_CTRL);
  require(m->sequenceEditorStates[1].text==page,"Cut leaves playing multiline pages intact");
  m->focusedChannel=1;m->openFocusedChannelEditPage();widget->step();
  selectText(0,3);key(GLFW_KEY_C,RACK_MOD_CTRL);require(std::string(glfwGetClipboardString(APP->window->win))=="120","Channel-view Copy uses the exact text selection");
  selectText(0,3);key(GLFW_KEY_X,RACK_MOD_CTRL);require(editor->text.substr(0,3)=="bpm","Channel-view Cut edits the selection");
  glfwSetClipboardString(APP->window->win,"140");key(GLFW_KEY_V,RACK_MOD_CTRL);require(editor->text.substr(0,6)=="140bpm","Channel-view Paste uses the standard shortcut");
  std::puts("Blunch All-view row drag, Shift range, read-only rows and selected transport checks passed");
}
