#pragma once

#include "Computerscare.hpp"

struct ColorableSmallKnob : Knob {
  engine::Param* colorModeParam = nullptr;
  engine::Param* colorStrengthParam = nullptr;
  engine::Param* colorHueParam = nullptr;
  engine::Param* fadeParam = nullptr;
  int* numInputChannels = nullptr;
  int* polyChannels = nullptr;
  int inputChannel = 0;
  int outputChannel = 0;
  bool disabled = false;
  bool useDisplayState = false;
  float displayValue = 1.f;
  float displayColorMode = 1.f;
  float displayColorStrength = 1.f;
  float displayColorHue = 0.f;
  float displayFade = 1.f;
  int displayNumInputChannels = 16;
  int displayPolyChannels = 16;

  ColorableSmallKnob() {
    box.size = Vec(18.f, 18.f);
    minAngle = -0.83f * M_PI;
    maxAngle = 0.83f * M_PI;
  }

  float clampf(float v, float lo, float hi) {
    return std::max(lo, std::min(hi, v));
  }

  NVGcolor mixColor(NVGcolor a, NVGcolor b, float amount) {
    amount = clampf(amount, 0.f, 1.f);
    return nvgRGBAf(a.r + (b.r - a.r) * amount, a.g + (b.g - a.g) * amount,
                    a.b + (b.b - a.b) * amount, a.a + (b.a - a.a) * amount);
  }

  NVGcolor sinColor(float t, std::vector<float> omega = {1.f, 2.f, 3.f},
                    std::vector<float> phi = {0.f, 5.f, 9.f}) {
    return nvgRGB(127 * (1 + sin(t * omega[0] + phi[0])),
                  127 * (1 + sin(t * omega[1] + phi[1])),
                  127 * (1 + sin(t * omega[2] + phi[2])));
  }

  NVGcolor hueColor(float hue) {
    hue = hue - std::floor(hue);
    return nvgHSLA(hue, 1.f, 0.5f, 0xff);
  }

  NVGcolor rotateColor(NVGcolor color, float hue, float amount) {
    return mixColor(color, hueColor(hue), amount);
  }

  NVGcolor boostColor(NVGcolor color, float hue, float amount) {
    return mixColor(color, hueColor(hue), amount);
  }

  float knobAngle() {
    if (useDisplayState) {
      return math::rescale(displayValue, -2.f, 2.f, minAngle, maxAngle);
    }

    ParamQuantity* pq = getParamQuantity();
    if (!pq) {
      return 0.f;
    }
    float value = pq->getValue();
    if (!pq->isBounded()) {
      return std::fmod(value * (2.f * (float)M_PI), 2.f * (float)M_PI);
    }
    if (pq->getRange() == 0.f) {
      return (minAngle + maxAngle) * 0.5f;
    }
    return math::rescale(value, pq->getMinValue(), pq->getMaxValue(), minAngle,
                         maxAngle);
  }

  bool colorEnabled() {
    return useDisplayState
               ? displayColorMode >= 0.5f
               : colorModeParam && colorModeParam->getValue() >= 0.5f;
  }

  float colorStrength() {
    return useDisplayState
               ? displayColorStrength
               : (colorStrengthParam ? colorStrengthParam->getValue() : 1.f);
  }

  float colorHue() {
    return useDisplayState ? displayColorHue
                           : (colorHueParam ? colorHueParam->getValue() : 0.f);
  }

  float knobFade() {
    return useDisplayState ? displayFade
                           : (fadeParam ? fadeParam->getValue() : 1.f);
  }

  NVGcolor knobColor(float value) {
    NVGcolor base = COLOR_COMPUTERSCARE_GREEN;
    if (!colorEnabled()) {
      return base;
    }

    float strength = clampf(colorStrength(), 0.f, 2.f);
    float colorAmount = clampf(strength, 0.f, 1.f);
    float boostAmount = clampf(strength - 1.f, 0.f, 1.f);
    float hue = colorHue() / 9.f;
    float positiveHue = 0.18f + hue;
    float negativeHue = 0.00f - hue * 0.73f;
    float negativePinkHue = 0.94f - hue * 0.55f;
    float rotateAmount = hue == 0.f ? 0.f : 0.85f;
    NVGcolor neutral = rotateColor(base, 0.42f + hue * 0.61f, rotateAmount);
    NVGcolor target = neutral;

    if (value <= -1.f) {
      NVGcolor hotRed = nvgRGB(0xff, 0x3a, 0x24);
      hotRed = rotateColor(hotRed, negativeHue, rotateAmount);
      NVGcolor pink = rotateColor(COLOR_COMPUTERSCARE_PINK, negativePinkHue,
                                  rotateAmount);
      target = mixColor(hotRed, pink,
                        clampf(-value - 1.f, 0.f, 1.f));
    } else if (value < 0.f) {
      NVGcolor hotRed =
          rotateColor(nvgRGB(0xff, 0x3a, 0x24), negativeHue, rotateAmount);
      target = mixColor(neutral, hotRed,
                        clampf(-value, 0.f, 1.f));
    } else if (value > 1.f) {
      NVGcolor neon =
          rotateColor(COLOR_COMPUTERSCARE_YELLOW, positiveHue, rotateAmount);
      target = mixColor(neutral, neon,
                        clampf(value - 1.f, 0.f, 1.f));
    }

    NVGcolor color = mixColor(base, target, colorAmount);
    if (boostAmount > 0.f && value != 0.f) {
      float boostHue = value < 0.f ? negativeHue : positiveHue;
      color = boostColor(color, boostHue, boostAmount * 0.45f);
    }
    return color;
  }

