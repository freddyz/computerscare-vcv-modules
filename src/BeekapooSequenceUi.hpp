// Included inside Beekapoo. Native live controls and immutable playback
// snapshots.
struct SequenceEditHistory : history::Action {
  int64_t owner = -1;
  json_t *before = nullptr, *after = nullptr;
  ~SequenceEditHistory() {
    json_decref(before);
    json_decref(after);
  }
  void apply(json_t* value) {
    if (auto* w = dynamic_cast<Beekapoo*>(APP->scene->rack->getModule(owner))) {
      auto* m = w->getModule<Module>();
      int playing = m->playingStep.load();
      w->loadSequence(value);
      m->playingStep.store(
          clamp(playing, 0, std::max(0, int(w->sequence.steps.size()) - 1)));
      w->saveGroup();
      w->layoutControls();
    }
  }
  void undo() override { apply(before); }
  void redo() override { apply(after); }
};
SequenceEditHistory* beginSequenceEdit(const char* name) {
  auto* action = new SequenceEditHistory;
  action->name = name;
  action->owner = module->id;
  action->before = sequenceJson();
  return action;
}
void finishSequenceEdit(SequenceEditHistory* action) {
  action->after = sequenceJson();
  APP->history->push(action);
}
SequenceBank sequence;
std::vector<ActionButton*> sequenceButtons;
ActionButton* stepSelector = nullptr;
bool sequenceModified = false;
unsigned sequenceStatusFrames = 0;
bool twoRowControls = false;

