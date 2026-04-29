#pragma once

#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>

#include "math/ComplexMath.hpp"

namespace cpx {
namespace complex_text {

struct FixedComplexTextStyle {
  NVGcolor numberColor = nvgRGB(0xC8, 0xEA, 0xE2);
  NVGcolor operatorColor = nvgRGB(0x88, 0x92, 0x8F);
  NVGcolor accentColor = COLOR_COMPUTERSCARE_LIGHT_GREEN;
  int textAlign = NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE;
  float fontSize = 12.f;
  float symbolFontSize = 13.f;
  float angleYOffset = -1.f;
  float degreeYOffset = -3.f;
  int decimals = 2;
};

inline std::string fixedSigned(float value, int integerDigits,
                               int decimals = 2) {
  std::ostringstream ss;
  int width = integerDigits + 1 + decimals + 1;
  ss << std::fixed << std::setprecision(decimals) << std::setw(width) << value;
  return ss.str();
}

inline std::string fixedUnsigned(float value, int integerDigits,
                                 int decimals = 2) {
  std::ostringstream ss;
  int width = integerDigits + 1 + decimals;
  ss << std::fixed << std::setprecision(decimals) << std::setw(width)
     << std::fabs(value);
  return ss.str();
}

inline float charWidth(NVGcontext* vg, float y = 0.f) {
  float bounds[4];
  nvgTextBounds(vg, 0.f, y, "0", nullptr, bounds);
  return bounds[2] - bounds[0];
}

inline float drawCells(NVGcontext* vg, const std::string& s, float x, float y,
                       float charW, NVGcolor color, int textAlign) {
  nvgTextAlign(vg, textAlign);
  nvgFillColor(vg, color);
  for (int i = 0; i < (int)s.size(); i++) {
    if (s[i] == ' ') continue;
    char buf[2] = {s[i], '\0'};
    nvgText(vg, x + charW * i, y, buf, nullptr);
  }
  return x + charW * s.size();
}

inline float drawOperator(NVGcontext* vg, const std::string& s, float x,
                          float y, float charW, NVGcolor color, int textAlign) {
  nvgTextAlign(vg, textAlign);
  nvgFillColor(vg, color);
  for (int i = 0; i < (int)s.size(); i++) {
    char buf[2] = {s[i], '\0'};
    nvgText(vg, x + charW * i, y, buf, nullptr);
  }
  return x + charW * s.size();
}

inline void drawAngleGlyph(NVGcontext* vg, float x, float y, float charW,
                           NVGcolor color) {
  const float left = x + charW * 0.12f;
  const float right = x + charW * 0.95f;
  const float baseY = y + 4.f;
  const float tipY = y - 2.f;
  nvgBeginPath(vg);
  nvgMoveTo(vg, right, baseY);
  nvgLineTo(vg, left, baseY);
  nvgLineTo(vg, right, tipY);
  nvgStrokeColor(vg, color);
  nvgStrokeWidth(vg, 0.65f);
  nvgLineJoin(vg, NVG_MITER);
  nvgLineCap(vg, NVG_BUTT);
  nvgStroke(vg);
}

inline void drawDegreeGlyph(NVGcontext* vg, float x, float y, NVGcolor color) {
  nvgBeginPath(vg);
  nvgCircle(vg, x, y, 1.35f);
  nvgStrokeColor(vg, color);
  nvgStrokeWidth(vg, 0.65f);
  nvgStroke(vg);
}

inline float drawRect(NVGcontext* vg, int fontHandle, int symbolFontHandle,
                      float x, float y, float real, float imag, float charW,
                      FixedComplexTextStyle style = {}) {
  nvgFontFaceId(vg, fontHandle);
  nvgFontSize(vg, style.fontSize);

  float cursorX = x;
  cursorX = drawCells(vg, fixedSigned(real, 2, style.decimals), cursorX, y,
                      charW, style.numberColor, style.textAlign);
  cursorX = drawOperator(vg, imag < 0.f ? " -" : " +", cursorX, y, charW,
                         style.operatorColor, style.textAlign);
  cursorX = drawCells(vg, fixedUnsigned(imag, 2, style.decimals), cursorX, y,
                      charW, style.numberColor, style.textAlign);

  nvgFontFaceId(vg, symbolFontHandle);
  nvgFontSize(vg, style.symbolFontSize);
  nvgTextAlign(vg, style.textAlign);
  nvgFillColor(vg, style.accentColor);
  nvgText(vg, cursorX + charW * 0.2f, y, "i", nullptr);
  nvgFontFaceId(vg, fontHandle);
  return cursorX + charW * 0.9f;
}

inline float drawPolar(NVGcontext* vg, int fontHandle, float x, float y,
                       float radius, float thetaRadians, float charW,
                       FixedComplexTextStyle style = {}) {
  nvgFontFaceId(vg, fontHandle);
  nvgFontSize(vg, style.fontSize);

  float cursorX = x;
  const float thetaDegrees = thetaRadians * 180.f / complex_math::pi;
  cursorX = drawCells(vg, fixedUnsigned(radius, 2, style.decimals), cursorX, y,
                      charW, style.numberColor, style.textAlign);
  cursorX += charW * 0.45f;
  drawAngleGlyph(vg, cursorX, y + style.angleYOffset, charW, style.accentColor);
  cursorX += charW * 1.35f;
  cursorX = drawCells(vg, fixedSigned(thetaDegrees, 3, style.decimals), cursorX,
                      y, charW, style.numberColor, style.textAlign);
  drawDegreeGlyph(vg, cursorX + charW * 0.4f, y + style.degreeYOffset,
                  style.numberColor);
  return cursorX + charW * 0.75f;
}

}  // namespace complex_text
}  // namespace cpx
