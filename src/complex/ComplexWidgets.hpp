#pragma once

#include "ComplexText.hpp"
#include "math/ComplexFormat.hpp"

using namespace rack;

namespace cpx {

constexpr int RECT_INTERLEAVED =
    static_cast<int>(cpx::complex_math::CoordinateMode::RectangularInterleaved);
constexpr int POLAR_INTERLEAVED =
    static_cast<int>(cpx::complex_math::CoordinateMode::PolarInterleaved);
constexpr int RECT_SEPARATED =
    static_cast<int>(cpx::complex_math::CoordinateMode::RectangularSeparated);
constexpr int POLAR_SEPARATED =
    static_cast<int>(cpx::complex_math::CoordinateMode::PolarSeparated);

inline void drawArrowTo(NVGcontext* vg, math::Vec tipPosition,
                        float baseWidth = 5.f,
                        NVGcolor fillColor = COLOR_COMPUTERSCARE_LIGHT_GREEN,
                        NVGcolor strokeColor = COLOR_COMPUTERSCARE_DARK_GREEN,
                        float strokeWidth = 1.f) {
  float angle = tipPosition.arg();
  float len = tipPosition.norm();

  // nvgSave(vg);
  nvgBeginPath(vg);
  nvgStrokeWidth(vg, strokeWidth);
  nvgStrokeColor(vg, strokeColor);
  nvgFillColor(vg, fillColor);

  nvgRotate(vg, angle);
  nvgMoveTo(vg, 0, -baseWidth);
  // nvgArcTo(vg,0,0,0,baseWidth/2,13);
  // nvgLineTo(vg,-2,-baseWidth/2);
  // nvgLineTo(vg,-2,baseWidth/2);
  nvgQuadTo(vg, baseWidth / 2, 0, 0, baseWidth);
  nvgLineTo(vg, 0, baseWidth);
  nvgLineTo(vg, len, 0);
  // nvgLineTo(vg,len,-1);

  nvgClosePath(vg);
  nvgFill(vg);
  nvgStroke(vg);

  // nvgRestore(vg);
}

enum class ComplexXYMaxMode {
  Radial,
  Rectangular,
};

enum class ComplexXYParamMode {
  Rectangular,
  PolarDegrees,
};

struct ComplexXY;

struct ComplexXYDragOverlay : TransparentWidget {
  ComplexXY* source = nullptr;

  void step() override;
  void draw(const DrawArgs& args) override;
};

struct ComplexXYTooltip : ui::Tooltip {
  ComplexXY* source = nullptr;

  void step() override;
};

struct ComplexXYValueField : ui::TextField {
  enum class Field { X, Y, R, Theta };

  ComplexXY* source = nullptr;
  Field field = Field::X;

  void step() override;
  void onSelectKey(const SelectKeyEvent& e) override;
  void commit();
};

struct ComplexXYValueQuantity : Quantity {
  ComplexXY* source = nullptr;
  ComplexXYValueField::Field field = ComplexXYValueField::Field::X;
  std::string label;

  float getValue() override;
  void setValue(float value) override;
  float getMinValue() override;
  float getMaxValue() override;
  float getDefaultValue() override;
  int getDisplayPrecision() override { return 5; }
  std::string getLabel() override { return ""; }
  std::string getUnit() override;
};

struct ComplexXYValueRow : ui::MenuEntry {
  ComplexXY* source = nullptr;
  ComplexXYValueField::Field field = ComplexXYValueField::Field::X;
  std::string label;
  ComplexXYValueQuantity* quantity = nullptr;
  ui::Slider* slider = nullptr;
  ComplexXYValueField* textField = nullptr;

  ComplexXYValueRow(ComplexXY* source, const std::string& label,
                    ComplexXYValueField::Field field);
  ~ComplexXYValueRow() override;
  void step() override;
  void draw(const DrawArgs& args) override;
};

struct ComplexXY : TransparentWidget {
  ComputerscareComplexBase* module;

  Vec clickedMousePosition;
  Vec thisPos;
  math::Vec deltaPos;

  Vec pixelsOrigin;
  Vec pixelsDiff;

  Vec origComplexValue;
  float origComplexLength;
  float origParamAValue = 0.f;
  float origParamBValue = 0.f;

  Vec newZ;

  int paramA;
  ComplexXYParamMode paramMode = ComplexXYParamMode::Rectangular;
  std::string controlName;

  bool editing = false;
  bool cancelledDrag = false;
  bool pendingLeftPress = false;
  double pendingLeftPressTime = 0.0;
  Vec pendingMousePosition;
  bool faded = false;
  NVGcolor normalBackgroundColor = nvgRGBA(0, 35, 25, 80);
  NVGcolor fadedBackgroundColor = nvgRGBA(150, 150, 150, 70);
  NVGcolor normalArrowFillColor = nvgRGB(70, 170, 120);
  NVGcolor normalArrowStrokeColor = nvgRGB(8, 48, 32);
  NVGcolor fadedArrowFillColor = nvgRGB(125, 125, 125);
  NVGcolor fadedArrowStrokeColor = nvgRGB(75, 75, 75);

  float originalMagnituteRadiusPixels = 120.f;
  ComplexXYMaxMode maxMode = ComplexXYMaxMode::Radial;
  float maxVoltage = 10.f;
  float drawingScale = 1.f;
  ComplexXYDragOverlay* dragOverlay = nullptr;
  ComplexXYTooltip* tooltip = nullptr;

  ComplexXY(ComputerscareComplexBase* mod, int indexParamA,
            ComplexXYParamMode mode = ComplexXYParamMode::Rectangular,
            std::string name = "") {
    module = mod;
    paramA = indexParamA;
    paramMode = mode;
    controlName = name;
    // box.size = Vec(30,30);
    TransparentWidget();
  }

  ~ComplexXY() override {
    destroyTooltip();
    if (dragOverlay) {
      if (dragOverlay->parent) dragOverlay->parent->removeChild(dragOverlay);
      delete dragOverlay;
      dragOverlay = nullptr;
    }
  }

  engine::ParamQuantity* getParamQuantity(int offset) {
    if (!module) return nullptr;
    int id = paramA + offset;
    if (id < 0 || id >= (int)module->paramQuantities.size()) return nullptr;
    return module->paramQuantities[id];
  }

  Vec getParamPair() {
    if (!module) return Vec(0.f, 0.f);
    return Vec(module->params[paramA].getValue(),
               module->params[paramA + 1].getValue());
  }

  Vec getRectValue() {
    Vec ab = getParamPair();
    if (paramMode == ComplexXYParamMode::PolarDegrees) {
      float thetaRadians = ab.y * cpx::complex_math::pi / 180.f;
      return Vec(ab.x * std::cos(thetaRadians), ab.x * std::sin(thetaRadians));
    }
    return ab;
  }

  Vec getPolarValue() {
    Vec xy = getRectValue();
    return Vec(std::hypot(xy.x, xy.y),
               std::atan2(xy.y, xy.x) * 180.f / cpx::complex_math::pi);
  }

  Vec rectToParamPair(float x, float y) {
    if (paramMode == ComplexXYParamMode::PolarDegrees) {
      return Vec(std::hypot(x, y),
                 std::atan2(y, x) * 180.f / cpx::complex_math::pi);
    }
    return Vec(x, y);
  }

