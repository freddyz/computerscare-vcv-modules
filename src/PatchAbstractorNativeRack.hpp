#pragma once
// Included inside patchabstractor, after the subview declarations.
struct ScopedSubviewOpacity {
  float previous;
  explicit ScopedSubviewOpacity(float opacity)
      : previous(settings::cableOpacity) {
    settings::cableOpacity = opacity;
  }
  ~ScopedSubviewOpacity() { settings::cableOpacity = previous; }
};
struct SubpatchNativeRack : widget::OpaqueWidget {
  SubpatchView* view;
  app::RackWidget* main;
  widget::Widget* mainPlugs = nullptr;
  widget::Widget *modules = new widget::Widget, *plugs = new widget::Widget,
                 *cables = new widget::Widget;
  widget::Widget* savedParent = nullptr;
  math::Rect savedBox;
  math::Rect savedContainerBoxes[3];
  app::ParamWidget* savedTouched = nullptr;
  float drawnOpacity = -1.f;
  static SubpatchNativeRack*& active() {
    static thread_local SubpatchNativeRack* value = nullptr;
    return value;
  }
  explicit SubpatchNativeRack(SubpatchView* v)
      : view(v), main(v->owner->homeRack) {
    // PlugWidget is opaque in the SDK. Locate its public parent by identity.
    auto* probe = new app::CableWidget;
    main->addCable(probe);
    for (auto* container : main->children)
      for (auto* child : container->children)
        if (static_cast<void*>(child) == static_cast<void*>(probe->inputPlug))
          mainPlugs = container;
    main->removeCable(probe);
    delete probe;
    assert(mainPlugs);
    box.size = Vec();
    addChild(modules);
    addChild(plugs);
    addChild(cables);
  }
  void exchangeChildren(widget::Widget* a, widget::Widget* b) {
    a->children.swap(b->children);
    for (auto* child : a->children) child->parent = a;
    for (auto* child : b->children) child->parent = b;
  }
  void enter() {
    savedParent = main->parent;
    savedBox = main->box;
    savedTouched = main->getTouchedParam();
    main->parent = parent;
    main->box = box;
    widget::Widget* canonical[] = {main->getModuleContainer(), mainPlugs,
                                   main->getCableContainer()};
    widget::Widget* local[] = {modules, plugs, cables};
    for (int i = 0; i < 3; ++i) {
      savedContainerBoxes[i] = canonical[i]->box;
      canonical[i]->box = Rect(Vec(), box.size);
      exchangeChildren(canonical[i], local[i]);
    }
    active() = this;
  }
  void leave() {
    widget::Widget* canonical[] = {main->getModuleContainer(), mainPlugs,
                                   main->getCableContainer()};
    widget::Widget* local[] = {modules, plugs, cables};
    for (int i = 0; i < 3; ++i) {
      exchangeChildren(canonical[i], local[i]);
      canonical[i]->box = savedContainerBoxes[i];
    }
    main->parent = savedParent;
    main->box = savedBox;
    main->setTouchedParam(savedTouched);
    active() = nullptr;
  }
  widget::Widget* getModuleContainer() {
    return active() == this ? main->getModuleContainer() : modules;
  }
  widget::Widget* getCableContainer() {
    return active() == this ? main->getCableContainer() : cables;
  }
  void addModule(app::ModuleWidget* w) {
    ScopedSubviewRack scope(this);
    main->addModule(w);
  }
  void addCable(app::CableWidget* w) {
    ScopedSubviewRack scope(this);
    main->addCable(w);
  }
  void removeCable(app::CableWidget* w) {
    ScopedSubviewRack scope(this);
    main->removeCable(w);
  }
  app::CableWidget* getCable(int64_t id) {
    ScopedSubviewRack scope(this);
    return main->getCable(id);
  }
  std::vector<app::CableWidget*> getCompleteCables() {
    ScopedSubviewRack scope(this);
    return main->getCompleteCables();
  }
  std::vector<app::ModuleWidget*> getModules() {
    ScopedSubviewRack scope(this);
    return main->getModules();
  }
  // This fixed subview never selects or moves modules in Rack's selection set.
  void deselectAll() {}
  void step() override;
  void draw(const DrawArgs& args) override;
  void drawLayer(const DrawArgs&, int) override {}
  void onHover(const HoverEvent& e) override {
    ScopedSubviewRack scope(this);
    main->onHover(e);
    if (e.getTarget() == main) e.setTarget(this);
  }
  void onDragHover(const DragHoverEvent& e) override {
    ScopedSubviewRack scope(this);
    main->onDragHover(e);
    if (e.getTarget() == main) e.setTarget(this);
  }
  void onDragStart(const DragStartEvent& e) override { e.consume(this); }
  void onButton(const ButtonEvent& e) override {
    // The local children are drawing/event proxies, not a second Rack instance.
    widget::Widget::onButton(e);
    if (!e.isConsumed()) e.consume(this);
  }
  void onHoverKey(const HoverKeyEvent& e) override {
    widget::Widget::onHoverKey(e);
    e.stopPropagating();
  }
};
ScopedSubviewRack::ScopedSubviewRack(SubpatchNativeRack* rack)
    : previous(SubpatchNativeRack::active()) {
  if (previous == rack) return;
  if (previous) previous->leave();
  if (rack) rack->enter();
}
ScopedSubviewRack::ScopedSubviewRack(app::RackWidget*)
    : ScopedSubviewRack(static_cast<SubpatchNativeRack*>(nullptr)) {}
