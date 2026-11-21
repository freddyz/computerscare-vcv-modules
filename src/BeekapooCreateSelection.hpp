#pragma once
// Flatten selected Beeks through the same native Eject history, then group
// the released modules. Module/cable IDs survive, including stacked inputs
// and feedback patched through the previous panels. One Undo restores all.
inline bool canCreateSelectedGroup() {
  auto* rack = APP->scene->rack;
  auto selected = rack->getSelected();
  if (selected.size() < 2) return false;
  for (auto* w : selected) {
    if (!w->module || !w->visible || w->module->leftExpander.producerMessage ||
        w->module->rightExpander.producerMessage)
      return false;
    for (auto* other : rack->getModules())
      if (auto* group = dynamic_cast<Beekapoo*>(other))
        if (group->contains(w->module) && !selected.count(group)) return false;
  }
  return true;
}
inline void createSelectedGroup() {
  if (!canCreateSelectedGroup()) return;
  auto* rack = APP->scene->rack;
  auto selected = rack->getSelected();
  Vec minimum(INFINITY, INFINITY), maximum(-INFINITY, -INFINITY);
  std::set<int64_t> children, panelCables;
  std::vector<int64_t> owners;
  std::set<std::pair<int64_t, int>> params, locks;
  struct ExposedPort {
    int64_t module;
    int port;
    bool input;
    float gain;
  };
  std::vector<ExposedPort> ports;
  for (auto* w : selected) {
    minimum.x = std::min(minimum.x, w->box.pos.x);
    minimum.y = std::min(minimum.y, w->box.pos.y);
    maximum.x = std::max(maximum.x, w->box.getRight());
    maximum.y = std::max(maximum.y, w->box.getBottom());
    if (auto* group = dynamic_cast<Beekapoo*>(w)) {
      owners.push_back(w->module->id);
      for (const auto& member : group->members) children.insert(member.id);
      for (const auto& p : group->parameters) params.insert({p.id, p.paramId});
      for (const auto& p : group->ports)
        ports.push_back(
            {p.id, p.portId, p.input,
             p.route >= 0
                 ? group->module
                       ->getParamQuantity(Module::INPUT_GAIN_BASE + p.route)
                       ->getValue()
                 : 1.f});
      locks.insert(group->randomLocks.begin(), group->randomLocks.end());
      panelCables.insert(group->panelCableIds.begin(),
                         group->panelCableIds.end());
      // Previously external inputs still pass through their attenuverter
      // when the source is now part of this combined selection.
      for (auto* cw : rack->getCompleteCables())
        if (cw->cable->inputModule == group->module &&
            cw->cable->inputId >= Module::inputBase)
          panelCables.insert(cw->cable->id);
    } else
      children.insert(w->module->id);
  }
  auto* module = modelComputerscareBeekapoo->createModule();
  APP->engine->addModule(module);
  app::ModuleWidget* widget = nullptr;
  try {
    widget = modelComputerscareBeekapoo->createModuleWidget(module);
  } catch (...) {
    APP->engine->removeModule(module);
    delete module;
    throw;
  }
  auto* group = static_cast<Beekapoo*>(widget);
  group->box.pos =
      Vec(maximum.x + RACK_GRID_WIDTH, maximum.y + RACK_GRID_HEIGHT);
  rack->setModulePosNearest(group, group->box.pos);
  rack->addModule(group);
  auto* transaction = new history::ComplexAction;
  transaction->name = "create Beek-A-Poo";
  auto* add = new history::ModuleAdd;
  add->setModule(group);
  transaction->push(add);
  for (auto id : owners)
    if (auto* old = dynamic_cast<Beekapoo*>(rack->getModule(id)))
      old->ejectAction(transaction, false);
  rack->deselectAll();
  for (auto id : children)
    if (auto* w = rack->getModule(id)) rack->select(w, true);
  auto* change = group->beginAction("create Beek-A-Poo");
  group->collapse();
  group->panelCableIds.insert(panelCables.begin(), panelCables.end());
  group->randomLocks = locks;
  for (const auto& p : params) group->exposeParameter(p.first, p.second);
  for (const auto& p : ports)
    if (auto* w = group->find(p.module)) {
      auto* source = p.input ? w->getInput(p.port) : w->getOutput(p.port);
      if (source) group->expose(w, source);
      for (const auto& mapped : group->ports)
        if (mapped.id == p.module && mapped.input && p.input &&
            mapped.portId == p.port && mapped.route >= 0)
          module->getParamQuantity(Module::INPUT_GAIN_BASE + mapped.route)
              ->setImmediateValue(p.gain);
    }
  group->exposeSelectionDefaults();
  group->syncInputCables(false);
  group->saveGroup();
  rack->setModulePosNearest(group, minimum);
  rack->deselectAll();
  rack->select(group, true);
  change->newModuleJ = group->toJson();
  change->after.capture();
  transaction->push(change);
  APP->history->push(transaction);
}

