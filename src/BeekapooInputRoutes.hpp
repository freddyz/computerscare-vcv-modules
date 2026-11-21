#pragma once
// Native Rack endpoints/cables provide summing, cable dragging, persistence,
// MIDI mapping and undo. Only the per-exposed-input multiplication is new DSP.
bool Beekapoo::inputBridge(const engine::Cable* c) const {
  if (!c || c->outputModule != module) return false;
  for (const auto& p : ports)
    if (p.route >= 0 && c->outputId == p.route && c->inputModule->id == p.id &&
        c->inputId == p.portId)
      return true;
  return false;
}
void Beekapoo::rewireInput(app::CableWidget* cw, app::PortWidget* target) {
  auto* c = cw->cable;
  APP->engine->removeCable(c);
  c->inputModule = target->module;
  c->inputId = target->portId;
  APP->engine->addCable(c);
  // setCable() destroys a previously owned engine cable. Transfer ownership
  // out before refreshing its native endpoint widgets with that same cable.
  cw->cable = nullptr;
  cw->setCable(c);
}
void Beekapoo::attachInput(Port& p, int preferred, int64_t bridge, float gain) {
  auto* m = getModule<Module>();
  auto* target = originalPort(p);
  if (!m || !target) return;
  auto used = [&](int slot) {
    for (const auto& other : ports)
      if (&other != &p && other.route == slot) return true;
    return false;
  };
  int slot = preferred;
  if (slot < 0 || slot >= Module::routeCapacity || used(slot)) {
    for (slot = 0; slot < Module::routeCapacity && used(slot); ++slot) {
    }
  }
  if (slot >= Module::routeCapacity) return;
  p.route = slot;
  ensureInputEndpoints(slot);
  auto* info = target->getPortInfo();
  std::string label = info ? info->getName() : "Input";
  m->configInput(Module::inputBase + slot, label);
  m->getParamQuantity(Module::INPUT_GAIN_BASE + slot)->name =
      label + " attenuverter";
  m->params[Module::INPUT_GAIN_BASE + slot].setValue(clamp(gain, -1.f, 1.f));
  auto* knob = new MappedParam<SmallKnob>;
  knob->module = m;
  knob->paramId = Module::INPUT_GAIN_BASE + slot;
  knob->initParamQuantity();
  int64_t id = p.id;
  knob->caption = [this, id, slot] {
    auto* q =
        getModule<Module>()->getParamQuantity(Module::INPUT_GAIN_BASE + slot);
    return memberName(id) + "\n" + q->getLabel() + ": " +
           q->getDisplayValueString() + q->getUnit();
  };
  controls->addChild(knob);
  p.gain = knob;
  p.face->gain = knob;
  p.face->source = getInput(Module::inputBase + slot);
  // Existing internal wires stay direct. Only exterior sources pass through
  // the new attenuverter, alongside any internal sources at the native sum.
  for (auto* cw : homeRack->getCompleteCables())
    if (cw->inputPort == target && !contains(cw->cable->outputModule) &&
        cw->cable->outputModule != m)
      rewireInput(cw, p.face->source.get());
  app::CableWidget* found = nullptr;
  for (auto* cw : homeRack->getCompleteCables())
    if (cw->cable->outputModule == m && cw->cable->outputId == slot &&
        cw->inputPort == target)
      found = cw;
  if (!found) {
    auto* c = new engine::Cable;
    c->outputModule = m;
    c->outputId = slot;
    c->inputModule = target->module;
    c->inputId = target->portId;
    if (bridge >= 0 && !APP->engine->getCable(bridge)) c->id = bridge;
    APP->engine->addCable(c);
    found = new app::CableWidget;
    found->setCable(c);
    homeRack->addCable(found);
  }
  p.bridge = found->cable->id;
  found->visible = false;
  showPlugs(found, false);
  m->enableInput(slot, true);
}
void Beekapoo::detachInput(Port& p) {
  if (p.route < 0) return;
  auto* m = getModule<Module>();
  if (m) m->enableInput(p.route, false);
  auto* target = originalPort(p);
  auto* proxy = getInput(Module::inputBase + p.route);
  for (auto* cw : homeRack->getCompleteCables())
    if (cw->inputPort == proxy && target) rewireInput(cw, target);
  if (auto* cw = homeRack->getCable(p.bridge)) {
    homeRack->removeCable(cw);
    delete cw;
  }
  if (p.gain) {
    controls->removeChild(p.gain);
    delete p.gain;
    p.gain = nullptr;
    p.face->gain = nullptr;
  }
  p.route = -1;
  p.bridge = -1;
  p.face->source = target;
}
void Beekapoo::ensureInputEndpoints(int slot) {
  if (slot < 0 || slot >= Module::routeCapacity ||
      getInput(Module::inputBase + slot))
    return;
  auto* in =
      createInput<app::PortWidget>(Vec(), module, Module::inputBase + slot);
  in->box.size = Vec(16, 16);
  in->visible = false;
  addInput(in);
  auto* out = createOutput<app::PortWidget>(Vec(), module, slot);
  out->box.size = Vec(16, 16);
  out->visible = false;
  addOutput(out);
}
void Beekapoo::layoutInputGains() {
  for (auto* item : controls->children)
    if (auto* face = dynamic_cast<PortFace*>(item))
      if (face->gain && face->visible) {
        if (auto* visual = dynamic_cast<ParamVisual*>(face->gain))
          visual->setVisualSize(20);
        face->gain->box.pos = face->box.pos + Vec(-2, -23);
        face->gain->visible = true;
      }
}
