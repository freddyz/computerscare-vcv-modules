#pragma once

#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>

#include "../Computerscare.hpp"
#include "../complex/ComplexText.hpp"
#include "NudiebugPlotDisplay.hpp"
#include "NudiebugState.hpp"

namespace nudiebug {

inline std::string formatVoltage(float voltage) {
  std::ostringstream stream;
  stream << std::showpos << std::fixed << std::setprecision(4) << voltage;
  return stream.str();
}

struct TextDisplay : TransparentWidget {
  Snapshot* snapshot = nullptr;
  DisplayOptions* options = nullptr;
  std::string fontPath = "res/fonts/RobotoMono-Regular.ttf";
  std::string symbolFontPath = "res/fonts/LibertinusSerif-Italic.ttf";
  PlotDisplay plotDisplay;

  void draw(const DrawArgs& args) override {
    nvgBeginPath(args.vg);
    nvgRect(args.vg, 0.f, 0.f, box.size.x, box.size.y);
    nvgFillColor(args.vg, nvgRGB(0x12, 0x14, 0x16));
    nvgFill(args.vg);

    if (!snapshot) return;

    const int barsMode = options ? options->visualizationMode : BARS_UNIPOLAR;
    const int textMode = options ? options->textMode : TEXT_POLY;
    const int plotMode = options ? options->plotMode : PLOT_OFF;
    const bool drawViz = barsMode != BARS_OFF;
    const bool drawText = textMode != TEXT_OFF;
    const bool drawPlot = options && plotMode != PLOT_OFF;
    const bool drawLabels = drawViz || drawText;
    const int rows = snapshot->maxChannels();
    const float top = 2.f;
    const float bottom = 2.f;
    const float rowHeight = (box.size.y - top - bottom) / 16.f;
    const float sideLabelMargin = 3.f;
    const float valueLabelGap = 11.f;
    const float leftValueX = sideLabelMargin + valueLabelGap;
    const float rightValueRight = box.size.x - sideLabelMargin - valueLabelGap;
    const float valueWidth = std::max(42.f, (box.size.x - 48.f) * 0.5f);
    const float rightValueX = rightValueRight - valueWidth;

    if (drawPlot) {
      plotDisplay.render(args.vg, *snapshot, *options,
                         std::max(1, static_cast<int>(std::ceil(box.size.x))),
                         std::max(1, static_cast<int>(std::ceil(box.size.y))));
      plotDisplay.draw(args.vg, box.size.x, box.size.y);
    }

    if (drawViz) {
      for (int c = 0; c < rows && c < kMaxChannels; c++) {
        float textY = top + c * rowHeight + rowHeight * 0.55f;
        if (isLeftChannelActive(c)) {
          drawBar(args.vg, leftValueX, textY, valueWidth,
                  snapshot->leftVoltages[c], false, barsMode);
        }
        if (isRightChannelActive(c)) {
          drawBar(args.vg, rightValueX, textY, valueWidth,
                  snapshot->rightVoltages[c], true, barsMode);
        }
      }
    }

    if (!drawLabels) return;

    std::shared_ptr<Font> font =
        APP->window->loadFont(asset::plugin(pluginInstance, fontPath));
    if (!font) return;
    std::shared_ptr<Font> symbolFont =
        APP->window->loadFont(asset::plugin(pluginInstance, symbolFontPath));

    nvgFontFaceId(args.vg, font->handle);
    nvgFontSize(args.vg, 12.f);
    nvgTextLetterSpacing(args.vg, 0.f);
    nvgTextLineHeight(args.vg, 1.f);

    const float charW = cpx::complex_text::charWidth(args.vg);

    for (int c = 0; c < rows && c < kMaxChannels; c++) {
      float y = top + c * rowHeight + rowHeight * 0.55f;

      if (textMode == TEXT_POLY || !drawText) {
        drawChannelLabel(args.vg, sideLabelMargin, y, c,
                         isLeftChannelActive(c));
        drawChannelLabel(args.vg, box.size.x - sideLabelMargin, y, c,
                         isRightChannelActive(c),
                         NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
      } else {
        drawChannelLabel(args.vg, sideLabelMargin, y, c,
                         isCompolyChannelActive(c));
      }

      if (!drawText) continue;

      if (textMode == TEXT_POLY) {
        drawPolyText(args.vg, font->handle, leftValueX, rightValueRight, y, c);
      } else if (textMode == TEXT_COMPOLY_RECT) {
        drawCompolyRectText(args.vg, font->handle,
                            symbolFont ? symbolFont->handle : font->handle,
                            leftValueX, y, c, charW);
      } else if (textMode == TEXT_COMPOLY_POLAR) {
        drawCompolyPolarText(args.vg, font->handle, leftValueX, y, c, charW);
      }
    }
  }

  bool isLeftChannelActive(int channel) const {
    return snapshot && channel < snapshot->leftChannels;
  }

  bool isRightChannelActive(int channel) const {
    return snapshot && channel < snapshot->rightChannels;
  }

  bool isCompolyChannelActive(int channel) const {
    return snapshot && channel < snapshot->compolyChannels;
  }

  void drawBar(NVGcontext* vg, float x, float centerY, float width,
               float voltage, bool rightAligned, int mode) {
    const float height = 14.f;
    const float y = centerY - height * 0.5f;
    const float clamped = clamp(std::fabs(voltage) / 10.f, 0.f, 1.f);
    const float barWidth = std::max(2.f, width * clamped);
    const NVGcolor fill = voltage >= 0.f ? nvgRGBA(0x24, 0xC9, 0xA6, 0x80)
                                         : nvgRGBA(0xC4, 0x34, 0x21, 0x80);

    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, y, width, height, 2.f);
    nvgFillColor(vg, nvgRGBA(0x23, 0x25, 0x27, 0xC0));
    nvgFill(vg);

    float bx = rightAligned ? x + width - barWidth : x;
    float bw = barWidth;
    if (mode == BARS_BIPOLAR) {
      const float centerX = x + width * 0.5f;
      bw = std::max(2.f, width * 0.5f * clamped);
      bx = voltage >= 0.f ? centerX : centerX - bw;
    }

    nvgBeginPath(vg);
    nvgRoundedRect(vg, bx, y, bw, height, 2.f);
    nvgFillColor(vg, fill);
    nvgFill(vg);
  }

