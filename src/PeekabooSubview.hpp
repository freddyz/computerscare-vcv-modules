#pragma once

// Included inside peekaboo. The modal view renders the original widgets
// and forwards control events; it never creates another engine or module copy.
struct SubpatchHit;
struct SubpatchCanvas;
struct SubpatchNativeRack;
struct SubpatchNativePort;
struct SubpatchZoom : widget::ZoomWidget {
  void step() override {
    widget::Widget::step();
    auto bounds = getVisibleChildrenBoundingBox();
    box.size =
        bounds.size.isFinite() ? bounds.getBottomRight() * getZoom() : Vec();
  }
};
struct SubpatchScroll : ui::ScrollWidget {
  void draw(const DrawArgs& args) override {
    nvgBeginPath(args.vg);
    nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
    nvgFillColor(args.vg, nvgRGB(39, 43, 41));
    nvgFill(args.vg);
    ui::ScrollWidget::draw(args);
  }
  void step() override {
    if (!offset.isFinite()) offset = Vec();
    ui::ScrollWidget::step();
  }
  void drawLayer(const DrawArgs& args, int layer) override {
    nvgSave(args.vg);
    nvgIntersectScissor(args.vg, 0, 0, box.size.x, box.size.y);
    ui::ScrollWidget::drawLayer(args, layer);
    nvgRestore(args.vg);
  }
};
struct SubpatchView : widget::OpaqueWidget {
  WeakPtr<Peekaboo> owner;
  std::set<int64_t> selected, pendingRemoval, browserExisting;
  bool browserPending = false;
  Vec browserPosition;
  void openBrowser(Vec position);
  void finishBrowser();
  void queueRemoval(int64_t id);
  void selectModule(int64_t id, int mods);
  GroupHistory* moveHistory = nullptr;
  std::map<int64_t, Vec> dragPositions;
  Vec dragDelta;
  void beginMove(int64_t id);
  void moveSelection(Vec delta);
  void endMove();
  void moduleMenu(int64_t id);
  ActionButton* addButton = nullptr;
  bool exposeMode = false;
  bool closing = false;
  SubpatchNativeRack* nativeRack = nullptr;
  std::vector<SubpatchNativePort*> portHits;
  std::map<app::CableWidget*, std::pair<int64_t, engine::Cable*>> mainKnown,
      localKnown;
  void syncFromMain();
  void syncToMain();
  void nativeAction(const std::function<void()>& action);
  void shutdownNative();
  SubpatchNativePort* portFor(app::PortWidget* source);
  ~SubpatchView() override;
  ui::Slider* cableOpacity = nullptr;
  WeakPtr<app::PortWidget> pending;
  ui::ScrollWidget* scroll = nullptr;
  SubpatchCanvas* canvas = nullptr;
  SubpatchZoom* zoom = nullptr;
  bool fitOnOpen = true;
  void setViewZoom(float amount, Vec focus);
  void fitView();
  std::vector<SubpatchHit*> hits;
  ActionButton *adjust = nullptr, *exposeButton = nullptr, *close = nullptr;
  ActionButton *zoomOut = nullptr, *zoomLabel = nullptr, *zoomIn = nullptr,
               *fit = nullptr;
  explicit SubpatchView(Peekaboo* group);
  void choose(app::PortWidget* port);
  bool connect(app::PortWidget* a, app::PortWidget* b);
  void disconnect(app::PortWidget* port);
  void setMode(bool expose);
  void dismiss() {
    if (owner) owner->subview = nullptr;
    setVisible(false);
    requestDelete();
  }
  void step() override;
  void onHide(const HideEvent& e) override;
  void draw(const DrawArgs& args) override;
  // Scene-level overlays must compose their own Rack layers. Don't draw them
  // a second time if a caller (including the preview runner) requests layer 1.
  void drawLayer(const DrawArgs&, int) override {}
  void onHoverScroll(const HoverScrollEvent& e) override {
    if ((APP->window->getMods() & RACK_MOD_CTRL) != 0) {
      setViewZoom(zoom->getZoom() * std::pow(1.12f, e.scrollDelta.y / 10.f),
                  e.pos - scroll->box.pos);
      e.consume(this);
      return;
    }
    widget::OpaqueWidget::onHoverScroll(e);
  }
  void onHoverKey(const HoverKeyEvent& e) override {
    if (e.key == GLFW_KEY_ESCAPE && e.action == GLFW_PRESS) {
      dismiss();
      e.consume(this);
      return;
    }
    widget::OpaqueWidget::onHoverKey(e);
  }
  void onButton(const ButtonEvent& e) override {
    widget::OpaqueWidget::onButton(e);
    if (!e.isConsumed()) e.consume(this);
  }
};
struct SubpatchTooltipAnchor : widget::TransparentWidget {
  WeakPtr<ui::Tooltip> tooltip;
  WeakPtr<widget::Widget> anchor;
  std::function<void()> dismiss;
  void step() override {
    if (!tooltip || !anchor) {
      requestDelete();
      return;
    }
    auto* hovered = APP->event->getHoveredWidget();
    auto* dragged = APP->event->getDraggedWidget();
    if ((hovered != anchor.get() && dragged != anchor.get()) ||
        !anchor->visible || !settings::tooltips) {
      if (dismiss) dismiss();
      requestDelete();
      return;
    }
    tooltip->box.pos = anchor->getAbsoluteOffset(anchor->box.size).round();
    if (parent) tooltip->box = tooltip->box.nudge(parent->box.zeroPos());
  }
};
struct SubpatchHit : widget::OpaqueWidget {
  SubpatchView* view;
  WeakPtr<app::ModuleWidget> sourceModule;
  WeakPtr<app::PortWidget> port;
  WeakPtr<app::ParamWidget> param;
  WeakPtr<ui::Tooltip> tooltip;
  WeakPtr<widget::Widget> tooltipAnchor;
  bool nativeTooltip = false;
  bool hovering = false;
  WeakPtr<widget::Widget> dragTarget;
  SubpatchHit(SubpatchView* v, app::ModuleWidget* m, app::PortWidget* p,
              app::ParamWidget* q)
      : view(v), sourceModule(m), port(p), param(q) {}
  void clearTooltip() {
    if (nativeTooltip) {
      widget::Widget::LeaveEvent leave;
      if (port)
        port->onLeave(leave);
      else if (param)
        param->onLeave(leave);
    }
    nativeTooltip = false;
    tooltip = nullptr;
    if (tooltipAnchor) tooltipAnchor->requestDelete();
    tooltipAnchor = nullptr;
  }
  ~SubpatchHit() override { clearTooltip(); }
  void step() override {
    auto* source = port ? static_cast<widget::Widget*>(port.get())
                        : static_cast<widget::Widget*>(param.get());
    visible = source && sourceModule && view->owner;
    if (!visible) return;
    for (auto* p = source; p && p != sourceModule.get(); p = p->parent)
      if (!p->visible) visible = false;
    if (port && !port->module) visible = false;
    if (param && (!param->module || !param->getParamQuantity()))
      visible = false;
    box.pos = source->getRelativeOffset(Vec(), sourceModule.get());
    box.size = source->box.size;
    if (port)
      for (const auto& p : view->owner->ports)
        if (view->owner->findPort(p) == port.get()) {
          box.pos = port->parent->getRelativeOffset(p.originalPosition,
                                                    sourceModule.get());
          break;
        }
    widget::OpaqueWidget::step();
  }
  bool locked() const {
    return port && view->owner && view->owner->boundaryPort(port.get());
  }
  bool selected() const {
    return view->owner && (port ? view->owner->exposed(port.get())
                                : param && view->owner->exposed(param.get()));
  }
  void draw(const DrawArgs& args) override {
    if (!view->owner || !view->exposeMode) return;
    bool lit = hovering || (view->exposeMode && selected()) ||
               (port && view->pending.get() == port.get());
    if (!lit && !locked()) return;
    NVGcolor color =
        locked() ? nvgRGB(228, 193, 74) : COLOR_COMPUTERSCARE_GREEN;
    nvgBeginPath(args.vg);
    nvgMoveTo(args.vg, -2, -1);
    nvgLineTo(args.vg, box.size.x + 2, -2);
    nvgLineTo(args.vg, box.size.x + 1, box.size.y + 2);
    nvgLineTo(args.vg, -1, box.size.y + 1);
    nvgClosePath(args.vg);
    if (view->exposeMode && selected()) {
      auto fill = color;
      fill.a = .18f;
      nvgFillColor(args.vg, fill);
      nvgFill(args.vg);
    }
    nvgStrokeColor(args.vg, color);
    nvgStrokeWidth(args.vg, locked() ? 2.f : 1.3f);
    nvgStroke(args.vg);
  }
  void onEnter(const EnterEvent& e) override {
    hovering = true;
    clearTooltip();
    std::set<widget::Widget*> before(APP->scene->children.begin(),
                                     APP->scene->children.end());
    if (port)
      port->onEnter(e);
    else if (param)
      param->onEnter(e);
    for (auto* child : APP->scene->children)
      if (!before.count(child))
        if (auto* tip = dynamic_cast<ui::Tooltip*>(child)) {
          tooltip = tip;
          break;
        }
    nativeTooltip = true;
    if (tooltip) {
      auto* positioner = new SubpatchTooltipAnchor;
      positioner->tooltip = tooltip;
      positioner->anchor = this;
      WeakPtr<SubpatchHit> live = this;
      positioner->dismiss = [live] {
        if (live) live->clearTooltip();
      };
      APP->scene->addChild(positioner);
      tooltipAnchor = positioner;
    }
  }
  void onLeave(const LeaveEvent&) override {
    hovering = false;
    clearTooltip();
  }
  void onHover(const HoverEvent& e) override {
    if (param && !view->exposeMode) param->onHover(e);
    e.consume(this);
  }
  void onButton(const ButtonEvent& e) override {
    if (!view->owner) {
      e.consume(this);
      return;
    }
    if (param && !view->exposeMode) {
      param->onButton(e);
      if (e.button == GLFW_MOUSE_BUTTON_LEFT && e.action == GLFW_PRESS &&
          e.context && e.context->target) {
        dragTarget = e.context->target;
        e.consume(this);
      }
      if (!e.isConsumed()) e.consume(this);
      return;
    }
    if (e.action == GLFW_PRESS) {
      if (view->exposeMode && e.button == GLFW_MOUSE_BUTTON_LEFT) {
        if (port)
          view->owner->togglePort(port.get());
        else if (param)
          view->owner->toggleParameter(param.get());
      } else if (port && !view->exposeMode) {
        if (e.button == GLFW_MOUSE_BUTTON_LEFT) view->choose(port.get());
        if (e.button == GLFW_MOUSE_BUTTON_RIGHT) view->disconnect(port.get());
      }
    }
    e.consume(this);
  }
  void onDragStart(const DragStartEvent& e) override {
    if (param && !view->exposeMode && dragTarget) dragTarget->onDragStart(e);
  }
  Vec adjustedDragDelta(Vec delta) {
    return dragTarget
               ? delta * (dragTarget->getAbsoluteZoom() / getAbsoluteZoom())
               : delta;
  }
  void onDragMove(const DragMoveEvent& e) override {
    if (param && !view->exposeMode && dragTarget) {
      auto adjusted = e;
      adjusted.mouseDelta = adjustedDragDelta(e.mouseDelta);
      dragTarget->onDragMove(adjusted);
    }
  }
  void onDragEnd(const DragEndEvent& e) override {
    if (param && dragTarget) dragTarget->onDragEnd(e);
    dragTarget = nullptr;
  }
  void onDoubleClick(const DoubleClickEvent& e) override {
    if (param && !view->exposeMode) param->onDoubleClick(e);
  }
  void onDragEnter(const DragEnterEvent& e) override { e.consume(this); }
  void onDragDrop(const DragDropEvent& e) override {
    auto* origin = dynamic_cast<SubpatchHit*>(e.origin);
    if (origin && origin != this && origin->view == view && !view->exposeMode &&
        origin->port && port) {
      view->connect(origin->port.get(), port.get());
      view->pending = nullptr;
    }
    e.consume(this);
  }
  void onHoverKey(const HoverKeyEvent& e) override {
    if (param && !view->exposeMode) param->onHoverKey(e);
    e.stopPropagating();
  }
  void onHoverScroll(const HoverScrollEvent& e) override {
    if (param && !view->exposeMode) param->onHoverScroll(e);
    if (e.isConsumed()) e.stopPropagating();
  }
};
// Exposed jack coordinates on the real widget serve the main rack's cables.
// Restore their panel-local coordinates only for this render, then put them
// back immediately. Engine processing never reads widget geometry.
struct SourcePortLayout {
  std::vector<std::pair<app::PortWidget*, Vec>> moved;
  SourcePortLayout(Peekaboo* owner, int64_t id) {
    for (const auto& p : owner->ports)
      if (p.id == id)
        if (auto* source = owner->findPort(p)) {
          moved.push_back({source, source->box.pos});
          source->box.pos = p.originalPosition;
        }
  }
  ~SourcePortLayout() {
    for (auto& p : moved) p.first->box.pos = p.second;
  }
};
#include "PeekabooNativeRack.hpp"
struct SubpatchModuleFace : app::ModuleWidget {
  SubpatchView* view;
  WeakPtr<app::ModuleWidget> source;
  Vec preferredPosition;
  bool dragAllowed = true;
  SubpatchModuleFace(SubpatchView* v, app::ModuleWidget* w)
      : view(v), source(w) {
    module = w->module;
    model = w->model;
    box.size = w->box.size;
    for (auto* p : w->getPorts())
      if (p->module) {
        auto* hit = new SubpatchNativePort(v, w, p);
        addChild(hit);
        v->portHits.push_back(hit);
      }
    for (auto* p : w->getParams()) {
      auto* hit = new SubpatchHit(v, w, nullptr, p);
      addChild(hit);
      v->hits.push_back(hit);
    }
  }
  ~SubpatchModuleFace() override { module = nullptr; }
  void step() override {
    if (!source || !view->owner || view->closing) return;
    auto current = source->getParams();
    std::vector<SubpatchHit*> existing;
    for (auto* child : children)
      if (auto* hit = dynamic_cast<SubpatchHit*>(child))
        existing.push_back(hit);
    bool changed = current.size() != existing.size();
    if (!changed)
      for (size_t i = 0; i < current.size(); ++i)
        if (existing[i]->param.get() != current[i]) {
          changed = true;
          break;
        }
    if (changed) {
      for (auto* hit : existing) {
        view->hits.erase(std::remove(view->hits.begin(), view->hits.end(), hit),
                         view->hits.end());
        removeChild(hit);
        delete hit;
      }
      for (auto* p : current) {
        auto* hit = new SubpatchHit(view, source.get(), nullptr, p);
        addChild(hit);
        view->hits.push_back(hit);
      }
    }
    box.size = source->box.size;
    widget::Widget::step();
  }
  void onHoverKey(const HoverKeyEvent& e) override {
    if (!view->exposeMode && e.key == GLFW_KEY_A && (e.mods & RACK_MOD_CTRL) &&
        e.action == GLFW_PRESS) {
      for (auto* w : view->nativeRack->getModules())
        view->selected.insert(w->module->id);
      e.consume(this);
      return;
    }
    widget::Widget::onHoverKey(e);
    e.stopPropagating();
  }
  void onButton(const ButtonEvent& e) override {
    if (!source || !view->owner) return;
    widget::Widget::onButton(e);
    if (e.isConsumed()) return;
    if (!view->exposeMode) {
      SourcePortLayout layout(view->owner.get(), source->module->id);
      source->widget::Widget::onButton(e);
      if (e.isConsumed()) return;
      if (e.action == GLFW_PRESS) {
        if (e.button == GLFW_MOUSE_BUTTON_LEFT) {
          dragAllowed = !(e.mods & (RACK_MOD_CTRL | GLFW_MOD_SHIFT));
          view->selectModule(module->id, e.mods);
        }
        if (e.button == GLFW_MOUSE_BUTTON_RIGHT) view->moduleMenu(module->id);
      }
    }
    e.consume(this);
  }
  void onDragStart(const DragStartEvent& e) override {
    if (dragAllowed && !view->exposeMode && e.button == GLFW_MOUSE_BUTTON_LEFT)
      view->beginMove(module->id);
  }
  void onDragMove(const DragMoveEvent& e) override {
    if (!view->exposeMode)
      view->moveSelection(e.mouseDelta / getAbsoluteZoom());
  }
  void onDragEnd(const DragEndEvent&) override { view->endMove(); }
  void draw(const DrawArgs& args) override {
    if (!source || !view->owner) return;
    {
      SourcePortLayout layout(view->owner.get(), source->module->id);
      nvgSave(args.vg);
      source->draw(args);
      nvgRestore(args.vg);
    }
    widget::Widget::draw(args);
    if (!view->exposeMode && view->selected.count(module->id)) {
      nvgBeginPath(args.vg);
      nvgRect(args.vg, 1, 1, box.size.x - 2, box.size.y - 2);
      nvgStrokeColor(args.vg, COLOR_COMPUTERSCARE_GREEN);
      nvgStrokeWidth(args.vg, 2);
      nvgStroke(args.vg);
    }
  }
  void drawLayer(const DrawArgs& args, int layer) override {
    if (!source || !view->owner) return;
    {
      SourcePortLayout layout(view->owner.get(), source->module->id);
      nvgSave(args.vg);
      source->drawLayer(args, layer);
      nvgRestore(args.vg);
    }
    widget::Widget::drawLayer(args, layer);
  }
};
// These aliases only supply viewport coordinates to the existing CableWidget.
// No cable is copied, added to the engine, or given a new owner.
struct SubpatchPortPosition : app::PortWidget {
  Vec position;
  SubpatchPortPosition(app::PortWidget* source, Vec center) {
    module = source->module;
    portId = source->portId;
    type = source->type;
    box.size = source->box.size;
    position = center - box.size * .5f;
  }
  Vec getRelativeOffset(Vec v, widget::Widget* ancestor) override {
    if (ancestor == APP->scene->rack) return position + v;
    return app::PortWidget::getRelativeOffset(v, ancestor);
  }
};
struct SubpatchCablePose {
  app::CableWidget* cable;
  app::PortWidget *input, *output;
  SubpatchPortPosition localInput, localOutput;
  SubpatchCablePose(app::CableWidget* c, Vec in, Vec out)
      : cable(c),
        input(c->inputPort),
        output(c->outputPort),
        localInput(input, in),
        localOutput(output, out) {
    cable->inputPort = &localInput;
    cable->outputPort = &localOutput;
    cable->step();
  }
  ~SubpatchCablePose() {
    cable->inputPort = input;
    cable->outputPort = output;
    cable->step();
  }
};
struct SubpatchCanvas : widget::Widget {
  SubpatchView* view;
  Vec mouse;
  std::vector<app::CableWidget*> subpatchCablesForLayout() {
    std::vector<app::CableWidget*> result;
    for (auto* child : view->nativeRack->getCableContainer()->children)
      if (auto* cw = dynamic_cast<app::CableWidget*>(child))
        result.push_back(cw);
    return result;
  }
  size_t renderedCables = 0;
  explicit SubpatchCanvas(SubpatchView* v) : view(v) { box.size = Vec(); }
  void step() override {
    widget::Widget::step();
    std::vector<SubpatchModuleFace*> faces;
    for (auto* child : view->nativeRack->getModuleContainer()->children)
      if (auto* face = dynamic_cast<SubpatchModuleFace*>(child))
        faces.push_back(face);
    box.size = Vec();
    for (auto* face : faces) {
      box.size.x = std::max(box.size.x, face->box.getRight() + 180);
      box.size.y =
          std::max(box.size.y, face->box.getBottom() + RACK_GRID_HEIGHT);
    }
    float z = view->zoom ? view->zoom->getZoom() : 1.f;
    box.size.x = std::max(box.size.x, view->scroll->box.size.x / z);
    box.size.y = std::max(box.size.y, view->scroll->box.size.y / z);
    if (view->owner)
      for (auto* cable : APP->scene->rack->getCompleteCables())
        if (view->owner->contains(cable->cable->inputModule) &&
            view->owner->contains(cable->cable->outputModule)) {
          Vec a, b;
          if (position(cable->inputPort, a) && position(cable->outputPort, b)) {
            Vec slump = (a + b) * .5f + Vec(0, (1 - settings::cableTension) *
                                                   (150 + (a - b).norm()));
            box.size.y = std::max(box.size.y, slump.y + 32);
          }
        }
  }
  void draw(const DrawArgs& args) override {
    view->nativeRack->box.size = box.size;
    widget::Widget::draw(args);
    if (view->nativeRack->selecting) {
      Vec a = view->nativeRack->selectionStart, b = view->nativeRack->pointer;
      nvgBeginPath(args.vg);
      nvgRect(args.vg, std::min(a.x, b.x), std::min(a.y, b.y),
              std::abs(a.x - b.x), std::abs(a.y - b.y));
      auto color = COLOR_COMPUTERSCARE_GREEN;
      color.a = .12f;
      nvgFillColor(args.vg, color);
      nvgFill(args.vg);
      nvgStrokeColor(args.vg, COLOR_COMPUTERSCARE_GREEN);
      nvgStrokeWidth(args.vg, 1);
      nvgStroke(args.vg);
    }
    renderedCables = view->nativeRack->getCompleteCables().size();
    view->nativeRack->box.size = box.size;
    {
      ScopedSubviewRack scope(view->nativeRack);
      for (auto* cw : subpatchCablesForLayout()) cw->step();
    }
  }

