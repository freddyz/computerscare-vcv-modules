#pragma once
// Capture only when Rack serializes the module, never on the audio/UI tick.
json_t* Beekapoo::portableContents(bool external) {
  if (members.empty()) return nullptr;
  auto* result = json_object();
  auto* modules = json_array();
  for (auto& member : members)
    if (auto* w = find(member.id)) {
      auto* item = w->toJson();
      json_object_set_new(item, "sourceId", json_integer(member.id));
      json_array_append_new(modules, item);
    }
  json_object_set_new(result, "modules", modules);
  auto* cables = json_array();
  for (auto* cw : homeRack->getCompleteCables()) {
    bool input = contains(cw->cable->inputModule),
         output = contains(cw->cable->outputModule);
    // Like Rack's selection duplicate, external incoming connections are
    // copied only by Duplicate with cables. Internal wiring always belongs
    // to the encapsulated instrument, even in a cable-free panel duplicate.
    if (!(input && output) && !(external && input)) continue;
    auto* item = cw->cable->toJson();
    cw->mergeJson(item);
    json_array_append_new(cables, item);
  }
  json_object_set_new(result, "cables", cables);
  return result;
}
// Locate the native add action so undo/redo treats the hidden contents as part
// of that same duplicate/paste operation, rather than leaving orphan modules.
inline bool attachContainedHistory(history::Action* action, int64_t id,
                                   std::vector<history::Action*>& additions,
                                   json_t* current) {
  auto* batch = dynamic_cast<history::ComplexAction*>(action);
  if (!batch) return false;
  for (size_t i = 0; i < batch->actions.size(); ++i) {
    auto* add = dynamic_cast<history::ModuleAdd*>(batch->actions[i]);
    if (add && !dynamic_cast<history::ModuleRemove*>(add) &&
        add->moduleId == id) {
      json_decref(add->moduleJ);
      add->moduleJ = json_incref(current);
      batch->actions.insert(batch->actions.begin() + i, additions.begin(),
                            additions.end());
      additions.clear();
      return true;
    }
    if (attachContainedHistory(batch->actions[i], id, additions, current))
      return true;
  }
  return false;
}
// Presets change an existing container rather than adding a new one. Its
// children must be swapped before applying either side of the host state.
struct ContainedPresetHistory : history::Action {
  history::ModuleChange* change;
  history::ComplexAction removed, added;
  explicit ContainedPresetHistory(history::ModuleChange* action)
      : change(action) {
    name = action->name;
  }
  ~ContainedPresetHistory() override { delete change; }
  Beekapoo* owner() const {
    return dynamic_cast<Beekapoo*>(
        APP->scene->rack->getModule(change->moduleId));
  }
  void undo() override {
    if (auto* group = owner()) group->endAbstraction();
    added.undo();
    removed.undo();
    change->undo();
    if (auto* group = owner()) group->step();
  }
  void redo() override {
    if (auto* group = owner()) group->endAbstraction();
    removed.redo();
    added.redo();
    change->redo();
    if (auto* group = owner()) group->step();
  }
};
history::ModuleChange* incomingGroupPreset(int64_t owner, json_t* root) {
  if (!json_object_get(root, "contents")) return nullptr;
  auto* state = APP->history;
  if (state->actionIndex <= 0 ||
      size_t(state->actionIndex) > state->actions.size())
    return nullptr;
  auto* candidate = dynamic_cast<history::ModuleChange*>(
      state->actions[state->actionIndex - 1]);
  // Native preset paste strips the root module ID. Reset/randomize snapshots
  // retain it. Already-handled presets use ContainedPresetHistory instead.
  if (!candidate || dynamic_cast<GroupHistory*>(candidate) ||
      candidate->moduleId != owner ||
      json_object_get(candidate->newModuleJ, "id"))
    return nullptr;
  auto* data = json_object_get(candidate->newModuleJ, "data");
  if (!json_equal(json_object_get(data, "contents"),
                  json_object_get(root, "contents")))
    return nullptr;
  auto* group = json_deep_copy(root);
  json_object_del(group, "contents");
  bool matches = json_equal(group, json_object_get(data, "group"));
  json_decref(group);
  return matches ? candidate : nullptr;
}
bool Beekapoo::importContents(json_t* root) {
  auto* contents = json_object_get(root, "contents");
  auto* list = json_object_get(contents, "modules");
  std::map<int64_t, int64_t> ids, cableIds;
  std::vector<app::ModuleWidget*> created;
  std::vector<app::CableWidget*> wires;
  std::vector<history::Action*> history;
  auto* state = APP->history;
  int presetIndex = state->actionIndex - 1;
  history::ModuleChange* preset = incomingGroupPreset(module->id, root);
  std::set<int64_t> previous;
  if (preset)
    for (auto& member : members) previous.insert(member.id);
  auto* remapped = json_deep_copy(root);
  json_object_del(remapped, "contents");
  saveGroup();
  auto* previousState = preset ? json_incref(preset->oldModuleJ) : toJson();
  DEFER({ json_decref(previousState); });
  bool committing = false;
  try {
    if (!json_is_array(list) || !json_array_size(list))
      throw Exception("No contained modules in preset");
    std::set<int64_t> sourceIds;
    // Validate dependencies and identities before creating any children.
    for (size_t i = 0; i < json_array_size(list); ++i) {
      auto* item = json_array_get(list, i);
      if (!plugin::modelFromJson(item))
        throw Exception("A contained module is not installed");
      auto* id = json_object_get(item, "sourceId");
      if (!json_is_integer(id) ||
          !sourceIds.insert(json_integer_value(id)).second)
        throw Exception("Invalid contained module identity");
    }
    // Allocate every native ID before applying any saved module data. Mapper
    // modules must never briefly bind to the original group's parameters.
    for (size_t i = 0; i < json_array_size(list); ++i) {
      auto* item = json_array_get(list, i);
      auto* model = plugin::modelFromJson(item);
      int64_t old = json_integer_value(json_object_get(item, "sourceId"));
      auto* m = model->createModule();
      APP->engine->addModule(m);
      app::ModuleWidget* w = nullptr;
      try {
        w = model->createModuleWidget(m);
      } catch (...) {
        APP->engine->removeModule(m);
        delete m;
        throw;
      }
      w->box.pos = parkingPosition();
      homeRack->addModule(w);
      created.push_back(w);
      ids[old] = m->id;
      if (auto* source = APP->engine->getModule(old))
        if (source->model == m->model &&
            system::isDirectory(source->getPatchStorageDirectory()))
          system::copy(source->getPatchStorageDirectory(),
                       m->getPatchStorageDirectory());
    }
    for (size_t i = 0; i < created.size(); ++i) {
      auto* copy = json_deep_copy(json_array_get(list, i));
      DEFER({ json_decref(copy); });
      json_object_del(copy, "id");
      json_object_del(copy, "leftModuleId");
      json_object_del(copy, "rightModuleId");
      // Rack MIDI Map, stoermelder's MapModuleBase and PatchMaster all use
      // data.maps entries with explicit moduleId/paramId pairs. Only remap
      // this known schema and targets inside the copied subpatch.
      auto* maps = json_object_get(json_object_get(copy, "data"), "maps");
      for (size_t j = 0; j < json_array_size(maps); ++j) {
        auto* entry = json_array_get(maps, j);
        auto* target = json_object_get(entry, "moduleId");
        if (json_is_integer(target) &&
            json_is_integer(json_object_get(entry, "paramId"))) {
          auto found = ids.find(json_integer_value(target));
          if (found != ids.end())
            json_object_set_new(entry, "moduleId", json_integer(found->second));
        }
      }
      // Widget::fromJson uses Engine::moduleFromJson's native engine lock.
      created[i]->fromJson(copy);
      auto* action = new history::ModuleAdd;
      action->setModule(created[i]);
      history.push_back(action);
    }
    for (const char* key : {"members", "subpatchPositions", "ports",
                            "parameters", "hiddenModules"}) {
      auto* entries = json_object_get(remapped, key);
      const char* field = (std::string(key) == "members" ||
                           std::string(key) == "subpatchPositions")
                              ? "id"
                              : "module";
      for (size_t i = 0; i < json_array_size(entries); ++i) {
        auto* entry = json_array_get(entries, i);
        auto old = json_integer_value(json_object_get(entry, field));
        if (!ids.count(old))
          throw Exception("Invalid contained module reference");
        json_object_set_new(entry, field, json_integer(ids.at(old)));
      }
    }
    auto* cables = json_object_get(contents, "cables");
    for (size_t i = 0; i < json_array_size(cables); ++i) {
      auto* item = json_array_get(cables, i);
      auto oldIn = json_integer_value(json_object_get(item, "inputModuleId"));
      auto oldOut = json_integer_value(json_object_get(item, "outputModuleId"));
      auto* input =
          ids.count(oldIn) ? APP->engine->getModule(ids.at(oldIn)) : nullptr;
      auto* output =
          APP->engine->getModule(ids.count(oldOut) ? ids.at(oldOut) : oldOut);
      int in = json_integer_value(json_object_get(item, "inputId")),
          out = json_integer_value(json_object_get(item, "outputId"));
      if (!input || !output || in < 0 || out < 0 ||
          in >= int(input->inputs.size()) || out >= int(output->outputs.size()))
        continue;
      auto* cable = new engine::Cable;
      cable->inputModule = input;
      cable->inputId = in;
      cable->outputModule = output;
      cable->outputId = out;
      APP->engine->addCable(cable);
      auto* cw = new app::CableWidget;
      cw->setCable(cable);
      cw->fromJson(item);
      homeRack->addCable(cw);
      wires.push_back(cw);
      cableIds[json_integer_value(json_object_get(item, "id"))] = cable->id;
      auto* action = new history::CableAdd;
      action->setCable(cw);
      history.push_back(action);
    }
    auto* panel = json_object_get(remapped, "panelCables");
    for (size_t i = 0; i < json_array_size(panel); ++i) {
      auto old = json_integer_value(json_array_get(panel, i));
      if (cableIds.count(old))
        json_array_set_new(panel, i, json_integer(cableIds.at(old)));
    }
    auto* internal = json_object_get(remapped, "internalCables");
    auto* mappedInternal = json_array();
    for (size_t i = 0; i < json_array_size(internal); ++i) {
      auto old = json_integer_value(json_array_get(internal, i));
      if (cableIds.count(old))
        json_array_append_new(mappedInternal, json_integer(cableIds.at(old)));
    }
    json_object_set_new(remapped, "internalCables", mappedInternal);
    json_object_del(remapped, "editing");
    json_object_set_new(remapped, "owner", json_integer(module->id));
    json_object_set_new(remapped, "expanded", json_false());
    committing = true;
    restore(remapped);
    json_decref(remapped);
    auto* current = module->toJson();
    if (preset) {
      auto* transaction = new ContainedPresetHistory(preset);
      // Successful restore released the former group. Remove those originals
      // with native history actions so Undo restores their IDs and cables.
      for (auto* cw : homeRack->getCompleteCables())
        if (previous.count(cw->cable->inputModule->id) ||
            previous.count(cw->cable->outputModule->id)) {
          auto* action = new history::CableRemove;
          action->setCable(cw);
          transaction->removed.push(action);
          homeRack->removeCable(cw);
          delete cw;
        }
      for (auto id : previous)
        if (auto* w = homeRack->getModule(id)) {
          auto* action = new history::ModuleRemove;
          action->setModule(w);
          transaction->removed.push(action);
          homeRack->removeModule(w);
          delete w;
        }
      transaction->added.actions.swap(history);
      json_decref(preset->newModuleJ);
      preset->newModuleJ = json_incref(current);
      state->actions[presetIndex] = transaction;
    } else if (state->actionIndex > 0 &&
               size_t(state->actionIndex) <= state->actions.size()) {
      attachContainedHistory(state->actions[state->actionIndex - 1], module->id,
                             history, current);
    }
    json_decref(current);
    for (auto* action : history) delete action;
    return true;
  } catch (const std::exception& e) {
    WARN("Beekapoo import: %s", e.what());
    if (committing) endAbstraction();
    for (auto* cw : wires) {
      homeRack->removeCable(cw);
      delete cw;
    }
    for (auto* w : created) {
      homeRack->removeModule(w);
      delete w;
    }
    for (auto* action : history) delete action;
    json_decref(remapped);
    // Restore both the host and its original group immediately. A failed
    // replacement must not release its members or leave changed host knobs.
    fromJson(previousState);
    if (auto* saved = getModule<Module>()->takeRestore()) {
      if (json_integer_value(json_object_get(saved, "owner")) == module->id)
        restore(saved);
      else
        endAbstraction();
      json_decref(saved);
    }
    status = "Could not restore contained modules: " + std::string(e.what());
    if (preset) {
      json_decref(preset->newModuleJ);
      preset->newModuleJ = json_incref(previousState);
      state->actions[presetIndex] = new ContainedPresetHistory(preset);
    }
    return false;
  }
}
void Beekapoo::duplicateAction(bool cables) {
  auto* m = getModule<Module>();
  if (!m) return;
  prepareContainedSave();
  m->copyBoundary = cables;
  app::ModuleWidget::cloneAction(cables);
  m->copyBoundary = false;
}
