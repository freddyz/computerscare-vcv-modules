#pragma once
inline void disconnectGroupPanels(const std::vector<Beekapoo*>& groups,
                                  const std::set<int64_t>& ordinary = {}) {
  auto* rack = APP->scene->rack;
  auto* batch = new history::ComplexAction;
  batch->name = "disconnect cables";
  for (auto* cable : rack->getCompleteCables()) {
    auto* c = cable->cable;
    bool remove = ordinary.count(c->inputModule->id) ||
                  ordinary.count(c->outputModule->id);
    for (auto* group : groups) {
      if (group->inputBridge(c)) continue;
      remove |= group->cableRole(c) == Beekapoo::CableRole::Panel ||
                (c->inputModule == group->module && !group->inputRoute(c));
    }
    if (!remove) continue;
    auto* action = new history::CableRemove;
    action->setCable(cable);
    batch->push(action);
    rack->removeCable(cable);
    delete cable;
  }
  if (batch->isEmpty())
    delete batch;
  else
    APP->history->push(batch);
}
void Beekapoo::disconnectPanelAction() { disconnectGroupPanels({this}); }
void disconnectBeekapooSelection() {
  std::vector<Beekapoo*> groups;
  auto selected = APP->scene->rack->getSelected();
  for (auto* w : selected)
    if (auto* group = dynamic_cast<Beekapoo*>(w)) groups.push_back(group);
  std::set<int64_t> ordinary;
  for (auto* w : selected) {
    if (!w->module || dynamic_cast<Beekapoo*>(w)) continue;
    bool included = false;
    for (auto* group : groups) included |= group->contains(w->module);
    if (!included) ordinary.insert(w->module->id);
  }
  disconnectGroupPanels(groups, ordinary);
}
struct GroupDisconnectMenuItem : ui::MenuItem {
  void onAction(const ActionEvent&) override { disconnectBeekapooSelection(); }
};
#include "BeekapooCreateSelection.hpp"
struct CreateBeekapooMenuItem : ui::MenuItem {
  CreateBeekapooMenuItem() {
    text = "Create Beek-A-Poo";
    disabled = !canCreateSelectedGroup();
  }
  void onAction(const ActionEvent&) override {
    if (!disabled) createSelectedGroup();
  }
};
void duplicateBeekapooSelection(bool cables) {
  ScopedGroupCopyPolicy scope(cables);
  APP->scene->rack->cloneSelectionAction(cables);
}
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
  size_t inspectedChildren = 0;
  void wrapMenus(widget::Widget* root) {
    // Compare against Rack's actual menu, including its native labels and
    // shortcuts. This also works with older SDKs and translated Rack builds.
    auto* menu = dynamic_cast<ui::Menu*>(root);
    if (!menu || (inspectedMenu.get() == menu &&
                  inspectedChildren == menu->children.size()))
      return;
    inspectedMenu = menu;
    inspectedChildren = menu->children.size();
    // More than one menu can be open at once. Do not append or wrap again
    // when visiting the original menu after inspecting a submenu.
    for (auto* child : menu->children)
      if (dynamic_cast<CreateBeekapooMenuItem*>(child) ||
          dynamic_cast<GroupDeleteMenuItem*>(child))
        return;
    ui::Menu native;
    APP->scene->rack->appendSelectionContextMenu(&native);
    if (native.children.size() < 3 ||
        menu->children.size() < native.children.size())
      return;
    auto matches = [](widget::Widget* expected, widget::Widget* child) {
      if (auto* label = dynamic_cast<ui::MenuLabel*>(expected)) {
        auto* found = dynamic_cast<ui::MenuLabel*>(child);
        return found && label->text == found->text;
      } else if (auto* item = dynamic_cast<ui::MenuItem*>(expected)) {
        auto* found = dynamic_cast<ui::MenuItem*>(child);
        return found && item->text == found->text &&
               item->rightText == found->rightText;
      }
      return typeid(*expected) == typeid(*child);
    };
    // The context menu starts with the selection commands. Rack's Edit menu
    // has Undo/Redo/etc before the same section. Find the complete native
    // section instead of assuming it is at the beginning of the menu.
    auto actual = menu->children.end();
    bool foundSection = false;
    for (auto start = menu->children.begin(); start != menu->children.end();
         ++start) {
      auto candidate = start;
      bool matched = true;
      for (auto* expected : native.children) {
        if (candidate == menu->children.end() ||
            !matches(expected, *candidate)) {
          matched = false;
          break;
        }
        ++candidate;
      }
      if (matched) {
        actual = candidate;
        foundSection = true;
        break;
      }
    }
    if (!foundSection) return;
    if (!hasSelectedGroup()) {
      if (APP->scene->rack->getSelected().size() >= 2) {
        menu->addChild(new ui::MenuSeparator);
        menu->addChild(new CreateBeekapooMenuItem);
      }
      return;
    }
    // Rack's final three selection commands are Duplicate, Duplicate with
    // cables, and Delete. Do not recurse into arbitrary module submenus.
    auto end = actual;
    if (menu->children.size() >= 5) {
      // Disconnect precedes Bypass and the final three native commands.
      auto position = end;
      std::advance(position, -5);
      if (auto* item = dynamic_cast<ui::MenuItem*>(*position)) {
        auto* replacement = new GroupDisconnectMenuItem;
        replacement->text = item->text;
        replacement->rightText = item->rightText;
        replacement->disabled = item->disabled;
        menu->addChildAbove(replacement, item);
        menu->removeChild(item);
        delete item;
      }
    }
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
    if (APP->scene->rack->getSelected().size() >= 2) {
      menu->addChild(new ui::MenuSeparator);
      menu->addChild(new CreateBeekapooMenuItem);
    }
  }
  void step() override {
    deleteSubpatches();
    box.size = APP->scene->box.size;
    if (!hasSelectedGroup() && APP->scene->rack->getSelected().size() < 2)
      return;
    for (auto* child : APP->scene->children)
      if (auto* overlay = dynamic_cast<ui::MenuOverlay*>(child))
        for (auto* menu : overlay->children) wrapMenus(menu);
  }
  void onHoverKey(const HoverKeyEvent& e) override {
    bool deleting = (e.key == GLFW_KEY_DELETE || e.key == GLFW_KEY_BACKSPACE) &&
                    !(e.mods & RACK_MOD_MASK);
    bool duplicating = e.key == GLFW_KEY_D && (e.mods & RACK_MOD_CTRL);
    bool disconnecting =
        e.key == GLFW_KEY_U && (e.mods & RACK_MOD_MASK) == RACK_MOD_CTRL;
    if (e.action != GLFW_PRESS ||
        (!deleting && !duplicating && !disconnecting) ||
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
    if (disconnecting) {
      disconnectBeekapooSelection();
      e.consume(this);
      return;
    }
    ScopedGroupCopyPolicy scope(e.mods & GLFW_MOD_SHIFT);
    APP->scene->rack->cloneSelectionAction(e.mods & GLFW_MOD_SHIFT);
    e.consume(this);
  }
};
#ifdef ARCH_MAC
inline void retryRackHooksAfterUiStartup() {
  static bool queued = false;
  if (queued || settings::headless) return;
  queued = true;
  // Plugin init precedes Context/Scene creation. Cocoa's main queue is
  // serviced by GLFW on the UI thread, even with no computerscare modules.
  dispatch_after_f(dispatch_time(DISPATCH_TIME_NOW, 10 * NSEC_PER_MSEC),
                   dispatch_get_main_queue(), nullptr, [](void*) {
                     queued = false;
                     installRackHooks();
                   });
}
#endif
void installRackHooks() {
  if (!APP || !APP->scene || !APP->engine) {
#ifdef ARCH_MAC
    retryRackHooksAfterUiStartup();
#endif
    return;
  }
  static WeakPtr<BeekapooRackHooks> installed;
  if (installed && installed->parent == APP->scene) return;
  for (auto* child : APP->scene->children)
    if (auto* hooks = dynamic_cast<BeekapooRackHooks*>(child)) {
      installed = hooks;
      return;
    }
  auto* hooks = new BeekapooRackHooks;
  APP->scene->addChild(hooks);
  installed = hooks;
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
                              history::Action* change) {
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
  auto* transaction = new history::ComplexAction;
  transaction->name = action->name;
  transaction->push(action);
  pruneSharedInputs(missing);
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
      if (it->route >= 0) {
        auto* proxy = findPort(*it);
        for (auto* cw : homeRack->getCompleteCables()) {
          if (cw->cable->inputModule != module ||
              cw->cable->inputId != proxy->portId)
            continue;
          auto* removed = new history::CableRemove;
          removed->setCable(cw);
          transaction->push(removed);
          homeRack->removeCable(cw);
          delete cw;
        }
      }
      detachInput(*it);
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
  compactMembers();
  layoutControls();
  saveGroup();
  refreshGeneratedName();
  action->newModuleJ = toJson();
  action->after.capture();
  bool attached = false;
  auto* state = APP->history;
  for (int i = state->actionIndex - 1; i >= 0 && !attached; --i)
    attached = attachMemberPrune(state->actions[i], missing, transaction);
  if (!attached) delete transaction;
}
