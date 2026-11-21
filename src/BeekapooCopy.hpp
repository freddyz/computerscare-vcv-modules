#pragma once
// Rack copies visible and hidden cable widgets alike. Snapshot Beek's
// contained wiring first, then lend only the remaining public action jacks
// to the native copy command. Engine cables keep processing throughout.
struct ScopedGroupCopyPolicy {
  struct Saved {
    WeakPtr<Beekapoo> owner;
    bool policy;
    json_t* previous;
    json_t* contents;
  };
  app::RackWidget* rack = APP->scene->rack;
  std::vector<Saved> groups;
  std::vector<app::CableWidget*> borrowed;
  std::map<widget::Widget*, size_t> plugOrder;
  widget::Widget* plugContainer = nullptr;

  static std::vector<Beekapoo*> selected() {
    std::vector<Beekapoo*> result;
    for (auto* w : APP->scene->rack->getSelected())
      if (auto* group = dynamic_cast<Beekapoo*>(w)) result.push_back(group);
    return result;
  }
  explicit ScopedGroupCopyPolicy(bool cables)
      : ScopedGroupCopyPolicy(selected(), cables) {}
  ScopedGroupCopyPolicy(const std::vector<Beekapoo*>& owners, bool cables) {
    // Capture all owners before removing any cable widgets; each snapshot
    // must see the full graph, including panel self-patches and stacked inputs.
    for (auto* group : owners) {
      group->prepareContainedSave();
      auto* m = group->getModule<Module>();
      auto* contents = group->portableContents(cables);
      groups.push_back({group, m->copyBoundary, m->copyContents, contents});
      m->copyBoundary = cables;
      m->copyContents = contents;
    }
    for (auto* cw : rack->getCompleteCables()) {
      auto* c = cw->cable;
      bool handled = false;
      for (auto* group : owners) {
        handled |= c->outputModule == group->module;
        for (const auto& p : group->ports)
          handled |= p.input && p.route >= 0 &&
                     c->inputModule == group->module &&
                     c->inputId == Module::inputBase + p.route;
      }
      if (handled) borrowed.push_back(cw);
    }
    // Preserve Rack's plug stacking order when returning the lent widgets.
    if (!borrowed.empty())
      for (auto* container : rack->children)
        for (auto* child : container->children)
          if (static_cast<void*>(child) == borrowed.front()->inputPlug)
            plugContainer = container;
    if (plugContainer) {
      size_t index = 0;
      for (auto* child : plugContainer->children) plugOrder[child] = index++;
    }
    for (auto* cw : borrowed) rack->removeCable(cw);
  }
  ~ScopedGroupCopyPolicy() {
    for (auto* cw : borrowed) rack->addCable(cw);
    if (plugContainer)
      plugContainer->children.sort([&](widget::Widget* a, widget::Widget* b) {
        auto ia = plugOrder.find(a), ib = plugOrder.find(b);
        return (ia == plugOrder.end() ? plugOrder.size() : ia->second) <
               (ib == plugOrder.end() ? plugOrder.size() : ib->second);
      });
    for (const auto& saved : groups) {
      if (saved.owner) {
        auto* m = saved.owner->getModule<Module>();
        m->copyBoundary = saved.policy;
        m->copyContents = saved.previous;
      }
      json_decref(saved.contents);
    }
  }
};
