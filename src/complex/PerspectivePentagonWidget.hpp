#pragma once

#include "SwitchableComplexControl.hpp"

namespace cpx {

struct PerspectivePentagonSettings {};

struct PerspectivePentagonGeometry {
  Vec face[5];
  Vec extruded[5];
  Vec vanishingPoint;
  float depletion = 0.72f;
  int pointCount = 5;
};

struct PerspectivePentagonDisplay : Widget {
  PerspectivePentagonSettings* settings = nullptr;
  int seed = 0;
  Vec ownerPos;
  Vec canvasSize = Vec(120.f, 380.f);
  NVGcolor faceColor = nvgRGB(0xa9, 0xad, 0xaa);
  NVGcolor sideColor = nvgRGB(0x69, 0x6e, 0x6b);
  int baseFaceShade = 0xb2;
  int baseSideShade = 0x8a;
  int sideShadeStep = 0x0c;

  float contentScale() const { return 0.94f; }

  Vec faceInset() const { return Vec(2.7f, 2.7f); }

  float distortion(int point, int axis) const {
    unsigned int x = 2166136261u;
    x = (x ^ (unsigned int)(seed + 31 * point + 131 * axis)) * 16777619u;
    x ^= x >> 13;
    x *= 1274126177u;
    float unit = (float)(x & 0xffffu) / 65535.f;
    return (unit - 0.5f) * 2.f;
  }

  int shadeOffset(int salt, float multiplier = 1.f) const {
    constexpr float colorVariationRange = 56.f;
    int range = std::round(colorVariationRange * multiplier);
    if (range <= 0) return 0;
    return std::round(distortion(seed + salt, 3) * range);
  }

