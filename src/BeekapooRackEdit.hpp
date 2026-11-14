#pragma once
// A nonmodal session over the actual Rack. No proxy modules or cable aliases.
struct RackEditSession;
struct RackEditHighlight : widget::TransparentWidget {
  WeakPtr<RackEditSession> session;
  WeakPtr<app::ModuleWidget> sourceModule;
  void draw(const DrawArgs& args) override;
};
struct RackEditPreview : widget::OpaqueWidget {
  WeakPtr<Beekapoo> owner;
  void draw(const DrawArgs& args) override {
    if (!owner) return;
    nvgSave(args.vg);
    float scale = std::min(box.size.x / owner->box.size.x,
                           box.size.y / owner->box.size.y);
    nvgScale(args.vg, scale, scale);
    bool visible = owner->controls->visible, expanded = owner->expanded;
    owner->controls->visible = true;
    owner->expanded = false;
    DrawArgs local = args;
    local.clipBox = Rect(args.clipBox.pos / scale, args.clipBox.size / scale);
    owner->draw(local);
    owner->drawLayer(local, 1);
    owner->expanded = expanded;
    owner->controls->visible = visible;
    nvgRestore(args.vg);
  }
};
struct RackEditSession : widget::Widget {
  WeakPtr<Beekapoo> owner;
  Vec previousOffset;
  float previousZoom;
  RackLayoutSnapshot previousSelection;
  std::set<int64_t> known, admitted;
  bool exposeMode = false, showPanel = true, finished = false;
  ActionButton *done, *exposeButton, *panelButton;
  RackEditPreview* preview;
  std::vector<WeakPtr<RackEditHighlight>> highlights;
  explicit RackEditSession(Beekapoo* group) : owner(group) {
    auto* scroll = APP->scene->rackScroll;
    previousOffset = scroll->getGridOffset();
    previousZoom = scroll->getZoom();
    previousSelection.capture();
    for (auto id : APP->engine->getModuleIds()) known.insert(id);
    for (auto& member : group->members) admitted.insert(member.id);
    auto addButton = [this](const std::string& label, float x, float width,
                            std::function<void()> action) {
      auto* button = new ActionButton;
      button->text = label;
      button->configure(width, 27, 3, 2, .8f, int(x) + 975);
      button->box.pos = Vec(x, 7);
      button->action = std::move(action);
      addChild(button);
      return button;
    };
    done = addButton("Done", 10, 65, [this] { finish(true); });
    exposeButton = addButton("Expose", 87, 79, [this] {
      exposeMode = !exposeMode;
      if (owner) owner->saveGroup();
    });
    exposeButton->pressed = [this] { return exposeMode; };
    panelButton = addButton("Panel", 178, 66, [this] {
      showPanel = !showPanel;
      if (owner) {
        owner->editPreview = showPanel;
        owner->layoutControls();
        owner->saveGroup();
      }
    });
    panelButton->pressed = [this] { return showPanel; };
    preview = new RackEditPreview;
    preview->owner = group;
    addChild(preview);
  }
  void adoptNewModules() {
    if (!owner || APP->scene->browser->visible ||
        APP->event->getDraggedWidget())
      return;
    std::vector<app::ModuleWidget*> added;
    for (auto* w : owner->homeRack->getModules()) {
      if (!w->module ||
          (known.count(w->module->id) &&
           (!admitted.count(w->module->id) || owner->contains(w->module))))
        continue;
      known.insert(w->module->id);
      if (w != owner.get() && !dynamic_cast<Beekapoo*>(w) &&
          !owner->ownedElsewhere(w->module) &&
          !w->module->leftExpander.producerMessage &&
          !w->module->rightExpander.producerMessage)
        added.push_back(w);
    }
    if (added.empty()) return;
    auto* action = owner->beginAction("add modules to Beekapoo");
    for (auto* w : added) {
      owner->members.push_back({w->module->id, w->box.pos, w, nullptr});
      admitted.insert(w->module->id);
    }
    owner->syncBoundaryPorts();
    owner->layoutControls();
    owner->saveGroup();
    owner->finishAction(action);
  }
  void finish(bool collapse) {
    if (finished) return;
    finished = true;
    for (auto& highlight : highlights)
      if (highlight) {
        highlight->hide();
        highlight->requestDelete();
      }
    highlights.clear();
    if (owner) {
      owner->editSession = nullptr;
      owner->editPreview = false;
      if (collapse && owner->expanded) {
        if (owner->members.empty()) {
          owner->expanded = false;
          owner->controls->visible = true;
          owner->saveGroup();
        } else
          owner->collapse();
      }
      owner->layoutControls();
    }
    auto* scroll = APP->scene->rackScroll;
    scroll->setZoom(previousZoom);
    scroll->setGridOffset(previousOffset);
    auto* rack = APP->scene->rack;
    rack->deselectAll();
    for (auto id : previousSelection.selected)
      if (auto* w = rack->getModule(id))
        if (w->visible) rack->select(w, true);
    hide();
    requestDelete();
  }
  template <class F>
  void forControls(F visit) {
    if (!owner) return;
    float zoom = APP->scene->rackScroll->getZoom();
    for (auto& member : owner->members)
      if (auto* w = member.widget.get()) {
        for (auto* p : w->getPorts())
          visit(Rect(p->getAbsoluteOffset(Vec()) - box.pos, p->box.size * zoom),
                p, static_cast<app::ParamWidget*>(nullptr));
        for (auto* p : w->getParams())
          visit(Rect(p->getAbsoluteOffset(Vec()) - box.pos, p->box.size * zoom),
                static_cast<app::PortWidget*>(nullptr), p);
      }
  }
  void onHoverKey(const HoverKeyEvent& e) override {
    if (e.action == GLFW_PRESS && e.key == GLFW_KEY_ESCAPE &&
        !APP->scene->browser->visible) {
      finish(true);
      e.consume(this);
      return;
    }
    widget::Widget::onHoverKey(e);
  }
  void onButton(const ButtonEvent& e) override {
    widget::Widget::onButton(e);
    if (e.isConsumed() || !exposeMode || !owner || e.action != GLFW_PRESS ||
        e.button != GLFW_MOUSE_BUTTON_LEFT)
      return;
    forControls([&](Rect r, app::PortWidget* port, app::ParamWidget* param) {
      if (e.isConsumed() || !r.contains(e.pos)) return;
      if (port)
        owner->togglePort(port);
      else
        owner->toggleParameter(param);
      e.consume(this);
    });
  }
  void step() override {
    if (finished) return;
    if (!owner) {
      finish(false);
      return;
    }
    adoptNewModules();
    for (auto& member : owner->members)
      if (auto* w = member.widget.get()) {
        bool found = false;
        for (auto& highlight : highlights)
          if (highlight && highlight->sourceModule.get() == w) {
            highlight->box.size = w->box.size;
            found = true;
          }
        if (found) continue;
        auto* highlight = new RackEditHighlight;
        highlight->session = this;
        highlight->sourceModule = w;
        highlight->box.size = w->box.size;
        std::set<widget::Widget*> roots;
        auto rootOf = [&](widget::Widget* control) {
          while (control && control->parent != w) control = control->parent;
          if (control) roots.insert(control);
        };
        for (auto* control : w->getParams()) rootOf(control);
        for (auto* control : w->getPorts()) rootOf(control);
        widget::Widget* first = nullptr;
        for (auto* child : w->children)
          if (roots.count(child)) {
            first = child;
            break;
          }
        if (first)
          w->addChildBelow(highlight, first);
        else if (w->getPanel())
          w->addChildAbove(highlight, w->getPanel());
        else
          w->addChildBottom(highlight);
        highlights.push_back(highlight);
      }

    box.pos = Vec(0, APP->scene->menuBar->box.getBottom());
    box.size = APP->scene->box.size - box.pos;
    preview->visible = showPanel;
    float width = std::min(280.f, owner->box.size.x);
    preview->box =
        Rect(Vec(std::max(260.f, box.size.x - width - 14), 48),
             Vec(width, width * RACK_GRID_HEIGHT / owner->box.size.x));
    widget::Widget::step();
  }
  void draw(const DrawArgs& args) override {
    nvgBeginPath(args.vg);
    nvgRect(args.vg, 0, 0, box.size.x, 43);
    nvgFillColor(args.vg, nvgRGB(60, 76, 70));
    nvgFill(args.vg);
    auto font =
        APP->window->loadFont(asset::system("res/fonts/DejaVuSans.ttf"));
    nvgFontFaceId(args.vg, font->handle);
    nvgFontSize(args.vg, 13);
    nvgFillColor(args.vg, nvgRGB(214, 225, 218));
    nvgText(args.vg, 260, 25,
            exposeMode ? "Beekapoo: click controls to expose"
                       : "Editing Beekapoo — new modules join this group",
            nullptr);
    widget::Widget::draw(args);
  }
};
void RackEditHighlight::draw(const DrawArgs& args) {
  if (!session || session->finished || !session->exposeMode ||
      !session->owner || !sourceModule || !sourceModule->module)
    return;
  auto* group = session->owner.get();
  auto* module = sourceModule.get();
  auto paint = [&](widget::Widget* control, int index, bool selected,
                   bool locked) {
    if (!control->visible) return;
    for (auto* parent = control->parent; parent && parent != module;
         parent = parent->parent)
      if (!parent->visible) return;
    Rect r(control->getRelativeOffset(Vec(), module) - Vec(3, 3),
           control->box.size + Vec(6, 6));
    if (!r.pos.isFinite() || !r.size.isFinite()) return;
    uint32_t seed =
        uint32_t(module->module->id) ^ uint32_t(index + 1) * 2654435761u;
    float a = float(seed % 9) * .25f, b = float((seed >> 5) % 9) * .25f;
    nvgBeginPath(args.vg);
    nvgMoveTo(args.vg, r.pos.x - 1, r.pos.y + a);
    nvgLineTo(args.vg, r.getRight(), r.pos.y - 1 - b);
    nvgLineTo(args.vg, r.getRight() + b, r.getBottom() - a);
    nvgLineTo(args.vg, r.pos.x + a, r.getBottom() + 1);
    nvgClosePath(args.vg);
    nvgFillColor(args.vg, locked     ? nvgRGBA(214, 179, 48, 155)
                          : selected ? nvgRGBA(28, 195, 147, 155)
                                     : nvgRGBA(95, 143, 117, 90));
    nvgFill(args.vg);
  };
  for (auto* p : module->getParams())
    paint(p, p->paramId, group->exposed(p), false);
  for (auto* p : module->getPorts())
    paint(p, 1000 + p->portId + int(p->type) * 100, group->exposed(p),
          group->boundaryPort(p));
}
void Beekapoo::closeEditSession() {
  if (auto* session = dynamic_cast<RackEditSession*>(editSession.get()))
    session->finish(false);
}
void Beekapoo::editSubpatch() {
  if (editSession) {
    static_cast<RackEditSession*>(editSession.get())->finish(true);
    return;
  }
  // One active editing target, so automatic membership is unambiguous.
  for (auto* w : homeRack->getModules())
    if (auto* group = dynamic_cast<Beekapoo*>(w))
      if (group != this && group->editSession)
        static_cast<RackEditSession*>(group->editSession.get())->finish(true);
  closeView();
  auto* session = new RackEditSession(this);
  revealMembers(true);
  // Restore originals as direct Rack children. Native movement, hotkeys,
  // module menus, selection and browser placement now use their real widgets.
  for (auto& member : members)
    if (auto* holder = member.holder.get()) {
      auto* w = member.widget.get();
      homeRack->select(holder, false);
      auto* parent = holder->parent;
      Vec position = holder->box.pos;
      holder->removeChild(w);
      parent->removeChild(holder);
      w->box.pos = position;
      w->visible = true;
      parent->addChild(w);
      member.holder = nullptr;
      delete holder;
    }
  float width = 0;
  for (auto& member : members)
    if (member.widget) width += member.widget->box.size.x;
  Vec origin(std::floor(box.pos.x / RACK_GRID_WIDTH) * RACK_GRID_WIDTH,
             (std::floor(box.pos.y / RACK_GRID_HEIGHT) + 4) * RACK_GRID_HEIGHT);
  bool blocked;
  do {
    blocked = false;
    for (auto* w : homeRack->getModules())
      if (w->visible && !contains(w->module) &&
          Rect(origin, Vec(width, RACK_GRID_HEIGHT)).intersects(w->box)) {
        origin.y += RACK_GRID_HEIGHT;
        blocked = true;
        break;
      }
  } while (blocked);
  Vec cursor = origin;
  homeRack->deselectAll();
  for (auto& member : members)
    if (auto* w = member.widget.get()) {
      w->box.pos = cursor;
      member.position = cursor;
      cursor.x += w->box.size.x;
    }
  expanded = true;
  editPreview = true;
  controls->visible = false;
  editSession = session;
  layoutControls();
  saveGroup();
  APP->scene->addChildBelow(session, APP->scene->browser);
  auto* scroll = APP->scene->rackScroll;
  // Keep normal zoom and leave breathing room below the editing toolbar.
  scroll->setGridOffset((origin - RACK_OFFSET) / RACK_GRID_SIZE - Vec(1, .25f));
}

