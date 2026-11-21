#pragma once
template<class Add, class Require>
void testBeekSectionControls(const std::string& output, float scale, Add add, Require require) {
  using beekapoo::Module;
  using Key = std::pair<int64_t, int>;
  auto* rack = APP->scene->rack;
  auto* utility = plugin::getModel("Fundamental", "8vert");
  require(utility, "Section controls need Fundamental 8vert");
  rack->clear(); APP->history->clear();
  auto* child = add(utility, Vec());
  auto* other = add(utility, Vec(90, 0));
  auto* locked = add(utility, Vec(180, 0));
  auto* outside = add(utility, Vec(1600, 0));
  auto* owner = static_cast<beekapoo::Beekapoo*>(add(modelBeekapooPreview, Vec(400, 0)));
  auto ownerId = owner->module->id;
  rack->deselectAll();
  for (auto* w : {child, other, locked}) rack->select(w, true);
  owner->collapse();
  for (auto* w : {child, other, locked})
    for (int i = 0; i < 8; ++i) w->module->getParamQuantity(i)->setImmediateValue(-.7f + i * .19f);
  owner->exposeParameter(child->module->id, 0);
  owner->exposeParameter(child->module->id, 1);
  owner->exposeParameter(locked->module->id, 0);
  for (int i = 0; i < 3; ++i) {
    owner->expose(child, child->getInput(i));
    owner->expose(child, child->getOutput(i));
  }
  owner->expose(locked, locked->getInput(0));
  owner->expose(locked, locked->getOutput(0));
  owner->randomLocks.insert({child->module->id, 0});
  owner->randomLocks.insert({other->module->id, 7}); // hidden locked parameter
  owner->randomLocks.insert({locked->module->id, -1});
  auto proxy = [&](int input) {
    for (const auto& p : owner->ports)
      if (p.id == child->module->id && p.input && p.portId == input) return owner->findPort(p);
    return static_cast<app::PortWidget*>(nullptr);
  };
  auto connect = [&](app::PortWidget* out, app::PortWidget* in) {
    auto* c = new engine::Cable;
    c->outputModule = out->module; c->outputId = out->portId;
    c->inputModule = in->module; c->inputId = in->portId;
    APP->engine->addCable(c);
    auto* cw = new app::CableWidget; cw->setCable(c); rack->addCable(cw); return cw;
  };
  connect(outside->getOutput(0), proxy(0));
  connect(outside->getOutput(1), proxy(0)); // stacked input
  connect(child->getOutput(0), outside->getInput(0));
  auto* loopback = connect(child->getOutput(1), proxy(1));
  // Real panel gestures record this immediately in PortFace::patched.
  owner->panelCableIds.insert(loopback->cable->id);
  connect(child->getOutput(2), other->getInput(3)); // internal wire survives hiding
  owner->pack(); owner->step();
  for (const auto& p : owner->ports)
    if (p.input) p.gain->getParamQuantity()->setValue(-.5f + p.route * .23f);
  owner->saveGroup();
  auto sets = [&] {
    std::array<std::set<Key>, 3> result;
    for (const auto& p : owner->parameters) result[0].insert({p.id, p.paramId});
    for (const auto& p : owner->ports) result[p.input ? 1 : 2].insert({p.id, p.portId});
    return result;
  };
  auto gains = [&] {
    std::map<int, float> result;
    for (const auto& p : owner->ports) if (p.input) result[p.route] = p.gain->getParamQuantity()->getValue();
    return result;
  };
  auto wires = [&] {
    std::map<int64_t, std::string> result;
    for (auto* cw : rack->getCompleteCables()) if (!owner->inputBridge(cw->cable)) {
      auto* c = cw->cable;
      result[c->id] = std::to_string(c->outputModule->id) + ":" + std::to_string(c->outputId) + "/" + std::to_string(c->inputModule->id) + ":" + std::to_string(c->inputId);
    }
    return result;
  };
  std::string phase = "initial";
  auto savedWires = wires(); auto savedGains = gains();
  auto verify = [&] {
    owner = dynamic_cast<beekapoo::Beekapoo*>(rack->getModule(ownerId));
    if (!owner || wires() != savedWires) {
      std::fprintf(stderr, "Section cable mismatch during %s\n", phase.c_str());
      for (const auto& c : savedWires) std::fprintf(stderr, "before %lld %s\n", (long long)c.first, c.second.c_str());
      for (const auto& c : wires()) std::fprintf(stderr, "after  %lld %s\n", (long long)c.first, c.second.c_str());
      require(false, "Section controls must not alter native wiring, stacked cables or self-patches");
    }
    auto mapped = sets();
    for (int kind = 0; kind < 3; ++kind) {
      require(mapped[kind].count({locked->module->id, 0}), "Locked module controls stay exposed");
      require(mapped[kind].count({child->module->id, 0}), "Locked parameter and externally wired jacks stay exposed");
      if (kind) require(mapped[kind].count({child->module->id, 1}), "Self-patched jacks stay exposed");
    }
    require(!mapped[0].count({other->module->id, 7}), "Hidden locked parameter cannot be picked");
    for (auto* w : {child, other, locked})
      for (int i = 0; i < 8; ++i)
        require(std::abs(w->module->getParamQuantity(i)->getValue() - (-.7f + i * .19f)) < .00001f, "Re-rolling exposure must not randomize parameter values");
  };
  for (int kind = 0; kind < 3; ++kind) {
    auto before = sets();
    phase = "dice " + std::to_string(kind);
    owner->sectionRerollButtons[kind]->action();
    auto after = sets(); verify();
    require(after[kind] != before[kind] && after[kind].size() == before[kind].size(), "Dice must change the exposed choices while retaining their count");
    for (int otherKind = 0; otherKind < 3; ++otherKind)
      if (kind != otherKind) require(before[otherKind] == after[otherKind], "Dice must affect only its own section");
    require(gains() == savedGains, "Re-rolling inputs must retain their native gain slots and values");
    phase = "undo dice " + std::to_string(kind); APP->history->undo(); owner->step(); verify(); require(sets() == before && gains() == savedGains, "Undo dice restores the exact mappings and gains");
    phase = "redo dice " + std::to_string(kind); APP->history->redo(); owner->step(); verify(); require(sets() == after && gains() == savedGains, "Redo dice restores the same roll");
    phase = "minus " + std::to_string(kind); before = sets(); owner->sectionRemoveButtons[kind]->action(); after = sets(); verify();
    require(after[kind].size() + 1 == before[kind].size(), "Minus removes exactly one eligible control");
    for (int otherKind = 0; otherKind < 3; ++otherKind)
      if (kind != otherKind) require(before[otherKind] == after[otherKind], "Minus must affect only its own section");
    require(!owner->removeExposedControl(kind) && !owner->rerollExposedControls(kind), "Protected-only section must be a no-op");
    APP->history->undo(); owner->step(); verify(); require(sets() == before && gains() == savedGains, "Undo minus restores mappings and gains in one step");
  }
  rack->clear(); APP->history->clear();
  for (int example = 0; example < 4; ++example) {
    rack->clear(); APP->history->clear();
    owner = static_cast<beekapoo::Beekapoo*>(add(modelBeekapooPreview, Vec()));
    require(owner->generateDefaultPatch(3 + example), "Section preview requires a real generated patch");
    owner->getModule<Module>()->panelTheme.store(example % 2 ? 2 : 1);
    float widths[] = {45, 90, 180, 240}; owner->requestedWidth = widths[example];
    owner->layoutControls(); owner->step();
    int kind = example % 3;
    auto* block = kind == 0 ? owner->frame : kind == 1 ? owner->inputFrame : owner->outputFrame;
    owner->updateSectionAddButtons(block->box.getCenter());
    std::array<beekapoo::ActionButton*, 3> buttons{{owner->sectionRemoveButtons[kind], owner->sectionRerollButtons[kind], owner->sectionAddButtons[kind]}};
    for (int i = 0; i < 3; ++i) {
      auto* b = buttons[i];
      require(b->visible && b->box.pos.x >= 0 && b->box.getRight() <= owner->box.size.x, "All three section buttons must fit even at the minimum width");
      if (i) require(buttons[i - 1]->box.getRight() <= b->box.pos.x, "Section action buttons must not overlap");
      owner->updateSectionAddButtons(b->box.getCenter());
      require(b->visible && buttons[0]->visible && buttons[2]->visible, "Hover must stay open across the action strip");
    }
    auto* parent = owner->parent; auto position = owner->box.pos;
    parent->removeChild(owner); owner->box.pos = Vec();
    capture(owner, output + string::f("/section-controls-%d.png", example + 1), scale, false);
    owner->box.pos = position; parent->addChild(owner);
    if (example == 0 && std::getenv("BEEKAPOO_SECTION_TOOLTIPS")) {
      bool previous = settings::tooltips; settings::tooltips = true;
      for (int i = 0; i < 3; ++i) {
        widget::Widget::EnterEvent enter; buttons[i]->onEnter(enter);
        auto* tip = buttons[i]->tooltip.get(); tip->step();
        auto* parent = tip->parent; parent->removeChild(tip); tip->box.pos = Vec();
        capture(tip, output + string::f("/section-tooltip-%d.png", i), scale, false);
        parent->addChild(tip); buttons[i]->clearTooltip();
      }
      settings::tooltips = previous;
    }
    owner->updateSectionAddButtons(Vec(-1, -1));
    for (auto* b : buttons) require(!b->visible, "Section actions must hide on leave");
  }
  rack->clear(); APP->history->clear();
  std::puts("Section dice/minus: counts, choice changes, locks, native values, stacked/self-patched cables, attenuverter IDs/gains, Undo/Redo and compact hover strips passed");
}
