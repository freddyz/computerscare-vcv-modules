#pragma once
// Rack's selection serializer does not tell contained modules whether its
// caller requested external cables. Scope that policy around the stock
// selection command; its placement, selection and history remain native.
struct ScopedGroupCopyPolicy {
  std::vector<std::pair<WeakPtr<Beekapoo>, bool>> groups;
  explicit ScopedGroupCopyPolicy(bool cables) {
    for (auto* w : APP->scene->rack->getSelected())
      if (auto* group = dynamic_cast<Beekapoo*>(w)) {
        auto* m = group->getModule<Module>();
        groups.push_back({group, m->copyBoundary});
        m->copyBoundary = cables;
        group->prepareContainedSave();
      }
  }
  ~ScopedGroupCopyPolicy() {
    for (auto& pair : groups)
      if (pair.first)
        pair.first->getModule<Module>()->copyBoundary = pair.second;
  }
};
inline void deleteSelectionIncludingContents(Beekapoo* target = nullptr) {
  auto* rack = APP->scene->rack;
  if (target && !rack->isSelected(target)) {
    rack->deselectAll();
    rack->select(target, true);
  }
  std::vector<int64_t> members;
  for (auto* w : rack->getSelected())
    if (auto* group = dynamic_cast<Beekapoo*>(w))
      for (auto& member : group->members) members.push_back(member.id);
  for (auto id : members)
    if (auto* w = rack->getModule(id)) rack->select(w, true);
  rack->deleteSelectionAction();
}
void Beekapoo::deleteGroupAction() { deleteSelectionIncludingContents(this); }
struct GroupDeleteMenuItem : ui::MenuItem {
  ui::MenuItem* delegate;
  explicit GroupDeleteMenuItem(ui::MenuItem* original) : delegate(original) {
    text = original->text;
    rightText = original->rightText;
    disabled = original->disabled;
  }
  ~GroupDeleteMenuItem() override { delete delegate; }
  void onAction(const ActionEvent&) override {
    deleteSelectionIncludingContents();
  }
};
struct GroupDuplicateMenuItem : ui::MenuItem {
  ui::MenuItem* delegate;
  bool cables;
  GroupDuplicateMenuItem(ui::MenuItem* original, bool withCables)
      : delegate(original), cables(withCables) {
    text = original->text;
    rightText = original->rightText;
    disabled = original->disabled;
  }
  ~GroupDuplicateMenuItem() override { delete delegate; }
  void onAction(const ActionEvent& e) override {
    ScopedGroupCopyPolicy scope(cables);
    delegate->onAction(e);
  }
};
// Insert the contained removals in the same native delete action. Restoring
// children and cables completes before the recreated container's next step.
inline bool attachContainedRemoval(history::Action* action, int64_t owner,
                                   std::vector<history::Action*>& removals) {
  auto* batch = dynamic_cast<history::ComplexAction*>(action);
  if (!batch) return false;
  for (size_t i = 0; i < batch->actions.size(); ++i) {
    auto* removal = dynamic_cast<history::ModuleRemove*>(batch->actions[i]);
    if (removal && removal->moduleId == owner) {
      // Restore surviving children before native cable undo actions.
      batch->actions.insert(batch->actions.end(), removals.begin(),
                            removals.end());
      removals.clear();
      return true;
    }
    if (attachContainedRemoval(batch->actions[i], owner, removals)) return true;
  }
  return false;
}
struct BeekapooRackHooks : widget::Widget {
  std::vector<std::pair<int64_t, std::vector<int64_t>>> pendingRemovals;
  void deleteSubpatches() {
    auto pending = std::move(pendingRemovals);
    pendingRemovals.clear();
    auto* rack = APP->scene->rack;
    for (auto& request : pending) {
      // Undo can recreate the owner before the next frame. In that case its
      // original children still exist and must not be removed.
      if (APP->engine->getModule(request.first)) continue;
      std::set<int64_t> live;
      for (auto id : request.second)
        if (rack->getModule(id)) live.insert(id);
      if (live.empty()) continue;
      std::vector<history::Action*> removals;
      auto cables = rack->getCompleteCables();
      for (auto* cw : cables)
        if (live.count(cw->cable->inputModule->id) ||
            live.count(cw->cable->outputModule->id)) {
          auto* action = new history::CableRemove;
          action->setCable(cw);
          removals.push_back(action);
          rack->removeCable(cw);
          delete cw;
        }
      for (auto id : live)
        if (auto* w = rack->getModule(id)) {
          auto* action = new history::ModuleRemove;
          action->setModule(w);
          removals.push_back(action);
          rack->removeModule(w);
          delete w;
        }
      auto* state = APP->history;
      for (int i = state->actionIndex - 1; i >= 0 && !removals.empty(); --i)
        if (attachContainedRemoval(state->actions[i], request.first, removals))
          break;
      for (auto* action : removals) delete action;
    }
  }

