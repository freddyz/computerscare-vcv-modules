#pragma once
template<class Add, class Require>
void testBeekCopyCables(const std::string& output, float scale, Add add, Require require) {
  using beekapoo::Module;
  auto* rack = APP->scene->rack;
  auto* utility = plugin::getModel("Fundamental", "8vert");
  require(utility, "Copy wiring test needs Fundamental 8vert");
  auto connect = [&](app::PortWidget* out, app::PortWidget* in) {
    auto* c = new engine::Cable;
    c->outputModule = out->module; c->outputId = out->portId;
    c->inputModule = in->module; c->inputId = in->portId;
    APP->engine->addCable(c);
    auto* cw = new app::CableWidget; cw->setCable(c); rack->addCable(cw); return cw;
  };
  auto wiring = [&] {
    std::map<int64_t, std::string> result;
    for (auto* cw : rack->getCompleteCables()) {
      auto* c = cw->cable;
      result[c->id] = std::to_string(c->outputModule->id) + ":" + std::to_string(c->outputId) + "/" + std::to_string(c->inputModule->id) + ":" + std::to_string(c->inputId);
    }
    return result;
  };
  for (bool legacy : {false,true}) for (bool selection : {false, true}) for (bool withCables : {false, true}) {
    rack->clear(); APP->history->clear();
    auto* a = add(utility, Vec()); auto* b = add(utility, Vec(90, 0));
    auto* outside = add(utility, Vec(1500, 0));
    connect(a->getOutput(2), b->getInput(0));
    auto* owner = static_cast<beekapoo::Beekapoo*>(add(modelBeekapooPreview, Vec(300, 0)));
    rack->deselectAll(); rack->select(a, true); rack->select(b, true); owner->collapse();
    for (int i = 0; i < 3; ++i) {owner->expose(a, a->getInput(i));if(legacy)legacyBeekGain(owner,owner->ports.back());}
    owner->expose(a, a->getOutput(1)); owner->exposeParameter(a->module->id, 0);
    auto proxy = [&](beekapoo::Beekapoo* group, int index) {
      for (const auto& p : group->ports) if (p.input && p.portId == index) return group->findPort(p);
      return static_cast<app::PortWidget*>(nullptr);
    };
    connect(outside->getOutput(0), proxy(owner, 0));
    connect(outside->getOutput(1), proxy(owner, 0)); // two real stacked signals
    auto* self = connect(a->getOutput(1), proxy(owner, 1));
    owner->panelCableIds.insert(self->cable->id);
    connect(outside->getOutput(2), owner->getInput(0)); // public action CV
    owner->step(); owner->saveGroup();
    auto before = wiring(); auto ownerId = owner->module->id;
    if (selection) {
      rack->deselectAll(); rack->select(owner, true);
      beekapoo::duplicateBeekapooSelection(withCables);
    } else owner->duplicateAction(withCables);
    beekapoo::Beekapoo* copy = nullptr;
    for (auto* w : rack->getModules())
      if (auto* group = dynamic_cast<beekapoo::Beekapoo*>(w)) if (group != owner) copy = group;
    require(copy, "Native duplication must create a Beek"); copy->step(); owner->step();
    auto copyId = copy->module->id;
    auto verify = [&] {
      owner = dynamic_cast<beekapoo::Beekapoo*>(rack->getModule(ownerId));
      copy = dynamic_cast<beekapoo::Beekapoo*>(rack->getModule(copyId));
      require(owner && copy && copy->members.size() == 2, "Duplicate retains the contained modules");
      auto current = wiring();
      for (const auto& entry : before) require(current[entry.first] == entry.second, "Duplicate must not change the original cables or IDs");
      require(current.size() == before.size() + (withCables ? 5 : 2) + (legacy ? 3 : 0), "Copy must include internal wiring once, never duplicate exposed input signals or private bridges");
      for (auto* group : {owner, copy})
        for (auto* cw : rack->getCompleteCables()) {
          auto* c = cw->cable;
          if (c->outputModule == group->module) {
            require(group->inputBridge(c), "Container output may connect only to its own input bridge");
            require(!cw->visible, "Private input bridge cable must be hidden");
            auto plugs = group->plugWidgets.find(c->id);
            require(plugs != group->plugWidgets.end() && plugs->second.input && plugs->second.output && !plugs->second.input->visible && !plugs->second.output->visible, "Both hidden bridge plugs must stay hidden");
          }
        }
      auto incoming = rack->getCompleteCablesOnPort(proxy(copy, 0));
      require(incoming.size() == (withCables ? 2u : 0u), "External input stack must copy exactly once");
      auto loops = rack->getCompleteCablesOnPort(proxy(copy, 1));
      require(loops.size() == 1 && copy->contains(loops.front()->cable->outputModule), "Panel loopback must use the cloned source, not the original");
      require(rack->getCompleteCablesOnPort(copy->getInput(0)).size() == (withCables ? 1u : 0u), "Native action CV duplication must still work");
    };
    verify();
    auto after = wiring();
    APP->history->undo(); owner->step(); require(wiring() == before, "Undo clone restores exact original wiring");
    APP->history->redo(); copy = dynamic_cast<beekapoo::Beekapoo*>(rack->getModule(copyId)); copy->step(); owner->step(); verify();
    require(wiring() == after, "Redo must retain exact cable IDs");
    // Recreate the saved cherry8 fault: clone's private outputs were copied
    // into the original panel's inputs. They must be removed, not just hidden.
    if(!legacy)continue;
    for (int i = 0; i < 3; ++i) connect(copy->getOutput(i), proxy(owner, i));
    require(wiring().size() == after.size() + 3, "Legacy phantom fixture must contain three extra cables");
    auto* saved = APP->engine->toJson(); rack->mergeJson(saved); rack->clear(); APP->history->clear(); APP->engine->fromJson(saved); rack->fromJson(saved); json_decref(saved);
    for (int frame = 0; frame < 4; ++frame) rack->step();
    verify(); require(wiring() == after, "Reload must repair legacy private-output cross-links without changing real cables");
  }
  rack->clear(); APP->history->clear();
  if (const char* path = std::getenv("BEEKAPOO_PHANTOM_PATCH")) {
    // Only the two Beeks, their children and passive mixer consumers. Never
    // instantiate the user's audio interface or Portaloof in this renderer.
    std::vector<plugin::Model*> extras{modelComputerscareOhPeas, modelComputerscareGolyPenerator, modelComputerscareRolyPouter};
    for (auto* model : extras) pluginInstance->addModel(model);
    DEFER({ rack->clear(); APP->history->clear(); for (auto* model : extras) pluginInstance->models.remove(model); });
    auto* patch = json_load_file(path, 0, nullptr); require(patch, "Saved phantom patch must load");
    DEFER({json_decref(patch);});
    auto* modules = json_object_get(patch, "modules");
    std::set<int64_t> retained;
    for (size_t i = json_array_size(modules); i-- > 0;) {
      auto* item = json_array_get(modules, i);
      std::string maker = json_string_value(json_object_get(item, "plugin"));
      std::string slug = json_string_value(json_object_get(item, "model"));
      bool keep = maker == "Fundamental" || slug == "computerscare-beekapoo" || slug == "computerscare-ohpeas" || slug == "computerscare-goly-penerator" || slug == "computerscare-roly-pouter";
      if (keep) retained.insert(json_integer_value(json_object_get(item, "id")));
      else json_array_remove(modules, i);
    }
    auto* cables = json_object_get(patch, "cables");
    for (size_t i = json_array_size(cables); i-- > 0;) {
      auto* c = json_array_get(cables, i);
      if (!retained.count(json_integer_value(json_object_get(c, "inputModuleId"))) || !retained.count(json_integer_value(json_object_get(c, "outputModuleId")))) json_array_remove(cables, i);
    }
    APP->engine->fromJson(patch); rack->fromJson(patch);
    auto before = wiring();
    for (int i = 0; i < 4; ++i) rack->step();
    auto after = wiring();
    require(before.size() == after.size() + 3, "Cherry8 must lose exactly its three erroneous private-output cables");
    for (const auto& cable : after) require(before[cable.first] == cable.second, "Repair must preserve every valid cable and endpoint");
    std::vector<app::ModuleWidget*> visible;
    for (auto* w : rack->getModules()) if (w->visible && w->box.size.x > 0) visible.push_back(w);
    std::sort(visible.begin(), visible.end(), [](app::ModuleWidget* a, app::ModuleWidget* b) { return a->box.pos.x < b->box.pos.x; });
    float x = 15;
    for (auto* w : visible) { w->box.pos = RACK_OFFSET + Vec(x, 0); x += w->box.size.x + 5; }
    APP->scene->box.size = Vec(860, 800);
    APP->scene->rackScroll->box = Rect(Vec(0, 30), Vec(860, 770));
    APP->scene->rackScroll->setZoom(1.8f);
    APP->scene->rackScroll->setGridOffset(Vec(0, -.035f));
    for (int i = 0; i < 4; ++i) APP->scene->rackScroll->step();
    capture(APP->scene, output + "/cherry8-repaired.png", 1.5f, false);
    std::puts("Saved cherry8: removed exactly three phantom connections; all real wiring retained");
  }
  std::puts("Beek copy cables: native single/selection copies with/without cables, hidden plugs, exact stacked inputs, self-patch isolation, action CV, Undo/Redo and legacy phantom repair passed");
}
