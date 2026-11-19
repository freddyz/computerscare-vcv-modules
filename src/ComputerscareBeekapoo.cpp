#include <atomic>
#include <mutex>
#include <plugin.hpp>

#include "BeekapooCpuMeter.hpp"
#include "BeekapooLayout.hpp"
#include "BeekapooPreferences.hpp"
#include "BeekapooRandomLocks.hpp"
#include "Computerscare.hpp"
#include "ComputerscareResizableHandle.hpp"
#include "DevWatcherToolbar.hpp"
#include "Phlooper/Block.hpp"
#include "Phlooper/Dropdown.hpp"

// Engine modules and cables stay in Rack; only their UI layout is grouped.
namespace beekapoo {
// Keep the packed layout, but let the irregular plaques overhang their
// allotted areas. Their sides overlap, with small unpainted gaps between
// independently warped corners rather than a rectangular panel backdrop.
struct PackedPanelBlock : phlooper::WarpedBlock {
  void draw(const DrawArgs& args) override {
    if (box.size.x <= 0 || box.size.y <= 0) return;
    Vec first(-2.f, -2.f), last = box.size + Vec(2.f, 2.f);
    // Fit the complete perspective silhouette, including its back corners,
    // inside the module. Interior edges can still overlap adjacent blocks.
    if (parent) {
      first.x = std::max(first.x, .75f - box.pos.x);
      first.y = std::max(first.y, .75f - box.pos.y);
      last.x = std::min(last.x, parent->box.size.x - box.pos.x - .75f);
      last.y = std::min(last.y, parent->box.size.y - box.pos.y - .75f);
    }
    // Fit the actual silhouette rather than the artwork's nominal rectangle,
    // whose built-in margins otherwise leave an extra strip at the right.
    float d = clamp(depth, 0.f, std::min(box.size.x, box.size.y) * .25f);
    float amount = clamp(screwup, 0.f, std::min(box.size.x, box.size.y) * .12f);
    float margin = .5f + amount * .5f;
    auto jitter = [&](unsigned corner) {
      unsigned bits = shapeSeed * 0x9e3779b9u + corner * 0x85ebca6bu;
      bits ^= bits >> 16;
      bits *= 0x7feb352du;
      return (float(bits & 1023u) / 1023.f - .5f) * amount;
    };
    Vec face[] = {Vec(margin, margin), Vec(box.size.x - d - margin, margin),
                  Vec(box.size.x - d - margin, box.size.y - d * .65f - margin),
                  Vec(margin, box.size.y - d * .65f - margin)};
    Vec minimum(INFINITY, INFINITY), maximum(-INFINITY, -INFINITY);
    for (int i = 0; i < 4; ++i) {
      Vec front =
          face[i] + Vec(jitter(i * 2), jitter(i * 2 + 1)) + cornerOffsets[i];
      for (Vec point : {front, front + Vec(d, d * .65f)}) {
        minimum.x = std::min(minimum.x, point.x);
        minimum.y = std::min(minimum.y, point.y);
        maximum.x = std::max(maximum.x, point.x);
        maximum.y = std::max(maximum.y, point.y);
      }
    }
    nvgSave(args.vg);
    nvgTranslate(args.vg, first.x, first.y);
    nvgScale(args.vg, (last.x - first.x) / (maximum.x - minimum.x),
             (last.y - first.y) / (maximum.y - minimum.y));
    nvgTranslate(args.vg, -minimum.x, -minimum.y);
    phlooper::WarpedBlock::draw(args);
    nvgRestore(args.vg);
  }
};
struct SubpatchNativeRack;
void installRackHooks();
void disconnectBeekapooSelection();
history::ModuleChange* incomingGroupPreset(int64_t owner, json_t* root);
void queueSubpatchDeletion(int64_t owner, const std::vector<int64_t>& members);
// Temporarily lend the existing Rack's public containers to the subpatch UI.
// No RackWidget construction or private SDK entry points are required.
struct ScopedSubviewRack {
  SubpatchNativeRack* previous;
  explicit ScopedSubviewRack(SubpatchNativeRack* rack);
  explicit ScopedSubviewRack(app::RackWidget* rack);
  ~ScopedSubviewRack();
};
struct Module : engine::Module {
  std::atomic<float> panelWidth{180.f};
  std::mutex stateMutex;
  json_t* group = nullptr;
  json_t* contents = nullptr;
  std::function<json_t*(bool)> captureContents;
  bool copyBoundary = false;
  bool restorePending = false;
  enum { WIGGLE_AMOUNT, SHOW_CABLE_OPACITY, PARAM_COUNT };
  dsp::SchmittTrigger actionTriggers[6];
  std::atomic<unsigned> pendingActions{0};
  std::atomic<unsigned> pendingHistory{0};
  std::atomic<int> pendingBypass{-1};
  Module() {
    config(PARAM_COUNT, 6, 0, 0);
    configParam(WIGGLE_AMOUNT, 0.f, 1.f, .1f, "Wiggle amount", "%", 0.f, 100.f);
    getParamQuantity(WIGGLE_AMOUNT)->randomizeEnabled = false;
    configParam(SHOW_CABLE_OPACITY, 0.f, 1.f, 1.f, "Cable opacity", "%", 0.f,
                100.f);
    getParamQuantity(SHOW_CABLE_OPACITY)->randomizeEnabled = false;
    getParamQuantity(SHOW_CABLE_OPACITY)->resetEnabled = false;
    const char* names[] = {"Wiggle visible",     "Randomize visible",
                           "Initialize visible", "Wiggle all",
                           "Randomize all",      "Initialize all"};
    for (int i = 0; i < 6; ++i) configInput(i, names[i]);
  }
  void process(const ProcessArgs&) override {
    for (int i = 0; i < 6; ++i)
      if (actionTriggers[i].process(inputs[i].getVoltage()))
        pendingActions.fetch_or(1u << i, std::memory_order_relaxed);
  }
  // Rack invokes these while holding its engine lock. Apply child changes
  // from the UI step, after that lock is released, using native bypassModule.
  void onBypass(const BypassEvent&) override {
    pendingBypass.store(1, std::memory_order_release);
  }
  void onUnBypass(const UnBypassEvent&) override {
    pendingBypass.store(0, std::memory_order_release);
  }
  void onRandomize(const RandomizeEvent&) override {
    pendingHistory.fetch_or(1u << 1, std::memory_order_relaxed);
    pendingActions.fetch_or(1u << 1, std::memory_order_relaxed);
  }

