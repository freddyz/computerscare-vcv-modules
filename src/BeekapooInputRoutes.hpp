#pragma once
// Native Rack endpoints/cables provide summing, cable dragging, persistence,
// MIDI mapping and undo. Only the per-exposed-input multiplication is new DSP.
bool Beekapoo::inputBridge(const engine::Cable* c) const {
  if (!c || c->outputModule != module) return false;
  for (const auto& p : ports)
    if (p.route >= 0 && c->outputId == p.route &&
        p.targets(c->inputModule->id, c->inputId))
      return true;
  return false;
}
// All outputs owned by the container are private input bridges. Public
// outputs belong to the child modules. Older native duplication could copy
// a bridge back into the original Beek, leaving black plugs and a cross-link.
// Repair that impossible route on load/Undo without touching public cables.
bool Beekapoo::discardInvalidBridge(app::CableWidget* cable) {
  if (cable->cable->outputModule != module || inputBridge(cable->cable))
    return false;
  homeRack->removeCable(cable);
  delete cable;
  return true;
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
  std::string label = p.shared.label.empty()
                          ? (info ? info->getName() : "Input")
                          : p.shared.label;
  m->configInput(Module::inputBase + slot, label);
  m->getParamQuantity(Module::INPUT_GAIN_BASE + slot)->name =
      label + " attenuverter";
  // Cancel any old smoothing target when restoring/reusing this native slot.
  m->getParamQuantity(Module::INPUT_GAIN_BASE + slot)->setImmediateValue(gain);
  auto* knob = new MappedParam<ThemedSmallKnob>;
  knob->module = m;
  knob->paramId = Module::INPUT_GAIN_BASE + slot;
  knob->initParamQuantity();
  knob->caption = [this, slot] {
    auto* q =
        getModule<Module>()->getParamQuantity(Module::INPUT_GAIN_BASE + slot);
    int64_t id = -1;
    for (const auto& p : ports)
      if (p.route == slot) id = p.id;
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
    if ((p.shared.key >= 0 ? p.shared.incoming.count(cw->cable->id) != 0
                           : (cw->cable->inputModule == target->module &&
                              cw->cable->inputId == target->portId &&
                              (!contains(cw->cable->outputModule) ||
                               attenuatedCableIds.count(cw->cable->id)))) &&
        cw->cable->outputModule != m)
      rewireInput(cw, p.face->source.get());
  for (auto* cw : homeRack->getCompleteCables())
    if (cw->cable->inputModule == m &&
        cw->cable->inputId == Module::inputBase + slot)
      attenuatedCableIds.insert(cw->cable->id);
  std::set<int64_t> connected;
  auto connect = [&](app::PortWidget* to, int64_t preferredId) {
    app::CableWidget* found = nullptr;
    for (auto* cw : homeRack->getCompleteCables())
      if (cw->cable->outputModule == m && cw->cable->outputId == slot &&
          cw->inputPort == to && !connected.count(cw->cable->id))
        found = cw;
    if (!found) {
      auto* c = new engine::Cable;
      c->outputModule = m;
      c->outputId = slot;
      c->inputModule = to->module;
      c->inputId = to->portId;
      if (preferredId >= 0 && !APP->engine->getCable(preferredId))
        c->id = preferredId;
      APP->engine->addCable(c);
      found = new app::CableWidget;
      found->setCable(c);
      homeRack->addCable(found);
    }
    connected.insert(found->cable->id);
    found->visible = false;
    showPlugs(found, false);
    return found->cable->id;
  };
  p.bridge = connect(target, bridge);
  for (auto& to : p.shared.extra)
    if (auto* w = find(to.id))
      if (auto* input = w->getInput(to.port))
        to.bridge = connect(input, to.bridge);
  m->setInputSilence(slot, p.shared.key >= 0);
  m->enableInput(slot, true);
}
void Beekapoo::detachInput(Port& p) {
  if (p.route < 0) return;
  auto* m = getModule<Module>();
  if (m) m->enableInput(p.route, false);
  auto* target = originalPort(p);
  auto* proxy = getInput(Module::inputBase + p.route);
  for (auto* cw : homeRack->getCompleteCables())
    if (cw->cable->inputModule == module &&
        cw->cable->inputId == proxy->portId && target)
      rewireInput(cw, target);
  for (auto* cw : homeRack->getCompleteCables())
    if (cw->cable->outputModule == module && cw->cable->outputId == p.route) {
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
        face->gain->box.pos =
            face->gainLaidOut
                ? face->gainCenter - Vec(10, 10)
                : Vec(face->box.getCenter().x - 10, face->box.pos.y - 23);
        face->gain->visible = true;
      }
}
void Beekapoo::syncInputCables(bool editing) {
  if (cableMutationDepth()) return;
  auto cables = homeRack->getCompleteCables();
  for (const auto& p : ports) {
    if (p.route < 0) continue;
    auto* target = originalPort(p);
    auto* proxy = findPort(p);
    if (!target || !proxy) continue;
    if (editing)
      proxy->box.pos =
          target->getRelativeOffset(target->box.size * .5f, homeRack) -
          proxy->parent->getRelativeOffset(Vec(), homeRack) -
          proxy->box.size * .5f;
    for (auto* cw : cables) {
      auto* c = cw->cable;
      // Native editing/Undo can connect an exterior cable directly to the
      // original jack. Retain its ID and route it through the attenuverter.
      if (p.targets(c->inputModule->id, c->inputId) &&
          c->outputModule != module &&
          (p.shared.key >= 0 ? p.shared.incoming.count(c->id) != 0
                             : (!contains(c->outputModule) ||
                                attenuatedCableIds.count(c->id) != 0)))
        rewireInput(cw, proxy);
      if (c->inputModule == module && c->inputId == proxy->portId) {
        // Keep Rack's widget/engine identities consistent. In Edit, position
        // this invisible endpoint at the original jack, without reparenting it.
        cw->inputPort = proxy;
        attenuatedCableIds.insert(c->id);
      }
    }
  }
}
void Beekapoo::withInputCablePresentation(app::PortWidget* target,
                                          const std::function<void()>& action) {
  app::PortWidget* proxy = nullptr;
  for (const auto& p : ports)
    if (!proxy && p.route >= 0 && p.targets(target->module->id, target->portId))
      proxy = findPort(p);
  if (!proxy) {
    action();
    return;
  }
  std::vector<WeakPtr<app::CableWidget>> borrowed;
  for (auto* cw : homeRack->getCompleteCables())
    if (cw->inputPort == proxy) {
      borrowed.push_back(cw);
      cw->inputPort = target;
    }
  // Lend the visible plugs to the original jack only for its native gesture.
  // Persistent aliases break Rack's module deletion and disconnect traversal.
  DEFER({
    for (auto& cw : borrowed)
      if (cw && cw->inputPort == target && cw->cable &&
          cw->cable->inputModule == module &&
          cw->cable->inputId == proxy->portId)
        cw->inputPort = proxy;
  });
  action();
}

// Releasing a live destination retains its branch and attenuation as native
// cables. The other destinations continue to use the existing shared input.
bool Beekapoo::releaseSharedInputs(const std::set<int64_t>& removed,
                                   history::ComplexAction* batch) {
  EjectedInputGains gains(this, false);
  std::set<size_t> split;
  for (const auto& p : ports) {
    if (p.shared.key < 0) continue;
    std::vector<InputDestination> destinations{{p.id, p.portId, p.bridge}};
    destinations.insert(destinations.end(), p.shared.extra.begin(),
                        p.shared.extra.end());
    auto count = destinations.size();
    destinations.erase(std::remove_if(destinations.begin(), destinations.end(),
                                      [&](const InputDestination& d) {
                                        return !removed.count(d.id);
                                      }),
                       destinations.end());
    if (destinations.empty()) continue;
    EjectedInputGains one(this, true, p.route);
    if (one.adapterCount() && !one.model) {
      status = "Install VCV Free to preserve input attenuation";
      return false;
    }
    if (one.routes.empty()) continue;  // one unity destination: normal detach
    if (one.model) {
      gains.model = one.model;
      gains.moduleWidth = one.moduleWidth;
    }
    auto route = one.routes.front();
    route.target = destinations.front().id;
    route.input = destinations.front().port;
    route.extra.assign(destinations.begin() + 1, destinations.end());
    if (destinations.size() < count) split.insert(gains.routes.size());
    gains.routes.push_back(std::move(route));
  }
  // Copy the incoming stack only when some destinations remain inside.
  // The original cable IDs and shared input must remain untouched there.
  for (auto index : split) {
    auto& route = gains.routes[index];
    std::vector<int64_t> copies;
    for (auto id : route.cables)
      if (auto* original = homeRack->getCable(id)) {
        auto* c = new engine::Cable;
        c->outputModule = original->cable->outputModule;
        c->outputId = original->cable->outputId;
        c->inputModule = find(route.target)->module;
        c->inputId = route.input;
        APP->engine->addCable(c);
        auto* cw = new app::CableWidget;
        cw->setCable(c);
        cw->color = original->color;
        homeRack->addCable(cw);
        auto* add = new history::CableAdd;
        add->setCable(cw);
        batch->push(add);
        copies.push_back(c->id);
      }
    route.cables = std::move(copies);
  }
  gains.position = box.pos + Vec(box.size.x, 0);
  for (auto* w : gains.install(this, batch, false))
    homeRack->setModulePosNearest(w, w->box.pos);
  return true;
}

// A shared input survives deletion/removal of one destination. Re-anchor its
// label/layout to the first remaining native jack; the incoming cable IDs stay.
void Beekapoo::pruneSharedInputs(const std::set<int64_t>& removed) {
  for (auto& p : ports) {
    if (p.shared.key < 0) continue;
    for (auto it = p.shared.extra.begin(); it != p.shared.extra.end();) {
      if (removed.count(it->id)) {
        if (auto* c = homeRack->getCable(it->bridge)) {
          homeRack->removeCable(c);
          delete c;
        }
        it = p.shared.extra.erase(it);
      } else
        ++it;
    }
    if (removed.count(p.id) && !p.shared.extra.empty()) {
      if (auto* c = homeRack->getCable(p.bridge)) {
        homeRack->removeCable(c);
        delete c;
      }
      auto first = p.shared.extra.front();
      p.shared.extra.erase(p.shared.extra.begin());
      p.id = first.id;
      p.portId = first.port;
      p.bridge = first.bridge;
      if (auto* target = originalPort(p)) p.originalPosition = target->box.pos;
    }
  }
}