  float knobAlpha(float value) {
    if (!colorEnabled()) {
      return 1.f;
    }
    float fade = clampf(knobFade(), 0.f, 2.f);
    float absValue = std::fabs(value);
    float normalized = clampf(absValue, 0.f, 1.f);
    float normalFadeAlpha = 0.1f + 0.9f * normalized;
    if (fade <= 1.f) {
      return 1.f + (normalFadeAlpha - 1.f) * fade;
    }
    float extremeNormalized = clampf(absValue / 2.f, 0.f, 1.f);
    float extremeFadeAlpha =
        0.02f + 0.98f * std::pow(extremeNormalized, 2.2f);
    return normalFadeAlpha +
           (extremeFadeAlpha - normalFadeAlpha) * (fade - 1.f);
  }

  void drawBodyPath(NVGcontext* vg) {
    nvgBeginPath(vg);
    nvgMoveTo(vg, 17.306156f, 8.928799f);
    nvgBezierTo(vg, 17.853300f, 15.421582f, 13.631479f, 18.224601f,
                8.787282f, 17.778464f);
    nvgBezierTo(vg, 4.332828f, 17.368221f, -0.464907f, 13.620190f,
                0.661091f, 8.970148f);
    nvgBezierTo(vg, 2.473968f, 1.483498f, 5.201003f, 0.480310f, 9.469406f,
                0.161832f);
    nvgBezierTo(vg, 9.740443f, 0.141612f, 9.963818f, 0.090232f, 10.086859f,
                0.174862f);
    nvgBezierTo(vg, 10.938155f, 0.759968f, 8.481710f, 2.412317f, 14.495388f,
                3.262220f);
    nvgBezierTo(vg, 14.495388f, 3.262220f, 17.153964f, 7.122867f,
                17.306156f, 8.928857f);
    nvgClosePath(vg);
  }

  void drawPointerPath(NVGcontext* vg) {
    nvgBeginPath(vg);
    nvgMoveTo(vg, 9.649306f, 0.217346f);
    nvgBezierTo(vg, 10.339440f, 1.480190f, 9.100341f, 3.434183f,
                9.100341f, 3.434183f);
    nvgLineTo(vg, 8.777169f, 4.295306f);
    nvgLineTo(vg, 8.407328f, 5.170551f);
    nvgLineTo(vg, 8.378368f, 6.246799f);
    nvgLineTo(vg, 8.832016f, 7.241607f);
    nvgLineTo(vg, 9.087293f, 8.610831f);
    nvgLineTo(vg, 8.257950f, 10.978162f);
    nvgBezierTo(vg, 9.834706f, 11.107129f, 9.893502f, 11.084118f,
                11.237064f, 10.280713f);
    nvgLineTo(vg, 10.582241f, 8.567480f);
    nvgLineTo(vg, 10.243278f, 7.163124f);
    nvgLineTo(vg, 10.215678f, 5.786943f);
    nvgLineTo(vg, 10.431958f, 5.191134f);
    nvgLineTo(vg, 10.016291f, 4.091004f);
    nvgLineTo(vg, 11.191095f, 0.610282f);
    nvgLineTo(vg, 10.280358f, 0.184630f);
    nvgBezierTo(vg, 9.649062f, 0.376794f, 9.640922f, 0.178330f, 9.636622f,
                0.275240f);
    nvgClosePath(vg);
  }

  void draw(const DrawArgs& args) override {
    if (useDisplayState) {
      disabled = ((displayNumInputChannels != 0 &&
                   inputChannel > displayNumInputChannels - 1) ||
                  outputChannel > displayPolyChannels - 1);
    } else if (numInputChannels && polyChannels) {
      disabled =
          ((*numInputChannels != 0 && inputChannel > *numInputChannels - 1) ||
           outputChannel > *polyChannels - 1);
    }

    float value = useDisplayState ? displayValue
                                  : (getParamQuantity()
                                         ? getParamQuantity()->getValue()
                                         : 1.f);
    NVGcolor bodyColor =
        disabled ? nvgRGBA(0x78, 0x78, 0x78, 0xff) : knobColor(value);
    NVGcolor pointerColor =
        disabled ? nvgRGBA(0x40, 0x40, 0x40, 0xff) : nvgRGB(0x24, 0x55, 0x59);
    NVGcolor strokeColor = nvgRGBA(0x00, 0x00, 0x00, 0xff);
    float opacity = knobAlpha(value);
    bodyColor.a *= opacity;
    pointerColor.a *= opacity;
    strokeColor.a *= opacity;

    nvgSave(args.vg);
    float cx = box.size.x * 0.5f;
    float cy = box.size.y * 0.5f;

    drawBodyPath(args.vg);
    nvgFillColor(args.vg, bodyColor);
    nvgFill(args.vg);
    nvgStrokeColor(args.vg, strokeColor);
    nvgStrokeWidth(args.vg, 0.794f);
    nvgStroke(args.vg);

    nvgTranslate(args.vg, cx, cy);
    nvgRotate(args.vg, knobAngle());
    nvgTranslate(args.vg, -cx, -cy);
    drawPointerPath(args.vg);
    nvgFillColor(args.vg, pointerColor);
    nvgFill(args.vg);
    nvgRestore(args.vg);
  }
};
