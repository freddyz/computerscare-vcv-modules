#pragma once
#include "VolyPectorLayout.hpp"

// UI-only controls address stored band/channel values directly. Existing
// single-view parameters and their mappings keep their original identities.
struct VolyMultiValueChange : history::ModuleAction {
  int address = 0;
  float before = 0, after = 0;
  void apply(float value) {
    if (auto* m = dynamic_cast<ComputerscareVolyPector*>(
            APP->engine->getModule(moduleId)))
      m->setMultiValue(address, value);
  }
  void undo() override { apply(before); }
  void redo() override { apply(after); }
};

struct VolyMultiKnob;
struct VolyMultiTooltip : ui::Tooltip {
  VolyMultiKnob* knob = nullptr;
  void step() override;
};

struct VolyMultiKnob : widget::OpaqueWidget {
  ComputerscareVolyPector* model = nullptr;
  int address = 0;
  float dragValue = 0, before = 0;
  ui::Tooltip* tooltip = nullptr;
  ComputerscareRoundKnob appearance;
  std::shared_ptr<Svg> enabledSvg, disabledSvg;
  bool inactive = false;
  SmallLetterDisplay lettering;
  float lastDrawnAngle = NAN;
  std::string label;
  VolyMultiKnob() {
    enabledSvg = APP->window->loadSvg(asset::plugin(
        pluginInstance, "res/components/computerscare-medium-small-knob.svg"));
    disabledSvg = APP->window->loadSvg(asset::plugin(
        pluginInstance,
        "res/components/computerscare-medium-small-knob-disabled.svg"));
    appearance.setSvg(enabledSvg);
    appearance.shadow->box.size = Vec();
    appearance.shadow->opacity = 0.f;
    lettering.fontSize = 16;
    lettering.letterSpacing = 1.6f;
  }
  ~VolyMultiKnob() override { clearTooltip(); }
  bool scaleControl() const {
    return address >= 256 && ((address - 256) / 16) % 2;
  }
  float minimum() const {
    return address < 256 ? (model->bipolarMainKnobs ? -10.f : 0.f)
                         : (scaleControl() ? -2.f : -10.f);
  }
  float maximum() const { return scaleControl() ? 2.f : 10.f; }
  float initial() const { return scaleControl() ? 1.f : 0.f; }
  std::string name() const {
    if (address < 256)
      return model->outputName(address / 16) + ", " +
             model->channelName(address % 16);
    int field = (address - 256) / 16;
    return (field < 2 ? model->outputName(address % 16)
                      : model->channelName(address % 16)) +
           (scaleControl() ? " Scale" : " Offset");
  }
  std::string tooltipText() {
    std::string text =
        name() + ": " + string::f("%.5g", model->multiValue(address));
    if (address >= 256 && !scaleControl()) text += " volts";
    int output =
        address < 256 ? address / 16 : (address < 288 ? address % 16 : -1);
    if (output >= 0) {
      std::string destination = volyPectorOutputDestinationText(
          getAncestorOfType<ModuleWidget>(),
          ComputerscareVolyPector::OUTPUT + output);
      if (!destination.empty()) text += "\n" + destination;
    }
    return text;
  }

  void record(float old) {
    float value = model->multiValue(address);
    if (old == value) return;
    auto* change = new VolyMultiValueChange;
    change->name = "change Voly Pector " + name();
    change->moduleId = model->id;
    change->address = address;
    change->before = old;
    change->after = value;
    APP->history->push(change);
  }
  void clearTooltip() {
    if (!tooltip) return;
    APP->scene->removeChild(tooltip);
    delete tooltip;
    tooltip = nullptr;
  }
  void step() override {
    if (!visible) clearTooltip();
    if (visible && model && address < 256) {
      bool candidate = model->mainValueInactive(address / 16, address % 16);
      if (candidate != inactive) {
        appearance.setSvg(candidate ? disabledSvg : enabledSvg);
        lastDrawnAngle = NAN;
        inactive = candidate;
      }
    }
    widget::OpaqueWidget::step();
  }
  void draw(const DrawArgs& args) override {
    float value = model->multiValue(address);
    float angle = math::rescale(value, minimum(), maximum(),
                                appearance.minAngle, appearance.maxAngle);
    if (angle != lastDrawnAngle) {
      auto center = appearance.sw->box.getCenter();
      appearance.tw->identity();
      appearance.tw->translate(center);
      appearance.tw->rotate(angle);
      appearance.tw->translate(center.neg());
      appearance.fb->setDirty();
      lastDrawnAngle = angle;
    }
    float ratio = box.size.x / appearance.box.size.x;
    auto nativeArgs = args;
    nativeArgs.clipBox.pos = nativeArgs.clipBox.pos.div(ratio);
    nativeArgs.clipBox.size = nativeArgs.clipBox.size.div(ratio);
    nvgSave(args.vg);
    nvgScale(args.vg, ratio, ratio);
    appearance.draw(nativeArgs);
    nvgRestore(args.vg);
  }
  void drawLabel(const DrawArgs& args) {
    if (label.empty()) return;
    float ratio = box.size.x / 24.f;
    lettering.value = label;
    lettering.textAlign = NVG_ALIGN_LEFT;
    lettering.breakRowWidth = 80.f;
    lettering.box.size = Vec(5, 10);
    nvgSave(args.vg);
    nvgTranslate(args.vg, box.pos.x - 4.5f * ratio, box.pos.y - 10.5f * ratio);
    nvgScale(args.vg, ratio, ratio);
    lettering.draw(args);
    nvgRestore(args.vg);
  }

