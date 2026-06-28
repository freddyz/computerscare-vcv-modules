#include "ComputerscareTextEditor.hpp"

#include <algorithm>

namespace {
struct ComputerscareTextEditorMetrics {
  float textX = BND_TEXT_RADIUS;
  float baselineY = BND_WIDGET_HEIGHT - BND_TEXT_PAD_DOWN;
  float width = 0.f;
  float desc = 0.f;
  float lineHeight = BND_WIDGET_HEIGHT;
};

ComputerscareTextEditorMetrics getEditorMetrics(NVGcontext* vg, Vec boxSize) {
  ComputerscareTextEditorMetrics metrics;
  metrics.width = boxSize.x - 2.f * BND_TEXT_RADIUS;
  nvgTextMetrics(vg, NULL, &metrics.desc, &metrics.lineHeight);
  return metrics;
}

float xForTextOffset(NVGcontext* vg, const char* label, NVGtextRow& row,
                     float rowX, float rowY, int offset, float defaultX) {
  static NVGglyphPosition glyphs[BND_MAX_GLYPHS];
  int glyphCount = nvgTextGlyphPositions(vg, rowX, rowY, row.start, row.end + 1,
                                         glyphs, BND_MAX_GLYPHS);
  float x = defaultX;
  for (int i = 0; i < glyphCount; i++) {
    int glyphOffset = glyphs[i].str - label;
    if (glyphOffset <= offset) {
      x = glyphs[i].x;
    }
    if (glyphOffset >= offset) {
      return glyphs[i].x;
    }
  }
  return x;
}
}  // namespace

ComputerscareTextEditor::ComputerscareTextEditor() {
  multiline = true;
  placeholder = "";
}

void ComputerscareTextEditor::setState(
    ComputerscareTextEditorState* editorState) {
  state = editorState;
  if (state) {
    setText(state->text);
    state->dirty = false;
  }
}

void ComputerscareTextEditor::step() {
  ui::TextField::step();
  if (state && state->dirty) {
    setText(state->text);
    state->dirty = false;
  }
}

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
  }
  Widget::drawLayer(args, layer);
}

void ComputerscareTextEditor::onButton(const ButtonEvent& e) {
  ui::TextField::onButton(e);
  if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
    APP->event->setSelectedWidget(this);
  }
}

void ComputerscareTextEditor::onChange(const ChangeEvent& e) {
  if (state) {
    state->text = text;
  }
}

int ComputerscareTextEditor::getTextPosition(Vec mousePos) {
  std::shared_ptr<Font> font = loadEditorFont();
  if (font && font->handle >= 0) {
    bndSetFont(font->handle);
    int textPos = bndTextFieldTextPosition(
        APP->window->vg, 0.f, 0.f, box.size.x, box.size.y, -1, text.c_str(),
        mousePos.x, mousePos.y);
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

void ComputerscareTextEditor::drawHighlightBackgrounds(const DrawArgs& args) {
  if (!state) {
    return;
  }

  for (const ComputerscareTextHighlight& highlight : state->highlights) {
    if (highlight.hasBackground && highlight.background.a > 0.f) {
      drawHighlightSpan(args, highlight, false);
    }
  }
}

void ComputerscareTextEditor::drawEditorText(const DrawArgs& args) {
  nvgScissor(args.vg, RECT_ARGS(args.clipBox));
  std::shared_ptr<Font> font = loadEditorFont();
  if (font && font->handle >= 0) {
    bndSetFont(font->handle);
    nvgFontFaceId(args.vg, font->handle);
    nvgFontSize(args.vg, BND_LABEL_FONT_SIZE);

    int begin = std::min(cursor, selection);
    int end =
        this == APP->event->selectedWidget ? std::max(cursor, selection) : -1;
    bndIconLabelCaret(args.vg, 0.f, 0.f, box.size.x, box.size.y, -1,
                      style.textColor, BND_LABEL_FONT_SIZE, text.c_str(),
                      style.selectionColor, begin, end);

    if (text.empty() && !placeholder.empty()) {
      bndIconLabelCaret(args.vg, 0.f, 0.f, box.size.x, box.size.y, -1,
                        style.placeholderColor, BND_LABEL_FONT_SIZE,
                        placeholder.c_str(), style.placeholderColor, 0, -1);
    }

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
      drawHighlightSpan(args, highlight, true);
    }
  }
}

void ComputerscareTextEditor::drawHighlightSpan(
    const DrawArgs& args, const ComputerscareTextHighlight& highlight,
    bool foreground) {
  if (text.empty() || highlight.end <= highlight.begin) {
    return;
  }

  std::shared_ptr<Font> font = loadEditorFont();
  if (!font || font->handle < 0) {
    return;
  }

  const int textSize = text.size();
  const int begin = std::max(0, std::min(highlight.begin, textSize));
  const int end = std::max(begin, std::min(highlight.end, textSize));
  if (begin == end) {
    return;
  }

  bndSetFont(font->handle);
  nvgFontFaceId(args.vg, font->handle);
  nvgFontSize(args.vg, BND_LABEL_FONT_SIZE);
  nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);

  ComputerscareTextEditorMetrics metrics = getEditorMetrics(args.vg, box.size);
  static NVGtextRow rows[BND_MAX_ROWS];
  int rowCount = nvgTextBreakLines(args.vg, text.c_str(), NULL, metrics.width,
                                   rows, BND_MAX_ROWS);
  const char* label = text.c_str();

  for (int rowIndex = 0; rowIndex < rowCount; rowIndex++) {
    NVGtextRow& row = rows[rowIndex];
    float rowX = metrics.textX;
    float rowY = metrics.baselineY + rowIndex * metrics.lineHeight;

    int rowBegin = row.start - label;
    int rowEnd = row.end - label;
    int spanBegin = std::max(begin, rowBegin);
    int spanEnd = std::min(end, rowEnd);
    if (spanBegin >= spanEnd) {
      continue;
    }

    float startX = xForTextOffset(args.vg, label, row, rowX, rowY, spanBegin,
                                  rowX + row.minx);
    float endX = xForTextOffset(args.vg, label, row, rowX, rowY, spanEnd,
                                rowX + row.maxx);
    float topY = rowY - metrics.lineHeight - metrics.desc;

    if (foreground) {
      std::string spanText = text.substr(spanBegin, spanEnd - spanBegin);
      nvgFillColor(args.vg, highlight.foreground);
      nvgText(args.vg, startX, rowY, spanText.c_str(), NULL);
    } else {
      nvgBeginPath(args.vg);
      nvgRoundedRect(args.vg, startX - 1.f, topY, endX - startX + 2.f,
                     metrics.lineHeight + 1.f, 2.f);
      nvgFillColor(args.vg, highlight.background);
      nvgFill(args.vg);
    }
  }

  bndSetFont(APP->window->uiFont->handle);
}
