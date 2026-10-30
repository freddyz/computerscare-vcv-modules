#pragma once

// Exercise the actual module, including saved patches, rather than a copy of
// its draft/queue rules.
inline void checkBlunchFormulaLayers(rack::plugin::Model* model) {
  using B = ComputerscareBlunch;
  auto require = [](bool ok, const char* why) {
    if (!ok) throw std::runtime_error(why);
  };
  auto make = [&]() {
    auto m = std::unique_ptr<B>(static_cast<B*>(model->createModule()));
    for (int ch = 0; ch < B::MAX_POLY_CHANNELS; ++ch) {
      m->sequenceEditorStates[ch].text.clear();
      m->runningLineTexts[ch].clear();
      m->pendingFormulas[ch] = B::PendingFormula{};
      m->sequencers[ch].activeProgram.clear();
      m->sequencers[ch].activeLineText.clear();
    }
    m->focusedChannel = 0;
    return m;
  };
  {
    auto master=make();
    for(int ch=0;ch<2;++ch) {
      master->focusedChannel=ch;master->sequenceEditorStates[ch].text="3hz";
      require(master->commitLine(0,true),"Master-start Now fixture compiles");
      master->params[B::WAIT_MODE_PARAM].setValue(B::WAIT_PAGE);
      master->sequenceEditorStates[ch].text="4hz";require(master->executePage(),"Master-start queues Next");
      master->sequenceEditorStates[ch].text="5hz";
    }
    master->sequencers[0].running=true;master->sequencers[1].running=false;
    rack::engine::Module::ProcessArgs args;args.sampleRate=48000;args.sampleTime=1.f/48000.f;
    master->params[B::RUN_PARAM].setValue(0);master->process(args);
    master->params[B::RUN_PARAM].setValue(1);master->process(args);
    for(int ch=0;ch<2;++ch)require(master->sequencers[ch].activeLineText=="5hz"&&!master->channelHasPendingFormula(ch),"Run-on immediately applies latest Draft and clears Next, even for paused channels");
    require(master->sequencers[0].running&&!master->sequencers[1].running,"Run-on preserves remembered paused state");
    for(int ch=0;ch<2;++ch)master->sequenceEditorStates[ch].text="6hz";
    master->sequenceEditorStates[2].text="broken";
    master->startAllRequested=true;master->process(args);
    for(int ch=0;ch<2;++ch)require(master->sequencers[ch].running&&master->sequencers[ch].activeLineText=="6hz"&&!master->channelHasPendingFormula(ch),"Mod+Space applies and starts all valid channels without Wait");
    require(!master->sequencers[2].running&&master->sequenceEditorStates[2].text=="broken","Invalid Draft remains editable and does not start");
  }
  auto m = make();
  m->sequenceEditorStates[0].text = "3hz";
  require(m->commitLine(0, true), "Now baseline compiles");
  m->params[B::WAIT_MODE_PARAM].setValue(B::WAIT_LINE);
  m->sequenceEditorStates[0].text = "4hz";
  require(m->executeLine(0, true), "Draft stages to Next");
  m->sequenceEditorStates[0].text = "5hz";
  m->inspectFocusedLine(0, false);
  require(m->lineVersion(0, 0, B::NEXT) == "4hz" &&
              m->lineVersion(0, 0, B::NOW) == "3hz" && m->lineHasDraft(0, 0),
          "Typing after submit preserves the queued snapshot");
  require(!m->activatePendingLineAtBoundary(B::WAIT_BOUNDARY_CLOCK, true),
          "Wrong boundary leaves Next alone");
  require(m->activatePendingLineAtBoundary(B::WAIT_BOUNDARY_LINE, true) &&
              m->sequencers[0].activeLineText == "4hz" &&
              m->sequenceEditorStates[0].text == "5hz" && !m->channelHasNext(0) &&
              m->channelHasDraft(0),
          "Boundary promotes Next without overwriting newer Draft");
  m->sequenceEditorStates[0].text = "6hz";
  m->executeLine(0, true);
  m->sequenceEditorStates[0].text = "not a formula";
  m->inspectFocusedLine(0, false);
  require(m->lineVersion(0, 0, B::NEXT) == "6hz" && m->syntaxError,
          "Invalid typing does not destroy Next");
  m->cancelPendingLine(0);
  require(m->sequenceEditorStates[0].text == "6hz" && m->channelHasNext(0) && !m->channelHasDraft(0),
          "First cancel discards Draft but retains Next");
  m->cancelPendingLine(0);
  require(m->sequenceEditorStates[0].text == "4hz" && !m->channelHasNext(0),
          "Second cancel unstages Next back to Now");
  m->params[B::WAIT_MODE_PARAM].setValue(B::WAIT_IMMEDIATE);
  m->sequenceEditorStates[0].text = "7hz";
  m->executeLine(0, true);
  require(m->sequencers[0].activeLineText == "7hz" && !m->channelHasNext(0),
          "Immediate promotes a submission directly to Now");

  auto page = make();
  page->sequenceEditorStates[0].text = "1hz\n2hz\n3hz";
  page->commitLine(0, true);
  page->params[B::WAIT_MODE_PARAM].setValue(B::WAIT_PAGE);
  page->sequenceEditorStates[0].text = "4hz\n5hz\n3hz";
  require(page->executePage() && page->pendingFormulas[0].lines.size() == 2 &&
              page->sequencers[0].activeLine == 0,
          "Page submission queues multiple lines without jumping");
  page->sequenceEditorStates[0].text = "6hz\n5hz\n3hz";
  page->inspectFocusedLine(0, false);
  page->moveToNextLine(false);
  require(page->sequencers[0].activeLineText == "2hz", "Wait Page retains old lines before wrap");
  page->moveToNextLine(false);
  page->moveToNextLine(false);
  require(page->sequencers[0].activeLineText == "4hz" &&
              page->lineVersion(0, 1, B::NOW) == "5hz" &&
              page->sequenceEditorStates[0].text == "6hz\n5hz\n3hz" &&
              !page->channelHasNext(0) && page->channelHasDraft(0),
          "Page wrap swaps submitted lines atomically and retains newer edits");
  page->sequenceEditorStates[0].text = "8hz\ninvalid\n3hz";
  require(!page->executePage() && !page->channelHasNext(0),
          "Invalid page submission stages no partial changes");

  // A running line may outlive its deleted Draft row. Applying the channel
  // must submit all edits and keep playing surviving rows instead of ghosts.
  for (int wait : {int(B::WAIT_IMMEDIATE), int(B::WAIT_LINE)}) {
    auto ghost = make();
    ghost->sequenceEditorStates[0].text = "120bpm\n240bpm";
    require(ghost->commitLine(1, true), "Ghost fixture starts on line two");
    ghost->params[B::WAIT_MODE_PARAM].setValue(wait);
    ghost->sequenceEditorStates[0].text = "90bpm";
    require(ghost->lineHasDraft(0,1), "Deleting line two leaves a draft deletion");
    BlunchChannelTransport play;
    play.blunch = ghost.get();
    rack::event::Button click;
    click.button = GLFW_MOUSE_BUTTON_LEFT; click.action = GLFW_PRESS; click.mods = 0;
    play.onButton(click);
    if (wait != B::WAIT_IMMEDIATE) {
      require(ghost->pendingFormulas[0].lines.size()==2,
              "Play stages the edit and ghost deletion together");
      ghost->activatePendingLineAtBoundary(B::WAIT_BOUNDARY_LINE, true);
      ghost->moveToNextLine(false);
      ghost->moveToNextLine(false);
    }
    require(ghost->lineVersion(0,0,B::NOW)=="90bpm" &&
                ghost->lineVersion(0,1,B::NOW).empty() &&
                ghost->sequencers[0].activeLine==0 &&
                ghost->sequencers[0].activeLineText=="90bpm" &&
                !ghost->channelHasDraft(0),
            "Applying Draft removes ghost line two and plays the surviving edited line");
  }

  for (bool allView : {false, true}) {
    auto* keyboard = make().release();
    APP->engine->addModule(keyboard);
    keyboard->sequenceEditorStates[0].text = "120bpm\n240bpm";
    keyboard->commitLine(1, true);
    keyboard->params[B::WAIT_MODE_PARAM].setValue(B::WAIT_PAGE);
    keyboard->sequenceEditorStates[0].text = "90bpm";
    keyboard->channelsCursorChannel = 0;
    if (allView) keyboard->returnToChannelsView();
    else keyboard->openFocusedChannelEditPage();
    auto widget = std::unique_ptr<ComputerscareBlunchWidget>(
        static_cast<ComputerscareBlunchWidget*>(model->createModuleWidget(keyboard)));
    widget->step();
    ++widget->editor->commands.submitCount;
    widget->step();
    require(keyboard->pendingFormulas[0].lines.size()==2 &&
                keyboard->lineVersion(0,0,B::NEXT)=="90bpm" &&
                keyboard->lineVersion(0,1,B::NEXT).empty(),
            "Keyboard apply stages the whole channel and deleted row in both editor views");
    ++widget->editor->commands.submitCount;
    widget->step();
    require(keyboard->pendingFormulas[0].lines.size()==2,
            "Submitting again retains the complete queued page");
  }

  auto saved = make();
  saved->sequenceEditorStates[0].text = "3hz";
  saved->commitLine(0, true);
  saved->params[B::WAIT_MODE_PARAM].setValue(B::WAIT_LINE);
  saved->sequenceEditorStates[0].text = "4hz";
  saved->executeLine(0, true);
  saved->sequenceEditorStates[0].text = "5hz";
  saved->setFormulaLayer(B::NEXT);
  auto data = std::unique_ptr<json_t, void (*)(json_t*)>(saved->dataToJson(), json_decref);
  auto restored = make();
  restored->params[B::WAIT_MODE_PARAM].setValue(B::WAIT_LINE);
  restored->dataFromJson(data.get());
  require(restored->sequencers[0].activeLineText == "3hz" &&
              restored->lineVersion(0, 0, B::NEXT) == "4hz" &&
              restored->sequenceEditorStates[0].text == "5hz" &&
              restored->formulaLayer == B::NEXT,
          "Patch reload preserves Draft, Next, and Now independently");
  restored->peekNow = true;
  require(restored->visibleEditorState().text == "3hz", "Peek renders Now without changing Draft");
  restored->peekNow = false;
  require(restored->visibleEditorState().text == "4hz" &&
              restored->sequenceEditorStates[0].text == "5hz",
          "Releasing peek restores Next without changing Draft");
  restored->discardChannelPending(0);
  require(restored->sequenceEditorStates[0].text == "4hz" && restored->channelHasNext(0),
          "Channel cancel first drops only Draft");
  restored->discardChannelPending(0);
  require(restored->sequenceEditorStates[0].text == "3hz" && !restored->channelHasNext(0),
          "Channel cancel then unstages Next");
  restored->sequenceEditorStates[0].text = "6hz";
  restored->executeLine(0, true);
  restored->sequenceEditorStates[0].text = "7hz";
  restored->discardChannelPending(0, true);
  require(restored->sequenceEditorStates[0].text == "3hz" && !restored->channelHasNext(0),
          "Shift cancel drops Draft and Next together");
  auto entry = make();
  entry->sequenceEditorStates[0].text = "1hz\n2hz";
  entry->commitLine(0, true);
  entry->params[B::WAIT_MODE_PARAM].setValue(B::WAIT_LINE);
  entry->sequenceEditorStates[0].text = "3hz\n4hz\n5hz";
  entry->executePage();
  entry->moveToNextLine(false);
  require(entry->sequencers[0].activeLineText == "4hz" && entry->channelHasNext(0),
          "Page submissions promote reached lines without consuming other queued lines");
  entry->moveToNextLine(false);
  require(entry->sequencers[0].activeLineText == "5hz",
          "Newly staged lines are reachable before returning to the start");
  auto empty = make();
  empty->sequenceEditorStates[0].text = "1hz";
  empty->commitLine(0, true);
  empty->params[B::WAIT_MODE_PARAM].setValue(B::WAIT_PAGE);
  empty->sequenceEditorStates[0].text.clear();
  empty->executePage();
  empty->moveToNextLine(false);
  require(empty->sequencers[0].activeProgram.empty() && !empty->sequencers[0].running,
          "A staged empty page stops the old formula at the boundary");
  auto units = make();
  units->sequenceEditorStates[0].text = "3";
  units->commitLine(0, true);
  units->params[B::WAIT_MODE_PARAM].setValue(B::WAIT_LINE);
  units->sequenceEditorStates[0].text = "4";
  units->executeLine(0, true);
  units->defaultNumberUnit = blunch::language::ClockUnit::Hertz;
  units->activatePendingLineAtBoundary(B::WAIT_BOUNDARY_LINE, true);
  require(std::fabs(units->sequencers[0].activeClockSpec.periodSeconds - 15.) < .001,
          "A staged snapshot retains its submitted default unit");
  units->sequenceEditorStates[0].text = "6";
  units->executeLine(0, true);
  units->defaultNumberUnit = blunch::language::ClockUnit::Milliseconds;
  auto unitData = std::unique_ptr<json_t, void (*)(json_t*)>(units->dataToJson(), json_decref);
  auto restoredUnits = make();
  restoredUnits->params[B::WAIT_MODE_PARAM].setValue(B::WAIT_LINE);
  restoredUnits->dataFromJson(unitData.get());
  require(std::fabs(restoredUnits->sequencers[0].activeClockSpec.periodSeconds - 15.) < .001,
          "Patch reload retains Now's unit context");
  restoredUnits->activatePendingLineAtBoundary(B::WAIT_BOUNDARY_LINE, true);
  require(std::fabs(restoredUnits->sequencers[0].activeClockSpec.periodSeconds - 1. / 6.) < .001,
          "Patch reload retains Next's unit context separately from Draft");
  auto randomClock = make();
  randomClock->params[B::SEED_PARAM].setValue(1.f);
  randomClock->sequenceEditorStates[0].text = "{80|160}@3s";
  std::vector<double> firstPass;
  for (int pass = 0; pass < 2; ++pass) {
    require(randomClock->commitLine(0, true), "seeded timed choice compiles");
    auto& seq = randomClock->sequencers[0];
    require(seq.activeProgram[0].hasDuration, "random choice uses timed repeat");
    std::vector<double> choices;
    for (int beat = 0; beat < 32; ++beat) {
      choices.push_back(seq.activeClockSpec.bpm);
      randomClock->advanceActiveProgramBeat(seq);
    }
    require(std::find(choices.begin(), choices.end(), 80.) != choices.end() &&
                std::find(choices.begin(), choices.end(), 160.) != choices.end(),
            "Timed repeats redraw random clock choices instead of freezing the first");
    if (pass == 0) firstPass = choices;
    else require(firstPass == choices, "Seeded timed choices repeat after reset");
  }
  std::puts("Blunch Draft / Next / Now, page staging, persistence, and seeded timed-choice checks passed.");
}
