#include <osdialog.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <patch.hpp>
#include <plugin.hpp>

#include "BeekapooCpuMeter.hpp"
#include "BeekapooExposurePlan.hpp"
#include "BeekapooKnob.hpp"
#include "BeekapooLabels.hpp"
#include "BeekapooLayout.hpp"
#include "BeekapooMemberLayout.hpp"
#include "BeekapooParamLayout.hpp"
#include "BeekapooPatchPlanner.hpp"
#include "BeekapooPortLayout.hpp"
#include "BeekapooPreferences.hpp"
#include "BeekapooRandomLocks.hpp"
#include "BeekapooSection.hpp"
#include "Blunch/BlunchRandomProgram.hpp"
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
  SectionSkin skin;
  std::array<Vec, 4> cachedCorners{};
  uint64_t cachedSkin = ~uint64_t(0);
  unsigned cachedProfile = ~0u;
  std::vector<std::vector<Vec>> polygons;
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
    std::array<Vec, 4> corners;
    bool changed = cachedSkin != skin.seed || cachedProfile != skin.profile;
    for (int i = 0; i < 4; ++i) {
      corners[i] =
          face[i] + Vec(jitter(i * 2), jitter(i * 2 + 1)) + cornerOffsets[i];
      changed |= !corners[i].equals(cachedCorners[i]);
    }
    if (changed) {
      polygons = sectionPolygons(corners, skin);
      cachedCorners = corners;
      cachedSkin = skin.seed;
      cachedProfile = skin.profile;
    }
    drawSection(args.vg, polygons, faceColor, d, skin.rounding);
    nvgRestore(args.vg);
  }
};
struct SubpatchNativeRack;
void installRackHooks();
void disconnectBeekapooSelection();
void duplicateBeekapooSelection(bool cables);
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
  std::atomic<float> panelWidth{90.f};
  std::atomic<int> panelTheme{0};
  std::mutex stateMutex;
  std::string patchName;
  bool customPatchName = false;
  bool freshDefault = true;
  std::string getPatchName() {
    std::lock_guard<std::mutex> lock(stateMutex);
    return patchName;
  }
  bool hasCustomPatchName() {
    std::lock_guard<std::mutex> lock(stateMutex);
    return customPatchName;
  }
  std::string displayPatchName(uint64_t hash) {
    std::lock_guard<std::mutex> lock(stateMutex);
    return customPatchName ? patchName : titleCase(patchName, hash);
  }
  void setPatchName(const std::string& name, bool custom = false) {
    std::lock_guard<std::mutex> lock(stateMutex);
    if (customPatchName && !custom) return;
    patchName = name;
    customPatchName = custom;
  }
  void restorePatchName(const std::string& name, bool custom) {
    std::lock_guard<std::mutex> lock(stateMutex);
    patchName = name;
    customPatchName = custom;
  }
  json_t* group = nullptr;
  json_t* contents = nullptr;
  std::function<json_t*(bool)> captureContents;
  bool copyBoundary = false;
  json_t* copyContents = nullptr;  // borrowed only during a native copy command
  bool restorePending = false;
  static constexpr int routeCapacity = 512;
  enum {
    WIGGLE_AMOUNT,
    SHOW_CABLE_OPACITY,
    INPUT_GAIN_BASE,
    PARAM_COUNT = INPUT_GAIN_BASE + routeCapacity
  };
  // Already saved by users: never insert actions before these route IDs.
  static constexpr int inputBase = 6;
  static constexpr int GENERATE_INPUT = inputBase + routeCapacity;
  static constexpr int GROW_INPUT = GENERATE_INPUT + 1;
  std::atomic<uint64_t> activeInputs[routeCapacity / 64]{};
  void enableInput(int route, bool enabled) {
    uint64_t bit = uint64_t(1) << (route % 64);
    if (enabled)
      activeInputs[route / 64].fetch_or(bit, std::memory_order_release);
    else
      activeInputs[route / 64].fetch_and(~bit, std::memory_order_release);
  }
  dsp::SchmittTrigger actionTriggers[8];
  std::atomic<unsigned> pendingActions{0};
  std::atomic<unsigned> pendingHistory{0};
  std::atomic<int> pendingBypass{-1};
  Module() {
    config(PARAM_COUNT, GROW_INPUT + 1, routeCapacity, 0);
    for (int i = 0; i < routeCapacity; ++i) {
      configParam(INPUT_GAIN_BASE + i, -1.f, 1.f, 1.f, "Input attenuverter",
                  "%", 0, 100);
      getParamQuantity(INPUT_GAIN_BASE + i)->randomizeEnabled = false;
      configInput(inputBase + i, "Exposed input");
      configOutput(i, "Exposed input connection");
      configBypass(inputBase + i, i);
    }
    configParam(WIGGLE_AMOUNT, 0.f, 1.f, .1f, "Wiggle amount", "%", 0.f, 100.f);
    getParamQuantity(WIGGLE_AMOUNT)->randomizeEnabled = false;
    configParam(SHOW_CABLE_OPACITY, 0.f, 1.f, 1.f, "Cable opacity", "%", 0.f,
                100.f);
    getParamQuantity(SHOW_CABLE_OPACITY)->randomizeEnabled = false;
    getParamQuantity(SHOW_CABLE_OPACITY)->resetEnabled = false;
    const char* names[] = {"Wiggle exposed",     "Randomize exposed",
                           "Initialize exposed", "Wiggle all",
                           "Randomize all",      "Initialize all"};
    for (int i = 0; i < 6; ++i) configInput(i, names[i]);
    configInput(GENERATE_INPUT, "Generate new subpatch");
    configInput(GROW_INPUT, "Grow subpatch");
  }
  void process(const ProcessArgs&) override {
    for (int block = 0; block < routeCapacity / 64; ++block) {
      uint64_t bits = activeInputs[block].load(std::memory_order_acquire);
      while (bits) {
        int bit = __builtin_ctzll(bits), route = block * 64 + bit;
        bits &= bits - 1;
        auto& input = inputs[inputBase + route];
        auto& output = outputs[route];
        int channels = input.getChannels();
        if (!channels) {
          // Rack's public setChannels(0) forces one channel. Native cable
          // propagation accepts zero, which preserves a child's unpatched
          // input normalization (including its default VCA gain).
          output.channels = 0;
          for (int c = 0; c < 16; ++c) output.setVoltage(0, c);
          continue;
        }
        // setChannels() ignores a port whose previous channel count is zero.
        // This active route has a real bridge cable, so reconnect it first.
        if (!output.channels) output.channels = 1;
        output.setChannels(channels);
        float gain = params[INPUT_GAIN_BASE + route].getValue();
        for (int c = 0; c < channels; c += 4)
          output.setVoltageSimd(input.getVoltageSimd<simd::float_4>(c) * gain,
                                c);
      }
    }
    for (int i = 0; i < 8; ++i)
      if (actionTriggers[i].process(inputs[i < 6    ? i
                                           : i == 6 ? GENERATE_INPUT
                                                    : GROW_INPUT]
                                        .getVoltage()))
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
    json_object_set_new(root, "panelTheme", json_integer(panelTheme.load()));
    json_object_set_new(root, "name", json_string(patchName.c_str()));
    json_object_set_new(root, "customName", json_boolean(customPatchName));
    if (group) {
      // Structure is cached, but knob values belong to native parameters.
      // Knob drags/MIDI don't call saveGroup(); never export an old gain from
      // that cache into a patch, duplicate, or structural Undo snapshot.
      auto* snapshot = json_deep_copy(group);
      auto* ports = json_object_get(snapshot, "ports");
      for (size_t i = 0; i < json_array_size(ports); ++i) {
        auto* port = json_array_get(ports, i);
        auto* route = json_object_get(port, "route");
        if (!json_is_true(json_object_get(port, "input")) ||
            !json_is_integer(route))
          continue;
        int slot = json_integer_value(route);
        if (slot >= 0 && slot < routeCapacity)
          json_object_set_new(
              port, "gain",
              json_real(getParamQuantity(INPUT_GAIN_BASE + slot)->getValue()));
      }
      json_object_set_new(root, "group", snapshot);
    }
    auto* fresh = copyContents      ? json_incref(copyContents)
                  : captureContents ? captureContents(copyBoundary)
                                    : nullptr;
    if (fresh)
      json_object_set_new(root, "contents", fresh);
    else if (group && contents &&
             json_array_size(json_object_get(group, "members")) > 0)
      json_object_set(root, "contents", contents);
    return root;
  }
  void dataFromJson(json_t* root) override {
    std::lock_guard<std::mutex> lock(stateMutex);
    freshDefault = false;
    auto* theme = json_object_get(root, "panelTheme");
    panelTheme.store(json_is_integer(theme)
                         ? validPanelTheme(json_integer_value(theme))
                         : 0);
    auto* name = json_string_value(json_object_get(root, "name"));
    patchName = name ? name : "";
    auto* custom = json_object_get(root, "customName");
    // Older patches did not distinguish generated names from user edits.
    // Preserve their existing names rather than risk losing a manual title.
    customPatchName = custom ? json_is_true(custom) : !patchName.empty();
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
// Renaming should not serialize or rebuild the contained patch on undo.
struct RenameHistory : history::Action {
  int64_t owner;
  std::string before, after;
  bool wasCustom;
  RenameHistory(Module* m, const std::string& next)
      : owner(m->id),
        before(m->getPatchName()),
        after(next),
        wasCustom(m->hasCustomPatchName()) {
    name = "rename Beekapoo";
  }
  void apply(const std::string& text, bool custom) {
    if (auto* m = dynamic_cast<Module*>(APP->engine->getModule(owner)))
      m->restorePatchName(text, custom);
  }
  void undo() override { apply(before, wasCustom); }
  void redo() override { apply(after, true); }
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
// Cable/module actions can run after the group's state during Undo. Reset
// the naming baseline only once the whole topology transaction is finished.
struct TopologyHistory : history::ComplexAction {
  int64_t owner;
  explicit TopologyHistory(int64_t id) : owner(id) {}
  void resetName();
  void undo() override {
    history::ComplexAction::undo();
    resetName();
  }
  void redo() override {
    history::ComplexAction::redo();
    resetName();
  }
};
struct LetteredFace : phlooper::WarpedDropdown {
  computerscare::type::Text captionText;
  std::shared_ptr<window::Font> labelFont() override {
    return computerscare::type::font();
  }
  std::string displayLabel() override {
    return captionText.get(label(), shapeSeed);
  }
  bool fitLabel() const override { return true; }
};
struct ActionButton : LetteredFace {
  ActionButton() { textColor = nvgRGB(36, 36, 36); }
  int icon = -1;
  bool iconAndText = false;
  std::function<std::string()> help;
  WeakPtr<ui::Tooltip> tooltip;
  void onEnter(const EnterEvent&) override {
    if (!settings::tooltips || tooltip) return;
    auto* created = new computerscare::type::Tooltip;
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
        (icon == 6 || icon == 10)
            ? .75f * std::min({1.f, (box.size.x - 4) / 12.f,
                               (box.size.y - 4) / 12.f})
            : std::min(1.f, std::max(.4f, (box.size.x - 6) / 16.f));
    if (iconAndText) iconScale = .7f * std::min(1.f, (box.size.y - 4) / 16.f);
    nvgScale(args.vg, iconScale, iconScale);
    nvgStrokeColor(args.vg, textColor);
    nvgFillColor(args.vg, textColor);
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
    } else if (icon == 9) {
      nvgBeginPath(args.vg);
      nvgMoveTo(args.vg, -7, 1);
      nvgBezierTo(args.vg, -3, -2, 3, 2, 7, -1);
      nvgMoveTo(args.vg, -1, -7);
      nvgBezierTo(args.vg, 2, -3, -2, 3, 1, 7);
      nvgStroke(args.vg);
    } else if (icon == 8) {
      for (Vec offset : {Vec(-2, -2), Vec(2, 2)}) {
        nvgBeginPath(args.vg);
        nvgMoveTo(args.vg, offset.x - 4, offset.y - 4);
        nvgLineTo(args.vg, offset.x + 4, offset.y - 3);
        nvgLineTo(args.vg, offset.x + 3, offset.y + 4);
        nvgLineTo(args.vg, offset.x - 4, offset.y + 3);
        nvgClosePath(args.vg);
        nvgStroke(args.vg);
        for (Vec dot : {Vec(-1.5f, -1.5f), Vec(1.5f, 1.5f)}) {
          nvgBeginPath(args.vg);
          nvgCircle(args.vg, offset.x + dot.x, offset.y + dot.y, .75f);
          nvgFill(args.vg);
        }
      }
    } else if (icon == 6 || icon == 10) {
      nvgStrokeWidth(args.vg, 1.8f);
      nvgBeginPath(args.vg);
      nvgMoveTo(args.vg, -6, .4f);
      nvgLineTo(args.vg, 6, -.3f);
      if (icon == 6) {
        nvgMoveTo(args.vg, .3f, -6);
        nvgLineTo(args.vg, -.2f, 6);
      }
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
// Invisible hit target over the existing header artwork. Three cached SVG
// poses animate on the UI thread only while an outside selection can be added.
struct SelectionLogo : ActionButton {
  std::array<widget::SvgWidget, 2> jaws;
  bool hovered = false;
  float phase = 0;
  void setDark(bool dark) {
    static constexpr const char* paths[] = {
        "res/Beekapoo/computerscare-chomp-1.svg",
        "res/Beekapoo/computerscare-chomp-2.svg"};
    for (size_t i = 0; i < jaws.size(); ++i) {
      auto path = asset::plugin(pluginInstance, paths[i]);
      jaws[i].setSvg(dark ? darkHeaderSvg(path) : APP->window->loadSvg(path));
    }
  }
  void setAvailable(bool available) {
    if (visible == available) return;
    visible = available;
    hovered = false;
    phase = 0;
    clearTooltip();
  }
  void advance(float seconds) {
    if (visible)
      phase = std::fmod(phase + seconds * (hovered ? 5.f : 1.8f), 1.f);
  }
  int pose() const {
    static constexpr int frames[] = {0, 1, 2, 1, 0, 0};
    return visible ? frames[std::min(5, int(phase * 6))] : 0;
  }
  void step() override {
    advance(float(APP->window->getLastFrameDuration()));
    ActionButton::step();
  }
  void onEnter(const EnterEvent& e) override {
    hovered = true;
    ActionButton::onEnter(e);
  }
  void onLeave(const LeaveEvent& e) override {
    hovered = false;
    ActionButton::onLeave(e);
  }
  void draw(const DrawArgs&) override {}  // The panel draws the logo last.
};
// Keep the source widget's switch semantics, including custom callbacks.
struct ExposedSwitch : app::Switch {
  ControlStyle skin = controlStyle("Parameter", "Module");
  WeakPtr<app::ParamWidget> source;
  struct Face : LetteredFace {
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
    face->textColor = nvgRGB(36, 36, 36);
    face->faceColor = nvgRGB(140, 140, 140);
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
struct MappingTooltip : computerscare::type::Tooltip {
  std::function<std::string()> caption;
  void step() override {
    text = caption ? caption() : "";
    computerscare::type::Tooltip::step();
  }
};
struct ParamVisual {
  virtual ~ParamVisual() = default;
  virtual void setVisualSize(float size) = 0;
};
template <class Base>
struct MappedParam : Base, ParamVisual {
  Vec naturalSize;
  float visualScale = 1;
  MappedParam() : naturalSize(this->box.size) {}
  void setVisualSize(float size) override {
    visualScale = size / std::max(naturalSize.x, naturalSize.y);
    this->box.size = naturalSize * visualScale;
  }
  void draw(const typename Base::DrawArgs& args) override {
    Vec displayed = this->box.size;
    this->box.size = naturalSize;
    auto scaled = args;
    scaled.clipBox.pos = scaled.clipBox.pos / visualScale;
    scaled.clipBox.size = scaled.clipBox.size / visualScale;
    nvgSave(args.vg);
    nvgScale(args.vg, visualScale, visualScale);
    Base::draw(scaled);
    nvgRestore(args.vg);
    this->box.size = displayed;
  }
  void drawLayer(const typename Base::DrawArgs& args, int layer) override {
    Vec displayed = this->box.size;
    this->box.size = naturalSize;
    auto scaled = args;
    scaled.clipBox.pos = scaled.clipBox.pos / visualScale;
    scaled.clipBox.size = scaled.clipBox.size / visualScale;
    nvgSave(args.vg);
    nvgScale(args.vg, visualScale, visualScale);
    Base::drawLayer(scaled, layer);
    nvgRestore(args.vg);
    this->box.size = displayed;
  }
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
// Private cached copies preserve the shared jack assets used by other modules.
inline std::shared_ptr<Svg> monochromeJack(const std::string& name) {
  static std::map<std::string, std::shared_ptr<Svg>> cache;
  auto& svg = cache[name];
  if (svg) return svg;
  auto bytes = system::readFile(
      asset::plugin(pluginInstance, "res/components/" + name + ".svg"));
  svg = std::make_shared<Svg>();
  svg->loadString(std::string(bytes.begin(), bytes.end()));
  auto gray = [](unsigned color) {
    unsigned level = unsigned(std::round((color & 255) * .2126 +
                                         ((color >> 8) & 255) * .7152 +
                                         ((color >> 16) & 255) * .0722));
    return (color & 0xff000000u) | level | (level << 8) | (level << 16);
  };
  auto paint = [&](NSVGpaint& p) {
    if (p.type == NSVG_PAINT_COLOR)
      p.color = gray(p.color);
    else if (p.type == NSVG_PAINT_LINEAR_GRADIENT ||
             p.type == NSVG_PAINT_RADIAL_GRADIENT)
      for (int i = 0; i < p.gradient->nstops; ++i)
        p.gradient->stops[i].color = gray(p.gradient->stops[i].color);
  };
  for (auto* shape = svg->handle->shapes; shape; shape = shape->next) {
    paint(shape->fill);
    paint(shape->stroke);
  }
  return svg;
}
struct PortFace : widget::OpaqueWidget {
  bool compact = true, input = true;
  bool gainLaidOut = false;
  Vec gainCenter;
  app::ParamWidget* gain = nullptr;
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
    tinyDown.setSvg(monochromeJack("jack-small-pointing-down"));
    tinyUp.setSvg(monochromeJack("jack-small-pointing-up"));
    largeDown.setSvg(monochromeJack("jack-large-pointing-down"));
    largeUp.setSvg(monochromeJack("jack-large-pointing-up"));
    tinyDown.type = engine::Port::INPUT;
    tinyUp.type = engine::Port::OUTPUT;
  }
  void draw(const DrawArgs& args) override {
    nvgSave(args.vg);
    if (compact) {
      tinyDown.type = engine::Port::INPUT;
      tinyUp.type = engine::Port::OUTPUT;
      if (input)
        tinyDown.draw(args);
      else
        tinyUp.draw(args);
    } else {
      Vec size = artwork()->box.size;
      nvgScale(args.vg, box.size.x / size.x, box.size.y / size.y);
      artwork()->draw(args);
    }
    nvgRestore(args.vg);
  }
  void drawLayer(const DrawArgs& args, int layer) override {
    if (!compact) {
      nvgSave(args.vg);
      Vec size = artwork()->box.size;
      nvgScale(args.vg, box.size.x / size.x, box.size.y / size.y);
      artwork()->drawLayer(args, layer);
      nvgRestore(args.vg);
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
struct ActionJack : JackSmallPointingDown {
  ActionJack() {
    setSvg(monochromeJack("jack-small-pointing-down"));
    box.size = Vec(11, 11);
  }
  void draw(const DrawArgs& args) override {
    auto scaled = args;
    scaled.clipBox.pos = scaled.clipBox.pos / (11.f / 16);
    scaled.clipBox.size = scaled.clipBox.size / (11.f / 16);
    nvgSave(args.vg);
    nvgScale(args.vg, 11.f / 16, 11.f / 16);
    JackSmallPointingDown::draw(scaled);
    nvgRestore(args.vg);
  }
  void drawLayer(const DrawArgs& args, int layer) override {
    auto scaled = args;
    scaled.clipBox.pos = scaled.clipBox.pos / (11.f / 16);
    scaled.clipBox.size = scaled.clipBox.size / (11.f / 16);
    nvgSave(args.vg);
    nvgScale(args.vg, 11.f / 16, 11.f / 16);
    JackSmallPointingDown::drawLayer(scaled, layer);
    nvgRestore(args.vg);
  }
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
    int route = -1;
    int64_t bridge = -1;
    app::ParamWidget* gain = nullptr;
    Port(int64_t id, int port, bool input, Vec pos, PortFace* face)
        : id(id),
          portId(port),
          input(input),
          originalPosition(pos),
          face(face) {}
  };
  void ensureInputEndpoints(int slot);
  void layoutInputGains();
  void attachInput(Port& p, int preferred, int64_t bridge, float gain);
  void detachInput(Port& p);
  bool inputBridge(const engine::Cable* c) const;
  bool discardInvalidBridge(app::CableWidget* cable);
  void syncInputCables(bool editing);
  void withInputCablePresentation(app::PortWidget* target,
                                  const std::function<void()>& action);
  void rewireInput(app::CableWidget* cw, app::PortWidget* target);
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
  PatchIdentity appearance;
  int appliedTheme = -1;
  bool darkPanel() const { return appliedTheme == 1; }
  void applyTheme(bool force = false) {
    bool dark = resolveDarkTheme(
        module ? getModule<Module>()->panelTheme.load() : 0,
        preferences().defaultTheme, settings::preferDarkPanels);
    if (!force && appliedTheme == int(dark)) return;
    appliedTheme = int(dark);
    titleFrame->faceColor = dark ? nvgRGB(44, 44, 44) : nvgRGB(200, 200, 200);
    ownFrame->faceColor = dark ? nvgRGB(52, 52, 52) : nvgRGB(214, 214, 214);
    frame->faceColor = dark ? nvgRGB(46, 46, 46) : nvgRGB(204, 204, 204);
    inputFrame->faceColor = dark ? nvgRGB(55, 55, 55) : nvgRGB(204, 204, 204);
    outputFrame->faceColor = dark ? nvgRGB(25, 25, 25) : nvgRGB(116, 116, 116);
    for (auto* svg : {&titleArtwork, &logoArtwork}) {
      auto path = asset::plugin(pluginInstance,
                                svg == &titleArtwork
                                    ? "res/Beekapoo/title.svg"
                                    : "res/Beekapoo/computerscare-logo.svg");
      svg->setSvg(dark ? darkHeaderSvg(path) : APP->window->loadSvg(path));
    }
    auto buttonStyle = [dark](ActionButton* button, bool all = false) {
      button->faceColor =
          all ? (dark ? nvgRGB(165, 165, 165) : nvgRGB(92, 92, 92))
              : (dark ? nvgRGB(82, 82, 82) : nvgRGB(214, 214, 214));
      button->textColor =
          (all != dark) ? nvgRGB(242, 242, 242) : nvgRGB(35, 35, 35);
    };
    for (auto* button : managementButtons) buttonStyle(button);
    addSelectionButton->setDark(dark);
    for (auto* button : {regenerateButton, growButton}) buttonStyle(button);
    for (size_t i = 0; i < actionButtons.size(); ++i)
      buttonStyle(actionButtons[i], i >= 3);
    for (const auto& buttons :
         {sectionAddButtons, sectionRerollButtons, sectionRemoveButtons})
      for (auto* button : buttons) buttonStyle(button);
    for (auto* own : {getParam(Module::WIGGLE_AMOUNT)})
      if (auto* knob = dynamic_cast<ThemedSmallKnob*>(own)) knob->setDark(dark);
    for (auto* item : controls->children) {
      if (auto* knob = dynamic_cast<ThemedSmallKnob*>(item))
        knob->setDark(dark);
      if (auto* knob = dynamic_cast<ParametricKnob*>(item)) knob->setDark(dark);
      if (auto* button = dynamic_cast<ExposedSwitch*>(item)) {
        for (auto* child : button->children)
          if (auto* face = dynamic_cast<ExposedSwitch::Face*>(child)) {
            face->faceColor = button->skin.color(0, dark);
            face->textColor = button->skin.indicatorColor(dark);
          }
      }
    }
  }
  void setAllThemes(int choice) {
    preferences().defaultTheme = validPanelTheme(choice);
    preferences().save();
    for (auto* w : homeRack->getModules())
      if (auto* owner = dynamic_cast<Beekapoo*>(w))
        if (auto* m = owner->getModule<Module>()) m->panelTheme.store(0);
  }
  void themeMenu(ui::Menu* menu) {
    menu->addChild(createSubmenuItem(
        "Theme", panelThemeName(getModule<Module>()->panelTheme.load()),
        [this](ui::Menu* sub) {
          for (int choice = 0; choice < 3; ++choice)
            sub->addChild(createCheckMenuItem(
                panelThemeName(choice), "",
                [this, choice] {
                  return getModule<Module>()->panelTheme.load() == choice;
                },
                [this, choice] {
                  getModule<Module>()->panelTheme.store(choice);
                }));
          sub->addChild(new ui::MenuSeparator);
          sub->addChild(createSubmenuItem(
              "All Beekapoos", panelThemeName(preferences().defaultTheme),
              [this](ui::Menu* all) {
                for (int choice = 0; choice < 3; ++choice)
                  all->addChild(createCheckMenuItem(
                      choice ? panelThemeName(choice) : "Default (Rack)", "",
                      [choice] { return preferences().defaultTheme == choice; },
                      [this, choice] { setAllThemes(choice); }));
              }));
        }));
  }
  uint64_t namedTopology = 0;
  bool namedTopologyReady = false;
  void refreshGeneratedName();
  void resetNameTopology() {
    refreshAppearance();
    namedTopology = appearance.wiring;
    namedTopologyReady = true;
  }
  uint64_t appearanceSnapshot = 0;
  unsigned appearancePoll = 0;
  bool refreshAppearance() {
    std::vector<std::string> types, snapshot;
    std::map<int64_t, int> index;
    for (const auto& member : members)
      if (auto* w = find(member.id)) {
        index[member.id] = int(types.size());
        types.push_back(moduleStyleName(w->model));
        snapshot.push_back(std::to_string(member.id) + types.back());
      }
    std::vector<StyleCable> cables;
    if (!index.empty())
      for (auto* wire : homeRack->getCompleteCables()) {
        auto* c = wire->cable;
        if (panelCableIds.count(c->id)) continue;
        auto a = index.find(c->outputModule->id),
             b = index.find(c->inputModule->id);
        if (a == index.end() || b == index.end()) continue;
        cables.emplace_back(
            a == index.end() ? -1 : a->second, c->outputId,
            b == index.end() ? -1 : b->second, c->inputId,
            a == index.end() ? moduleStyleName(c->outputModule->model) : "",
            b == index.end() ? moduleStyleName(c->inputModule->model) : "");
        snapshot.push_back(identityRecord(
            {std::to_string(c->outputModule->id), std::to_string(c->outputId),
             std::to_string(c->inputModule->id), std::to_string(c->inputId),
             cables.back().outsideFrom, cables.back().outsideTo}));
      }
    std::sort(snapshot.begin(), snapshot.end());
    uint64_t signature = identityHash(identityRecord(snapshot));
    if (signature == appearanceSnapshot) return false;
    appearanceSnapshot = signature;
    auto next = patchIdentity(types, cables);
    bool changed = next != appearance;
    appearance = next;
    return changed;
  }

  widget::SvgWidget titleArtwork, logoArtwork;
  Rect titleInk, headerArtworkArea, headerLogoArea;
  computerscare::type::Text titleText;
  ComputerscareResizeHandle *leftHandle = nullptr, *rightHandle = nullptr;
  phlooper::WarpedBlock *ownFrame = nullptr, *titleFrame = nullptr;
  struct ControlLabel {
    Vec pos;
    float width;
    std::string text;
    bool output;
    bool required;
    int align;
    float fontSize, maxHeight;
    mutable bool prepared = false, readable = false;
    mutable float preparedSize = 0, lineHeight = 0;
    mutable std::vector<std::string> lines;
    ControlLabel(Vec position, float available, std::string label, bool out,
                 int alignment, float size = 0, float height = 0,
                 bool always = false)
        : pos(position),
          width(available),
          text(std::move(label)),
          output(out),
          required(always),
          align(alignment),
          fontSize(size),
          maxHeight(height) {}
  };
  std::vector<ControlLabel> controlLabels;
  std::vector<ActionButton*> actionButtons;
  std::vector<ActionButton*> managementButtons;
  SelectionLogo* addSelectionButton = nullptr;
  ActionButton* regenerateButton = nullptr;
  ActionButton* growButton = nullptr;
  std::array<ActionButton*, 3> sectionAddButtons{}, sectionRerollButtons{},
      sectionRemoveButtons{};
  int hoveredSection = -1;
  struct SectionCandidates {
    std::vector<app::ParamWidget*> parameters;
    std::vector<app::PortWidget*> inputs, outputs;
  };
  SectionCandidates sectionCandidates();
  bool sectionPortProtected(const Port& p);
  bool addExposedControl(int kind);
  bool rerollExposedControls(int kind);
  bool removeExposedControl(int kind);
  bool changeExposedControls(int kind, bool reroll);
  void updateSectionAddButtons(Vec mouse);
  phlooper::WarpedBlock* frame = nullptr;
  phlooper::WarpedBlock *inputFrame = nullptr, *outputFrame = nullptr;
  struct Heading {
    Vec pos;
    std::string text;
    bool output;
    mutable computerscare::type::Text rendered;
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
  bool importContents(json_t* root, const std::string& storageRoot = "",
                      int64_t sourceOwner = -1,
                      history::ModuleChange* replacement = nullptr);
  void saveBeek(const std::string& path);
  bool loadBeek(const std::string& path, int64_t sourceOwner = -1,
                bool convertAudio = false);
  void saveBeekDialog();
  void loadBeekDialog();
  bool generateDefaultPatch(int requestedCount = 0);
  bool regeneratePatch();
  bool growPatch(history::ComplexAction* parent = nullptr);
  void initializeBrowserPreview();
  std::vector<std::unique_ptr<engine::Module>> browserModules;
  std::unique_ptr<engine::Module> browserInputGains;
  std::string browserName;
  unsigned browserMemberCount = 0;
  void generateOnBrowserAdd();
  void renameMenu(ui::Menu* menu);
  WeakPtr<widget::Widget> nameEditor;
  void editName();
  void rename(const std::string& name) {
    if (!module) return;
    auto* m = getModule<Module>();
    if (m->getPatchName() == name && m->hasCustomPatchName()) return;
    auto* action = new RenameHistory(m, name);
    m->setPatchName(name, true);
    APP->history->push(action);
  }
  void onHover(const HoverEvent& e) override {
    if (module && addSelectionButton->visible &&
        addSelectionButton->box.contains(e.pos)) {
      e.consume(addSelectionButton);
      return;
    }
    ModuleWidget::onHover(e);
  }
  void onButton(const ButtonEvent& e) override {
    // Rack intercepts all clicks on selected modules before dispatching to
    // children. This action must also work with Beekapoo in the selection.
    if (module && addSelectionButton->visible &&
        addSelectionButton->box.contains(e.pos) &&
        e.button == GLFW_MOUSE_BUTTON_LEFT && e.action == GLFW_PRESS &&
        (e.mods & RACK_MOD_MASK) != GLFW_MOD_SHIFT) {
      auto click = e;
      click.pos -= addSelectionButton->box.pos;
      addSelectionButton->onButton(click);
      return;
    }
    if (module && e.button == GLFW_MOUSE_BUTTON_RIGHT &&
        e.action == GLFW_PRESS && headerArtworkArea.contains(e.pos)) {
      editName();
      e.consume(this);
      return;
    }
    ModuleWidget::onButton(e);
  }
  void duplicateAction(bool cables);
  void disconnectPanelAction();
  void prepareContainedSave() {
    for (auto& member : members)
      if (auto* w = find(member.id)) APP->engine->prepareSaveModule(w->module);
  }
  void deleteGroupAction();
  void pruneMissingMembers();
  void editSubpatch();
  void ejectAction(history::ComplexAction* parent = nullptr,
                   bool preserveGains = true);
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
                          nvgRGB(200, 200, 200));
    addChild(titleFrame);
    ownFrame = new PackedPanelBlock;
    ownFrame->configure(Rect(Vec(6, 26), Vec(168, 100)), 4, 1.3f, 811,
                        nvgRGB(195, 195, 195));
    addChild(ownFrame);
    frame = new PackedPanelBlock;
    frame->configure(Rect(Vec(9, 140), Vec(342, 213)), 5, 1.8f, 719,
                     nvgRGB(204, 204, 204));
    addChild(frame);
    inputFrame = new PackedPanelBlock;
    inputFrame->configure(Rect(Vec(9, 253), Vec(165, 100)), 5, 2.f, 723,
                          nvgRGB(204, 204, 204));
    addChild(inputFrame);
    outputFrame = new PackedPanelBlock;
    outputFrame->configure(Rect(Vec(186, 253), Vec(165, 100)), 5, 2.f, 727,
                           nvgRGB(116, 116, 116));
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
      return (editShowsText ? "Edit " : "") +
             std::to_string(module ? members.size() : browserMemberCount);
    };
    show->configure(52, 24, 3, 2, .8f, 713);
    show->box.pos = Vec(15, 39);
    show->help = [] { return "Edit subpatch in Rack [E]"; };
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
    addSelectionButton = new SelectionLogo;
    addSelectionButton->help = [] {
      return "Add selected modules to Beekapoo";
    };
    addSelectionButton->action = [this] {
      if (!canAddSelection()) return;
      addSelection();
    };
    addChild(addSelectionButton);
    regenerateButton = new ActionButton;
    regenerateButton->configure(14, 14, 1.1f, .8f, .8f, 718);
    regenerateButton->icon = 8;
    regenerateButton->help = [] {
      return "Generate new subpatch (replace contents)";
    };
    regenerateButton->action = [this] { regeneratePatch(); };
    addChild(regenerateButton);
    growButton = new ActionButton;
    growButton->configure(18, 18, 1.1f, .8f, .8f, 719);
    growButton->icon = 9;
    growButton->help = [] {
      return "Grow subpatch: add a cable or VCA connection";
    };
    growButton->action = [this] { growPatch(); };
    addChild(growButton);
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
        addInput(
            createInputCentered<ActionJack>(Vec(x + 68, y + 11), m, index));
      }
    addParam(createParamCentered<ThemedSmallKnob>(Vec(328, 94), m,
                                                  Module::WIGGLE_AMOUNT));
    for (int i : {Module::GENERATE_INPUT, Module::GROW_INPUT})
      addInput(createInput<ActionJack>(Vec(), m, i));
    if (m) {
      auto* saved = json_object_get(m->group, "ports");
      for (size_t i = 0; i < json_array_size(saved); ++i) {
        auto* route = json_object_get(json_array_get(saved, i), "route");
        if (json_is_integer(route))
          ensureInputEndpoints(json_integer_value(route));
      }
    }
    controls = new widget::Widget;
    addChild(controls);
    for (int kind = 0; kind < 3; ++kind) {
      const std::string singular = kind == 0   ? "parameter"
                                   : kind == 1 ? "input"
                                               : "output";
      for (int operation = 0; operation < 3; ++operation) {
        auto* button = new ActionButton;
        button->configure(16, 16, 1.1f, .8f, .8f, 770 + kind * 3 + operation);
        button->icon = operation == 0 ? 6 : operation == 1 ? 1 : 10;
        button->visible = false;
        button->help = [singular, operation] {
          if (operation == 0) return "Expose another " + singular;
          return operation == 1 ? "Re-roll exposed " + singular + "s"
                                : "Remove one exposed " + singular;
        };
        button->action = [this, kind, operation] {
          if (operation == 0)
            addExposedControl(kind);
          else if (operation == 1)
            rerollExposedControls(kind);
          else
            removeExposedControl(kind);
        };
        button->pressed = [button] {
          return APP->event->getDraggedWidget() == button;
        };
        (operation == 0   ? sectionAddButtons
         : operation == 1 ? sectionRerollButtons
                          : sectionRemoveButtons)[kind] = button;
        addChild(button);
      }
    }
    leftHandle = new ComputerscareResizeHandle;
    leftHandle->box.size.x = 5;
    addChild(leftHandle);
    rightHandle = new ComputerscareResizeHandle;
    rightHandle->right = true;
    rightHandle->box.size.x = 5;
    addChild(rightHandle);
    layoutControls();
    installRackHooks();
    if (!m) initializeBrowserPreview();
    if (m)
      m->captureContents = [this](bool external) {
        return portableContents(external);
      };
  }
  ~Beekapoo() override {
    if (!module && controls) controls->clearChildren();
    if (auto* m = getModule<Module>()) {
      std::lock_guard<std::mutex> lock(m->stateMutex);
      m->captureContents = {};
    }
    closeView();
    for (const auto& port : ports)
      if (auto* source = originalPort(port))
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
  void compactMembers() {
    std::vector<Member*> ordered;
    for (auto& member : members)
      if (find(member.id)) ordered.push_back(&member);
    if (ordered.empty()) return;
    std::stable_sort(ordered.begin(), ordered.end(), [](Member* a, Member* b) {
      return a->position.y == b->position.y ? a->position.x < b->position.x
                                            : a->position.y < b->position.y;
    });
    Vec origin(INFINITY, INFINITY);
    std::vector<float> widths;
    for (auto* member : ordered) {
      origin.x = std::min(origin.x, member->position.x);
      origin.y = std::min(origin.y, member->position.y);
      widths.push_back(member->widget->box.size.x);
    }
    auto cells = compactMemberCells(widths);
    for (size_t i = 0; i < ordered.size(); ++i) {
      ordered[i]->position =
          origin + Vec(cells[i].x, cells[i].row * RACK_GRID_HEIGHT);
      if (expanded) {
        auto* registered = homeRack->getModule(ordered[i]->id);
        if (registered) registered->box.pos = ordered[i]->position;
      }
    }
    // Reuse the existing placement/collision handling when editing in Rack.
    if (editSession) {
      auto* scroll = APP->scene->rackScroll;
      Vec previous = scroll->getGridOffset();
      moveEditedMembers(false, false, true);
      scroll->setGridOffset(previous);
    }
  }
  bool repairSeparatedRows() {
    // Older Grow added a member at y=0 even when the stored arrangement was
    // in absolute Rack coordinates. That leaves ~100 empty rows between
    // members, so both Edit and Eject center on empty rack. Recover only
    // very large *empty* row gaps; ordinary multi-row arrangements survive.
    std::vector<Member*> live;
    for (auto& member : members)
      if (find(member.id)) live.push_back(&member);
    if (live.size() < 2) return false;
    std::vector<std::pair<float, float>> rows;
    for (auto* member : live)
      rows.emplace_back(member->position.y,
                        member->position.y + member->widget->box.size.y);
    std::sort(rows.begin(), rows.end());
    float bottom = rows.front().second;
    bool separated = false;
    for (const auto& row : rows) {
      separated |= row.first - bottom > 8 * RACK_GRID_HEIGHT;
      bottom = std::max(bottom, row.second);
    }
    if (!separated) return false;
    float anchor = rows[rows.size() / 2].first;
    std::vector<Rect> placed;
    for (auto* member : live)
      if (std::abs(member->position.y - anchor) <= 8 * RACK_GRID_HEIGHT)
        placed.emplace_back(member->position, member->widget->box.size);
    for (auto* member : live) {
      if (std::abs(member->position.y - anchor) <= 8 * RACK_GRID_HEIGHT)
        continue;
      auto size = member->widget->box.size;
      member->position.y = anchor;
      bool collision;
      do {
        collision = false;
        for (auto rect : placed)
          if (Rect(member->position, size).intersects(rect)) {
            member->position.x = rect.getRight();
            collision = true;
            break;
          }
      } while (collision);
      placed.emplace_back(member->position, size);
    }
    hasEditPlacement = false;
    return true;
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
          list,
          json_pack("{s:I,s:i,s:b,s:i,s:I,s:f}", "module", (json_int_t)port.id,
                    "port", port.portId, "input", port.input, "route",
                    port.route, "bridge", (json_int_t)port.bridge, "gain",
                    double(port.gain ? port.gain->getParamQuantity()->getValue()
                                     : 1)));
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
    unsigned role = 0;
    for (auto* block : {titleFrame, ownFrame, frame, inputFrame, outputFrame}) {
      auto* section = static_cast<PackedPanelBlock*>(block);
      section->skin = sectionSkin(appearance, role++);
      block->shapeSeed =
          unsigned(section->skin.seed ^ (section->skin.seed >> 32));
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
      if (section->skin.profile <= 1) {
        block->cornerOffsets = {};
        block->screwup = 0;
      }
      if (block == ownFrame) {
        block->cornerOffsets = {};
        block->depth = 1.2f;
        block->screwup = .4f;
        block->faceColor = nvgRGB(214, 214, 214);
      }
    }
    applyTheme(true);
  }
  struct HeaderLayout {
    Rect artwork, logo;
    std::array<Rect, 2> buttons{};
    float height = 0;
    bool text = false;
  };
  static HeaderLayout headerLayout(float width, int members) {
    HeaderLayout result;
    result.text = width >= 150;
    float editWidth =
        (result.text ? 46.f : 19.f) + 5 * std::to_string(members).size();
    float ejectWidth = result.text ? 47.f : 18.f;
    float buttonWidth = editWidth + 2 + ejectWidth;
    float right = width - 10;
    bool beside = width >= 270;
    float titleHeight = width <= 90 ? 32.f : 27.f;
    float logoWidth = std::min(32.f, (width - 16) * .4f);
    result.logo = Rect(Vec(right - logoWidth, 5), Vec(logoWidth, titleHeight));
    result.artwork = Rect(
        Vec(6, 5),
        Vec((beside ? result.logo.pos.x - buttonWidth - 6 : result.logo.pos.x) -
                9,
            titleHeight));
    float x = beside ? result.logo.pos.x - buttonWidth - 4 : 6;
    float y = beside ? 9 : titleHeight + 8;
    for (int i = 0; i < 2; ++i) {
      float w = i == 0 ? editWidth : ejectWidth;
      if (x + w > right && x > 6) {
        x = 6;
        y += 21;
      }
      result.buttons[i] = Rect(Vec(x, y), Vec(w, 18));
      x += w + 2;
    }
    result.height = std::max(result.artwork.getBottom(), y + 18) + 8;
    return result;
  }
  float layoutHeaderControls(float width) {
    addSelectionButton->setAvailable(canAddSelection());
    auto plan =
        headerLayout(width, module ? members.size() : browserMemberCount);
    headerArtworkArea = plan.artwork;
    headerLogoArea = plan.logo;
    addSelectionButton->box = logoTarget();
    editShowsText = plan.text;
    const int icons[] = {3, 7};
    for (int i = 0; i < 2; ++i) {
      auto* button = managementButtons[i];
      button->visible = true;
      button->icon = icons[i];
      button->iconAndText = i == 0 || (i == 1 && plan.text);
      button->textSize = plan.text ? 10 : 9;
      button->faceColor = nvgRGB(214, 214, 214);
      button->textColor = nvgRGB(45, 45, 45);
      button->upDepth = 1.1f;
      button->downDepth = .8f;
      button->box = plan.buttons[i];
    }
    return plan.height;
  }
  struct OwnControlLayout {
    std::array<Vec, 6> positions{};
    float height = 0;
    int columns = 0;
  };
  static OwnControlLayout ownControlLayout(float width) {
    OwnControlLayout result;
    float x = 6, y = 1, right = width - 6;
    // Keep each button and its CV directly above/below one another.
    for (int first : {0, 2, 4}) {
      if (x + 37 > right && x > 6) {
        x = 6;
        y += 32;
      }
      for (int i = first; i < first + 2; ++i) {
        result.positions[i] = Vec(x, y);
        x += 19;
      }
      result.columns = std::max(result.columns, int((x - 6) / 19));
    }
    result.height = y + 33;
    return result;
  }
  static float tinyOwnHeight(float width) {
    return ownControlLayout(width).height;
  }
  float layoutOwnControls(float width, float top) {
    getParam(Module::WIGGLE_AMOUNT)->visible = false;
    for (int i = 0; i < 6; ++i) getInput(i)->visible = false;
    for (int i = 0; i < 6; ++i) actionButtons[i]->visible = i % 3 != 2;
    auto plan = ownControlLayout(width);
    const std::array<ActionButton*, 6> buttons = {
        actionButtons[0], actionButtons[1], actionButtons[3],
        actionButtons[4], regenerateButton, growButton};
    for (int i : {0, 1, 3, 4}) actionButtons[i]->icon = i % 3;
    const int ids[] = {0, 1, 3, 4, Module::GENERATE_INPUT, Module::GROW_INPUT};
    for (int i = 0; i < 6; ++i) {
      auto* button = buttons[i];
      button->visible = true;
      button->iconAndText = false;
      button->textSize = 9;
      bool all = i == 2 || i == 3;
      button->faceColor = all ? nvgRGB(92, 92, 92) : nvgRGB(214, 214, 214);
      button->textColor = all ? nvgRGB(242, 242, 242) : nvgRGB(45, 45, 45);
      button->upDepth = 1.1f;
      button->downDepth = .8f;
      button->box = Rect(plan.positions[i] + Vec(0, top), Vec(18, 18));
      auto* jack = getInput(ids[i]);
      jack->visible = true;
      jack->box.pos = Vec(button->box.getCenter().x - jack->box.size.x * .5f,
                          button->box.getBottom() + 1);
    }
    ownFrame->visible = true;
    ownFrame->box = Rect(Vec(6, top), Vec(width - 12, plan.height));
    responsive.ownColumns = plan.columns;
    responsive.ownHeight = plan.height;
    return plan.height;
  }
  static float minimumJackWidth(int inputs, int outputs,
                                int parameterCount = 0) {
    // Fixed action inputs are budgeted above, not in the exposed-port rows.
    inputs = std::max(0, inputs - 6);
    for (int width = 45; width < 30000; width += 15) {
      int cols = std::max(1, int((width - 25) / 20));
      float height =
          4 + headerLayout(width, 9).height + tinyOwnHeight(width) + 8 +
          layoutRows(inputs, cols) * 64 + layoutRows(outputs, cols) * 40 +
          (inputs ? 14 : 0) + (outputs ? 14 : 0) + (inputs && outputs ? 8 : 0) +
          (parameterCount ? std::max(32, layoutRows(parameterCount, cols) * 16)
                          : 0);
      if (height <= 368) return width;
    }
    return 30000;
  }
  void layoutMappedParameters(
      const std::vector<widget::Widget*>& knobs,
      const std::map<widget::Widget*, std::pair<std::string, std::string>>&
          names,
      float top, float bottom) {
    std::vector<widget::Widget*> active;
    std::vector<ParamLayoutItem> metadata;
    std::map<engine::Module*, int> moduleOrder;
    for (auto* item : knobs) {
      if (!item) continue;
      auto* param = dynamic_cast<app::ParamWidget*>(item);
      if (!param) continue;
      item->visible = false;
      if (!moduleOrder.count(param->module))
        moduleOrder[param->module] = int(moduleOrder.size());
      auto name = names.find(item);
      metadata.push_back(
          {name == names.end() ? "Parameter" : name->second.first,
           moduleOrder[param->module]});
      active.push_back(item);
    }
    for (size_t i = 0; i < active.size(); ++i) {
      auto* q = static_cast<app::ParamWidget*>(active[i]);
      metadata[i].scale =
          controlStyle(metadata[i].label, moduleStyleName(q->module->model),
                       appearance)
              .size;
    }
    if (auto font = computerscare::type::font()) {
      auto* vg = APP->window->vg;
      nvgSave(vg);
      nvgFontFaceId(vg, font->handle);
      nvgFontSize(vg, 14);
      for (auto& item : metadata) {
        std::string full = item.label;
        auto measure = [&](const std::string& word) {
          auto text = computerscare::type::alternates(
              labelCase(word, appearance.modules), appearance.modules);
          return nvgTextBounds(vg, 0, 0, text.c_str(), nullptr, nullptr);
        };
        for (const auto& word : labelWords(full))
          if (measure(word) > box.size.x - 30) {
            item.label = abbreviatedLabel(full);
            break;
          }
        for (const auto& word : labelWords(item.label))
          item.captionWidth = std::max(item.captionWidth, measure(word) + 6);
      }
      nvgRestore(vg);
    }
    float available = std::max(0.f, bottom - top - 10);
    auto cells =
        layoutParams(metadata, box.size.x - 25, available, arrangeByModule);
    frame->box =
        Rect(Vec(6, top), Vec(box.size.x - 12, std::max(0.f, bottom - top)));
    // The empty parameter area is still its own section. It must not make
    // the action strip grow to occupy the whole panel when nothing is mapped.
    frame->visible = available > 0 && (!expanded || editPreview);
    struct Caption {
      std::string text;
      float left = INFINITY, right = -INFINITY, bottom = 0, size = 0, row = -1,
            height = 0;
      int count = 0;
    };
    std::map<std::string, Caption> captions;
    for (const auto& cell : cells) {
      auto* item = active[cell.index];
      auto* parameter = dynamic_cast<app::ParamWidget*>(item);
      auto label = metadata[cell.index].label;
      if (auto* knob = dynamic_cast<ParametricKnob*>(item))
        knob->setIdentity(label, moduleStyleName(parameter->module->model),
                          appearance);
      if (auto* button = dynamic_cast<ExposedSwitch*>(item)) {
        auto skin = controlStyle(
            label, moduleStyleName(parameter->module->model), appearance);
        button->skin = skin;
        for (auto* child : button->children)
          if (auto* face = dynamic_cast<ExposedSwitch::Face*>(child)) {
            face->faceColor = skin.color();
            face->textColor = skin.indicatorColor();
            face->shapeSeed = unsigned(controlHash(label) ^ appearance.wiring);
            face->screwup = .3f + skin.ripple * 5;
            face->upDepth = skin.bevel * 1.5f;
            face->downDepth = skin.bevel * .7f;
            face->box.size = Vec(25 * std::max(.85f, skin.aspectX),
                                 24 * std::max(.85f, skin.aspectY));
            face->box.pos = (Vec(25, 24) - face->box.size) * .5f;
            face->textSize = 12;
          }
      }
      if (auto* visual = dynamic_cast<ParamVisual*>(item))
        visual->setVisualSize(cell.diameter);
      Vec center(10 + cell.x, top + 5 + cell.y);
      item->box.pos = center - item->box.size * .5f;
      item->visible = true;

      if (cell.labelWidth <= 0) continue;
      if (cell.block >= 0) {
        controlLabels.push_back(
            {Vec(10 + cell.captionX, top + 5 + cell.captionY), cell.labelWidth,
             cell.captionText, false, NVG_ALIGN_CENTER, 14 * cell.labelScale,
             cell.labelHeight});
        continue;
      }
      std::string key =
          (arrangeByModule ? std::to_string(metadata[cell.index].module) + ":"
                           : "") +
          paramFamily(label);
      auto& caption = captions[key];
      if (!caption.count)
        caption.text = label;
      else if (caption.text != label) {
        // A numbered family has one common caption; the individual number
        // is retained on each generated knob rather than repeating the name.
        std::string common;
        for (unsigned char c : caption.text)
          if (!std::isdigit(c)) common += char(c);
        for (size_t duplicate = common.find("  ");
             duplicate != std::string::npos; duplicate = common.find("  "))
          common.erase(duplicate, 1);
        while (!common.empty() &&
               (common.back() == ' ' || common.back() == '-' ||
                common.back() == '#'))
          common.pop_back();
        caption.text = common;
      }
      // A family spanning rows has one caption under its final row. Bounds
      // from earlier rows must not cover a neighbour's caption on that row.
      if (caption.row != cell.y) {
        caption.left = INFINITY;
        caption.right = -INFINITY;
        caption.row = cell.y;
        caption.height = cell.labelHeight;
      }
      float half = cell.labelWidth * .5f;
      caption.left = std::min(caption.left, center.x - half);
      caption.right = std::max(caption.right, center.x + half);
      caption.bottom =
          std::max(caption.bottom, center.y + cell.diameter * .5f + 5);
      caption.size =
          std::max(caption.size,
                   clamp(cell.diameter * .36f, 14.f, 18.f) * cell.labelScale);
      ++caption.count;
    }
    for (const auto& entry : captions) {
      const auto& caption = entry.second;
      controlLabels.push_back(
          {Vec((caption.left + caption.right) * .5f, caption.bottom),
           caption.right - caption.left, caption.text, false, NVG_ALIGN_CENTER,
           caption.size, caption.height});
    }
  }
  void layoutPortBlock(const std::vector<widget::Widget*>& items,
                       phlooper::WarpedBlock* block, Rect region) {
    block->visible = !items.empty() && (!expanded || editPreview);
    block->box = region;
  }
  void layoutPortControls(
      const std::vector<widget::Widget*>& items, phlooper::WarpedBlock* block,
      int columns, bool output,
      const std::map<widget::Widget*, std::pair<std::string, std::string>>&
          names) {
    if (items.empty()) return;
    // The frames expand into the gaps in finishFrames(). Center the full
    // jack/attenuverter/caption group within that final face, not the old grid.
    Rect region = frameContent(block);
    columns = std::max(1, columns);
    int rows = layoutRows(items.size(), columns);
    float cellW = region.size.x / columns;
    float cellH = region.size.y / rows;
    for (size_t i = 0; i < items.size(); ++i) {
      auto* face = static_cast<PortFace*>(items[i]);
      auto cell = planPortCell(cellW, cellH, !output);
      int rowCount =
          std::min(columns, int(items.size()) - int(i / columns) * columns);
      Vec origin = region.pos + Vec((columns - rowCount) * cellW * .5f +
                                        (i % columns) * cellW,
                                    (i / columns) * cellH);
      face->compact = !cell.large;
      face->box = Rect(origin + Vec(cell.jack.x, cell.jack.y),
                       Vec(cell.jack.w, cell.jack.h));
      face->visible = true;
      face->gainLaidOut = !output;
      face->gainCenter = origin + Vec(cell.gain.x + 10, cell.gain.y + 10);
      auto found = names.find(face);
      if (found != names.end() && cell.label.h > 0)
        controlLabels.push_back(
            {origin + Vec(cell.label.x + cell.label.w * .5f, cell.label.y),
             cell.label.w, found->second.first, output, NVG_ALIGN_CENTER,
             cell.fontSize, cell.label.h, true});
    }
  }
  void layoutOnlyJacks(
      std::vector<widget::Widget*> inputs, std::vector<widget::Widget*> outputs,
      const std::vector<widget::Widget*>& knobs,
      const std::map<widget::Widget*, std::pair<std::string, std::string>>&
          names) {
    jacksOnly = true;
    getParam(Module::WIGGLE_AMOUNT)->visible = false;
    for (auto& parameter : parameters) parameter.knob->visible = false;
    float minimum =
        minimumJackWidth(inputs.size() + 6, outputs.size(), knobs.size());
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
    layoutTier = 0;
    responsive = ResponsiveLayout();
    responsive.width = width;
    float ownTop = layoutHeaderControls(width) + 4;
    layoutOwnControls(width, ownTop);
    int cols = std::max(1, int((width - 25) / 20));
    int inputCols = std::min(int(inputs.size()), cols);
    int outputCols = std::min(int(outputs.size()), cols);
    // Fill both port blocks horizontally before spending vertical space.
    // Captions require a real label column, not a tiny sliver beside a jack.
    bool beside = !inputs.empty() && !outputs.empty() && width >= 150;
    if (beside) {
      int total = std::max(2, int((width - 46) / 20));
      inputCols = std::max(1, std::min(int(inputs.size()), total / 2));
      outputCols =
          std::max(1, std::min(int(outputs.size()), total - inputCols));
      inputCols = std::max(1, std::min(int(inputs.size()), total - outputCols));
    }
    if (beside) {
      float side = 12 + 45 * std::max(layoutRows(inputs.size(), inputCols),
                                      layoutRows(outputs.size(), outputCols));
      float stack = 32 + 45 * (layoutRows(inputs.size(), cols) +
                               layoutRows(outputs.size(), cols));
      if (side > stack) {
        beside = false;
        inputCols = std::min(int(inputs.size()), cols);
        outputCols = std::min(int(outputs.size()), cols);
      }
    }
    float slotWidth = beside
                          ? (width - 46) / std::max(1, inputCols + outputCols)
                          : (width - 25) / std::max(1, cols);
    bool portLabels =
        width >= 120 && inputs.size() + outputs.size() <= 4 && slotWidth >= 35;
    float inputRow = portLabels ? 76.f : 64.f;
    float outputRow = portLabels ? 50.f : 40.f;
    auto paneHeight = [&](size_t count, int columns, float row) {
      return count ? 12 + layoutRows(count, std::max(1, columns)) * row : 0.f;
    };
    auto space =
        portPaneSpace(width - 46, inputCols, outputCols, outputs.size());
    float inputWidth = beside ? space.first + 13 : width - 12;
    float outputWidth = beside ? space.second + 13 : width - 12;
    float outputPaneHeight = paneHeight(outputs.size(), outputCols, outputRow);
    float besideHeight = std::max(
        paneHeight(inputs.size(), inputCols, inputRow), outputPaneHeight);
    float inputHeight =
        beside ? besideHeight : paneHeight(inputs.size(), inputCols, inputRow);
    float outputHeight = beside ? besideHeight : outputPaneHeight;
    float y =
        368 - (beside ? std::max(inputHeight, outputHeight)
                      : inputHeight + outputHeight +
                            (!inputs.empty() && !outputs.empty() ? 8 : 0));
    float parameterTop = ownFrame->box.getBottom() + 8;
    float parameterBottom = y - 8;
    layoutPortBlock(inputs, inputFrame,
                    Rect(Vec(6, y), Vec(inputWidth, inputHeight)));
    if (!beside && !inputs.empty()) y += inputHeight + 8;
    layoutPortBlock(outputs, outputFrame,
                    Rect(Vec(beside ? width - 6 - outputWidth : 6, y),
                         Vec(outputWidth, outputHeight)));
    outputFrame->visible = !outputs.empty() && (!expanded || editPreview);
    layoutMappedParameters(knobs, names, parameterTop, parameterBottom);
    finishFrames();
    layoutPortControls(inputs, inputFrame, inputCols, false, names);
    layoutPortControls(outputs, outputFrame, outputCols, true, names);
    layoutInputGains();
    if (changed && !ejecting && parent == homeRack->getModuleContainer())
      homeRack->setModulePosForce(this, box.pos);
  }
  void layoutControls() {
    refreshAppearance();
    // Editing reveals the originals without blanking the owner's live panel.
    // The optional floating preview is independent of this Rack presentation.
    editPreview = bool(editSession);
    controls->visible = !expanded || editPreview;
    std::vector<widget::Widget*> items[3];
    for (auto& p : parameters) p.knob->visible = controlVisible(p.id, 0);
    for (auto& p : ports) {
      p.face->visible = controlVisible(p.id, p.input ? 1 : 2) && findPort(p);
      if (p.gain) p.gain->visible = p.face->visible;
    }
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
            names[p->knob] = {q->getLabel(), memberName(p->id)};
          }
      for (auto* p : jacks) {
        items[p->input ? 1 : 2].push_back(p->face);
        auto* source = originalPort(*p);
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

    if (auto* state = getModule<Module>())
      state->panelWidth.store(requestedWidth);
    responsive = responsiveLayout(requestedWidth, items[0].size(),
                                  items[1].size(), items[2].size());
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
    float ownTop = layoutHeaderControls(width) + 4;
    float ownHeight = layoutOwnControls(width, ownTop);
    responsive.paramTop = ownTop + ownHeight + 8;
    float py = responsive.paramTop;
    frame->box = Rect(Vec(responsive.paramLeft, py),
                      Vec(responsive.paramWidth, responsive.paramHeight));
    frame->visible = responsive.paramHeight > 0 && (!expanded || editPreview);
    if (!items[1].empty()) responsive.portHeight += 23;
    float gap = items[1].empty() || items[2].empty() ? 0 : 8;
    int panes = (!items[1].empty() ? 1 : 0) + 1;
    auto space =
        portPaneSpace(width - 12 - gap - panes * 13, responsive.inputColumns,
                      std::max(1, responsive.outputColumns), items[2].size());
    float inWidth = responsive.inputColumns ? space.first + 13 : 0;
    float outWidth = space.second + 13;
    responsive.portHeight =
        std::max(responsive.portHeight,
                 12.f + 40 * layoutRows(items[2].size(),
                                        std::max(1, responsive.outputColumns)));
    // Every input row needs its own attenuverters, not just the first row.
    responsive.portHeight = std::max(
        responsive.portHeight,
        items[1].empty()
            ? 0.f
            : 12.f + 64 * layoutRows(items[1].size(), responsive.inputColumns));
    if (responsive.portHeight >
        368 - responsive.paramTop - (items[0].empty() ? 8 : 40)) {
      layoutOnlyJacks(items[1], items[2], items[0], names);
      return;
    }
    float portY = 368 - responsive.portHeight;
    layoutPortBlock(items[1], inputFrame,
                    Rect(Vec(6, portY), Vec(inWidth, responsive.portHeight)));
    layoutPortBlock(items[2], outputFrame,
                    Rect(Vec(width - 6 - outWidth, portY),
                         Vec(outWidth, responsive.portHeight)));
    outputFrame->visible = !items[2].empty() && (!expanded || editPreview);
    layoutMappedParameters(items[0], names, responsive.paramTop, portY - 8);
    finishFrames();
    layoutPortControls(items[1], inputFrame, responsive.inputColumns, false,
                       names);
    layoutPortControls(items[2], outputFrame, responsive.outputColumns, true,
                       names);
    layoutInputGains();
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
    if (sourceSwitch || (dynamic_cast<engine::SwitchQuantity*>(q) &&
                         !dynamic_cast<app::Knob*>(source))) {
      auto* button = createParamCentered<MappedParam<ExposedSwitch>>(
          Vec(), w->module, index);
      button->source = source;
      button->momentary = sourceSwitch && sourceSwitch->momentary;
      knob = button;
    } else {
      auto* generated = new MappedParam<ParametricKnob>;
      generated->module = w->module;
      generated->paramId = index;
      generated->snap = discrete;
      generated->initParamQuantity();
      generated->setIdentity(q->getLabel(), moduleStyleName(w->model),
                             appearance);
      knob = generated;
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
    if (auto* mapped = dynamic_cast<MappedParam<ParametricKnob>*>(knob))
      mapped->caption = caption;
    controls->addChild(knob);
    parameters.push_back({id, index, knob});
    layoutControls();
    saveGroup();
  }
  void restore(json_t* root) {
    namedTopologyReady = false;
    DEFER({ resetNameTopology(); });
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
      if (port) {
        auto* route = json_object_get(item, "route");
        auto* bridge = json_object_get(item, "bridge");
        auto* gain = json_object_get(item, "gain");
        expose(w, port, json_is_integer(route) ? json_integer_value(route) : -1,
               json_is_integer(bridge) ? json_integer_value(bridge) : -1,
               json_is_number(gain) ? json_number_value(gain) : 1.f);
      }
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
    repairSeparatedRows();
    pack();
    if (json_is_true(json_object_get(root, "expanded"))) revealMembers(false);
    saveGroup();
    status = expanded ? "Modules expanded — abstraction retained"
                      : std::to_string(members.size()) +
                            " modules running behind this panel";
    if (expanded && json_is_object(json_object_get(root, "editing")))
      resumeEditSession(json_object_get(root, "editing"));
  }
  app::PortWidget* originalPort(const Port& p) {
    auto* w = find(p.id);
    return w ? (p.input ? w->getInput(p.portId) : w->getOutput(p.portId))
             : nullptr;
  }
  app::PortWidget* findPort(const Port& p) {
    return p.route >= 0 ? getInput(Module::inputBase + p.route)
                        : originalPort(p);
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
    if (together) compactMembers();
    auto* rack = homeRack;
    for (const auto& p : ports)
      if (auto* source = originalPort(p)) source->box.pos = p.originalPosition;
    std::vector<Member*> ordered;
    for (auto& member : members)
      if (find(member.id)) ordered.push_back(&member);
    std::sort(ordered.begin(), ordered.end(), [](Member* a, Member* b) {
      return a->position.y == b->position.y ? a->position.x < b->position.x
                                            : a->position.y < b->position.y;
    });
    Vec minimum(INFINITY, INFINITY), maximum(-INFINITY, -INFINITY);
    for (auto* member : ordered) {
      minimum.x = std::min(minimum.x, member->position.x);
      minimum.y = std::min(minimum.y, member->position.y);
      maximum.x =
          std::max(maximum.x, member->position.x + member->widget->box.size.x);
      maximum.y =
          std::max(maximum.y, member->position.y + member->widget->box.size.y);
    }
    Vec cursor(std::ceil(box.getRight() / RACK_GRID_WIDTH) * RACK_GRID_WIDTH,
               box.pos.y);
    if (together) {
      bool blocked;
      do {
        blocked = false;
        Rect candidate(cursor, maximum - minimum);
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
      registered->box.pos =
          together ? cursor + member->position - minimum : member->position;
      member->position = registered->box.pos;
      rack->select(registered, true);
    }
    expanded = true;
    syncInputCables(true);
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
    for (auto& p : ports) detachInput(p);
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
  void expose(app::ModuleWidget* w, app::PortWidget* source, int route = -1,
              int64_t bridge = -1, float gain = 1) {
    for (const auto& p : ports)
      if (p.id == w->module->id &&
          p.input == (source->type == engine::Port::INPUT) &&
          p.portId == source->portId)
        return;
    // Let the original panel establish its own layout before we relocate any
    // native endpoint. Panels such as Voly Pector cache child positions on
    // their first step; caching our collapsed coordinates creates ghost jacks
    // when those originals are shown again.
    bool first = true;
    for (const auto& p : ports)
      if (p.id == w->module->id) first = false;
    if (first) {
      const auto type = source->type;
      const int index = source->portId;
      w->step();
      source = type == engine::Port::INPUT ? w->getInput(index)
                                           : w->getOutput(index);
      if (!source) return;
    }
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
    if (face->input) attachInput(ports.back(), route, bridge, gain);
    layoutControls();
    saveGroup();
  }
  bool boundaryPort(app::PortWidget* port) const {
    ScopedSubviewRack mainScope(homeRack);
    for (const auto& p : ports)
      if (p.route >= 0 && p.id == port->module->id &&
          p.portId == port->portId && port->type == engine::Port::INPUT) {
        auto* proxy =
            const_cast<Beekapoo*>(this)->getInput(Module::inputBase + p.route);
        for (auto* cable : homeRack->getCompleteCables())
          if (cable->cable->inputModule == module &&
              cable->cable->inputId == proxy->portId &&
              !contains(cable->cable->outputModule))
            return true;
      }
    for (auto* cable : homeRack->getCompleteCables())
      if (!inputBridge(cable->cable) &&
          (cable->inputPort == port || cable->outputPort == port) &&
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
      if (originalPort(*it) == source) {
        source->box.pos = it->originalPosition;
        detachInput(*it);
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
  void finishAction(GroupHistory* action,
                    history::ComplexAction* parent = nullptr) {
    refreshGeneratedName();
    action->newModuleJ = toJson();
    action->after.capture();
    if (parent)
      parent->push(action);
    else
      APP->history->push(action);
  }
  void exposeSelectionDefaults();
  void groupAction(bool collapsing, bool exposeDefaults = false,
                   history::ComplexAction* parent = nullptr) {
    if (!module || (!collapsing && (members.empty() || expanded)) ||
        (collapsing && !members.empty() && !expanded))
      return;
    auto* action =
        beginAction(collapsing ? (members.empty() ? "abstract modules"
                                                  : "collapse abstraction")
                               : "expand abstraction");
    bool creating = collapsing && members.empty();
    if (collapsing)
      collapse();
    else
      expand();
    if (collapsing && members.empty()) {
      delete action;
      return;
    }
    if (creating && exposeDefaults) exposeSelectionDefaults();
    finishAction(action, parent);
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
      if (inputBridge(cable->cable)) continue;
      bool in = contains(cable->cable->inputModule),
           out = contains(cable->cable->outputModule);
      if (in == out) continue;
      auto* port = in ? cable->inputPort : cable->outputPort;
      if (!exposed(port)) expose(find(port->module->id), port);
    }
  }
  bool canAbsorb(app::ModuleWidget* w) const {
    return module && w && w->module && w != this && w->visible &&
           !contains(w->module) && !ownedElsewhere(w->module) &&
           !w->module->leftExpander.producerMessage &&
           !w->module->rightExpander.producerMessage;
  }
  bool canAddSelection() const {
    for (auto* w : homeRack->getSelected())
      if (canAbsorb(w)) return true;
    return false;
  }
  void mergeSelection();
  void addSelection(history::ComplexAction* parent = nullptr,
                    bool preservePanelRoutes = false) {
    if (!module) return;
    std::vector<app::ModuleWidget*> added;
    for (auto* w : homeRack->getSelected())
      if (canAbsorb(w)) {
        if (dynamic_cast<Beekapoo*>(w)) {
          mergeSelection();
          return;
        }
        added.push_back(w);
      }
    if (added.empty()) return;
    if (members.empty()) {
      groupAction(true, true, parent);
      return;
    }
    auto* action = beginAction("add selected modules to abstraction");
    // Capture the boundary before membership changes. Ports that become
    // entirely internal must stop aliasing their original jacks on the panel.
    std::set<std::pair<int64_t, int>> oldInputs, oldOutputs;
    for (auto* cw : homeRack->getCompleteCables()) {
      auto* c = cw->cable;
      if (inputBridge(c)) continue;
      if (c->inputModule == module) {
        for (const auto& p : ports)
          if (p.route >= 0 && c->inputId == Module::inputBase + p.route)
            oldInputs.insert({p.id, p.portId});
        continue;
      }
      bool in = contains(c->inputModule), out = contains(c->outputModule);
      if (in == out) continue;
      (in ? oldInputs : oldOutputs)
          .insert({(in ? c->inputModule : c->outputModule)->id,
                   in ? c->inputId : c->outputId});
    }
    closeView();
    bool wasExpanded = expanded;
    std::set<int64_t> addedIds;
    for (auto* registered : added) {
      auto* holder = dynamic_cast<Holder*>(registered);
      auto* w = holder ? holder->original : registered;
      if (!w || !w->module) continue;
      addedIds.insert(w->module->id);
      members.push_back({w->module->id, registered->box.pos, w, holder});
    }
    // An incoming source that has just joined is now an internal wire.
    // Keep remaining outside sources on the attenuated input, summed by Rack.
    std::set<std::pair<int64_t, int>> internalizedInputs;
    for (auto& p : ports)
      if (p.route >= 0 && !preservePanelRoutes) {
        auto* target = originalPort(p);
        auto* proxy = findPort(p);
        for (auto* cw : homeRack->getCompleteCables())
          if (cw->cable->inputModule == module &&
              cw->cable->inputId == proxy->portId &&
              addedIds.count(cw->cable->outputModule->id)) {
            rewireInput(cw, target);
            internalizedInputs.insert({p.id, p.portId});
          }
      }
    for (auto* cw : homeRack->getCompleteCables())
      if (contains(cw->cable->inputModule) &&
          contains(cw->cable->outputModule) &&
          (addedIds.count(cw->cable->inputModule->id) ||
           addedIds.count(cw->cable->outputModule->id))) {
        panelCableIds.erase(cw->cable->id);
        internalCableIds.insert(cw->cable->id);
      }
    for (auto it = ports.begin(); it != ports.end();) {
      auto* source = findPort(*it);
      const auto& previous = it->input ? oldInputs : oldOutputs;
      bool affected = previous.count({it->id, it->portId}) ||
                      internalizedInputs.count({it->id, it->portId});
      bool outside = source && boundaryPort(source);
      if (it->route >= 0 && source) {
        outside = false;
        for (auto* cw : homeRack->getCompleteCables())
          if (cw->cable->inputModule == module &&
              cw->cable->inputId == source->portId)
            outside |= !contains(cw->cable->outputModule);
      }
      if (source && affected && !outside && !preservePanelRoutes) {
        if (auto* original = originalPort(*it))
          original->box.pos = it->originalPosition;
        detachInput(*it);
        controls->removeChild(it->face);
        delete it->face;
        it = ports.erase(it);
      } else
        ++it;
    }
    syncBoundaryPorts();
    compactMembers();
    pack();
    layoutControls();
    if (wasExpanded) revealMembers(true);
    saveGroup();
    finishAction(action, parent);
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
        detachInput(*it);
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
    compactMembers();
    saveGroup();
    finishAction(action);
  }
  void parameterAction(int index, bool recordHistory,
                       history::ComplexAction* parent = nullptr) {
    if (!module || members.empty() || index < 0 || index >= 6) return;
    const int operation = index % 3;
    const bool all = index >= 3;
    auto* batch =
        recordHistory || parent ? new history::ComplexAction : nullptr;
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
      else if (parent)
        parent->push(batch);
      else
        APP->history->push(batch);
    }
  }
  void onHoverKey(const HoverKeyEvent& e) override {
    if (module && e.action == GLFW_PRESS && !(e.mods & RACK_MOD_MASK) &&
        e.key == GLFW_KEY_E && !APP->scene->browser->visible &&
        !dynamic_cast<ui::TextField*>(APP->event->getSelectedWidget())) {
      if (!editSession) editSubpatch();
      e.consume(this);
      return;
    }
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
      if (homeRack->isSelected(this))
        duplicateBeekapooSelection(e.mods & GLFW_MOD_SHIFT);
      else
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
        if (auto* source = originalPort(p))
          p.originalPosition = source->box.pos;
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
    compactMembers();
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
    applyTheme();
    if (!module) {
      ModuleWidget::step();
      return;
    }

    updateSectionAddButtons(homeRack->getMousePos() -
                            getRelativeOffset(Vec(), homeRack));
    generateOnBrowserAdd();
    addSelectionButton->setAvailable(canAddSelection());
    addSelectionButton->box = logoTarget();
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
      if (actions & (1u << 6)) regeneratePatch();
      if (actions & (1u << 7)) growPatch();
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
    if (++appearancePoll % 8 == 0) {
      if (refreshAppearance()) layoutControls();
      refreshGeneratedName();
    }
    if (expanded) {
      for (auto* cw : homeRack->getCompleteCables()) {
        if (discardInvalidBridge(cw)) continue;
        if (inputBridge(cw->cable)) {
          cw->visible = false;
          showPlugs(cw, false);
        }
      }
      syncBoundaryPorts();
      syncInputCables(true);
      for (auto& p : ports)
        if (auto* source = originalPort(p))
          p.originalPosition = source->box.pos;
      if (syncMemberPositions()) saveGroup();
      ModuleWidget::step();
      return;
    }
    syncInputCables(false);
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
      if (discardInvalidBridge(cable)) continue;
      auto id = cable->cable->id;
      currentCables.insert(id);
      if (inputBridge(cable->cable)) {
        cable->visible = false;
        showPlugs(cable, false);
        continue;
      }
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
  Rect logoTarget() {
    bool unnamed = module ? getModule<Module>()->getPatchName().empty()
                          : browserName.empty();
    return box.size.x <= 90 && unnamed
               ? Rect(Vec(6, 5), Vec(box.size.x - 16, headerArtworkArea.size.y))
               : headerLogoArea;
  }
  void draw(const DrawArgs& args) override {
    ModuleWidget::draw(args);
    auto font = computerscare::type::font();
    if (titleFrame && titleFrame->visible) {
      Rect area = headerArtworkArea;
      std::string name =
          module ? getModule<Module>()->displayPatchName(appearance.modules)
                 : titleCase(browserName, appearance.modules);
      bool logoOnly = box.size.x <= 90 && name.empty();
      if (name.empty() && !logoOnly) name = "Beekapoo";
      name = titleText.get(name, appearance.modules);
      Rect logoArea = logoTarget();
      auto drawHeaderArt = [&](widget::SvgWidget& svg, Rect target) {
        if (!svg.svg || target.size.x <= 0 || target.size.y <= 0) return;
        Rect ink = &svg == &titleArtwork ? titleInk : Rect(Vec(), svg.box.size);
        float scale =
            std::min(target.size.x / ink.size.x, target.size.y / ink.size.y);
        nvgSave(args.vg);
        nvgTranslate(args.vg,
                     (&svg != &titleArtwork
                          ? target.getRight() - ink.size.x * scale
                          : target.getCenter().x - ink.size.x * scale * .5f),
                     target.getCenter().y - ink.size.y * scale * .5f);
        nvgScale(args.vg, scale, scale);
        nvgTranslate(args.vg, -ink.pos.x, -ink.pos.y);
        svg.draw(args);
        nvgRestore(args.vg);
      };
      if (!name.empty() && font) {
        Rect textArea = area;
        nvgSave(args.vg);
        nvgIntersectScissor(args.vg, textArea.pos.x, textArea.pos.y,
                            textArea.size.x, textArea.size.y);
        nvgFontFaceId(args.vg, computerscare::type::handle(
                                   computerscare::type::Face::Display));
        nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
        float bounds[4];
        for (int halfPoints = 48; halfPoints >= 8; --halfPoints) {
          nvgFontSize(args.vg, halfPoints * .5f);
          bool wordsFit = true;
          float widestWord = 0;
          for (size_t start = name.find_first_not_of(" \t\r\n");
               start != std::string::npos;) {
            size_t end = name.find_first_of(" \t\r\n", start);
            if (end == std::string::npos) end = name.size();
            widestWord = std::max(
                widestWord, nvgTextBounds(args.vg, 0, 0, name.c_str() + start,
                                          name.c_str() + end, nullptr));
            if (widestWord > textArea.size.x - 1) wordsFit = false;
            start = name.find_first_not_of(" \t\r\n", end);
          }
          nvgTextBoxBounds(args.vg, 0, 0, textArea.size.x, name.c_str(),
                           nullptr, bounds);
          if (wordsFit && bounds[3] - bounds[1] <= textArea.size.y - 2) break;
          // The smallest header can need less than 4 pt. Never let NanoVG
          // split a long name in the middle of a word at the size floor.
          if (halfPoints == 8 && !wordsFit && widestWord > 0)
            nvgFontSize(args.vg,
                        4.f * std::max(1.f, textArea.size.x - 1) / widestWord);
        }
        nvgFillColor(args.vg,
                     darkPanel() ? nvgRGB(235, 235, 235) : nvgRGB(25, 25, 25));
        nvgTextBox(args.vg, textArea.pos.x, textArea.pos.y + 1, textArea.size.x,
                   name.c_str(), nullptr);
        nvgRestore(args.vg);
      } else if (!logoOnly)
        drawHeaderArt(titleArtwork, area);
      int pose = addSelectionButton->pose();
      drawHeaderArt(pose ? addSelectionButton->jaws[pose - 1] : logoArtwork,
                    logoArea);
    }
    if (!font) return;
    nvgFontFaceId(args.vg, font->handle);
    nvgFontSize(args.vg, 10);
    nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);
    for (const auto& h : headings) {
      nvgFillColor(args.vg, (h.output || darkPanel()) ? nvgRGB(231, 231, 231)
                                                      : nvgRGB(56, 56, 56));
      nvgText(args.vg, h.pos.x, h.pos.y,
              h.rendered.get(h.text, appearance.modules).c_str(), nullptr);
    }
    nvgFontSize(args.vg, layoutTier >= 2 ? 9 : 7);
    for (const auto& label : controlLabels) {
      if (label.width <= 0) continue;
      float height = label.maxHeight > 0 ? label.maxHeight : 26;
      float left = label.pos.x -
                   (label.align == NVG_ALIGN_CENTER ? label.width * .5f : 0);
      float top =
          label.maxHeight > 0 ? label.pos.y : label.pos.y - height * .5f;
      int preferred =
          int(std::max(8.f, label.fontSize > 0 ? label.fontSize : 14.f) * 2);
      nvgSave(args.vg);
      nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
      if (!label.prepared) {
        for (const auto& original :
             labelAlternatives(label.text, label.required)) {
          auto text = computerscare::type::alternates(
              labelCase(original, appearance.modules), appearance.modules);
          int minimum = original == label.text &&
                                abbreviatedLabel(label.text) != label.text
                            ? std::max(20, preferred - 6)
                            : (label.required ? 13 : 16);
          for (int halfPoints = preferred; halfPoints >= minimum;
               --halfPoints) {
            label.preparedSize = halfPoints * .5f;
            nvgFontSize(args.vg, label.preparedSize);
            auto measure = [&](const std::string& text) {
              return nvgTextBounds(args.vg, 0, 0, text.c_str(), nullptr,
                                   nullptr);
            };
            label.lines = wholeWordLines(text, label.width - 2, measure);
            nvgTextMetrics(args.vg, nullptr, nullptr, &label.lineHeight);
            label.lineHeight *= 1.02f;
            bool wordsFit = true;
            for (const auto& line : label.lines)
              wordsFit &= measure(line) <= label.width - 2;
            label.readable =
                wordsFit && label.lines.size() * label.lineHeight <= height;
            if (label.readable) break;
          }
          if (label.readable) break;
        }
        if (label.required && !label.readable) {
          // Even unusual third-party labels retain a visible identifier.
          // Ordinary layouts reserve >=12 px for it; this is only a last
          // resort for pathological names or an exceptionally dense panel.
          auto choices = labelAlternatives(label.text, true);
          auto text = choices.empty() ? std::string("Port") : choices.back();
          text = computerscare::type::alternates(
              labelCase(text, appearance.modules), appearance.modules);
          nvgFontSize(args.vg, 8);
          float width =
              nvgTextBounds(args.vg, 0, 0, text.c_str(), nullptr, nullptr);
          nvgTextMetrics(args.vg, nullptr, nullptr, &label.lineHeight);
          label.preparedSize =
              8.f *
              std::min(
                  1.f,
                  std::min(
                      std::max(1.f, label.width - 2) / std::max(1.f, width),
                      height / std::max(1.f, label.lineHeight * 1.02f)));
          label.lines = {text};
          label.lineHeight = height;
          label.readable = true;
        }
        label.prepared = true;
      }
      if (!label.readable) {
        nvgRestore(args.vg);
        continue;
      }
      nvgFontSize(args.vg, label.preparedSize);
      nvgIntersectScissor(args.vg, left, top, label.width, height);
      nvgFillColor(args.vg, (label.output || darkPanel())
                                ? nvgRGB(236, 236, 236)
                                : nvgRGB(41, 41, 41));
      nvgTextAlign(args.vg, label.align | NVG_ALIGN_TOP);
      float x = label.align == NVG_ALIGN_CENTER ? label.pos.x : left + 1;
      for (size_t line = 0; line < label.lines.size(); ++line)
        nvgText(args.vg, x, top + line * label.lineHeight,
                label.lines[line].c_str(), nullptr);
      nvgRestore(args.vg);
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

    renameMenu(menu);
    themeMenu(menu);
    auto* amount = new ui::Slider;
    amount->quantity =
        getModule<Module>()->getParamQuantity(Module::WIGGLE_AMOUNT);
    amount->box.size = Vec(200, 24);
    menu->addChild(amount);

    menu->addChild(createMenuItem(
        "Save Beek…", "", [this] { saveBeekDialog(); }, members.empty()));
    menu->addChild(
        createMenuItem("Load Beek…", "", [this] { loadBeekDialog(); }));
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
                                    [this] { groupAction(true, true); }));
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
  // Anchor the start of the released block near the owner. Centering a
  // wide subpatch could put its first modules several screens to the left.
  Vec desired = group->box.pos;
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
#include "BeekapooEjectGains.hpp"
void Beekapoo::ejectAction(history::ComplexAction* parentAction,
                           bool preserveGains) {
  if (!module) return;
  EjectedInputGains gains(this, preserveGains);
  if (!gains.routes.empty() && !gains.model) {
    status = "Install VCV Free to preserve input attenuation when ejecting";
    return;
  }
  auto* change = beginAction("eject subpatch");
  ejecting = true;
  closeView();
  repairSeparatedRows();
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
  if (minimum.isFinite()) {
    Vec origin =
        closestEjectPosition(this, maximum - minimum + Vec(gains.width(), 0));
    gains.position = origin + Vec(maximum.x - minimum.x, 0);
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
  gains.install(this, batch);
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
  if (parentAction)
    parentAction->push(batch);
  else
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
          dynamic_cast<Beekapoo*>(APP->scene->rack->getModule(moduleId))) {
    group->resetNameTopology();
    group->focusEditedMembers();
  }
}
void GroupHistory::redo() {
  history::ModuleChange::redo();
  if (auto* group =
          dynamic_cast<Beekapoo*>(APP->scene->rack->getModule(moduleId)))
    group->step();
  after.restore();
  if (auto* group =
          dynamic_cast<Beekapoo*>(APP->scene->rack->getModule(moduleId))) {
    group->resetNameTopology();
    group->focusEditedMembers();
  }
}
void TopologyHistory::resetName() {
  if (auto* group = dynamic_cast<Beekapoo*>(APP->scene->rack->getModule(owner)))
    group->resetNameTopology();
}
#include "BeekapooExposure.hpp"
#include "BeekapooGenerator.hpp"
#include "BeekapooGrowth.hpp"
#include "BeekapooImportDialog.hpp"
#include "BeekapooInputRoutes.hpp"
#include "BeekapooNameEditor.hpp"
#include "BeekapooPatchFile.hpp"
#include "BeekapooPortable.hpp"
#include "BeekapooRackEdit.hpp"
#include "BeekapooRackHooks.hpp"
#include "BeekapooSectionAdd.hpp"
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