  Vec polarToParamPair(float r, float thetaDegrees) {
    if (paramMode == ComplexXYParamMode::PolarDegrees) {
      return Vec(r, thetaDegrees);
    }

    float thetaRadians = thetaDegrees * cpx::complex_math::pi / 180.f;
    return Vec(r * std::cos(thetaRadians), r * std::sin(thetaRadians));
  }

  std::string shortFloatString(float value, int precision = 5) {
    std::ostringstream ss;
    ss << std::setprecision(precision) << math::normalizeZero(value);
    return ss.str();
  }

  std::string hoverValueString() {
    Vec xy = getRectValue();
    Vec rt = getPolarValue();
    return "x: " + shortFloatString(xy.x) + "   y: " + shortFloatString(xy.y) +
           "\nr: " + shortFloatString(rt.x) +
           "   θ: " + shortFloatString(rt.y) + "°";
  }

  std::vector<std::string> splitWords(const std::string& s) {
    std::istringstream ss(s);
    std::vector<std::string> words;
    std::string word;
    while (ss >> word) {
      words.push_back(word);
    }
    return words;
  }

  std::string joinWords(const std::vector<std::string>& words, int first,
                        int last) {
    std::string out;
    for (int i = first; i < last; i++) {
      if (!out.empty()) out += " ";
      out += words[i];
    }
    return out;
  }

  bool isCoordinateWord(const std::string& word) {
    return word == "Real" || word == "Imaginary" || word == "Radius" ||
           word == "Theta" || word == "X" || word == "Y" || word == "x" ||
           word == "y" || word == "R" || word == "r" || word == "θ";
  }

  std::string withoutTrailingCoordinateWord(const std::string& label) {
    std::vector<std::string> words = splitWords(label);
    if (!words.empty() && isCoordinateWord(words.back())) {
      words.pop_back();
    }
    return joinWords(words, 0, words.size());
  }

  std::string withoutLeadingCoordinateWord(const std::string& label) {
    std::vector<std::string> words = splitWords(label);
    int first = (!words.empty() && isCoordinateWord(words.front())) ? 1 : 0;
    return joinWords(words, first, words.size());
  }

  std::string commonWordPrefix(const std::string& a, const std::string& b) {
    std::vector<std::string> aw = splitWords(a);
    std::vector<std::string> bw = splitWords(b);
    int n = std::min(aw.size(), bw.size());
    int common = 0;
    while (common < n && aw[common] == bw[common]) {
      common++;
    }
    return joinWords(aw, 0, common);
  }

  std::string menuControlName() {
    if (!controlName.empty()) return controlName;

    engine::ParamQuantity* pqa = getParamQuantity(0);
    engine::ParamQuantity* pqb = getParamQuantity(1);
    if (pqa && pqb) {
      std::string a = pqa->getLabel();
      std::string b = pqb->getLabel();
      if (a == b) return a;

      std::string trailingA = withoutTrailingCoordinateWord(a);
      std::string trailingB = withoutTrailingCoordinateWord(b);
      if (!trailingA.empty() && trailingA == trailingB) return trailingA;

      std::string prefix = commonWordPrefix(a, b);
      if (!prefix.empty()) return prefix;

      std::string leadingA = withoutLeadingCoordinateWord(a);
      std::string leadingB = withoutLeadingCoordinateWord(b);
      if (!leadingA.empty() && leadingA == leadingB) return leadingA;

      return a + " / " + b;
    }
    if (pqa) return pqa->getLabel();
    if (pqb) return pqb->getLabel();
    return "Complex Arrow";
  }

  std::string tooltipString() {
    return menuControlName() + "\n" + hoverValueString();
  }

  void setControlName(const std::string& name) { controlName = name; }

  std::string rectDragDisplayString(float x, float y) {
    return cpx::complex_math::fixedWidthRectString(x, y);
  }

  std::string polarDragDisplayString(float x, float y) {
    return cpx::complex_math::fixedWidthPolarEngineeringString(
        std::hypot(x, y), std::atan2(y, x));
  }

  std::string voltageLabelString(float value) {
    std::ostringstream ss;
    ss << std::setprecision(3) << std::noshowpoint << value << "v";
    return ss.str();
  }

  void setRadialMax(float max) {
    maxMode = ComplexXYMaxMode::Radial;
    maxVoltage = std::max(0.f, max);
  }

  void setRectangularMax(float max) {
    maxMode = ComplexXYMaxMode::Rectangular;
    maxVoltage = std::max(0.f, max);
  }

  void setDrawingScale(float scale) { drawingScale = std::max(0.f, scale); }

  Vec clampToMax(Vec z) {
    if (maxVoltage <= 0.f) return Vec(0.f, 0.f);

    if (maxMode == ComplexXYMaxMode::Radial) {
      float length = z.norm();
      if (length > maxVoltage) return z.mult(maxVoltage / length);
      return z;
    }

    float absX = std::fabs(z.x);
    float absY = std::fabs(z.y);
    if (absX <= maxVoltage && absY <= maxVoltage) return z;

    float scale = 1.f;
    if (absX > 0.f) scale = std::min(scale, maxVoltage / absX);
    if (absY > 0.f) scale = std::min(scale, maxVoltage / absY);
    return z.mult(scale);
  }

  void setParamPairRaw(float a, float b) {
    if (!module) return;
    module->params[paramA].setValue(a);
    module->params[paramA + 1].setValue(b);
  }

  void setRectRaw(float x, float y) {
    Vec ab = rectToParamPair(x, y);
    setParamPairRaw(ab.x, ab.y);
  }

  void setPolarRaw(float r, float thetaDegrees) {
    Vec ab = polarToParamPair(r, thetaDegrees);
    setParamPairRaw(ab.x, ab.y);
  }

  float getFieldValue(ComplexXYValueField::Field field) {
    Vec xy = getRectValue();
    Vec rt = getPolarValue();
    switch (field) {
      case ComplexXYValueField::Field::X:
        return xy.x;
      case ComplexXYValueField::Field::Y:
        return xy.y;
      case ComplexXYValueField::Field::R:
        return rt.x;
      case ComplexXYValueField::Field::Theta:
        return rt.y;
    }
    return 0.f;
  }

  void setFieldRaw(ComplexXYValueField::Field field, float value) {
    Vec xy = getRectValue();
    Vec rt = getPolarValue();
    switch (field) {
      case ComplexXYValueField::Field::X:
        setRectRaw(value, xy.y);
        break;
      case ComplexXYValueField::Field::Y:
        setRectRaw(xy.x, value);
        break;
      case ComplexXYValueField::Field::R:
        setPolarRaw(value, rt.y);
        break;
      case ComplexXYValueField::Field::Theta:
        setPolarRaw(rt.x, value);
        break;
    }
  }

  float getFieldMin(ComplexXYValueField::Field field) {
    if (field == ComplexXYValueField::Field::R) return 0.f;
    if (field == ComplexXYValueField::Field::Theta) return -180.f;
    return -maxVoltage;
  }

  float getFieldMax(ComplexXYValueField::Field field) {
    if (field == ComplexXYValueField::Field::R) {
      return maxMode == ComplexXYMaxMode::Rectangular
                 ? maxVoltage * std::sqrt(2.f)
                 : maxVoltage;
    }
    if (field == ComplexXYValueField::Field::Theta) return 180.f;
    return maxVoltage;
  }

