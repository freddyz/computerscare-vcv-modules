#pragma once
// Grow in place: existing module/parameter IDs and external cables survive.
// Each mutation uses native Rack history, followed by the membership change.
bool Beekapoo::growPatch(history::ComplexAction* parent) {
  if (!module || members.empty()) return false;
  ScopedSubviewRack mainScope(homeRack);
  struct Destination {
    app::ModuleWidget* module;
    int input;
  };
  std::vector<Destination> free;
  std::vector<app::CableWidget*> insertions;
  std::vector<app::PortWidget*> sources;
  const auto& specs = beekapoo_patch::library();
  for (const auto& member : members) {
    if (randomLocked(member.id, -1)) continue;
    auto* w = find(member.id);
    if (!w) continue;
    for (const auto& spec : specs)
      if (w->model->plugin->slug == "Fundamental" &&
          w->model->slug == spec.slug) {
        auto inputConnected = [&](int id) {
          auto* p = w->getInput(id);
          return p && !homeRack->getCompleteCablesOnPort(p).empty();
        };
        auto outputUsed = [&](int id) {
          auto* p = w->getOutput(id);
          return p &&
                 (exposed(p) || !homeRack->getCompleteCablesOnPort(p).empty());
        };
        for (const auto& in : spec.modulationInputs) {
          auto* port = w->getInput(in.id);
          if (!port || exposed(port) || inputConnected(in.id)) continue;
          if (beekapoo_patch::controlUseful(in, inputConnected, outputUsed))
            free.push_back({w, in.id});
        }
        for (const auto& out : spec.outputs) {
          auto* port = w->getOutput(out.port);
          if (!port) continue;
          bool ready = true;
          // Use a preferred main output before adding secondary taps.
          for (const auto& preferred : spec.outputs)
            if (preferred.priority > out.priority &&
                !outputUsed(preferred.port))
              ready = false;
          for (const auto& req : out.requirements) {
            bool met = false;
            for (int id : req.anyOf)
              if (auto* p = w->getInput(id))
                met |= !homeRack->getCompleteCablesOnPort(p).empty();
            ready &= met;
          }
          if (ready) sources.push_back(port);
        }
      }
  }
  for (auto* cw : homeRack->getCompleteCables())
    if (!inputBridge(cw->cable) && contains(cw->cable->inputModule) &&
        contains(cw->cable->outputModule) &&
        !randomLocked(cw->cable->inputModule->id, -1) &&
        !randomLocked(cw->cable->outputModule->id, -1))
      insertions.push_back(cw);
  std::vector<std::pair<app::PortWidget*, Destination>> candidates;
  for (auto* from : sources)
    for (const auto& to : free)
      if (from->module != to.module->module) candidates.push_back({from, to});
  if (candidates.empty() && insertions.empty()) {
    status = "No unlocked connections available to grow";
    return false;
  }
  auto* state = beginAction("grow subpatch");
  auto* batch = new TopologyHistory(module->id);
  batch->name = "grow subpatch";
  auto wire = [&](app::PortWidget* from, app::PortWidget* to) {
    auto* c = new engine::Cable;
    c->outputModule = from->module;
    c->outputId = from->portId;
    c->inputModule = to->module;
    c->inputId = to->portId;
    APP->engine->addCable(c);
    auto* cw = new app::CableWidget;
    cw->setCable(c);
    cw->color = patchCableColor(appearance, random::u32());
    homeRack->addCable(cw);
    internalCableIds.insert(c->id);
    knownCableIds.insert(c->id);
    auto* action = new history::CableAdd;
    action->setCable(cw);
    batch->push(action);
  };
  unsigned choice = random::u32() % 100;
  if (!candidates.empty() && (choice < 65 || insertions.empty())) {
    const auto& c = candidates[random::u32() % candidates.size()];
    wire(c.first, c.second.module->getInput(c.second.input));
  } else {
    auto* old = insertions[random::u32() % insertions.size()];
    auto* from = old->outputPort;
    auto* to = old->inputPort;
    const char* slug = "VCA-1";
    auto* model = plugin::getModel("Fundamental", slug);
    if (!model) {
      delete state;
      delete batch;
      status = "Install VCV Free to add a module";
      return false;
    }
    auto* m = model->createModule();
    for (const auto& spec : specs)
      if (spec.slug == std::string(slug))
        for (const auto& r : spec.initial)
          if (auto* q = m->getParamQuantity(r.param))
            q->setImmediateValue(
                clamp(r.low + random::uniform() * (r.high - r.low),
                      q->getMinValue(), q->getMaxValue()));
    if (std::string(slug) == "VCA-1")
      m->params[0].setValue(.25f + random::uniform() * .6f);
    APP->engine->addModule(m);
    auto* w = model->createModuleWidget(m);
    w->box.pos = parkingPosition();
    homeRack->addModule(w);
    auto* add = new history::ModuleAdd;
    add->setModule(w);
    batch->push(add);
    Vec pos(-INFINITY, 0);
    for (const auto& member : members)
      if (auto* other = find(member.id)) {
        float right = member.position.x + other->box.size.x;
        if (right > pos.x) pos = Vec(right, member.position.y);
      }
    if (expanded) {
      homeRack->setModulePosNearest(w, pos);
      pos = w->box.pos;
    }
    members.push_back({m->id, pos, w, nullptr});
    auto* remove = new history::CableRemove;
    remove->setCable(old);
    batch->push(remove);
    homeRack->removeCable(old);
    delete old;
    wire(from, w->getInput(1));
    wire(w->getOutput(0), to);
    if (std::string(slug) == "VCA-1" && !sources.empty())
      wire(sources[random::u32() % sources.size()], w->getInput(0));
  }
  compactMembers();
  if (!expanded) pack();
  layoutControls();
  saveGroup();
  refreshGeneratedName();
  state->newModuleJ = toJson();
  state->after.capture();
  batch->push(state);
  if (parent)
    parent->push(batch);
  else
    APP->history->push(batch);
  status = "Subpatch grown";
  return true;
}
