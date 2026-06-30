#include "ComputerscareTextEditor.hpp"

#include <algorithm>
#include <cmath>

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

ComputerscareTextEditorMetrics getEditorMetrics(
    NVGcontext* vg, Vec boxSize, const ComputerscareTextEditorStyle& style) {
  ComputerscareTextEditorMetrics metrics;
  metrics.visibleWidth = std::max(1.f, boxSize.x - 2.f * BND_TEXT_RADIUS);
  metrics.width = style.lineWrapping ? metrics.visibleWidth : 100000.f;
  nvgTextMetrics(vg, NULL, &metrics.desc, &metrics.lineHeight);
  return metrics;
}

float xForTextOffset(NVGcontext* vg, const char* label, NVGtextRow& row,
                     float rowX, float rowY, int offset, float defaultX) {
  int rowBegin = row.start - label;
  int rowEnd = row.end - label;
  int clampedOffset = std::max(rowBegin, std::min(offset, rowEnd));
  if (clampedOffset <= rowBegin) {
    return defaultX;
  }

  static NVGglyphPosition glyphs[BND_MAX_GLYPHS];
  int glyphCount = nvgTextGlyphPositions(vg, rowX, rowY, row.start, row.end + 1,
                                         glyphs, BND_MAX_GLYPHS);
  float x = defaultX;
  for (int i = 0; i < glyphCount; i++) {
    int glyphOffset = glyphs[i].str - label;
    x = glyphs[i].x;
    if (glyphOffset >= clampedOffset) {
      break;
    }
  }
  return x;
}

int textOffsetForMouseX(NVGcontext* vg, const char* label, NVGtextRow& row,
                        float rowX, float rowTop, float rowWidth,
                        float rowHeight, float fontSize, Vec mousePos) {
  int rowBegin = row.start - label;
  int rowEnd = row.end - label;
  if (rowEnd <= rowBegin) {
    return rowBegin;
  }

  std::string rowText(row.start, row.end);
  int rowOffset = bndIconLabelTextPosition(
      vg, rowX, rowTop, rowWidth, rowHeight, -1, fontSize, rowText.c_str(),
      (int)std::round(mousePos.x), (int)std::round(mousePos.y));
  rowOffset = std::max(0, std::min(rowOffset, rowEnd - rowBegin));
  return rowBegin + rowOffset;
}

int lineForTextOffset(const std::string& text, int offset) {
  int line = 0;
  int cursorLimit = std::max(0, std::min(offset, (int)text.size()));
  for (int i = 0; i < cursorLimit; i++) {
    if (text[i] == '\n') {
      line++;
    }
  }
  return line;
}

