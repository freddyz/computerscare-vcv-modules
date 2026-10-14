#pragma once

#include <string>
#include <vector>

#include "ComputerscareTextEditorTypes.hpp"

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

struct ComputerscareTextEditorLayout {
  ComputerscareTextEditorMetrics metrics;
  std::vector<ComputerscareTextEditorLayoutRow> rows;

  static ComputerscareTextEditorLayout build(
      NVGcontext* vg, const std::string& text, Vec boxSize,
      const ComputerscareTextEditorStyle& style);

  int hitTest(Vec mousePos) const;
  ComputerscareTextEditorCaret caretForOffset(const std::string& text,
                                              int offset) const;

 private:
  void addEmptyRow(int offset, int logicalLine, int visualRowIndex);
  void addTextRow(NVGcontext* vg, const std::string& rowText, int begin,
                  int end, int logicalLine, bool isLineStart, bool isLineEnd,
                  int visualRowIndex);
  void setRowGeometry(ComputerscareTextEditorLayoutRow& row,
                      int visualRowIndex) const;
  void buildCaretPositions(NVGcontext* vg, const std::string& rowText,
                           ComputerscareTextEditorLayoutRow& row) const;
};
