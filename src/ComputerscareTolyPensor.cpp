#include <array>
#include <atomic>
#include <vector>

#include "Computerscare.hpp"
#include "Phlooper/Block.hpp"
#include "Phlooper/Dropdown.hpp"
#include "TolyPensor/Matrix.hpp"

struct ComputerscareTolyPensor;
struct TolyChange : history::ModuleAction {
  struct Value {
    int index;
    float before, after;
  };
  std::vector<Value> values;
  void apply(bool redo);
  void undo() override { apply(false); }
  void redo() override { apply(true); }
};
struct ComputerscareTolyPensor : ComputerscarePolyModule {
  enum Param {
    SELECT,
    VIEW = SELECT + 4,
    ROW_AXIS,
    COLUMN_AXIS,
    POLY,
    RANDOM_CHANCE,
    RANDOM_RANGE,
    WIGGLE_CHANCE,
    WIGGLE_RANGE,
    PARAMS
  };
  enum Input { AUDIO, ACTION = AUDIO + 16, INPUTS = ACTION + 15 };
  enum Output { AUDIO_OUT, OUTPUTS = 16 };
  toly::Matrix matrix;
  std::array<std::atomic<int>, 16> widths;
  std::atomic<unsigned> connectedOutputs{0};
  std::atomic<int> outputChannels{16};
  std::array<dsp::SchmittTrigger, 15 * 16> triggers;
  std::array<int, 15> previousTriggerChannels{};
  unsigned divider = 0;
  ComputerscareTolyPensor() {
    config(PARAMS, INPUTS, OUTPUTS, 0);
    const char* axes[] = {"Input port", "Input channel", "Output port",
                          "Output channel"};
    for (int i = 0; i < 4; ++i) {
      configParam(SELECT + i, -1, 15, (i == 1 || i == 3) ? -1 : 0, axes[i])
          ->snapEnabled = true;
    }
    configSwitch(VIEW, 0, 1, 1, "Controls", {"None", "Controls"});
    configSwitch(
        ROW_AXIS, 0, 3, 1, "Rows",
        {"Input port", "Input channel", "Output port", "Output channel"});
    configSwitch(
        COLUMN_AXIS, 0, 3, 3, "Columns",
        {"Input port", "Input channel", "Output port", "Output channel"});
    configSwitch(POLY, 0, 16, 0, "Output channels", polyChannelLabels(true));
    configParam(RANDOM_CHANCE, 0, 1, 1, "Randomize probability", "%", 0, 100);
    configParam(RANDOM_RANGE, 0, 2, 1, "Randomize range", " V/V");
    configParam(WIGGLE_CHANCE, 0, 1, 1, "Wiggle probability", "%", 0, 100);
    configParam(WIGGLE_RANGE, 0, 1, .1f, "Wiggle amount", " V/V");
    for (int i = 0; i < PARAMS; ++i)
      getParamQuantity(i)->randomizeEnabled = false;
    for (int i = 0; i < 16; ++i) {
      widths[i] = 0;
      configInput(i, std::string("Input ") + char('a' + i));
      configOutput(i, std::string("Output ") + char('A' + i));
    }
    for (int scope = 0; scope < 5; ++scope)
      for (int action = 0; action < 3; ++action) {
        std::string text = action == 0   ? "Randomize "
                           : action == 1 ? "Wiggle "
                                         : "Initialize ";
        text += scope == 4 ? "everything" : axes[scope];
        configInput(ACTION + scope * 3 + action, text);
      }
  }
  std::array<int, 4> selected() {
    std::array<int, 4> result;
    for (int i = 0; i < 4; ++i)
      result[i] = clamp(int(std::round(params[SELECT + i].getValue())), 0, 15);
    return result;
  }
  bool all(int axis) { return params[SELECT + axis].getValue() < 0; }
  int allCount() {
    int n = 0;
    for (int a = 0; a < 4; ++a) n += all(a);
    return n;
  }
  int rowAxis() {
    for (int a = 0; a < 4; ++a)
      if (all(a)) return a;
    return -1;
  }
  int columnAxis() {
    int first = rowAxis();
    for (int a = first + 1; a < 4; ++a)
      if (all(a)) return a;
    return -1;
  }
  int displayView() {
    if (params[VIEW].getValue() == 0) return 0;
    int count = allCount();
    return count > 2 ? -1 : count;
  }
  int indexFor(int row, int column = 0) {
    auto fixed = selected();
    if (rowAxis() >= 0) fixed[rowAxis()] = row;
    if (columnAxis() >= 0) fixed[columnAxis()] = column;
    return toly::cell(fixed[0], fixed[1], fixed[2], fixed[3]);
  }
  float value(int index) const {
    if (index < toly::cells) return matrix.get(index);
    index -= toly::cells;
    int axis = index / 32, indexIn = (index % 32) / 2;
    return index % 2 ? matrix.offset[axis][indexIn].load()
                     : matrix.scale[axis][indexIn].load();
  }
  void setValue(int index, float value) {
    if (!std::isfinite(value)) return;
    if (index < toly::cells)
      matrix.set(index, clamp(value, -2.f, 2.f));
    else {
      index -= toly::cells;
      int axis = index / 32, indexIn = (index % 32) / 2;
      if (index % 2)
        matrix.offset[axis][indexIn] = clamp(value, -10.f, 10.f);
      else
        matrix.scale[axis][indexIn] = clamp(value, -4.f, 4.f);
    }
  }
  static std::string address(int index) {
    auto x = toly::indices(index);
    return std::string(1, char('a' + x[0])) + std::to_string(x[1] + 1) + " → " +
           char('A' + x[2]) + std::to_string(x[3] + 1);
  }
  bool inactive(int cell) const {
    bool any = connectedOutputs.load() != 0;
    for (const auto& width : widths) any |= width.load() != 0;
    if (!any) return false;
    auto x = toly::indices(cell);
    return x[1] >= widths[x[0]].load() ||
           !(connectedOutputs.load() & (1u << x[2])) ||
           x[3] >= outputChannels.load();
  }
  void action(int scope, int kind, int selectedIndex, bool undoable) {
    auto* history = undoable ? new TolyChange : nullptr;
    if (history) {
      history->moduleId = id;
      history->name = kind == 0   ? "randomize Toly Pensor"
                      : kind == 1 ? "wiggle Toly Pensor"
                                  : "initialize Toly Pensor";
      history->values.reserve(scope == 4 || selectedIndex < 0 ? 65536 : 4096);
    }
    float chance = params[kind == 1 ? WIGGLE_CHANCE : RANDOM_CHANCE].getValue();
    float range = params[kind == 1 ? WIGGLE_RANGE : RANDOM_RANGE].getValue();
    auto write = [&](int index, float next) {
      float old = value(index);
      if (old == next) return;
      if (history) history->values.push_back({index, old, next});
      setValue(index, next);
    };
    // Iterate only the selected tensor plane for dimension-scoped actions.
    if (scope == 4 || selectedIndex < 0) {
      for (int index = 0; index < toly::cells; ++index) {
        float old = matrix.get(index), next;
        if (kind == 2)
          next = index / 256 == index % 256 ? 1.f : 0.f;
        else {
          if (random::uniform() >= chance) continue;
          float delta = (random::uniform() * 2.f - 1.f) * range;
          next = kind == 1 ? clamp(old + delta, -2.f, 2.f) : delta;
        }
        write(index, next);
      }
    } else {
      for (int a = 0; a < 16; ++a)
        for (int b = 0; b < 16; ++b)
          for (int c = 0; c < 16; ++c) {
            std::array<int, 4> x;
            int values[] = {a, b, c}, n = 0;
            for (int axis = 0; axis < 4; ++axis)
              x[axis] = axis == scope ? selectedIndex : values[n++];
            int index = toly::cell(x[0], x[1], x[2], x[3]);
            float old = matrix.get(index), next;
            if (kind == 2)
              next = index / 256 == index % 256 ? 1.f : 0.f;
            else {
              if (random::uniform() >= chance) continue;
              float delta = (random::uniform() * 2.f - 1.f) * range;
              next = kind == 1 ? clamp(old + delta, -2.f, 2.f) : delta;
            }
            write(index, next);
          }
    }
    if (kind == 2)
      for (int axis = 0; axis < 4; ++axis)
        for (int i = 0; i < 16; ++i)
          if (scope == 4 ||
              (scope == axis && (i == selectedIndex || selectedIndex < 0))) {
            write(toly::cells + axis * 32 + i * 2, 1);
            write(toly::cells + axis * 32 + i * 2 + 1, 0);
          }
    matrix.changed();
    if (history) {
      if (history->values.empty())
        delete history;
      else
        APP->history->push(history);
    }
  }
  void process(const ProcessArgs&) override {
    for (int scope = 0; scope < 5; ++scope)
      for (int kind = 0; kind < 3; ++kind) {
        int port = ACTION + scope * 3 + kind;
        auto& input = inputs[port];
        int count =
            input.isConnected() ? (scope == 4 ? 1 : input.getChannels()) : 0;
        for (int i = count; i < previousTriggerChannels[port - ACTION]; ++i)
          triggers[(port - ACTION) * 16 + i].reset();
        previousTriggerChannels[port - ACTION] = count;
        if (!count) continue;
        for (int i = 0; i < count; ++i)
          if (triggers[(port - ACTION) * 16 + i].process(input.getVoltage(i) /
                                                         2.f))
            action(scope, kind,
                   scope == 4 ? 0
                   : input.getChannels() == 1
                       ? int(params[SELECT + scope].getValue())
                       : i,
                   false);
      }
    if (divider++ % 32 == 0) {
      std::array<int, 16> layout;
      unsigned out = 0;
      int poly = 1;
      for (int i = 0; i < 16; ++i) {
        layout[i] = inputs[i].getChannels();
        widths[i] = layout[i];
        poly = std::max(poly, layout[i]);
        if (outputs[i].isConnected()) out |= 1u << i;
      }
      if (params[POLY].getValue() > 0)
        poly = clamp(int(params[POLY].getValue()), 1, 16);
      else if (std::all_of(layout.begin(), layout.end(),
                           [](int n) { return n == 0; }))
        poly = 16;
      connectedOutputs = out;
      outputChannels = poly;
      polyChannels = poly;
      matrix.prepare(layout, out, poly);
      for (int i = 0; i < 16; ++i)
        outputs[i].setChannels(out & (1u << i) ? poly : 0);
    }
    if (!connectedOutputs.load()) return;
    std::array<float, 256> input, output;
    if (matrix.sources.size() == 256) {
      for (int p = 0; p < 16; ++p)
        for (int c = 0; c < 16; c += 4)
          inputs[p].getVoltageSimd<simd::float_4>(c).store(input.data() +
                                                           toly::signal(p, c));
    } else {
      for (int source : matrix.sources)
        input[source] = inputs[source / 16].getVoltage(source % 16);
    }
    matrix.process<simd::float_4>(input, output);
    for (int op = 0; op < 16; ++op)
      if (matrix.outputMask & (1u << op))
        for (int oc = 0; oc < matrix.polyphony; oc += 4)
          outputs[op].setVoltageSimd(
              simd::float_4::load(output.data() + toly::signal(op, oc)), oc);
  }
  void onRandomize() override { action(4, 0, 0, false); }
  void onReset(const ResetEvent& e) override {
    Module::onReset(e);
    action(4, 2, 0, false);
  }
  json_t* dataToJson() override {
    auto* root = json_object();
    auto* routes = json_array();
    for (int i = 0; i < toly::cells; ++i) {
      float v = matrix.get(i);
      if (v == 0) continue;
      auto* pair = json_array();
      json_array_append_new(pair, json_integer(i));
      json_array_append_new(pair, json_real(v));
      json_array_append_new(routes, pair);
    }
    json_object_set_new(root, "routes", routes);
    auto* adjustments = json_array();
    for (int i = 0; i < 128; ++i)
      json_array_append_new(adjustments, json_real(value(toly::cells + i)));
    json_object_set_new(root, "adjustments", adjustments);
    return root;
  }
  void dataFromJson(json_t* root) override {
    auto* routes = json_object_get(root, "routes");
    if (json_is_array(routes)) {
      for (int i = 0; i < toly::cells; ++i) matrix.set(i, 0);
      size_t n;
      json_t* item;
      json_array_foreach(routes, n, item) {
        auto* key = json_array_get(item, 0);
        auto* val = json_array_get(item, 1);
        if (!json_is_integer(key) || !json_is_number(val)) continue;
        int index = int(json_integer_value(key));
        if (index >= 0 && index < toly::cells)
          setValue(index, float(json_number_value(val)));
      }
    }
    auto* adjustments = json_object_get(root, "adjustments");
    for (int i = 0; i < 128 && i < int(json_array_size(adjustments)); ++i)
      setValue(toly::cells + i,
               float(json_number_value(json_array_get(adjustments, i))));
    matrix.changed();
  }
};
void TolyChange::apply(bool redo) {
  auto* module =
      dynamic_cast<ComputerscareTolyPensor*>(APP->engine->getModule(moduleId));
  if (!module) return;
  for (const auto& v : values)
    module->setValue(v.index, redo ? v.after : v.before);
  module->matrix.changed();
}