// Absorb other containers into this owner instead of creating a replacement.
// Their native Eject transactions preserve IDs; re-exposure restores input
// gains, locks and feedback, including connections between the old panels.
void Beekapoo::mergeSelection() {
  auto* rack = homeRack;
  std::vector<int64_t> owners;
  std::set<int64_t> children, panels;
  std::set<std::pair<int64_t, int>> params, locks;
  struct Mapping {
    int64_t id;
    int port;
    bool input;
    float gain;
  };
  std::vector<Mapping> mappings;
  for (auto* selected : rack->getSelected()) {
    if (!canAbsorb(selected)) continue;
    if (auto* other = dynamic_cast<Beekapoo*>(selected)) {
      owners.push_back(other->module->id);
      for (const auto& member : other->members) children.insert(member.id);
      for (const auto& p : other->parameters) params.insert({p.id, p.paramId});
      locks.insert(other->randomLocks.begin(), other->randomLocks.end());
      panels.insert(other->panelCableIds.begin(), other->panelCableIds.end());
      for (const auto& p : other->ports)
        mappings.push_back(
            {p.id, p.portId, p.input,
             p.route >= 0
                 ? other->module
                       ->getParamQuantity(Module::INPUT_GAIN_BASE + p.route)
                       ->getValue()
                 : 1.f});
      for (auto* cw : rack->getCompleteCables())
        if (cw->cable->inputModule == other->module &&
            cw->cable->inputId >= Module::inputBase)
          panels.insert(cw->cable->id);
    } else
      children.insert(selected->module->id);
  }
  if (owners.empty()) return;
  auto* transaction = new TopologyHistory(module->id);
  transaction->name = "merge Beekapoos";
  closeView();
  for (auto id : owners)
    if (auto* other = dynamic_cast<Beekapoo*>(rack->getModule(id)))
      other->ejectAction(transaction, false);
  rack->deselectAll();
  for (auto id : children)
    if (auto* w = rack->getModule(id)) rack->select(w, true);
  addSelection(transaction, true);
  auto* change = beginAction("restore merged controls");
  randomLocks.insert(locks.begin(), locks.end());
  panelCableIds.insert(panels.begin(), panels.end());
  for (const auto& p : params) exposeParameter(p.first, p.second);
  for (const auto& p : mappings)
    if (auto* w = find(p.id)) {
      auto* source = p.input ? w->getInput(p.port) : w->getOutput(p.port);
      if (source) expose(w, source);
      for (const auto& mapped : ports)
        if (mapped.id == p.id && mapped.input && p.input &&
            mapped.portId == p.port && mapped.route >= 0)
          module->getParamQuantity(Module::INPUT_GAIN_BASE + mapped.route)
              ->setImmediateValue(p.gain);
    }
  syncInputCables(expanded);
  compactMembers();
  layoutControls();
  saveGroup();
  finishAction(change, transaction);
  rack->deselectAll();
  rack->select(this, true);
  APP->history->push(transaction);
}
