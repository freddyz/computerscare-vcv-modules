#pragma once

#include "SwitchableComplexControl.hpp"

namespace cpx {

enum class PerspectivePentagonSize {
  Small = 0,
  Medium = 1,
  Large = 2,
};

enum class PerspectivePentagonPreset {
  DownLeft = 0,
  DownRight = 1,
  UpLeft = 2,
  UpRight = 3,
  DeepLeft = 4,
  ShallowLeft = 5,
};

enum class PerspectivePentagonShape {
  Pentagon = 0,
  Rectangle = 1,
};

struct PerspectivePentagonSettings {
  int size = static_cast<int>(PerspectivePentagonSize::Medium);
  int preset = static_cast<int>(PerspectivePentagonPreset::DownLeft);
  int colorVariation = 1;
  int shape = static_cast<int>(PerspectivePentagonShape::Rectangle);
};

struct PerspectivePentagonGeometry {
  Vec face[5];
  Vec extruded[5];
  Vec vanishingPoint;
  float depletion = 0.72f;
  int pointCount = 5;
};

inline int clampPerspectivePentagonSize(int size) {
  return std::max(0, std::min(2, size));
}

inline int clampPerspectivePentagonPreset(int preset) {
  return std::max(0, std::min(5, preset));
}

inline int clampPerspectivePentagonColorVariation(int variation) {
  return std::max(0, std::min(4, variation));
}

inline int clampPerspectivePentagonShape(int shape) {
  return std::max(0, std::min(1, shape));
}

inline std::string perspectivePentagonSizeName(int size) {
  switch (clampPerspectivePentagonSize(size)) {
    case 0:
      return "small";
    case 2:
      return "large";
  }
  return "medium";
}

inline std::string perspectivePentagonPresetName(int preset) {
  switch (clampPerspectivePentagonPreset(preset)) {
    case 1:
      return "down right";
    case 2:
      return "up left";
    case 3:
      return "up right";
    case 4:
      return "deep left";
    case 5:
      return "shallow left";
  }
  return "down left";
}

inline std::string perspectivePentagonColorVariationName(int variation) {
  switch (clampPerspectivePentagonColorVariation(variation)) {
    case 0:
      return "none";
    case 2:
      return "medium";
    case 3:
      return "high";
    case 4:
      return "wild";
  }
  return "low";
}

inline std::string perspectivePentagonShapeName(int shape) {
  switch (clampPerspectivePentagonShape(shape)) {
    case 1:
      return "rectangle";
  }
  return "pentagon";
}

struct PerspectivePentagonDisplay : Widget {
  PerspectivePentagonSettings* settings = nullptr;
  int seed = 0;
  Vec ownerPos;
  Vec canvasSize = Vec(120.f, 380.f);
  NVGcolor faceColor = nvgRGB(0xa9, 0xad, 0xaa);
  NVGcolor sideColor = nvgRGB(0x69, 0x6e, 0x6b);
  int baseFaceShade = 0x82;
  int baseSideShade = 0x5d;
  int sideShadeStep = 0x0c;

  int currentSize() const {
    return settings ? clampPerspectivePentagonSize(settings->size)
                    : static_cast<int>(PerspectivePentagonSize::Medium);
  }

  int currentPreset() const {
    return settings ? clampPerspectivePentagonPreset(settings->preset)
                    : static_cast<int>(PerspectivePentagonPreset::DownLeft);
  }

  int currentColorVariation() const {
    return settings ? clampPerspectivePentagonColorVariation(
                          settings->colorVariation)
                    : 1;
  }

  int currentShape() const {
    return settings ? clampPerspectivePentagonShape(settings->shape)
                    : static_cast<int>(PerspectivePentagonShape::Rectangle);
  }

  float contentScale() const {
    switch (currentSize()) {
      case 0:
        return 0.78f;
      case 2:
        return 1.08f;
    }
    return 0.94f;
  }

  Vec faceInset() const {
    switch (currentSize()) {
      case 0:
        return Vec(4.5f, 4.5f);
      case 2:
        return Vec(1.2f, 1.2f);
    }
    return Vec(2.7f, 2.7f);
  }

