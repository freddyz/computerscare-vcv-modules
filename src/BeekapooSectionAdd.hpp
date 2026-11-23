#pragma once
// Section actions change exposed controls. Input minus can also fold a
// self-patch inside, preserving the native stack and any input attenuation.
void Beekapoo::updateSectionAddButtons(Vec mouse) {
  auto* dragged = APP->event->getDraggedWidget();
  bool resizing = dragged && (dragged == leftHandle || dragged == rightHandle);
  bool onHandle = (leftHandle && leftHandle->box.contains(mouse)) ||
                  (rightHandle && rightHandle->box.contains(mouse));
  std::array<phlooper::WarpedBlock*, 3> blocks{
      {frame, inputFrame, outputFrame}};
  std::array<Rect, 3> bars;
  for (int kind = 0; kind < 3; ++kind) {
    auto* block = blocks[kind];
    float size = std::min(16.f, (box.size.x - 12.f) / 3.f);
    // Hang the whole strip over the corner, keeping its hit area inside the
    // panel. Narrow sections can borrow room to their left while hovered.
    float right =
        std::min(box.size.x - 5.f, block->box.getRight() + size * .3f);
    Vec origin(std::max(5.f, right - size * 3 - 2),
               block->box.pos.y - size * .4f);
    bars[kind] = Rect(origin, Vec(size * 3 + 2, size));
    std::array<ActionButton*, 3> buttons{{sectionRemoveButtons[kind],
                                          sectionRerollButtons[kind],
                                          sectionAddButtons[kind]}};
    for (int i = 0; i < 3; ++i) {
      buttons[i]->box.size = Vec(size, size);
      buttons[i]->box.pos = origin + Vec(i * (size + 1), 0);
    }
  }
  // Only section hover opens a strip. Its overhanging buttons remain
  // reachable, but hovering or dragging a resize handle always hides it.
  int active = -1;
  if (!resizing && !onHandle) {
    if (hoveredSection >= 0 && blocks[hoveredSection]->visible &&
        bars[hoveredSection].contains(mouse))
      active = hoveredSection;
    else
      for (int kind = 0; kind < 3; ++kind)
        if (blocks[kind]->visible && blocks[kind]->box.contains(mouse)) {
          active = kind;
          break;
        }
  }
  hoveredSection = active;
  for (int kind = 0; kind < 3; ++kind)
    for (auto* button : {sectionAddButtons[kind], sectionRerollButtons[kind],
                         sectionRemoveButtons[kind]}) {
      button->visible = module && controls->visible && blocks[kind]->visible &&
                        kind == active;
      if (!button->visible) button->clearTooltip();
    }
}
Beekapoo::SectionCandidates Beekapoo::sectionCandidates(bool allParameters) {
  SectionCandidates choices;
  const auto& specs = beekapoo_patch::library();
  for (const auto& member : members) {
    auto* w = find(member.id);
    if (!w || randomLocked(member.id, -1)) continue;
    auto inputConnected = [&](int id) {
      auto* p = w->getInput(id);
      return p && !homeRack->getCompleteCablesOnPort(p).empty();
    };
    auto outputUsed = [&](int id) {
      auto* p = w->getOutput(id);
      return p && (exposed(p) || !homeRack->getCompleteCablesOnPort(p).empty());
    };
    const beekapoo_patch::Spec* spec = nullptr;
    for (const auto& candidate : specs)
      if (w->model->plugin->slug == "Fundamental" &&
          w->model->slug == candidate.slug)
        spec = &candidate;
    for (auto* p : w->getParams()) {
      auto* q = p->getParamQuantity();
      auto* button = dynamic_cast<app::Switch*>(p);
      if (!q || !q->isBounded() || q->getRange() <= 0 || exposed(p) ||
          randomLocked(member.id, p->paramId) ||
          (!allParameters && button && button->momentary))
        continue;
      bool useful = allParameters || !spec;
      if (spec && !allParameters)
        for (const auto& control : spec->parameters)
          if (control.id == p->paramId)
            useful = beekapoo_patch::controlUseful(control, inputConnected,
                                                   outputUsed);
      if (useful) choices.parameters.push_back(p);
    }
    for (auto* p : w->getInputs())
      if (!exposed(p) && homeRack->getCompleteCablesOnPort(p).empty()) {
        bool useful = !spec;
        if (spec)
          for (const auto& control : spec->modulationInputs)
            if (control.id == p->portId)
              useful = beekapoo_patch::controlUseful(control, inputConnected,
                                                     outputUsed);
        if (useful) choices.inputs.push_back(p);
      }
    for (auto* p : w->getOutputs())
      if (!exposed(p)) {
        bool useful = !spec;
        if (spec)
          for (const auto& out : spec->outputs)
            if (out.port == p->portId) {
              useful = true;
              for (const auto& req : out.requirements) {
                bool met = false;
                for (int id : req.anyOf) met |= inputConnected(id);
                useful &= met;
              }
            }
        if (useful) choices.outputs.push_back(p);
      }
  }
  return choices;
}
bool Beekapoo::addExposedControl(int kind) {
  if (!module || members.empty() || kind < 0 || kind > 2) return false;
  ScopedSubviewRack mainScope(homeRack);
  auto choices = sectionCandidates();
  // Prefer controls relevant to the current wiring, but manual + must not
  // stop at the generator's curated list. Exposing a control does not fire it.
  if (kind == 0 && choices.parameters.empty())
    choices = sectionCandidates(true);
  if ((kind == 0 && choices.parameters.empty()) ||
      (kind == 1 && choices.inputs.empty()) ||
      (kind == 2 && choices.outputs.empty())) {
    status = "No more eligible controls in this section";
    return false;
  }
  auto* change = beginAction("expose another control");
  if (kind == 0) {
    auto* p = choices.parameters[random::u32() % choices.parameters.size()];
    exposeParameter(p->module->id, p->paramId);
  } else {
    auto& ports = kind == 1 ? choices.inputs : choices.outputs;
    auto* p = ports[random::u32() % ports.size()];
    expose(find(p->module->id), p);
  }
  saveGroup();
  finishAction(change);
  return true;
}