  void setParamPairWithHistory(float a, float b, const std::string& name) {
    if (!module) return;
    engine::ParamQuantity* pqa = getParamQuantity(0);
    engine::ParamQuantity* pqb = getParamQuantity(1);
    if (!pqa || !pqb) return;

    float oldA = pqa->getValue();
    float oldB = pqb->getValue();
    pqa->setValue(a);
    pqb->setValue(b);
    float newA = pqa->getValue();
    float newB = pqb->getValue();

    history::ComplexAction* h = new history::ComplexAction;
    h->name = name;
    if (oldA != newA) {
      history::ParamChange* ha = new history::ParamChange;
      ha->name = name;
      ha->moduleId = module->id;
      ha->paramId = paramA;
      ha->oldValue = oldA;
      ha->newValue = newA;
      h->push(ha);
    }
    if (oldB != newB) {
      history::ParamChange* hb = new history::ParamChange;
      hb->name = name;
      hb->moduleId = module->id;
      hb->paramId = paramA + 1;
      hb->oldValue = oldB;
      hb->newValue = newB;
      h->push(hb);
    }
    if (h->isEmpty()) {
      delete h;
    } else {
      APP->history->push(h);
    }
  }

  void setRectWithHistory(float x, float y, const std::string& name) {
    Vec ab = rectToParamPair(x, y);
    setParamPairWithHistory(ab.x, ab.y, name);
  }

  void setPolarWithHistory(float r, float thetaDegrees,
                           const std::string& name) {
    Vec ab = polarToParamPair(r, thetaDegrees);
    setParamPairWithHistory(ab.x, ab.y, name);
  }

  void resetWithHistory() {
    engine::ParamQuantity* pqa = getParamQuantity(0);
    engine::ParamQuantity* pqb = getParamQuantity(1);
    if (!pqa || !pqb) return;
    setParamPairWithHistory(pqa->getDefaultValue(), pqb->getDefaultValue(),
                            "initialize complex arrow");
  }

  void startEditing(Vec mousePosition) {
    if (editing || !module) return;
    editing = true;
    pendingLeftPress = false;
    cancelledDrag = false;
    clickedMousePosition = mousePosition;

    Vec xy = getRectValue();
    origParamAValue = module->params[paramA].getValue();
    origParamBValue = module->params[paramA + 1].getValue();
    origComplexValue = Vec(xy.x, -xy.y);
    origComplexLength = origComplexValue.norm();

    if (origComplexLength < 0.1) {
      origComplexLength = 1;
      pixelsOrigin = clickedMousePosition;
    } else {
      pixelsOrigin = clickedMousePosition.minus(origComplexValue.mult(
          originalMagnituteRadiusPixels / origComplexLength));
    }
  }

  void createTooltip() {
    if (!settings::tooltips || tooltip || !module || editing) return;
    tooltip = new ComplexXYTooltip;
    tooltip->source = this;
    APP->scene->addChild(tooltip);
  }

  void destroyTooltip() {
    if (!tooltip) return;
    if (tooltip->parent) tooltip->parent->removeChild(tooltip);
    delete tooltip;
    tooltip = nullptr;
  }

  void createContextMenu() {
    destroyTooltip();
    ui::Menu* menu = createMenu();
    menu->addChild(construct<MenuLabel>(&MenuLabel::text, menuControlName()));

    addValueField(menu, "x", ComplexXYValueField::Field::X);
    addValueField(menu, "y", ComplexXYValueField::Field::Y);
    addValueField(menu, "r", ComplexXYValueField::Field::R);
    addValueField(menu, "θ°", ComplexXYValueField::Field::Theta);

    menu->addChild(construct<MenuLabel>(&MenuLabel::text, ""));
    menu->addChild(createMenuItem("Initialize", "double-click",
                                  [=]() { resetWithHistory(); }));
  }

  void addValueField(ui::Menu* menu, const std::string& label,
                     ComplexXYValueField::Field field) {
    menu->addChild(new ComplexXYValueRow(this, label, field));
  }

  void drawDragText(NVGcontext* vg, const std::string& text, Vec pos,
                    NVGcolor color, float size,
                    int align = NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE) {
    auto font = APP->window->loadFont(
        asset::plugin(pluginInstance, "res/fonts/RobotoMono-Regular.ttf"));
    if (!font) return;

    nvgFontFaceId(vg, font->handle);
    nvgFontSize(vg, size);
    nvgTextLetterSpacing(vg, 0.f);
    nvgTextAlign(vg, align);

    float bounds[4];
    nvgTextBounds(vg, pos.x, pos.y, text.c_str(), nullptr, bounds);
    constexpr float padX = 7.f;
    constexpr float padY = 4.f;
    nvgBeginPath(vg);
    nvgRoundedRect(vg, bounds[0] - padX, bounds[1] - padY,
                   bounds[2] - bounds[0] + 2.f * padX,
                   bounds[3] - bounds[1] + 2.f * padY, 4.f);
    nvgFillColor(vg, nvgRGBA(0, 0, 0, 175));
    nvgFill(vg);

    nvgFillColor(vg, color);
    nvgText(vg, pos.x, pos.y, text.c_str(), nullptr);
  }

  void drawRectDragText(NVGcontext* vg, const std::string& text, Vec pos,
                        NVGcolor color, float size) {
    auto monoFont = APP->window->loadFont(
        asset::plugin(pluginInstance, "res/fonts/RobotoMono-Regular.ttf"));
    auto iFont = APP->window->loadFont(
        asset::plugin(pluginInstance, "res/fonts/LibertinusSerif-Italic.ttf"));
    if (!monoFont || !iFont) return;

    std::string prefix = text;
    if (!prefix.empty() && prefix[prefix.size() - 1] == 'i')
      prefix.resize(prefix.size() - 1);

    float charBounds[4];
    float iBounds[4];
    nvgFontSize(vg, size);
    nvgTextLetterSpacing(vg, 0.f);
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgFontFaceId(vg, monoFont->handle);
    nvgTextBounds(vg, 0.f, pos.y, "0", nullptr, charBounds);
    nvgFontFaceId(vg, iFont->handle);
    nvgTextBounds(vg, 0.f, pos.y, "i", nullptr, iBounds);

    float charW = charBounds[2] - charBounds[0];
    float iW = iBounds[2] - iBounds[0];
    float prefixW = charW * prefix.size();
    float totalW = prefixW + iW;
    float x = pos.x - totalW / 2.f;
    constexpr float padX = 7.f;
    constexpr float padY = 4.f;

    nvgBeginPath(vg);
    nvgRoundedRect(vg, x - padX, pos.y - size * 0.5f - padY,
                   totalW + 2.f * padX, size + 2.f * padY, 4.f);
    nvgFillColor(vg, nvgRGBA(0, 0, 0, 175));
    nvgFill(vg);

    nvgFillColor(vg, color);
    nvgFontFaceId(vg, monoFont->handle);
    for (int i = 0; i < (int)prefix.size(); i++) {
      if (prefix[i] == ' ') continue;
      char buf[2] = {prefix[i], '\0'};
      nvgText(vg, x + charW * i, pos.y, buf, nullptr);
    }
    nvgFontFaceId(vg, iFont->handle);
    nvgText(vg, x + prefixW, pos.y, "i", nullptr);
  }

  void cancelDrag() {
    if (!editing) return;
    if (module) {
      module->params[paramA].setValue(origParamAValue);
      module->params[paramA + 1].setValue(origParamBValue);
      newZ = origComplexValue;
    }
    editing = false;
    cancelledDrag = true;
  }

  void setFaded(bool shouldFade) { faded = shouldFade; }

