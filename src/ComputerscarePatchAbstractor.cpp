#include <atomic>
#include <mutex>

#include "Computerscare.hpp"
#include "Phlooper/Block.hpp"
#include "Phlooper/Dropdown.hpp"

// Engine modules and cables stay in Rack; only their UI layout is grouped.
namespace patchabstractor {
struct SubpatchNativeRack;
// Temporarily lend the existing Rack's public containers to the subpatch UI.
// No RackWidget construction or private SDK entry points are required.
struct ScopedSubviewRack {
  SubpatchNativeRack* previous;
  explicit ScopedSubviewRack(SubpatchNativeRack* rack);
  explicit ScopedSubviewRack(app::RackWidget* rack);
  ~ScopedSubviewRack();
};
struct Module : engine::Module {
  std::mutex stateMutex;
  json_t* group = nullptr;
  bool restorePending = false;
  enum { WIGGLE_AMOUNT, SHOW_CABLE_OPACITY, PARAM_COUNT };
  dsp::SchmittTrigger actionTriggers[6];
  std::atomic<unsigned> pendingActions{0};
  std::atomic<unsigned> pendingHistory{0};
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
  void onRandomize(const RandomizeEvent&) override {
    pendingHistory.fetch_or(1u << 1, std::memory_order_relaxed);
    pendingActions.fetch_or(1u << 1, std::memory_order_relaxed);
  }

