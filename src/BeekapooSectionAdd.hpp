#pragma once
// Section actions change exposed controls without modifying the subpatch.
void Beekapoo::updateSectionAddButtons(Vec mouse) {
  std::array<phlooper::WarpedBlock*, 3> blocks{
      {frame, inputFrame, outputFrame}};
  std::array<Rect, 3> bars;
  float size = std::min(16.f, (box.size.x - 12.f) / 3.f);
  for (int kind = 0; kind < 3; ++kind) {
    auto* block = blocks[kind];
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
  // Keep the strip open while moving between its buttons, even where it
  // overhangs a neighbouring section. Never show two overlapping strips.
  int active = -1;
  if (hoveredSection >= 0 && blocks[hoveredSection]->visible &&
      bars[hoveredSection].contains(mouse))
    active = hoveredSection;
  else
    for (int kind = 0; kind < 3; ++kind)
      if (blocks[kind]->visible && blocks[kind]->box.contains(mouse)) {
        active = kind;
        break;
      }
  hoveredSection = active;
  for (int kind = 0; kind < 3; ++kind)
    for (auto* button : {sectionAddButtons[kind], sectionRerollButtons[kind],
                         sectionRemoveButtons[kind]}) {
      button->visible = module && controls->visible && kind == active;
      if (!button->visible) button->clearTooltip();
    }
}
Beekapoo::SectionCandidates Beekapoo::sectionCandidates() {
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
          randomLocked(member.id, p->paramId) || (button && button->momentary))
        continue;
      bool useful = !spec;
      if (spec)
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
  auto* source = originalPort(p);
  if (!source || boundaryPort(source)) return true;
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
bool Beekapoo::changeExposedControls(int kind, bool reroll) {
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
      float gain = it->gain ? it->gain->getParamQuantity()->getValue() : 1.f;
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
  finishAction(change);
  return true;
}