  void onButton(const event::Button& e) override {
    if (e.button == GLFW_MOUSE_BUTTON_RIGHT && e.action == GLFW_PRESS &&
        (e.mods & RACK_MOD_MASK) == 0) {
      createContextMenu();
      e.consume(this);
      return;
    }

    if (e.button == GLFW_MOUSE_BUTTON_LEFT) {
      if (e.action == GLFW_PRESS) {
        destroyTooltip();
        e.consume(this);
        pendingLeftPress = true;
        cancelledDrag = false;
        pendingMousePosition = APP->scene->getMousePos();
        pendingLeftPressTime = glfwGetTime();
      } else if (e.action == GLFW_RELEASE) {
        pendingLeftPress = false;
      }
    }
  }

  void onDoubleClick(const DoubleClickEvent& e) override {
    pendingLeftPress = false;
    cancelDrag();
    resetWithHistory();
    e.consume(this);
  }

  void onEnter(const EnterEvent& e) override { createTooltip(); }

  void onLeave(const LeaveEvent& e) override { destroyTooltip(); }

  void onHover(const HoverEvent& e) override {
    createTooltip();
    e.consume(this);
  }

  void onHoverKey(const HoverKeyEvent& e) override {
    if (editing && e.action == GLFW_PRESS && e.key == GLFW_KEY_ESCAPE) {
      cancelDrag();
      e.consume(this);
    }
  }

  void onDragEnd(const event::DragEnd& e) override {
    pendingLeftPress = false;
    if (!cancelledDrag) editing = false;
    cancelledDrag = false;
  }