  void onHover(const HoverEvent& e) override {
    mouse = e.pos;
    widget::Widget::onHover(e);
  }
  void onDragHover(const DragHoverEvent& e) override {
    mouse = e.pos;
    widget::Widget::onDragHover(e);
  }
  bool position(app::PortWidget* port, Vec& out) {
    for (auto* hit : view->portHits)
      if (hit->source.get() == port) {
        out = hit->getRelativeOffset(hit->box.size * .5f, this);
        return true;
      }
    return false;
  }
  void wire(NVGcontext* vg, Vec a, Vec b, NVGcolor color) {
    nvgBeginPath(vg);
    nvgMoveTo(vg, a.x, a.y);
    float sag = std::min(90.f, 25.f + std::abs(b.x - a.x) * .2f);
    nvgBezierTo(vg, a.x, a.y + sag, b.x, b.y + sag, b.x, b.y);
    nvgStrokeColor(vg, nvgRGBA(0, 0, 0, 180));
    nvgStrokeWidth(vg, 4);
    nvgStroke(vg);
    nvgStrokeColor(vg, color);
    nvgStrokeWidth(vg, 2);
    nvgStroke(vg);
    for (auto p : {a, b}) {
      nvgBeginPath(vg);
      nvgCircle(vg, p.x, p.y, 4);
      nvgFillColor(vg, color);
      nvgFill(vg);
    }
  }
  widget::Widget* plugWidget(void* identity) {
    for (auto* parent : APP->scene->rack->children)
      for (auto* child : parent->children)
        if (static_cast<void*>(child) == identity) return child;
    return nullptr;
  }
  void drawPlug(const DrawArgs& args, void* identity) {
    auto* plug = plugWidget(identity);
    if (!plug) return;
    auto local = args;
    local.clipBox = Rect(Vec(-40, -40), Vec(80, 80));
    nvgSave(args.vg);
    nvgTranslate(args.vg, plug->box.pos.x, plug->box.pos.y);
    plug->draw(local);
    plug->drawLayer(local, 1);
    nvgRestore(args.vg);
  }
  void drawLayer(const DrawArgs& args, int layer) override {
    widget::Widget::drawLayer(args, layer);
    if (layer != 1 || !view->owner) return;
    for (auto* cable : view->owner->homeRack->getCompleteCables()) {
      bool in = view->owner->contains(cable->cable->inputModule),
           out = view->owner->contains(cable->cable->outputModule);
      if (in == out) continue;
      Vec a, b;
      if (out && !position(cable->outputPort, a)) continue;
      if (in && !position(cable->inputPort, b)) continue;
      if (!in) b = a + Vec(0, 35);
      if (!out) a = b + Vec(0, 35);
      SubpatchCablePose pose(cable, b, a);
      if (out) drawPlug(args, static_cast<void*>(cable->outputPlug));
      if (in) drawPlug(args, static_cast<void*>(cable->inputPlug));
    }
    if (view->pending) {
      Vec a;
      if (position(view->pending.get(), a))
        wire(args.vg, a, mouse, COLOR_COMPUTERSCARE_GREEN);
    }
  }
};
SubpatchView::SubpatchView(Peekaboo* group) : owner(group) {
  adjust = new ActionButton;
  adjust->text = "Adjust subpatch";
  adjust->configure(155, 32, 4, 3, .9f, 871);
  adjust->action = [this] { setMode(false); };
  adjust->pressed = [this] { return !exposeMode; };
  addChild(adjust);
  exposeButton = new ActionButton;
  exposeButton->text = "Expose";
  exposeButton->configure(88, 32, 4, 3, .9f, 872);
  exposeButton->action = [this] { setMode(true); };
  exposeButton->pressed = [this] { return exposeMode; };
  addChild(exposeButton);
  close = new ActionButton;
  close->text = "Close";
  close->configure(74, 32, 4, 3, .9f, 873);
  close->action = [this] { dismiss(); };
  addChild(close);
  zoomOut = new ActionButton;
  zoomOut->text = "−";
  zoomOut->configure(34, 32, 4, 3, .8f, 876);
  zoomOut->action = [this] {
    setViewZoom(zoom->getZoom() / 1.2f, scroll->box.size * .5f);
  };
  addChild(zoomOut);
  zoomLabel = new ActionButton;
  zoomLabel->configure(70, 32, 4, 3, .8f, 877);
  zoomLabel->caption = [this] {
    return std::to_string(int(std::round(zoom->getZoom() * 100))) + "%";
  };
  zoomLabel->action = [this] { setViewZoom(1.f, scroll->box.size * .5f); };
  addChild(zoomLabel);
  zoomIn = new ActionButton;
  zoomIn->text = "+";
  zoomIn->configure(34, 32, 4, 3, .8f, 878);
  zoomIn->action = [this] {
    setViewZoom(zoom->getZoom() * 1.2f, scroll->box.size * .5f);
  };
  addChild(zoomIn);
  fit = new ActionButton;
  fit->text = "Fit";
  fit->configure(48, 32, 4, 3, .8f, 879);
  fit->action = [this] { fitView(); };
  addChild(fit);
  cableOpacity = new ui::Slider;
  cableOpacity->quantity =
      group->module->getParamQuantity(Module::SHOW_CABLE_OPACITY);
  cableOpacity->box.size = Vec(174, 23);
  addChild(cableOpacity);
  for (auto* button :
       {adjust, exposeButton, close, zoomOut, zoomLabel, zoomIn, fit}) {
    button->textSize = 16;
    button->textInset = 5;
  }
  addButton = new ActionButton;
  addButton->text = "Add module";
  addButton->configure(100, 30, 4, 3, .8f, 949);
  addButton->textSize = 16;
  addButton->action = [this] {
    openBrowser(scroll->offset / zoom->getZoom() + Vec(30, 30));
  };
  addChild(addButton);
  scroll = new SubpatchScroll;
  addChild(scroll);
  canvas = new SubpatchCanvas(this);
  nativeRack = new SubpatchNativeRack(this);
  canvas->addChild(nativeRack);
  zoom = new SubpatchZoom;
  zoom->box.size = Vec();
  zoom->addChild(canvas);
  scroll->container->addChild(zoom);
  Vec minimum(INFINITY, INFINITY);
  for (const auto& member : group->members) {
    minimum.x = std::min(minimum.x, member.position.x);
    minimum.y = std::min(minimum.y, member.position.y);
  }
  Vec cursor;
  float rowY = minimum.y;
  for (const auto& member : group->members)
    if (auto* w = group->find(member.id)) {
      auto* face = new SubpatchModuleFace(this, w);
      face->preferredPosition = member.position - minimum;
      if (member.position.y != rowY) {
        cursor.x = 0;
        cursor.y += RACK_GRID_HEIGHT;
        rowY = member.position.y;
      }
      auto saved = group->subpatchPositions.find(member.id);
      face->box.pos =
          saved == group->subpatchPositions.end() ? cursor : saved->second;
      cursor.x += face->box.size.x + 15;
      group->subpatchPositions[member.id] = face->box.pos;
      {
        ScopedSubviewRack scope(nativeRack);
        nativeRack->addModule(face);
      }
      canvas->box.size.x =
          std::max(canvas->box.size.x, face->box.getRight() + 16);
      canvas->box.size.y =
          std::max(canvas->box.size.y, face->box.getBottom() + 100);
    }
  group->saveGroup();
}
void SubpatchView::selectModule(int64_t id, int mods) {
  if (mods & (RACK_MOD_CTRL | GLFW_MOD_SHIFT)) {
    if (selected.count(id))
      selected.erase(id);
    else
      selected.insert(id);
  } else if (!selected.count(id)) {
    selected.clear();
    selected.insert(id);
  }
}
void SubpatchView::beginMove(int64_t id) {
  if (!owner || exposeMode || settings::lockModules) return;
  if (!selected.count(id)) {
    selected.clear();
    selected.insert(id);
  }
  dragPositions.clear();
  dragDelta = Vec();
  for (auto* w : nativeRack->getModules())
    if (selected.count(w->module->id))
      dragPositions[w->module->id] = w->box.pos;
  moveHistory = owner->beginAction("move subpatch modules");
}
void SubpatchView::moveSelection(Vec delta) {
  if (!moveHistory || !owner) return;
  dragDelta += delta;
  Vec snapped = dragDelta.div(RACK_GRID_SIZE).round().mult(RACK_GRID_SIZE);
  auto modules = nativeRack->getModules();
  for (auto* w : modules)
    if (dragPositions.count(w->module->id)) {
      Rect candidate(dragPositions[w->module->id] + snapped, w->box.size);
      if (candidate.pos.x < 0 || candidate.pos.y < 0) return;
      for (auto* other : modules)
        if (!dragPositions.count(other->module->id) &&
            candidate.intersects(other->box))
          return;
    }
  for (auto* w : modules)
    if (dragPositions.count(w->module->id)) {
      w->box.pos = dragPositions[w->module->id] + snapped;
      owner->subpatchPositions[w->module->id] = w->box.pos;
    }
}
void SubpatchView::endMove() {
  if (!moveHistory) return;
  auto* action = moveHistory;
  moveHistory = nullptr;
  bool changed = false;
  if (owner)
    for (const auto& p : dragPositions)
      if (owner->subpatchPositions[p.first] != p.second) changed = true;
  dragPositions.clear();
  if (owner && changed) {
    owner->saveGroup();
    owner->finishAction(action);
  } else
    delete action;
}
void SubpatchView::queueRemoval(int64_t id) {
  if (!selected.count(id)) {
    selected.clear();
    selected.insert(id);
  }
  pendingRemoval = selected;
}
void SubpatchView::moduleMenu(int64_t id) {
  if (!selected.count(id)) {
    selected.clear();
    selected.insert(id);
  }
  auto* menu = createMenu();
  WeakPtr<SubpatchView> live = this;
  menu->addChild(createMenuLabel(
      selected.size() > 1 ? std::to_string(selected.size()) + " modules"
                          : owner->memberName(id)));
  menu->addChild(createMenuItem("Remove from abstraction", "", [live, id] {
    if (live) live->queueRemoval(id);
  }));
  menu->addChild(createMenuItem("Select all", "", [live] {
    if (live)
      for (auto* w : live->nativeRack->getModules())
        live->selected.insert(w->module->id);
  }));
  menu->addChild(createMenuItem("Add module…", "", [live] {
    if (live)
      live->openBrowser(live->scroll->offset / live->zoom->getZoom() +
                        Vec(30, 30));
  }));
}
void SubpatchView::openBrowser(Vec position) {
  if (!owner || exposeMode || closing) return;
  browserPosition = position.div(RACK_GRID_SIZE).round().mult(RACK_GRID_SIZE);
  browserExisting.clear();
  for (auto id : APP->engine->getModuleIds()) browserExisting.insert(id);
  browserPending = true;
  // The stock browser initially places a module in the main Rack. Reserve an
  // empty row so this temporary placement cannot push unrelated modules.
  widget::Widget::HoverEvent parkingHover;
  parkingHover.pos = owner->parkingPosition() + Vec(60, RACK_GRID_HEIGHT);
  owner->homeRack->onHover(parkingHover);
  auto* browser = APP->scene->browser;
  // Use the existing browser, above this modal, without constructing private
  // UI.
  if (browser->parent) {
    auto* p = browser->parent;
    p->removeChild(browser);
    p->addChild(browser);
  }
  browser->setVisible(true);
}
void SubpatchView::finishBrowser() {
  browserPending = false;
  if (!owner) return;
  auto* group = owner.get();
  auto* rack = group->homeRack;
  std::vector<app::ModuleWidget*> added;
  for (auto* w : rack->getModules())
    if (w->module && !browserExisting.count(w->module->id) &&
        !dynamic_cast<Peekaboo*>(w) &&
        !w->module->leftExpander.producerMessage &&
        !w->module->rightExpander.producerMessage)
      added.push_back(w);
  if (added.empty()) return;
  // Browser placement uses the main Rack. End its placement drag before the
  // original is parked, then expose the module's proxy at the requested spot.
  APP->event->setDraggedWidget(nullptr, GLFW_MOUSE_BUTTON_LEFT);
  Vec position = browserPosition;
  for (auto* w : added) {
    bool overlap = true;
    while (overlap) {
      overlap = false;
      for (auto* face : nativeRack->getModules())
        if (Rect(position, w->box.size).intersects(face->box)) {
          position.x = face->box.getRight() + RACK_GRID_WIDTH;
          overlap = true;
          break;
        }
    }
    group->subpatchPositions[w->module->id] = position;
    position.x += w->box.size.x + RACK_GRID_WIDTH;
  }
  rack->deselectAll();
  for (auto* w : added) rack->select(w, true);
  group->addSelection();
  group->showView();
}
// Engine connections have one authoritative widget in the main rack. The
// local Rack owns only visual counterparts, released before its destruction.
inline std::vector<app::CableWidget*> subpatchCables(SubpatchNativeRack* rack) {
  std::vector<app::CableWidget*> result;
  for (auto* child : rack->getCableContainer()->children)
    if (auto* cw = dynamic_cast<app::CableWidget*>(child)) result.push_back(cw);
  return result;
}
inline bool subpatchCableMatches(app::CableWidget* cw, engine::Cable* engine) {
  return engine && cw->inputPort && cw->outputPort &&
         cw->inputPort->module == engine->inputModule &&
         cw->inputPort->portId == engine->inputId &&
         cw->outputPort->module == engine->outputModule &&
         cw->outputPort->portId == engine->outputId;
}
void SubpatchView::syncFromMain() {
  if (closing || !owner || !nativeRack) return;
  auto* main = owner->homeRack;
  ScopedSubviewRack mainScope(main);
  for (auto* cw : subpatchCables(nativeRack))
    if (cw->cable) {
      auto old = localKnown.find(cw);
      auto* engine = old == localKnown.end()
                         ? nullptr
                         : APP->engine->getCable(old->second.first);
      if (!engine || engine != cw->cable || !subpatchCableMatches(cw, engine)) {
        ScopedSubviewRack scope(nativeRack);
        cw->releaseCable();
        nativeRack->removeCable(cw);
        delete cw;
      }
    }
  mainKnown.clear();
  for (auto* cw : main->getCompleteCables()) {
    mainKnown[cw] = {cw->cable->id, cw->cable};
    if (!owner->contains(cw->cable->inputModule) ||
        !owner->contains(cw->cable->outputModule))
      continue;
    ScopedSubviewRack scope(nativeRack);
    auto* local = nativeRack->getCable(cw->cable->id);
    if (!local) {
      local = new app::CableWidget;
      local->setCable(cw->cable);
      nativeRack->addCable(local);
    }
    local->color = cw->color;
  }
  localKnown.clear();
  for (auto* cw : subpatchCables(nativeRack))
    if (cw->cable) localKnown[cw] = {cw->cable->id, cw->cable};
}
void SubpatchView::syncToMain() {
  if (closing || !owner) return;
  auto* main = owner->homeRack;
  ScopedSubviewRack scope(main);
  for (auto* cw : main->getCompleteCables()) {
    auto known = mainKnown.find(cw);
    if (known == mainKnown.end()) continue;
    auto* engine = APP->engine->getCable(known->second.first);
    if (engine != cw->cable || !subpatchCableMatches(cw, engine)) {
      cw->releaseCable();
      main->removeCable(cw);
      delete cw;
    }
  }
  for (auto* cw : nativeRack->getCompleteCables())
    if (cw->cable) {
      auto* canonical = main->getCable(cw->cable->id);
      if (!canonical) {
        canonical = new app::CableWidget;
        canonical->setCable(cw->cable);
        main->addCable(canonical);
      }
      canonical->color = cw->color;
      if (!owner->expanded) {
        canonical->visible = false;
        owner->showPlugs(canonical, false);
      }
    }
  mainKnown.clear();
  for (auto* cw : main->getCompleteCables())
    mainKnown[cw] = {cw->cable->id, cw->cable};
  localKnown.clear();
  for (auto* cw : subpatchCables(nativeRack))
    if (cw->cable) localKnown[cw] = {cw->cable->id, cw->cable};
}
void SubpatchView::nativeAction(const std::function<void()>& action) {
  if (closing || !owner) return;
  syncFromMain();
  try {
    ScopedSubviewRack scope(nativeRack);
    action();
  } catch (...) {
    syncToMain();
    throw;
  }
  syncToMain();
}
SubpatchNativePort* SubpatchView::portFor(app::PortWidget* source) {
  for (auto* p : portHits)
    if (p->source.get() == source) return p;
  return nullptr;
}
void SubpatchView::shutdownNative() {
  endMove();
  if (closing || !nativeRack) return;
  closing = true;
  for (auto* port : portHits) port->clearTooltip();
  ScopedSubviewRack scope(nativeRack);
  for (auto* cw : subpatchCables(nativeRack)) {
    cw->releaseCable();
    nativeRack->removeCable(cw);
    delete cw;
  }
  nativeRack->deselectAll();
  auto modules = nativeRack->getModules();
  for (auto* w : modules) {
    w->module = nullptr;
    w->parent->removeChild(w);
    delete w;
  }
  portHits.clear();
  hits.clear();
}
SubpatchView::~SubpatchView() { shutdownNative(); }
void SubpatchNativeRack::step() {
  if (view->closing) return;
  for (auto* child : children) child->box = Rect(Vec(), box.size);
  ScopedSubviewRack scope(this);
  main->step();
}
void SubpatchNativeRack::draw(const DrawArgs& args) {
  if (view->closing || !view->owner) return;
  ScopedSubviewRack scope(this);
  drawnOpacity =
      view->owner->module->params[Module::SHOW_CABLE_OPACITY].getValue();
  ScopedSubviewOpacity opacity(drawnOpacity);
  main->draw(args);
}
void SubpatchView::setMode(bool expose) {
  for (auto* hit : hits) hit->clearTooltip();
  for (auto* port : portHits) port->clearTooltip();
  exposeMode = expose;
  pending = nullptr;
}
void SubpatchView::onHide(const HideEvent& e) {
  for (auto* hit : hits) hit->clearTooltip();
  for (auto* port : portHits) port->clearTooltip();
  shutdownNative();
  widget::OpaqueWidget::onHide(e);
}
void SubpatchView::step() {
  if (!owner) {
    dismiss();
    return;
  }
  if (closing) return;
  if (!pendingRemoval.empty()) {
    auto* group = owner.get();
    auto ids = pendingRemoval;
    pendingRemoval.clear();
    group->removeMembers(ids);
    group->showView();
    return;
  }
  if (browserPending && !APP->scene->browser->visible) {
    finishBrowser();
    if (closing) return;
  }
  syncFromMain();
  if (parent) box.size = parent->box.size;
  adjust->box.pos = Vec(8, 8);
  exposeButton->box.pos = Vec(162, 8);
  zoomOut->box.pos = Vec(264, 8);
  zoomLabel->box.pos = Vec(301, 8);
  zoomIn->box.pos = Vec(374, 8);
  fit->box.pos = Vec(415, 8);
  addButton->box.pos = Vec(475, 8);
  addButton->visible = !exposeMode;
  close->box.pos = Vec(std::max(585.f, box.size.x - 82), 8);
  cableOpacity->box.pos = Vec(std::max(330.f, box.size.x - 182), 47);
  scroll->box = Rect(Vec(0, 80), Vec(std::max(100.f, box.size.x),
                                     std::max(100.f, box.size.y - 80)));
  widget::OpaqueWidget::step();
  if (fitOnOpen && scroll->box.size.x > 0 && canvas->box.size.x > 0) {
    fitOnOpen = false;
    fitView();
  }
}
void SubpatchView::draw(const DrawArgs& args) {
  nvgBeginPath(args.vg);
  nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
  nvgFillColor(args.vg, nvgRGBA(10, 15, 13, 240));
  nvgFill(args.vg);
  auto font = APP->window->loadFont(asset::system("res/fonts/DejaVuSans.ttf"));
  nvgFontFaceId(args.vg, font->handle);
  nvgFontSize(args.vg, 12);
  nvgFillColor(args.vg, nvgRGB(148, 164, 155));
  std::string text = exposeMode
                         ? "Click controls to expose. Yellow jacks are locked."
                         : "Drag modules to move. Shift-click to select. "
                           "Right-click for actions. Yellow jacks are locked.";
  nvgText(args.vg, 10, 63, text.c_str(), nullptr);
  widget::OpaqueWidget::draw(args);
  // Match Rack's explicit extra-layer composition on the normal Scene draw
  // path, rather than relying on the preview runner to call drawLayer().
  nvgSave(args.vg);
  widget::Widget::drawLayer(args, 1);
  nvgRestore(args.vg);
}
void SubpatchView::setViewZoom(float amount, Vec focus) {
  if (!std::isfinite(amount)) return;
  amount = clamp(amount, .15f, 3.f);
  Vec point = (scroll->offset + focus) / zoom->getZoom();
  zoom->setZoom(amount);
  zoom->box.size = canvas->box.size * amount;
  scroll->offset = point * amount - focus;
  scroll->step();
}
void SubpatchView::fitView() {
  Vec extent;
  for (auto* w : nativeRack->getModules()) {
    extent.x = std::max(extent.x, w->box.getRight() + 18);
    extent.y = std::max(extent.y, w->box.getBottom() + 18);
  }
  if (extent.x <= 0 || extent.y <= 0) return;
  float amount = std::min((scroll->box.size.x - 18) / extent.x,
                          (scroll->box.size.y - 18) / extent.y);
  zoom->setZoom(clamp(amount, .15f, 1.f));
  scroll->offset = Vec();
  scroll->step();
}
void SubpatchView::choose(app::PortWidget*) {}
bool SubpatchView::connect(app::PortWidget* a, app::PortWidget* b) {
  auto* first = portFor(a);
  auto* second = portFor(b);
  if (!first || !second || first->locked() || second->locked() ||
      a->type == b->type)
    return false;
  widget::Widget::DragStartEvent start;
  start.button = GLFW_MOUSE_BUTTON_LEFT;
  first->onDragStart(start);
  widget::Widget::DragDropEvent drop;
  drop.button = GLFW_MOUSE_BUTTON_LEFT;
  drop.origin = first;
  second->onDragDrop(drop);
  widget::Widget::DragEndEvent end;
  end.button = GLFW_MOUSE_BUTTON_LEFT;
  first->onDragEnd(end);
  return true;
}
void SubpatchView::disconnect(app::PortWidget* source) {
  auto* port = portFor(source);
  if (!port || port->locked()) return;
  nativeAction([&] { port->deleteTopCableAction(); });
}
void Peekaboo::showView() {
  if (subview) {
    closeView();
    return;
  }
  auto* view = new SubpatchView(this);
  subview = view;
  APP->scene->addChild(view);
}
