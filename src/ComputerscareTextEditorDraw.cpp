#include <algorithm>
#include <cmath>

#include "ComputerscareTextEditor.hpp"
#include "ComputerscareTextEditorLayout.hpp"

namespace {
enum HighlightDrawMode {
  HIGHLIGHT_BACKGROUND,
  HIGHLIGHT_FOREGROUND,
  HIGHLIGHT_BORDER,
  HIGHLIGHT_PROGRESS
};
}  // namespace

void ComputerscareTextEditor::draw(const DrawArgs& args) {
  nvgScissor(args.vg, RECT_ARGS(args.clipBox));

  nvgBeginPath(args.vg);
  nvgRoundedRect(args.vg, 0.f, 0.f, box.size.x, box.size.y, style.cornerRadius);
  nvgFillColor(args.vg, style.backgroundColor);
  nvgFill(args.vg);

  drawHighlightBackgrounds(args);

  nvgBeginPath(args.vg);
  nvgRoundedRect(args.vg, 0.5f, 0.5f, box.size.x - 1.f, box.size.y - 1.f,
                 style.cornerRadius);
  NVGcolor border = style.borderColor;
  border.a = this == APP->event->selectedWidget ? 0.95f : 0.45f;
  nvgStrokeColor(args.vg, border);
  nvgStrokeWidth(args.vg, 1.2f);
  nvgStroke(args.vg);

  nvgResetScissor(args.vg);
}

void ComputerscareTextEditor::drawLayer(const DrawArgs& args, int layer) {
  if (layer == 1) {
    drawEditorText(args);
    drawHighlightForegrounds(args);
    drawHighlightDecorations(args);
    drawCursor(args);
  }
  Widget::drawLayer(args, layer);
}

int ComputerscareTextEditor::getTextPosition(Vec mousePos) {
  std::shared_ptr<Font> font = loadEditorFont();
  if (font && font->handle >= 0) {
    bndSetFont(font->handle);
    NVGcontext* vg = APP->window->vg;
    nvgSave(vg);
    nvgResetTransform(vg);
    float rackZoom = std::max(0.001f, getAbsoluteZoom());
    nvgScale(vg, rackZoom, rackZoom);
    applyTextStyle(vg, font);
    Vec scaledBox = getScaledBoxSize();
    Vec scaledMouse(mousePos.x / getFontScaleX(), mousePos.y / getFontScaleY());
    ComputerscareTextEditorLayout layout =
        ComputerscareTextEditorLayout::build(vg, text, scaledBox, style);
    int textPos = layout.hitTest(scaledMouse);
    nvgRestore(vg);
    bndSetFont(APP->window->uiFont->handle);
    return textPos;
  }

  return ui::TextField::getTextPosition(mousePos);
}

std::shared_ptr<Font> ComputerscareTextEditor::loadEditorFont() {
  std::shared_ptr<Font> font =
      APP->window->loadFont(asset::system(style.fontPath));
  if (!font || font->handle < 0) {
    font = APP->window->uiFont;
  }
  return font;
}

float ComputerscareTextEditor::getFontScaleX() const {
  return std::max(0.5f, 1.f + style.fontWidthOffset);
}

float ComputerscareTextEditor::getFontScaleY() const {
  return std::max(0.5f, 1.f + style.fontHeightOffset);
}

Vec ComputerscareTextEditor::getScaledBoxSize() const {
  return Vec(box.size.x / getFontScaleX(), box.size.y / getFontScaleY());
}

void ComputerscareTextEditor::applyTextStyle(NVGcontext* vg,
                                             std::shared_ptr<Font> font) {
  nvgFontFaceId(vg, font->handle);
  nvgFontSize(vg, std::max(6.f, style.fontSize));
  nvgTextLetterSpacing(vg, style.letterSpacing);
  nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);
}

void ComputerscareTextEditor::drawHighlightBackgrounds(const DrawArgs& args) {
  if (!state) {
    return;
  }

  for (const ComputerscareTextHighlight& highlight : state->highlights) {
    if (highlight.hasBackground && highlight.background.a > 0.f) {
      drawHighlightSpan(args, highlight, HIGHLIGHT_BACKGROUND);
    }
  }
}

