#pragma once

#include <algorithm>
#include <string>

namespace computerscare {
namespace text_editor {

inline int clampOffset(const std::string& text, int offset) {
  return std::max(0, std::min(offset, (int)text.size()));
}

inline int lineForOffset(const std::string& text, int offset) {
  int line = 0;
  int cursorLimit = clampOffset(text, offset);
  for (int i = 0; i < cursorLimit; i++) {
    if (text[i] == '\n') {
      line++;
    }
  }
  return line;
}

inline int lineCount(const std::string& text) {
  return lineForOffset(text, (int)text.size()) + 1;
}

inline int lineStartPosition(const std::string& text, int line) {
  int targetLine = std::max(0, line);
  int currentLine = 0;
  int position = 0;

  while (position < (int)text.size() && currentLine < targetLine) {
    if (text[position] == '\n') {
      currentLine++;
    }
    position++;
  }

  return clampOffset(text, position);
}

inline int lineEndPosition(const std::string& text, int line) {
  int position = lineStartPosition(text, line);
  while (position < (int)text.size() && text[position] != '\n') {
    position++;
  }
  return position;
}

inline int moveOffsetLeft(const std::string& text, int offset) {
  return clampOffset(text, offset - 1);
}

inline int moveOffsetRight(const std::string& text, int offset) {
  return clampOffset(text, offset + 1);
}

}  // namespace text_editor
}  // namespace computerscare