  void step() override {
    if (pendingLeftPress && module && APP && APP->scene) {
      Vec mouse = APP->scene->getMousePos();
      bool moved = mouse.minus(pendingMousePosition).norm() > 2.f;
      bool held = glfwGetTime() - pendingLeftPressTime > 0.18;
      if (moved || held) startEditing(pendingMousePosition);
    }

    if (editing && module) {
      if (!dragOverlay && APP && APP->scene) {
        dragOverlay = new ComplexXYDragOverlay();
        dragOverlay->source = this;
        dragOverlay->box.pos = Vec(0.f, 0.f);
        dragOverlay->box.size = APP->scene->box.size;
        APP->scene->addChild(dragOverlay);
      }
      if (dragOverlay && APP && APP->scene) {
        dragOverlay->box.pos = Vec(0.f, 0.f);
        dragOverlay->box.size = APP->scene->box.size;
      }

      if (glfwGetKey(APP->window->win, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        cancelDrag();
        Widget::step();
        return;
      }

      thisPos = APP->scene->getMousePos();

      // in scaled pixels

      pixelsDiff = thisPos.minus(pixelsOrigin);
      newZ = clampToMax(pixelsDiff.div(originalMagnituteRadiusPixels)
                            .mult(origComplexLength));
      pixelsDiff = newZ.mult(originalMagnituteRadiusPixels / origComplexLength);

      Vec ab = rectToParamPair(newZ.x, -newZ.y);
      setParamPairRaw(ab.x, ab.y);
    } else {
      if (dragOverlay) {
        if (dragOverlay->parent) dragOverlay->parent->removeChild(dragOverlay);
        delete dragOverlay;
        dragOverlay = nullptr;
      }

      if (module) {
        Vec xy = getRectValue();
        newZ = Vec(xy.x, -xy.y);
      } else {
        newZ = Vec(-10 + 20 * random::uniform(), -10 + 20 * random::uniform());
      }
    }
  }

  void draw(const DrawArgs& args) override {
    float fullR = box.size.x / 2;
    float drawR = fullR * drawingScale;

    // circle at complex radius 1
    nvgSave(args.vg);
    // nvgResetTransform(args.vg);
    nvgTranslate(args.vg, fullR, fullR);
    nvgBeginPath(args.vg);
    nvgStrokeWidth(args.vg, 2.f);
    nvgFillColor(args.vg, faded ? fadedBackgroundColor : normalBackgroundColor);
    // nvgMoveTo(args.vg,box.size.x/2,box.size.y/2);
    nvgEllipse(args.vg, 0, 0, drawR, drawR);
    nvgClosePath(args.vg);
    nvgFill(args.vg);

    nvgBeginPath(args.vg);
    nvgStrokeWidth(args.vg, faded ? 1.3f : 2.4f);
    nvgStrokeColor(args.vg,
                   faded ? fadedArrowStrokeColor : normalArrowStrokeColor);
    nvgMoveTo(args.vg, 0, 0);

    float length = newZ.norm();
    Vec tip = newZ.normalize().mult(2 * drawR / M_PI * std::atan(length));

    drawArrowTo(args.vg, tip, drawR / 3.2f,
                faded ? fadedArrowFillColor : normalArrowFillColor,
                faded ? fadedArrowStrokeColor : normalArrowStrokeColor,
                faded ? 1.f : 1.5f);
    nvgRestore(args.vg);
  }
  void drawEditingOverlay(const DrawArgs& args) {
    if (editing) {
      float pxRatio = APP->window->pixelRatio;

      nvgSave(args.vg);
      // reset to "undo" the zoom
      nvgResetTransform(args.vg);
      nvgScale(args.vg, pxRatio, pxRatio);

      nvgTranslate(args.vg, pixelsOrigin.x, pixelsOrigin.y);

      // circle at complex radius 1
      float r1Pixels = originalMagnituteRadiusPixels / origComplexLength;
      float rMaxPixels =
          maxVoltage * originalMagnituteRadiusPixels / origComplexLength;
      float labelAngleScale = 1.f / std::sqrt(2.f);
      Vec labelOutward = Vec(labelAngleScale, -labelAngleScale).mult(10.f);
      nvgBeginPath(args.vg);
      nvgStrokeWidth(args.vg, 3.f);
      nvgFillColor(args.vg, nvgRGBA(0, 10, 30, 60));
      nvgEllipse(args.vg, 0, 0, r1Pixels, r1Pixels);
      nvgClosePath(args.vg);
      nvgFill(args.vg);
      nvgBeginPath(args.vg);
      nvgStrokeWidth(args.vg, 3.f);
      nvgStrokeColor(args.vg, nvgRGB(0, 100, 200));
      nvgEllipse(args.vg, 0, 0, r1Pixels, r1Pixels);
      nvgClosePath(args.vg);
      nvgStroke(args.vg);
      Vec r1LabelOutward = Vec(labelAngleScale, -labelAngleScale).mult(3.f);
      Vec r1LabelPos =
          Vec(r1Pixels * labelAngleScale, -r1Pixels * labelAngleScale)
              .plus(r1LabelOutward);
      drawDragText(args.vg, "1v", r1LabelPos, nvgRGB(120, 190, 255), 28.f);

      // circle at the zero point
      nvgBeginPath(args.vg);
      nvgStrokeWidth(args.vg, 3.f);
      nvgFillColor(args.vg, nvgRGB(249, 220, 214));
      nvgEllipse(args.vg, 0, 0, 10.f, 10.f);
      nvgClosePath(args.vg);
      nvgFill(args.vg);

      // circle at the original complex value magnitude
      nvgBeginPath(args.vg);
      nvgStrokeWidth(args.vg, 3.f);
      nvgStrokeColor(args.vg, nvgRGB(140, 120, 80));
      nvgEllipse(args.vg, 0, 0, originalMagnituteRadiusPixels,
                 originalMagnituteRadiusPixels);
      nvgClosePath(args.vg);
      nvgStroke(args.vg);
      Vec originalMagnitudeLabelPos =
          Vec(originalMagnituteRadiusPixels * labelAngleScale,
              -originalMagnituteRadiusPixels * labelAngleScale)
              .plus(labelOutward);
      drawDragText(args.vg, voltageLabelString(origComplexLength),
                   originalMagnitudeLabelPos, nvgRGB(230, 205, 150), 28.f);

      // max radius guide
      nvgBeginPath(args.vg);
      nvgStrokeWidth(args.vg, 3.f);
      nvgStrokeColor(args.vg, nvgRGB(240, 30, 51));
      nvgEllipse(args.vg, 0, 0, rMaxPixels, rMaxPixels);
      nvgClosePath(args.vg);
      nvgStroke(args.vg);
      Vec rMaxLabelPos =
          Vec(rMaxPixels * labelAngleScale, -rMaxPixels * labelAngleScale)
              .plus(labelOutward);
      drawDragText(args.vg, voltageLabelString(maxVoltage), rMaxLabelPos,
                   nvgRGB(255, 120, 120), 28.f);

      // line from the zero point to the original complex number
      Vec originalComplexPixels = origComplexValue.mult(
          originalMagnituteRadiusPixels / origComplexLength);
      nvgBeginPath(args.vg);
      nvgStrokeWidth(args.vg, 5.f);
      nvgStrokeColor(args.vg, nvgRGB(140, 120, 80));
      nvgMoveTo(args.vg, 0, 0);
      nvgLineTo(args.vg, originalComplexPixels.x, originalComplexPixels.y);
      nvgClosePath(args.vg);
      nvgStroke(args.vg);
      // line from the zero point to the users mouse

      nvgSave(args.vg);
      drawArrowTo(args.vg, pixelsDiff);
      nvgRestore(args.vg);

      nvgRestore(args.vg);

      nvgSave(args.vg);
      nvgResetTransform(args.vg);
      nvgScale(args.vg, pxRatio, pxRatio);
      drawRectDragText(
          args.vg, rectDragDisplayString(newZ.x, -newZ.y),
          pixelsOrigin.plus(Vec(0.f, originalMagnituteRadiusPixels + 38.f)),
          nvgRGB(245, 245, 245), 34.f);
      drawRectDragText(
          args.vg,
          rectDragDisplayString(origComplexValue.x, -origComplexValue.y),
          pixelsOrigin.plus(Vec(0.f, originalMagnituteRadiusPixels + 78.f)),
          nvgRGB(230, 205, 150), 28.f);
      drawDragText(
          args.vg, polarDragDisplayString(newZ.x, -newZ.y),
          pixelsOrigin.plus(Vec(0.f, originalMagnituteRadiusPixels + 116.f)),
          nvgRGB(245, 245, 245), 34.f, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
      nvgRestore(args.vg);
    }
  }
  void drawLayer(const DrawArgs& args, int layer) override {
    Widget::drawLayer(args, layer);
  }
};

inline void ComplexXYTooltip::step() {
  if (!source || !source->module || source->editing) return;
  text = source->tooltipString();
  ui::Tooltip::step();
  box.pos = source->getAbsoluteOffset(source->box.size).round();
  if (parent) box = box.nudge(parent->box.zeroPos());
}

inline float ComplexXYValueQuantity::getValue() {
  return source ? source->getFieldValue(field) : 0.f;
}

inline void ComplexXYValueQuantity::setValue(float value) {
  if (!source) return;
  source->setFieldRaw(field, math::clamp(value, getMinValue(), getMaxValue()));
}

inline float ComplexXYValueQuantity::getMinValue() {
  return source ? source->getFieldMin(field) : 0.f;
}

inline float ComplexXYValueQuantity::getMaxValue() {
  return source ? source->getFieldMax(field) : 1.f;
}

inline float ComplexXYValueQuantity::getDefaultValue() {
  return source ? source->getFieldValue(field) : 0.f;
}

inline std::string ComplexXYValueQuantity::getUnit() {
  return field == ComplexXYValueField::Field::Theta ? "°" : "";
}

inline ComplexXYValueRow::ComplexXYValueRow(ComplexXY* source,
                                            const std::string& label,
                                            ComplexXYValueField::Field field) {
  this->source = source;
  this->label = label;
  this->field = field;
  box.size = Vec(270.f, BND_WIDGET_HEIGHT);

  quantity = new ComplexXYValueQuantity;
  quantity->source = source;
  quantity->field = field;
  quantity->label = label;

  slider = new ui::Slider;
  slider->quantity = quantity;
  slider->box.size = Vec(132.f, BND_WIDGET_HEIGHT);
  addChild(slider);

  textField = new ComplexXYValueField;
  textField->source = source;
  textField->field = field;
  textField->box.size = Vec(84.f, BND_WIDGET_HEIGHT);
  if (source) textField->text = source->shortFloatString(quantity->getValue());
  addChild(textField);
}

inline ComplexXYValueRow::~ComplexXYValueRow() { delete quantity; }

inline void ComplexXYValueRow::step() {
  if (slider) slider->box.pos = Vec(34.f, 0.f);
  if (textField) {
    textField->box.pos = Vec(174.f, 0.f);
    if (source && APP->event->selectedWidget != textField) {
      textField->text = source->shortFloatString(quantity->getValue());
    }
  }
  ui::MenuEntry::step();
}

inline void ComplexXYValueRow::draw(const DrawArgs& args) {
  std::shared_ptr<Font> font = APP->window->uiFont;
  if (font) {
    nvgFontFaceId(args.vg, font->handle);
    nvgFontSize(args.vg, 13.f);
    nvgTextLetterSpacing(args.vg, 0.f);
    nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgFillColor(args.vg, nvgRGB(0xee, 0xee, 0xee));
    nvgText(args.vg, 8.f, box.size.y * 0.5f, label.c_str(), nullptr);
  }
  ui::MenuEntry::draw(args);
}

inline void ComplexXYValueField::step() { ui::TextField::step(); }

inline void ComplexXYValueField::onSelectKey(const SelectKeyEvent& e) {
  if (e.action == GLFW_PRESS &&
      (e.key == GLFW_KEY_ENTER || e.key == GLFW_KEY_KP_ENTER)) {
    commit();
    if (ui::MenuOverlay* overlay = getAncestorOfType<ui::MenuOverlay>()) {
      overlay->requestDelete();
    }
    e.consume(this);
  }

  if (!e.getTarget()) ui::TextField::onSelectKey(e);
}

inline void ComplexXYValueField::commit() {
  if (!source) return;
  try {
    float value = std::stof(text);
    Vec xy = source->getRectValue();
    Vec rt = source->getPolarValue();
    switch (field) {
      case Field::X:
        source->setRectWithHistory(value, xy.y, "set complex x");
        break;
      case Field::Y:
        source->setRectWithHistory(xy.x, value, "set complex y");
        break;
      case Field::R:
        source->setPolarWithHistory(value, rt.y, "set complex radius");
        break;
      case Field::Theta:
        source->setPolarWithHistory(rt.x, value, "set complex theta");
        break;
    }
  } catch (...) {
  }
}

inline void ComplexXYDragOverlay::step() {
  if (!source || !source->editing) {
    return;
  }
  if (APP && APP->scene) {
    box.pos = Vec(0.f, 0.f);
    box.size = APP->scene->box.size;
  }
  Widget::step();
}

inline void ComplexXYDragOverlay::draw(const DrawArgs& args) {
  if (source) source->drawEditingOverlay(args);
}

struct CompolyModeParam : SwitchQuantity {
  CompolyModeParam() {
    snapEnabled = true;
    randomizeEnabled = false;
    resetEnabled = false;
    labels = {
        "Rectangular Interleaved",
        "Polar Interleaved",
        "Rectangular Separated",
        "Polar Separated",
    };
  }

  std::string getDisplayValueString() override {
    int mode = getValue();
    if (mode == RECT_INTERLEAVED) {
      return "Rectangular Interleaved";
    } else if (mode == POLAR_INTERLEAVED) {
      return "Polar Interleaved";
    } else if (mode == RECT_SEPARATED) {
      return "Rectangular Separated";
    } else if (mode == POLAR_SEPARATED) {
      return "Polar Separated";
    }
    return "";
  }
};

template <int TModeParamIndex, int blockNum = 0>
struct CompolyPortInfo : PortInfo {
  std::string getName() override { return name + ", " + getModeName(); }
  std::string getModeName() {
    if (module) {
      int mode = module->params[TModeParamIndex].getValue();
      if (mode == RECT_INTERLEAVED) {
        return blockNum == 0 ? "x,y #1-8" : "x,y #9-16";
      } else if (mode == POLAR_INTERLEAVED) {
        return blockNum == 0 ? "r,θ #1-8" : "r,θ #9-16";
      } else if (mode == RECT_SEPARATED) {
        return blockNum == 0 ? "x" : "y";
      } else if (mode == POLAR_SEPARATED) {
        return blockNum == 0 ? "r" : "θ";
      }
    }
    return "";
  }
};

struct ComplexOutport : ComputerscareSvgPort {
  ComplexOutport() {
    // setSvg(APP->window->loadSvg(asset::plugin(pluginInstance,
    // "res/complex-outjack-skewR.svg")));
  }
};

struct CompolyTypeLabelSwitch : app::SvgSwitch {
  widget::TransformWidget* tw;
  CompolyTypeLabelSwitch() {
    shadow->opacity = 0.f;
    addFrame(APP->window->loadSvg(
        asset::plugin(pluginInstance, "res/complex-labels/xy.svg")));
    addFrame(APP->window->loadSvg(
        asset::plugin(pluginInstance, "res/complex-labels/rtheta.svg")));
    addFrame(APP->window->loadSvg(
        asset::plugin(pluginInstance, "res/complex-labels/x.svg")));
    addFrame(APP->window->loadSvg(
        asset::plugin(pluginInstance, "res/complex-labels/r.svg")));
  }
};

struct CompolySingleLabelSwitch : SvgWidget {
  widget::TransformWidget* tw;
  CompolySingleLabelSwitch(std::string svgLabelFilename = "z") {
    setSvg(APP->window->loadSvg(asset::plugin(
        pluginInstance, "res/complex-labels/" + svgLabelFilename + ".svg")));
  }
};

struct ScaledSvgWidget : TransformWidget {
  SvgWidget* svg;
  ScaledSvgWidget(float scale) {
    TransformWidget* tw = new TransformWidget();
    tw->scale(scale);
    svg = new SvgWidget();

    tw->addChild(svg);

    addChild(tw);
  }
  void setSVG(std::string path) {
    svg->setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, path)));
  }
};