  float distortion(int point, int axis) const {
    unsigned int x = 2166136261u;
    x = (x ^ (unsigned int)(seed + 31 * point + 131 * axis)) * 16777619u;
    x ^= x >> 13;
    x *= 1274126177u;
    float unit = (float)(x & 0xffffu) / 65535.f;
    return (unit - 0.5f) * 2.f;
  }

  int shadeOffset(int salt, float multiplier = 1.f) const {
    static constexpr float ranges[] = {0.f, 8.f, 18.f, 34.f, 78.f};
    int range = std::round(ranges[currentColorVariation()] * multiplier);
    if (range <= 0) return 0;
    return std::round(distortion(seed + salt, 3) * range);
  }

  PerspectivePentagonGeometry geometry() const {
    PerspectivePentagonGeometry g;
    int preset = currentPreset();
    Vec vp = Vec(-canvasSize.x * 0.45f, canvasSize.y * 1.22f);
    float depletion = 0.72f;
    if (preset == 1) vp = Vec(canvasSize.x * 1.45f, canvasSize.y * 1.22f);
    if (preset == 2) vp = Vec(-canvasSize.x * 0.45f, -canvasSize.y * 0.35f);
    if (preset == 3) vp = Vec(canvasSize.x * 1.45f, -canvasSize.y * 0.35f);
    if (preset == 4) {
      vp = Vec(-canvasSize.x * 0.9f, canvasSize.y * 1.65f);
      depletion = 0.88f;
    }
    if (preset == 5) {
      vp = Vec(-canvasSize.x * 0.18f, canvasSize.y * 1.08f);
      depletion = 0.56f;
    }
    g.vanishingPoint = vp.minus(ownerPos);
    g.depletion = depletion;

    Vec inset = faceInset();
    float maxJitter = std::min(box.size.x, box.size.y) * 0.04f;
    if (currentShape() ==
        static_cast<int>(PerspectivePentagonShape::Rectangle)) {
      g.pointCount = 4;
      Vec points[4] = {Vec(inset.x, inset.y),
                       Vec(box.size.x - inset.x, inset.y),
                       Vec(box.size.x - inset.x, box.size.y - inset.y),
                       Vec(inset.x, box.size.y - inset.y)};
      for (int i = 0; i < 4; i++) {
        Vec p = points[i];
        p.x += distortion(i, 0) * maxJitter;
        p.y += distortion(i, 1) * maxJitter;
        g.face[i] = p;
        g.extruded[i] = p.plus(g.vanishingPoint.minus(p).mult(depletion));
      }
    } else {
      g.pointCount = 5;
      Vec center = box.size.mult(0.5f);
      float rx = std::max(1.f, box.size.x * 0.5f - inset.x);
      float ry = std::max(1.f, box.size.y * 0.5f - inset.y);
      float angle0 = -M_PI * 0.5f;
      maxJitter = std::min(rx, ry) * 0.08f;
      for (int i = 0; i < 5; i++) {
        float a = angle0 + i * 2.f * M_PI / 5.f;
        Vec p = center.plus(Vec(std::cos(a) * rx, std::sin(a) * ry));
        p.x += distortion(i, 0) * maxJitter;
        p.y += distortion(i, 1) * maxJitter;
        g.face[i] = p;
        g.extruded[i] = p.plus(g.vanishingPoint.minus(p).mult(depletion));
      }
    }
    return g;
  }

  Rect contentRect() const {
    Vec inset = faceInset().plus(Vec(3.f, 4.f));
    float scale = contentScale();
    Vec size = Vec(std::max(1.f, box.size.x - inset.x * 2.f),
                   std::max(1.f, box.size.y - inset.y * 2.f));
    Vec scaled = size.mult(scale);
    return Rect(box.size.minus(scaled).mult(0.5f), scaled);
  }

  void drawFacePath(NVGcontext* vg, const PerspectivePentagonGeometry& g) {
    nvgMoveTo(vg, g.face[0].x, g.face[0].y);
    for (int i = 1; i < g.pointCount; i++) {
      nvgLineTo(vg, g.face[i].x, g.face[i].y);
    }
    nvgClosePath(vg);
  }

  int sideOrientationShadeOffset(const PerspectivePentagonGeometry& g,
                                 int edgeIndex) const {
    if (g.pointCount != 4) return 0;
    if (edgeIndex == 2) return -14;
    if (edgeIndex == 3) return 4;
    return 0;
  }