  PerspectivePentagonGeometry geometry() const {
    PerspectivePentagonGeometry g;
    Vec vp = Vec(-canvasSize.x * 0.45f, canvasSize.y * 1.22f);
    float depletion = 0.72f;
    g.vanishingPoint = vp.minus(ownerPos);
    g.depletion = depletion;

    Vec inset = faceInset();
    float maxJitter = std::min(box.size.x, box.size.y) * 0.04f;
    g.pointCount = 4;
    Vec points[4] = {Vec(inset.x, inset.y), Vec(box.size.x - inset.x, inset.y),
                     Vec(box.size.x - inset.x, box.size.y - inset.y),
                     Vec(inset.x, box.size.y - inset.y)};
    for (int i = 0; i < 4; i++) {
      Vec p = points[i];
      p.x += distortion(i, 0) * maxJitter;
      p.y += distortion(i, 1) * maxJitter;
      g.face[i] = p;
      g.extruded[i] = p.plus(g.vanishingPoint.minus(p).mult(depletion));
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

  void drawFaceHighlight(const DrawArgs& args) {
    PerspectivePentagonGeometry g = geometry();
    NVGcolor highlightColor = nvgRGBA(238, 209, 61, 58);
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

struct PerspectivePentagonWidget : Widget {
  PerspectivePentagonDisplay* display = nullptr;
  Widget* content = nullptr;
  PerspectivePentagonSettings* settings = nullptr;
  bool drawSidesEnabled = true;
  bool drawFaceEnabled = true;
  bool hoverHighlightEnabled = false;
  bool contentFillsBox = false;
  bool layoutInitialized = false;
  Vec lastLayoutPos;
  Vec lastLayoutSize;
  Vec lastLayoutParentSize;

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

  void setContentFillsBox(bool enabled) {
    contentFillsBox = enabled;
    layout();
  }

  bool containsFacePoint(Vec p) const {
    if (!display) return false;
    PerspectivePentagonGeometry g = display->geometry();
    bool inside = false;
    for (int i = 0, j = g.pointCount - 1; i < g.pointCount; j = i++) {
      Vec pi = g.face[i];
      Vec pj = g.face[j];
      bool crosses =
          ((pi.y > p.y) != (pj.y > p.y)) &&
          (p.x < (pj.x - pi.x) * (p.y - pi.y) / (pj.y - pi.y) + pi.x);
      if (crosses) inside = !inside;
    }
    return inside;
  }

  bool isHoveredWithin() const {
    Widget* hovered = APP->event->hoveredWidget;
    while (hovered) {
      if (hovered == this) return true;
      hovered = hovered->parent;
    }
    return false;
  }

  bool isMouseOverFace() {
    if (!APP || !APP->scene) return false;
    Vec mouse = APP->scene->getMousePos();
    Vec pos = getAbsoluteOffset(Vec());
    float zoom = getAbsoluteZoom();
    if (zoom <= 0.f) return false;
    return containsFacePoint(mouse.minus(pos).div(zoom));
  }

  void onHover(const event::Hover& e) override {
    if (!containsFacePoint(e.pos)) return;
    Widget::onHover(e);
    if (!e.isConsumed()) e.consume(this);
  }

  void layout() {
    if (display) {
      display->box = Rect(Vec(0.f, 0.f), box.size);
      display->ownerPos = box.pos;
      if (parent) display->canvasSize = parent->box.size;
    }
    if (content && display) {
      content->box = contentFillsBox ? Rect(Vec(0.f, 0.f), box.size)
                                     : display->contentRect();
    }
    layoutInitialized = true;
    lastLayoutPos = box.pos;
    lastLayoutSize = box.size;
    lastLayoutParentSize = parent ? parent->box.size : Vec(0.f, 0.f);
  }

  bool needsLayout() const {
    if (!layoutInitialized) return true;
    Vec parentSize = parent ? parent->box.size : Vec(0.f, 0.f);
    return box.pos.x != lastLayoutPos.x || box.pos.y != lastLayoutPos.y ||
           box.size.x != lastLayoutSize.x || box.size.y != lastLayoutSize.y ||
           parentSize.x != lastLayoutParentSize.x ||
           parentSize.y != lastLayoutParentSize.y;
  }

  void onResize(const ResizeEvent& e) override {
    layout();
    Widget::onResize(e);
  }

  void step() override {
    if (needsLayout()) layout();
    Widget::step();
  }

  void draw(const DrawArgs& args) override {
    if (display && drawSidesEnabled) display->drawSides(args);
    if (display && drawFaceEnabled) display->drawFace(args);
    if (display && drawFaceEnabled && hoverHighlightEnabled &&
        isHoveredWithin() && isMouseOverFace())
      display->drawFaceHighlight(args);
    Widget::draw(args);
  }

  void onButton(const event::Button& e) override {
    if (!containsFacePoint(e.pos)) return;
    Widget::onButton(e);
  }
};

struct PerspectiveLabeledSwitchableComplexControl : PerspectivePentagonWidget {
  LabeledSwitchableComplexControl* complexControl = nullptr;
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
  }

  void setArrowDrawingScale(float scale) {
    if (complexControl) complexControl->setArrowDrawingScale(scale);
  }

  void setArrowYOffset(float offset) {
    if (complexControl) complexControl->setArrowYOffset(offset);
  }

  void cycleControlMode() {
    if (!module || modeParamId < 0 || modeParamId >= (int)module->params.size())
      return;
    int mode = std::round(module->params[modeParamId].getValue());
    module->params[modeParamId].setValue((mode + 1) % 3);
  }

  void createModeMenu() {
    if (!module || modeParamId < 0 || modeParamId >= (int)module->params.size())
      return;
    Menu* menu = createMenu();
    menu->addChild(construct<MenuLabel>(&MenuLabel::text, "View"));

    static const std::vector<std::string> labels = {"arrow", "xy", "rtheta"};
    int currentMode = std::round(module->params[modeParamId].getValue());
    for (int i = 0; i < (int)labels.size(); ++i) {
      ParamSettingItem* item =
          new ParamSettingItem(i, &module->params[modeParamId]);
      item->text = labels[i];
      item->rightText = CHECKMARK(i == currentMode);
      menu->addChild(item);
    }
  }

  bool isModeSwitchPoint(Vec pos) const {
    return complexControl && complexControl->modeSwitch &&
           complexControl->modeSwitch->box.contains(pos);
  }

  void onButton(const event::Button& e) override {
    if (e.button == GLFW_MOUSE_BUTTON_LEFT && e.action == GLFW_PRESS &&
        (e.mods & RACK_MOD_MASK) == 0 && e.pos.y <= 12.f) {
      cycleControlMode();
      e.consume(this);
      return;
    }
    if (e.button == GLFW_MOUSE_BUTTON_RIGHT && e.action == GLFW_PRESS &&
        isModeSwitchPoint(e.pos)) {
      createModeMenu();
      e.consume(this);
      return;
    }
    PerspectivePentagonWidget::onButton(e);
  }
};

}  // namespace cpx