template <typename BASE>
struct CompolyInOrOutWidget : Widget {
  ComputerscareComplexBase* module;
  int paramID;
  int numPorts = 2;
  int lastOutMode = -1;
  std::vector<BASE*> ports;

  ScaledSvgWidget* leftLabel;
  ScaledSvgWidget* rightLabel;

  CompolyInOrOutWidget(math::Vec pos) {
    leftLabel = new ScaledSvgWidget(0.5);
    leftLabel->box.pos = pos.minus(Vec(0, 10));
    leftLabel->setSVG("res/complex-labels/r.svg");

    rightLabel = new ScaledSvgWidget(0.5);
    rightLabel->box.pos = pos.plus(Vec(50, -10));
    rightLabel->setSVG("res/complex-labels/r.svg");

    addChild(leftLabel);
    addChild(rightLabel);
  }

  void setLabels(std::string leftFilename, std::string rightFilename) {
    leftLabel->setSVG("res/complex-labels/" + leftFilename);
    rightLabel->setSVG("res/complex-labels/" + rightFilename);
    rightLabel->visible = true;
  }
  void setLabels(std::string leftFilename) {
    leftLabel->setSVG("res/complex-labels/" + leftFilename);
    rightLabel->visible = false;
  }
  void setPorts(std::string leftPortFilename, std::string rightPortFilename) {
    ports[0]->setSvg(APP->window->loadSvg(
        asset::plugin(pluginInstance, "res/" + leftPortFilename)));
    ports[1]->setSvg(APP->window->loadSvg(
        asset::plugin(pluginInstance, "res/" + rightPortFilename)));
  }

  void step() {
    if (module) {
      int outMode = module->params[paramID].getValue();

      if (lastOutMode != outMode) {
        lastOutMode = outMode;
        if (ports[0] && ports[1]) {
          if (outMode == RECT_INTERLEAVED) {
            setPorts("complex-outjack-skewL.svg", "complex-outjack-skewL.svg");
            setLabels("xy.svg");
          } else if (outMode == POLAR_INTERLEAVED) {
            setPorts("complex-outjack-slantL.svg",
                     "complex-outjack-slantL.svg");
            setLabels("rtheta.svg");
          } else if (outMode == RECT_SEPARATED) {
            setPorts("complex-outjack-skewL.svg", "complex-outjack-skewR.svg");
            setLabels("x.svg", "yy.svg");
          } else if (outMode == POLAR_SEPARATED) {
            setPorts("complex-outjack-slantR.svg",
                     "complex-outjack-slantL.svg");
            setLabels("r.svg", "theta.svg");
          }
        }
      }
    } else {
      setPorts("complex-outjack-skewL.svg", "complex-outjack-skewL.svg");
      setLabels("xy.svg");
    }

    Widget::step();
  }
};

struct CompolyPortsWidget : CompolyInOrOutWidget<ComplexOutport> {
  ComplexOutport* port;
  CompolySingleLabelSwitch* compolyLabel;
  TransformWidget* compolyLabelTransform;
  Vec labelOffset;
  int hoveredModeHotspots = 0;

  struct ModeHotspot : app::ParamWidget {
    CompolyPortsWidget* owner = nullptr;
    bool hovering = false;

    void draw(const DrawArgs& args) override {}

    void onEnter(const event::Enter& e) override {
      if (!hovering && owner) owner->hoveredModeHotspots++;
      hovering = true;
      app::ParamWidget::onEnter(e);
    }

    void onLeave(const event::Leave& e) override {
      if (hovering && owner)
        owner->hoveredModeHotspots =
            std::max(0, owner->hoveredModeHotspots - 1);
      hovering = false;
      app::ParamWidget::onLeave(e);
    }

    void onButton(const event::Button& e) override {
      if (owner && e.button == GLFW_MOUSE_BUTTON_LEFT &&
          e.action == GLFW_PRESS) {
        owner->cycleMode();
        e.consume(this);
        return;
      }
      app::ParamWidget::onButton(e);
    }
  };

  ModeHotspot* portLabelsHotspot = nullptr;

