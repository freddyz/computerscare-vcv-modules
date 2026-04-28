#pragma once

#include "math/ComplexFormat.hpp"

using namespace rack;

namespace cpx {

constexpr int RECT_INTERLEAVED =
    static_cast<int>(cpx::complex_math::CoordinateMode::RectInterleaved);
constexpr int POLAR_INTERLEAVED =
    static_cast<int>(cpx::complex_math::CoordinateMode::PolarInterleaved);
constexpr int RECT_SEPARATED =
    static_cast<int>(cpx::complex_math::CoordinateMode::RectSeparated);
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

  bool editing = false;
  bool cancelledDrag = false;
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

  ComplexXY(ComputerscareComplexBase* mod, int indexParamA) {
    module = mod;
    paramA = indexParamA;
    // box.size = Vec(30,30);
    TransparentWidget();
  }

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
    if (e.button == GLFW_MOUSE_BUTTON_LEFT) {
      if (e.action == GLFW_PRESS) {
        e.consume(this);
        editing = true;
        cancelledDrag = false;
        clickedMousePosition = APP->scene->getMousePos();
        if (module) {
          float complexA = module->params[paramA].getValue();
          float complexB = module->params[paramA + 1].getValue();
          origParamAValue = complexA;
          origParamBValue = complexB;
          origComplexValue = Vec(complexA, -complexB);
          origComplexLength = origComplexValue.norm();

          if (origComplexLength < 0.1) {
            origComplexLength = 1;
            pixelsOrigin = clickedMousePosition;
          } else {
            pixelsOrigin = clickedMousePosition.minus(origComplexValue.mult(
                originalMagnituteRadiusPixels / origComplexLength));
          }
        }
      }
    }
  }
  void onHoverKey(const HoverKeyEvent& e) override {
    if (editing && e.action == GLFW_PRESS && e.key == GLFW_KEY_ESCAPE) {
      cancelDrag();
      e.consume(this);
    }
  }

  void onDragEnd(const event::DragEnd& e) override {
    if (!cancelledDrag) editing = false;
    cancelledDrag = false;
  }

  void step() override {
    if (editing && module) {
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

      module->params[paramA].setValue(newZ.x);
      module->params[paramA + 1].setValue(-newZ.y);
    } else {
      if (module) {
        newZ = Vec(module->params[paramA].getValue(),
                   -module->params[paramA + 1].getValue());
      } else {
        newZ = Vec(-10 + 20 * random::uniform(), -10 + 20 * random::uniform());
      }
    }
  }

  void draw(const DrawArgs& args) override {
    float fullR = box.size.x / 2;

    // circle at complex radius 1
    nvgSave(args.vg);
    // nvgResetTransform(args.vg);
    nvgTranslate(args.vg, fullR, fullR);
    nvgBeginPath(args.vg);
    nvgStrokeWidth(args.vg, 2.f);
    nvgFillColor(args.vg, faded ? fadedBackgroundColor : normalBackgroundColor);
    // nvgMoveTo(args.vg,box.size.x/2,box.size.y/2);
    nvgEllipse(args.vg, 0, 0, fullR, fullR);
    nvgClosePath(args.vg);
    nvgFill(args.vg);

    nvgBeginPath(args.vg);
    nvgStrokeWidth(args.vg, faded ? 1.3f : 2.4f);
    nvgStrokeColor(args.vg,
                   faded ? fadedArrowStrokeColor : normalArrowStrokeColor);
    nvgMoveTo(args.vg, 0, 0);

    float length = newZ.norm();
    Vec tip = newZ.normalize().mult(2 * fullR / M_PI * std::atan(length));

    drawArrowTo(args.vg, tip, box.size.x / 6.4f,
                faded ? fadedArrowFillColor : normalArrowFillColor,
                faded ? fadedArrowStrokeColor : normalArrowStrokeColor,
                faded ? 1.f : 1.5f);
    nvgRestore(args.vg);
  }
  void drawLayer(const DrawArgs& args, int layer) override {
    if (layer == 1) {
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
        Vec r1LabelPos =
            Vec(r1Pixels * labelAngleScale, -r1Pixels * labelAngleScale)
                .plus(labelOutward);
        drawDragText(args.vg, "1v", r1LabelPos, nvgRGB(120, 190, 255), 28.f);

        // circle at the zero point
        nvgBeginPath(args.vg);
        nvgStrokeWidth(args.vg, 3.f);
        nvgFillColor(args.vg, nvgRGB(249, 220, 214));
        nvgEllipse(args.vg, 0, 0, 10.f, 10.f);
        nvgClosePath(args.vg);
        nvgFill(args.vg);

        // circle at the current complex value
        nvgBeginPath(args.vg);
        nvgStrokeWidth(args.vg, 3.f);
        nvgStrokeColor(args.vg, nvgRGB(0, 100, 200));
        nvgEllipse(args.vg, 0, 0, originalMagnituteRadiusPixels,
                   originalMagnituteRadiusPixels);
        nvgClosePath(args.vg);
        nvgStroke(args.vg);

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
        drawDragText(
            args.vg, polarDragDisplayString(newZ.x, -newZ.y),
            pixelsOrigin.plus(Vec(0.f, originalMagnituteRadiusPixels + 78.f)),
            nvgRGB(245, 245, 245), 34.f, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgRestore(args.vg);
      }
    }
    Widget::drawLayer(args, layer);
  }
};

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

