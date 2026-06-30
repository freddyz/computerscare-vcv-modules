#include "ComputerscareTextEditor.hpp"

#include <algorithm>
#include <cmath>

#include "ComputerscareTextEditorLogic.hpp"

namespace {
enum HighlightDrawMode {
  HIGHLIGHT_BACKGROUND,
  HIGHLIGHT_FOREGROUND,
  HIGHLIGHT_BORDER,
  HIGHLIGHT_PROGRESS
};

struct ComputerscareTextEditorMetrics {
  float textX = BND_TEXT_RADIUS;
  float baselineY = BND_WIDGET_HEIGHT - BND_TEXT_PAD_DOWN;
  float width = 0.f;
  float visibleWidth = 0.f;
  float desc = 0.f;
  float lineHeight = BND_WIDGET_HEIGHT;
};

struct ComputerscareTextEditorLayoutRow {
  int begin = 0;
  int end = 0;
  int lineIndex = 0;
  bool isLineStart = false;
  bool isLineEnd = false;
  float x = 0.f;
  float y = 0.f;
  float top = 0.f;
  float bottom = 0.f;
  std::vector<float> carets;
};

struct ComputerscareTextEditorCaret {
  float x = 0.f;
  float y = 0.f;
  float top = 0.f;
  float bottom = 0.f;
};

ComputerscareTextEditorMetrics getEditorMetrics(
    NVGcontext* vg, Vec boxSize, const ComputerscareTextEditorStyle& style) {
  ComputerscareTextEditorMetrics metrics;
  metrics.visibleWidth = std::max(1.f, boxSize.x - 2.f * BND_TEXT_RADIUS);
  metrics.width = style.lineWrapping ? metrics.visibleWidth : 100000.f;
  nvgTextMetrics(vg, NULL, &metrics.desc, &metrics.lineHeight);
  return metrics;
}

int rowForMouseY(const ComputerscareTextEditorMetrics& metrics, float mouseY,
                 int rowCount) {
  float rowFloat =
      (mouseY - metrics.baselineY + metrics.lineHeight + metrics.desc) /
      metrics.lineHeight;
  int row = (int)std::floor(rowFloat);
  return std::max(0, std::min(row, std::max(0, rowCount - 1)));
}

struct ComputerscareTextEditorLayout {
  ComputerscareTextEditorMetrics metrics;
  std::vector<ComputerscareTextEditorLayoutRow> rows;

  static ComputerscareTextEditorLayout build(
      NVGcontext* vg, const std::string& text, Vec boxSize,
      const ComputerscareTextEditorStyle& style) {
    ComputerscareTextEditorLayout layout;
    layout.metrics = getEditorMetrics(vg, boxSize, style);
    int visualRowIndex = 0;
    int logicalLine = 0;
    int lineStart = 0;

    while (lineStart <= (int)text.size()) {
      int lineEnd = lineStart;
      while (lineEnd < (int)text.size() && text[lineEnd] != '\n') {
        lineEnd++;
      }

      std::string lineText = text.substr(lineStart, lineEnd - lineStart);
      if (lineText.empty()) {
        layout.addEmptyRow(lineStart, logicalLine, visualRowIndex++);
      } else {
        static NVGtextRow textRows[BND_MAX_ROWS];
        int textRowCount =
            nvgTextBreakLines(vg, lineText.c_str(), NULL, layout.metrics.width,
                              textRows, BND_MAX_ROWS);
        for (int textRowIndex = 0; textRowIndex < textRowCount;
             textRowIndex++) {
          NVGtextRow& textRow = textRows[textRowIndex];
          int rowBegin = lineStart + (textRow.start - lineText.c_str());
          int rowEnd = lineStart + (textRow.end - lineText.c_str());
          bool isLineStart = rowBegin == lineStart;
          bool isLineEnd = rowEnd == lineEnd;
          std::string rowText(textRow.start, textRow.end);
          layout.addTextRow(vg, rowText, rowBegin, rowEnd, logicalLine,
                            isLineStart, isLineEnd, visualRowIndex++);
        }
      }

      if (lineEnd >= (int)text.size()) {
        break;
      }
      lineStart = lineEnd + 1;
      logicalLine++;
    }

    if (layout.rows.empty()) {
      layout.addEmptyRow(0, 0, 0);
    }
    return layout;
  }

  void addEmptyRow(int offset, int logicalLine, int visualRowIndex) {
    ComputerscareTextEditorLayoutRow row;
    row.begin = offset;
    row.end = offset;
    row.lineIndex = logicalLine;
    row.isLineStart = true;
    row.isLineEnd = true;
    setRowGeometry(row, visualRowIndex);
    row.carets.push_back(metrics.textX);
    rows.push_back(row);
  }

  void addTextRow(NVGcontext* vg, const std::string& rowText, int begin,
                  int end, int logicalLine, bool isLineStart, bool isLineEnd,
                  int visualRowIndex) {
    ComputerscareTextEditorLayoutRow row;
    row.begin = begin;
    row.end = end;
    row.lineIndex = logicalLine;
    row.isLineStart = isLineStart;
    row.isLineEnd = isLineEnd;
    setRowGeometry(row, visualRowIndex);
    buildCaretPositions(vg, rowText, row);
    rows.push_back(row);
  }