json_t* Beekapoo::editSessionState() {
  auto* session = dynamic_cast<RackEditSession*>(editSession.get());
  if (!session || session->finished) return nullptr;
  auto* saved = json_pack(
      "{s:f,s:f,s:f,s:b,s:b}", "x", double(session->previousOffset.x), "y",
      double(session->previousOffset.y), "zoom", double(session->previousZoom),
      "panel", session->showPanel, "expose", session->exposeMode);
  auto* selected = json_array();
  for (auto id : session->previousSelection.selected)
    json_array_append_new(selected, json_integer(id));
  json_object_set_new(saved, "selected", selected);
  return saved;
}
void Beekapoo::resumeEditSession(json_t* saved) {
  auto* session = new RackEditSession(this);
  Vec offset(json_number_value(json_object_get(saved, "x")),
             json_number_value(json_object_get(saved, "y")));
  double zoom = json_number_value(json_object_get(saved, "zoom"));
  if (offset.isFinite()) session->previousOffset = offset;
  if (std::isfinite(zoom) && zoom > 0)
    session->previousZoom = clamp(float(zoom), .1f, 4.f);
  session->showPanel = json_is_true(json_object_get(saved, "panel"));
  session->exposeMode = json_is_true(json_object_get(saved, "expose"));
  session->previousSelection.selected.clear();
  auto* selected = json_object_get(saved, "selected");
  for (size_t i = 0; i < json_array_size(selected); ++i)
    session->previousSelection.selected.push_back(
        json_integer_value(json_array_get(selected, i)));
  for (auto& member : members)
    if (auto* holder = member.holder.get()) {
      auto* w = member.widget.get();
      auto* parent = holder->parent;
      Vec position = holder->box.pos;
      homeRack->select(holder, false);
      holder->removeChild(w);
      parent->removeChild(holder);
      w->box.pos = position;
      w->visible = true;
      parent->addChild(w);
      member.holder = nullptr;
      delete holder;
    }
  editSession = session;
  editPreview = session->showPanel;
  controls->visible = false;
  layoutControls();
  APP->scene->addChildBelow(session, APP->scene->browser);
  saveGroup();
}
