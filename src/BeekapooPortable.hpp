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
bool Beekapoo::importContents(json_t* root) {
  auto* contents = json_object_get(root, "contents");
  auto* list = json_object_get(contents, "modules");
  if (!json_is_array(list) || !json_array_size(list)) return false;
  std::map<int64_t, int64_t> ids, cableIds;
  std::vector<app::ModuleWidget*> created;
  std::vector<app::CableWidget*> wires;
  std::vector<history::Action*> history;
  auto* remapped = json_deep_copy(root);
  json_object_del(remapped, "contents");
  try {
    for (size_t i = 0; i < json_array_size(list); ++i) {
      auto* item = json_array_get(list, i);
      auto* model = plugin::modelFromJson(item);
      if (!model) throw Exception("A contained module is not installed");
      auto* copy = json_deep_copy(item);
      int64_t old = json_integer_value(json_object_get(item, "sourceId"));
      json_object_del(copy, "id");
      json_object_del(copy, "leftModuleId");
      json_object_del(copy, "rightModuleId");
      auto* m = model->createModule();
      try {
        m->fromJson(copy);
      } catch (...) {
        delete m;
        json_decref(copy);
        throw;
      }
      APP->engine->addModule(m);
      app::ModuleWidget* w = nullptr;
      try {
        w = model->createModuleWidget(m);
        w->fromJson(copy);
      } catch (...) {
        APP->engine->removeModule(m);
        if (w)
          delete w;
        else
          delete m;
        json_decref(copy);
        throw;
      }
      json_decref(copy);
      w->box.pos = parkingPosition();
      homeRack->addModule(w);
      created.push_back(w);
      ids[old] = m->id;
      // Match Rack's native duplicate handling of per-module patch storage.
      if (auto* source = APP->engine->getModule(old))
        if (source->model == m->model &&
            system::isDirectory(source->getPatchStorageDirectory()))
          system::copy(source->getPatchStorageDirectory(),
                       m->getPatchStorageDirectory());
      auto* action = new history::ModuleAdd;
      action->setModule(w);
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
    restore(remapped);
    json_decref(remapped);
    auto* current = module->toJson();
    auto* state = APP->history;
    if (state->actionIndex > 0 &&
        size_t(state->actionIndex) <= state->actions.size())
      attachContainedHistory(state->actions[state->actionIndex - 1], module->id,
                             history, current);
    json_decref(current);
    for (auto* action : history) delete action;
    return true;
  } catch (const std::exception& e) {
    WARN("Beekapoo import: %s", e.what());
    endAbstraction();
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
    status = "Could not restore contained modules";
    return false;
  }
}
void Beekapoo::duplicateAction(bool cables) {
  auto* m = getModule<Module>();
  if (!m) return;
  m->copyBoundary = cables;
  app::ModuleWidget::cloneAction(cables);
  m->copyBoundary = false;
}