  ~Module() override {
    json_decref(group);
    json_decref(contents);
  }
  void saveGroup(json_t* next) {
    std::lock_guard<std::mutex> lock(stateMutex);
    json_decref(group);
    group = next;
    if (!group) {
      json_decref(contents);
      contents = nullptr;
    }
  }
  json_t* dataToJson() override {
    std::lock_guard<std::mutex> lock(stateMutex);
    auto* root = json_object();
    json_object_set_new(root, "panelWidth", json_real(panelWidth.load()));
    if (group) json_object_set(root, "group", group);
    auto* fresh = captureContents ? captureContents(copyBoundary) : nullptr;
    if (fresh)
      json_object_set_new(root, "contents", fresh);
    else if (group && contents &&
             json_array_size(json_object_get(group, "members")) > 0)
      json_object_set(root, "contents", contents);
    return root;
  }
  void dataFromJson(json_t* root) override {
    std::lock_guard<std::mutex> lock(stateMutex);
    json_decref(group);
    auto* width = json_object_get(root, "panelWidth");
    float value =
        json_is_number(width) ? float(json_number_value(width)) : 180.f;
    panelWidth.store(std::isfinite(value) ? clamp(value, 45.f, 30000.f)
                                          : 180.f);
    group = json_deep_copy(json_object_get(root, "group"));
    json_decref(contents);
    contents = json_deep_copy(json_object_get(root, "contents"));
    restorePending = true;
  }
  json_t* takeRestore() {
    std::lock_guard<std::mutex> lock(stateMutex);
    if (!restorePending) return nullptr;
    restorePending = false;
    auto* restored = group ? json_deep_copy(group) : json_object();
    if (contents) json_object_set(restored, "contents", contents);
    return restored;
  }
};
// Registered as the original engine module, but with no rack footprint while
// collapsed. The real widget keeps its dimensions, ownership, and UI stepping.
// On expansion the holder remains as a transparent full-size parent. Keeping
// its identity also makes Rack's selection/delete snapshots safe to finish.
struct Holder : app::ModuleWidget {
  app::ModuleWidget* original;
  explicit Holder(app::ModuleWidget* w) : original(w) {
    module = w->module;
    model = w->model;
    box.size = w->box.size;
    w->box.pos = Vec();
    addChild(w);
  }
  void appendContextMenu(ui::Menu* menu) override {
    original->appendContextMenu(menu);
  }
  void onButton(const ButtonEvent& e) override {
    // Controls still receive their real widget events; module-level actions
    // must target the registered holder, not its nested original widget.
    if (!APP->scene->rack->isSelected(this)) {
      original->widget::Widget::onButton(e);
      if (e.isConsumed()) return;
    }
    bool visible = original->visible;
    original->visible = false;
    ModuleWidget::onButton(e);
    original->visible = visible;
  }
  void onHoverKey(const HoverKeyEvent& e) override {
    // Native delete may destroy this object or its nested widget.
    WeakPtr<app::ModuleWidget> live = original;
    if (!live) return;
    live->widget::Widget::onHoverKey(e);
    if (e.isConsumed()) return;
    auto* afterChildren = live.get();
    if (!afterChildren) return;
    bool wasVisible = afterChildren->visible;
    afterChildren->visible = false;
    ModuleWidget::onHoverKey(e);
    if (live) live->visible = wasVisible;
    if (!e.isConsumed() && live) live->onHoverKey(e);
  }
  ~Holder() override {
    // Native Rack removal sees this wrapper, whereas the touched control's
    // nearest ModuleWidget ancestor is the original nested widget.
    if (APP && APP->scene) {
      auto* rack = APP->scene->rack;
      if (rack->touchedParam &&
          rack->touchedParam->getAncestorOfType<app::ModuleWidget>() ==
              original)
        rack->touchedParam = nullptr;
    }
    // Only the original widget owns the engine module.
    module = nullptr;
  }
};
struct Beekapoo;
struct RackLayoutSnapshot {
  std::map<int64_t, Vec> positions;
  std::vector<int64_t> selected;
  void capture() {
    auto* rack = APP->scene->rack;
    for (auto* w : rack->getModules())
      if (w->visible) positions[w->module->id] = w->box.pos;
    for (auto* w : rack->getSelected()) selected.push_back(w->module->id);
  }
  void restore() const {
    auto* rack = APP->scene->rack;
    app::ModuleWidget* last = nullptr;
    for (const auto& p : positions)
      if (auto* w = rack->getModule(p.first))
        if (w->visible) {
          w->box.pos = p.second;
          last = w;
        }
    if (last) rack->requestModulePos(last, last->box.pos);
    rack->deselectAll();
    for (auto id : selected)
      if (auto* w = rack->getModule(id))
        if (w->visible) rack->select(w, true);
  }
};
struct GroupHistory : history::ModuleChange {
  RackLayoutSnapshot before, after;
  void undo() override;
  void redo() override;
};
struct ActionButton : phlooper::WarpedDropdown {
  int icon = -1;
  bool iconAndText = false;
  std::function<std::string()> help;
  WeakPtr<ui::Tooltip> tooltip;
  void onEnter(const EnterEvent&) override {
    if (!settings::tooltips || tooltip) return;
    auto* created = new ui::Tooltip;
    created->text = help ? help() : label();
    created->box.pos = getAbsoluteOffset(box.size);
    APP->scene->addChild(created);
    tooltip = created;
  }
  void onLeave(const LeaveEvent&) override { clearTooltip(); }
  void clearTooltip() {
    if (tooltip) {
      // Scene may already be traversing its children while this control is
      // destroyed. Leave sibling removal to that traversal.
      tooltip->hide();
      tooltip->requestDelete();
      tooltip = nullptr;
    }
  }
  ~ActionButton() override { clearTooltip(); }
  void draw(const DrawArgs& args) override {
    showLabel = icon < 0 || iconAndText;
    if (iconAndText) textInset = 17;
    phlooper::WarpedDropdown::draw(args);
    if (icon < 0) return;
    Vec c = box.size * .5f + (isPressed() ? Vec(2, 1) : Vec(-1, -1));
    if (iconAndText) c.x = 8 + (isPressed() ? 2 : -1);
    nvgSave(args.vg);
    nvgTranslate(args.vg, c.x, c.y);
    float iconScale =
        icon == 6 ? .75f * std::min({1.f, (box.size.x - 4) / 12.f,
                                     (box.size.y - 4) / 12.f})
                  : std::min(1.f, std::max(.4f, (box.size.x - 6) / 16.f));
    if (iconAndText) iconScale = .7f * std::min(1.f, (box.size.y - 4) / 16.f);
    nvgScale(args.vg, iconScale, iconScale);
    nvgStrokeColor(args.vg, nvgRGB(15, 42, 36));
    nvgFillColor(args.vg, nvgRGB(15, 42, 36));
    nvgStrokeWidth(args.vg, 1.3f);
    if (icon == 1) {
      nvgBeginPath(args.vg);
      nvgMoveTo(args.vg, -6, -6);
      nvgLineTo(args.vg, 6, -5);
      nvgLineTo(args.vg, 5, 6);
      nvgLineTo(args.vg, -5, 5);
      nvgClosePath(args.vg);
      nvgStroke(args.vg);
      for (Vec p :
           {Vec(-3, -3), Vec(3, -2), Vec(0, 0), Vec(-2, 3), Vec(3, 3)}) {
        nvgBeginPath(args.vg);
        nvgCircle(args.vg, p.x, p.y, .8f);
        nvgFill(args.vg);
      }
    } else if (icon == 0) {
      for (int j = 0; j < 2; ++j) {
        nvgBeginPath(args.vg);
        nvgMoveTo(args.vg, -7, -2 + j * 4);
        nvgBezierTo(args.vg, -2, -9 + j * 4, 2, 5 + j * 4, 7, -2 + j * 4);
        nvgStroke(args.vg);
      }
    } else if (icon == 3) {
      nvgBeginPath(args.vg);
      nvgMoveTo(args.vg, -7, 0);
      nvgBezierTo(args.vg, -3, -7, 3, -7, 7, 0);
      nvgBezierTo(args.vg, 3, 7, -3, 7, -7, 0);
      nvgStroke(args.vg);
      nvgBeginPath(args.vg);
      nvgCircle(args.vg, 0, 0, 2);
      nvgFill(args.vg);
    } else if (icon == 4) {
      nvgBeginPath(args.vg);
      nvgMoveTo(args.vg, -7, -2);
      nvgLineTo(args.vg, 0, -6);
      nvgLineTo(args.vg, 7, -2);
      nvgLineTo(args.vg, 0, 2);
      nvgClosePath(args.vg);
      nvgStroke(args.vg);
      nvgBeginPath(args.vg);
      nvgMoveTo(args.vg, -7, 2);
      nvgLineTo(args.vg, 0, 6);
      nvgLineTo(args.vg, 7, 2);
      nvgStroke(args.vg);
    } else if (icon == 7) {
      // An uneven eject triangle above a separate tray.
      nvgBeginPath(args.vg);
      nvgMoveTo(args.vg, -6, 1);
      nvgLineTo(args.vg, .5f, -6);
      nvgLineTo(args.vg, 6, 1.5f);
      nvgClosePath(args.vg);
      nvgFill(args.vg);
      nvgBeginPath(args.vg);
      nvgMoveTo(args.vg, -6, 4);
      nvgLineTo(args.vg, 6, 4.5f);
      nvgLineTo(args.vg, 5.5f, 7);
      nvgLineTo(args.vg, -5.5f, 6.5f);
      nvgClosePath(args.vg);
      nvgFill(args.vg);
    } else if (icon == 6) {
      nvgStrokeWidth(args.vg, 1.8f);
      nvgBeginPath(args.vg);
      nvgMoveTo(args.vg, -6, .4f);
      nvgLineTo(args.vg, 6, -.3f);
      nvgMoveTo(args.vg, .3f, -6);
      nvgLineTo(args.vg, -.2f, 6);
      nvgStroke(args.vg);
    } else if (icon == 5) {
      nvgBeginPath(args.vg);
      nvgMoveTo(args.vg, -5, -5);
      nvgLineTo(args.vg, 5, 5);
      nvgMoveTo(args.vg, 5, -5);
      nvgLineTo(args.vg, -5, 5);
      nvgStroke(args.vg);
    } else {
      nvgBeginPath(args.vg);
      nvgMoveTo(args.vg, 5, -4);
      nvgBezierTo(args.vg, -5, -10, -10, 5, 0, 6);
      nvgLineTo(args.vg, 5, 5);
      nvgStroke(args.vg);
      nvgBeginPath(args.vg);
      nvgMoveTo(args.vg, 1, 2);
      nvgLineTo(args.vg, 6, 5);
      nvgLineTo(args.vg, 2, 8);
      nvgStroke(args.vg);
    }
    nvgRestore(args.vg);
  }
  std::string text;
  std::function<std::string()> caption;
  std::function<void()> action;
  std::function<bool()> pressed;
  std::string label() override { return caption ? caption() : text; }
  bool isPressed() override { return pressed && pressed(); }
  void onButton(const ButtonEvent& e) override {
    if (e.button == GLFW_MOUSE_BUTTON_LEFT && e.action == GLFW_PRESS) {
      WeakPtr<widget::Widget> live = this;
      auto callback = action;
      e.consume(this);
      if (callback) callback();
      if (!live) e.consume(nullptr);
      return;
    }
    widget::OpaqueWidget::onButton(e);
  }
};
// Keep the source widget's switch semantics, including custom callbacks.
struct ExposedSwitch : app::Switch {
  WeakPtr<app::ParamWidget> source;
  struct Face : phlooper::WarpedDropdown {
    ExposedSwitch* control;
    std::string label() override {
      auto* q = control->getParamQuantity();
      if (!q) return "";
      if (control->momentary) return "•";
      if (q->getMaxValue() - q->getMinValue() <= 1.f)
        return q->getValue() > q->getMinValue() ? "On" : "Off";
      return std::to_string(int(std::round(q->getValue())));
    }
    bool isPressed() override {
      auto* q = control->getParamQuantity();
      return q && q->getValue() > q->getMinValue();
    }
    void onButton(const ButtonEvent&) override {}
    void onHover(const HoverEvent&) override {}
    void onEnter(const EnterEvent&) override {}
    void onLeave(const LeaveEvent&) override {}
  };
  ExposedSwitch() {
    box.size = Vec(25, 24);
    auto* face = new Face;
    face->control = this;
    face->configure(25, 24, 3, 2, .9f, 41);
    face->textSize = 11;
    face->faceColor = nvgRGB(48, 167, 145);
    addChild(face);
  }
  void onHover(const HoverEvent& e) override { e.consume(this); }
  void onDragStart(const DragStartEvent& e) override {
    if (source)
      source->onDragStart(e);
    else
      app::Switch::onDragStart(e);
  }
  void onDragEnd(const DragEndEvent& e) override {
    if (source)
      source->onDragEnd(e);
    else
      app::Switch::onDragEnd(e);
  }
  void onDoubleClick(const DoubleClickEvent& e) override {
    if (source)
      source->onDoubleClick(e);
    else
      app::Switch::onDoubleClick(e);
  }
};
struct MappingTooltip : ui::Tooltip {
  std::function<std::string()> caption;
  void step() override {
    text = caption ? caption() : "";
    ui::Tooltip::step();
  }
};
template <class Base>
struct MappedParam : Base {
  std::function<std::string()> caption;
  WeakPtr<MappingTooltip> tip;
  void clearTip() {
    if (tip) {
      // Drop callbacks before the source control dies; Scene owns deletion.
      tip->caption = {};
      tip->hide();
      tip->requestDelete();
      tip = nullptr;
    }
  }
  ~MappedParam() override { clearTip(); }
  void onEnter(const widget::Widget::EnterEvent&) override {
    if (!settings::tooltips || tip) return;
    auto* created = new MappingTooltip;
    created->caption = caption;
    created->box.pos = this->getAbsoluteOffset(this->box.size);
    APP->scene->addChild(created);
    tip = created;
  }
  void onLeave(const widget::Widget::LeaveEvent&) override { clearTip(); }
};
// Keep Rack responsible for port details and live voltages; only add the
// source module heading to the presentation on Beekapoo's front panel.
struct MappedPortTooltip : ui::Tooltip {
  WeakPtr<ui::Tooltip> native;
  WeakPtr<widget::Widget> anchor;
  std::function<std::string()> caption;
  void step() override {
    if (!native || !anchor) {
      hide();
      requestDelete();
      return;
    }
    native->step();
    std::string heading = caption ? caption() : "";
    heading = heading.substr(0, heading.find('\n'));
    text = heading.empty() ? native->text : heading + "\n" + native->text;
    ui::Tooltip::step();
    box.pos = anchor->getAbsoluteOffset(anchor->box.size).round();
    if (parent) box = box.nudge(parent->box.zeroPos());
  }
};
struct ScopedVisiblePortCables {
  widget::Widget* container = nullptr;
  std::list<widget::Widget*> hidden, hiddenCables;
  widget::Widget* cableContainer;
  ScopedVisiblePortCables(app::RackWidget* rack, app::PortWidget* source,
                          bool filterCables = false) {
    cableContainer = rack->getCableContainer();
    std::set<void*> omit;
    for (auto* cable : rack->getCompleteCablesOnPort(source))
      if (!cable->visible) {
        omit.insert(static_cast<void*>(cable->inputPlug));
        omit.insert(static_cast<void*>(cable->outputPlug));
      }
    if (filterCables)
      for (auto it = cableContainer->children.begin();
           it != cableContainer->children.end();) {
        auto current = it++;
        if (auto* cable = dynamic_cast<app::CableWidget*>(*current))
          if (!cable->visible &&
              (cable->inputPort == source || cable->outputPort == source))
            hiddenCables.splice(hiddenCables.end(), cableContainer->children,
                                current);
      }
    // PlugWidget is opaque in released Rack SDKs. Find its public parent
    // by identity, as the panel's plug-visibility code already does.
    for (auto* child : rack->children)
      for (auto* plug : child->children)
        if (omit.count(static_cast<void*>(plug))) container = child;
    if (!container) return;
    for (auto it = container->children.begin();
         it != container->children.end();) {
      auto current = it++;
      if (omit.count(static_cast<void*>(*current)))
        hidden.splice(hidden.end(), container->children, current);
    }
  }
  ~ScopedVisiblePortCables() {
    cableContainer->children.splice(cableContainer->children.begin(),
                                    hiddenCables);
    // Keep hidden plugs below visible ones; their ownership never changes.
    if (container)
      container->children.splice(container->children.begin(), hidden);
  }
};
struct VisiblePortDeleteItem : ui::MenuItem {
  WeakPtr<app::PortWidget> source;
  void onAction(const ActionEvent&) override {
    if (!source) return;
    ScopedVisiblePortCables scope(APP->scene->rack, source.get());
    source->deleteTopCableAction();
  }
};
struct PortFace : widget::OpaqueWidget {
  bool compact = true, input = true;
  JackSmallPointingDown tinyDown;
  JackSmallPointingUp tinyUp;
  JackLargePointingDown largeDown;
  JackLargePointingUp largeUp;
  app::PortWidget* artwork() {
    auto* result = input ? static_cast<app::PortWidget*>(&largeDown)
                         : static_cast<app::PortWidget*>(&largeUp);
    result->type = input ? engine::Port::INPUT : engine::Port::OUTPUT;
    return result;
  }
  std::function<std::string()> caption;
  WeakPtr<ui::Tooltip> tip;
  bool nativeTooltip = false;
  void clearTip() {
    if (source) {
      LeaveEvent leave;
      source->onLeave(leave);
    }
    if (tip) {
      if (auto* fallback = dynamic_cast<MappingTooltip*>(tip.get()))
        fallback->caption = {};
      if (auto* mapped = dynamic_cast<MappedPortTooltip*>(tip.get()))
        mapped->caption = {};
      tip->hide();
      tip->requestDelete();
    }
    nativeTooltip = false;
    tip = nullptr;
  }
  void nativeTipAction(const std::function<void()>& action) {
    std::set<widget::Widget*> before(APP->scene->children.begin(),
                                     APP->scene->children.end());
    action();
    for (auto* child : APP->scene->children)
      if (!before.count(child))
        if (auto* tooltip = dynamic_cast<ui::Tooltip*>(child)) {
          auto* mapped = new MappedPortTooltip;
          mapped->native = tooltip;
          mapped->anchor = this;
          mapped->caption = caption;
          tooltip->hide();
          APP->scene->addChild(mapped);
          tip = mapped;
          nativeTooltip = true;
          break;
        }
  }
  void step() override {
    if (tip) {
      auto* hovered = APP->event->getHoveredWidget();
      auto* dragHovered = APP->event->getDragHoveredWidget();
      bool shown = visible;
      for (auto* p = parent; p && shown; p = p->parent) shown = p->visible;
      if (!shown || !settings::tooltips ||
          (hovered != this && hovered != source.get() && dragHovered != this &&
           dragHovered != source.get()))
        clearTip();
    }
    widget::OpaqueWidget::step();
  }
  ~PortFace() override { clearTip(); }
  WeakPtr<app::PortWidget> source;
  std::function<void(app::CableWidget*)> patched;
  PortFace() {
    tinyDown.type = engine::Port::INPUT;
    tinyUp.type = engine::Port::OUTPUT;
  }
  void draw(const DrawArgs& args) override {
    if (compact) {
      tinyDown.type = engine::Port::INPUT;
      tinyUp.type = engine::Port::OUTPUT;
      if (input)
        tinyDown.draw(args);
      else
        tinyUp.draw(args);
    } else if (source) {
      artwork()->draw(args);
    }
  }
  void drawLayer(const DrawArgs& args, int layer) override {
    if (!compact && source) {
      artwork()->drawLayer(args, layer);
    }
  }
  void onHover(const HoverEvent& e) override {
    // Rack highlights cables by exact port-widget identity. Target the real
    // endpoint, as we already do for patching, rather than the artwork face.
    if (source) {
      source->onHover(e);
      // Real mouse hover targets the original port for Rack's cable
      // highlighting, so its Enter event bypasses this front-panel face.
      // Create/decorate that same native tooltip before Rack dispatches Enter.
      if (auto* mapped = dynamic_cast<MappedPortTooltip*>(tip.get()))
        if (!mapped->native) clearTip();
      if (settings::tooltips && !tip && e.context &&
          e.context->target == source.get())
        nativeTipAction([&] {
          EnterEvent enter;
          source->onEnter(enter);
        });
    } else
      widget::OpaqueWidget::onHover(e);
  }
  void onButton(const ButtonEvent& e) override {
    if (!source) return;
    if (e.button == GLFW_MOUSE_BUTTON_RIGHT && e.action == GLFW_PRESS) {
      std::set<widget::Widget*> before(APP->scene->children.begin(),
                                       APP->scene->children.end());
      ScopedVisiblePortCables scope(APP->scene->rack, source.get(), true);
      source->onButton(e);
      for (auto* child : APP->scene->children)
        if (!before.count(child))
          for (auto* sub : child->children)
            if (auto* menu = dynamic_cast<ui::Menu*>(sub))
              if (menu->children.size() > 1 &&
                  dynamic_cast<ui::MenuLabel*>(menu->children.front()))
                if (auto* item = dynamic_cast<ui::MenuItem*>(
                        *std::next(menu->children.begin()))) {
                  // Native port menus start with the port name and Delete top
                  // cable.
                  auto* safe = new VisiblePortDeleteItem;
                  safe->source = source;
                  safe->text = item->text;
                  safe->rightText = item->rightText;
                  safe->disabled = item->disabled;
                  menu->addChildAbove(safe, item);
                  menu->removeChild(item);
                  delete item;
                }
      return;
    }
    if (e.button != GLFW_MOUSE_BUTTON_LEFT || e.action != GLFW_PRESS) {
      source->onButton(e);
      return;
    }
    // Rack's getTopPlug() includes invisible plugs. Temporarily omit hidden
    // internal plugs while it handles this click and starts the native drag,
    // so a front-panel gesture cannot pick up an invisible internal cable.
    ScopedVisiblePortCables hidden(APP->scene->rack, source.get());
    source->onButton(e);
    if (e.context && e.context->target == source.get())
      APP->event->setDraggedWidget(source.get(), e.button);
  }
  void onDragDrop(const DragDropEvent& e) override {
    std::vector<WeakPtr<app::CableWidget>> incomplete;
    for (auto* child : APP->scene->rack->getCableContainer()->children)
      if (auto* cable = dynamic_cast<app::CableWidget*>(child))
        if (!cable->isComplete()) incomplete.push_back(cable);
    if (source) nativeTipAction([&] { source->onDragDrop(e); });
    if (patched)
      for (auto& cable : incomplete)
        if (cable && cable->isComplete() && cable->cable) patched(cable.get());
  }
  void onDragEnter(const DragEnterEvent& e) override {
    if (source) nativeTipAction([&] { source->onDragEnter(e); });
  }
  void onDragLeave(const DragLeaveEvent& e) override {
    if (source) source->onDragLeave(e);
    clearTip();
  }
  void onEnter(const EnterEvent& e) override {
    if (auto* mapped = dynamic_cast<MappedPortTooltip*>(tip.get()))
      if (!mapped->native) clearTip();
    if (!settings::tooltips || tip) return;
    if (source && source->module) {
      nativeTipAction([&] { source->onEnter(e); });
    } else {
      auto* fallback = new MappingTooltip;
      fallback->caption = caption;
      fallback->box.pos = getAbsoluteOffset(box.size);
      APP->scene->addChild(fallback);
      tip = fallback;
    }
  }
  void onLeave(const LeaveEvent& e) override { clearTip(); }
};
struct Beekapoo : app::ModuleWidget {
  struct Member {
    int64_t id;
    Vec position;
    WeakPtr<app::ModuleWidget> widget;
    WeakPtr<Holder> holder;
  };
  struct Port {
    int64_t id;
    int portId;
    bool input;
    Vec originalPosition;
    PortFace* face;
  };
  app::RackWidget* homeRack = nullptr;
  std::vector<Member> members;
  std::map<int64_t, Vec> subpatchPositions;
  bool expanded = false;
  bool ejecting = false;
  bool lastBypass = false;
  bool captureNewModules = true, hasEditPlacement = false;
  bool markMembers = true;
  std::set<std::pair<int64_t, int>> randomLocks;
  bool randomLocked(int64_t id, int param) const {
    return randomLocks.count({id, -1}) || randomLocks.count({id, param});
  }
  void toggleRandomLock(int64_t id, int param) {
    auto* action = beginAction("toggle subpatch randomization lock");
    auto key = std::make_pair(id, param);
    if (!randomLocks.erase(key)) randomLocks.insert(key);
    saveGroup();
    finishAction(action);
  }
  void unlockRandomization() {
    if (randomLocks.empty()) return;
    auto* action = beginAction("unlock subpatch randomization");
    randomLocks.clear();
    saveGroup();
    finishAction(action);
  }
  bool arrangeByModule = false;
  bool jacksOnly = false;
  bool editShowsText = false;
  std::set<int64_t> panelCableIds, knownCableIds, internalCableIds;
  struct PlugWidgets {
    WeakPtr<widget::Widget> input, output;
  };
  std::map<int64_t, PlugWidgets> plugWidgets;
  unsigned hiddenKinds = 0;
  std::map<int64_t, unsigned> hiddenModuleKinds;
  bool controlVisible(int64_t id, int kind) const {
    auto it = hiddenModuleKinds.find(id);
    unsigned local = it == hiddenModuleKinds.end() ? 0 : it->second;
    return !((hiddenKinds | local) & (1u << kind));
  }
  float actionPulse[6]{};
  float returnHighlight = 0;
  std::vector<Port> ports;
  widget::Widget* controls = nullptr;
  struct Parameter {
    int64_t id;
    int paramId;
    app::ParamWidget* knob;
  };
  std::vector<Parameter> parameters;
  int layoutTier =
      0;  // derived responsive tier; retained only for old patch migration
  float requestedWidth = 180, lastLayoutWidth = -1;
  ResponsiveLayout responsive;
  widget::SvgWidget titleArtwork, logoArtwork;
  Rect titleInk;
  ComputerscareResizeHandle *leftHandle = nullptr, *rightHandle = nullptr;
  phlooper::WarpedBlock *ownFrame = nullptr, *titleFrame = nullptr;
  struct ControlLabel {
    Vec pos;
    float width;
    std::string text;
    bool output;
    int align;
  };
  std::vector<ControlLabel> controlLabels;
  std::vector<ActionButton*> actionButtons;
  std::vector<ActionButton*> managementButtons;
  ActionButton* addSelectionButton = nullptr;
  phlooper::WarpedBlock* frame = nullptr;
  phlooper::WarpedBlock *inputFrame = nullptr, *outputFrame = nullptr;
  struct Heading {
    Vec pos;
    std::string text;
    bool output;
  };
  std::vector<Heading> headings;
  std::string memberName(int64_t id) {
    auto* w = find(id);
    if (!w || !w->model) return "Module";
    int count = 0, index = 0;
    for (const auto& m : members)
      if (auto* other = find(m.id))
        if (other->model == w->model) {
          ++count;
          if (m.id == id) index = count;
        }
    return (w->model->name.empty() ? w->model->slug : w->model->name) +
           (count > 1 ? " #" + std::to_string(index) : "");
  }
  WeakPtr<widget::Widget> subview;
  void showView();
  WeakPtr<widget::Widget> editSession;
  bool editPreview = false;
  json_t* portableContents(bool external);
  bool importContents(json_t* root);
  void duplicateAction(bool cables);
  void disconnectPanelAction();
  void prepareContainedSave() {
    for (auto& member : members)
      if (auto* w = find(member.id)) APP->engine->prepareSaveModule(w->module);
  }
  void deleteGroupAction();
  void pruneMissingMembers();
  void editSubpatch();
  void ejectAction();
  void focusEditedMembers();
  void moveEditedMembers(bool near, bool recordHistory = true,
                         bool keepPosition = false);
  void closeEditSession();
  void forgetEditedMembers(const std::set<int64_t>& removed);
  json_t* editSessionState();
  void resumeEditSession(json_t* saved);
  void closeView() {
    closeEditSession();
    if (subview) {
      subview->setVisible(false);
      subview->requestDelete();
      subview = nullptr;
    }
  }
  std::string status = "Select modules, then right-click here";
  Beekapoo(Module* m) {
    homeRack = APP->scene->rack;
    installDevWatcherToolbar();
    setModule(m);
    if (m) requestedWidth = m->panelWidth.load();
    auto* panel = new ComputerscareSVGPanel;
    panel->setBackground(APP->window->loadSvg(asset::plugin(
        pluginInstance, "res/panels/ComputerscareBeekapooPanel.svg")));
    setPanel(panel);
    panel->visible = false;
    titleArtwork.setSvg(APP->window->loadSvg(
        asset::plugin(pluginInstance, "res/Beekapoo/title.svg")));
    titleInk = Rect(Vec(), titleArtwork.box.size);
    if (titleArtwork.svg && titleArtwork.svg->handle->shapes) {
      Vec minimum(INFINITY, INFINITY), maximum(-INFINITY, -INFINITY);
      for (auto* shape = titleArtwork.svg->handle->shapes; shape;
           shape = shape->next) {
        minimum.x = std::min(minimum.x, shape->bounds[0]);
        minimum.y = std::min(minimum.y, shape->bounds[1]);
        maximum.x = std::max(maximum.x, shape->bounds[2]);
        maximum.y = std::max(maximum.y, shape->bounds[3]);
      }
      titleInk = Rect(minimum, maximum - minimum);
    }
    logoArtwork.setSvg(APP->window->loadSvg(
        asset::plugin(pluginInstance, "res/Beekapoo/computerscare-logo.svg")));
    titleFrame = new PackedPanelBlock;
    titleFrame->configure(Rect(Vec(6, 4), Vec(168, 32)), 4, 1.1f, 809,
                          nvgRGB(198, 201, 199));
    addChild(titleFrame);
    ownFrame = new PackedPanelBlock;
    ownFrame->configure(Rect(Vec(6, 26), Vec(168, 100)), 4, 1.3f, 811,
                        nvgRGB(186, 198, 192));
    addChild(ownFrame);
    frame = new PackedPanelBlock;
    frame->configure(Rect(Vec(9, 140), Vec(342, 213)), 5, 1.8f, 719,
                     nvgRGB(198, 206, 202));
    addChild(frame);
    inputFrame = new PackedPanelBlock;
    inputFrame->configure(Rect(Vec(9, 253), Vec(165, 100)), 5, 2.f, 723,
                          nvgRGB(198, 206, 202));
    addChild(inputFrame);
    outputFrame = new PackedPanelBlock;
    outputFrame->configure(Rect(Vec(186, 253), Vec(165, 100)), 5, 2.f, 727,
                           nvgRGB(105, 119, 114));
    addChild(outputFrame);
    // Saved module IDs keep the artwork stable on reload. New instances and
    // duplicates get different shapes; browser artwork is randomized once.
    uint64_t identity = m && m->id >= 0 ? uint64_t(m->id) : random::u64();
    unsigned artworkSeed = unsigned(identity) ^ unsigned(identity >> 32);
    for (auto* block : {titleFrame, ownFrame, frame, inputFrame, outputFrame}) {
      unsigned seed = block->shapeSeed ^ artworkSeed;
      seed ^= seed >> 16;
      seed *= 0x7feb352du;
      seed ^= seed >> 15;
      seed *= 0x846ca68bu;
      block->shapeSeed = seed ^ (seed >> 16);
    }
    auto* show = new ActionButton;
    show->text = "Edit";
    show->caption = [this] {
      return (editShowsText ? "Edit " : "") + std::to_string(members.size());
    };
    show->configure(52, 24, 3, 2, .8f, 713);
    show->box.pos = Vec(15, 39);
    show->help = [] { return "Edit subpatch in Rack"; };
    show->action = [this] { editSubpatch(); };
    show->pressed = [this] { return bool(editSession); };
    addChild(show);
    managementButtons.push_back(show);
    auto* eject = new ActionButton;
    eject->text = "Eject";
    eject->configure(50, 24, 3, 2, .8f, 715);
    eject->icon = 7;
    eject->help = [] { return "Release the subpatch and delete Beekapoo"; };
    eject->action = [this] { ejectAction(); };
    addChild(eject);
    managementButtons.push_back(eject);
    addSelectionButton = new ActionButton;
    addSelectionButton->text = "+";
    addSelectionButton->icon = 6;
    addSelectionButton->configure(18, 21, 3, 2, .8f, 716);
    addSelectionButton->textSize = 14;
    addSelectionButton->help = [] {
      return "Add selected modules to Beekapoo";
    };
    addSelectionButton->action = [this] {
      if (!canAddSelection()) return;
      if (members.empty())
        groupAction(true);
      else
        addSelection();
    };
    addChild(addSelectionButton);
    const char* captions[] = {"Wiggle", "Randomize", "Initialize"};
    for (int row = 0; row < 2; ++row)
      for (int col = 0; col < 3; ++col) {
        int index = row * 3 + col;
        float x = 55 + col * 80, y = 73 + row * 28;
        auto* button = new ActionButton;
        button->text = captions[col];
        std::string fullHelp =
            std::string(captions[col]) +
            (row == 0 ? " exposed parameters" : " all member modules");
        button->help = [fullHelp] { return fullHelp; };
        button->textSize = 12;
        button->configure(56, 23, 3, 2, .9f, 730 + index);
        button->box.pos = Vec(x, y);
        button->faceColor = COLOR_COMPUTERSCARE_GREEN;
        button->action = [this, index] { parameterAction(index, true); };
        button->pressed = [this, index, button] {
          return actionPulse[index] > 0 ||
                 APP->event->getDraggedWidget() == button;
        };
        addChild(button);
        actionButtons.push_back(button);
        addInput(createInputCentered<JackSmallPointingDown>(Vec(x + 68, y + 11),
                                                            m, index));
      }
    addParam(
        createParamCentered<SmallKnob>(Vec(328, 94), m, Module::WIGGLE_AMOUNT));
    controls = new widget::Widget;
    addChild(controls);
    leftHandle = new ComputerscareResizeHandle;
    leftHandle->box.size.x = 5;
    addChild(leftHandle);
    rightHandle = new ComputerscareResizeHandle;
    rightHandle->right = true;
    rightHandle->box.size.x = 5;
    addChild(rightHandle);
    layoutControls();
    installRackHooks();
    if (m)
      m->captureContents = [this](bool external) {
        return portableContents(external);
      };
  }
  ~Beekapoo() override {
    if (auto* m = getModule<Module>()) {
      std::lock_guard<std::mutex> lock(m->stateMutex);
      m->captureContents = {};
    }
    closeView();
    for (const auto& port : ports)
      if (auto* source = findPort(port))
        source->box.pos = port.originalPosition;
    std::vector<int64_t> owned;
    for (const auto& member : members) owned.push_back(member.id);
    if (module && !owned.empty()) queueSubpatchDeletion(module->id, owned);
    // The scene hook deletes siblings after native bulk-deletion traversals
    // finish. Destroying them here would invalidate Rack's selected list.
  }
  app::ModuleWidget* find(int64_t id) {
    ScopedSubviewRack mainScope(homeRack);
    auto* registered = APP && APP->scene ? homeRack->getModule(id) : nullptr;
    for (const auto& member : members)
      if (member.id == id) {
        if (!member.widget) return nullptr;
        auto* expected =
            member.holder ? static_cast<app::ModuleWidget*>(member.holder.get())
                          : member.widget.get();
        return registered == expected ? member.widget.get() : nullptr;
      }
    if (auto* holder = dynamic_cast<Holder*>(registered))
      return holder->original;
    return registered;
  }
  void saveGroup() {
    auto* m = getModule<Module>();
    if (!m) return;
    if (members.empty() && !editSession) {
      m->saveGroup(nullptr);
      return;
    }
    auto* root = json_object();

    json_object_set_new(root, "owner", json_integer(m->id));
    if (auto* editing = editSessionState())
      json_object_set_new(root, "editing", editing);
    json_object_set_new(root, "expanded", json_boolean(expanded));
    json_object_set_new(root, "captureNewModules",
                        json_boolean(captureNewModules));
    json_object_set_new(root, "hasEditPlacement",
                        json_boolean(hasEditPlacement));
    json_object_set_new(root, "markMembers", json_boolean(markMembers));
    auto* locks = json_array();
    for (auto it = randomLocks.begin(); it != randomLocks.end();) {
      bool present = false;
      for (const auto& member : members) present |= member.id == it->first;
      if (!present) {
        it = randomLocks.erase(it);
        continue;
      }
      json_array_append_new(
          locks, json_pack("{s:I,s:i}", "module", (json_int_t)it->first,
                           "param", it->second));
      ++it;
    }
    json_object_set_new(root, "randomLocks", locks);
    json_object_set_new(root, "arrangeByModule", json_boolean(arrangeByModule));
    json_object_set_new(root, "hiddenKinds", json_integer(hiddenKinds));
    auto* hidden = json_array();
    for (auto& item : hiddenModuleKinds)
      json_array_append_new(
          hidden, json_pack("{s:I,s:i}", "module", (json_int_t)item.first,
                            "kinds", int(item.second)));
    json_object_set_new(root, "hiddenModules", hidden);
    auto* list = json_array();
    for (const auto& member : members)
      json_array_append_new(
          list,
          json_pack("{s:I,s:f,s:f}", "id", (json_int_t)member.id, "x",
                    (double)member.position.x, "y", (double)member.position.y));
    json_object_set_new(root, "members", list);
    auto* panelCables = json_array();
    for (auto id : panelCableIds)
      json_array_append_new(panelCables, json_integer(id));
    json_object_set_new(root, "panelCables", panelCables);
    auto* internalCables = json_array();
    for (auto id : internalCableIds)
      json_array_append_new(internalCables, json_integer(id));
    json_object_set_new(root, "internalCables", internalCables);
    auto* positions = json_array();
    for (const auto& p : subpatchPositions)
      json_array_append_new(
          positions, json_pack("{s:I,s:f,s:f}", "id", (json_int_t)p.first, "x",
                               double(p.second.x), "y", double(p.second.y)));
    json_object_set_new(root, "subpatchPositions", positions);
    list = json_array();
    for (const auto& port : ports)
      json_array_append_new(
          list, json_pack("{s:I,s:i,s:b}", "module", (json_int_t)port.id,
                          "port", port.portId, "input", port.input));
    json_object_set_new(root, "ports", list);
    list = json_array();
    for (const auto& param : parameters)
      json_array_append_new(
          list, json_pack("{s:I,s:i}", "module", (json_int_t)param.id, "param",
                          param.paramId));
    json_object_set_new(root, "parameters", list);
    m->saveGroup(root);
  }
  Vec parkingPosition() {
    // A point on the visible patch's bottom edge adds no occupied area or
    // scroll extent. On load, real-sized widgets can briefly occupy the row
    // below that edge without Rack shoving any visible modules sideways.
    float bottom = box.pos.y + box.size.y;
    for (auto* w : homeRack->getModules())
      if (w->visible) bottom = std::max(bottom, w->box.getBottom());
    return Vec(box.pos.x, bottom);
  }
  void pack() {
    expanded = false;
    controls->visible = true;
    knownCableIds.clear();
    for (auto* cw : homeRack->getCompleteCables()) {
      knownCableIds.insert(cw->cable->id);
      if (contains(cw->cable->inputModule) &&
          contains(cw->cable->outputModule) &&
          !panelCableIds.count(cw->cable->id))
        internalCableIds.insert(cw->cable->id);
    }
    auto* rack = homeRack;
    for (auto& member : members) {
      auto* w = member.widget.get();
      auto* holder = member.holder.get();
      if (!holder) {
        auto* parent = w->parent;
        rack->select(w, false);
        parent->removeChild(w);
        holder = new Holder(w);
        // Holders remain direct Rack children so native ID/port lookup works.
        parent->addChildBelow(holder, this);
        member.holder = holder;
      }
      rack->select(holder, false);
      holder->box.pos = parkingPosition();
      holder->box.size = Vec();
      holder->visible = false;
      w->visible = false;
    }
  }
  static Rect frameContent(const phlooper::WarpedBlock* block) {
    return Rect(
        block->box.pos + Vec(4, 4),
        Vec(std::max(0.f, block->box.size.x - block->depth - 8),
            std::max(0.f, block->box.size.y - block->depth * .65f - 6.5f)));
  }
  void finishFrames() {
    // Tile the whole face without changing the control grid. Empty space
    // belongs to the nearest section, never a separate panel backdrop.
    float width = box.size.x, height = box.size.y;
    float headerBottom = ownFrame->box.pos.y - 4;
    float portsTop = height;
    if (inputFrame->visible) portsTop = inputFrame->box.pos.y - 4;
    if (outputFrame->visible)
      portsTop = std::min(portsTop, outputFrame->box.pos.y - 4);
    float ownBottom = frame->visible ? frame->box.pos.y - 4 : portsTop;
    titleFrame->box = Rect(Vec(), Vec(width, headerBottom));
    ownFrame->box =
        Rect(Vec(0, headerBottom), Vec(width, ownBottom - headerBottom));
    if (frame->visible)
      frame->box = Rect(Vec(0, ownBottom), Vec(width, portsTop - ownBottom));
    if (inputFrame->visible && outputFrame->visible) {
      bool beside =
          std::abs(inputFrame->box.pos.y - outputFrame->box.pos.y) < 1;
      if (beside) {
        float split =
            (inputFrame->box.getRight() + outputFrame->box.pos.x) * .5f;
        inputFrame->box = Rect(Vec(0, portsTop), Vec(split, height - portsTop));
        outputFrame->box =
            Rect(Vec(split, portsTop), Vec(width - split, height - portsTop));
      } else {
        float split =
            (inputFrame->box.getBottom() + outputFrame->box.pos.y) * .5f;
        inputFrame->box = Rect(Vec(0, portsTop), Vec(width, split - portsTop));
        outputFrame->box = Rect(Vec(0, split), Vec(width, height - split));
      }
    } else {
      auto* ports = inputFrame->visible ? inputFrame : outputFrame;
      if (ports->visible)
        ports->box = Rect(Vec(0, portsTop), Vec(width, height - portsTop));
    }
    for (auto* block : {titleFrame, ownFrame, frame, inputFrame, outputFrame}) {
      block->screwup = block == titleFrame ? 2.f : 2.6f;
      auto inset = [&](unsigned corner) {
        unsigned bits = block->shapeSeed + corner * 0x9e3779b9u;
        bits ^= bits >> 16;
        bits *= 0x7feb352du;
        bits ^= bits >> 15;
        return float(bits & 1023u) / 1023.f * 1.2f;
      };
      // Inward corner offsets vary the dimensions and slant without ever
      // pushing the full silhouette past its fitted drawing bounds.
      block->cornerOffsets = {Vec(inset(1), inset(2)), Vec(-inset(3), inset(4)),
                              Vec(-inset(5), -inset(6)),
                              Vec(inset(7), -inset(8))};
      float topSlope = (block == titleFrame ? 1.3f : 2.f) + inset(10);
      float bottomSlope = (block == titleFrame ? 1.3f : 2.f) + inset(11);
      // Independent slopes on the long edges make trapezoids rather than
      // just adding small corner noise to otherwise horizontal sections.
      block->cornerOffsets[(block->shapeSeed & 1u) ? 0 : 1].y += topSlope;
      block->cornerOffsets[(block->shapeSeed & 2u) ? 2 : 3].y -= bottomSlope;
      block->depth = 3.6f + inset(9);
    }
  }
  static float compactHeaderHeight(float width) {
    return width <= 90 ? 44.f : 32.f;
  }
  static float tinyOwnHeight(float width) {
    float actions = width < 90 ? 18 + 3 * 18 : 3 * 18;
    return (width < 105 ? 2 : 1) * 18 + actions + 12;
  }
  static float minimumJackWidth(int inputs, int outputs) {
    // `inputs` includes the six module CVs, which stay beside their buttons.
    inputs = std::max(0, inputs - 6);
    for (int width = 45; width < 30000; width += 15) {
      if (width - 25 < 34) continue;
      int cols = std::max(1, int((width - 25) / 20));
      float height =
          8 + compactHeaderHeight(width) + tinyOwnHeight(width) + 8 +
          (layoutRows(inputs, cols) + layoutRows(outputs, cols)) * 22 +
          (inputs ? 14 : 0) + (outputs ? 14 : 0) + (inputs && outputs ? 8 : 0);
      if (height <= 368) return width;
    }
    return 30000;
  }
  void layoutOnlyJacks(
      std::vector<widget::Widget*> inputs, std::vector<widget::Widget*> outputs,
      const std::vector<widget::Widget*>& knobs,
      const std::map<widget::Widget*, std::pair<std::string, std::string>>&
          names) {
    jacksOnly = true;
    getParam(Module::WIGGLE_AMOUNT)->visible = false;
    for (auto& parameter : parameters) parameter.knob->visible = false;
    float minimum = minimumJackWidth(inputs.size() + 6, outputs.size());
    float width = std::max(minimum, std::ceil(requestedWidth / 15) * 15);
    bool changed = box.size.x != width;
    box.size.x = width;
    lastLayoutWidth = width;
    if (auto* m = getModule<Module>()) m->panelWidth.store(requestedWidth);
    leftHandle->minWidth = rightHandle->minWidth = minimum;
    rightHandle->box.pos.x = width - rightHandle->box.size.x;
    controlLabels.clear();
    headings.clear();
    frame->visible = false;
    titleFrame->visible = true;
    titleFrame->box = Rect(Vec(6, 4), Vec(std::min(width - 12, 205.f),
                                          compactHeaderHeight(width)));
    layoutTier = 0;
    responsive = ResponsiveLayout();
    responsive.width = width;
    float ownWidth = width - 12;
    float available = ownWidth - 13;
    ownFrame->visible = true;
    float ownTop = 8 + compactHeaderHeight(width);
    ownFrame->box = Rect(Vec(6, ownTop), Vec(ownWidth, tinyOwnHeight(width)));
    bool showAdd = canAddSelection();
    float digits = std::to_string(members.size()).size();
    float editTextWidth = 45 + digits * 6;
    float usable = width - 20;
    editShowsText = usable >= editTextWidth + 17 + (showAdd ? 17 : 0);
    bool ejectText =
        editShowsText && usable >= editTextWidth + 50 + (showAdd ? 17 : 0);
    float x = 10;
    int managementRow = 0;
    for (int i = 0; i < 3; ++i) {
      auto* button = i < 2 ? managementButtons[i] : addSelectionButton;
      button->visible = i < 2 || showAdd;
      if (!button->visible) continue;
      button->icon = i == 0 ? 3 : i == 1 ? 7 : 6;
      button->iconAndText = i == 0 || (i == 1 && ejectText);
      button->textSize = i == 0 && !editShowsText ? 10 : 11;
      float bw = i == 0 ? (editShowsText ? editTextWidth : 22 + digits * 6)
                 : i == 1 && ejectText ? 47.f
                                       : 14.f;
      bw = std::min(bw, usable);
      if (x + bw > width - 9) {
        x = 10;
        ++managementRow;
      }
      button->box = Rect(Vec(x, ownTop + 4 + managementRow * 18), Vec(bw, 16));
      x += bw + 3;
    }
    float actionTop =
        ownTop + 4 + std::max(width < 105 ? 2 : 1, managementRow + 1) * 18;
    bool pairedActions = width >= 90;
    float scopeWidth =
        std::min(std::max(pairedActions ? 34.f : 17.f, available * .5f), 66.f);
    for (int i = 0; i < 6; ++i) {
      int scope = i / 3, action = i % 3;
      Vec cell(10 + scope * scopeWidth, actionTop + action * 18);
      auto* button = actionButtons[i];
      button->visible = pairedActions || action == 0;
      button->icon = action;
      button->box =
          Rect(pairedActions ? cell : Vec(cell.x, actionTop), Vec(14, 16));
      auto* port = getInput(i);
      port->visible = true;
      port->box.pos = pairedActions ? cell + Vec(18, 0)
                                    : Vec(cell.x, actionTop + 18 + action * 18);
    }
    auto portPlan = planLayout(width, 0, 0, inputs.size(), outputs.size());
    bool portLabels = width >= 120 && inputs.size() + outputs.size() <= 4;
    float portRow = portLabels ? 32.f : 22.f;
    int cols = std::max(1, int((width - 25) / 20));
    auto paneHeight = [&](size_t count, int columns) {
      return count ? 12 + layoutRows(count, std::max(1, columns)) * portRow
                   : 0.f;
    };
    float stackedHeight =
        paneHeight(inputs.size(), cols) + paneHeight(outputs.size(), cols) + 8;
    float besideHeight =
        std::max(paneHeight(inputs.size(), portPlan.inputColumns),
                 paneHeight(outputs.size(), portPlan.outputColumns));
    bool beside = !inputs.empty() && !outputs.empty() && width >= 105 &&
                  besideHeight <= stackedHeight;
    int inputCols = beside ? portPlan.inputColumns : cols;
    int outputCols = beside ? portPlan.outputColumns : cols;
    float inputHeight =
        beside ? besideHeight : paneHeight(inputs.size(), inputCols);
    float outputHeight =
        beside ? besideHeight : paneHeight(outputs.size(), outputCols);
    float y =
        368 - (beside ? std::max(inputHeight, outputHeight)
                      : inputHeight + outputHeight +
                            (!inputs.empty() && !outputs.empty() ? 8 : 0));
    float paramTop = ownFrame->box.getBottom() + 8;
    float step = 28;
    for (auto* knob : knobs)
      step = std::max(step, std::max(knob->box.size.x, knob->box.size.y) + 4);
    int knobCols = std::max(1, int((width - 25) / step));
    int labelCols = std::max(1, int((width - 25) / std::max(44.f, step)));
    float labelStep = std::max(42.f, step + 12);
    bool knobLabels = width >= 120 && !knobs.empty() &&
                      12 + layoutRows(knobs.size(), labelCols) * labelStep <=
                          y - paramTop - 8;
    if (knobLabels) {
      knobCols = labelCols;
      step = labelStep;
    }
    bool horizontallyFits =
        knobs.empty() || width - 25 >= step - (knobLabels ? 16 : 4);
    int knobRows = std::max(0, int((y - paramTop - 20) / step));
    int shown =
        horizontallyFits ? std::min(int(knobs.size()), knobCols * knobRows) : 0;
    frame->visible = shown > 0 && (!expanded || editPreview);
    frame->box = Rect(Vec(6, paramTop),
                      Vec(width - 12, 12 + layoutRows(shown, knobCols) * step));
    float cellWidth = (width - 25) / knobCols;
    auto label = [&](widget::Widget* item, Vec pos, float available,
                     bool output) {
      auto found = names.find(item);
      if (found != names.end())
        controlLabels.push_back(
            {pos, available, found->second.first, output, NVG_ALIGN_CENTER});
    };
    for (int i = 0; i < shown; ++i) {
      auto* knob = knobs[i];
      knob->visible = true;
      Vec center(10 + (i % knobCols) * cellWidth + cellWidth * .5f,
                 paramTop + 6 + (i / knobCols) * step +
                     (knobLabels ? 14 : step * .5f));
      knob->box.pos = center - knob->box.size * .5f;
      if (knobLabels) label(knob, center + Vec(0, 23), cellWidth - 5, false);
    }
    float portColumns = inputCols + outputCols;
    float sharedCell = (width - 46) / std::max(1.f, portColumns);
    float inputWidth = beside ? inputCols * sharedCell + 13 : width - 12;
    float outputWidth = beside ? outputCols * sharedCell + 13 : width - 12;
    auto place = [&](std::vector<widget::Widget*>& items,
                     phlooper::WarpedBlock* block, int columns, float x,
                     float blockWidth, float height, bool output) {
      block->visible = !items.empty() && (!expanded || editPreview);
      if (items.empty()) return;
      block->box = Rect(Vec(x, y), Vec(blockWidth, height));
      float cell = (blockWidth - 13) / columns;
      for (size_t i = 0; i < items.size(); ++i) {
        auto* item = items[i];
        item->visible = true;
        if (auto* face = dynamic_cast<PortFace*>(item)) {
          face->compact = true;
          face->box.size = Vec(16, 16);
        }
        Vec center(x + 4 + (i % columns) * cell + cell * .5f,
                   y + 14 + (i / columns) * portRow);
        item->box.pos = center - item->box.size * .5f;
        if (portLabels) label(item, center + Vec(0, 17), cell - 4, output);
      }
    };
    place(inputs, inputFrame, inputCols, 6, inputWidth, inputHeight, false);
    if (!beside && !inputs.empty()) y += inputHeight + 8;
    place(outputs, outputFrame, outputCols,
          beside ? width - 6 - outputWidth : 6, outputWidth, outputHeight,
          true);
    finishFrames();
    if (changed && !ejecting && parent == homeRack->getModuleContainer())
      homeRack->setModulePosForce(this, box.pos);
  }
  void layoutControls() {
    // Editing reveals the originals without blanking the owner's live panel.
    // The optional floating preview is independent of this Rack presentation.
    editPreview = bool(editSession);
    controls->visible = !expanded || editPreview;
    std::vector<widget::Widget*> items[3];
    std::map<widget::Widget*, int> kinds;
    std::map<widget::Widget*, int64_t> moduleIds;
    for (auto& p : parameters) p.knob->visible = controlVisible(p.id, 0);
    for (auto& p : ports)
      p.face->visible = controlVisible(p.id, p.input ? 1 : 2) && findPort(p);
    std::map<widget::Widget*, std::pair<std::string, std::string>> names;
    std::vector<const Member*> ordered;
    for (const auto& m : members) ordered.push_back(&m);
    std::sort(
        ordered.begin(), ordered.end(), [](const Member* a, const Member* b) {
          return a->position.y == b->position.y ? a->position.x < b->position.x
                                                : a->position.y < b->position.y;
        });
    for (auto* m : ordered) {
      std::vector<const Parameter*> params;
      std::vector<const Port*> jacks;
      for (const auto& p : parameters)
        if (p.id == m->id && controlVisible(p.id, 0)) params.push_back(&p);
      for (const auto& p : ports)
        if (p.id == m->id && controlVisible(p.id, p.input ? 1 : 2) &&
            findPort(p))
          jacks.push_back(&p);
      std::sort(params.begin(), params.end(),
                [](const Parameter* a, const Parameter* b) {
                  return a->paramId < b->paramId;
                });
      std::sort(jacks.begin(), jacks.end(), [](const Port* a, const Port* b) {
        return a->portId < b->portId;
      });
      for (auto* p : params)
        if (auto* w = find(p->id))
          if (auto* q = w->module->getParamQuantity(p->paramId)) {
            items[0].push_back(p->knob);
            kinds[p->knob] = 0;
            moduleIds[p->knob] = p->id;
            names[p->knob] = {q->getLabel(), memberName(p->id)};
          }
      for (auto* p : jacks) {
        items[p->input ? 1 : 2].push_back(p->face);
        kinds[p->face] = p->input ? 1 : 2;
        moduleIds[p->face] = p->id;
        auto* source = findPort(*p);
        auto* info = source ? source->getPortInfo() : nullptr;
        names[p->face] = {info ? info->getName() : "Port", memberName(p->id)};
      }
    }
    if (requestedWidth < 210 ||
        minimumLayoutWidth(items[0].size(), items[1].size(), items[2].size()) >
            requestedWidth) {
      layoutOnlyJacks(items[1], items[2], items[0], names);
      return;
    }
    jacksOnly = false;
    titleFrame->visible = true;
    ownFrame->visible = true;
    for (auto* button : managementButtons) button->visible = true;
    for (auto* button : actionButtons) button->visible = true;
    getParam(Module::WIGGLE_AMOUNT)->visible = true;
    if (auto* state = getModule<Module>())
      state->panelWidth.store(requestedWidth);
    responsive = responsiveLayout(requestedWidth, items[0].size(),
                                  items[1].size(), items[2].size());
    if (arrangeByModule) {
      std::vector<widget::Widget*> unified;
      for (auto* m : ordered)
        for (int k = 0; k < 1; ++k)
          for (auto* item : items[k])
            if (moduleIds[item] == m->id) unified.push_back(item);
      responsive = responsiveLayout(requestedWidth, unified.size(),
                                    items[1].size(), items[2].size());
      // Each module starts a row. Feed the padding back to the same planner,
      // so physical minimums still account for the complete grouped layout.
      for (int pass = 0; pass < 12; ++pass) {
        int cols = std::max(1, responsive.paramColumns);
        std::vector<widget::Widget*> padded;
        int64_t last = -1;
        for (auto* item : unified) {
          if (moduleIds[item] != last && !padded.empty())
            while (padded.size() % cols) padded.push_back(nullptr);
          padded.push_back(item);
          last = moduleIds[item];
        }
        auto next =
            responsiveLayout(std::max(requestedWidth, responsive.width),
                             padded.size(), items[1].size(), items[2].size());
        if (next.paramColumns == cols) {
          items[0] = padded;
          responsive = next;
          break;
        }
        responsive = next;
        items[0] = padded;
      }
    }
    float width = responsive.width;
    layoutTier = responsive.tier;
    bool changed = box.size.x != width;
    box.size.x = width;
    lastLayoutWidth = width;
    float minimumWidth = minimumJackWidth(items[1].size() + 6, items[2].size());
    if (leftHandle) leftHandle->minWidth = minimumWidth;
    if (rightHandle) {
      rightHandle->minWidth = minimumWidth;
      rightHandle->box.pos.x = width - rightHandle->box.size.x;
    }
    controlLabels.clear();
    headings.clear();
    titleFrame->box = Rect(Vec(6, 4), Vec(std::min(width - 12, 205.f), 32));
    ownFrame->box =
        Rect(Vec(6, 40), Vec(responsive.ownWidth, responsive.ownHeight));
    float ownLeft = ownFrame->box.pos.x + 4, ownTop = ownFrame->box.pos.y;
    float available = responsive.ownWidth - 12;
    bool showAdd = canAddSelection();
    float textAvailable = available - (showAdd ? 21.f : 0.f) -
                          (responsive.amountInTop ? 36.f : 0.f) - 6;
    bool icons = textAvailable < 85;
    editShowsText = !icons;
    float managementX = ownLeft;
    for (int i = 0; i < 2; ++i) {
      auto* button = managementButtons[i];
      float bw = icons ? 21.f
                       : std::min(i == 0 ? 54.f : 62.f,
                                  textAvailable * (i == 0 ? .46f : .54f));
      if (i == 0)
        bw = icons ? 23.f + 6.f * std::to_string(members.size()).size()
                   : 53.f + 7.f * std::to_string(members.size()).size();
      button->box.pos = Vec(managementX, ownTop + 5);
      button->box.size = Vec(bw, 21);
      button->textSize = bw < 45 ? 12 : 14;
      button->icon = i == 0 ? 3 : 7;
      button->iconAndText = i == 0 || !icons;
      managementX += bw + 3;
    }
    addSelectionButton->visible = showAdd;
    addSelectionButton->box = Rect(Vec(managementX, ownTop + 5), Vec(18, 21));
    if (showAdd) managementX += 21;
    if (layoutTier && responsive.ownColumns == 2) {
      headings.push_back({Vec(ownLeft, ownTop + 36), "Exposed", false});
      headings.push_back(
          {Vec(ownLeft + responsive.ownCell + 8, ownTop + 36), "All", false});
    }
    float matrixTop = ownTop + 28 + (layoutTier ? 14 : 0);
    for (int i = 0; i < 6; ++i) {
      int action = i % 3, scope = i / 3;
      int col = responsive.ownColumns == 2 ? scope : 0;
      int row = responsive.ownColumns == 2 ? action : scope * 3 + action;
      auto* button = actionButtons[i];
      button->icon = action;
      button->box.pos =
          Vec(ownLeft + col * (responsive.ownCell + 8), matrixTop + row * 24);
      button->box.size = Vec(layoutTier ? 23 : 17, 21);
      auto* port = getInput(i);
      if (port)
        port->box.pos = Vec(button->box.getRight() + (layoutTier ? 3 : 1),
                            button->box.getCenter().y - port->box.size.y * .5f);
      if (layoutTier >= 2 && port) {
        std::string label = action == 0   ? "Wiggle"
                            : action == 1 ? "Randomize"
                                          : "Initialize";
        controlLabels.push_back(
            {Vec(port->box.getRight() + 4, button->box.getCenter().y),
             responsive.ownCell - 48, label, false, NVG_ALIGN_LEFT});
      }
    }
    if (auto* amount = getParam(Module::WIGGLE_AMOUNT)) {
      Vec center =
          responsive.amountInTop
              ? Vec(managementX + 8 + amount->box.size.x * .5f, ownTop + 15.5f)
              : Vec(ownLeft + amount->box.size.x * .5f,
                    matrixTop + (responsive.ownColumns == 2 ? 3 : 6) * 24 +
                        10.5f);
      amount->box.pos = center - amount->box.size * .5f;
      if (layoutTier) {
        float labelWidth =
            ownFrame->box.getRight() - 8 - amount->box.getRight() - 4;
        if (labelWidth >= 26)
          controlLabels.push_back({Vec(amount->box.getRight() + 4, center.y),
                                   labelWidth, "Amount", false,
                                   NVG_ALIGN_LEFT});
      }
    }
    float py = responsive.paramTop;
    frame->box = Rect(Vec(responsive.paramLeft, py),
                      Vec(responsive.paramWidth, responsive.paramHeight));
    frame->visible = responsive.paramHeight > 0 && (!expanded || editPreview);
    float portY = 368 - responsive.portHeight;
    float gap = items[1].empty() || items[2].empty() ? 0 : 8;
    int portColumns = responsive.inputColumns + responsive.outputColumns;
    int panes = (!items[1].empty() ? 1 : 0) + (!items[2].empty() ? 1 : 0);
    float portAvailable = width - 12 - gap - panes * 13;
    float portStep = portColumns ? portAvailable / portColumns : 0;
    float inWidth =
        responsive.inputColumns ? responsive.inputColumns * portStep + 13 : 0;
    float outWidth =
        responsive.outputColumns ? responsive.outputColumns * portStep + 13 : 0;
    inputFrame->box = Rect(Vec(6, portY), Vec(inWidth, responsive.portHeight));
    outputFrame->box = Rect(Vec(width - 6 - outWidth, portY),
                            Vec(outWidth, responsive.portHeight));
    inputFrame->visible = !items[1].empty() && (!expanded || editPreview);
    outputFrame->visible = !items[2].empty() && (!expanded || editPreview);
    for (int k = 0; k < 3; ++k) {
      Rect region = k == 0   ? frame->box
                    : k == 1 ? inputFrame->box
                             : outputFrame->box;
      int cols = k == 0   ? responsive.paramColumns
                 : k == 1 ? responsive.inputColumns
                          : responsive.outputColumns;
      if (!cols) continue;
      float row = k == 0 ? responsive.paramRow : responsive.portRow;
      float step = (region.size.x - 13) / cols;
      float header = k == 0 && layoutTier ? 14 : 6;
      std::string lastModule;
      for (size_t i = 0; i < items[k].size(); ++i) {
        auto* item = items[k][i];
        if (!item) continue;
        int kind = kinds[item];
        if (kind) {
          auto* face = static_cast<PortFace*>(item);
          face->compact = layoutTier < 2;
          face->box.size =
              face->compact ? Vec(16, 16) : face->artwork()->box.size;
        }
        bool centered = layoutTier == 0 || (layoutTier == 1 && kind);
        float inset = centered ? step * .5f : item->box.size.x * .5f + 1.5f;
        float rowCenter = (layoutTier == 1 && kind) ? 10.f : row * .5f;
        Vec center = region.pos + Vec(4 + (i % cols) * step + inset,
                                      header + (i / cols) * row + rowCenter);
        item->box.pos = center - item->box.size * .5f;
        if (!layoutTier) continue;
        auto name = names[item];
        if (layoutTier == 1 && name.first.size() > 14)
          name.first = name.first.substr(0, 12) + "…";
        if ((layoutTier >= 3 || (arrangeByModule && layoutTier >= 2)) &&
            k == 0 && name.second != lastModule)
          headings.push_back(
              {Vec(center.x - 10, center.y - 14), name.second, false});
        lastModule = name.second;
        if (layoutTier == 1 && kind) {
          if (name.first.size() > 10)
            name.first = name.first.substr(0, 9) + "…";
          controlLabels.push_back({center + Vec(0, 16), step - 4, name.first,
                                   !arrangeByModule && kind == 2,
                                   NVG_ALIGN_CENTER});
        } else
          controlLabels.push_back({center + Vec(item->box.size.x * .5f + 4, 0),
                                   std::max(12.f, step - item->box.size.x - 6),
                                   name.first, !arrangeByModule && kind == 2,
                                   NVG_ALIGN_LEFT});
      }
    }
    finishFrames();
    if (changed && !ejecting && parent == homeRack->getModuleContainer())
      homeRack->setModulePosForce(this, box.pos);
  }
  void exposeParameter(int64_t id, int index) {
    for (const auto& p : parameters)
      if (p.id == id && p.paramId == index) return;
    auto* w = find(id);
    if (!w || index < 0 || index >= int(w->module->params.size())) return;
    auto* q = w->module->getParamQuantity(index);
    if (!q) return;
    auto* source = w->getParam(index);
    auto* sourceSwitch = dynamic_cast<app::Switch*>(source);
    bool discrete = dynamic_cast<engine::SwitchQuantity*>(q) || q->snapEnabled;
    app::ParamWidget* knob;
    if (sourceSwitch || (discrete && !dynamic_cast<app::Knob*>(source))) {
      auto* button = createParamCentered<MappedParam<ExposedSwitch>>(
          Vec(), w->module, index);
      button->source = source;
      button->momentary = sourceSwitch && sourceSwitch->momentary;
      knob = button;
    } else {
      knob =
          createParamCentered<MappedParam<SmallKnob>>(Vec(), w->module, index);
    }
    auto caption = [this, id, index] {
      auto* w = find(id);
      auto* q = w ? w->module->getParamQuantity(index) : nullptr;
      return memberName(id) + "\n" +
             (q ? q->getLabel() + ": " + q->getDisplayValueString() +
                      q->getUnit()
                : "");
    };
    if (auto* mapped = dynamic_cast<MappedParam<ExposedSwitch>*>(knob))
      mapped->caption = caption;
    if (auto* mapped = dynamic_cast<MappedParam<SmallKnob>*>(knob))
      mapped->caption = caption;
    controls->addChild(knob);
    parameters.push_back({id, index, knob});
    layoutControls();
    saveGroup();
  }
  void restore(json_t* root) {
    if (incomingGroupPreset(module->id, root)) {
      importContents(root);
      return;
    }
    // Native duplicate/preset operations don't remap embedded module IDs.
    // Refuse a copied container's references instead of stealing its group.
    auto* owner = json_object_get(root, "owner");
    if (!json_is_integer(owner) || json_integer_value(owner) != module->id) {
      if (json_object_get(root, "contents"))
        importContents(root);
      else
        endAbstraction();
      return;
    }
    captureNewModules =
        !json_is_false(json_object_get(root, "captureNewModules"));
    hasEditPlacement = json_is_true(json_object_get(root, "hasEditPlacement"));
    markMembers = !json_is_false(json_object_get(root, "markMembers"));
    auto* list = json_object_get(root, "members");
    if (!json_is_array(list)) return;
    if (json_array_size(list) == 0) {
      endAbstraction();
      if (auto* editing = json_object_get(root, "editing")) {
        if (json_is_object(editing)) {
          expanded = true;
          resumeEditSession(editing);
        }
      }
      saveGroup();
      return;
    }
    endAbstraction();
    arrangeByModule = json_is_true(json_object_get(root, "arrangeByModule"));
    auto* locks = json_object_get(root, "randomLocks");
    for (size_t i = 0; i < json_array_size(locks); ++i) {
      auto* lock = json_array_get(locks, i);
      auto* id = json_object_get(lock, "module");
      auto* param = json_object_get(lock, "param");
      if (json_is_integer(id) && json_is_integer(param) &&
          json_integer_value(param) >= -1)
        randomLocks.insert(
            {json_integer_value(id), int(json_integer_value(param))});
    }
    hiddenKinds =
        unsigned(json_integer_value(json_object_get(root, "hiddenKinds"))) & 7;
    hiddenModuleKinds.clear();
    auto* hidden = json_object_get(root, "hiddenModules");
    for (size_t n = 0; n < json_array_size(hidden); ++n) {
      auto* item = json_array_get(hidden, n);
      hiddenModuleKinds[json_integer_value(json_object_get(item, "module"))] =
          unsigned(json_integer_value(json_object_get(item, "kinds"))) & 7;
    }
    panelCableIds.clear();
    auto* panelCables = json_object_get(root, "panelCables");
    for (size_t n = 0; n < json_array_size(panelCables); ++n)
      panelCableIds.insert(json_integer_value(json_array_get(panelCables, n)));
    internalCableIds.clear();
    auto* internalCables = json_object_get(root, "internalCables");
    for (size_t n = 0; n < json_array_size(internalCables); ++n)
      internalCableIds.insert(
          json_integer_value(json_array_get(internalCables, n)));
    bool complete = true;
    std::set<int64_t> seen;
    for (size_t i = 0; i < json_array_size(list); ++i) {
      auto* item = json_array_get(list, i);
      auto* idJ = json_object_get(item, "id");
      int64_t id = json_integer_value(idJ);
      auto* registered = homeRack->getModule(id);
      auto* holder = dynamic_cast<Holder*>(registered);
      auto* w = holder ? holder->original : registered;
      double x = json_number_value(json_object_get(item, "x"));
      double y = json_number_value(json_object_get(item, "y"));
      if (!json_is_integer(idJ) || !w || w == this ||
          dynamic_cast<Beekapoo*>(w) ||
          (!w->visible && (!holder || ownedElsewhere(w->module))) ||
          !seen.insert(id).second || !std::isfinite(x) || !std::isfinite(y)) {
        complete = false;
        continue;
      }
      members.push_back({id, Vec(x, y), w, holder});
    }
    if (!complete) {
      endAbstraction();
      status = "Group incomplete — available modules expanded";
      return;
    }
    auto* savedPositions = json_object_get(root, "subpatchPositions");
    for (size_t n = 0; n < json_array_size(savedPositions); ++n) {
      auto* p = json_array_get(savedPositions, n);
      int64_t id = json_integer_value(json_object_get(p, "id"));
      Vec pos(json_number_value(json_object_get(p, "x")),
              json_number_value(json_object_get(p, "y")));
      if (contains(APP->engine->getModule(id)) && pos.isFinite() &&
          pos.x >= 0 && pos.y >= 0)
        subpatchPositions[id] = pos;
    }
    auto* savedPorts = json_object_get(root, "ports");
    for (size_t i = 0; i < json_array_size(savedPorts); ++i) {
      auto* item = json_array_get(savedPorts, i);
      int64_t id = json_integer_value(json_object_get(item, "module"));
      int index = json_integer_value(json_object_get(item, "port"));
      bool input = json_is_true(json_object_get(item, "input"));
      if (!contains(APP->engine->getModule(id))) continue;
      auto* w = find(id);
      if (!w || index < 0) continue;
      auto* port = input ? w->getInput(index) : w->getOutput(index);
      if (port) expose(w, port);
    }
    auto restoreParam = [&](json_t* param) {
      int64_t id = json_integer_value(json_object_get(param, "module"));
      if (contains(APP->engine->getModule(id)))
        exposeParameter(id,
                        json_integer_value(json_object_get(param, "param")));
    };
    auto* savedParams = json_object_get(root, "parameters");
    if (json_is_array(savedParams)) {
      for (size_t n = 0; n < json_array_size(savedParams); ++n)
        restoreParam(json_array_get(savedParams, n));
    } else if (auto* legacy = json_object_get(root, "parameter")) {
      restoreParam(legacy);
    }
    pack();
    if (json_is_true(json_object_get(root, "expanded"))) revealMembers(false);
    saveGroup();
    status = expanded ? "Modules expanded — abstraction retained"
                      : std::to_string(members.size()) +
                            " modules running behind this panel";
    if (expanded && json_is_object(json_object_get(root, "editing")))
      resumeEditSession(json_object_get(root, "editing"));
  }
  app::PortWidget* findPort(const Port& p) {
    auto* w = find(p.id);
    return w ? (p.input ? w->getInput(p.portId) : w->getOutput(p.portId))
             : nullptr;
  }
  bool contains(engine::Module* m) const {
    for (const auto& member : members)
      if (m && member.id == m->id) return true;
    return false;
  }
  void showPlugs(app::CableWidget* cable, bool visible) {
    // Cache weak references to Rack's own plugs. Rediscover only when a cable
    // or plug is recreated, rather than scanning the widget tree every frame.
    auto& plugs = plugWidgets[cable->cable->id];
    if (!plugs.input || !plugs.output ||
        static_cast<void*>(plugs.input.get()) != cable->inputPlug ||
        static_cast<void*>(plugs.output.get()) != cable->outputPlug) {
      plugs = {};
      for (auto* container : homeRack->children)
        for (auto* child : container->children) {
          if (static_cast<void*>(child) == cable->inputPlug)
            plugs.input = child;
          if (static_cast<void*>(child) == cable->outputPlug)
            plugs.output = child;
        }
    }
    if (plugs.input) plugs.input->visible = visible;
    if (plugs.output) plugs.output->visible = visible;
  }
  void revealMembers(bool together) {
    if (members.empty()) return;
    closeView();
    auto* rack = homeRack;
    for (const auto& p : ports)
      if (auto* source = findPort(p)) source->box.pos = p.originalPosition;
    std::vector<Member*> ordered;
    for (auto& member : members)
      if (find(member.id)) ordered.push_back(&member);
    std::sort(ordered.begin(), ordered.end(), [](Member* a, Member* b) {
      return a->position.y == b->position.y ? a->position.x < b->position.x
                                            : a->position.y < b->position.y;
    });
    float width = 0;
    for (auto* member : ordered) width += member->widget->box.size.x;
    Vec cursor(std::ceil(box.getRight() / RACK_GRID_WIDTH) * RACK_GRID_WIDTH,
               box.pos.y);
    if (together) {
      bool blocked;
      do {
        blocked = false;
        Rect candidate(cursor, Vec(width, RACK_GRID_HEIGHT));
        for (auto* w : rack->getModules())
          if (w->visible && !contains(w->module) &&
              candidate.intersects(w->box)) {
            cursor.x = std::ceil(w->box.getRight() / RACK_GRID_WIDTH) *
                       RACK_GRID_WIDTH;
            blocked = true;
            break;
          }
      } while (blocked);
    }
    rack->deselectAll();
    for (auto* member : ordered) {
      auto* w = member->widget.get();
      auto* registered =
          member->holder ? static_cast<app::ModuleWidget*>(member->holder.get())
                         : w;
      w->visible = true;
      registered->visible = true;
      registered->box.size = w->box.size;
      registered->box.pos = together ? cursor : member->position;
      if (together) cursor.x += w->box.size.x;
      member->position = registered->box.pos;
      rack->select(registered, true);
    }
    expanded = true;
    controls->visible = false;
    for (auto* cable : rack->getCompleteCables())
      if (contains(cable->cable->inputModule) &&
          contains(cable->cable->outputModule)) {
        cable->visible = true;
        showPlugs(cable, true);
      }
    if (!ordered.empty()) {
      auto* w = rack->getModule(ordered.front()->id);
      rack->requestModulePos(w, w->box.pos);
    }
    status = "Modules expanded — abstraction retained";
    saveGroup();
  }
  void expand() {
    if (!expanded) revealMembers(true);
  }
  void endAbstraction() {
    closeView();
    if (!APP || !APP->scene) return;
    if (!expanded && !members.empty()) revealMembers(true);
    ports.clear();
    members.clear();
    randomLocks.clear();
    subpatchPositions.clear();
    panelCableIds.clear();
    knownCableIds.clear();
    internalCableIds.clear();
    plugWidgets.clear();
    controls->clearChildren();
    parameters.clear();
    expanded = false;
    controls->visible = true;
    layoutControls();
    saveGroup();
    status = "Abstraction ended — modules remain in rack";
  }
  void expose(app::ModuleWidget* w, app::PortWidget* source) {
    for (const auto& p : ports)
      if (p.id == w->module->id &&
          p.input == (source->type == engine::Port::INPUT) &&
          p.portId == source->portId)
        return;
    auto* face = new PortFace;
    face->source = source;
    face->input = source->type == engine::Port::INPUT;
    face->patched = [this](app::CableWidget* cable) {
      panelCableIds.insert(cable->cable->id);
      knownCableIds.insert(cable->cable->id);
      cable->visible = true;
      showPlugs(cable, true);
      saveGroup();
    };
    face->box.size = face->artwork()->box.size;
    int64_t id = w->module->id;
    face->caption = [this, face, id] {
      auto* source = face->source.get();
      auto* info = source ? source->getPortInfo() : nullptr;
      return memberName(id) + "\n" + (info ? info->getName() : "Port");
    };
    controls->addChild(face);
    ports.push_back({w->module->id, source->portId,
                     source->type == engine::Port::INPUT, source->box.pos,
                     face});
    layoutControls();
    saveGroup();
  }
  bool boundaryPort(app::PortWidget* port) const {
    ScopedSubviewRack mainScope(homeRack);
    for (auto* cable : homeRack->getCompleteCables())
      if ((cable->inputPort == port || cable->outputPort == port) &&
          contains(cable->cable->inputModule) !=
              contains(cable->cable->outputModule))
        return true;
    return false;
  }
  bool exposed(app::PortWidget* port) const {
    if (!port || !port->module) return false;
    for (const auto& p : ports)
      if (p.id == port->module->id && p.portId == port->portId &&
          p.input == (port->type == engine::Port::INPUT))
        return true;
    return false;
  }
  bool exposed(app::ParamWidget* param) const {
    if (!param || !param->module) return false;
    for (const auto& p : parameters)
      if (p.id == param->module->id && p.paramId == param->paramId) return true;
    return false;
  }
  void togglePort(app::PortWidget* source) {
    if (!source || !source->module || boundaryPort(source)) return;
    auto* action = beginAction("change exposed port");
    for (auto it = ports.begin(); it != ports.end(); ++it)
      if (findPort(*it) == source) {
        source->box.pos = it->originalPosition;
        controls->removeChild(it->face);
        delete it->face;
        ports.erase(it);
        layoutControls();
        saveGroup();
        finishAction(action);
        return;
      }
    expose(find(source->module->id), source);
    finishAction(action);
  }
  void toggleParameter(app::ParamWidget* source) {
    if (!source || !source->module) return;
    auto* action = beginAction("change exposed parameter");
    for (auto it = parameters.begin(); it != parameters.end(); ++it)
      if (it->id == source->module->id && it->paramId == source->paramId) {
        controls->removeChild(it->knob);
        delete it->knob;
        parameters.erase(it);
        layoutControls();
        saveGroup();
        finishAction(action);
        return;
      }
    exposeParameter(source->module->id, source->paramId);
    finishAction(action);
  }
  void exposePortAction(int64_t id, int index, bool input) {
    auto* w = find(id);
    auto* port =
        w ? (input ? w->getInput(index) : w->getOutput(index)) : nullptr;
    if (port && !exposed(port)) togglePort(port);
  }
  void exposeParameterAction(int64_t id, int index) {
    auto* w = find(id);
    if (!w || index < 0 || index >= int(w->module->params.size()) ||
        !w->module->getParamQuantity(index))
      return;
    for (const auto& param : parameters)
      if (param.id == id && param.paramId == index) return;
    auto* action = beginAction("expose parameter");
    exposeParameter(id, index);
    finishAction(action);
  }
  bool syncMemberPositions() {
    bool moved = false;
    for (auto& member : members)
      if (auto* w = homeRack->getModule(member.id))
        if (w->box.pos != member.position) {
          member.position = w->box.pos;
          moved = true;
        }
    return moved;
  }
  GroupHistory* beginAction(const std::string& name) {
    if (expanded && syncMemberPositions()) saveGroup();
    auto* action = new GroupHistory;
    action->name = name;
    action->moduleId = module->id;
    action->before.capture();
    action->oldModuleJ = toJson();
    return action;
  }
  void finishAction(GroupHistory* action) {
    action->newModuleJ = toJson();
    action->after.capture();
    APP->history->push(action);
  }
  void groupAction(bool collapsing) {
    if (!module || (!collapsing && (members.empty() || expanded)) ||
        (collapsing && !members.empty() && !expanded))
      return;
    auto* action =
        beginAction(collapsing ? (members.empty() ? "abstract modules"
                                                  : "collapse abstraction")
                               : "expand abstraction");
    if (collapsing)
      collapse();
    else
      expand();
    if (collapsing && members.empty()) {
      delete action;
      return;
    }
    finishAction(action);
  }
  void endAction() {
    if (members.empty()) return;
    auto* action = beginAction("end abstraction");
    endAbstraction();
    finishAction(action);
  }
  bool ownedElsewhere(engine::Module* m) const {
    for (auto* w : homeRack->getModules())
      if (auto* group = dynamic_cast<Beekapoo*>(w))
        if (group != this && group->contains(m)) return true;
    return false;
  }
  void syncBoundaryPorts() {
    for (auto* cable : homeRack->getCompleteCables()) {
      bool in = contains(cable->cable->inputModule),
           out = contains(cable->cable->outputModule);
      if (in == out) continue;
      auto* port = in ? cable->inputPort : cable->outputPort;
      if (!exposed(port)) expose(find(port->module->id), port);
    }
  }
  bool canAddSelection() const {
    if (!module) return false;
    for (auto* w : homeRack->getSelected())
      if (w != this && w->visible && !contains(w->module) &&
          !ownedElsewhere(w->module) && !dynamic_cast<Beekapoo*>(w) &&
          !w->module->leftExpander.producerMessage &&
          !w->module->rightExpander.producerMessage)
        return true;
    return false;
  }
  void addSelection() {
    if (!module || members.empty()) return;
    std::vector<app::ModuleWidget*> added;
    for (auto* w : homeRack->getSelected())
      if (w != this && w->visible && !contains(w->module) &&
          !ownedElsewhere(w->module) && !dynamic_cast<Beekapoo*>(w) &&
          !w->module->leftExpander.producerMessage &&
          !w->module->rightExpander.producerMessage)
        added.push_back(w);
    if (added.empty()) return;
    auto* action = beginAction("add selected modules to abstraction");
    closeView();
    bool wasExpanded = expanded;
    for (auto* registered : added) {
      auto* holder = dynamic_cast<Holder*>(registered);
      auto* w = holder ? holder->original : registered;
      if (!w || !w->module) continue;
      members.push_back({w->module->id, registered->box.pos, w, holder});
    }
    syncBoundaryPorts();
    pack();
    if (wasExpanded) revealMembers(true);
    saveGroup();
    finishAction(action);
  }
  void removeSelection() {
    if (!expanded || members.empty()) return;
    auto* rack = homeRack;
    std::set<int64_t> removed;
    for (auto* w : rack->getSelected())
      if (contains(w->module)) removed.insert(w->module->id);
    if (removed.empty()) return;
    removeMembers(removed);
  }
  void removeMembers(const std::set<int64_t>& removed) {
    if (removed.empty()) return;
    bool wasExpanded = expanded;
    std::vector<app::ModuleWidget*> released;
    for (const auto& member : members)
      if (removed.count(member.id))
        if (auto* w = homeRack->getModule(member.id)) released.push_back(w);
    std::sort(released.begin(), released.end(),
              [](app::ModuleWidget* a, app::ModuleWidget* b) {
                return a->box.pos.y == b->box.pos.y
                           ? a->box.pos.x < b->box.pos.x
                           : a->box.pos.y < b->box.pos.y;
              });
    auto* action = beginAction("remove selected members from abstraction");
    bool editing = bool(editSession);
    if (editing)
      forgetEditedMembers(removed);
    else
      closeView();
    if (!expanded) revealMembers(true);
    for (auto id : removed) subpatchPositions.erase(id);
    for (auto it = ports.begin(); it != ports.end();)
      if (removed.count(it->id)) {
        controls->removeChild(it->face);
        delete it->face;
        it = ports.erase(it);
      } else
        ++it;
    for (auto it = parameters.begin(); it != parameters.end();)
      if (removed.count(it->id)) {
        controls->removeChild(it->knob);
        delete it->knob;
        it = parameters.erase(it);
      } else
        ++it;
    members.erase(std::remove_if(members.begin(), members.end(),
                                 [&](const Member& member) {
                                   return removed.count(member.id);
                                 }),
                  members.end());
    if (members.empty()) {
      expanded = editing;
      controls->visible = !editing;
      status = editing ? "No modules in this group"
                       : "Abstraction ended — modules remain in rack";
    } else {
      syncBoundaryPorts();
      if (!wasExpanded) pack();
    }
    layoutControls();
    if (editing) {
      // Release into the main patch near the container, outside the editing
      // area. Let Rack find free grid positions without pushing other modules.
      Vec next = box.pos + Vec(box.size.x + RACK_GRID_WIDTH, 0);
      for (auto* w : released) {
        homeRack->setModulePosNearest(w, next);
        next = Vec(w->box.getRight() + RACK_GRID_WIDTH, w->box.pos.y);
      }
    }
    saveGroup();
    finishAction(action);
  }
  void parameterAction(int index, bool recordHistory) {
    if (!module || members.empty() || index < 0 || index >= 6) return;
    const int operation = index % 3;
    const bool all = index >= 3;
    auto* batch = recordHistory ? new history::ComplexAction : nullptr;
    if (batch)
      batch->name = std::string(operation == 0   ? "wiggle "
                                : operation == 1 ? "randomize "
                                                 : "initialize ") +
                    (all ? "all subpatch modules" : "exposed parameters");
    actionPulse[index] = .12f;
    auto change = [&](engine::ParamQuantity* q, bool momentary) {
      if (!q || !q->isBounded()) return;
      if (operation != 2 && randomLocked(q->module->id, q->paramId)) return;
      if (operation == 2 ? !q->resetEnabled
                         : momentary || (all && !q->randomizeEnabled))
        return;
      float before = q->getValue();
      if (operation == 2)
        q->reset();
      else if (operation == 1)
        // Exposed controls use Rack's bounded distribution directly, rather
        // than a module/quantity's custom randomize policy.
        q->engine::ParamQuantity::randomize();
      else {
        float amount = module->params[Module::WIGGLE_AMOUNT].getValue();
        q->setImmediateValue(q->fromScaled(clamp(
            q->getScaledValue() + (random::uniform() * 2.f - 1.f) * amount, 0.f,
            1.f)));
      }
      if (batch && before != q->getValue()) {
        auto* action = new history::ParamChange;
        action->moduleId = q->module->id;
        action->paramId = q->paramId;
        action->oldValue = before;
        action->newValue = q->getValue();
        batch->push(action);
      }
    };
    if (all && operation != 0) {
      for (const auto& member : members)
        if (auto* w = find(member.id)) {
          if (operation == 1 && randomLocked(member.id, -1)) continue;
          auto* before = batch ? APP->engine->moduleToJson(w->module) : nullptr;
          if (operation == 1) {
            std::set<int> locked;
            for (const auto& lock : randomLocks)
              if (lock.first == member.id && lock.second >= 0)
                locked.insert(lock.second);
            if (locked.empty())
              APP->engine->randomizeModule(w->module);
            else
              randomizeWithLocks(w->module, locked);
          } else
            APP->engine->resetModule(w->module);
          if (batch) {
            auto* action = new history::ModuleChange;
            action->moduleId = member.id;
            action->oldModuleJ = before;
            action->newModuleJ = APP->engine->moduleToJson(w->module);
            batch->push(action);
          }
        }
    } else if (all) {
      for (const auto& member : members)
        if (auto* w = find(member.id))
          for (auto* q : w->module->paramQuantities) {
            auto* source =
                q ? dynamic_cast<app::Switch*>(w->getParam(q->paramId))
                  : nullptr;
            change(q, source && source->momentary);
          }
    } else {
      for (const auto& param : parameters)
        if (controlVisible(param.id, 0))
          if (auto* w = find(param.id))
            change(w->module->getParamQuantity(param.paramId),
                   dynamic_cast<app::Switch*>(param.knob) &&
                       static_cast<app::Switch*>(param.knob)->momentary);
    }
    if (batch) {
      if (batch->isEmpty())
        delete batch;
      else
        APP->history->push(batch);
    }
  }
  void onHoverKey(const HoverKeyEvent& e) override {
    if (e.action == GLFW_PRESS && e.key == GLFW_KEY_U &&
        (e.mods & RACK_MOD_MASK) == RACK_MOD_CTRL) {
      if (homeRack->isSelected(this))
        disconnectBeekapooSelection();
      else
        disconnectPanelAction();
      e.consume(this);
      return;
    }
    if (e.action == GLFW_PRESS && !(e.mods & RACK_MOD_MASK) &&
        (e.key == GLFW_KEY_DELETE || e.key == GLFW_KEY_BACKSPACE)) {
      deleteGroupAction();
      e.consume(nullptr);
      return;
    }
    if (e.action == GLFW_PRESS && e.key == GLFW_KEY_D &&
        (e.mods & RACK_MOD_CTRL)) {
      if (homeRack->isSelected(this)) {
        std::vector<Module*> groups;
        for (auto* selected : homeRack->getSelected())
          if (auto* group = dynamic_cast<Beekapoo*>(selected)) {
            group->prepareContainedSave();
            auto* state = group->getModule<Module>();
            state->copyBoundary = e.mods & GLFW_MOD_SHIFT;
            groups.push_back(state);
          }
        homeRack->cloneSelectionAction(e.mods & GLFW_MOD_SHIFT);
        for (auto* state : groups) state->copyBoundary = false;
      } else
        duplicateAction(e.mods & GLFW_MOD_SHIFT);
      e.consume(this);
      return;
    }
    if ((e.action == GLFW_PRESS || e.action == GLFW_REPEAT) &&
        e.key == GLFW_KEY_R && (e.mods & RACK_MOD_MASK) == RACK_MOD_CTRL) {
      parameterAction(1, true);
      e.consume(this);
      return;
    }
    ModuleWidget::onHoverKey(e);
  }
  void collapse() {
    if (!members.empty()) {
      if (!expanded) return;
      closeView();
      for (auto& member : members)
        if (auto* w = homeRack->getModule(member.id))
          member.position = w->box.pos;
      for (auto& p : ports)
        if (auto* source = findPort(p)) p.originalPosition = source->box.pos;
      syncBoundaryPorts();
      pack();
      saveGroup();
      step();
      return;
    }
    auto* rack = homeRack;
    std::vector<app::ModuleWidget*> selected;
    for (auto* w : rack->getSelected()) {
      if (w == this || !w->module || !w->visible) continue;
      if (dynamic_cast<Beekapoo*>(w) || ownedElsewhere(w->module) ||
          w->module->leftExpander.producerMessage ||
          w->module->rightExpander.producerMessage) {
        status = "Expanders and nested groups are not supported";
        return;
      }
      selected.push_back(w);
    }
    if (selected.empty()) {
      status = "Select some other modules first";
      return;
    }
    for (auto* registered : selected) {
      auto* holder = dynamic_cast<Holder*>(registered);
      auto* w = holder ? holder->original : registered;
      if (!w || !w->module) continue;
      members.push_back({w->module->id, registered->box.pos, w, holder});
    }
    std::vector<std::pair<app::ModuleWidget*, app::PortWidget*>> boundary;
    for (auto* cable : rack->getCompleteCables()) {
      bool in = contains(cable->cable->inputModule);
      bool out = contains(cable->cable->outputModule);
      if (in == out) continue;
      auto* source = in ? cable->inputPort : cable->outputPort;
      auto* w = rack->getModule(source->module->id);
      bool duplicate = false;
      for (const auto& item : boundary)
        if (item.second == source) duplicate = true;
      if (!duplicate) boundary.push_back({w, source});
    }
    for (auto item : boundary) expose(item.first, item.second);
    pack();
    saveGroup();
    status =
        std::to_string(members.size()) + " modules running behind this panel";
    step();
  }
  void syncBypass() {
    auto* m = getModule<Module>();
    if (!m) return;
    bool bypassed = m->isBypassed();
    bool requested =
        m->pendingBypass.exchange(-1, std::memory_order_acq_rel) >= 0;
    // Also covers saved bypass state and new members added while bypassed.
    // Normal un-bypassed playback does not scan or change child bypass states.
    if (bypassed || requested || lastBypass != bypassed)
      for (const auto& member : members)
        if (auto* child = APP->engine->getModule(member.id))
          APP->engine->bypassModule(child, bypassed);
    lastBypass = bypassed;
  }
  void step() override {
    if (addSelectionButton->visible != canAddSelection()) layoutControls();
    installDevWatcherToolbar();
    returnHighlight = std::max(
        0.f, returnHighlight - float(APP->window->getLastFrameDuration()));
    auto* sizing = getModule<Module>();
    if (sizing && sizing->panelWidth.load() != requestedWidth) {
      requestedWidth = sizing->panelWidth.load();
      layoutControls();
    }
    if (box.size.x != lastLayoutWidth) {
      requestedWidth = box.size.x;
      layoutControls();
    }
    for (auto& pulse : actionPulse)
      pulse = std::max(0.f, pulse - float(APP->window->getLastFrameDuration()));
    if (auto* m = getModule<Module>())
      if (auto* saved = m->takeRestore()) {
        restore(saved);
        json_decref(saved);
      }
    if (auto* m = getModule<Module>()) {
      unsigned actions =
          m->pendingActions.exchange(0, std::memory_order_relaxed);
      unsigned histories =
          m->pendingHistory.exchange(0, std::memory_order_relaxed);
      for (int i = 0; i < 6; ++i)
        if (actions & (1u << i))
          parameterAction(i, (histories & (1u << i)) != 0);
    }
    // A native Undo can replace a member widget before pruning runs. Rebuild
    // mappings against that restored widget instead of using its old quantity.
    bool replaced = false;
    for (auto& member : members)
      if (!member.widget && homeRack->getModule(member.id)) replaced = true;
    if (replaced) {
      auto* saved = getModule<Module>()->dataToJson();
      if (auto* group = json_object_get(saved, "group")) restore(group);
      json_decref(saved);
    }
    syncBypass();
    if (members.empty()) {
      ModuleWidget::step();
      return;
    }
    // Prune deleted members before stepping their mapped controls. The native
    // deletion transaction also restores this membership on Undo.
    for (const auto& member : members)
      if (!find(member.id)) {
        pruneMissingMembers();
        break;
      }
    if (expanded) {
      syncBoundaryPorts();
      for (auto& p : ports)
        if (auto* source = findPort(p)) p.originalPosition = source->box.pos;
      if (syncMemberPositions()) saveGroup();
      ModuleWidget::step();
      return;
    }
    const Vec parking = parkingPosition();
    for (const auto& member : members)
      if (member.holder) {
        member.holder->box.pos = parking;
        homeRack->select(member.holder.get(), false);
      }
    for (const auto& p : ports) {
      p.face->source = findPort(p);
      if (auto* source = findPort(p)) {
        Vec desired =
            controls->getRelativeOffset(p.face->box.getCenter(), homeRack);
        Vec parentOrigin = source->parent->getRelativeOffset(Vec(), homeRack);
        source->box.pos = desired - source->box.size * .5f - parentOrigin;
      }
    }
    std::set<int64_t> currentCables;
    bool presentationChanged = false;
    for (auto* cable : homeRack->getCompleteCables()) {
      auto id = cable->cable->id;
      currentCables.insert(id);
      bool input = contains(cable->cable->inputModule),
           output = contains(cable->cable->outputModule);
      if ((input || output) && !knownCableIds.count(id) &&
          !internalCableIds.count(id)) {
        presentationChanged |= panelCableIds.insert(id).second;
        cable->visible = true;
        showPlugs(cable, true);
      }
      if (input != output) {
        cable->visible = true;
        showPlugs(cable, true);
      }
      if (input && output) {
        bool visible = panelCableIds.count(id);
        cable->visible = visible;
        showPlugs(cable, visible);
      }
    }
    knownCableIds = std::move(currentCables);
    for (auto it = plugWidgets.begin(); it != plugWidgets.end();)
      if (!knownCableIds.count(it->first))
        it = plugWidgets.erase(it);
      else
        ++it;
    for (auto it = panelCableIds.begin(); it != panelCableIds.end();)
      if (!knownCableIds.count(*it)) {
        it = panelCableIds.erase(it);
        presentationChanged = true;
      } else
        ++it;
    if (presentationChanged) saveGroup();
    ModuleWidget::step();
  }
  void draw(const DrawArgs& args) override {
    ModuleWidget::draw(args);
    auto font =
        APP->window->loadFont(asset::system("res/fonts/DejaVuSans.ttf"));
    if (titleFrame && titleFrame->visible) {
      Rect area = frameContent(titleFrame);
      bool logoOnly = box.size.x <= 90;
      float logoWidth =
          logoOnly ? area.size.x : std::min(24.f, area.size.x * .5f);
      Rect logoArea(Vec(logoOnly ? area.getCenter().x - logoWidth * .5f
                                 : area.getRight() - logoWidth,
                        area.pos.y),
                    Vec(logoWidth, area.size.y));
      auto drawHeaderArt = [&](widget::SvgWidget& svg, Rect target) {
        if (!svg.svg || target.size.x <= 0 || target.size.y <= 0) return;
        Rect ink = &svg == &titleArtwork ? titleInk : Rect(Vec(), svg.box.size);
        float scale =
            std::min(target.size.x / ink.size.x, target.size.y / ink.size.y);
        nvgSave(args.vg);
        nvgTranslate(args.vg, target.getCenter().x - ink.size.x * scale * .5f,
                     target.getCenter().y - ink.size.y * scale * .5f);
        nvgScale(args.vg, scale, scale);
        nvgTranslate(args.vg, -ink.pos.x, -ink.pos.y);
        svg.draw(args);
        nvgRestore(args.vg);
      };
      if (!logoOnly)
        drawHeaderArt(
            titleArtwork,
            Rect(area.pos, Vec(area.size.x - logoWidth - 3, area.size.y)));
      drawHeaderArt(logoArtwork, logoArea);
    }
    if (!font) return;
    nvgFontFaceId(args.vg, font->handle);
    nvgFontSize(args.vg, 8);
    nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);
    for (const auto& h : headings) {
      nvgFillColor(args.vg,
                   h.output ? nvgRGB(222, 234, 227) : nvgRGB(40, 61, 50));
      nvgText(args.vg, h.pos.x, h.pos.y, h.text.c_str(), nullptr);
    }
    nvgFontSize(args.vg, layoutTier >= 2 ? 9 : 7);
    for (const auto& label : controlLabels) {
      nvgFillColor(args.vg,
                   label.output ? nvgRGB(230, 238, 232) : nvgRGB(27, 45, 36));
      nvgTextAlign(args.vg, label.align | NVG_ALIGN_BASELINE);
      float x = label.pos.x -
                (label.align == NVG_ALIGN_CENTER ? label.width * .5f : 0);
      if (layoutTier == 1 || jacksOnly) {
        std::string text = label.text;
        auto trim = [](std::string& value) {
          if (value.empty()) return;
          size_t i = value.size() - 1;
          while (i > 0 && (static_cast<unsigned char>(value[i]) & 0xc0) == 0x80)
            --i;
          value.resize(i);
        };
        bool shortened = false;
        while (!text.empty() &&
               nvgTextBounds(args.vg, 0, 0,
                             (text + (shortened ? "…" : "")).c_str(), nullptr,
                             nullptr) > label.width) {
          trim(text);
          shortened = true;
        }
        if (shortened) text += "…";
        nvgTextAlign(args.vg, label.align | (label.align == NVG_ALIGN_CENTER
                                                 ? NVG_ALIGN_BASELINE
                                                 : NVG_ALIGN_MIDDLE));
        nvgText(args.vg, label.pos.x, label.pos.y, text.c_str(), nullptr);
      } else {
        float bounds[4];
        nvgTextBoxBounds(args.vg, x, 0, label.width, label.text.c_str(),
                         nullptr, bounds);
        float baseline = label.pos.y - (bounds[1] + bounds[3]) * .5f;
        nvgTextBox(args.vg, x, baseline, label.width, label.text.c_str(),
                   nullptr);
      }
    }
    if (returnHighlight > 0 && !editSession) {
      float fade = returnHighlight / .8f;
      fade = fade * fade * (3 - 2 * fade);
      nvgSave(args.vg);
      nvgBeginPath(args.vg);
      nvgMoveTo(args.vg, 2, 3);
      nvgLineTo(args.vg, box.size.x - 2, 1);
      nvgLineTo(args.vg, box.size.x - 1, box.size.y - 3);
      nvgLineTo(args.vg, 3, box.size.y - 1);
      nvgClosePath(args.vg);
      nvgFillColor(args.vg,
                   nvgTransRGBAf(COLOR_COMPUTERSCARE_GREEN, .55f * fade));
      nvgFill(args.vg);
      nvgStrokeColor(args.vg,
                     nvgTransRGBAf(COLOR_COMPUTERSCARE_GREEN, .95f * fade));
      nvgStrokeWidth(args.vg, 2);
      nvgStroke(args.vg);
      nvgRestore(args.vg);
    }
    if (module && settings::cpuMeter) drawSubpatchCpu(args, font->handle);
  }
  float subpatchCpuPercent() const {
    double seconds = 0;
    for (const auto& member : members)
      seconds += moduleCpuSeconds(APP->engine->getModule(member.id));
    return float(seconds * APP->engine->getSampleRate() * 100.);
  }
  void drawSubpatchCpu(const DrawArgs& args, int font) {
    std::string total =
        box.size.x < 135 ? string::f("Σ %.1f%%", subpatchCpuPercent())
                         : string::f("Subpatch: %.1f%%", subpatchCpuPercent());
    std::string host =
        string::f("%.1f%%", moduleCpuSeconds(module) *
                                APP->engine->getSampleRate() * 100.f);
    nvgSave(args.vg);
    // Retain Rack's native host plot. Repaint only the footer so both labels
    // share a font size, rather than squeezing the total beside larger text.
    float y = box.size.y - BND_WIDGET_HEIGHT;
    bndMenuBackground(args.vg, 0, y, box.size.x, BND_WIDGET_HEIGHT,
                      BND_CORNER_ALL);
    nvgFontFaceId(args.vg, font);
    float size = 10.f;
    nvgFontSize(args.vg, size);
    nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    float totalWidth =
        nvgTextBounds(args.vg, 0, 0, total.c_str(), nullptr, nullptr);
    float hostWidth =
        nvgTextBounds(args.vg, 0, 0, host.c_str(), nullptr, nullptr);
    float available = std::max(1.f, box.size.x - 10.f);
    if (totalWidth + hostWidth > available)
      size *= available / (totalWidth + hostWidth);
    nvgFontSize(args.vg, size);
    nvgFillColor(args.vg, componentlibrary::SCHEME_ORANGE);
    nvgText(args.vg, 3, y + BND_WIDGET_HEIGHT * .5f, total.c_str(), nullptr);
    nvgTextAlign(args.vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
    nvgFillColor(args.vg, nvgRGB(180, 180, 180));
    nvgText(args.vg, box.size.x - 3, y + BND_WIDGET_HEIGHT * .5f, host.c_str(),
            nullptr);
    nvgRestore(args.vg);
  }
  void appendContextMenu(ui::Menu* menu) override {
    if (!module) return;
    std::vector<widget::Widget*> nativeDuplicates;
    for (auto* child : menu->children)
      if (auto* item = dynamic_cast<ui::MenuItem*>(child))
        if (item->text == "Duplicate" || item->text == "Delete" ||
            item->text.find("Duplicate with cables") != std::string::npos)
          nativeDuplicates.push_back(item);
    for (auto* item : nativeDuplicates) {
      menu->removeChild(item);
      delete item;
    }
    // Preserve Rack's own translated label and shortcut. This is its unique
    // native U command; the SDK predates the translation helper API.
    for (auto* child : menu->children)
      if (auto* item = dynamic_cast<ui::MenuItem*>(child))
        if (!item->rightText.empty() && item->rightText.back() == 'U') {
          auto* replacement = createMenuItem(
              item->text, item->rightText, [this] { disconnectPanelAction(); });
          menu->addChildAbove(replacement, item);
          menu->removeChild(item);
          delete item;
          break;
        }
    menu->addChild(
        createMenuItem("Duplicate", "", [this] { duplicateAction(false); }));
    menu->addChild(createMenuItem("Duplicate with cables", "",
                                  [this] { duplicateAction(true); }));
    menu->addChild(
        createMenuItem("Delete", "", [this] { deleteGroupAction(); }));
    menu->addChild(new ui::MenuSeparator);

    menu->addChild(
        createMenuItem("Edit subpatch", "", [this] { editSubpatch(); }));
    menu->addChild(new OutsideFadeSlider);
    menu->addChild(
        createSubmenuItem("Panel arrangement", "", [this](ui::Menu* sub) {
          for (bool grouped : {false, true})
            sub->addChild(createCheckMenuItem(
                grouped ? "By module" : "By control type", "",
                [this, grouped] { return arrangeByModule == grouped; },
                [this, grouped] {
                  arrangeByModule = grouped;
                  layoutControls();
                  saveGroup();
                }));
        }));
    menu->addChild(
        createSubmenuItem("Show controls", "", [this](ui::Menu* sub) {
          const char* labels[] = {"Parameters", "Inputs", "Outputs"};
          for (int kind = 0; kind < 3; ++kind)
            sub->addChild(createCheckMenuItem(
                labels[kind], "",
                [this, kind] { return !(hiddenKinds & (1u << kind)); },
                [this, kind] {
                  hiddenKinds ^= 1u << kind;
                  layoutControls();
                  saveGroup();
                }));
          sub->addChild(new ui::MenuSeparator);
          for (auto& member : members) {
            auto id = member.id;
            sub->addChild(createSubmenuItem(
                memberName(id), "", [this, id](ui::Menu* options) {
                  options->addChild(createCheckMenuItem(
                      "All", "",
                      [this, id] { return hiddenModuleKinds[id] == 0; },
                      [this, id] {
                        hiddenModuleKinds[id] = hiddenModuleKinds[id] ? 0 : 7;
                        layoutControls();
                        saveGroup();
                      }));
                  const char* kinds[] = {"Parameters", "Inputs", "Outputs"};
                  for (int kind = 0; kind < 3; ++kind)
                    options->addChild(createCheckMenuItem(
                        kinds[kind], "",
                        [this, id, kind] {
                          return !(hiddenModuleKinds[id] & (1u << kind));
                        },
                        [this, id, kind] {
                          hiddenModuleKinds[id] ^= 1u << kind;
                          layoutControls();
                          saveGroup();
                        }));
                }));
          }
        }));
    if (members.empty())
      menu->addChild(createMenuItem("Abstract selected modules", "",
                                    [this] { groupAction(true); }));
    menu->addChild(
        createMenuItem("Eject subpatch", "", [this] { ejectAction(); }));
    menu->addChild(createMenuItem(
        "Add selected modules", "", [this] { addSelection(); },
        members.empty()));
    menu->addChild(createMenuItem(
        "Remove selected members", "", [this] { removeSelection(); },
        !expanded));
    menu->addChild(createSubmenuItem("Expose port", "", [this](ui::Menu* sub) {
      for (const auto& member : members)
        if (auto* w = find(member.id)) {
          sub->addChild(
              createSubmenuItem(w->model->name, "", [this, w](ui::Menu* list) {
                for (auto* port : w->getPorts()) {
                  std::string label =
                      port->type == engine::Port::INPUT ? "In: " : "Out: ";
                  label += port->getPortInfo()->getName();
                  int64_t id = w->module->id;
                  int index = port->portId;
                  bool input = port->type == engine::Port::INPUT;
                  list->addChild(
                      createMenuItem(label, "", [this, id, index, input] {
                        exposePortAction(id, index, input);
                      }));
                }
              }));
        }
    }));
    menu->addChild(
        createSubmenuItem("Expose parameter", "", [this](ui::Menu* sub) {
          for (const auto& member : members)
            if (auto* w = find(member.id)) {
              sub->addChild(createSubmenuItem(
                  w->model->name, "", [this, w](ui::Menu* list) {
                    for (int i = 0; i < int(w->module->params.size()); ++i) {
                      auto* q = w->module->getParamQuantity(i);
                      if (!q) continue;
                      int64_t id = w->module->id;
                      list->addChild(createMenuItem(
                          q->getLabel(), "",
                          [this, id, i] { exposeParameterAction(id, i); }));
                    }
                  }));
            }
        }));
  }
};
// Closest collision-free placement of a complete rectangular subpatch. Sweep
// obstacle row boundaries, merging forbidden grid columns in each row. This
// runs only on Eject and never moves unrelated modules.
Vec closestEjectPosition(Beekapoo* group, Vec size) {
  std::vector<Rect> obstacles;
  for (auto* outside : group->homeRack->getModules())
    if (outside != group && outside->visible &&
        !group->contains(outside->module))
      obstacles.push_back(outside->box);
  bool fits = true;
  for (auto& obstacle : obstacles)
    if (Rect(group->box.pos, size).intersects(obstacle)) fits = false;
  if (fits) return group->box.pos;
  Vec desired = group->box.getCenter() - size * .5f;
  std::set<int> rows;
  rows.insert(int(std::floor(desired.y / RACK_GRID_HEIGHT)));
  rows.insert(int(std::ceil(desired.y / RACK_GRID_HEIGHT)));
  rows.insert(int(std::round(group->box.pos.y / RACK_GRID_HEIGHT)));
  for (auto& obstacle : obstacles) {
    rows.insert(int(std::floor((obstacle.pos.y - size.y) / RACK_GRID_HEIGHT)));
    rows.insert(int(std::ceil(obstacle.getBottom() / RACK_GRID_HEIGHT)));
  }
  Vec best = group->box.pos;
  double bestDistance = INFINITY;
  for (int row : rows) {
    float y = row * RACK_GRID_HEIGHT;
    std::vector<std::pair<int, int>> blocked, merged;
    for (auto& obstacle : obstacles)
      if (y < obstacle.getBottom() && y + size.y > obstacle.pos.y) {
        int left =
            int(std::floor((obstacle.pos.x - size.x) / RACK_GRID_WIDTH)) + 1;
        int right = int(std::ceil(obstacle.getRight() / RACK_GRID_WIDTH)) - 1;
        if (left <= right) blocked.emplace_back(left, right);
      }
    std::sort(blocked.begin(), blocked.end());
    for (auto interval : blocked) {
      if (!merged.empty() && interval.first <= merged.back().second + 1)
        merged.back().second = std::max(merged.back().second, interval.second);
      else
        merged.push_back(interval);
    }
    for (int target : {int(std::floor(desired.x / RACK_GRID_WIDTH)),
                       int(std::ceil(desired.x / RACK_GRID_WIDTH))}) {
      int left = target, right = target;
      for (auto interval : merged)
        if (target >= interval.first && target <= interval.second) {
          left = interval.first - 1;
          right = interval.second + 1;
          break;
        }
      for (int column : {left, right}) {
        Vec candidate(column * RACK_GRID_WIDTH, y);
        Vec distance = candidate - desired;
        double score =
            double(distance.x) * distance.x + double(distance.y) * distance.y;
        if (score < bestDistance) {
          bestDistance = score;
          best = candidate;
        }
      }
    }
  }
  return best;
}
void Beekapoo::ejectAction() {
  if (!module) return;
  auto* change = beginAction("eject subpatch");
  std::vector<int64_t> released;
  for (auto& member : members) released.push_back(member.id);
  Vec minimum(INFINITY, INFINITY), maximum(-INFINITY, -INFINITY);
  for (auto& member : members)
    if (auto* w = member.widget.get()) {
      minimum.x = std::min(minimum.x, member.position.x);
      minimum.y = std::min(minimum.y, member.position.y);
      maximum.x = std::max(maximum.x, member.position.x + w->box.size.x);
      maximum.y = std::max(maximum.y, member.position.y + w->box.size.y);
    }
  ejecting = true;
  if (minimum.isFinite()) {
    Vec origin = closestEjectPosition(this, maximum - minimum);
    for (auto& member : members) member.position += origin - minimum;
    revealMembers(false);
  }
  endAbstraction();
  homeRack->deselectAll();
  for (auto id : released)
    if (auto* w = homeRack->getModule(id)) homeRack->select(w, true);
  change->newModuleJ = toJson();
  change->after.capture();
  auto* removal = new history::ModuleRemove;
  removal->setModule(this);
  auto* batch = new history::ComplexAction;
  batch->name = "eject subpatch";
  batch->push(change);
  for (auto* cable : homeRack->getCompleteCables())
    if (cable->cable->inputModule == module ||
        cable->cable->outputModule == module) {
      auto* action = new history::CableRemove;
      action->setCable(cable);
      batch->push(action);
      homeRack->removeCable(cable);
      delete cable;
    }
  batch->push(removal);
  APP->history->push(batch);
  homeRack->removeModule(this);
  delete this;
}
void GroupHistory::undo() {
  history::ModuleChange::undo();
  if (auto* group =
          dynamic_cast<Beekapoo*>(APP->scene->rack->getModule(moduleId)))
    group->step();
  before.restore();
  if (auto* group =
          dynamic_cast<Beekapoo*>(APP->scene->rack->getModule(moduleId)))
    group->focusEditedMembers();
}
void GroupHistory::redo() {
  history::ModuleChange::redo();
  if (auto* group =
          dynamic_cast<Beekapoo*>(APP->scene->rack->getModule(moduleId)))
    group->step();
  after.restore();
  if (auto* group =
          dynamic_cast<Beekapoo*>(APP->scene->rack->getModule(moduleId)))
    group->focusEditedMembers();
}
#include "BeekapooPortable.hpp"
#include "BeekapooRackEdit.hpp"
#include "BeekapooRackHooks.hpp"
#include "BeekapooSubview.hpp"
}  // namespace beekapoo
void installBeekapooRackHooks() { beekapoo::installRackHooks(); }
Model* modelComputerscareBeekapoo =
    createModel<beekapoo::Module, beekapoo::Beekapoo>("computerscare-beekapoo");

Model* modelComputerscareBeekapooLegacy = [] {
  auto* model = createModel<beekapoo::Module, beekapoo::Beekapoo>(
      "computerscare-patch-abstractor");
  model->hidden = true;
  return model;
}();

Model* modelComputerscarePeekabooLegacy = [] {
  auto* model = createModel<beekapoo::Module, beekapoo::Beekapoo>(
      "computerscare-peekaboo");
  model->hidden = true;
  return model;
}();
