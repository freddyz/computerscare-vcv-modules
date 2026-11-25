// Included inside Beekapoo. Native live controls and immutable playback
// snapshots.
struct SequenceEditHistory : history::Action {
  int64_t owner;
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
bool showSavedValues = false;
bool twoRowControls = false;
float sequenceStatusY = 0;

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
  json_object_set_new(root, "showSavedValues", json_boolean(showSavedValues));
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
  if (auto* m = getModule<Module>()) m->publishSequence(sequence);
}
void loadSequence(json_t* root) {
  sequence.steps.clear();
  auto* steps = json_object_get(root, "steps");
  for (size_t i = 0; i < json_array_size(steps); ++i)
    sequence.steps.push_back(stepValuesFromJson(json_array_get(steps, i)));
  showSavedValues = json_is_true(json_object_get(root, "showSavedValues"));
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
}
void selectSequenceStep(int index) {
  if (index < 0 || index >= int(sequence.steps.size())) return;
  if (auto* m = getModule<Module>()) m->requestedStep.store(index);
}
void storeSequence(bool append) {
  auto* m = getModule<Module>();
  if (!m || members.empty()) return;
  int target = m->playingStep.load();
  if (!append && (target < 0 || target >= int(sequence.steps.size()))) return;
  auto* action = beginSequenceEdit(append ? "save new parameter step"
                                          : "store parameter step");
  auto values = captureSequenceValues();
  if (append) {
    sequence.steps.push_back(values);
    // Already sounding these values: only change the active snapshot index.
    m->playingStep.store(int(sequence.steps.size()) - 1);
    m->requestedStep.store(-1);
  } else
    sequence.steps[target] = values;
  saveGroup();
  finishSequenceEdit(action);
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
  finishSequenceEdit(action);
}
// Read-only reference for ghosts; never substitutes a parameter quantity.
bool savedSequenceValue(int64_t id, int param, float& value) {
  if (!showSavedValues) return false;
  auto* m = getModule<Module>();
  int step = m ? m->playingStep.load() : -1;
  if (step < 0 || step >= int(sequence.steps.size())) return false;
  for (const auto& entry : sequence.steps[step])
    if (entry.module == id && entry.param == param) {
      value = entry.value;
      return true;
    }
  return false;
}
std::string sequenceStatus(bool compact = false) {
  auto* m = getModule<Module>();
  int play = m ? m->playingStep.load() : 0;
  return std::string(compact ? "" : "Step ") +
         std::to_string(std::max(0, play) + 1) + "/" +
         std::to_string(std::max(1, int(sequence.steps.size())));
}
void showStepSelector() {
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
  menu->addChild(createCheckMenuItem(
      "Show saved values", "", [live] { return live && live->showSavedValues; },
      [live] {
        if (live) {
          live->showSavedValues = !live->showSavedValues;
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
  menu->addChild(createMenuItem(
      "Store", "",
      [live] {
        if (live) live->storeSequence(false);
      },
      sequence.steps.empty()));
  menu->addChild(createMenuItem("Save as New", "", [live] {
    if (live) live->storeSequence(true);
  }));
}
void showSequenceMenu() {
  auto* menu = createMenu();
  appendSequenceMenu(menu);
}
void createSequenceControls(Module* m) {
  addParam(createParam<ThemedSmallKnob>(Vec(), m, Module::SEQUENCE_SCAN));
  const int icons[] = {13, 20, 14, 16, 15};
  const char* help[] = {"Clock on/off", "Advance playback\nActions CV ch 7",
                        "Reset playback\nActions CV ch 8", "Save as New",
                        "Store step"};
  for (int i = 0; i < 5; ++i) {
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
      else
        storeSequence(i == 3);
    };
    sequenceButtons.push_back(b);
    addChild(b);
  }
  stepSelector = new ActionButton;
  stepSelector->configure(64, 14, 1.f, .6f, .3f, 1950);
  stepSelector->textSize = 9;
  stepSelector->caption = [this] { return sequenceStatus(box.size.x < 150); };
  stepSelector->help = [] { return "Choose and recall step"; };
  stepSelector->action = [this] {
    if (module) showStepSelector();
  };
  addChild(stepSelector);
}