  bool hasSelectedGroup() const {
    for (auto* w : APP->scene->rack->getSelected())
      if (dynamic_cast<Beekapoo*>(w)) return true;
    return false;
  }
  WeakPtr<ui::Menu> inspectedMenu;
  void wrapMenus(widget::Widget* root) {
    // Compare against Rack's actual menu, including its native labels and
    // shortcuts. This also works with older SDKs and translated Rack builds.
    auto* menu = dynamic_cast<ui::Menu*>(root);
    if (!menu || inspectedMenu.get() == menu) return;
    inspectedMenu = menu;
    ui::Menu native;
    APP->scene->rack->appendSelectionContextMenu(&native);
    if (native.children.size() < 3 ||
        menu->children.size() < native.children.size())
      return;
    auto actual = menu->children.begin();
    for (auto* expected : native.children) {
      auto* child = *actual++;
      if (auto* label = dynamic_cast<ui::MenuLabel*>(expected)) {
        auto* found = dynamic_cast<ui::MenuLabel*>(child);
        if (!found || label->text != found->text) return;
      } else if (auto* item = dynamic_cast<ui::MenuItem*>(expected)) {
        auto* found = dynamic_cast<ui::MenuItem*>(child);
        if (!found || item->text != found->text ||
            item->rightText != found->rightText)
          return;
      } else if (typeid(*expected) != typeid(*child))
        return;
    }
    // Rack's final three selection commands are Duplicate, Duplicate with
    // cables, and Delete. Do not recurse into arbitrary module submenus.
    auto end = actual;
    std::advance(actual, -3);
    int command = 0;
    while (actual != end) {
      auto* item = dynamic_cast<ui::MenuItem*>(*actual++);
      if (!item) return;
      ui::MenuItem* wrapper =
          command == 2
              ? static_cast<ui::MenuItem*>(new GroupDeleteMenuItem(item))
              : static_cast<ui::MenuItem*>(
                    new GroupDuplicateMenuItem(item, command == 1));
      menu->addChildAbove(wrapper, item);
      menu->removeChild(item);
      ++command;
    }
  }
  void step() override {
    deleteSubpatches();
    box.size = APP->scene->box.size;
    if (!hasSelectedGroup()) return;
    for (auto* child : APP->scene->children)
      if (auto* overlay = dynamic_cast<ui::MenuOverlay*>(child))
        for (auto* menu : overlay->children) wrapMenus(menu);
  }
  void onHoverKey(const HoverKeyEvent& e) override {
    bool deleting = (e.key == GLFW_KEY_DELETE || e.key == GLFW_KEY_BACKSPACE) &&
                    !(e.mods & RACK_MOD_MASK);
    bool duplicating = e.key == GLFW_KEY_D && (e.mods & RACK_MOD_CTRL);
    if (e.action != GLFW_PRESS || (!deleting && !duplicating) ||
        APP->scene->browser->visible || !hasSelectedGroup())
      return;
    auto* hovered = APP->event->getHoveredWidget();
    if (!hovered || (!dynamic_cast<app::RackWidget*>(hovered) &&
                     !hovered->getAncestorOfType<app::RackWidget>()))
      return;
    if (deleting) {
      deleteSelectionIncludingContents();
      e.consume(this);
      return;
    }
    ScopedGroupCopyPolicy scope(e.mods & GLFW_MOD_SHIFT);
    APP->scene->rack->cloneSelectionAction(e.mods & GLFW_MOD_SHIFT);
    e.consume(this);
  }
};
void installRackHooks() {
  for (auto* child : APP->scene->children)
    if (dynamic_cast<BeekapooRackHooks*>(child)) return;
  APP->scene->addChild(new BeekapooRackHooks);
}

void queueSubpatchDeletion(int64_t owner, const std::vector<int64_t>& members) {
  if (!APP || !APP->scene) return;
  for (auto* child : APP->scene->children)
    if (auto* hooks = dynamic_cast<BeekapooRackHooks*>(child)) {
      hooks->pendingRemovals.push_back({owner, members});
      return;
    }
}

inline bool attachMemberPrune(history::Action* action,
                              const std::set<int64_t>& missing,
                              GroupHistory* change) {
  auto* batch = dynamic_cast<history::ComplexAction*>(action);
  if (!batch) return false;
  for (auto* child : batch->actions) {
    auto* removal = dynamic_cast<history::ModuleRemove*>(child);
    if (removal && missing.count(removal->moduleId)) {
      // Undo native member/cable deletion before restoring its membership.
      batch->actions.insert(batch->actions.begin(), change);
      return true;
    }
    if (attachMemberPrune(child, missing, change)) return true;
  }
  return false;
}
void Beekapoo::pruneMissingMembers() {
  std::set<int64_t> missing;
  for (auto& member : members)
    if (!find(member.id)) missing.insert(member.id);
  if (missing.empty()) return;
  auto* action = beginAction("remove deleted subpatch members");
  for (auto it = parameters.begin(); it != parameters.end();) {
    if (missing.count(it->id)) {
      controls->removeChild(it->knob);
      delete it->knob;
      it = parameters.erase(it);
    } else
      ++it;
  }
  for (auto it = ports.begin(); it != ports.end();) {
    if (missing.count(it->id)) {
      controls->removeChild(it->face);
      delete it->face;
      it = ports.erase(it);
    } else
      ++it;
  }
  for (auto id : missing) subpatchPositions.erase(id);
  members.erase(std::remove_if(members.begin(), members.end(),
                               [&](const Member& member) {
                                 return missing.count(member.id);
                               }),
                members.end());
  layoutControls();
  saveGroup();
  action->newModuleJ = toJson();
  action->after.capture();
  bool attached = false;
  auto* state = APP->history;
  for (int i = state->actionIndex - 1; i >= 0 && !attached; --i)
    attached = attachMemberPrune(state->actions[i], missing, action);
  if (!attached) delete action;
}