void ComputerscareTextEditor::drawEditorText(const DrawArgs& args) {
  nvgScissor(args.vg, RECT_ARGS(args.clipBox));
  std::shared_ptr<Font> font = loadEditorFont();
  if (font && font->handle >= 0) {
    bndSetFont(font->handle);
    Vec scaledBox = getScaledBoxSize();
    nvgSave(args.vg);
    applyTextStyle(args.vg, font);
    ComputerscareTextEditorLayout layout =
        ComputerscareTextEditorLayout::build(args.vg, text, scaledBox, style);
    nvgRestore(args.vg);

    nvgSave(args.vg);
    nvgScale(args.vg, getFontScaleX(), getFontScaleY());
    applyTextStyle(args.vg, font);
    bool hasSelection =
        this == APP->event->selectedWidget && cursor != selection;
    int selectionBegin = hasSelection ? std::min(cursor, selection) : 0;
    int selectionEnd = hasSelection ? std::max(cursor, selection) : 0;

    if (hasSelection) {
      for (size_t rowIndex = 0; rowIndex < layout.rows.size(); rowIndex++) {
        const ComputerscareTextEditorLayoutRow& row = layout.rows[rowIndex];
        int rowBegin = row.begin;
        int rowEnd = row.end;
        int spanBegin = std::max(selectionBegin, rowBegin);
        int spanEnd = std::min(selectionEnd, rowEnd);
        if (spanBegin >= spanEnd) {
          continue;
        }
        int localBegin = spanBegin - row.begin;
        int localEnd = spanEnd - row.begin;
        float startX = row.carets[localBegin];
        float endX = row.carets[localEnd];
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, startX - 1.f, row.top, endX - startX + 2.f,
                       layout.metrics.lineHeight + 1.f, 2.f);
        nvgFillColor(args.vg, style.selectionColor);
        nvgFill(args.vg);
      }
    }

    if (!text.empty()) {
      nvgFillColor(args.vg, style.textColor);
      for (size_t rowIndex = 0; rowIndex < layout.rows.size(); rowIndex++) {
        const ComputerscareTextEditorLayoutRow& row = layout.rows[rowIndex];
        if (row.end <= row.begin) {
          continue;
        }
        nvgText(args.vg, layout.metrics.textX, row.y, text.c_str() + row.begin,
                text.c_str() + row.end);
      }
    } else if (!placeholder.empty()) {
      nvgFillColor(args.vg, style.placeholderColor);
      nvgText(args.vg, layout.metrics.textX, layout.metrics.baselineY,
              placeholder.c_str(), NULL);
    }

    nvgRestore(args.vg);
    bndSetFont(APP->window->uiFont->handle);
  }
  nvgResetScissor(args.vg);
}

void ComputerscareTextEditor::drawHighlightForegrounds(const DrawArgs& args) {
  if (!state) {
    return;
  }

  for (const ComputerscareTextHighlight& highlight : state->highlights) {
    if (highlight.hasForeground && highlight.foreground.a > 0.f) {
      drawHighlightSpan(args, highlight, HIGHLIGHT_FOREGROUND);
    }
  }
}

void ComputerscareTextEditor::drawHighlightDecorations(const DrawArgs& args) {
  if (!state) {
    return;
  }

  for (const ComputerscareTextHighlight& highlight : state->highlights) {
    if (highlight.hasProgress && highlight.progressColor.a > 0.f) {
      drawHighlightSpan(args, highlight, HIGHLIGHT_PROGRESS);
    }
    if (highlight.hasBorder && highlight.border.a > 0.f) {
      drawHighlightSpan(args, highlight, HIGHLIGHT_BORDER);
    }
  }
}

