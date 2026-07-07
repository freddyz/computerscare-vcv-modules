#include "ComputerscareTextEditorLogic.hpp"

#include <cstdio>
#include <cstdlib>

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    std::exit(1);
  }
}

} // namespace

int main() {
  namespace te = computerscare::text_editor;

  std::string twoLines = "abc\ndef";
  require(te::lineStartPosition(twoLines, 0) == 0,
          "first line starts at zero");
  require(te::lineStartPosition(twoLines, 1) == 4,
          "second line starts after newline");
  require(te::lineEndPosition(twoLines, 0) == 3,
          "first line ends at newline offset");
  require(te::lineEndPosition(twoLines, 1) == 7,
          "second line ends at text end");
  require(te::lineForOffset(twoLines, 3) == 0,
          "first line end offset maps to first line");
  require(te::moveOffsetRight(twoLines, 3) == 4,
          "right from first line end moves to second line start");
  require(te::moveOffsetLeft(twoLines, 5) == 4,
          "left from between first and second char moves to line start");

  std::string emptyMiddle = "abc\n\ndef";
  require(te::lineStartPosition(emptyMiddle, 1) == 4,
          "empty middle line starts after first newline");
  require(te::lineEndPosition(emptyMiddle, 1) == 4,
          "empty middle line has zero length selectable range");
  require(te::lineStartPosition(emptyMiddle, 2) == 5,
          "line after empty line starts after second newline");

  require(te::clampOffset(twoLines, -100) == 0,
          "negative offsets clamp to start");
  require(te::clampOffset(twoLines, 100) == (int)twoLines.size(),
          "large offsets clamp to end");

  std::string bracketedNumbers = "[1423 23]";
  te::SelectionRange range = te::wordRangeAtOffset(bracketedNumbers, 2);
  require(range.begin == 1 && range.end == 5,
          "double-clicking inside bracketed number selects number only");
  range = te::wordRangeAtOffset(bracketedNumbers, 5);
  require(range.begin == 1 && range.end == 5,
          "double-clicking just after a word selects previous word");
  range = te::wordRangeAtOffset(bracketedNumbers, 6);
  require(range.begin == 6 && range.end == 8,
          "double-clicking second bracketed number selects second number");
  range = te::wordRangeAtOffset(bracketedNumbers, 0);
  require(range.begin == 0 && range.end == 0,
          "double-clicking punctuation leaves empty word selection");

  std::string wordCharacters = "abc_123!";
  range = te::wordRangeAtOffset(wordCharacters, 4);
  require(range.begin == 0 && range.end == 7,
          "word selection includes letters numbers and underscores");

  std::string tempo = "135bpm";
  range = te::wordRangeAtOffset(tempo, 2);
  require(range.begin == 0 && range.end == 6,
          "double-clicking between digits in tempo selects whole tempo word");

  std::puts("text editor logic tests passed");
  return 0;
}
