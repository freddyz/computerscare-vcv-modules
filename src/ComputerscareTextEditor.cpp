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
  lastSnapshot = captureSnapshot();
}

void ComputerscareTextEditor::setState(
    ComputerscareTextEditorState* editorState) {
  state = editorState;
  if (state) {
    suppressChangeTracking = true;
    setText(state->text);
    suppressChangeTracking = false;
    state->dirty = false;
    clearHistory();
    lastSnapshot = captureSnapshot();
  }
}

void ComputerscareTextEditor::step() {
  ui::TextField::step();
  if (state && state->dirty) {
    suppressChangeTracking = true;
    setText(state->text);
    suppressChangeTracking = false;
    state->dirty = false;
    clearHistory();
    lastSnapshot = captureSnapshot();
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
  lastSnapshot = captureSnapshot();
}

void ComputerscareTextEditor::onSelectText(const SelectTextEvent& e) {
  ComputerscareTextEditorSnapshot before = captureSnapshot();
  handlingTrackedInput = true;
  ui::TextField::onSelectText(e);
  handlingTrackedInput = false;
  if (text != before.text) {
    pushUndoSnapshot(before);
    redoStack.clear();
  }
  lastSnapshot = captureSnapshot();
}

void ComputerscareTextEditor::onSelectKey(const SelectKeyEvent& e) {
  if (e.action == GLFW_PRESS || e.action == GLFW_REPEAT) {
    int mods = e.mods & RACK_MOD_MASK;
    bool isZ = e.key == GLFW_KEY_Z || e.keyName == "z";
    bool isY = e.key == GLFW_KEY_Y || e.keyName == "y";
    if ((isZ && mods == (RACK_MOD_CTRL | GLFW_MOD_SHIFT)) ||
        (isY && mods == RACK_MOD_CTRL)) {
      redo();
      e.consume(this);
      return;
    }
    if (isZ && mods == RACK_MOD_CTRL) {
      undo();
      e.consume(this);
      return;
    }
  }

  ComputerscareTextEditorSnapshot before = captureSnapshot();
  handlingTrackedInput = true;
  ui::TextField::onSelectKey(e);
  handlingTrackedInput = false;
  if (text != before.text) {
    pushUndoSnapshot(before);
    redoStack.clear();
  }
  lastSnapshot = captureSnapshot();
}

void ComputerscareTextEditor::onChange(const ChangeEvent& e) {
  if (!suppressChangeTracking && !handlingTrackedInput &&
      text != lastSnapshot.text) {
    pushUndoSnapshot(lastSnapshot);
    redoStack.clear();
  }
  if (state) {
    state->text = text;
  }
  lastSnapshot = captureSnapshot();
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

int ComputerscareTextEditor::getCursorLine() const {
  int line = 0;
  int cursorLimit = std::max(0, std::min(cursor, (int)text.size()));
  for (int i = 0; i < cursorLimit; i++) {
    if (text[i] == '\n') {
      line++;
    }
  }
  return line;
}

ComputerscareTextEditorSnapshot ComputerscareTextEditor::captureSnapshot()
    const {
  ComputerscareTextEditorSnapshot snapshot;
  snapshot.text = text;
  snapshot.cursor = cursor;
  snapshot.selection = selection;
  return snapshot;
}

void ComputerscareTextEditor::restoreSnapshot(
    const ComputerscareTextEditorSnapshot& snapshot) {
  suppressChangeTracking = true;
  text = snapshot.text;
  cursor = std::max(0, std::min(snapshot.cursor, (int)text.size()));
  selection = std::max(0, std::min(snapshot.selection, (int)text.size()));
  if (state) {
    state->text = text;
  }
  suppressChangeTracking = false;
  lastSnapshot = captureSnapshot();
}

void ComputerscareTextEditor::pushUndoSnapshot(
    const ComputerscareTextEditorSnapshot& snapshot) {
  if (!undoStack.empty() && undoStack.back().text == snapshot.text &&
      undoStack.back().cursor == snapshot.cursor &&
      undoStack.back().selection == snapshot.selection) {
    return;
  }
  undoStack.push_back(snapshot);
  if (undoStack.size() > maxUndoDepth) {
    undoStack.erase(undoStack.begin());
  }
}

void ComputerscareTextEditor::clearHistory() {
  undoStack.clear();
  redoStack.clear();
}

void ComputerscareTextEditor::undo() {
  if (undoStack.empty()) {
    return;
  }
  redoStack.push_back(captureSnapshot());
  ComputerscareTextEditorSnapshot snapshot = undoStack.back();
  undoStack.pop_back();
  restoreSnapshot(snapshot);
}

void ComputerscareTextEditor::redo() {
  if (redoStack.empty()) {
    return;
  }
  undoStack.push_back(captureSnapshot());
  ComputerscareTextEditorSnapshot snapshot = redoStack.back();
  redoStack.pop_back();
  restoreSnapshot(snapshot);
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