  void drawChannelLabel(NVGcontext* vg, float x, float y, int channel,
                        bool active,
                        int align = NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE) {
    nvgFillColor(vg, active ? nvgRGB(0xF3, 0xF1, 0xDE)
                            : nvgRGBA(0xF3, 0xF1, 0xDE, 0x55));
    nvgTextAlign(vg, align);
    nvgText(vg, x, y, string::f("%02d", channel + 1).c_str(), nullptr);
  }

  void drawPolyText(NVGcontext* vg, int fontHandle, float leftValueX,
                    float rightValueRight, float y, int channel) {
    nvgFontFaceId(vg, fontHandle);
    nvgFontSize(vg, 12.f);

    nvgFillColor(vg, nvgRGB(0xC8, 0xEA, 0xE2));
    if (isLeftChannelActive(channel)) {
      nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
      std::string left = formatVoltage(snapshot->leftVoltages[channel]);
      nvgText(vg, leftValueX + 5.f, y, left.c_str(), nullptr);
    }

    if (isRightChannelActive(channel)) {
      nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
      std::string right = formatVoltage(snapshot->rightVoltages[channel]);
      nvgText(vg, rightValueRight - 5.f, y, right.c_str(), nullptr);
    }
  }

  void drawCompolyRectText(NVGcontext* vg, int fontHandle, int symbolFontHandle,
                           float valueX, float y, int channel, float charW) {
    if (!isCompolyChannelActive(channel)) return;

    nvgFontFaceId(vg, fontHandle);
    nvgFontSize(vg, 12.f);

    cpx::complex_text::FixedComplexTextStyle style;
    style.textAlign = NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE;
    cpx::complex_text::drawRect(vg, fontHandle, symbolFontHandle, valueX + 5.f,
                                y, snapshot->rectX[channel],
                                snapshot->rectY[channel], charW, style);
  }

  void drawCompolyPolarText(NVGcontext* vg, int fontHandle, float valueX,
                            float y, int channel, float charW) {
    if (!isCompolyChannelActive(channel)) return;

    nvgFontFaceId(vg, fontHandle);
    nvgFontSize(vg, 12.f);

    cpx::complex_text::FixedComplexTextStyle style;
    style.textAlign = NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE;
    cpx::complex_text::drawPolar(vg, fontHandle, valueX + 5.f, y,
                                 snapshot->polarR[channel],
                                 snapshot->polarTheta[channel], charW, style);
  }
};

}  // namespace nudiebug