ScopedSubviewRack::~ScopedSubviewRack() {
  auto* current = SubpatchNativeRack::active();
  if (current == previous) return;
  if (current) current->leave();
  if (previous) previous->enter();
}
struct SubpatchNativePort : app::PortWidget {
  SubpatchView* view;
  WeakPtr<app::ModuleWidget> sourceModule;
  WeakPtr<app::PortWidget> source;
  WeakPtr<ui::Tooltip> tooltip;
  WeakPtr<widget::Widget> tooltipAnchor;
  bool hovering = false;
  SubpatchNativePort(SubpatchView* v, app::ModuleWidget* m, app::PortWidget* p)
      : view(v), sourceModule(m), source(p) {
    module = p->module;
    portId = p->portId;
    type = p->type;
    box.size = p->box.size;
  }
  bool locked() const {
    return source && view->owner && view->owner->boundaryPort(source.get());
  }
  void clearTooltip() {
    if (source) {
      LeaveEvent e;
      ScopedSubviewRack scope(view->owner ? view->owner->homeRack
                                          : APP->scene->rack);
      source->onLeave(e);
    }
    tooltip = nullptr;
    if (tooltipAnchor) tooltipAnchor->requestDelete();
    tooltipAnchor = nullptr;
  }
  ~SubpatchNativePort() override { clearTooltip(); }
  void step() override {
    visible = source && sourceModule && module;
    if (!visible) return;
    for (auto* p = static_cast<widget::Widget*>(source.get());
         p && p != sourceModule.get(); p = p->parent)
      if (!p->visible) visible = false;
    box.pos = source->getRelativeOffset(Vec(), sourceModule.get());
    box.size = source->box.size;
    if (view->owner)
      for (const auto& p : view->owner->ports)
        if (view->owner->findPort(p) == source.get()) {
          box.pos = source->parent->getRelativeOffset(p.originalPosition,
                                                      sourceModule.get());
          break;
        }
  }
  void draw(const DrawArgs& args) override {
    if (!view->owner || (!view->exposeMode && !locked())) return;
    bool selected = view->exposeMode && view->owner->exposed(source.get());
    if (!selected && !hovering && !locked()) return;
    auto color = locked() ? nvgRGB(228, 193, 74) : COLOR_COMPUTERSCARE_GREEN;
    nvgBeginPath(args.vg);
    nvgRect(args.vg, -2, -2, box.size.x + 4, box.size.y + 4);
    if (selected) {
      auto fill = color;
      fill.a = .18f;
      nvgFillColor(args.vg, fill);
      nvgFill(args.vg);
    }
    nvgStrokeColor(args.vg, color);
    nvgStrokeWidth(args.vg, locked() ? 2 : 1.3f);
    nvgStroke(args.vg);
  }
  void onEnter(const EnterEvent& e) override {
    hovering = true;
    if (!source || !view->owner) return;
    clearTooltip();
    std::set<widget::Widget*> before(APP->scene->children.begin(),
                                     APP->scene->children.end());
    {
      ScopedSubviewRack scope(view->owner->homeRack);
      source->onEnter(e);
    }
    for (auto* child : APP->scene->children)
      if (!before.count(child))
        if (auto* tip = dynamic_cast<ui::Tooltip*>(child)) {
          tooltip = tip;
          break;
        }
    if (tooltip) {
      auto* anchor = new SubpatchTooltipAnchor;
      anchor->tooltip = tooltip;
      anchor->anchor = this;
      WeakPtr<SubpatchNativePort> live = this;
      anchor->dismiss = [live] {
        if (live) live->clearTooltip();
      };
      APP->scene->addChild(anchor);
      tooltipAnchor = anchor;
    }
  }
  void onLeave(const LeaveEvent&) override {
    hovering = false;
    clearTooltip();
  }
  void onButton(const ButtonEvent& e) override;
  void onDragStart(const DragStartEvent& e) override;
  void onDragEnd(const DragEndEvent& e) override;
  void onDragDrop(const DragDropEvent& e) override;
  void onDragEnter(const DragEnterEvent& e) override;
  void onDragLeave(const DragLeaveEvent& e) override;
};
struct SubpatchNativeMenuItem : ui::MenuItem {
  std::unique_ptr<ui::MenuItem> delegate;
  WeakPtr<SubpatchView> view;
  SubpatchNativeMenuItem(ui::MenuItem* item, SubpatchView* v)
      : delegate(item), view(v) {
    text = item->text;
    rightText = item->rightText;
    disabled = item->disabled;
  }
  void step() override;
  void onAction(const ActionEvent& e) override;
  ui::Menu* createChildMenu() override;
};
inline void wrapSubpatchMenu(ui::Menu* menu, SubpatchView* view) {
  std::vector<ui::MenuItem*> items;
  for (auto* child : menu->children)
    if (auto* item = dynamic_cast<ui::MenuItem*>(child)) items.push_back(item);
  for (auto* item : items) {
    menu->addChildAbove(new SubpatchNativeMenuItem(item, view), item);
    menu->removeChild(item);
  }
}
void SubpatchNativeMenuItem::step() {
  if (!view || view->closing) {
    disabled = true;
    ui::MenuItem::step();
    return;
  }
  {
    ScopedSubviewRack scope(view->nativeRack);
    delegate->step();
  }
  text = delegate->text;
  rightText = delegate->rightText;
  disabled = delegate->disabled;
  ui::MenuItem::step();
}
void SubpatchNativeMenuItem::onAction(const ActionEvent& e) {
  if (view && !view->closing)
    view->nativeAction([&] { delegate->onAction(e); });
}
ui::Menu* SubpatchNativeMenuItem::createChildMenu() {
  if (!view || view->closing) return nullptr;
  ScopedSubviewRack scope(view->nativeRack);
  auto* menu = delegate->createChildMenu();
  if (menu) wrapSubpatchMenu(menu, view.get());
  return menu;
}
void SubpatchNativePort::onButton(const ButtonEvent& e) {
  if (!view->owner || view->closing) {
    e.consume(this);
    return;
  }
  if (view->exposeMode) {
    if (e.button == GLFW_MOUSE_BUTTON_LEFT && e.action == GLFW_PRESS)
      view->owner->togglePort(source.get());
    e.consume(this);
    return;
  }
  if (locked()) {
    e.consume(this);
    return;
  }
  std::set<widget::Widget*> before(APP->scene->children.begin(),
                                   APP->scene->children.end());
  view->nativeAction([&] { app::PortWidget::onButton(e); });
  if (e.button == GLFW_MOUSE_BUTTON_RIGHT && e.action == GLFW_PRESS)
    for (auto* child : APP->scene->children)
      if (!before.count(child))
        for (auto* nested : child->children)
          if (auto* menu = dynamic_cast<ui::Menu*>(nested))
            wrapSubpatchMenu(menu, view);
}
void SubpatchNativePort::onDragStart(const DragStartEvent& e) {
  if (view->exposeMode || locked() || view->closing) return;
  view->nativeAction([&] { app::PortWidget::onDragStart(e); });
}
void SubpatchNativePort::onDragEnd(const DragEndEvent& e) {
  if (view->closing) {
    ScopedSubviewRack scope(view->nativeRack);
    app::PortWidget::onDragEnd(e);
    return;
  }
  view->nativeAction([&] { app::PortWidget::onDragEnd(e); });
}
void SubpatchNativePort::onDragDrop(const DragDropEvent& e) {
  auto* origin = dynamic_cast<SubpatchNativePort*>(e.origin);
  if (view->exposeMode || locked() || !origin || origin->view != view ||
      origin->locked())
    return;
  view->nativeAction([&] { app::PortWidget::onDragDrop(e); });
}
void SubpatchNativePort::onDragEnter(const DragEnterEvent& e) {
  auto* origin = dynamic_cast<SubpatchNativePort*>(e.origin);
  if (view->exposeMode || locked() || !origin || origin->view != view ||
      origin->locked())
    return;
  view->nativeAction([&] { app::PortWidget::onDragEnter(e); });
}
void SubpatchNativePort::onDragLeave(const DragLeaveEvent& e) {
  if (!view->closing)
    view->nativeAction([&] { app::PortWidget::onDragLeave(e); });
}
