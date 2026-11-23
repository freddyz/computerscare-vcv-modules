#pragma once
// Flatten selected Beeks through the same native Eject history, then group
// the released modules. Module/cable IDs survive, including stacked inputs
// and feedback patched through the previous panels. One Undo restores all.
// Snapshot controls and routing before changing membership. Both entry points
// (Create Beek and logo absorption) restore the same independent gain routes
// and panel intent, then classify cables against the final membership.
struct MergedControls {
  struct Mapping {
    int64_t id;
    int port;
    bool input;
    float gain;
    Beekapoo::SharedInput shared;
  };
  std::vector<Mapping> mappings;
  std::set<std::pair<int64_t, int>> parameters, locks;
  std::set<int64_t> panels, attenuated;
  void collect(Beekapoo* owner) {
    for (const auto& p : owner->parameters)
      parameters.insert({p.id, p.paramId});
    locks.insert(owner->randomLocks.begin(), owner->randomLocks.end());
    panels.insert(owner->panelCableIds.begin(), owner->panelCableIds.end());
    attenuated.insert(owner->attenuatedCableIds.begin(),
                      owner->attenuatedCableIds.end());
    for (const auto& p : owner->ports) {
      auto shared = p.shared;
      shared.incoming.clear();
      for (auto* cw : owner->homeRack->getCompleteCables())
        if (owner->inputRoute(cw->cable) == &p)
          shared.incoming.insert(cw->cable->id);
      mappings.push_back(
          {p.id, p.portId, p.input, owner->inputGain(p), shared});
    }
    for (auto* cw : owner->homeRack->getCompleteCables()) {
      if (owner->inputRoute(cw->cable)) attenuated.insert(cw->cable->id);
      if (owner->cableRole(cw->cable) == Beekapoo::CableRole::Panel)
        panels.insert(cw->cable->id);
    }
  }
  void apply(Beekapoo* owner) const {
    owner->randomLocks.insert(locks.begin(), locks.end());
    owner->panelCableIds.insert(panels.begin(), panels.end());
    owner->attenuatedCableIds.insert(attenuated.begin(), attenuated.end());
    for (const auto& p : parameters) owner->exposeParameter(p.first, p.second);
    for (const auto& p : mappings)
      if (auto* w = owner->find(p.id)) {
        auto* source = p.input ? w->getInput(p.port) : w->getOutput(p.port);
        if (source && p.shared.key >= 0) {
          // Collapse can automatically expose the temporary first destination.
          // Replace that provisional mapping before restoring independent
          // buses.
          for (auto it = owner->ports.begin(); it != owner->ports.end();) {
            if (it->input && it->id == p.id && it->portId == p.port &&
                it->shared.key < 0) {
              owner->detachInput(*it);
              owner->controls->removeChild(it->face);
              delete it->face;
              it = owner->ports.erase(it);
            } else
              ++it;
          }
          owner->expose(w, source, -1, -1, p.gain, p.shared);
        } else if (source)
          owner->expose(w, source);
        for (auto& mapped : owner->ports)
          if (mapped.id == p.id && mapped.input && p.input &&
              mapped.portId == p.port && mapped.shared.key == p.shared.key &&
              p.gain != owner->inputGain(mapped)) {
            owner->detachInput(mapped);
            owner->attachInput(mapped, -1, -1, p.gain);
          }
      }
  }
};
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
  std::set<int64_t> children;
  std::vector<int64_t> owners;
  MergedControls controls;
  for (auto* w : selected) {
    minimum.x = std::min(minimum.x, w->box.pos.x);
    minimum.y = std::min(minimum.y, w->box.pos.y);
    maximum.x = std::max(maximum.x, w->box.getRight());
    maximum.y = std::max(maximum.y, w->box.getBottom());
    if (auto* group = dynamic_cast<Beekapoo*>(w)) {
      owners.push_back(w->module->id);
      for (const auto& member : group->members) children.insert(member.id);
      controls.collect(group);
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
  CableMutation mutation;
  auto* transaction = new TopologyHistory(module->id);
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
  controls.apply(group);
  group->exposeSelectionDefaults();
  group->syncInputCables(false);
  mutation.finish();
  group->step();
  group->refreshGeneratedName();
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
  std::set<int64_t> children;
  MergedControls controls;
  controls.collect(this);
  for (auto* selected : rack->getSelected()) {
    if (!canAbsorb(selected)) continue;
    if (auto* other = dynamic_cast<Beekapoo*>(selected)) {
      owners.push_back(other->module->id);
      for (const auto& member : other->members) children.insert(member.id);
      controls.collect(other);
    } else
      children.insert(selected->module->id);
  }
  if (owners.empty()) return;
  CableMutation mutation;
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
  controls.apply(this);
  syncInputCables(expanded);
  compactMembers();
  layoutControls();
  mutation.finish();
  step();
  saveGroup();
  finishAction(change, transaction);
  rack->deselectAll();
  rack->select(this, true);
  APP->history->push(transaction);
}