// Wired panel jacks stay put, including self-patches and cables between two
// members made on the outside panel. Internal native wires are untouched.
bool Beekapoo::sectionPortProtected(const Port& p) {
  if (randomLocked(p.id, -1)) return true;
  for (const auto& to : p.shared.extra)
    if (randomLocked(to.id, -1)) return true;
  auto* source = originalPort(p);
  if (!source || (p.shared.key < 0 && boundaryPort(source))) return true;
  for (auto* wire : homeRack->getCompleteCables()) {
    auto* c = wire->cable;
    if (p.input && c->inputModule == module &&
        c->inputId == Module::inputBase + p.route)
      return true;
    if (panelCableIds.count(c->id) &&
        (p.input ? (c->inputModule->id == p.id && c->inputId == p.portId)
                 : (c->outputModule->id == p.id && c->outputId == p.portId)))
      return true;
    // A cable ending at our own proxy input is an exterior loopback even if
    // it was restored before its panel-cable marker was recorded.
    if (!p.input && c->outputModule->id == p.id && c->outputId == p.portId &&
        c->inputModule == module)
      return true;
  }
  return false;
}
bool Beekapoo::rerollExposedControls(int kind) {
  return changeExposedControls(kind, true);
}
bool Beekapoo::removeExposedControl(int kind) {
  return changeExposedControls(kind, false);
}
bool Beekapoo::foldSelfPatchedInput(history::ComplexAction* parent) {
  std::vector<size_t> candidates;
  for (size_t i = 0; i < ports.size(); ++i) {
    const auto& p = ports[i];
    if (!p.input || randomLocked(p.id, -1)) continue;
    bool locked = false;
    for (const auto& to : p.shared.extra) locked |= randomLocked(to.id, -1);
    if (locked) continue;
    auto* target = originalPort(p);
    if (!target || (p.shared.key < 0 && boundaryPort(target))) continue;
    bool connected = false, selfOnly = true;
    for (auto* cw : homeRack->getCompleteCables()) {
      if (p.route >= 0 ? inputRoute(cw->cable) != &p : cw->inputPort != target)
        continue;
      connected = true;
      selfOnly &= contains(cw->cable->outputModule) && exposed(cw->outputPort);
    }
    if (connected && selfOnly) candidates.push_back(i);
  }
  if (candidates.empty()) {
    status = "No unpatched or self-patched inputs to remove";
    return false;
  }
  size_t index = candidates[random::u32() % candidates.size()];
  auto& p = ports[index];
  EjectedInputGains gains(this, p.route >= 0, p.route);
  if (gains.adapterCount() && !gains.model) {
    status = "Install VCV Free to preserve input attenuation";
    return false;
  }
  auto* state = beginAction("internalize exposed input");
  auto* batch = new TopologyHistory(module->id);
  batch->name = state->name;
  CableMutation mutation;
  std::vector<int64_t> routed;
  for (auto* cw : homeRack->getCompleteCables())
    if (p.route >= 0 ? inputRoute(cw->cable) == &p
                     : cw->inputPort == originalPort(p))
      routed.push_back(cw->cable->id);

  // Reuse Eject's native 8vert conversion, but keep the adapter inside.
  // Unity routes need no module; detachInput reconnects those directly.
  gains.position = parkingPosition();
  for (auto* w : gains.install(this, batch, false)) {
    Vec pos = members.front().position;
    for (const auto& member : members)
      if (auto* child = find(member.id))
        if (member.position.x + child->box.size.x > pos.x)
          pos = member.position + Vec(child->box.size.x, 0);
    members.push_back({w->module->id, pos, w, nullptr});
  }
  originalPort(p)->box.pos = p.originalPosition;
  detachInput(p);
  controls->removeChild(p.face);
  delete p.face;
  ports.erase(ports.begin() + index);
  for (auto id : routed) {
    panelCableIds.erase(id);
    attenuatedCableIds.erase(id);
    internalCableIds.insert(id);
  }
  if (!gains.routes.empty()) compactMembers();
  if (!expanded) pack();
  layoutControls();
  mutation.finish();
  reconcileCablePresentation();
  saveGroup();
  finishAction(state, batch);
  if (parent)
    parent->push(batch);
  else
    APP->history->push(batch);
  status = "Input moved inside the subpatch";
  return true;
}
bool Beekapoo::changeExposedControls(int kind, bool reroll,
                                     history::ComplexAction* parent) {
  if (!module || members.empty() || kind < 0 || kind > 2) return false;
  ScopedSubviewRack mainScope(homeRack);
  std::vector<std::pair<int64_t, int>> removable;
  if (kind == 0) {
    for (const auto& p : parameters)
      if (!randomLocked(p.id, p.paramId))
        removable.emplace_back(p.id, p.paramId);
  } else {
    for (const auto& p : ports)
      if (p.input == (kind == 1) && !sectionPortProtected(p))
        removable.emplace_back(p.id, p.portId);
  }
  if (removable.empty()) {
    if (kind == 1 && !reroll) return foldSelfPatchedInput(parent);
    status = "No unlocked controls to change in this section";
    return false;
  }
  std::mt19937 rng(random::u32());
  std::shuffle(removable.begin(), removable.end(), rng);
  auto choices = reroll ? sectionCandidates() : SectionCandidates{};
  auto& portChoices = kind == 1 ? choices.inputs : choices.outputs;
  size_t count =
      reroll ? std::min(removable.size(), kind == 0 ? choices.parameters.size()
                                                    : portChoices.size())
             : 1;
  if (!count) {
    status = "No different eligible controls in this section";
    return false;
  }
  std::shuffle(choices.parameters.begin(), choices.parameters.end(), rng);
  std::shuffle(portChoices.begin(), portChoices.end(), rng);
  auto* change = beginAction(reroll ? "re-roll exposed controls"
                                    : "remove exposed control");
  for (size_t i = 0; i < count; ++i) {
    auto key = removable[i];
    if (kind == 0) {
      auto it = std::find_if(
          parameters.begin(), parameters.end(), [&](const Parameter& p) {
            return p.id == key.first && p.paramId == key.second;
          });
      controls->removeChild(it->knob);
      delete it->knob;
      parameters.erase(it);
      if (reroll)
        exposeParameter(choices.parameters[i]->module->id,
                        choices.parameters[i]->paramId);
    } else {
      auto it = std::find_if(ports.begin(), ports.end(), [&](const Port& p) {
        return p.id == key.first && p.portId == key.second &&
               p.input == (kind == 1);
      });
      int route = it->route;
      float gain = inputGain(*it);
      if (auto* source = originalPort(*it))
        source->box.pos = it->originalPosition;
      detachInput(*it);
      controls->removeChild(it->face);
      delete it->face;
      ports.erase(it);
      if (reroll) {
        auto* replacement = portChoices[i];
        // Reuse the native input slot and gain. Existing MIDI mappings and
        // attenuation stay with that panel input when its destination changes.
        expose(find(replacement->module->id), replacement, route, -1, gain);
      }
    }
  }
  layoutControls();
  saveGroup();
  finishAction(change, parent);
  return true;
}

void Beekapoo::compactSubpatch() {
  if (!module || members.empty()) return;
  ScopedSubviewRack mainScope(homeRack);
  if (reconcileCablePresentation()) saveGroup();
  auto* batch = new TopologyHistory(module->id);
  batch->name = "compact subpatch";
  size_t before = ports.size();
  for (int kind : {1, 2})
    while (changeExposedControls(kind, false, batch)) {
    }
  if (batch->isEmpty())
    delete batch;
  else
    APP->history->push(batch);
  status = std::to_string(before - ports.size()) + " exposed ports removed";
}
