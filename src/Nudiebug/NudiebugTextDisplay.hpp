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

    const int displayType = options ? options->displayType : DISPLAY_TYPE_POLY;
    const bool compolyDisplay = displayType == DISPLAY_TYPE_COMPOLY;
    const int configuredBarsMode =
        options ? options->visualizationMode : BARS_UNIPOLAR;
    const int barsMode = compolyDisplay && configuredBarsMode == BARS_BIPOLAR
                             ? BARS_UNI_MID
                             : configuredBarsMode;
    const int textMode = options ? options->textMode : TEXT_POLY;
    const int compolyRepresentation =
        options ? options->compolyRepresentation : COMPOLY_REP_RECT;
    const int plotMode = options ? options->plotMode : PLOT_OFF;
    const bool drawViz = barsMode != BARS_OFF;
    const bool drawText = textMode != TEXT_OFF;
    const bool drawPlot = options && plotMode != PLOT_OFF;
    const int labelsMode =
        options ? options->channelLabelsMode : CHANNEL_LABELS_BOTH;
    const bool labelsEnabled = labelsMode != CHANNEL_LABELS_OFF;
    const bool drawLabels = labelsEnabled && (drawViz || drawText);
    const bool stretchChannels =
        options && options->channelLayoutMode == CHANNEL_LAYOUT_STRETCH;
    const bool horizontal =
        options && options->displayOrientation == DISPLAY_HORIZONTAL;
    const float top = 2.f;
    const float bottom = 2.f;
    const float labelReserve = labelsEnabled ? 18.f : 0.f;
    const float labelTextGap = labelsEnabled ? 3.f : 0.f;
    const float leftLabelX = labelReserve - labelTextGap;
    const float rightLabelX = box.size.x - labelReserve + labelTextGap;
    const float centerGap = 4.f;
    const float leftValueX = labelsEnabled ? labelReserve : 0.f;
    const float rightValueRight =
        labelsEnabled ? box.size.x - labelReserve : box.size.x;
    const float barCenterGap = drawText ? centerGap : 6.f;
    const float barLeftX = labelReserve;
    const float barRightValueRight = box.size.x - labelReserve;
    const float barWidth =
        std::max(8.f, (box.size.x - labelReserve * 2.f - barCenterGap) * 0.5f);
    const float barRightX = barRightValueRight - barWidth;
    const int verticalSlots =
        stretchChannels ? activeSlotCount(compolyDisplay) : 16;
    const float rowHeight =
        (box.size.y - top - bottom) / std::max(1, verticalSlots);

    if (drawPlot) {
      if (!args.fb) {
        plotDisplay.render(
            args.vg, *snapshot, *options,
            std::max(1, static_cast<int>(std::ceil(box.size.x))),
            std::max(1, static_cast<int>(std::ceil(box.size.y))));
      }
      plotDisplay.draw(args.vg, box.size.x, box.size.y);
    }

    if (horizontal) {
      std::shared_ptr<Font> font;
      std::shared_ptr<Font> symbolFont;
      float charW = 0.f;
      if (drawLabels || drawText) {
        font = APP->window->loadFont(asset::plugin(pluginInstance, fontPath));
        if (!font) {
          drawHorizontal(args.vg, -1, -1, 0.f, barsMode, textMode,
                         compolyRepresentation, compolyDisplay, drawViz, false,
                         false, stretchChannels);
          return;
        }
        symbolFont = APP->window->loadFont(
            asset::plugin(pluginInstance, symbolFontPath));

        nvgFontFaceId(args.vg, font->handle);
        nvgFontSize(args.vg, 12.f);
        nvgTextLetterSpacing(args.vg, 0.f);
        nvgTextLineHeight(args.vg, 1.f);
        charW = cpx::complex_text::charWidth(args.vg);
      }
      drawHorizontal(
          args.vg, font ? font->handle : -1,
          symbolFont ? symbolFont->handle : (font ? font->handle : -1), charW,
          barsMode, textMode, compolyRepresentation, compolyDisplay, drawViz,
          drawText, drawLabels, stretchChannels);
      return;
    }

    if (drawViz) {
      for (int slot = 0; slot < verticalSlots; slot++) {
        const int c = stretchChannels ? slot : slot;
        const float textY = top + slot * rowHeight + rowHeight * 0.5f;
        const float slotMargin = stretchChannels ? 2.f : 0.f;
        const float barHeight =
            stretchChannels ? std::max(2.f, rowHeight - slotMargin * 2.f)
                            : 14.f;
        if (compolyDisplay) {
          if (isCompolyChannelActive(c)) {
            float first = 0.f;
            float second = 0.f;
            compolyBarParts(c, compolyRepresentation, first, second);
            drawPartBars(args.vg, barLeftX, barRightX, textY, barWidth, first,
                         second, barsMode, barHeight);
          }
        } else {
          const bool drawFirst = isLeftChannelActive(c);
          const bool drawSecond = isRightChannelActive(c);
          drawPartBars(args.vg, barLeftX, barRightX, textY, barWidth,
                       drawFirst ? snapshot->leftVoltages[c] : 0.f,
                       drawSecond ? snapshot->rightVoltages[c] : 0.f, barsMode,
                       barHeight, drawFirst, drawSecond);
        }
      }
    }

    if (!drawLabels && !drawText) return;

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

    for (int slot = 0; slot < verticalSlots; slot++) {
      const int c = stretchChannels ? slot : slot;
      const float y = top + slot * rowHeight + rowHeight * 0.55f;

      if (drawLabels && !compolyDisplay) {
        drawChannelLabel(args.vg, leftLabelX, y, c, isLeftChannelActive(c),
                         NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE, font->handle);
        drawChannelLabel(args.vg, rightLabelX, y, c, isRightChannelActive(c),
                         NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE, font->handle);
      } else if (drawLabels) {
        if (labelsMode == CHANNEL_LABELS_LEFT ||
            labelsMode == CHANNEL_LABELS_BOTH) {
          drawChannelLabel(args.vg, leftLabelX, y, c, isCompolyChannelActive(c),
                           NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE, font->handle);
        }
        if (labelsMode == CHANNEL_LABELS_RIGHT ||
            labelsMode == CHANNEL_LABELS_BOTH) {
          drawChannelLabel(args.vg, rightLabelX, y, c,
                           isCompolyChannelActive(c),
                           NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE, font->handle);
        }
      }

      if (!drawText) continue;

      if (!compolyDisplay) {
        drawPolyText(args.vg, font->handle, leftValueX, rightValueRight, y, c);
      } else {
        const float textW = compolyTextWidth(charW, compolyRepresentation);
        float x = leftValueX;
        if (textMode == TEXT_MIDDLE) {
          x = box.size.x * 0.5f - textW * 0.5f;
        } else if (textMode == TEXT_RIGHT) {
          x = rightValueRight - textW;
        }
        if (compolyRepresentation == COMPOLY_REP_RECT) {
          drawCompolyRectText(args.vg, font->handle,
                              symbolFont ? symbolFont->handle : font->handle, x,
                              y, c, charW);
        } else {
          drawCompolyPolarText(args.vg, font->handle, x, y, c, charW);
        }
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

  int activeSlotCount(bool compolyDisplay) const {
    if (!snapshot) return 1;
    if (!compolyDisplay) {
      return std::max(
          1, std::max(snapshot->leftChannels, snapshot->rightChannels));
    }
    return std::max(1, snapshot->compolyChannels);
  }

  void compolyBarParts(int channel, int compolyRepresentation, float& first,
                       float& second) const {
    if (!snapshot || !isCompolyChannelActive(channel)) {
      first = 0.f;
      second = 0.f;
      return;
    }

    if (compolyRepresentation == COMPOLY_REP_POLAR) {
      first = snapshot->polarR[channel];
      second = cpx::complex_math::thetaRadiansToCableVoltage(
          snapshot->polarTheta[channel]);
      return;
    }

    first = snapshot->rectX[channel];
    second = snapshot->rectY[channel];
  }

  void drawPartBars(NVGcontext* vg, float leftX, float rightX, float centerY,
                    float width, float first, float second, int mode,
                    float height, bool drawFirst = true,
                    bool drawSecond = true) {
    const bool middle = mode == BARS_UNI_MID;
    if (drawFirst) {
      drawBar(vg, leftX, centerY, width, first, middle, mode, height);
    }
    if (drawSecond) {
      drawBar(vg, rightX, centerY, width, second, !middle, mode, height);
    }
  }

  void drawBar(NVGcontext* vg, float x, float centerY, float width,
               float voltage, bool rightAligned, int mode,
               float height = 14.f) {
    const float y = centerY - height * 0.5f;
    const float clamped = clamp(std::fabs(voltage) / 10.f, 0.f, 1.f);
    const float barWidth = std::max(2.f, width * clamped);
    const NVGcolor fill = voltage >= 0.f ? nvgRGBA(0x24, 0xC9, 0xA6, 0x80)
                                         : nvgRGBA(0xC4, 0x34, 0x21, 0x80);

    if (!options || options->barBackgroundEnabled) {
      nvgBeginPath(vg);
      nvgRoundedRect(vg, x, y, width, height, 2.f);
      nvgFillColor(vg, nvgRGBA(0x23, 0x25, 0x27, 0xC0));
      nvgFill(vg);
    }

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
                        int align = NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE,
                        int fontHandle = -1) {
    if (fontHandle >= 0) {
      nvgFontFaceId(vg, fontHandle);
      nvgFontSize(vg, 12.f);
      nvgTextLetterSpacing(vg, 0.f);
    }
    nvgFillColor(vg, active ? nvgRGB(0xF3, 0xF1, 0xDE)
                            : nvgRGBA(0xF3, 0xF1, 0xDE, 0x55));
    nvgTextAlign(vg, align);
    nvgText(vg, x, y, string::f("%d", channel + 1).c_str(), nullptr);
  }

  void drawPolyText(NVGcontext* vg, int fontHandle, float leftValueX,
                    float rightValueRight, float y, int channel) {
    nvgFontFaceId(vg, fontHandle);
    nvgFontSize(vg, 12.f);

    nvgFillColor(vg, nvgRGB(0xC8, 0xEA, 0xE2));
    if (isLeftChannelActive(channel)) {
      nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
      std::string left = formatVoltage(snapshot->leftVoltages[channel]);
      nvgText(vg, leftValueX, y, left.c_str(), nullptr);
    }

    if (isRightChannelActive(channel)) {
      nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
      std::string right = formatVoltage(snapshot->rightVoltages[channel]);
      nvgText(vg, rightValueRight, y, right.c_str(), nullptr);
    }
  }

  void drawCompolyRectText(NVGcontext* vg, int fontHandle, int symbolFontHandle,
                           float valueX, float y, int channel, float charW,
                           int align = NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE) {
    if (!isCompolyChannelActive(channel)) return;

    nvgFontFaceId(vg, fontHandle);
    nvgFontSize(vg, 12.f);

    cpx::complex_text::FixedComplexTextStyle style;
    style.textAlign = align;
    cpx::complex_text::drawRect(vg, fontHandle, symbolFontHandle, valueX, y,
                                snapshot->rectX[channel],
                                snapshot->rectY[channel], charW, style);
  }

  void drawCompolyPolarText(NVGcontext* vg, int fontHandle, float valueX,
                            float y, int channel, float charW,
                            int align = NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE) {
    if (!isCompolyChannelActive(channel)) return;

    nvgFontFaceId(vg, fontHandle);
    nvgFontSize(vg, 12.f);

    cpx::complex_text::FixedComplexTextStyle style;
    style.textAlign = align;
    cpx::complex_text::drawPolar(vg, fontHandle, valueX, y,
                                 snapshot->polarR[channel],
                                 snapshot->polarTheta[channel], charW, style);
  }

  float compolyTextWidth(float charW, int compolyRepresentation) const {
    return charW * (compolyRepresentation == COMPOLY_REP_RECT ? 14.f : 15.f);
  }

  void drawHorizontal(NVGcontext* vg, int fontHandle, int symbolFontHandle,
                      float charW, int barsMode, int textMode,
                      int compolyRepresentation, bool compolyDisplay,
                      bool drawViz, bool drawText, bool drawLabels,
                      bool stretchChannels) {
    if (!snapshot) return;

    const int slots = stretchChannels ? activeSlotCount(compolyDisplay) : 16;
    const float slotW = box.size.x / std::max(1, slots);
    const float labelTopY = 12.f;
    const float labelBottomY = box.size.y - 8.f;
    const float barWidth =
        std::max(3.f, slotW * (stretchChannels ? 0.9f : 0.62f));
    const float edgeMargin = drawLabels ? 18.f : 0.f;
    const float centerMargin = drawText ? 24.f : 6.f;
    const float topBarTop = edgeMargin;
    const float topBarBottom = box.size.y * 0.5f - centerMargin * 0.5f;
    const float bottomBarTop = box.size.y * 0.5f + centerMargin * 0.5f;
    const float bottomBarBottom = box.size.y - edgeMargin;
    const float topBarHeight = std::max(2.f, topBarBottom - topBarTop);
    const float bottomBarHeight = std::max(2.f, bottomBarBottom - bottomBarTop);

    if (drawViz) {
      for (int c = 0; c < slots && c < kMaxChannels; c++) {
        const float x = c * slotW + slotW * 0.5f;
        if (compolyDisplay) {
          if (isCompolyChannelActive(c)) {
            float first = 0.f;
            float second = 0.f;
            compolyBarParts(c, compolyRepresentation, first, second);
            drawVerticalBar(vg, x - barWidth * 0.5f, topBarTop, barWidth,
                            topBarHeight, first, true, barsMode);
            drawVerticalBar(vg, x - barWidth * 0.5f, bottomBarTop, barWidth,
                            bottomBarHeight, second, false, barsMode);
          }
        } else {
          if (isLeftChannelActive(c)) {
            drawVerticalBar(vg, x - barWidth * 0.5f, topBarTop, barWidth,
                            topBarHeight, snapshot->leftVoltages[c], true,
                            barsMode);
          }
          if (isRightChannelActive(c)) {
            drawVerticalBar(vg, x - barWidth * 0.5f, bottomBarTop, barWidth,
                            bottomBarHeight, snapshot->rightVoltages[c], false,
                            barsMode);
          }
        }
      }
    }

    for (int c = 0; c < slots && c < kMaxChannels; c++) {
      const float x = c * slotW + slotW * 0.5f;
      if (drawLabels) {
        if (!compolyDisplay) {
          drawChannelLabel(vg, x, labelTopY, c, isLeftChannelActive(c),
                           NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE, fontHandle);
          drawChannelLabel(vg, x, labelBottomY, c, isRightChannelActive(c),
                           NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE, fontHandle);
        } else {
          const int labelMode =
              options ? options->channelLabelsMode : CHANNEL_LABELS_BOTH;
          if (labelMode == CHANNEL_LABELS_LEFT ||
              labelMode == CHANNEL_LABELS_BOTH) {
            drawChannelLabel(vg, x, labelTopY, c, isCompolyChannelActive(c),
                             NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE, fontHandle);
          }
          if (labelMode == CHANNEL_LABELS_RIGHT ||
              labelMode == CHANNEL_LABELS_BOTH) {
            drawChannelLabel(vg, x, labelBottomY, c, isCompolyChannelActive(c),
                             NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE, fontHandle);
          }
        }
      }

      if (!drawText) continue;

      nvgFontFaceId(vg, fontHandle);
      nvgFontSize(vg, 12.f);
      nvgFillColor(vg, nvgRGB(0xC8, 0xEA, 0xE2));
      if (!compolyDisplay) {
        if (isLeftChannelActive(c)) {
          nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
          std::string left = formatVoltage(snapshot->leftVoltages[c]);
          nvgText(vg, x, box.size.y * 0.5f - 10.f, left.c_str(), nullptr);
        }
        if (isRightChannelActive(c)) {
          nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
          std::string right = formatVoltage(snapshot->rightVoltages[c]);
          nvgText(vg, x, box.size.y * 0.5f + 10.f, right.c_str(), nullptr);
        }
      } else if (isCompolyChannelActive(c)) {
        float y = box.size.y * 0.5f;
        if (textMode == TEXT_LEFT) y = box.size.y * 0.5f - 10.f;
        if (textMode == TEXT_RIGHT) y = box.size.y * 0.5f + 10.f;
        const float textX =
            x - compolyTextWidth(charW, compolyRepresentation) * 0.5f;
        if (compolyRepresentation == COMPOLY_REP_RECT) {
          drawCompolyRectText(vg, fontHandle, symbolFontHandle, textX, y, c,
                              charW);
        } else {
          drawCompolyPolarText(vg, fontHandle, textX, y, c, charW);
        }
      }
    }
  }

  void drawVerticalBar(NVGcontext* vg, float x, float y, float width,
                       float height, float voltage, bool growUp, int mode) {
    const float clamped = clamp(std::fabs(voltage) / 10.f, 0.f, 1.f);
    const NVGcolor fill = voltage >= 0.f ? nvgRGBA(0x24, 0xC9, 0xA6, 0x80)
                                         : nvgRGBA(0xC4, 0x34, 0x21, 0x80);

    if (!options || options->barBackgroundEnabled) {
      nvgBeginPath(vg);
      nvgRoundedRect(vg, x, y, width, height, 2.f);
      nvgFillColor(vg, nvgRGBA(0x23, 0x25, 0x27, 0xC0));
      nvgFill(vg);
    }

    float by = growUp ? y + height * (1.f - clamped) : y;
    float bh = std::max(2.f, height * clamped);
    if (growUp) by = y + height - bh;
    if (mode == BARS_BIPOLAR) {
      const float centerY = y + height * 0.5f;
      bh = std::max(2.f, height * 0.5f * clamped);
      by = voltage >= 0.f ? centerY - bh : centerY;
    }

    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, by, width, bh, 2.f);
    nvgFillColor(vg, fill);
    nvgFill(vg);
  }
};

}  // namespace nudiebug
