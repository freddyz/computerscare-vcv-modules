#pragma once
// Native cable endpoints describe the DSP route. A proxy input belongs to
// this container in Rack, but logically ends at one of its child inputs.
const Beekapoo::Port* Beekapoo::inputRoute(const engine::Cable* cable) const {
  if (cable->inputModule != module) return nullptr;
  for (const auto& p : ports)
    if (p.route >= 0 && cable->inputId == Module::inputBase + p.route)
      return &p;
  return nullptr;
}
bool Beekapoo::panelEndpoint(int64_t id, int port, bool input) const {
  for (const auto& p : ports)
    if (p.id == id && p.portId == port && p.input == input)
      return controlVisible(id, input ? 1 : 2);
  return false;
}
Beekapoo::CableRole Beekapoo::cableRole(const engine::Cable* cable) const {
  if (inputBridge(cable)) return CableRole::Bridge;
  const auto* route = inputRoute(cable);
  bool in = route || contains(cable->inputModule);
  bool out = contains(cable->outputModule);
  if (!in && !out) return CableRole::Unrelated;
  if (in != out) return CableRole::Panel;
  bool mappedInput =
      route || panelEndpoint(cable->inputModule->id, cable->inputId, true);
  bool mappedOutput =
      panelEndpoint(cable->outputModule->id, cable->outputId, false);
  return panelCableIds.count(cable->id) && mappedInput && mappedOutput
             ? CableRole::Panel
             : CableRole::Internal;
}
bool Beekapoo::reconcileCablePresentation() {
  if (cableMutationDepth()) return false;
  std::set<int64_t> current, attenuated;
  bool changed = false;
  for (auto* cw : homeRack->getCompleteCables()) {
    if (discardInvalidBridge(cw)) continue;
    auto* c = cw->cable;
    current.insert(c->id);
    // A native drag/Undo can complete a panel loopback without passing
    // through PortFace::onDragDrop. Infer only genuinely exposed endpoints.
    auto* route = inputRoute(c);
    if (route) attenuated.insert(c->id);
    bool mappedInput =
        route || panelEndpoint(c->inputModule->id, c->inputId, true);
    bool mappedOutput = panelEndpoint(c->outputModule->id, c->outputId, false);
    if (!expanded && !knownCableIds.count(c->id) &&
        !internalCableIds.count(c->id) && mappedInput && mappedOutput)
      changed |= panelCableIds.insert(c->id).second;
    auto role = cableRole(c);
    if (role == CableRole::Unrelated) continue;
    changed |= !knownCableIds.count(c->id);
    if (role == CableRole::Internal) {
      // Repair old merge snapshots too. Keeping a gain route does not mean
      // that its source has a front-panel jack.
      changed |= panelCableIds.erase(c->id) != 0;
      changed |= internalCableIds.insert(c->id).second;
    } else if (role == CableRole::Panel) {
      changed |= internalCableIds.erase(c->id) != 0;
    }
    bool visible =
        role != CableRole::Bridge && (expanded || role == CableRole::Panel);
    cw->visible = visible;
    showPlugs(cw, visible);
  }
  knownCableIds = std::move(current);
  changed |= attenuatedCableIds != attenuated;
  attenuatedCableIds = std::move(attenuated);
  for (auto* ids : {&panelCableIds, &internalCableIds, &attenuatedCableIds})
    for (auto it = ids->begin(); it != ids->end();)
      if (!knownCableIds.count(*it)) {
        it = ids->erase(it);
        changed = true;
      } else
        ++it;
  for (auto it = plugWidgets.begin(); it != plugWidgets.end();)
    if (!knownCableIds.count(it->first))
      it = plugWidgets.erase(it);
    else
      ++it;
  return changed;
}

inline void reconcileBeekCables() {
  if (cableMutationDepth() || !APP || !APP->scene) return;
  std::vector<WeakPtr<Beekapoo>> owners;
  for (auto* w : APP->scene->rack->getModules())
    if (auto* owner = dynamic_cast<Beekapoo*>(w)) owners.push_back(owner);
  for (auto& owner : owners)
    if (owner) {
      owner->resetNameTopology();
      owner->step();
      owner->resetNameTopology();
    }
}