  CompolyPortsWidget(math::Vec pos, ComputerscareComplexBase* cModule,
                     int firstPortID, int compolyTypeParamID, float scale = 1.0,
                     bool isOutput = true, std::string labelSvgFilename = "z")
      : CompolyInOrOutWidget(pos) {
    module = cModule;
    paramID = compolyTypeParamID;

    compolyLabel = new CompolySingleLabelSwitch(labelSvgFilename);

    compolyLabelTransform = new TransformWidget();
    compolyLabelTransform->box.pos = pos.minus(Vec(40, 0));
    compolyLabelTransform->scale(scale);

    compolyLabelTransform->addChild(compolyLabel);
    addChild(compolyLabelTransform);

    ports.resize(numPorts);

    for (int i = 0; i < numPorts; i++) {
      math::Vec myPos = pos.plus(Vec(30 * i, 0));
      if (isOutput) {
        port = createOutput<ComplexOutport>(myPos, cModule, firstPortID + i);
      } else {
        port = createInput<ComplexOutport>(myPos, cModule, firstPortID + i);
      }
      ports[i] = port;
      addChild(port);
    }

    portLabelsHotspot = createModeHotspot(cModule, compolyTypeParamID);
    addChild(portLabelsHotspot);
  }

  ModeHotspot* createModeHotspot(ComputerscareComplexBase* cModule,
                                 int compolyTypeParamID) {
    ModeHotspot* hotspot = new ModeHotspot();
    hotspot->owner = this;
    hotspot->app::ParamWidget::module = cModule;
    hotspot->app::ParamWidget::paramId = compolyTypeParamID;
    hotspot->initParamQuantity();
    return hotspot;
  }

  void setHotspotRect(ModeHotspot* hotspot, Rect rect, bool visible = true) {
    if (!visible && hotspot->hovering) {
      hotspot->hovering = false;
      hoveredModeHotspots = std::max(0, hoveredModeHotspots - 1);
    }
    hotspot->box = rect;
    hotspot->visible = visible;
  }

  void syncModeHotspots() {
    setHotspotRect(portLabelsHotspot, modeHeaderRect());
  }

  Rect modeHeaderRect() {
    float left = leftLabel->box.pos.x - 3.f;
    float right = left + 60.f;
    if (ports.size() >= 2 && ports[0] && ports[1]) {
      left = std::min(ports[0]->box.pos.x, ports[1]->box.pos.x) - 3.f;
      float firstRight = ports[0]->box.pos.x + ports[0]->box.size.x;
      float secondRight = ports[1]->box.pos.x + ports[1]->box.size.x;
      right = std::max(firstRight, secondRight) + 3.f;
    }

    float top = leftLabel->box.pos.y + 12.f;
    if (ports.size() >= 1 && ports[0]) {
      top = ports[0]->box.pos.y - 11.f;
    }
    return Rect(Vec(left, top), Vec(right - left, 14.f));
  }

  void cycleMode() {
    if (!module) return;
    int mode = module->params[paramID].getValue();
    int nextMode = mode >= POLAR_SEPARATED ? RECT_INTERLEAVED : mode + 1;
    if (ParamQuantity* pq = module->paramQuantities[paramID]) {
      pq->setValue(nextMode);
    } else {
      module->params[paramID].setValue(nextMode);
    }
  }

  int mode() const {
    if (module) return module->params[paramID].getValue();
    return RECT_INTERLEAVED;
  }

  void drawModeHeaderPath(NVGcontext* vg, Rect r) {
    int currentMode = mode();
    if (currentMode == RECT_INTERLEAVED) {
      float skew = 3.f;
      nvgMoveTo(vg, r.pos.x + skew, r.pos.y);
      nvgLineTo(vg, r.pos.x + r.size.x, r.pos.y);
      nvgLineTo(vg, r.pos.x + r.size.x - skew, r.pos.y + r.size.y);
      nvgLineTo(vg, r.pos.x, r.pos.y + r.size.y);
      nvgClosePath(vg);
    } else if (currentMode == POLAR_INTERLEAVED) {
      nvgRoundedRect(vg, r.pos.x, r.pos.y, r.size.x, r.size.y,
                     r.size.y * 0.48f);
    } else {
      nvgRect(vg, r.pos.x, r.pos.y, r.size.x, r.size.y);
    }
  }

  void step() override {
    syncModeHotspots();
    CompolyInOrOutWidget<ComplexOutport>::step();
  }

  void draw(const DrawArgs& args) override {
    if (hoveredModeHotspots > 0) {
      Rect r = modeHeaderRect();
      nvgBeginPath(args.vg);
      drawModeHeaderPath(args.vg, r);
      nvgFillColor(args.vg, nvgRGBA(0x24, 0xc9, 0xa6, 0x28));
      nvgFill(args.vg);
      nvgStrokeWidth(args.vg, 1.f);
      nvgStrokeColor(args.vg, COLOR_COMPUTERSCARE_DARK_GREEN);
      nvgStroke(args.vg);
    }
    CompolyInOrOutWidget<ComplexOutport>::draw(args);
  }
};

// ─── ComplexDisplayWidget ────────────────────────────────────────────────────
// Renders a complex number as styled text.  Number tokens are drawn in
// normalColor, operator tokens in dimColor, and the imaginary unit / angle
// symbol token in accentColor.  The SVG glyphs for "i" and "e" (from
// res/complex-labels/) are composed as scaled child widgets positioned each
// frame to sit inline with the surrounding text.

struct ComplexDisplayWidget : Widget {
  Module* module = nullptr;
  int paramX = 0;
  int paramY = 0;

  enum class DisplayMode { Rect, Polar };
  enum class SourceMode { Rect, Polar };
  DisplayMode displayMode = DisplayMode::Rect;
  SourceMode sourceMode = SourceMode::Rect;
  cpx::complex_math::AngleUnit angleUnit = cpx::complex_math::AngleUnit::Degree;
  cpx::complex_math::PolarDisplayStyle polarStyle =
      cpx::complex_math::PolarDisplayStyle::Engineering;
  int decimals = -1;

  NVGcolor normalColor = nvgRGB(0xe0, 0xe0, 0xe0);
  NVGcolor dimColor = nvgRGB(0x78, 0x78, 0x78);
  NVGcolor accentColor = COLOR_COMPUTERSCARE_LIGHT_GREEN;

  // SVG glyph children — positioned each frame in draw()
  ScaledSvgWidget* iGlyph = nullptr;
  ScaledSvgWidget* eGlyph = nullptr;
  ComplexDisplayWidget* overlayWidget = nullptr;
  ComplexDisplayWidget* sourceWidget = nullptr;
  bool sceneOverlay = false;

  ComplexDisplayWidget() {
    iGlyph = new ScaledSvgWidget(0.5f);
    iGlyph->setSVG("res/complex-labels/i.svg");
    iGlyph->visible = false;
    addChild(iGlyph);

    eGlyph = new ScaledSvgWidget(0.5f);
    eGlyph->setSVG("res/complex-labels/e.svg");
    eGlyph->visible = false;
    addChild(eGlyph);
  }

  ~ComplexDisplayWidget() override {
    if (overlayWidget) {
      if (overlayWidget->parent)
        overlayWidget->parent->removeChild(overlayWidget);
      delete overlayWidget;
      overlayWidget = nullptr;
    }
  }