bool sequenceEligible(int64_t id, int param) {
  auto* w = find(id);
  if (!w || !contains(w->module) || param < 0 ||
      param >= int(w->module->paramQuantities.size()))
    return false;
  auto* q = w->module->getParamQuantity(param);
  auto* sw = dynamic_cast<app::Switch*>(w->getParam(param));
  return q && q->isBounded() && !(sw && sw->momentary);
}
StepValues captureSequenceValues() {
  StepValues values;
  for (const auto& member : members)
    if (auto* w = find(member.id))
      for (auto* q : w->module->paramQuantities)
        if (q && sequenceEligible(member.id, q->paramId) &&
            std::isfinite(q->getValue()))
          values.emplace_back(member.id, q->paramId, q->getValue());
  return values;
}
json_t* sequenceJson() {
  auto* root = json_object();
  auto* steps = json_array();
  for (const auto& values : sequence.steps)
    json_array_append_new(steps, stepValuesToJson(values));
  json_object_set_new(root, "steps", steps);
  auto* m = getModule<Module>();
  json_object_set_new(root, "clockEnabled",
                      json_boolean(m && m->clockEnabled.load()));
  json_object_set_new(root, "scanEnabled",
                      json_boolean(m && m->scanEnabled.load()));
  json_object_set_new(root, "clockedScan",
                      json_boolean(m && m->clockedScan.load()));
  json_object_set_new(root, "playing",
                      json_integer(m ? m->playingStep.load() : 0));
  return root;
}
void publishSequence() {
  if (sequence.steps.empty() && !members.empty()) {
    sequence.steps.push_back(captureSequenceValues());
    if (auto* m = getModule<Module>()) m->playingStep.store(0);
  }
  for (auto& values : sequence.steps)
    values.erase(std::remove_if(values.begin(), values.end(),
                                [&](const StepValue& v) {
                                  return !sequenceEligible(v.module, v.param);
                                }),
                 values.end());
  sequence.locks = randomLocks;
  if (auto* m = getModule<Module>()) m->publishSequence(sequence);
}
void loadSequence(json_t* root) {
  sequence.steps.clear();
  auto* steps = json_object_get(root, "steps");
  for (size_t i = 0; i < json_array_size(steps); ++i)
    sequence.steps.push_back(stepValuesFromJson(json_array_get(steps, i)));
  if (auto* m = getModule<Module>()) {
    m->clockEnabled.store(json_is_true(json_object_get(root, "clockEnabled")));
    m->scanEnabled.store(json_is_true(json_object_get(root, "scanEnabled")));
    m->clockedScan.store(json_is_true(json_object_get(root, "clockedScan")));
    int playing = json_integer_value(json_object_get(root, "playing"));
    m->playingStep.store(
        playing >= 0 && playing < int(sequence.steps.size()) ? playing : 0);
    m->requestedStep.store(-1);
  }
  // Old silent drafts are intentionally not restored or applied.
  publishSequence();
  refreshSequenceModified();
}
void selectSequenceStep(int index) {
  if (index < 0 || index >= int(sequence.steps.size())) return;
  if (auto* m = getModule<Module>()) m->requestedStep.store(index);
}
void appendSequenceStep() {
  auto* m = getModule<Module>();
  if (!m || members.empty()) return;
  auto* action = beginSequenceEdit("capture new parameter step");
  sequence.steps.push_back(captureSequenceValues());
  sequenceModified = false;
  // These values are already sounding; select without recalling anything.
  m->playingStep.store(int(sequence.steps.size()) - 1);
  m->requestedStep.store(-1);
  saveGroup();
  layoutControls();
  finishSequenceEdit(action);
}
void updateSequenceStep() {
  auto* m = getModule<Module>();
  int step = m ? m->playingStep.load() : -1;
  if (step < 0 || step >= int(sequence.steps.size())) return;
  auto* action = beginSequenceEdit("update parameter step");
  sequence.steps[step] = captureSequenceValues();
  sequenceModified = false;
  saveGroup();
  finishSequenceEdit(action);
}
void recallSequenceStep() {
  if (auto* m = getModule<Module>()) selectSequenceStep(m->playingStep.load());
}
void deleteSequenceStep(int index = -1) {
  auto* m = getModule<Module>();
  if (!m || sequence.steps.size() <= 1) return;
  if (index < 0) index = m->playingStep.load();
  if (index < 0 || index >= int(sequence.steps.size())) return;
  auto* action = beginSequenceEdit("delete parameter step");
  sequence.steps.erase(sequence.steps.begin() + index);
  int playing = m->playingStep.load();
  bool removedPlaying = playing == index;
  if (playing > index) --playing;
  playing = clamp(playing, 0, int(sequence.steps.size()) - 1);
  m->playingStep.store(playing);
  m->requestedStep.store(removedPlaying ? playing : -1);
  saveGroup();
  layoutControls();
  finishSequenceEdit(action);
}
void clearSequenceSteps() {
  auto* m = getModule<Module>();
  if (!m || sequence.steps.size() <= 1) return;
  auto* action = beginSequenceEdit("remove all sequence steps");
  // Keep the sound actually playing, including manual edits and held locks.
  sequence.steps.assign(1, captureSequenceValues());
  m->playingStep.store(0);
  m->requestedStep.store(-1);
  m->manualClocks.store(0);
  m->manualResets.store(0);
  saveGroup();
  layoutControls();
  finishSequenceEdit(action);
}
// Compare on the UI thread only. Stored values are immutable during playback.
// Protected controls deliberately stay independent of the selected snapshot.
void refreshSequenceModified() {
  auto* m = getModule<Module>();
  if (!m) {
    sequenceModified = false;
    return;
  }
  auto epoch = m->sequenceRecallEpoch.load();
  if (epoch & 1u) return;
  int step = m->playingStep.load();
  bool modified = false;
  if (step >= 0 && step < int(sequence.steps.size()))
    for (const auto& entry : sequence.steps[step]) {
      if (randomLocked(entry.module, entry.param) ||
          APP->engine->getParamHandle(entry.module, entry.param))
        continue;
      auto* w = find(entry.module);
      auto* q = w ? w->module->getParamQuantity(entry.param) : nullptr;
      if (q && std::abs(q->getValue() - entry.getValue()) > .00001f) {
        modified = true;
        break;
      }
    }
  if (epoch == m->sequenceRecallEpoch.load() && step == m->playingStep.load())
    sequenceModified = modified;
}
std::string sequenceStatus() {
  auto* m = getModule<Module>();
  int play = m ? m->playingStep.load() : 0;
  return std::to_string(std::max(0, play) + 1) + "/" +
         std::to_string(std::max(1, int(sequence.steps.size()))) +
         (sequenceModified ? " •" : "");
}
void showStepSelector() {
  if (sequence.steps.size() <= 1) return;
  auto* menu = createMenu();
  WeakPtr<Beekapoo> live = this;
  for (int i = 0; i < int(sequence.steps.size()); ++i)
    menu->addChild(createMenuItem(std::to_string(i + 1), "", [live, i] {
      if (live) live->selectSequenceStep(i);
    }));
}
void appendSequenceMenu(ui::Menu* menu) {
  WeakPtr<Beekapoo> live = this;
  menu->addChild(createMenuLabel(sequenceStatus()));
  menu->addChild(createSubmenuItem(
      "Scan",
      !getModule<Module>()->scanEnabled.load()  ? "Off"
      : getModule<Module>()->clockedScan.load() ? "Clocked"
                                                : "Continuous",
      [live](ui::Menu* sub) {
        const char* modes[] = {"Off", "Continuous", "Clocked"};
        for (int mode = 0; mode < 3; ++mode)
          sub->addChild(createCheckMenuItem(
              modes[mode], "",
              [live, mode] {
                if (!live) return false;
                auto* m = live->getModule<Module>();
                return mode == (!m->scanEnabled.load()  ? 0
                                : m->clockedScan.load() ? 2
                                                        : 1);
              },
              [live, mode] {
                if (live) {
                  auto* m = live->getModule<Module>();
                  m->clockedScan.store(mode == 2);
                  m->scanEnabled.store(mode != 0);
                  live->saveGroup();
                }
              }));
      }));
  menu->addChild(createCheckMenuItem(
      "Clock enabled", "",
      [live] { return live && live->getModule<Module>()->clockEnabled.load(); },
      [live] {
        if (live) {
          auto* m = live->getModule<Module>();
          m->clockEnabled.store(!m->clockEnabled.load());
          live->saveGroup();
        }
      }));
  menu->addChild(createMenuItem("Choose step", "", [live] {
    if (live) live->showStepSelector();
  }));
  menu->addChild(createSubmenuItem(
      "Delete step", "",
      [live](ui::Menu* sub) {
        if (!live) return;
        for (int i = 0; i < int(live->sequence.steps.size()); ++i)
          sub->addChild(createMenuItem(std::to_string(i + 1), "", [live, i] {
            if (live) live->deleteSequenceStep(i);
          }));
      },
      sequence.steps.size() <= 1));
  menu->addChild(new ui::MenuSeparator);
  menu->addChild(createMenuItem("Advance", "Actions CV ch 7", [live] {
    if (live) live->getModule<Module>()->manualClocks.fetch_add(1);
  }));
  menu->addChild(createMenuItem("Reset", "Actions CV ch 8", [live] {
    if (live) live->getModule<Module>()->manualResets.fetch_add(1);
  }));
  menu->addChild(createMenuItem("Capture New", "", [live] {
    if (live) live->appendSequenceStep();
  }));
  menu->addChild(createMenuItem("Update current step", "", [live] {
    if (live) live->updateSequenceStep();
  }));
  menu->addChild(createMenuItem("Recall current step", "", [live] {
    if (live) live->recallSequenceStep();
  }));
}
void showSequenceMenu() {
  auto* menu = createMenu();
  appendSequenceMenu(menu);
}
void createSequenceControls(Module* m) {
  addParam(createParam<ThemedSmallKnob>(Vec(), m, Module::SEQUENCE_SCAN));
  const int icons[] = {13, 20, 14, 16, 15, 21, 18};
  const char* help[] = {"Clock on/off",
                        "Advance playback\nActions CV ch 7",
                        "Reset playback\nActions CV ch 8",
                        "Capture New",
                        "Update current step",
                        "Recall current step",
                        "Delete current step"};
  for (int i = 0; i < 7; ++i) {
    auto* b = new ActionButton;
    b->icon = icons[i];
    std::string hint = help[i];
    b->help = [this, i, hint] {
      if (i == 0)
        return std::string("Clock ") +
               (module && getModule<Module>()->clockEnabled.load() ? "on"
                                                                   : "off") +
               " — Actions CV ch 7";
      return hint;
    };
    b->configure(18, 18, 1.1f, .8f, .4f, 1900 + i);
    b->textSize = 8;
    b->pressed = [this, i] {
      return i == 0 && module && getModule<Module>()->clockEnabled.load();
    };
    b->action = [this, i] {
      auto* m = getModule<Module>();
      if (!m) return;
      if (i == 0) {
        m->clockEnabled.store(!m->clockEnabled.load());
        saveGroup();
      } else if (i == 1)
        m->manualClocks.fetch_add(1);
      else if (i == 2)
        m->manualResets.fetch_add(1);
      else if (i == 3)
        appendSequenceStep();
      else if (i == 4)
        updateSequenceStep();
      else if (i == 5)
        recallSequenceStep();
      else
        deleteSequenceStep();
    };
    sequenceButtons.push_back(b);
    addChild(b);
  }
  stepSelector = new ActionButton;
  stepSelector->configure(64, 14, 1.f, .6f, .3f, 1950);
  stepSelector->textSize = 9;
  stepSelector->caption = [this] { return sequenceStatus(); };
  stepSelector->help = [this] {
    return sequenceModified ? "Choose and recall step\nModified: Capture New "
                              "or Update to keep changes"
                            : "Choose and recall step";
  };
  stepSelector->action = [this] {
    if (module) showStepSelector();
  };
  addChild(stepSelector);
}