struct CompolySingleLabelSwitch : app::SvgSwitch {
  widget::TransformWidget* tw;
  // SvgWidget* svg;
  CompolySingleLabelSwitch(std::string svgLabelFilename = "z") {
    shadow->opacity = 0.f;
    addFrame(APP->window->loadSvg(asset::plugin(
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

  CompolyPortsWidget(math::Vec pos, ComputerscareComplexBase* cModule,
                     int firstPortID, int compolyTypeParamID, float scale = 1.0,
                     bool isOutput = true, std::string labelSvgFilename = "z")
      : CompolyInOrOutWidget(pos) {
    module = cModule;
    paramID = compolyTypeParamID;

    compolyLabel = new CompolySingleLabelSwitch(labelSvgFilename);

    compolyLabel->app::ParamWidget::module = cModule;
    compolyLabel->app::ParamWidget::paramId = compolyTypeParamID;
    compolyLabel->initParamQuantity();

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

  void draw(const DrawArgs& args) override {
    if (!module) {
      iGlyph->visible = false;
      eGlyph->visible = false;
      Widget::draw(args);
      return;
    }

    auto font = APP->window->loadFont(
        asset::plugin(pluginInstance, "res/fonts/RobotoMono-Regular.ttf"));
    if (!font) {
      Widget::draw(args);
      return;
    }

    float vx = module->params[paramX].getValue();
    float vy = module->params[paramY].getValue();

    cpx::complex_math::ComplexRenderParts parts;
    bool polar = (displayMode == DisplayMode::Polar);
    if (polar) {
      float r = vx;
      float theta = vy;
      if (sourceMode == SourceMode::Rect) {
        r = std::hypot(vx, vy);
        theta = std::atan2(vy, vx);
      }
      parts = cpx::complex_math::polarParts(r, theta, angleUnit, polarStyle,
                                            decimals);
    } else {
      float x = vx;
      float y = vy;
      if (sourceMode == SourceMode::Polar) {
        x = vx * std::cos(vy);
        y = vx * std::sin(vy);
      }
      parts = cpx::complex_math::rectParts(x, y, decimals);
    }

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
      float cursorX = fieldStartX;
      float realValue =
          sourceMode == SourceMode::Polar ? vx * std::cos(vy) : vx;
      float imagValue =
          sourceMode == SourceMode::Polar ? vx * std::sin(vy) : vy;
      cursorX = drawFixedCells(fixedNumberString(realValue, 2), cursorX, charW,
                               normalColor);
      cursorX = fixedOperator(imagValue < 0.f ? " - " : " + ", cursorX, charW,
                              dimColor);
      cursorX = drawFixedCells(fixedNumberString(std::fabs(imagValue), 2),
                               cursorX, charW, normalColor);

      iGlyph->box.pos = Vec(cursorX, midY - glyphH);
      iGlyph->box.size = Vec(iW, glyphH);
      iGlyph->visible = true;
    } else if (polarStyle ==
               cpx::complex_math::PolarDisplayStyle::Engineering) {
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
      cursorX = fixedOperator(" ∠ ", cursorX, charW, accentColor);
      cursorX = drawFixedCells(fixedNumberString(displayAngle, 3, true),
                               cursorX, charW, normalColor);
      drawCellText(unitSuffix, cursorX, normalColor);
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