void ComputerscareTextEditor::drawCursor(const DrawArgs& args) {
  if (this != APP->event->selectedWidget || cursor != selection) {
    return;
  }

  std::shared_ptr<Font> font = loadEditorFont();
  if (!font || font->handle < 0) {
    return;
  }

  bndSetFont(font->handle);
  int cursorOffset = std::max(0, std::min(cursor, (int)text.size()));
  nvgSave(args.vg);
  applyTextStyle(args.vg, font);
  ComputerscareTextEditorLayout layout = ComputerscareTextEditorLayout::build(
      args.vg, text, getScaledBoxSize(), style);
  ComputerscareTextEditorCaret caret =
      layout.caretForOffset(text, cursorOffset);
  nvgRestore(args.vg);

  nvgSave(args.vg);
  nvgScale(args.vg, getFontScaleX(), getFontScaleY());
  applyTextStyle(args.vg, font);

  float blink = 0.5f + 0.5f * std::sin((float)rack::system::getTime() * 3.f);
  NVGcolor color = COLOR_COMPUTERSCARE_BLUE;
  color.r = color.r + (1.f - color.r) * blink * 0.48f;
  color.g = color.g + (1.f - color.g) * blink * 0.48f;
  color.b = color.b + (1.f - color.b) * blink * 0.48f;
  color.a = 0.72f + 0.28f * blink;
  nvgBeginPath(args.vg);
  nvgRect(args.vg, caret.x, caret.top, 2.2f, caret.bottom - caret.top);
  nvgFillColor(args.vg, color);
  nvgFill(args.vg);

  nvgRestore(args.vg);
  bndSetFont(APP->window->uiFont->handle);
}