  void setRowGeometry(ComputerscareTextEditorLayoutRow& row,
                      int visualRowIndex) const {
    row.x = metrics.textX;
    row.y = metrics.baselineY + visualRowIndex * metrics.lineHeight;
    row.top = row.y - metrics.lineHeight - metrics.desc;
    row.bottom = row.top + metrics.lineHeight + 1.f;
  }

  void buildCaretPositions(NVGcontext* vg, const std::string& rowText,
                           ComputerscareTextEditorLayoutRow& row) const {
    int rowLength = std::max(0, row.end - row.begin);
    row.carets.assign(rowLength + 1, row.x);
    if (rowLength <= 0) {
      return;
    }

    static NVGglyphPosition glyphs[BND_MAX_GLYPHS];
    int glyphCount = nvgTextGlyphPositions(vg, row.x, row.y, rowText.c_str(),
                                           NULL, glyphs, BND_MAX_GLYPHS);
    for (int i = 0; i < glyphCount; i++) {
      int localOffset = glyphs[i].str - rowText.c_str();
      if (localOffset >= 0 && localOffset <= rowLength) {
        row.carets[localOffset] = glyphs[i].x;
      }
      if (localOffset + 1 >= 0 && localOffset + 1 <= rowLength) {
        row.carets[localOffset + 1] = glyphs[i].maxx;
      }
    }

    float measuredWidth =
        nvgTextBounds(vg, row.x, row.y, rowText.c_str(), NULL, NULL);
    row.carets[rowLength] =
        std::max(row.carets[rowLength], row.x + measuredWidth);
  }

  int hitTest(Vec mousePos) const {
    if (rows.empty()) {
      return 0;
    }

    int rowIndex = rowForMouseY(metrics, mousePos.y, rows.size());
    const ComputerscareTextEditorLayoutRow& row = rows[rowIndex];
    if (mousePos.x <= row.x || row.carets.size() <= 1) {
      return row.begin;
    }

    int rowLength = row.end - row.begin;
    for (int i = 0; i < rowLength; i++) {
      float midpoint = 0.5f * (row.carets[i] + row.carets[i + 1]);
      if (mousePos.x < midpoint) {
        return row.begin + i;
      }
    }
    return row.end;
  }

  ComputerscareTextEditorCaret caretForOffset(const std::string& text,
                                              int offset) const {
    ComputerscareTextEditorCaret caret;
    int textSize = text.size();
    int clampedOffset = computerscare::text_editor::clampOffset(text, offset);

    const ComputerscareTextEditorLayoutRow* bestRow = nullptr;
    for (size_t i = 0; i < rows.size(); i++) {
      const ComputerscareTextEditorLayoutRow& row = rows[i];
      if (clampedOffset == row.begin && row.isLineStart) {
        bestRow = &row;
        break;
      }
      if (clampedOffset > row.begin && clampedOffset < row.end) {
        bestRow = &row;
        break;
      }
      if (clampedOffset == row.end && row.isLineEnd) {
        bestRow = &row;
        break;
      }
      if (clampedOffset == textSize && clampedOffset == row.end) {
        bestRow = &row;
        break;
      }
    }

    if (!bestRow) {
      bestRow = rows.empty() ? nullptr : &rows.front();
    }
    if (!bestRow) {
      caret.x = metrics.textX;
      caret.y = metrics.baselineY;
      caret.top = metrics.baselineY - metrics.lineHeight - metrics.desc + 2.f;
      caret.bottom = caret.top + metrics.lineHeight - 1.f;
      return caret;
    }

    int localOffset = std::max(0, std::min(clampedOffset - bestRow->begin,
                                           (int)bestRow->carets.size() - 1));
    caret.x =
        bestRow->carets.empty() ? bestRow->x : bestRow->carets[localOffset];
    caret.y = bestRow->y;
    caret.top = bestRow->top + 1.f;
    caret.bottom = bestRow->bottom - 1.f;
    return caret;
  }
};
}  // namespace

ComputerscareTextEditor::ComputerscareTextEditor() {
  multiline = true;
  placeholder = "";
  lastSnapshot = captureSnapshot();
}

void ComputerscareTextEditor::setState(
    ComputerscareTextEditorState* editorState) {
  state = editorState;
  syncFromState();
}

void ComputerscareTextEditor::syncFromState() {
  if (!state) {
    return;
  }

  suppressChangeTracking = true;
  setText(state->text);
  suppressChangeTracking = false;
  state->dirty = false;
  clearHistory();
  lastSnapshot = captureSnapshot();
}

void ComputerscareTextEditor::step() {
  ui::TextField::step();
  if (state && state->dirty) {
    syncFromState();
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
    drawHighlightDecorations(args);
    drawCursor(args);
  }
  Widget::drawLayer(args, layer);
}

