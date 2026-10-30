#pragma once
inline void checkBlunchLineMenus(rack::plugin::Model* model,
                                const std::string& output, float scale) {
  using B = ComputerscareBlunch;
  auto require=[](bool ok,const char* why){if(!ok)throw std::runtime_error(why);};
  auto* m=static_cast<B*>(model->createModule());APP->engine->addModule(m);
  auto widget=std::unique_ptr<ComputerscareBlunchWidget>(static_cast<ComputerscareBlunchWidget*>(model->createModuleWidget(m)));
  for(int ch=0;ch<4;++ch){m->sequenceEditorStates[ch].text=ch==1?"100bpm\n150bpm\n[250 500]ms":"120bpm";m->commitLineForChannel(ch,0,true);m->stopSequencer(ch,true);}
  m->params[B::POLY_CHANNELS_PARAM].setValue(4);
  m->returnToChannelsView();widget->step();
  auto* editor=static_cast<BlunchTextEditor*>(widget->editor);
  auto open=[&](int line){
    Vec point(20,0);
    for(float y=.5f;y<editor->box.size.y;y+=.5f) {
      int offset=editor->getTextPosition(Vec(20,y));
      if(getAllViewTextPositionForOffset(editor->text,offset).line==line){point.y=y+2;break;}
    }
    widget::EventContext context;widget::Widget::ButtonEvent event;
    event.context=&context;event.button=GLFW_MOUSE_BUTTON_RIGHT;event.action=GLFW_PRESS;event.pos=point;
    editor->onButton(event);
    return dynamic_cast<ui::Menu*>(APP->scene->children.back()->children.front());
  };
  auto item=[&](ui::Menu* menu,const std::string& name){
    for(auto* child:menu->children)if(auto* entry=dynamic_cast<ui::MenuItem*>(child))if(entry->text==name)return entry;
    return static_cast<ui::MenuItem*>(nullptr);
  };
  auto close=[](ui::Menu* menu){auto* overlay=menu->parent;APP->scene->removeChild(overlay);delete overlay;};
  auto action=[&](ui::Menu* menu,const std::string& name){auto* entry=item(menu,name);require(entry,"Missing line menu action");event::Action e;entry->onAction(e);};
  auto show=[&](ui::Menu* menu,const char* filename){
    auto* overlay=menu->parent;overlay->removeChild(menu);APP->scene->removeChild(overlay);delete overlay;
    widget::Widget composition;composition.box.size=widget->box.size;
    composition.addChild(widget.get());composition.addChild(menu);
    menu->box.pos=editor->box.pos.plus(Vec(75,65));
    capture(&composition,output+"/"+filename,scale);
    composition.removeChild(widget.get());composition.removeChild(menu);delete menu;
  };
  const char* clipboard=glfwGetClipboardString(APP->window->win);
  std::string previous=clipboard?clipboard:"";
  m->controlSelection.selectOnly(0);
  auto* menu=open(2);require(menu,"Right-click opens row menu");
  require(item(menu,"Play channel 3")&&item(menu,"Stop channel 3"),"All menu targets clicked channel");
  action(menu,"Play channel 3");close(menu);
  require(m->channelPlaying(2)&&!m->channelPlaying(0)&&!m->channelPlaying(1),"Row Play ignores other selected/focused channels");
  menu=open(2);action(menu,"Copy line");require(std::string(glfwGetClipboardString(APP->window->win))=="120bpm","Row Copy uses clicked formula");
  require(item(menu,"Channel 3 playing")&&item(menu,"Channel 3 playing")->disabled,"Playing channel is identified and cannot be started again");
  show(menu,"context-all.png");
  menu=open(2);action(menu,"Stop channel 3");close(menu);m->processControlRequests();require(!m->channelPlaying(2),"Row Stop targets clicked channel");
  auto select=[&](int begin,int end){editor->selection=begin;editor->cursor=end;editor->state->selection=begin;editor->state->cursor=end;};
  select(0,3);menu=open(2);action(menu,"Copy");require(std::string(glfwGetClipboardString(APP->window->win))=="120","Context Copy uses selection rather than clicked row");
  action(menu,"Cut");close(menu);require(getLineInfo(editor->text,0).text=="bpm"&&getLineCount(editor->text)==16,"Context Cut preserves All rows");
  editor->undo();widget->step();select(0,3);glfwSetClipboardString(APP->window->win,"145");menu=open(2);action(menu,"Paste");close(menu);require(getLineInfo(editor->text,0).text=="145bpm","Context Paste replaces selected text");editor->undo();widget->step();
  m->params[B::AUTO_ADVANCE_PARAM].setValue(0);m->channelsCursorChannel=1;m->focusedChannel=1;m->openFocusedChannelEditPage();widget->step();
  menu=open(1);require(item(menu,"Play line 2")&&!item(menu,"Stop channel 2"),"Detail menu offers Play without Stop");
  action(menu,"Copy line");require(std::string(glfwGetClipboardString(APP->window->win))=="150bpm","Detail Copy uses clicked line");
  action(menu,"Play line 2");require(m->channelPlaying(1)&&m->sequencers[1].activeLine==1,"Detail Play starts clicked line");
  close(menu);menu=open(1);require(item(menu,"Line 2 playing")&&item(menu,"Line 2 playing")->disabled,"Playing formula is identified and disabled");show(menu,"context-channel.png");
  select(0,3);menu=open(2);action(menu,"Cut");close(menu);require(getLineInfo(editor->text,0).text=="bpm","Detail context Cut uses selected text");editor->undo();widget->step();
  select(0,3);glfwSetClipboardString(APP->window->win,"135");menu=open(2);action(menu,"Paste");close(menu);require(getLineInfo(editor->text,0).text=="135bpm","Detail context Paste uses selected text");editor->undo();widget->step();
  glfwSetClipboardString(APP->window->win,previous.c_str());
  std::puts("Blunch clicked-row transport/copy menus and channel-detail Play checks passed");
}