  void syncFromSource(ComplexDisplayWidget* source) {
    module = source->module;
    paramX = source->paramX;
    paramY = source->paramY;
    displayMode = source->displayMode;
    sourceMode = source->sourceMode;
    angleUnit = source->angleUnit;
    polarStyle = source->polarStyle;
    decimals = source->decimals;
    normalColor = source->normalColor;
    dimColor = source->dimColor;
    accentColor = source->accentColor;

    Vec topLeft = source->getAbsoluteOffset(Vec(0.f, 0.f));
    Vec bottomRight = source->getAbsoluteOffset(source->box.size);
    box.pos = topLeft;
    box.size = bottomRight.minus(topLeft);
  }

  void step() override {
    if (!sceneOverlay) {
      if (!overlayWidget && APP && APP->scene) {
        overlayWidget = new ComplexDisplayWidget();
        overlayWidget->sceneOverlay = true;
        overlayWidget->sourceWidget = this;
        APP->scene->addChild(overlayWidget);
      }
      if (overlayWidget) overlayWidget->syncFromSource(this);
    } else if (sourceWidget) {
      syncFromSource(sourceWidget);
    }

    Widget::step();
  }

  void draw(const DrawArgs& args) override {
    if (!sceneOverlay) {
      iGlyph->visible = false;
      eGlyph->visible = false;
      return;
    }

    if (!module) {
      iGlyph->visible = false;
      eGlyph->visible = false;
      return;
    }

    auto font = APP->window->loadFont(
        asset::plugin(pluginInstance, "res/fonts/RobotoMono-Regular.ttf"));
    if (!font) {
      return;
    }
    auto symbolFont = APP->window->loadFont(
        asset::plugin(pluginInstance, "res/fonts/LibertinusSerif-Italic.ttf"));

    float vx = module->params[paramX].getValue();
    float vy = module->params[paramY].getValue();

    bool polar = (displayMode == DisplayMode::Polar);
    float fsize = box.size.y * 0.72f;
    float midY = box.size.y * 0.55f;
    float bounds[4];

    nvgFontFaceId(args.vg, font->handle);
    nvgFontSize(args.vg, fsize);
    nvgTextLetterSpacing(args.vg, 0.f);
    nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);

    int displayDecimals = decimals < 0 ? 2 : decimals;
    auto fixedNumberString = [&](float value, int integerDigits,
                                 bool showPos = false) {
      std::ostringstream ss;
      float absValue = std::fabs(value);
      int width = integerDigits + 1 + displayDecimals;
      ss << std::fixed << std::setprecision(displayDecimals) << std::setw(width)
         << absValue;
      std::string numeric = ss.str();
      if (showPos) return std::string(value < 0.f ? "-" : "+") + numeric;
      if (value < 0.f) return std::string("-") + numeric;
      return std::string(" ") + numeric;
    };
    auto drawCellText = [&](const std::string& s, float x, NVGcolor color) {
      nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);
      nvgFillColor(args.vg, color);
      nvgText(args.vg, x, midY, s.c_str(), nullptr);
    };
    auto drawFixedCells = [&](const std::string& s, float x, float charW,
                              NVGcolor color) {
      for (int i = 0; i < (int)s.size(); i++) {
        if (s[i] == ' ') continue;
        char buf[2] = {s[i], '\0'};
        drawCellText(buf, x + charW * i, color);
      }
      return x + charW * s.size();
    };
    auto fixedOperator = [&](const std::string& s, float x, float charW,
                             NVGcolor color) {
      for (int i = 0; i < (int)s.size(); i++) {
        char buf[2] = {s[i], '\0'};
        drawCellText(buf, x + charW * i, color);
      }
      return x + charW * s.size();
    };
    auto textWidth = [&](const std::string& s) {
      nvgTextBounds(args.vg, 0.f, midY, s.c_str(), nullptr, bounds);
      return bounds[2] - bounds[0];
    };

    // Glyph sizing: scale svg children to match the text cap-height.
    // i.svg viewBox height is 9.2, e.svg viewBox height is 7.0.
    // At scale 0.5 the natural mm->px mapping gives ~17px for i, ~13px for e
    // at typical rack DPI.  We just position them; ScaledSvgWidget handles
    // scale.
    float glyphH = fsize * 0.85f;
    float iW = glyphH * 0.35f;  // i is narrow
    float eW = glyphH * 0.80f;

    iGlyph->visible = false;
    eGlyph->visible = false;

    if (!polar) {
      float fieldStartX = 1.f;
      float charW = textWidth("0");
      float realValue =
          sourceMode == SourceMode::Polar ? vx * std::cos(vy) : vx;
      float imagValue =
          sourceMode == SourceMode::Polar ? vx * std::sin(vy) : vy;
      cpx::complex_text::FixedComplexTextStyle style;
      style.numberColor = normalColor;
      style.operatorColor = dimColor;
      style.accentColor = accentColor;
      style.textAlign = NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE;
      style.fontSize = fsize;
      style.symbolFontSize = fsize;
      style.decimals = displayDecimals;
      cpx::complex_text::drawRect(
          args.vg, font->handle, symbolFont ? symbolFont->handle : font->handle,
          fieldStartX, midY, realValue, imagValue, charW, style);
    } else if (polarStyle ==
               cpx::complex_math::PolarDisplayStyle::Engineering) {
      float fieldStartX = 1.f;
      float charW = textWidth("0");

      float r = vx;
      float theta = vy;
      if (sourceMode == SourceMode::Rect) {
        r = std::hypot(vx, vy);
        theta = std::atan2(vy, vx);
      }
      cpx::complex_text::FixedComplexTextStyle style;
      style.numberColor = normalColor;
      style.operatorColor = dimColor;
      style.accentColor = accentColor;
      style.textAlign = NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE;
      style.fontSize = fsize;
      style.decimals = displayDecimals;
      style.angleYOffset = -1.f;
      style.degreeYOffset = -7.f;
      cpx::complex_text::drawPolar(args.vg, font->handle, fieldStartX, midY, r,
                                   theta, charW, style);
    } else {
      float fieldStartX = 1.f;
      float charW = textWidth("0");
      float cursorX = fieldStartX;

      float r = vx;
      float theta = vy;
      if (sourceMode == SourceMode::Rect) {
        r = std::hypot(vx, vy);
        theta = std::atan2(vy, vx);
      }
      float displayAngle = angleUnit == cpx::complex_math::AngleUnit::Degree
                               ? theta * 180.f / cpx::complex_math::pi
                               : theta;
      std::string unitSuffix =
          angleUnit == cpx::complex_math::AngleUnit::Degree ? "°" : " rad";

      cursorX =
          drawFixedCells(fixedNumberString(r, 2), cursorX, charW, normalColor);

      cursorX += charW;
      eGlyph->box.pos = Vec(cursorX, midY - glyphH);
      eGlyph->box.size = Vec(eW, glyphH);
      eGlyph->visible = true;
      cursorX += eW + charW;

      cursorX = fixedOperator("^(", cursorX, charW, dimColor);

      iGlyph->box.pos = Vec(cursorX, midY - glyphH);
      iGlyph->box.size = Vec(iW, glyphH);
      iGlyph->visible = true;
      cursorX += iW;

      cursorX = fixedOperator("·", cursorX, charW, dimColor);
      cursorX = drawFixedCells(fixedNumberString(displayAngle, 3, true),
                               cursorX, charW, dimColor);
      drawCellText(unitSuffix + ")", cursorX, dimColor);
    }

    Widget::draw(args);
  }
};

}  // namespace cpx
