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
    const bool clearPlotPerFrame = options && options->clearPlotPerFrame;
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
      if (clearPlotPerFrame) {
        plotDisplay.destroyFramebuffer();
        plotDisplay.dotsRenderer.draw(args.vg, *snapshot, box.size.x,
                                      box.size.y, plotMode);
      } else if (!args.fb) {
        plotDisplay.render(
            args.vg, *snapshot, *options,
            std::max(1, static_cast<int>(std::ceil(box.size.x))),
            std::max(1, static_cast<int>(std::ceil(box.size.y))));
        plotDisplay.draw(args.vg, box.size.x, box.size.y);
      }
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

    const int ioMode = options ? options->ioViewMode : IO_VIEW_INPUT;

    if (drawViz) {
      for (int slot = 0; slot < verticalSlots; slot++) {
        const int c = stretchChannels ? slot : slot;
        const float textY = top + slot * rowHeight + rowHeight * 0.5f;
        const float slotMargin = stretchChannels ? 2.f : 0.f;
        const float barHeight =
            stretchChannels ? std::max(2.f, rowHeight - slotMargin * 2.f)
                            : 14.f;
        if (compolyDisplay) {
          drawCompolyRowBars(args.vg, barLeftX, barRightX, textY, barWidth,
                             barsMode, barHeight, c, compolyRepresentation,
                             ioMode);
        } else {
          drawPolyRowBars(args.vg, barLeftX, barRightX, textY, barWidth,
                          barsMode, barHeight, c, ioMode);
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
        const bool leftActive = isLeftSideActive(c, ioMode);
        const bool rightActive = isRightSideActive(c, ioMode);
        drawChannelLabel(args.vg, leftLabelX, y, c, leftActive,
                         NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE, font->handle);
        drawChannelLabel(args.vg, rightLabelX, y, c, rightActive,
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
        if (textMode == TEXT_BOTH) {
          drawPolyText(args.vg, font->handle, leftValueX, rightValueRight, y,
                       c);
        }
        const float textW = compolyTextWidth(charW, compolyRepresentation);
        if (ioMode == IO_VIEW_BOTH && isCompolyChannelActive(c)) {
          drawCompolyText(args.vg, font->handle,
                          symbolFont ? symbolFont->handle : font->handle,
                          leftValueX, y, c, charW, compolyRepresentation);
          drawCompolyText(args.vg, font->handle,
                          symbolFont ? symbolFont->handle : font->handle,
                          rightValueRight - textW, y, c, charW,
                          compolyRepresentation);
        } else {
          float x = leftValueX;
          if (textMode == TEXT_MIDDLE || textMode == TEXT_BOTH) {
            x = box.size.x * 0.5f - textW * 0.5f;
          } else if (textMode == TEXT_RIGHT) {
            x = rightValueRight - textW;
          }
          drawCompolyText(args.vg, font->handle,
                          symbolFont ? symbolFont->handle : font->handle, x, y,
                          c, charW, compolyRepresentation);
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

  bool isLeftOutputActive(int channel) const {
    return snapshot && channel < snapshot->leftOutputChannels;
  }

  bool isRightOutputActive(int channel) const {
    return snapshot && channel < snapshot->rightOutputChannels;
  }

  bool isLeftSideActive(int channel, int ioMode) const {
    if (ioMode == IO_VIEW_INPUT) return isLeftChannelActive(channel);
    if (ioMode == IO_VIEW_OUTPUT) return isLeftOutputActive(channel);
    return isLeftChannelActive(channel) || isLeftOutputActive(channel);
  }

  bool isRightSideActive(int channel, int ioMode) const {
    if (ioMode == IO_VIEW_INPUT) return isRightChannelActive(channel);
    if (ioMode == IO_VIEW_OUTPUT) return isRightOutputActive(channel);
    return isRightChannelActive(channel) || isRightOutputActive(channel);
  }

  bool isCompolyChannelActive(int channel) const {
    return snapshot && channel < snapshot->compolyChannels;
  }

  int activeSlotCount(bool compolyDisplay) const {
    if (!snapshot) return 1;
    const int ioMode = options ? options->ioViewMode : IO_VIEW_INPUT;
    if (!compolyDisplay) {
      int count = 0;
      if (ioMode != IO_VIEW_OUTPUT) {
        count = std::max(count,
                         std::max(snapshot->leftChannels, snapshot->rightChannels));
      }
      if (ioMode != IO_VIEW_INPUT) {
        count = std::max(count, std::max(snapshot->leftOutputChannels,
                                         snapshot->rightOutputChannels));
      }
      return std::max(1, count);
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

  void drawPolyRowBars(NVGcontext* vg, float barLeftX, float barRightX,
                       float centerY, float barWidth, int barsMode,
                       float barHeight, int channel, int ioMode) {
    const bool middle = barsMode == BARS_UNI_MID;
    if (ioMode != IO_VIEW_BOTH) {
      const bool useOutput = ioMode == IO_VIEW_OUTPUT;
      const bool drawFirst =
          useOutput ? isLeftOutputActive(channel) : isLeftChannelActive(channel);
      const bool drawSecond = useOutput ? isRightOutputActive(channel)
                                        : isRightChannelActive(channel);
      const float first =
          useOutput ? snapshot->leftOutputVoltages[channel]
                    : snapshot->leftVoltages[channel];
      const float second =
          useOutput ? snapshot->rightOutputVoltages[channel]
                    : snapshot->rightVoltages[channel];
      drawPartBars(vg, barLeftX, barRightX, centerY, barWidth,
                   drawFirst ? first : 0.f, drawSecond ? second : 0.f, barsMode,
                   barHeight, drawFirst, drawSecond);
      return;
    }

    const float halfWidth = std::max(2.f, barWidth * 0.5f);
    if (isLeftOutputActive(channel)) {
      drawBar(vg, barLeftX, centerY, halfWidth,
              snapshot->leftOutputVoltages[channel], middle, barsMode,
              barHeight);
    }
    if (isLeftChannelActive(channel)) {
      drawBar(vg, barLeftX + halfWidth, centerY, halfWidth,
              snapshot->leftVoltages[channel], middle, barsMode, barHeight);
    }
    if (isRightChannelActive(channel)) {
      drawBar(vg, barRightX, centerY, halfWidth,
              snapshot->rightVoltages[channel], !middle, barsMode, barHeight);
    }
    if (isRightOutputActive(channel)) {
      drawBar(vg, barRightX + halfWidth, centerY, halfWidth,
              snapshot->rightOutputVoltages[channel], !middle, barsMode,
              barHeight);
    }
  }

  void drawCompolyRowBars(NVGcontext* vg, float barLeftX, float barRightX,
                          float centerY, float barWidth, int barsMode,
                          float barHeight, int channel, int compolyRepresentation,
                          int ioMode) {
    if (!isCompolyChannelActive(channel)) return;
    float first = 0.f;
    float second = 0.f;
    compolyBarParts(channel, compolyRepresentation, first, second);
    if (ioMode != IO_VIEW_BOTH) {
      drawPartBars(vg, barLeftX, barRightX, centerY, barWidth, first, second,
                   barsMode, barHeight);
      return;
    }

    const float halfWidth = std::max(2.f, barWidth * 0.5f);
    const bool middle = barsMode == BARS_UNI_MID;
    drawBar(vg, barLeftX, centerY, halfWidth, first, middle, barsMode,
            barHeight);
    drawBar(vg, barLeftX + halfWidth, centerY, halfWidth, first, middle,
            barsMode, barHeight);
    drawBar(vg, barRightX, centerY, halfWidth, second, !middle, barsMode,
            barHeight);
    drawBar(vg, barRightX + halfWidth, centerY, halfWidth, second, !middle,
            barsMode, barHeight);
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

    const int ioMode = options ? options->ioViewMode : IO_VIEW_INPUT;

    if (ioMode != IO_VIEW_BOTH) {
      const bool useOutput = ioMode == IO_VIEW_OUTPUT;
      const bool leftActive = useOutput ? isLeftOutputActive(channel)
                                        : isLeftChannelActive(channel);
      const bool rightActive = useOutput ? isRightOutputActive(channel)
                                         : isRightChannelActive(channel);
      if (leftActive) {
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        const float v = useOutput ? snapshot->leftOutputVoltages[channel]
                                  : snapshot->leftVoltages[channel];
        nvgText(vg, leftValueX, y, formatVoltage(v).c_str(), nullptr);
      }
      if (rightActive) {
        nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
        const float v = useOutput ? snapshot->rightOutputVoltages[channel]
                                  : snapshot->rightVoltages[channel];
        nvgText(vg, rightValueRight, y, formatVoltage(v).c_str(), nullptr);
      }
      return;
    }

    const float centerX = (leftValueX + rightValueRight) * 0.5f;
    const float innerGap = 2.f;
    if (isLeftOutputActive(channel)) {
      nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
      nvgText(vg, leftValueX, y,
              formatVoltage(snapshot->leftOutputVoltages[channel]).c_str(),
              nullptr);
    }
    if (isLeftChannelActive(channel)) {
      nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
      nvgText(vg, centerX - innerGap, y,
              formatVoltage(snapshot->leftVoltages[channel]).c_str(), nullptr);
    }
    if (isRightChannelActive(channel)) {
      nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
      nvgText(vg, centerX + innerGap, y,
              formatVoltage(snapshot->rightVoltages[channel]).c_str(), nullptr);
    }
    if (isRightOutputActive(channel)) {
      nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
      nvgText(vg, rightValueRight, y,
              formatVoltage(snapshot->rightOutputVoltages[channel]).c_str(),
              nullptr);
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

  void drawCompolyText(NVGcontext* vg, int fontHandle, int symbolFontHandle,
                       float x, float y, int channel, float charW,
                       int compolyRepresentation,
                       int align = NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE) {
    if (compolyRepresentation == COMPOLY_REP_RECT) {
      drawCompolyRectText(vg, fontHandle, symbolFontHandle, x, y, channel,
                          charW, align);
    } else {
      drawCompolyPolarText(vg, fontHandle, x, y, channel, charW, align);
    }
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

    const int ioMode = options ? options->ioViewMode : IO_VIEW_INPUT;
    const bool ioBoth = ioMode == IO_VIEW_BOTH;
    const int slots = stretchChannels ? activeSlotCount(compolyDisplay) : 16;
    const float slotW = box.size.x / std::max(1, slots);
    const float labelTopY = 12.f;
    const float labelBottomY = box.size.y - 8.f;
    const float barWidth =
        std::max(3.f, slotW * (stretchChannels ? 0.9f : 0.62f));
    const float edgeMargin = drawLabels ? 18.f : 0.f;
    const float centerMargin =
        drawText ? (ioBoth ? 56.f : 24.f) : 6.f;
    const float topBarTop = edgeMargin;
    const float topBarBottom = box.size.y * 0.5f - centerMargin * 0.5f;
    const float bottomBarTop = box.size.y * 0.5f + centerMargin * 0.5f;
    const float bottomBarBottom = box.size.y - edgeMargin;
    const float topBarHeight = std::max(2.f, topBarBottom - topBarTop);
    const float bottomBarHeight = std::max(2.f, bottomBarBottom - bottomBarTop);
    const float topBarMidY = (topBarTop + topBarBottom) * 0.5f;
    const float bottomBarMidY = (bottomBarTop + bottomBarBottom) * 0.5f;
    const float halfTopBarHeight = std::max(2.f, topBarMidY - topBarTop);
    const float halfBottomBarHeight =
        std::max(2.f, bottomBarBottom - bottomBarMidY);

    if (drawViz) {
      for (int c = 0; c < slots && c < kMaxChannels; c++) {
        const float x = c * slotW + slotW * 0.5f;
        if (compolyDisplay) {
          if (isCompolyChannelActive(c)) {
            float first = 0.f;
            float second = 0.f;
            compolyBarParts(c, compolyRepresentation, first, second);
            if (!ioBoth) {
              drawVerticalBar(vg, x - barWidth * 0.5f, topBarTop, barWidth,
                              topBarHeight, first, true, barsMode);
              drawVerticalBar(vg, x - barWidth * 0.5f, bottomBarTop, barWidth,
                              bottomBarHeight, second, false, barsMode);
            } else {
              drawVerticalBar(vg, x - barWidth * 0.5f, topBarTop, barWidth,
                              halfTopBarHeight, first, true, barsMode);
              drawVerticalBar(vg, x - barWidth * 0.5f, topBarMidY, barWidth,
                              halfTopBarHeight, first, true, barsMode);
              drawVerticalBar(vg, x - barWidth * 0.5f, bottomBarTop, barWidth,
                              halfBottomBarHeight, second, false, barsMode);
              drawVerticalBar(vg, x - barWidth * 0.5f, bottomBarMidY, barWidth,
                              halfBottomBarHeight, second, false, barsMode);
            }
          }
        } else {
          drawHorizontalPolyBars(vg, x - barWidth * 0.5f, barWidth, topBarTop,
                                 topBarMidY, topBarBottom, bottomBarTop,
                                 bottomBarMidY, bottomBarBottom, c, barsMode,
                                 ioMode);
        }
      }
    }

    for (int c = 0; c < slots && c < kMaxChannels; c++) {
      const float x = c * slotW + slotW * 0.5f;
      if (drawLabels) {
        if (!compolyDisplay) {
          const bool leftActive = isLeftSideActive(c, ioMode);
          const bool rightActive = isRightSideActive(c, ioMode);
          drawChannelLabel(vg, x, labelTopY, c, leftActive,
                           NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE, fontHandle);
          drawChannelLabel(vg, x, labelBottomY, c, rightActive,
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
        drawHorizontalPolyText(vg, x, c, ioMode);
      } else if (isCompolyChannelActive(c)) {
        float y = box.size.y * 0.5f;
        if (textMode == TEXT_LEFT) y = box.size.y * 0.5f - 10.f;
        if (textMode == TEXT_RIGHT) y = box.size.y * 0.5f + 10.f;
        if (textMode == TEXT_BOTH) {
          drawHorizontalPolyText(vg, x, c, ioMode);
        }
        const float textX =
            x - compolyTextWidth(charW, compolyRepresentation) * 0.5f;
        drawCompolyText(vg, fontHandle, symbolFontHandle, textX, y, c, charW,
                        compolyRepresentation);
      }
    }
  }

  void drawHorizontalPolyBars(NVGcontext* vg, float x, float barWidth,
                              float topBarTop, float topBarMidY,
                              float topBarBottom, float bottomBarTop,
                              float bottomBarMidY, float bottomBarBottom,
                              int channel, int barsMode, int ioMode) {
    const float topHeight = std::max(2.f, topBarBottom - topBarTop);
    const float bottomHeight = std::max(2.f, bottomBarBottom - bottomBarTop);
    const float halfTopHeight = std::max(2.f, topBarMidY - topBarTop);
    const float halfBottomHeight = std::max(2.f, bottomBarBottom - bottomBarMidY);

    if (ioMode != IO_VIEW_BOTH) {
      const bool useOutput = ioMode == IO_VIEW_OUTPUT;
      if (useOutput ? isLeftOutputActive(channel) : isLeftChannelActive(channel)) {
        const float v = useOutput ? snapshot->leftOutputVoltages[channel]
                                  : snapshot->leftVoltages[channel];
        drawVerticalBar(vg, x, topBarTop, barWidth, topHeight, v, true,
                        barsMode);
      }
      if (useOutput ? isRightOutputActive(channel) : isRightChannelActive(channel)) {
        const float v = useOutput ? snapshot->rightOutputVoltages[channel]
                                  : snapshot->rightVoltages[channel];
        drawVerticalBar(vg, x, bottomBarTop, barWidth, bottomHeight, v, false,
                        barsMode);
      }
      return;
    }

    if (isLeftOutputActive(channel)) {
      drawVerticalBar(vg, x, topBarTop, barWidth, halfTopHeight,
                      snapshot->leftOutputVoltages[channel], true, barsMode);
    }
    if (isLeftChannelActive(channel)) {
      drawVerticalBar(vg, x, topBarMidY, barWidth, halfTopHeight,
                      snapshot->leftVoltages[channel], true, barsMode);
    }
    if (isRightChannelActive(channel)) {
      drawVerticalBar(vg, x, bottomBarTop, barWidth, halfBottomHeight,
                      snapshot->rightVoltages[channel], false, barsMode);
    }
    if (isRightOutputActive(channel)) {
      drawVerticalBar(vg, x, bottomBarMidY, barWidth, halfBottomHeight,
                      snapshot->rightOutputVoltages[channel], false, barsMode);
    }
  }

  void drawHorizontalPolyText(NVGcontext* vg, float x, int channel, int ioMode) {
    const float centerY = box.size.y * 0.5f;
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    if (ioMode != IO_VIEW_BOTH) {
      const bool useOutput = ioMode == IO_VIEW_OUTPUT;
      const bool leftActive = useOutput ? isLeftOutputActive(channel)
                                        : isLeftChannelActive(channel);
      const bool rightActive = useOutput ? isRightOutputActive(channel)
                                         : isRightChannelActive(channel);
      if (leftActive) {
        const float v = useOutput ? snapshot->leftOutputVoltages[channel]
                                  : snapshot->leftVoltages[channel];
        nvgText(vg, x, centerY - 10.f, formatVoltage(v).c_str(), nullptr);
      }
      if (rightActive) {
        const float v = useOutput ? snapshot->rightOutputVoltages[channel]
                                  : snapshot->rightVoltages[channel];
        nvgText(vg, x, centerY + 10.f, formatVoltage(v).c_str(), nullptr);
      }
      return;
    }
    if (isLeftOutputActive(channel)) {
      nvgText(vg, x, centerY - 24.f,
              formatVoltage(snapshot->leftOutputVoltages[channel]).c_str(),
              nullptr);
    }
    if (isLeftChannelActive(channel)) {
      nvgText(vg, x, centerY - 10.f,
              formatVoltage(snapshot->leftVoltages[channel]).c_str(), nullptr);
    }
    if (isRightChannelActive(channel)) {
      nvgText(vg, x, centerY + 10.f,
              formatVoltage(snapshot->rightVoltages[channel]).c_str(), nullptr);
    }
    if (isRightOutputActive(channel)) {
      nvgText(vg, x, centerY + 24.f,
              formatVoltage(snapshot->rightOutputVoltages[channel]).c_str(),
              nullptr);
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