int rowForMouseY(const ComputerscareTextEditorMetrics& metrics, float mouseY,
                 int rowCount) {
  float rowFloat =
      (mouseY - metrics.baselineY + metrics.lineHeight + metrics.desc) /
      metrics.lineHeight;
  int row = (int)std::floor(rowFloat);
  return std::max(0, std::min(row, std::max(0, rowCount - 1)));
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
    ComputerscareTextEditorMetrics metrics =
        getEditorMetrics(vg, scaledBox, style);
    static NVGtextRow rows[BND_MAX_ROWS];
    int rowCount = nvgTextBreakLines(vg, text.c_str(), NULL, metrics.width,
                                     rows, BND_MAX_ROWS);
    int textPos = text.size();
    if (rowCount > 0) {
      int rowIndex = rowForMouseY(metrics, scaledMouse.y, rowCount);
      NVGtextRow& row = rows[rowIndex];
      const char* label = text.c_str();
      float rowY = metrics.baselineY + rowIndex * metrics.lineHeight;
      float rowTop = rowY - metrics.lineHeight - metrics.desc;
      textPos = textOffsetForMouseX(vg, label, row, metrics.textX, rowTop,
                                    metrics.visibleWidth, metrics.lineHeight,
                                    std::max(6.f, style.fontSize), scaledMouse);
    }
    nvgRestore(vg);
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

void ComputerscareTextEditor::setCursorLine(int line) {
  int targetLine = std::max(0, line);
  int currentLine = 0;
  int position = 0;

  while (position < (int)text.size() && currentLine < targetLine) {
    if (text[position] == '\n') {
      currentLine++;
    }
    position++;
  }

  cursor = std::max(0, std::min(position, (int)text.size()));
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
  int targetLine = std::max(0, line);
  int currentLine = 0;
  int position = 0;

  while (position < (int)text.size() && currentLine < targetLine) {
    if (text[position] == '\n') {
      currentLine++;
    }
    position++;
  }

  return std::max(0, std::min(position, (int)text.size()));
}

int ComputerscareTextEditor::getLineEndPosition(int line) const {
  int position = getLineStartPosition(line);
  while (position < (int)text.size() && text[position] != '\n') {
    position++;
  }
  return position;
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
    ComputerscareTextEditorMetrics metrics =
        getEditorMetrics(args.vg, scaledBox, style);

    const char* label = text.c_str();
    static NVGtextRow rows[BND_MAX_ROWS];
    int rowCount = nvgTextBreakLines(args.vg, label, NULL, metrics.width, rows,
                                     BND_MAX_ROWS);
    bool hasSelection =
        this == APP->event->selectedWidget && cursor != selection;
    int selectionBegin = hasSelection ? std::min(cursor, selection) : 0;
    int selectionEnd = hasSelection ? std::max(cursor, selection) : 0;

    if (hasSelection) {
      for (int rowIndex = 0; rowIndex < rowCount; rowIndex++) {
        NVGtextRow& row = rows[rowIndex];
        int rowBegin = row.start - label;
        int rowEnd = row.end - label;
        int spanBegin = std::max(selectionBegin, rowBegin);
        int spanEnd = std::min(selectionEnd, rowEnd);
        if (spanBegin >= spanEnd) {
          continue;
        }
        float rowY = metrics.baselineY + rowIndex * metrics.lineHeight;
        float startX = xForTextOffset(args.vg, label, row, metrics.textX, rowY,
                                      spanBegin, metrics.textX + row.minx);
        float endX = xForTextOffset(args.vg, label, row, metrics.textX, rowY,
                                    spanEnd, metrics.textX + row.maxx);
        float topY = rowY - metrics.lineHeight - metrics.desc;
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, startX - 1.f, topY, endX - startX + 2.f,
                       metrics.lineHeight + 1.f, 2.f);
        nvgFillColor(args.vg, style.selectionColor);
        nvgFill(args.vg);
      }
    }

    if (!text.empty()) {
      nvgFillColor(args.vg, style.textColor);
      for (int rowIndex = 0; rowIndex < rowCount; rowIndex++) {
        float rowY = metrics.baselineY + rowIndex * metrics.lineHeight;
        nvgText(args.vg, metrics.textX, rowY, rows[rowIndex].start,
                rows[rowIndex].end);
      }
    } else if (!placeholder.empty()) {
      nvgFillColor(args.vg, style.placeholderColor);
      nvgText(args.vg, metrics.textX, metrics.baselineY, placeholder.c_str(),
              NULL);
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

  ComputerscareTextEditorMetrics metrics =
      getEditorMetrics(args.vg, getScaledBoxSize(), style);
  float cursorX = metrics.textX;
  int cursorOffset = std::max(0, std::min(cursor, (int)text.size()));
  int cursorLine = lineForTextOffset(text, cursorOffset);
  float cursorY = metrics.baselineY + cursorLine * metrics.lineHeight -
                  metrics.lineHeight - metrics.desc + 2.f;

  if (!text.empty()) {
    static NVGtextRow rows[BND_MAX_ROWS];
    int rowCount = nvgTextBreakLines(args.vg, text.c_str(), NULL, metrics.width,
                                     rows, BND_MAX_ROWS);
    const char* label = text.c_str();
    for (int rowIndex = 0; rowIndex < rowCount; rowIndex++) {
      NVGtextRow& row = rows[rowIndex];
      int rowBegin = row.start - label;
      int rowEnd = row.end - label;
      if (cursorOffset >= rowBegin && cursorOffset <= rowEnd) {
        float rowY = metrics.baselineY + rowIndex * metrics.lineHeight;
        cursorX = xForTextOffset(args.vg, label, row, metrics.textX, rowY,
                                 cursorOffset, metrics.textX + row.maxx);
        cursorY = rowY - metrics.lineHeight - metrics.desc + 2.f;
        break;
      }
    }
  }

  float blink = 0.5f + 0.5f * std::sin((float)rack::system::getTime() * 6.f);
  NVGcolor color = COLOR_COMPUTERSCARE_BLUE;
  color.r = color.r + (1.f - color.r) * blink * 0.48f;
  color.g = color.g + (1.f - color.g) * blink * 0.48f;
  color.b = color.b + (1.f - color.b) * blink * 0.48f;
  color.a = 0.72f + 0.28f * blink;
  nvgBeginPath(args.vg);
  nvgRect(args.vg, cursorX, cursorY - 1.f, 1.4f, metrics.lineHeight - 1.f);
  nvgFillColor(args.vg, color);
  nvgFill(args.vg);

  nvgRestore(args.vg);
  bndSetFont(APP->window->uiFont->handle);
}

void ComputerscareTextEditor::drawHighlightSpan(
    const DrawArgs& args, const ComputerscareTextHighlight& highlight,
    int mode) {
  if (highlight.end <= highlight.begin) {
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

  ComputerscareTextEditorMetrics metrics =
      getEditorMetrics(args.vg, getScaledBoxSize(), style);
  static NVGtextRow rows[BND_MAX_ROWS];
  int rowCount = nvgTextBreakLines(args.vg, text.c_str(), NULL, metrics.width,
                                   rows, BND_MAX_ROWS);
  const char* label = text.c_str();

  if (highlight.fullLine && (text.empty() || begin == end)) {
    int line = lineForTextOffset(text, begin);
    float topY = metrics.baselineY + line * metrics.lineHeight -
                 metrics.lineHeight - metrics.desc;
    if (mode == HIGHLIGHT_BACKGROUND) {
      nvgBeginPath(args.vg);
      nvgRoundedRect(args.vg, metrics.textX - 2.f, topY,
                     metrics.visibleWidth + 4.f, metrics.lineHeight + 1.f, 2.f);
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

    for (int rowIndex = 0; rowIndex < rowCount; rowIndex++) {
      NVGtextRow& row = rows[rowIndex];
      int rowBegin = row.start - label;
      int rowEnd = row.end - label;
      if (rowEnd <= begin || rowBegin >= end) {
        continue;
      }

      float rowY = metrics.baselineY + rowIndex * metrics.lineHeight;
      float rowTop = rowY - metrics.lineHeight - metrics.desc;
      float rowBottom = rowTop + metrics.lineHeight + 1.f;
      if (!foundRow) {
        topY = rowTop;
        bottomY = rowBottom;
        foundRow = true;
      } else {
        topY = std::min(topY, rowTop);
        bottomY = std::max(bottomY, rowBottom);
      }
    }

    if (foundRow) {
      float startX = metrics.textX - 2.f;
      float endX = metrics.textX + metrics.visibleWidth + 2.f;
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

    if (highlight.fullLine) {
      startX = metrics.textX - 2.f;
      endX = metrics.textX + metrics.visibleWidth + 2.f;
    }

    if (mode == HIGHLIGHT_FOREGROUND) {
      std::string spanText = text.substr(spanBegin, spanEnd - spanBegin);
      nvgFillColor(args.vg, highlight.foreground);
      nvgText(args.vg, startX, rowY, spanText.c_str(), NULL);
    } else if (mode == HIGHLIGHT_BACKGROUND) {
      nvgBeginPath(args.vg);
      nvgRoundedRect(args.vg, startX - 1.f, topY, endX - startX + 2.f,
                     metrics.lineHeight + 1.f, 2.f);
      nvgFillColor(args.vg, highlight.background);
      nvgFill(args.vg);
    } else if (mode == HIGHLIGHT_BORDER) {
      nvgBeginPath(args.vg);
      nvgRoundedRect(args.vg, startX - 1.5f, topY + 0.5f, endX - startX + 3.f,
                     metrics.lineHeight, 2.f);
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
          nvgRect(args.vg, startX, topY + metrics.lineHeight - 2.f, width, 2.f);
          nvgFillColor(args.vg, highlight.progressColor);
          nvgFill(args.vg);
        }
      } else {
        float width = (endX - startX) * progress;
        if (width <= 0.f) {
          continue;
        }
        nvgBeginPath(args.vg);
        nvgRect(args.vg, startX, topY + metrics.lineHeight - 2.f, width, 2.f);
        nvgFillColor(args.vg, highlight.progressColor);
        nvgFill(args.vg);
      }
    }
  }

  nvgRestore(args.vg);
  bndSetFont(APP->window->uiFont->handle);
}