  void drawHighlight(const DrawArgs& args) {
    PerspectivePentagonGeometry g = geometry();
    NVGcolor highlightColor = nvgRGBA(238, 209, 61, 58);
    for (int i = 0; i < g.pointCount; i++) {
      int j = (i + 1) % g.pointCount;
      nvgBeginPath(args.vg);
      nvgMoveTo(args.vg, g.face[i].x, g.face[i].y);
      nvgLineTo(args.vg, g.face[j].x, g.face[j].y);
      nvgLineTo(args.vg, g.extruded[j].x, g.extruded[j].y);
      nvgLineTo(args.vg, g.extruded[i].x, g.extruded[i].y);
      nvgClosePath(args.vg);
      nvgFillColor(args.vg, highlightColor);
      nvgFill(args.vg);
    }

    nvgBeginPath(args.vg);
    drawFacePath(args.vg, g);
    nvgFillColor(args.vg, highlightColor);
    nvgFill(args.vg);
  }

  void drawSides(const DrawArgs& args) {
    PerspectivePentagonGeometry g = geometry();
    float edgeScores[5];
    int visibleEdges[3] = {0, 1, 2};
    for (int i = 0; i < g.pointCount; i++) {
      int j = (i + 1) % g.pointCount;
      Vec edgeMid = g.face[i].plus(g.face[j]).mult(0.5f);
      edgeScores[i] = g.vanishingPoint.minus(edgeMid).norm();
    }

    for (int slot = 0; slot < 3; slot++) {
      int best = -1;
      for (int i = 0; i < g.pointCount; i++) {
        bool used = false;
        for (int prev = 0; prev < slot; prev++) {
          if (visibleEdges[prev] == i) used = true;
        }
        if (used) continue;
        if (best < 0 || edgeScores[i] < edgeScores[best]) best = i;
      }
      visibleEdges[slot] = best;
    }

    for (int slot = 2; slot >= 0; slot--) {
      int i = visibleEdges[slot];
      int j = (i + 1) % g.pointCount;

      nvgBeginPath(args.vg);
      nvgMoveTo(args.vg, g.face[i].x, g.face[i].y);
      nvgLineTo(args.vg, g.face[j].x, g.face[j].y);
      nvgLineTo(args.vg, g.extruded[j].x, g.extruded[j].y);
      nvgLineTo(args.vg, g.extruded[i].x, g.extruded[i].y);
      nvgClosePath(args.vg);
      int shade =
          std::max(0, std::min(255, baseSideShade + sideShadeStep * slot +
                                        sideOrientationShadeOffset(g, i) +
                                        shadeOffset(i)));
      nvgFillColor(args.vg, nvgRGB(shade, shade, shade));
      nvgFill(args.vg);
    }
  }

  void drawFace(const DrawArgs& args) {
    PerspectivePentagonGeometry g = geometry();
    nvgBeginPath(args.vg);
    drawFacePath(args.vg, g);
    int faceShade =
        std::max(0, std::min(255, baseFaceShade + shadeOffset(43, 0.6f)));
    nvgFillColor(args.vg, nvgRGB(faceShade, faceShade, faceShade));
    nvgFill(args.vg);
  }

  void drawBack(const DrawArgs& args) {
    drawSides(args);
    drawFace(args);
  }

  void draw(const DrawArgs& args) override { drawBack(args); }
};

struct PerspectivePentagonMenuItem : MenuItem {
  int* value = nullptr;
  int nextValue = 0;