void ComputerscareTextEditor::drawHighlightSpan(
    const DrawArgs& args, const ComputerscareTextHighlight& highlight,
    int mode) {
  if (highlight.end < highlight.begin ||
      (highlight.end == highlight.begin && !highlight.fullLine)) {
    return;
  }

  std::shared_ptr<Font> font = loadEditorFont();
  if (!font || font->handle < 0) {
    return;
  }

  const int textSize = text.size();
  const int begin = std::max(0, std::min(highlight.begin, textSize));
  const int end = std::max(begin, std::min(highlight.end, textSize));
  if (begin == end && !highlight.fullLine) {
    return;
  }

  bndSetFont(font->handle);
  nvgSave(args.vg);
  applyTextStyle(args.vg, font);
  ComputerscareTextEditorLayout layout = ComputerscareTextEditorLayout::build(
      args.vg, text, getScaledBoxSize(), style);
  nvgRestore(args.vg);

  nvgSave(args.vg);
  nvgScale(args.vg, getFontScaleX(), getFontScaleY());
  applyTextStyle(args.vg, font);

  if (highlight.fullLine && (text.empty() || begin == end)) {
    float topY = layout.rows.empty() ? 0.f : layout.rows.front().top;
    for (size_t rowIndex = 0; rowIndex < layout.rows.size(); rowIndex++) {
      const ComputerscareTextEditorLayoutRow& row = layout.rows[rowIndex];
      if (row.begin == begin && row.end == begin) {
        topY = row.top;
        break;
      }
    }
    if (mode == HIGHLIGHT_BACKGROUND) {
      nvgBeginPath(args.vg);
      nvgRoundedRect(args.vg, layout.metrics.textX - 2.f, topY,
                     layout.metrics.visibleWidth + 4.f,
                     layout.metrics.lineHeight + 1.f, 2.f);
      nvgFillColor(args.vg, highlight.background);
      nvgFill(args.vg);
    }
    nvgRestore(args.vg);
    bndSetFont(APP->window->uiFont->handle);
    return;
  }

  if (highlight.fullLine) {
    float topY = 0.f;
    float bottomY = 0.f;
    bool foundRow = false;

    for (size_t rowIndex = 0; rowIndex < layout.rows.size(); rowIndex++) {
      const ComputerscareTextEditorLayoutRow& row = layout.rows[rowIndex];
      int rowBegin = row.begin;
      int rowEnd = row.end;
      bool emptyRowInRange =
          rowBegin == rowEnd && rowBegin >= begin && rowBegin <= end;
      if (!emptyRowInRange && (rowEnd <= begin || rowBegin >= end)) {
        continue;
      }

      if (!foundRow) {
        topY = row.top;
        bottomY = row.bottom;
        foundRow = true;
      } else {
        topY = std::min(topY, row.top);
        bottomY = std::max(bottomY, row.bottom);
      }
    }

    if (foundRow) {
      float startX = layout.metrics.textX - 2.f;
      float endX = layout.metrics.textX + layout.metrics.visibleWidth + 2.f;
      if (mode == HIGHLIGHT_BACKGROUND) {
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, startX - 1.f, topY, endX - startX + 2.f,
                       bottomY - topY, 2.f);
        nvgFillColor(args.vg, highlight.background);
        nvgFill(args.vg);
      } else if (mode == HIGHLIGHT_BORDER) {
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, startX - 1.5f, topY + 0.5f, endX - startX + 3.f,
                       bottomY - topY - 0.5f, 2.f);
        nvgStrokeColor(args.vg, highlight.border);
        nvgStrokeWidth(args.vg, 1.2f);
        nvgStroke(args.vg);
      } else if (mode == HIGHLIGHT_PROGRESS) {
        float progress = std::max(0.f, std::min(highlight.progress, 1.f));
        if (highlight.progressSegments > 1) {
          int filledSegments =
              std::min(highlight.progressSegments,
                       (int)std::floor(progress * highlight.progressSegments));
          float segmentWidth =
              (endX - startX) / (float)highlight.progressSegments;
          float width = segmentWidth * filledSegments;
          if (width > 0.f) {
            nvgBeginPath(args.vg);
            nvgRect(args.vg, startX, bottomY - 2.f, width, 2.f);
            nvgFillColor(args.vg, highlight.progressColor);
            nvgFill(args.vg);
          }
        } else {
          float width = (endX - startX) * progress;
          if (width > 0.f) {
            nvgBeginPath(args.vg);
            nvgRect(args.vg, startX, bottomY - 2.f, width, 2.f);
            nvgFillColor(args.vg, highlight.progressColor);
            nvgFill(args.vg);
          }
        }
      }
    }

    nvgRestore(args.vg);
    bndSetFont(APP->window->uiFont->handle);
    return;
  }

  for (size_t rowIndex = 0; rowIndex < layout.rows.size(); rowIndex++) {
    const ComputerscareTextEditorLayoutRow& row = layout.rows[rowIndex];
    int rowBegin = row.begin;
    int rowEnd = row.end;
    int spanBegin = std::max(begin, rowBegin);
    int spanEnd = std::min(end, rowEnd);
    if (spanBegin >= spanEnd) {
      continue;
    }

    float startX = row.carets[spanBegin - row.begin];
    float endX = row.carets[spanEnd - row.begin];
    float topY = row.top;

    if (highlight.fullLine) {
      startX = layout.metrics.textX - 2.f;
      endX = layout.metrics.textX + layout.metrics.visibleWidth + 2.f;
    }

    if (mode == HIGHLIGHT_FOREGROUND) {
      std::string spanText = text.substr(spanBegin, spanEnd - spanBegin);
      nvgFillColor(args.vg, highlight.foreground);
      nvgText(args.vg, startX, row.y, spanText.c_str(), NULL);
    } else if (mode == HIGHLIGHT_BACKGROUND) {
      nvgBeginPath(args.vg);
      nvgRoundedRect(args.vg, startX - 1.f, topY, endX - startX + 2.f,
                     layout.metrics.lineHeight + 1.f, 2.f);
      nvgFillColor(args.vg, highlight.background);
      nvgFill(args.vg);
    } else if (mode == HIGHLIGHT_BORDER) {
      nvgBeginPath(args.vg);
      nvgRoundedRect(args.vg, startX - 1.5f, topY + 0.5f, endX - startX + 3.f,
                     layout.metrics.lineHeight, 2.f);
      nvgStrokeColor(args.vg, highlight.border);
      nvgStrokeWidth(args.vg, 1.2f);
      nvgStroke(args.vg);
    } else if (mode == HIGHLIGHT_PROGRESS) {
      float progress = std::max(0.f, std::min(highlight.progress, 1.f));
      if (highlight.progressSegments > 1) {
        int filledSegments =
            std::min(highlight.progressSegments,
                     (int)std::floor(progress * highlight.progressSegments));
        float segmentWidth =
            (endX - startX) / (float)highlight.progressSegments;
        float width = segmentWidth * filledSegments;
        if (width > 0.f) {
          nvgBeginPath(args.vg);
          nvgRect(args.vg, startX, topY + layout.metrics.lineHeight - 2.f,
                  width, 2.f);
          nvgFillColor(args.vg, highlight.progressColor);
          nvgFill(args.vg);
        }
      } else {
        float width = (endX - startX) * progress;
        if (width <= 0.f) {
          continue;
        }
        nvgBeginPath(args.vg);
        nvgRect(args.vg, startX, topY + layout.metrics.lineHeight - 2.f, width,
                2.f);
        nvgFillColor(args.vg, highlight.progressColor);
        nvgFill(args.vg);
      }
    }
  }

  nvgRestore(args.vg);
  bndSetFont(APP->window->uiFont->handle);
}
