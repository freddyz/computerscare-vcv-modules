#include "ComputerscareTextEditorLayout.hpp"

#include <algorithm>
#include <cmath>

#include "ComputerscareTextEditorLogic.hpp"

namespace {
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
}  // namespace

ComputerscareTextEditorLayout ComputerscareTextEditorLayout::build(
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
      for (int textRowIndex = 0; textRowIndex < textRowCount; textRowIndex++) {
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

void ComputerscareTextEditorLayout::addEmptyRow(int offset, int logicalLine,
                                                int visualRowIndex) {
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

void ComputerscareTextEditorLayout::addTextRow(
    NVGcontext* vg, const std::string& rowText, int begin, int end,
    int logicalLine, bool isLineStart, bool isLineEnd, int visualRowIndex) {
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

void ComputerscareTextEditorLayout::setRowGeometry(
    ComputerscareTextEditorLayoutRow& row, int visualRowIndex) const {
  row.x = metrics.textX;
  row.y = metrics.baselineY + visualRowIndex * metrics.lineHeight;
  row.top = row.y - metrics.lineHeight - metrics.desc;
  row.bottom = row.top + metrics.lineHeight + 1.f;
}

void ComputerscareTextEditorLayout::buildCaretPositions(
    NVGcontext* vg, const std::string& rowText,
    ComputerscareTextEditorLayoutRow& row) const {
  int rowLength = std::max(0, row.end - row.begin);
  row.carets.assign(rowLength + 1, row.x);
  if (rowLength <= 0) {
    return;
  }

  std::vector<bool> hasCaret(rowLength + 1, false);
  row.carets[0] = row.x;
  hasCaret[0] = true;

  static NVGglyphPosition glyphs[BND_MAX_GLYPHS];
  int glyphCount = nvgTextGlyphPositions(vg, row.x, row.y, rowText.c_str(),
                                         NULL, glyphs, BND_MAX_GLYPHS);
  for (int i = 0; i < glyphCount; i++) {
    int localOffset = glyphs[i].str - rowText.c_str();
    if (localOffset >= 0 && localOffset <= rowLength) {
      row.carets[localOffset] = glyphs[i].x;
      hasCaret[localOffset] = true;
    }
  }

  float measuredWidth =
      nvgTextBounds(vg, row.x, row.y, rowText.c_str(), NULL, NULL);
  row.carets[rowLength] = row.x + measuredWidth;
  hasCaret[rowLength] = true;

  int previousSet = 0;
  for (int i = 1; i <= rowLength; i++) {
    if (!hasCaret[i]) {
      continue;
    }
    float previousX = row.carets[previousSet];
    float currentX = std::max(previousX, row.carets[i]);
    row.carets[i] = currentX;
    int span = i - previousSet;
    for (int j = previousSet + 1; j < i; j++) {
      float t = (float)(j - previousSet) / (float)span;
      row.carets[j] = previousX + (currentX - previousX) * t;
    }
    previousSet = i;
  }
}

int ComputerscareTextEditorLayout::hitTest(Vec mousePos) const {
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

ComputerscareTextEditorCaret ComputerscareTextEditorLayout::caretForOffset(
    const std::string& text, int offset) const {
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
  caret.x = bestRow->carets.empty() ? bestRow->x : bestRow->carets[localOffset];
  caret.y = bestRow->y;
  caret.top = bestRow->top + 1.f;
  caret.bottom = bestRow->bottom - 1.f;
  return caret;
}