  void onEnter(const EnterEvent& e) override {
    clearTooltip();
    if (!settings::tooltips) return;
    auto* hint = new VolyMultiTooltip;
    hint->knob = this;
    tooltip = hint;
    tooltip->text = tooltipText();
    APP->scene->addChild(tooltip);
    widget::OpaqueWidget::onEnter(e);
  }
  void onLeave(const LeaveEvent& e) override {
    clearTooltip();
    widget::OpaqueWidget::onLeave(e);
  }
  void onButton(const ButtonEvent& e) override {
    if (e.button == GLFW_MOUSE_BUTTON_LEFT) {
      e.consume(this);
      return;
    }
    if (e.button != GLFW_MOUSE_BUTTON_RIGHT || e.action != GLFW_PRESS) return;
    auto* menu = createMenu();
    struct Field : ui::TextField {
      ComputerscareVolyPector* model = nullptr;
      int address = 0;
      void onSelectKey(const SelectKeyEvent& e) override {
        if (e.action == GLFW_PRESS &&
            (e.key == GLFW_KEY_ENTER || e.key == GLFW_KEY_KP_ENTER)) {
          char* end = nullptr;
          float value = std::strtof(text.c_str(), &end);
          if (end != text.c_str() && *end == '\0' && std::isfinite(value)) {
            auto* change = new VolyMultiValueChange;
            change->name = "set Voly Pector value";
            change->moduleId = model->id;
            change->address = address;
            change->before = model->multiValue(address);
            model->setMultiValue(address, value);
            change->after = model->multiValue(address);
            APP->history->push(change);
            if (auto* overlay = getAncestorOfType<ui::MenuOverlay>())
              overlay->requestDelete();
          }
          e.consume(this);
          return;
        }
        ui::TextField::onSelectKey(e);
      }
    };
    menu->addChild(createMenuLabel(name()));
    auto* field = new Field;
    field->model = model;
    field->address = address;
    field->box.size.x = 150;
    field->text = string::f("%.6g", model->multiValue(address));
    menu->addChild(field);
    field->selectAll();
    APP->event->setSelectedWidget(field);
    e.consume(this);
  }
  void onDragStart(const DragStartEvent& e) override {
    if (e.button != GLFW_MOUSE_BUTTON_LEFT) return;
    before = dragValue = model->multiValue(address);
    clearTooltip();
    e.consume(this);
  }
  void onDragMove(const DragMoveEvent& e) override {
    if (e.button != GLFW_MOUSE_BUTTON_LEFT) return;
    float sensitivity = APP->window->getMods() & RACK_MOD_CTRL ? .1f : 1.f;
    dragValue = clamp(dragValue - e.mouseDelta.y * (maximum() - minimum()) *
                                      .003f * sensitivity,
                      minimum(), maximum());
    model->setMultiValue(address, dragValue);
    e.consume(this);
  }
  void onDragEnd(const DragEndEvent& e) override {
    if (e.button != GLFW_MOUSE_BUTTON_LEFT) return;
    record(before);
    e.consume(this);
  }
  void onDoubleClick(const DoubleClickEvent& e) override {
    float old = model->multiValue(address);
    model->setMultiValue(address, initial());
    record(old);
    // Rack sends drag updates after the second press, even with zero delta.
    // Continue from the reset value and avoid a duplicate undo on release.
    before = dragValue = model->multiValue(address);
    e.consume(this);
  }
};

inline void VolyMultiTooltip::step() {
  if (knob) text = knob->tooltipText();
  ui::Tooltip::step();
  if (knob) box.pos = knob->getAbsoluteOffset(knob->box.size).round();
  if (parent) box = box.nudge(parent->box.zeroPos());
}