  ~Module() override { json_decref(group); }
  void saveGroup(json_t* next) {
    std::lock_guard<std::mutex> lock(stateMutex);
    json_decref(group);
    group = next;
  }
  json_t* dataToJson() override {
    std::lock_guard<std::mutex> lock(stateMutex);
    auto* root = json_object();
    if (group) json_object_set(root, "group", group);
    return root;
  }
  void dataFromJson(json_t* root) override {
    std::lock_guard<std::mutex> lock(stateMutex);
    json_decref(group);
    group = json_deep_copy(json_object_get(root, "group"));
    restorePending = true;
  }
  json_t* takeRestore() {
    std::lock_guard<std::mutex> lock(stateMutex);
    if (!restorePending) return nullptr;
    restorePending = false;
    return group ? json_deep_copy(group) : json_object();
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
struct Abstractor;
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
  void draw(const DrawArgs& args) override {
    showLabel = icon < 0;
    phlooper::WarpedDropdown::draw(args);
    if (icon < 0) return;
    Vec c = box.size * .5f + (isPressed() ? Vec(2, 1) : Vec(-1, -1));
    nvgSave(args.vg);
    nvgTranslate(args.vg, c.x, c.y);
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
      if (action) action();
      e.consume(this);
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
    text = caption();
    ui::Tooltip::step();
  }
};
template <class Base>
struct MappedParam : Base {
  std::function<std::string()> caption;
  MappingTooltip* tip = nullptr;
  void clearTip() {
    if (tip) {
      APP->scene->removeChild(tip);
      delete tip;
      tip = nullptr;
    }
  }
  ~MappedParam() override { clearTip(); }
  void onEnter(const widget::Widget::EnterEvent&) override {
    if (!settings::tooltips || tip) return;
    tip = new MappingTooltip;
    tip->caption = caption;
    tip->box.pos = this->getAbsoluteOffset(this->box.size);
    APP->scene->addChild(tip);
  }
  void onLeave(const widget::Widget::LeaveEvent&) override { clearTip(); }
};
struct PortFace : widget::OpaqueWidget {
  std::function<std::string()> caption;
  MappingTooltip* tip = nullptr;
  void clearTip() {
    if (tip) {
      APP->scene->removeChild(tip);
      delete tip;
      tip = nullptr;
    }
  }
  ~PortFace() override { clearTip(); }
  WeakPtr<app::PortWidget> source;
  void draw(const DrawArgs& args) override { source->draw(args); }
  void drawLayer(const DrawArgs& args, int layer) override {
    if (source) source->drawLayer(args, layer);
  }
  void onButton(const ButtonEvent& e) override {
    if (source) source->onButton(e);
  }
  void onDragDrop(const DragDropEvent& e) override {
    if (source) source->onDragDrop(e);
  }
  void onDragEnter(const DragEnterEvent& e) override {
    if (source) source->onDragEnter(e);
  }
  void onDragLeave(const DragLeaveEvent& e) override {
    if (source) source->onDragLeave(e);
  }
  void onEnter(const EnterEvent& e) override {
    if (!settings::tooltips || tip) return;
    tip = new MappingTooltip;
    tip->caption = caption;
    tip->box.pos = getAbsoluteOffset(box.size);
    APP->scene->addChild(tip);
  }
  void onLeave(const LeaveEvent& e) override { clearTip(); }
};
struct Abstractor : app::ModuleWidget {
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
  bool expanded = false;
  float actionPulse[6]{};
  std::vector<Port> ports;
  widget::Widget* controls = nullptr;
  struct Parameter {
    int64_t id;
    int paramId;
    app::ParamWidget* knob;
  };
  std::vector<Parameter> parameters;
  int viewMode = 0;
  ActionButton* viewButton = nullptr;
  std::vector<ActionButton*> actionButtons;
  std::vector<ActionButton*> managementButtons;
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
  void closeView() {
    if (subview) {
      subview->setVisible(false);
      subview->requestDelete();
      subview = nullptr;
    }
  }
  std::string status = "Select modules, then right-click here";
  Abstractor(Module* m) {
    homeRack = APP->scene->rack;
    setModule(m);
    auto* panel = new ComputerscareSVGPanel;
    panel->setBackground(APP->window->loadSvg(asset::plugin(
        pluginInstance, "res/panels/ComputerscarePatchAbstractorPanel.svg")));
    setPanel(panel);
    frame = new phlooper::WarpedBlock;
    frame->configure(Rect(Vec(9, 140), Vec(342, 213)), 5, 1.8f, 719,
                     nvgRGB(198, 206, 202));
    addChild(frame);
    inputFrame = new phlooper::WarpedBlock;
    inputFrame->configure(Rect(Vec(9, 253), Vec(165, 100)), 5, 2.f, 723,
                          nvgRGB(198, 206, 202));
    addChild(inputFrame);
    outputFrame = new phlooper::WarpedBlock;
    outputFrame->configure(Rect(Vec(186, 253), Vec(165, 100)), 5, 2.f, 727,
                           nvgRGB(84, 101, 96));
    addChild(outputFrame);
    auto* show = new ActionButton;
    show->text = "Show";
    show->configure(52, 24, 3, 2, .8f, 713);
    show->box.pos = Vec(15, 39);
    show->action = [this] { showView(); };
    show->pressed = [this] { return bool(subview); };
    addChild(show);
    managementButtons.push_back(show);
    auto* reveal = new ActionButton;
    reveal->configure(77, 24, 3, 2, .8f, 714);
    reveal->box.pos = Vec(72, 39);
    reveal->caption = [this] {
      return members.empty() ? "Abstract" : expanded ? "Collapse" : "Expand";
    };
    reveal->pressed = [this] { return expanded; };
    reveal->action = [this] { groupAction(members.empty() || expanded); };
    addChild(reveal);
    managementButtons.push_back(reveal);
    auto* end = new ActionButton;
    end->text = "End abstraction";
    end->configure(131, 24, 3, 2, .8f, 715);
    end->box.pos = Vec(154, 39);
    end->action = [this] { endAction(); };
    addChild(end);
    managementButtons.push_back(end);
    const char* captions[] = {"Wiggle", "Randomize", "Initialize"};
    for (int row = 0; row < 2; ++row)
      for (int col = 0; col < 3; ++col) {
        int index = row * 3 + col;
        float x = 55 + col * 80, y = 73 + row * 28;
        auto* button = new ActionButton;
        button->text = captions[col];
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
    viewButton = new ActionButton;
    viewButton->caption = [this] {
      return viewMode == 0 ? "Full" : viewMode == 1 ? "Med" : "Small";
    };
    viewButton->configure(44, 24, 3, 2, .8f, 903);
    viewButton->action = [this] {
      viewMode = (viewMode + 1) % 3;
      layoutControls();
      saveGroup();
    };
    addChild(viewButton);
    controls = new widget::Widget;
    addChild(controls);
    layoutControls();
  }
  ~Abstractor() override { endAbstraction(); }
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
    if (members.empty()) {
      m->saveGroup(nullptr);
      return;
    }
    auto* root = json_object();
    json_object_set_new(root, "view", json_integer(viewMode));
    json_object_set_new(root, "owner", json_integer(m->id));
    json_object_set_new(root, "expanded", json_boolean(expanded));
    auto* list = json_array();
    for (const auto& member : members)
      json_array_append_new(
          list,
          json_pack("{s:I,s:f,s:f}", "id", (json_int_t)member.id, "x",
                    (double)member.position.x, "y", (double)member.position.y));
    json_object_set_new(root, "members", list);
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
  void layoutControls() {
    headings.clear();
    float cell = viewMode == 0 ? 82.f : viewMode == 1 ? 66.f : 31.f;
    if (viewMode == 0) {
      for (const auto& p : parameters)
        if (auto* w = find(p.id))
          if (auto* q = w->module->getParamQuantity(p.paramId))
            cell = std::max(cell, 34.f + q->getLabel().size() * 5.5f);
      for (const auto& p : ports)
        if (auto* source = findPort(p))
          if (auto* info = source->getPortInfo())
            cell = std::max(cell, 34.f + info->getName().size() * 5.5f);
    }
    std::vector<widget::Widget*> items[3];
    std::vector<int64_t> ids[3];
    for (const auto& m : members) {
      std::vector<const Parameter*> params;
      std::vector<const Port*> jacks;
      for (const auto& p : parameters)
        if (p.id == m.id) params.push_back(&p);
      for (const auto& p : ports)
        if (p.id == m.id) jacks.push_back(&p);
      std::sort(params.begin(), params.end(),
                [](const Parameter* a, const Parameter* b) {
                  return a->paramId < b->paramId;
                });
      std::sort(jacks.begin(), jacks.end(), [](const Port* a, const Port* b) {
        return a->portId < b->portId;
      });
      for (auto* p : params) {
        items[0].push_back(p->knob);
        ids[0].push_back(m.id);
      }
      for (auto* p : jacks) {
        int k = p->input ? 1 : 2;
        items[k].push_back(p->face);
        ids[k].push_back(m.id);
      }
    }
    int pc = std::max(1, int((items[0].size() + 2) / 3));
    int ic = std::max(1, int((items[1].size() + 2) / 3));
    int oc = std::max(1, int((items[2].size() + 2) / 3));
    float width = std::ceil(std::max(viewMode == 2   ? 240.f
                                     : viewMode == 1 ? 270.f
                                                     : 360.f,
                                     std::max(pc, ic + oc) * cell + 42) /
                            15) *
                  15;
    bool changed = box.size.x != width;
    box.size.x = width;
    float split = 18 + ic * cell;
    for (int k = 0; k < 3; ++k) {
      int cols = k == 0   ? std::max(pc, int((width - 36) / cell))
                 : k == 1 ? ic
                          : oc;
      float start = k == 2 ? split + 12 : 18;
      for (size_t i = 0; i < items[k].size(); ++i) {
        Vec center(start + 12 + (i % cols) * cell,
                   (k == 0 ? 164.f : 277.f) + (i / cols) * 29);
        items[k][i]->box.pos = center - items[k][i]->box.size * .5f;
        if (viewMode == 0 && (i == 0 || ids[k][i] != ids[k][i - 1]))
          headings.push_back(
              {center + Vec(-10, -16), memberName(ids[k][i]), k == 2});
      }
    }
    frame->box = Rect(Vec(9, 140), Vec(width - 18, 105));
    inputFrame->box = Rect(Vec(9, 253), Vec(split - 12, 100));
    outputFrame->box = Rect(Vec(split + 3, 253), Vec(width - split - 12, 100));
    inputFrame->visible = !items[1].empty();
    outputFrame->visible = !items[2].empty();
    viewButton->box.pos = Vec(width - 51, 8);
    bool compact = viewMode != 0;
    for (size_t i = 0; i < actionButtons.size(); ++i) {
      int row = i / 3, col = i % 3;
      auto* button = actionButtons[i];
      button->icon = compact ? col : -1;
      button->box.pos =
          Vec(compact ? 42 + col * 48 : 55 + col * 80, 73 + row * 28);
      button->box.size = Vec(compact ? 25 : 56, 23);
      if (auto* port = getInput(i))
        port->box.pos =
            button->box.pos + Vec(compact ? 36 : 68, 11) - port->box.size * .5f;
    }
    if (auto* amount = getParam(Module::WIGGLE_AMOUNT))
      amount->box.pos = Vec(compact ? 209 : 328, 94) - amount->box.size * .5f;
    if (managementButtons.size() == 3) {
      managementButtons[2]->box.pos = Vec(154, 39);
      managementButtons[2]->box.size.x = compact ? 78 : 131;
      managementButtons[2]->text = compact ? "End group" : "End abstraction";
    }
    if (changed && parent == homeRack->getModuleContainer())
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
    viewMode =
        clamp(int(json_integer_value(json_object_get(root, "view"))), 0, 2);
    // Native duplicate/preset operations don't remap embedded module IDs.
    // Refuse a copied container's references instead of stealing its group.
    auto* owner = json_object_get(root, "owner");
    if (!json_is_integer(owner) || json_integer_value(owner) != module->id) {
      endAbstraction();
      return;
    }
    auto* list = json_object_get(root, "members");
    if (!json_is_array(list) || json_array_size(list) == 0) return;
    endAbstraction();
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
          dynamic_cast<Abstractor*>(w) || !w->visible ||
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
    // PlugWidget is intentionally opaque in the SDK. Find its public Widget
    // in Rack's plug container by identity instead of assuming its layout.
    for (auto* container : homeRack->children)
      for (auto* child : container->children)
        if (static_cast<void*>(child) == static_cast<void*>(cable->inputPlug) ||
            static_cast<void*>(child) == static_cast<void*>(cable->outputPlug))
          child->visible = visible;
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
    face->box.size = source->box.size;
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
    if (!source || !source->module) return;
    if (boundaryPort(source)) return;
    for (auto it = ports.begin(); it != ports.end(); ++it)
      if (findPort(*it) == source) {
        source->box.pos = it->originalPosition;
        controls->removeChild(it->face);
        delete it->face;
        ports.erase(it);
        layoutControls();
        saveGroup();
        return;
      }
    expose(find(source->module->id), source);
  }
  void toggleParameter(app::ParamWidget* source) {
    if (!source || !source->module) return;
    for (auto it = parameters.begin(); it != parameters.end(); ++it)
      if (it->id == source->module->id && it->paramId == source->paramId) {
        controls->removeChild(it->knob);
        delete it->knob;
        parameters.erase(it);
        layoutControls();
        saveGroup();
        return;
      }
    exposeParameter(source->module->id, source->paramId);
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
      if (auto* group = dynamic_cast<Abstractor*>(w))
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
  void addSelection() {
    if (!module || members.empty()) return;
    std::vector<app::ModuleWidget*> added;
    for (auto* w : homeRack->getSelected())
      if (w != this && w->visible && !contains(w->module) &&
          !ownedElsewhere(w->module) && !dynamic_cast<Abstractor*>(w) &&
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
    auto* action = beginAction("remove selected members from abstraction");
    closeView();
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
      expanded = false;
      controls->visible = true;
      status = "Abstraction ended — modules remain in rack";
    } else
      syncBoundaryPorts();
    layoutControls();
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
    auto change = [&](engine::ParamQuantity* q) {
      if (!q || !q->isBounded() ||
          (operation == 2 ? !q->resetEnabled : !q->randomizeEnabled))
        return;
      float before = q->getValue();
      if (operation == 2)
        q->reset();
      else if (operation == 1)
        q->randomize();
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
          auto* before = batch ? APP->engine->moduleToJson(w->module) : nullptr;
          if (operation == 1)
            APP->engine->randomizeModule(w->module);
          else
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
          for (auto* q : w->module->paramQuantities) change(q);
    } else {
      for (const auto& param : parameters)
        if (auto* w = find(param.id))
          change(w->module->getParamQuantity(param.paramId));
    }
    if (batch) {
      if (batch->isEmpty())
        delete batch;
      else
        APP->history->push(batch);
    }
  }
  void onHoverKey(const HoverKeyEvent& e) override {
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
      if (dynamic_cast<Abstractor*>(w) || ownedElsewhere(w->module) ||
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
  void step() override {
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
    if (members.empty()) {
      ModuleWidget::step();
      return;
    }
    // Deleting a hidden module through another tool fails open rather than
    // leaving a dangling proxy. Do this before stepping any mapped controls.
    for (const auto& member : members)
      if (!find(member.id)) {
        endAbstraction();
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
        Vec desired = controls->getRelativeOffset(p.face->box.pos, homeRack);
        Vec parentOrigin = source->parent->getRelativeOffset(Vec(), homeRack);
        source->box.pos = desired - parentOrigin;
      }
    }
    for (auto* cable : homeRack->getCompleteCables()) {
      if (contains(cable->cable->inputModule) &&
          contains(cable->cable->outputModule)) {
        if (cable->visible) {
          cable->visible = false;
          showPlugs(cable, false);
        }
      }
    }
    ModuleWidget::step();
  }
  void draw(const DrawArgs& args) override {
    nvgBeginPath(args.vg);
    nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
    nvgFillColor(args.vg, nvgRGB(198, 201, 199));
    nvgFill(args.vg);
    ModuleWidget::draw(args);
    auto font =
        APP->window->loadFont(asset::system("res/fonts/DejaVuSans.ttf"));
    nvgFontFaceId(args.vg, font->handle);
    nvgFontSize(args.vg, 10);
    nvgFillColor(args.vg, nvgRGB(30, 40, 35));
    nvgFontSize(args.vg, viewMode == 2 ? 7 : 10);
    if (viewMode != 2) nvgText(args.vg, 15, 367, status.c_str(), nullptr);
    nvgFontSize(args.vg, 8);
    if (viewMode != 2)
      nvgText(args.vg, viewMode == 0 ? 309 : 193, 119, "Amount", nullptr);
    if (expanded) {
      nvgFontSize(args.vg, 14);
      std::string message =
          std::to_string(members.size()) + " modules expanded in rack";
      nvgText(args.vg, 24, 110, message.c_str(), nullptr);
      return;
    }
    if (viewMode == 2) return;
    nvgFontSize(args.vg, 7);
    for (const auto& h : headings) {
      nvgFillColor(args.vg,
                   h.output ? nvgRGB(222, 234, 227) : nvgRGB(30, 40, 35));
      nvgText(args.vg, h.pos.x, h.pos.y, h.text.c_str(), nullptr);
    }
    nvgFontSize(args.vg, 8);
    nvgFillColor(args.vg, nvgRGB(30, 40, 35));
    for (size_t i = 0; i < parameters.size(); ++i) {
      const auto& p = parameters[i];
      auto* w = find(p.id);
      if (!w) continue;
      auto* q = w->module->getParamQuantity(p.paramId);
      if (!q) continue;
      auto pos = p.knob->box.getCenter() - Vec(12, 12);
      if (viewMode == 1)
        nvgTextBox(args.vg, pos.x + 29, pos.y + 6, 33, q->getLabel().c_str(),
                   nullptr);
      else
        nvgText(args.vg, pos.x + 29, pos.y + 13, q->getLabel().c_str(),
                nullptr);
    }
    for (const auto& p : ports)
      if (auto* source = findPort(p)) {
        auto* info = source->getPortInfo();
        std::string name;
        name += info ? info->getName() : "Port";
        nvgFillColor(args.vg,
                     p.input ? nvgRGB(30, 40, 35) : nvgRGB(222, 234, 227));
        if (viewMode == 1)
          nvgTextBox(args.vg, p.face->box.pos.x + 29, p.face->box.pos.y + 6, 33,
                     name.c_str(), nullptr);
        else
          nvgText(args.vg, p.face->box.pos.x + 29, p.face->box.pos.y + 13,
                  name.c_str(), nullptr);
      }
  }
  void appendContextMenu(ui::Menu* menu) override {
    if (!module) return;
    menu->addChild(new ui::MenuSeparator);

    menu->addChild(createMenuItem(
        "Show subpatch", "", [this] { showView(); }, members.empty()));
    menu->addChild(createMenuItem(
        members.empty() ? "Abstract selected modules" : "Collapse modules", "",
        [this] { groupAction(true); }, !members.empty() && !expanded));
    menu->addChild(createMenuItem(
        "Expand modules", "", [this] { groupAction(false); },
        members.empty() || expanded));
    menu->addChild(createMenuItem(
        "End abstraction", "", [this] { endAction(); }, members.empty()));
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
                  list->addChild(createMenuItem(
                      label, "", [this, w, port] { expose(w, port); }));
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
                      list->addChild(createMenuItem(
                          q->getLabel(), "",
                          [this, w, i] { exposeParameter(w->module->id, i); }));
                    }
                  }));
            }
        }));
  }
};
void GroupHistory::undo() {
  history::ModuleChange::undo();
  if (auto* group =
          dynamic_cast<Abstractor*>(APP->scene->rack->getModule(moduleId)))
    group->step();
  before.restore();
}
void GroupHistory::redo() {
  history::ModuleChange::redo();
  if (auto* group =
          dynamic_cast<Abstractor*>(APP->scene->rack->getModule(moduleId)))
    group->step();
  after.restore();
}
#include "PatchAbstractorSubview.hpp"
}  // namespace patchabstractor
Model* modelComputerscarePatchAbstractor =
    createModel<patchabstractor::Module, patchabstractor::Abstractor>(
        "computerscare-patch-abstractor");
