#pragma once

#include <algorithm>
#include <cctype>
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

struct SelectionRange {
  int begin = 0;
  int end = 0;
};

inline bool isWordCharacter(char c) {
  unsigned char value = static_cast<unsigned char>(c);
  return std::isalnum(value) || c == '_';
}

inline SelectionRange wordRangeAtOffset(const std::string& text, int offset) {
  SelectionRange range;
  if (text.empty()) {
    return range;
  }

  int position = clampOffset(text, offset);
  if (position == (int)text.size() || !isWordCharacter(text[position])) {
    if (position == 0 || !isWordCharacter(text[position - 1])) {
      range.begin = range.end = position;
      return range;
    }
    position--;
  }

  int begin = position;
  while (begin > 0 && isWordCharacter(text[begin - 1])) {
    begin--;
  }

  int end = position + 1;
  while (end < (int)text.size() && isWordCharacter(text[end])) {
    end++;
  }

  range.begin = begin;
  range.end = end;
  return range;
}

}  // namespace text_editor
}  // namespace computerscare