struct VolyMultiPanel : widget::Widget {
  ComputerscareVolyPector* model = nullptr;
  std::array<VolyMultiKnob*, 320> knobs{};
  struct Block {
    Rect box;
    std::string title;
    int index;
    bool channels;
  };
  VolyPectorViewTitle titleArtwork;
  std::vector<Block> blocks;
  unsigned lastBands = 0, lastChannels = 0, lastConnected = 0;
  int lastPoly = -1;
  float width = 54;
  bool dense = false;
  bool showControls = false;
  explicit VolyMultiPanel(ComputerscareVolyPector* m) : model(m) {
    box.pos = Vec(0, 24);
    box.size = Vec(54, 264);
    if (!model) return;
    for (int i = 0; i < 320; ++i) {
      auto* knob = new VolyMultiKnob;
      knob->model = model;
      knob->address = i;
      if (knob->scaleControl())
        knob->appearance.setSvg(APP->window->loadSvg(asset::plugin(
            pluginInstance,
            "res/components/computerscare-small-knob-effed.svg")));
      knob->visible = false;
      knobs[i] = knob;
      addChild(knob);
    }
  }
  void layout() {
    visible = model && model->multiView();
    if (!visible) {
      width = 54;
      lastPoly = -1;
      return;
    }
    unsigned connected = 0;
    for (int i = 0; i < 16; ++i)
      if (model->outputs[i].isConnected()) connected |= 1u << i;
    int poly = clamp(model->polyChannels, 1, 16);
    if (lastBands == model->selectedBands &&
        lastChannels == model->selectedChannels && lastConnected == connected &&
        lastPoly == poly)
      return;
    lastBands = model->selectedBands;
    lastChannels = model->selectedChannels;
    lastConnected = connected;
    lastPoly = poly;
    for (auto* knob : knobs) {
      knob->visible = false;
      knob->clearTooltip();
    }
    blocks.clear();
    bool channels = model->channelViewActive();
    std::vector<int> groups, members;
    for (int i = 0; i < 16; ++i) {
      bool activeBand = !connected || (connected & (1u << i));
      if (channels) {
        if ((model->selectedChannels & (1u << i)) &&
            model->multiChannelVisible(i))
          groups.push_back(i);
        if (activeBand) members.push_back(i);
      } else {
        if ((model->selectedBands & (1u << i)) && model->multiBandVisible(i))
          groups.push_back(i);
        if (i < poly) members.push_back(i);
      }
    }
    width = box.size.x = 54;
    if (groups.empty() || members.empty()) return;
    auto geometry = computerscare::voly::layout::calculate(
        int(groups.size()), int(members.size()), width, box.size.y);
    if (!geometry.valid) return;
    showControls = geometry.controls;
    dense = !geometry.controls;
    int count = int(members.size());
    auto rect = [](const computerscare::voly::layout::Rect& value) {
      return Rect(Vec(value.x, value.y), Vec(value.width, value.height));
    };
    for (size_t row = 0; row < groups.size(); ++row) {
      int group = groups[row];
      const auto& division = geometry.groups[row];
      float y = division.bounds.y;
      std::string title = channels ? "Channel " + std::to_string(group + 1)
                                   : "Band " + std::string(1, 'A' + group);
      blocks.push_back(
          {Rect(Vec(1, y + 1), Vec(width - 2, geometry.divisionHeight - 2)),
           title, group, channels});
      for (int col = 0; col < count; ++col) {
        int address =
            channels ? members[col] * 16 + group : group * 16 + members[col];
        auto* knob = knobs[address];
        knob->visible = true;
        knob->label = channels ? std::string(1, 'A' + members[col])
                               : std::to_string(members[col] + 1);
        knob->box = rect(division.values[col]);
      }
      if (showControls) {
        auto* offset = knobs[256 + (channels ? 32 : 0) + group];
        auto* scale = knobs[272 + (channels ? 32 : 0) + group];
        offset->visible = scale->visible = true;
        offset->label.clear();
        scale->label.clear();
        offset->box = rect(division.offset);
        scale->box = rect(division.scale);
      }
    }
  }
  void draw(const DrawArgs& args) override {
    // Draw only the existing artwork and lettering, on the original panel.
    // No fill, frame, or rectangular backing for this view.
    for (const auto& block : blocks) {
      titleArtwork.previewMode = true;
      titleArtwork.previewChannelView = block.channels;
      titleArtwork.previewSelectedOutput = block.index;
      titleArtwork.previewSelectedChannel = block.index;
      titleArtwork.fontSize = dense ? 13.5f : 16.5f;
      if (auto font = titleArtwork.getFont()) {
        nvgSave(args.vg);
        nvgFontFaceId(args.vg, font->handle);
        nvgFontSize(args.vg, titleArtwork.fontSize);
        float textWidth =
            nvgTextBounds(args.vg, 0, 0, titleArtwork.title().c_str(), nullptr,
                          nullptr) *
            titleArtwork.xScale;
        if (textWidth > 48.f) titleArtwork.fontSize *= 48.f / textWidth;
        nvgRestore(args.vg);
      }
      titleArtwork.box.size = Vec(52, dense ? 16.f : 18.f);
      nvgSave(args.vg);
      nvgTranslate(args.vg, 0, block.box.pos.y + (dense ? 1.f : -1.f));
      titleArtwork.draw(args);
      nvgRestore(args.vg);
    }
    for (auto* knob : knobs)
      if (knob && knob->visible) knob->drawLabel(args);
    widget::Widget::draw(args);
  }
};