void ComputerscareTextEditor::onEnter(const EnterEvent& e) {
  ui::TextField::onEnter(e);
  static GLFWcursor* editorCursor = nullptr;
  if (!editorCursor) {
    editorCursor = glfwCreateStandardCursor(GLFW_IBEAM_CURSOR);
  }
  glfwSetCursor(APP->window->win, editorCursor);
}

void ComputerscareTextEditor::onLeave(const LeaveEvent& e) {
  ui::TextField::onLeave(e);
  glfwSetCursor(APP->window->win, nullptr);
}

void ComputerscareTextEditor::onButton(const ButtonEvent& e) {
  ui::TextField::onButton(e);
  if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
    APP->event->setSelectedWidget(this);
    cursor = selection =
        std::max(0, std::min(getTextPosition(e.pos), (int)text.size()));
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
    bool isEnter = e.key == GLFW_KEY_ENTER || e.key == GLFW_KEY_KP_ENTER;
    if (submitOnEnter && isEnter && mods == RACK_MOD_CTRL) {
      if (state && e.action == GLFW_PRESS) {
        state->submitCount++;
      }
      e.consume(this);
      return;
    }
    if (e.key == GLFW_KEY_ESCAPE) {
      if (state && e.action == GLFW_PRESS) {
        state->cancelCount++;
      }
      e.consume(this);
      return;
    }
    if (e.key == GLFW_KEY_TAB) {
      if (state && e.action == GLFW_PRESS) {
        state->switchViewCount++;
      }
      e.consume(this);
      return;
    }
    if (e.key == GLFW_KEY_UP) {
      moveCursorToAdjacentLogicalLine(-1);
      e.consume(this);
      return;
    }
    if (e.key == GLFW_KEY_DOWN) {
      moveCursorToAdjacentLogicalLine(1);
      e.consume(this);
      return;
    }
    if ((e.key == GLFW_KEY_LEFT || e.key == GLFW_KEY_RIGHT) &&
        (e.mods & (GLFW_MOD_CONTROL | GLFW_MOD_SUPER | GLFW_MOD_ALT)) == 0) {
      int nextCursor =
          e.key == GLFW_KEY_LEFT
              ? computerscare::text_editor::moveOffsetLeft(text, cursor)
              : computerscare::text_editor::moveOffsetRight(text, cursor);
      cursor = nextCursor;
      if ((e.mods & GLFW_MOD_SHIFT) == 0) {
        selection = cursor;
      }
      lastSnapshot = captureSnapshot();
      e.consume(this);
      return;
    }
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
    NVGcontext* vg = APP->window->vg;
    nvgSave(vg);
    nvgResetTransform(vg);
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

int ComputerscareTextEditor::getCursorLine() const {
  return computerscare::text_editor::lineForOffset(text, cursor);
}

void ComputerscareTextEditor::setCursorLine(int line) {
  cursor = computerscare::text_editor::lineStartPosition(text, line);
  selection = cursor;
  lastSnapshot = captureSnapshot();
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

int ComputerscareTextEditor::getLineStartPosition(int line) const {
  return computerscare::text_editor::lineStartPosition(text, line);
}

int ComputerscareTextEditor::getLineEndPosition(int line) const {
  return computerscare::text_editor::lineEndPosition(text, line);
}

int ComputerscareTextEditor::getCursorColumn() const {
  int line = getCursorLine();
  int lineStart = getLineStartPosition(line);
  int cursorPosition = std::max(0, std::min(cursor, (int)text.size()));
  return std::max(0, cursorPosition - lineStart);
}

void ComputerscareTextEditor::moveCursorToAdjacentLogicalLine(int direction) {
  int currentLine = getCursorLine();
  int targetLine = std::max(0, currentLine + direction);
  int targetStart = getLineStartPosition(targetLine);
  int targetEnd = getLineEndPosition(targetLine);
  int column = getCursorColumn();

  cursor = std::max(targetStart, std::min(targetStart + column, targetEnd));
  selection = cursor;
  lastSnapshot = captureSnapshot();
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
    nvgSave(args.vg);
    nvgScale(args.vg, getFontScaleX(), getFontScaleY());
    applyTextStyle(args.vg, font);
    Vec scaledBox = getScaledBoxSize();
    ComputerscareTextEditorLayout layout =
        ComputerscareTextEditorLayout::build(args.vg, text, scaledBox, style);
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
  nvgSave(args.vg);
  nvgScale(args.vg, getFontScaleX(), getFontScaleY());
  applyTextStyle(args.vg, font);

  int cursorOffset = std::max(0, std::min(cursor, (int)text.size()));
  ComputerscareTextEditorLayout layout = ComputerscareTextEditorLayout::build(
      args.vg, text, getScaledBoxSize(), style);
  ComputerscareTextEditorCaret caret =
      layout.caretForOffset(text, cursorOffset);

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
  nvgScale(args.vg, getFontScaleX(), getFontScaleY());
  applyTextStyle(args.vg, font);

  ComputerscareTextEditorLayout layout = ComputerscareTextEditorLayout::build(
      args.vg, text, getScaledBoxSize(), style);

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