  void onAction(const event::Action& e) override {
    if (value) *value = nextValue;
  }
};

inline void addPerspectivePentagonMenuItems(
    Menu* menu, PerspectivePentagonSettings* settings) {
  menu->addChild(construct<MenuLabel>(&MenuLabel::text, "Pentagon Display"));
  menu->addChild(construct<MenuLabel>(
      &MenuLabel::text, "shape: " + perspectivePentagonShapeName(
                                        settings ? settings->shape : 1)));
  for (int i = 0; i < 2; i++) {
    PerspectivePentagonMenuItem* item = new PerspectivePentagonMenuItem();
    item->text = perspectivePentagonShapeName(i);
    item->rightText = CHECKMARK(
        settings && clampPerspectivePentagonShape(settings->shape) == i);
    item->value = settings ? &settings->shape : nullptr;
    item->nextValue = i;
    menu->addChild(item);
  }

  menu->addChild(new MenuSeparator);
  menu->addChild(construct<MenuLabel>(
      &MenuLabel::text,
      "size: " + perspectivePentagonSizeName(settings ? settings->size : 1)));
  for (int i = 0; i < 3; i++) {
    PerspectivePentagonMenuItem* item = new PerspectivePentagonMenuItem();
    item->text = perspectivePentagonSizeName(i);
    item->rightText = CHECKMARK(settings && settings->size == i);
    item->value = settings ? &settings->size : nullptr;
    item->nextValue = i;
    menu->addChild(item);
  }

  menu->addChild(new MenuSeparator);
  menu->addChild(construct<MenuLabel>(
      &MenuLabel::text,
      "perspective: " +
          perspectivePentagonPresetName(settings ? settings->preset : 0)));
  for (int i = 0; i < 6; i++) {
    PerspectivePentagonMenuItem* item = new PerspectivePentagonMenuItem();
    item->text = perspectivePentagonPresetName(i);
    item->rightText = CHECKMARK(settings && settings->preset == i);
    item->value = settings ? &settings->preset : nullptr;
    item->nextValue = i;
    menu->addChild(item);
  }

  menu->addChild(new MenuSeparator);
  menu->addChild(construct<MenuLabel>(
      &MenuLabel::text,
      "color variation: " + perspectivePentagonColorVariationName(
                                settings ? settings->colorVariation : 1)));
  for (int i = 0; i < 5; i++) {
    PerspectivePentagonMenuItem* item = new PerspectivePentagonMenuItem();
    item->text = perspectivePentagonColorVariationName(i);
    item->rightText =
        CHECKMARK(settings && clampPerspectivePentagonColorVariation(
                                  settings->colorVariation) == i);
    item->value = settings ? &settings->colorVariation : nullptr;
    item->nextValue = i;
    menu->addChild(item);
  }
}

inline void addPerspectivePentagonMenu(Menu* menu,
                                       PerspectivePentagonSettings* settings) {
  menu->addChild(new MenuSeparator);
  addPerspectivePentagonMenuItems(menu, settings);
}

struct PerspectivePentagonWidget : Widget {
  PerspectivePentagonDisplay* display = nullptr;
  Widget* content = nullptr;
  PerspectivePentagonSettings* settings = nullptr;
  bool drawSidesEnabled = true;
  bool drawFaceEnabled = true;
  bool hoverHighlightEnabled = false;

  PerspectivePentagonWidget(PerspectivePentagonSettings* settings, int seed) {
    this->settings = settings;
    display = new PerspectivePentagonDisplay();
    display->settings = settings;
    display->seed = seed;
  }

  ~PerspectivePentagonWidget() override {
    delete display;
    display = nullptr;
  }

  void setContent(Widget* newContent) {
    if (content) {
      removeChild(content);
      delete content;
    }
    content = newContent;
    if (content) addChild(content);
    layout();
  }

  void setBaseShades(int faceShade, int sideShade, int sideStep = 0x0c) {
    if (!display) return;
    display->baseFaceShade = faceShade;
    display->baseSideShade = sideShade;
    display->sideShadeStep = sideStep;
  }

  void setDrawParts(bool drawSides, bool drawFace) {
    drawSidesEnabled = drawSides;
    drawFaceEnabled = drawFace;
  }

  void setHoverHighlightEnabled(bool enabled) {
    hoverHighlightEnabled = enabled;
  }

  bool isHoveredWithin() const {
    Widget* hovered = APP->event->hoveredWidget;
    while (hovered) {
      if (hovered == this) return true;
      hovered = hovered->parent;
    }
    return false;
  }

  void layout() {
    if (display) {
      display->box = Rect(Vec(0.f, 0.f), box.size);
      display->ownerPos = box.pos;
      if (parent) display->canvasSize = parent->box.size;
    }
    if (content && display) content->box = display->contentRect();
  }

