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
void highlightBorderPath(NVGcontext* vg, float x, float y, float width,
                         float height, float distortion, unsigned seed) {
  if (distortion <= 0.f) {
    nvgRoundedRect(vg, x, y, width, height, 2.f);
    return;
  }
  float amount = std::min(distortion, std::min(width, height) * .08f);
  auto jitter = [&](unsigned corner) {
    unsigned bits = seed * 0x9e3779b9u + corner * 0x85ebca6bu;
    bits ^= bits >> 16;
    bits *= 0x7feb352du;
    return (float(bits & 1023u) / 1023.f - .5f) * amount * 2.f;
  };
  Vec corners[] = {Vec(x + jitter(0), y + jitter(1)),
                   Vec(x + width + jitter(2), y + jitter(3)),
                   Vec(x + width + jitter(4), y + height + jitter(5)),
                   Vec(x + jitter(6), y + height + jitter(7))};
  bool curved = jitter(17) > 0.f;
  if (curved) {
    Vec entry[4], exit[4];
    for (int i = 0; i < 4; ++i) {
      entry[i] =
          corners[i].plus(corners[(i + 3) % 4].minus(corners[i]).mult(.12f));
      exit[i] =
          corners[i].plus(corners[(i + 1) % 4].minus(corners[i]).mult(.12f));
    }
    nvgMoveTo(vg, entry[0].x, entry[0].y);
    for (int i = 0; i < 4; ++i) {
      nvgQuadTo(vg, corners[i].x, corners[i].y, exit[i].x, exit[i].y);
      Vec edge = entry[(i + 1) % 4].minus(exit[i]);
      float length = std::max(.01f, edge.norm());
      Vec bend = Vec(-edge.y, edge.x).mult(jitter(20 + i) / length);
      Vec first = exit[i].plus(edge.mult(1.f / 3.f)).plus(bend);
      Vec second = exit[i].plus(edge.mult(2.f / 3.f)).plus(bend);
      Vec end = entry[(i + 1) % 4];
      nvgBezierTo(vg, first.x, first.y, second.x, second.y, end.x, end.y);
    }
  } else {
    nvgMoveTo(vg, corners[0].x, corners[0].y);
    for (int i = 1; i < 4; ++i) nvgLineTo(vg, corners[i].x, corners[i].y);
  }
  nvgClosePath(vg);
  nvgLineJoin(vg, curved ? NVG_ROUND : NVG_MITER);
}
}  // namespace

void ComputerscareTextEditor::draw(const DrawArgs& args) {
  nvgScissor(args.vg, RECT_ARGS(args.clipBox));

  nvgBeginPath(args.vg);
  nvgRoundedRect(args.vg, 0.f, 0.f, box.size.x, box.size.y, style.cornerRadius);
  nvgFillColor(args.vg, style.backgroundColor);
  nvgFill(args.vg);

  drawHighlightBackgrounds(args);

  if (style.drawBorder) {
    nvgBeginPath(args.vg);
    nvgRoundedRect(args.vg, 0.5f, 0.5f, box.size.x - 1.f, box.size.y - 1.f,
                   style.cornerRadius);
    NVGcolor border = style.borderColor;
    border.a = this == APP->event->selectedWidget ? 0.95f : 0.45f;
    nvgStrokeColor(args.vg, border);
    nvgStrokeWidth(args.vg, 1.2f);
    nvgStroke(args.vg);
  }

  nvgResetScissor(args.vg);
}

void ComputerscareTextEditor::drawLayer(const DrawArgs& args, int layer) {
  if (layer == 1) {
    drawEditorText(args);
    nvgScissor(args.vg, RECT_ARGS(args.clipBox));
    drawHighlightForegrounds(args);
    drawHighlightDecorations(args);
    drawCursor(args);
    nvgResetScissor(args.vg);
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
        nvgRect(args.vg, startX - 1.f, row.top, endX - startX + 2.f,
                layout.metrics.lineHeight);
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
        bool italic = false;
        float textOpacity = 1.f;
        if (state)
          for (const auto& highlight : state->highlights) {
            if (highlight.begin < row.end && highlight.end > row.begin) {
              italic = italic || highlight.italic;
              textOpacity = std::min(textOpacity, highlight.textOpacity);
            }
          }
        nvgSave(args.vg);
        NVGcolor textColor = style.textColor;
        textColor.a *= std::max(0.f, std::min(1.f, textOpacity));
        nvgFillColor(args.vg, textColor);
        // Lean the glyphs around their baseline without changing caret
        // advances.
        if (italic)
          nvgTransform(args.vg, 1.f, 0.f, -.18f, 1.f, .18f * row.y, 0.f);
        nvgText(args.vg, layout.metrics.textX, row.y, text.c_str() + row.begin,
                text.c_str() + row.end);
        nvgRestore(args.vg);
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
  if (hideCursor || this != APP->event->selectedWidget || cursor != selection) {
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
  // Center the insertion marker on the measured character boundary instead
  // of placing its entire width to the right of that boundary.
  float cursorWidth = 1.5f / getFontScaleX();
  nvgRect(args.vg, caret.x - cursorWidth * .5f, caret.top, cursorWidth,
          caret.bottom - caret.top);
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

  if (highlight.fullLine) {
    float topY = 0.f;
    float bottomY = layout.metrics.lineHeight;
    bool foundRow = layout.rows.empty();

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
        bottomY = row.top + layout.metrics.lineHeight;
        foundRow = true;
      } else {
        topY = std::min(topY, row.top);
        bottomY = std::max(bottomY, row.top + layout.metrics.lineHeight);
      }
    }

    if (foundRow) {
      // Full-row decoration geometry belongs to the editor, not individual
      // highlight producers. Text padding never shrinks an edge-to-edge row.
      float startX = style.fullWidthRows ? 0.f : layout.metrics.textX - 3.f;
      float endX = style.fullWidthRows ? getScaledBoxSize().x
                                       : layout.metrics.textX +
                                             layout.metrics.visibleWidth + 3.f;
      if (mode == HIGHLIGHT_BACKGROUND) {
        nvgBeginPath(args.vg);
        nvgRect(args.vg, startX, topY, endX - startX, bottomY - topY);
        nvgFillColor(args.vg, highlight.background);
        nvgFill(args.vg);
      } else if (mode == HIGHLIGHT_BORDER) {
        nvgBeginPath(args.vg);
        nvgLineJoin(args.vg, NVG_MITER);
        float inset = highlight.borderWidth * .5f;
        nvgRect(args.vg, startX + inset, topY + inset,
                std::max(0.f, endX - startX - 2.f * inset),
                std::max(0.f, bottomY - topY - 2.f * inset));
        nvgStrokeColor(args.vg, highlight.border);
        nvgStrokeWidth(args.vg, highlight.borderWidth);
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
      nvgRect(args.vg, startX - 1.f, topY, endX - startX + 2.f,
              layout.metrics.lineHeight);
      nvgFillColor(args.vg, highlight.background);
      nvgFill(args.vg);
    } else if (mode == HIGHLIGHT_BORDER) {
      nvgBeginPath(args.vg);
      highlightBorderPath(args.vg, startX - 1.5f, topY + .5f,
                          endX - startX + 3.f, layout.metrics.lineHeight,
                          highlight.borderDistortion, unsigned(begin));
      nvgStrokeColor(args.vg, highlight.border);
      nvgStrokeWidth(args.vg, highlight.borderWidth);
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