struct TolyValueField : ui::TextField {
  int64_t moduleId = -1;
  int index = 0;
  void onSelectKey(const event::SelectKey& e) override {
    if (e.action == GLFW_PRESS &&
        (e.key == GLFW_KEY_ENTER || e.key == GLFW_KEY_KP_ENTER)) {
      auto* module = dynamic_cast<ComputerscareTolyPensor*>(
          APP->engine->getModule(moduleId));
      struct Value : Quantity {
        float result = NAN;
        void setValue(float v) override { result = v; }
        float getValue() override { return result; }
      } parsed;
      parsed.setDisplayValueString(text);
      if (module && std::isfinite(parsed.result)) {
        float before = module->value(index);
        module->setValue(index, parsed.result);
        module->matrix.changed();
        float after = module->value(index);
        if (before != after) {
          auto* h = new TolyChange;
          h->moduleId = moduleId;
          h->name = "set Toly Pensor value";
          h->values.push_back({index, before, after});
          APP->history->push(h);
        }
        if (auto* overlay = getAncestorOfType<ui::MenuOverlay>())
          overlay->requestDelete();
      }
      e.consume(this);
      return;
    }
    ui::TextField::onSelectKey(e);
  }
};
struct TolyKnob : widget::OpaqueWidget {
  ComputerscareTolyPensor* module = nullptr;
  int row = 0, column = 0, axis = -1, field = 0, dragIndex = -1;
  float before = 0, dragValue = 0;
  widget::SvgWidget art;
  WeakPtr<ui::Tooltip> tooltip;
  TolyKnob() {
    box.size = Vec(12, 12);
    art.setSvg(APP->window->loadSvg(asset::plugin(
        pluginInstance, "res/components/computerscare-small-knob-effed.svg")));
  }
  ~TolyKnob() override {
    if (auto* tip = tooltip.get()) tip->requestDelete();
  }
  int index() {
    return axis < 0
               ? module->indexFor(row, column)
               : toly::cells + axis * 32 + module->selected()[axis] * 2 + field;
  }
  std::string label() {
    if (!module) return "Connection";
    if (axis < 0) return ComputerscareTolyPensor::address(index());
    const char* names[] = {"Input port", "Input channel", "Output port",
                           "Output channel"};
    int selected = module->selected()[axis];
    std::string address = axis == 0   ? std::string(1, char('a' + selected))
                          : axis == 2 ? std::string(1, char('A' + selected))
                                      : std::to_string(selected + 1);
    return std::string(names[axis]) + " " + address +
           (field ? " offset" : " scale");
  }
  void pushChange(int at, float old, float next) {
    if (old == next) return;
    auto* h = new TolyChange;
    h->moduleId = module->id;
    h->name = "adjust Toly Pensor " + label();
    h->values.push_back({at, old, next});
    APP->history->push(h);
  }
  void draw(const DrawArgs& args) override {
    float value = module     ? module->value(index())
                  : axis < 0 ? (row == column ? 1.f : 0.f)
                  : field    ? 0.f
                             : 1.f;
    float range = axis < 0 ? 2.f : field ? 10.f : 4.f;
    bool faded = module && axis < 0 && module->inactive(index());
    nvgSave(args.vg);
    nvgGlobalAlpha(args.vg, faded ? .27f : 1.f);
    nvgTranslate(args.vg, box.size.x * .5f, box.size.y * .5f);
    nvgRotate(args.vg, clamp(value / range, -1.f, 1.f) * 2.5f);
    nvgScale(args.vg, box.size.x / 18.f, box.size.y / 18.f);
    nvgTranslate(args.vg, -9, -9);
    art.draw(args);
    nvgRestore(args.vg);
  }
  void step() override {
    widget::OpaqueWidget::step();
    if (auto* tip = tooltip.get()) {
      tip->text =
          label() + string::f(": %+.3f", module ? module->value(index()) : 0.f);
    }
  }
  void onEnter(const event::Enter&) override {
    if (!module) return;
    auto* tip = new ui::Tooltip;
    tip->text = label() + string::f(": %+.3f", module->value(index()));
    tooltip = tip;
    APP->scene->addChild(tip);
  }
  void onLeave(const event::Leave&) override {
    if (auto* tip = tooltip.get()) tip->requestDelete();
    tooltip = nullptr;
  }
  void onButton(const event::Button& e) override {
    if (!module) return;
    if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT)
      e.consume(this);
    if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_RIGHT) {
      int at = index();
      auto* menu = createMenu();
      auto* caption = new ui::MenuLabel;
      caption->text = label();
      menu->addChild(caption);
      auto* valueField = new TolyValueField;
      valueField->moduleId = module->id;
      valueField->index = at;
      valueField->text = string::f("%.6g", module->value(at));
      valueField->box.size = Vec(180, 22);
      menu->addChild(valueField);
      for (float v : {0.f, 1.f, -1.f})
        menu->addChild(createMenuItem(string::f("Set %+.0f", v), "", [=]() {
          float old = module->value(at);
          module->setValue(at, v);
          module->matrix.changed();
          pushChange(at, old, v);
        }));
      e.consume(this);
    }
  }
  void onDoubleClick(const event::DoubleClick& e) override {
    if (!module) return;
    int at = index();
    float old = module->value(at),
          next = axis < 0 ? (at / 256 == at % 256 ? 1.f : 0.f)
                 : field  ? 0.f
                          : 1.f;
    module->setValue(at, next);
    module->matrix.changed();
    pushChange(at, old, next);
    e.consume(this);
  }
  void onDragStart(const event::DragStart& e) override {
    if (!module || e.button != GLFW_MOUSE_BUTTON_LEFT) return;
    dragIndex = index();
    before = dragValue = module->value(dragIndex);
  }
  void onDragMove(const event::DragMove& e) override {
    if (!module || dragIndex < 0) return;
    float range = axis < 0 ? 4.f : field ? 20.f : 8.f;
    int mods = APP->window->getMods();
    float fine = (mods & RACK_MOD_CTRL) ? .1f : 1.f;
    dragValue = clamp(dragValue - e.mouseDelta.y * range * .002f * fine,
                      -range * .5f, range * .5f);
    module->setValue(dragIndex, dragValue);
    module->matrix.changed();
  }
  void onDragEnd(const event::DragEnd&) override {
    if (module && dragIndex >= 0)
      pushChange(dragIndex, before, module->value(dragIndex));
    dragIndex = -1;
  }
};
struct TolyChoice : phlooper::WarpedDropdown {
  WeakPtr<ui::MenuOverlay> overlay;
  bool isPressed() override {
    auto* menu = overlay.get();
    return menu && !menu->requestedDelete;
  }
  ComputerscareTolyPensor* module = nullptr;
  int param = 0;
  static std::string indexLabel(int axis, int index) {
    return index < 0   ? "All"
           : axis == 0 ? std::string(1, char('a' + index))
           : axis == 2 ? std::string(1, char('A' + index))
                       : std::to_string(index + 1);
  }
  static std::string axisLabel(int axis) {
    const char* names[] = {"In port", "In ch", "Out port", "Out ch"};
    return names[clamp(axis, 0, 3)];
  }
  std::string label() override {
    int value = module ? int(module->params[param].getValue())
                : param == ComputerscareTolyPensor::VIEW        ? 2
                : param == ComputerscareTolyPensor::ROW_AXIS    ? 1
                : param == ComputerscareTolyPensor::COLUMN_AXIS ? 3
                                                                : 0;
    if (module && param == ComputerscareTolyPensor::COLUMN_AXIS)
      value = module->columnAxis();
    if (param < 4) return axisLabel(param) + " " + indexLabel(param, value);
    if (param == ComputerscareTolyPensor::VIEW)
      return value == 0 ? "None" : "Controls";
    return std::string(param == ComputerscareTolyPensor::ROW_AXIS ? "Rows: "
                                                                  : "Cols: ") +
           axisLabel(value);
  }
  void onButton(const event::Button& e) override {
    if (!module || e.action != GLFW_PRESS || e.button != GLFW_MOUSE_BUTTON_LEFT)
      return;
    auto* menu = createMenu();
    overlay = menu->getAncestorOfType<ui::MenuOverlay>();
    int count = param < 4 ? 16 : param == ComputerscareTolyPensor::VIEW ? 2 : 4;
    for (int i = 0; i < count; ++i) {
      std::string name = param < 4
                             ? axisLabel(param) + " " + indexLabel(param, i)
                         : param == ComputerscareTolyPensor::VIEW
                             ? (i == 0 ? "None" : "Controls")
                             : axisLabel(i);
      menu->addChild(createCheckMenuItem(
          name, "",
          [=]() {
            return param == ComputerscareTolyPensor::VIEW
                       ? (module->params[param].getValue() > 0) == (i > 0)
                       : module->params[param].getValue() == i;
          },
          [=]() {
            auto* h = new history::ParamChange;
            h->name = "Toly Pensor view";
            h->moduleId = module->id;
            h->paramId = param;
            h->oldValue = module->params[param].getValue();
            h->newValue = i;
            module->params[param].setValue(i);
            APP->history->push(h);
          }));
    }
    e.consume(this);
  }
};
struct TolyFocusButton : ComputerscareBlankButton {
  ComputerscareTolyPensor* module = nullptr;
  int axis = 0, index = 0;
  float xScale = .82f, yScale = 1.3f;
  void configure(int dimension) {
    axis = dimension;
    bool letter = axis == 0 || axis == 2;
    xScale = letter ? .82f : .76f;
    yScale = 1.4f;
    box.size.x *= xScale;
    box.size.y *= yScale;
  }
  bool isPressed() {
    return module ? int(module->params[axis].getValue()) == index
                  : index == ((axis == 1 || axis == 3) ? -1 : 0);
  }
  void step() override {
    ComputerscareBlankButton::step();
    if (frames.size() > 1) {
      auto frame = frames[isPressed() ? 1 : 0];
      if (sw->svg != frame) {
        sw->setSvg(frame);
        fb->setDirty();
      }
    }
  }
  void draw(const DrawArgs& args) override {
    nvgSave(args.vg);
    nvgScale(args.vg, xScale, yScale);
    ComputerscareBlankButton::draw(args);
    nvgRestore(args.vg);
    auto font = APP->window->loadFont(
        asset::plugin(pluginInstance, "res/fonts/Oswald-Regular.ttf"));
    if (!font) return;
    bool letter = axis == 0 || axis == 2;
    nvgFontFaceId(args.vg, font->handle);
    nvgFontSize(args.vg, index < 0 ? 11.f : letter ? 17.f : 15.f);
    nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgFillColor(args.vg, nvgRGB(0, 0, 0));
    std::string text = TolyChoice::indexLabel(axis, index);
    nvgText(args.vg, box.size.x * .5f + (isPressed() ? 1.8f * xScale : -1.3f),
            box.size.y * .52f + (isPressed() ? 1.f : -1.7f), text.c_str(),
            nullptr);
  }
  void onButton(const event::Button& e) override {
    if (!module || e.action != GLFW_PRESS || e.button != GLFW_MOUSE_BUTTON_LEFT)
      return;
    auto* h = new history::ParamChange;
    h->name = "Toly Pensor focus";
    h->moduleId = module->id;
    h->paramId = axis;
    h->oldValue = module->params[axis].getValue();
    h->newValue = index;
    module->params[axis].setValue(index);
    APP->history->push(h);
    e.consume(this);
  }
};
struct TolyAction : phlooper::WarpedDropdown {
  ComputerscareTolyPensor* module = nullptr;
  int scope = 0, kind = 0;
  bool down = false;
  std::string label() override {
    return kind == 0 ? "Rnd" : kind == 1 ? "Wig" : "Init";
  }
  bool isPressed() override { return down; }
  void onButton(const event::Button& e) override {
    if (e.button == GLFW_MOUSE_BUTTON_LEFT && e.action == GLFW_PRESS) {
      down = true;
      if (module)
        module->action(
            scope, kind,
            scope < 4
                ? int(module->params[ComputerscareTolyPensor::SELECT + scope]
                          .getValue())
                : 0,
            true);
      e.consume(this);
    }
  }
  void onDragEnd(const event::DragEnd&) override { down = false; }
  void onLeave(const event::Leave&) override { down = false; }
};
struct TolyGrid : widget::Widget {
  widget::SvgWidget* adjustmentLabels = nullptr;
  ComputerscareTolyPensor* module = nullptr;
  std::array<TolyKnob*, 256> knobs{};
  std::array<TolyKnob*, 8> adjustments{};
  void step() override {
    int view = module ? (module->allCount() == 0 &&
                                 module->params[ComputerscareTolyPensor::VIEW]
                                         .getValue() != 0
                             ? 3
                             : module->displayView())
                      : 2;
    for (int i = 0; i < 256; ++i) {
      auto* k = knobs[i];
      k->visible = view == 2 || (view == 1 && i < 16) || (view == 3 && i == 0);
      k->row = view == 2 ? i / 16 : i;
      k->column = view == 2 ? i % 16 : 0;
      k->box.size = Vec(view == 2 ? 12.f : 27.f, view == 2 ? 12.f : 27.f);
      k->box.pos = view == 2 ? Vec(202 + (i % 16) * 27.f, 87 + (i / 16) * 13.f)
                             : Vec(242 + (i % 4) * 96.f, 97 + (i / 4) * 49.f);
    }
    for (auto* k : adjustments)
      k->visible = view > 0 && (module ? !module->all(k->axis)
                                       : k->axis == 0 || k->axis == 2);
    if (adjustmentLabels) adjustmentLabels->visible = false;
    widget::Widget::step();
  }
  void draw(const DrawArgs& args) override {
    auto font =
        APP->window->loadFont(asset::system("res/fonts/DejaVuSans.ttf"));
    if (!font) return;
    nvgFontFaceId(args.vg, font->handle);
    nvgFontSize(args.vg, 8);
    nvgFillColor(args.vg, nvgRGB(38, 55, 50));
    nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    int view = module ? (module->allCount() == 0 &&
                                 module->params[ComputerscareTolyPensor::VIEW]
                                         .getValue() != 0
                             ? 3
                             : module->displayView())
                      : 2;
    int rowAxis = module ? module->rowAxis() : 1,
        columnAxis = module ? module->columnAxis() : 3;
    if (view == 1 || view == 2) {
      nvgFontSize(args.vg, 11);
      nvgText(args.vg, 260, 48,
              ("Rows: " + TolyChoice::axisLabel(rowAxis)).c_str(), nullptr);
      if (view == 2)
        nvgText(args.vg, 365, 48,
                ("Columns: " + TolyChoice::axisLabel(columnAxis)).c_str(),
                nullptr);
      nvgFontSize(args.vg, 8);
    }
    if (view == 2)
      for (int i = 0; i < 16; ++i) {
        auto row = TolyChoice::indexLabel(rowAxis, i),
             column = TolyChoice::indexLabel(columnAxis, i);
        nvgText(args.vg, 188, 93 + i * 13.f, row.c_str(), nullptr);
        nvgText(args.vg, 208 + i * 27.f, 77, column.c_str(), nullptr);
      }
    if (view == 1)
      for (int i = 0; i < 16; ++i) {
        auto text = TolyChoice::indexLabel(rowAxis, i);
        nvgText(args.vg, 255 + (i % 4) * 96.f, 130 + (i / 4) * 49.f,
                text.c_str(), nullptr);
      }
    if (view < 0) {
      nvgFontSize(args.vg, 12);
      nvgText(args.vg, 414, 170,
              "Select at most two All dimensions to edit a slice", nullptr);
    }
    if (view == 0) {
      nvgFontSize(args.vg, 14);
      nvgText(args.vg, 414, 170, "Routing active · controls hidden", nullptr);
    }
    if (view > 0)
      for (int axis = 0; axis < 4; ++axis) {
        if (module ? module->all(axis) : axis == 1 || axis == 3) continue;
        nvgText(args.vg, 110, 113 + axis * 54.f, "scale", nullptr);
        nvgText(args.vg, 145, 113 + axis * 54.f, "offset", nullptr);
        std::string text =
            TolyChoice::axisLabel(axis) + " " +
            TolyChoice::indexLabel(axis, module ? module->selected()[axis] : 0);
        nvgFontSize(args.vg, 8);
        nvgText(args.vg, 128, 77 + axis * 54.f, text.c_str(), nullptr);
      }
    widget::Widget::draw(args);
  }
};
struct TolyBackground : widget::TransparentWidget {
  void draw(const DrawArgs& args) override {
    nvgBeginPath(args.vg);
    nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
    nvgFillColor(args.vg, nvgRGB(222, 222, 222));
    nvgFill(args.vg);
  }
};
struct ComputerscareTolyPensorWidget : ModuleWidget {
  TolyGrid* grid = nullptr;
  explicit ComputerscareTolyPensorWidget(ComputerscareTolyPensor* module) {
    setModule(module);
    box.size = Vec(780, 380);
    auto* background = new TolyBackground;
    background->box.size = box.size;
    addChild(background);
    auto block = [&](Rect rect, int grey, unsigned seed) {
      auto* b = new phlooper::WarpedBlock;
      b->configure(rect, 3, 1.5f, seed, nvgRGB(grey, grey, grey));
      addChild(b);
    };
    block(Rect(Vec(8, 3), Vec(244, 31)), 205, 301);
    block(Rect(Vec(8, 30), Vec(78, 346)), 223, 302);
    block(Rect(Vec(91, 64), Vec(74, 234)), 210, 303);
    block(Rect(Vec(173, 68), Vec(483, 230)), 232, 304);
    block(Rect(Vec(670, 30), Vec(102, 346)), 175, 305);
    auto* panel = new ComputerscareSVGPanel;
    panel->setBackground(APP->window->loadSvg(asset::plugin(
        pluginInstance, "res/panels/ComputerscareTolyPensorPanel.svg")));
    addChild(panel);
    auto choice = [&](int param, Vec pos, float width) {
      auto* c = new TolyChoice;
      c->module = module;
      c->param = param;
      c->configure(width, 19, 3, 2, .8f, 310 + param);
      c->box.pos = pos;
      c->textSize = 11;
      addChild(c);
    };
    choice(ComputerscareTolyPensor::VIEW, Vec(639, 8), 76);

    addChild(new PolyOutputChannelsWidget(Vec(731, 1), module,
                                          ComputerscareTolyPensor::POLY, 16));
    for (int i = 0; i < 4; ++i)
      addParam(createParamCentered<SmallKnob>(
          Vec(463 + i * 49.f, 47), module,
          ComputerscareTolyPensor::RANDOM_CHANCE + i));
    for (int i = 0; i < 16; ++i) {
      float y = 65 + i * 20.f;
      addInput(
          createInputCentered<JackSmallPointingDown>(Vec(22, y), module, i));
      addOutput(
          createOutputCentered<JackSmallPointingUp>(Vec(752, y), module, i));
      for (int axis = 0; axis < 4; ++axis) {
        const float x[] = {34, 59, 712, 687};
        auto* b = new TolyFocusButton;
        b->module = module;
        b->axis = axis;
        b->index = i;
        b->configure(axis);
        b->box.pos = Vec(x[axis], y - 9);
        addChild(b);
      }
    }
    for (int axis = 0; axis < 4; ++axis) {
      const float x[] = {34, 59, 712, 687};
      auto* b = new TolyFocusButton;
      b->module = module;
      b->index = -1;
      b->configure(axis);
      b->box.pos = Vec(x[axis], 32);
      addChild(b);
    }
    grid = new TolyGrid;
    grid->module = module;
    grid->box.size = box.size;
    addChild(grid);
    for (int i = 0; i < 256; ++i) {
      auto* k = new TolyKnob;
      k->module = module;
      grid->knobs[i] = k;
      grid->addChild(k);
    }
    for (int axis = 0; axis < 4; ++axis)
      for (int field = 0; field < 2; ++field) {
        auto* k = new TolyKnob;
        k->module = module;
        k->axis = axis;
        k->field = field;
        k->box.size = Vec(18, 18);
        k->box.pos = Vec(101 + field * 32, 89 + axis * 54);
        grid->adjustments[axis * 2 + field] = k;
        grid->addChild(k);
      }
    grid->adjustmentLabels = new widget::SvgWidget;
    grid->adjustmentLabels->setSvg(APP->window->loadSvg(asset::plugin(
        pluginInstance,
        "res/panels/ComputerscareTolyPensorAdjustmentLabels.svg")));
    grid->addChild(grid->adjustmentLabels);
    for (int scope = 0; scope < 5; ++scope) {
      float x = 94 + scope * 113.f;
      block(Rect(Vec(x, 309), Vec(105, 53)), 202, 320 + scope);
      for (int kind = 0; kind < 3; ++kind) {
        auto* b = new TolyAction;
        b->module = module;
        b->scope = scope;
        b->kind = kind;
        b->configure(27, 16, 2.5, 1.5, .8f, 330 + scope * 3 + kind);
        b->textSize = 10;
        b->faceColor = nvgRGB(36, 201, 166);
        b->box.pos = Vec(x + 5 + kind * 33, 325);
        addChild(b);
        addInput(createInputCentered<JackSmallPointingDown>(
            Vec(x + 18 + kind * 33, 351), module,
            ComputerscareTolyPensor::ACTION + scope * 3 + kind));
      }
    }
    // Artwork remains above the generated group backgrounds.
    auto* captions = new ComputerscareSVGPanel;
    captions->setBackground(APP->window->loadSvg(asset::plugin(
        pluginInstance, "res/panels/ComputerscareTolyPensorLabels.svg")));
    addChild(captions);
    grid->step();
  }
};
Model* modelComputerscareTolyPensor =
    createModel<ComputerscareTolyPensor, ComputerscareTolyPensorWidget>(
        "computerscare-toly-pensor");