  void onResize(const ResizeEvent& e) override {
    layout();
    Widget::onResize(e);
  }

  void step() override {
    layout();
    Widget::step();
  }

  void draw(const DrawArgs& args) override {
    if (display && drawSidesEnabled) display->drawSides(args);
    if (display && drawFaceEnabled) display->drawFace(args);
    if (display && hoverHighlightEnabled && isHoveredWithin())
      display->drawHighlight(args);
    Widget::draw(args);
  }

  Menu* makeMenu() {
    Menu* menu = createMenu();
    addPerspectivePentagonMenu(menu, settings);
    return menu;
  }

  void onButton(const event::Button& e) override {
    if (e.button == GLFW_MOUSE_BUTTON_RIGHT && e.action == GLFW_PRESS &&
        (e.mods & RACK_MOD_MASK) == 0) {
      makeMenu();
      e.consume(this);
      return;
    }
    Widget::onButton(e);
  }
};

struct PerspectiveControlCycleOverlay : Widget {
  ComputerscareComplexBase* module = nullptr;
  int modeParamId = -1;

  void cycleControlMode() {
    if (!module || modeParamId < 0 || modeParamId >= (int)module->params.size())
      return;
    int mode = std::round(module->params[modeParamId].getValue());
    module->params[modeParamId].setValue((mode + 1) % 3);
  }

  void onButton(const event::Button& e) override {
    if (e.button == GLFW_MOUSE_BUTTON_LEFT && e.action == GLFW_PRESS &&
        (e.mods & RACK_MOD_MASK) == 0) {
      cycleControlMode();
      e.consume(this);
      return;
    }
    Widget::onButton(e);
  }
};

struct PerspectiveLabeledSwitchableComplexControl : PerspectivePentagonWidget {
  LabeledSwitchableComplexControl* complexControl = nullptr;
  PerspectiveControlCycleOverlay* cycleOverlay = nullptr;
  ComputerscareComplexBase* module = nullptr;
  int modeParamId = -1;

  PerspectiveLabeledSwitchableComplexControl(
      PerspectivePentagonSettings* settings, int seed,
      ComputerscareComplexBase* module, int paramIndex, int polarParamIndex,
      int modeParamId, ComplexXYMaxMode arrowMaxMode, float arrowMaxVoltage,
      const std::string& label, Vec controlSize, Vec labelPos, Vec labelSize,
      int laneIndex = -1, bool showDisabledOverlay = false,
      const std::string& controlName = "",
      int labelTextAlign = NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE,
      Vec controlPos = Vec(0.f, 0.f))
      : PerspectivePentagonWidget(settings, seed) {
    this->module = module;
    this->modeParamId = modeParamId;
    complexControl = new LabeledSwitchableComplexControl(
        module, paramIndex, polarParamIndex, modeParamId, arrowMaxMode,
        arrowMaxVoltage, label, controlSize, labelPos, labelSize, laneIndex,
        showDisabledOverlay, controlName, labelTextAlign, controlPos);
    setContent(complexControl);
    cycleOverlay = new PerspectiveControlCycleOverlay();
    cycleOverlay->module = module;
    cycleOverlay->modeParamId = modeParamId;
    addChild(cycleOverlay);
  }

  void setArrowDrawingScale(float scale) {
    if (complexControl) complexControl->setArrowDrawingScale(scale);
  }

  void setArrowYOffset(float offset) {
    if (complexControl) complexControl->setArrowYOffset(offset);
  }

  void step() override {
    if (cycleOverlay) cycleOverlay->box = Rect(Vec(0.f, 0.f), box.size);
    PerspectivePentagonWidget::step();
  }

  void cycleControlMode() {
    if (!module || modeParamId < 0 || modeParamId >= (int)module->params.size())
      return;
    int mode = std::round(module->params[modeParamId].getValue());
    module->params[modeParamId].setValue((mode + 1) % 3);
  }

  void onButton(const event::Button& e) override {
    if (e.button == GLFW_MOUSE_BUTTON_LEFT && e.action == GLFW_PRESS &&
        (e.mods & RACK_MOD_MASK) == 0) {
      cycleControlMode();
      e.consume(this);
      return;
    }
    PerspectivePentagonWidget::onButton(e);
  }
};

}  // namespace cpx
