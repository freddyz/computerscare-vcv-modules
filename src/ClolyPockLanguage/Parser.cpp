#include "Parser.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace cloly {
namespace language {

namespace {
std::string lowerCopy(const std::string& value) {
  std::string lowered = value;
  for (size_t i = 0; i < lowered.size(); i++) {
    lowered[i] =
        static_cast<char>(std::tolower(static_cast<unsigned char>(lowered[i])));
  }
  return lowered;
}

bool isIntegerLexeme(const std::string& value) {
  if (value.empty()) {
    return false;
  }
  for (size_t i = 0; i < value.size(); i++) {
    if (std::isdigit(static_cast<unsigned char>(value[i])) == 0) {
      return false;
    }
  }
  return true;
}

double parseDouble(const std::string& value) {
  return std::strtod(value.c_str(), NULL);
}

int parseInt(const std::string& value) { return std::atoi(value.c_str()); }

SourceRange rangeFromToken(const Token& token) {
  SourceRange range;
  range.begin = token.begin;
  range.end = token.end;
  return range;
}

void addDiagnostic(ParseResult& result, const std::string& message,
                   SourceRange range) {
  Diagnostic diagnostic;
  diagnostic.message = message;
  diagnostic.range = range;
  result.diagnostics.push_back(diagnostic);
}

bool isDurationUnit(ClockUnit unit) {
  return unit == ClockUnit::Milliseconds || unit == ClockUnit::Seconds ||
         unit == ClockUnit::Minutes;
}

bool hasExplicitUnit(const ClockLiteralAst& ast) {
  return ast.unitRange.end > ast.unitRange.begin;
}

void applyUnitToUnitlessBlock(ClockBlockAst& block, ClockUnit unit,
                              SourceRange unitRange) {
  if (!hasExplicitUnit(block.literal)) {
    block.literal.unit = unit;
    block.literal.unitRange = unitRange;
  }
}

std::vector<ClockBlockAst> interleaveBlocks(
    const std::vector<ClockBlockAst>& left,
    const std::vector<ClockBlockAst>& right) {
  std::vector<std::vector<ClockBlockAst>> lanes;
  lanes.push_back(left);
  lanes.push_back(right);

  std::vector<ClockBlockAst> output;
  std::vector<size_t> indices(lanes.size(), 0);
  size_t laneIndex = 0;
  int steps = 0;
  bool allAtStart = false;

  while (!lanes.empty() && ((!allAtStart && steps < 6000) || steps == 0)) {
    if (!lanes[laneIndex].empty()) {
      output.push_back(lanes[laneIndex][indices[laneIndex]]);
      indices[laneIndex] = (indices[laneIndex] + 1) % lanes[laneIndex].size();
    }
    laneIndex = (laneIndex + 1) % lanes.size();
    steps++;
    allAtStart = laneIndex == 0;
    for (size_t i = 0; i < lanes.size(); i++) {
      allAtStart = allAtStart && indices[i] == 0;
    }
  }

  return output;
}

std::vector<ClockBlockAst> spreadInterleaveLanes(
    const std::vector<std::vector<ClockBlockAst>>& lanes) {
  std::vector<ClockBlockAst> output;
  size_t rowCount = 0;
  for (size_t i = 0; i < lanes.size(); i++) {
    rowCount = std::max(rowCount, lanes[i].size());
  }

  for (size_t row = 0; row < rowCount && row < 6000; row++) {
    for (size_t laneIndex = 0; laneIndex < lanes.size(); laneIndex++) {
      if (lanes[laneIndex].empty()) {
        continue;
      }
      output.push_back(lanes[laneIndex][row % lanes[laneIndex].size()]);
    }
  }

  return output;
}

class Parser {
 public:
  explicit Parser(const std::vector<Token>& tokens) : tokens(tokens) {}

  ParseResult parse() {
    ParseResult result;

    if (peek().type == TokenType::End) {
      addDiagnostic(result, "Expected clock program", rangeFromToken(peek()));
      return result;
    }

    result.program.range.begin = peek().begin;
    while (!isAtEnd()) {
      if (matchSeparator()) {
        continue;
      }
      parseBlock(result, result.program.blocks);
    }

    if (!result.program.blocks.empty()) {
      result.program.range.end = result.program.blocks.back().range.end;
      result.ast = result.program.blocks.front().literal;
    }
    return result;
  }

 private:
  const std::vector<Token>& tokens;
  size_t current = 0;
  int nextTotalDurationGroupId = 0;

  const Token& peek() const { return tokens[current]; }

  const Token& previous() const { return tokens[current - 1]; }

  bool isAtEnd() const { return peek().type == TokenType::End; }

  const Token& advance() {
    if (!isAtEnd()) {
      current++;
    }
    return previous();
  }

  bool matchSeparator() {
    if (peek().type == TokenType::Comma) {
      advance();
      return true;
    }
    return false;
  }

  void parseBlock(ParseResult& result, std::vector<ClockBlockAst>& blocks) {
    if (peek().type == TokenType::Invalid) {
      addDiagnostic(result, "Invalid token", rangeFromToken(peek()));
      advance();
      return;
    }

    if (peek().type == TokenType::Tilde) {
      parseRestBlock(result, blocks);
      return;
    }

    if (peek().type == TokenType::RightBracket) {
      addDiagnostic(result, "Unexpected ']'", rangeFromToken(peek()));
      advance();
      return;
    }

    if (peek().type == TokenType::RightParen) {
      addDiagnostic(result, "Unexpected ')'", rangeFromToken(peek()));
      advance();
      return;
    }

    if (peek().type == TokenType::LeftBracket) {
      parseBracketBlock(result, blocks);
      return;
    }

    if (peek().type == TokenType::LeftParen) {
      parseInterleaveBlock(result, blocks);
      return;
    }

    if (peek().type != TokenType::Number &&
        peek().type != TokenType::LeftBrace) {
      addDiagnostic(result, "Expected clock literal", rangeFromToken(peek()));
      advance();
      return;
    }

    ClockBlockAst block;
    block.literal = parseLiteral(result);
    block.range = block.literal.range;
    parseProbability(result, block);
    parseRepeat(result, block);
    parseTotalDuration(result, block);
    blocks.push_back(block);
  }

  void parseRestBlock(ParseResult& result, std::vector<ClockBlockAst>& blocks) {
    Token restToken = advance();
    SourceRange restRange = rangeFromToken(restToken);
    if (peek().type == TokenType::At) {
      ClockBlockAst block;
      block.rest = true;
      block.restRange = restRange;
      block.range.begin = restToken.begin;
      block.literal.kind = ClockLiteralKind::Empty;
      block.literal.range = restRange;
      parseRepeat(result, block);
      if (block.repeatRange.end <= block.repeatRange.begin) {
        addDiagnostic(result, "Expected rest duration after '~'",
                      rangeFromToken(restToken));
        block.range.end = restToken.end;
      } else {
        block.range.end = block.repeatRange.end;
      }
      if (!block.repeatIsDuration) {
        addDiagnostic(result, "Rest duration requires a time unit",
                      block.repeatValueRange);
      }
      blocks.push_back(block);
      return;
    }

    size_t blockBegin = blocks.size();
    parseBlock(result, blocks);
    if (blocks.size() == blockBegin) {
      addDiagnostic(result, "Expected rest target after '~'", restRange);
      return;
    }

    for (size_t i = blockBegin; i < blocks.size(); i++) {
      blocks[i].rest = true;
      blocks[i].restRange = restRange;
      if (i == blockBegin) {
        blocks[i].range.begin = restToken.begin;
      }
      if (blocks[i].repeatRange.end > blocks[i].range.end) {
        blocks[i].range.end = blocks[i].repeatRange.end;
      }
    }
  }

  void parseSequenceUntil(ParseResult& result,
                          std::vector<ClockBlockAst>& blocks,
                          TokenType terminator) {
    while (!isAtEnd() && peek().type != terminator) {
      if (matchSeparator()) {
        continue;
      }
      parseBlock(result, blocks);
    }
  }

  void parseBracketBlock(ParseResult& result,
                         std::vector<ClockBlockAst>& blocks) {
    Token leftBracket = advance();
    std::vector<ClockBlockAst> groupBlocks;

    parseSequenceUntil(result, groupBlocks, TokenType::RightBracket);

    Token rightBracket;
    if (peek().type == TokenType::RightBracket) {
      rightBracket = advance();
    } else {
      addDiagnostic(result, "Expected ']'", rangeFromToken(leftBracket));
      rightBracket = leftBracket;
    }

    ClockBlockAst repeatBlock;
    repeatBlock.range.begin = leftBracket.begin;
    repeatBlock.range.end = rightBracket.end;
    SourceRange suffixUnitRange;
    ClockUnit suffixUnit = parseGroupUnitSuffix(result, suffixUnitRange);
    parseRepeat(result, repeatBlock);
    ClockBlockAst totalDurationBlock;
    parseTotalDuration(result, totalDurationBlock);

    if (groupBlocks.empty()) {
      addDiagnostic(result, "Expected clock literal in brackets",
                    rangeFromToken(leftBracket));
      return;
    }

    for (size_t i = 0; i < groupBlocks.size(); i++) {
      if (suffixUnit != ClockUnit::Unknown) {
        applyUnitToUnitlessBlock(groupBlocks[i], suffixUnit, suffixUnitRange);
      }
      if (repeatBlock.repeatIsDuration) {
        groupBlocks[i].repeatIsDuration = true;
        groupBlocks[i].repeatDuration = repeatBlock.repeatDuration;
      } else {
        groupBlocks[i].repeat *= repeatBlock.repeat;
      }
      if (repeatBlock.repeatRange.end > repeatBlock.repeatRange.begin) {
        groupBlocks[i].repeatRange = repeatBlock.repeatRange;
        if (!groupBlocks[i].repeatValueIsOwn) {
          groupBlocks[i].repeatValueRange = repeatBlock.repeatValueRange;
        }
      }
    }

    applyTotalDurationGroup(groupBlocks, totalDurationBlock);
    for (size_t i = 0; i < groupBlocks.size(); i++) {
      blocks.push_back(groupBlocks[i]);
    }
  }

  void parseInterleaveBlock(ParseResult& result,
                            std::vector<ClockBlockAst>& blocks) {
    Token leftParen = advance();
    std::vector<ClockBlockAst> rightLane;
    std::vector<std::vector<ClockBlockAst>> lanes;
    parseInterleaveContent(result, rightLane, lanes);

    Token rightParen;
    if (peek().type == TokenType::RightParen) {
      rightParen = advance();
    } else {
      addDiagnostic(result, "Expected ')'", rangeFromToken(leftParen));
      rightParen = leftParen;
    }

    ClockBlockAst repeatBlock;
    repeatBlock.range.begin = leftParen.begin;
    repeatBlock.range.end = rightParen.end;
    SourceRange suffixUnitRange;
    ClockUnit suffixUnit = parseGroupUnitSuffix(result, suffixUnitRange);
    parseProbability(result, repeatBlock);
    parseRepeat(result, repeatBlock);
    ClockBlockAst totalDurationBlock;
    parseTotalDuration(result, totalDurationBlock);
    bool hasGroupSuffix =
        suffixUnit != ClockUnit::Unknown ||
        repeatBlock.probabilityRange.end > repeatBlock.probabilityRange.begin ||
        repeatBlock.repeatRange.end > repeatBlock.repeatRange.begin ||
        totalDurationBlock.totalDurationRange.end >
            totalDurationBlock.totalDurationRange.begin;

    if (blocks.empty() || hasGroupSuffix) {
      std::vector<ClockBlockAst> interleaved = spreadInterleaveLanes(lanes);
      applyGroupModifiers(result, interleaved, repeatBlock, suffixUnit,
                          suffixUnitRange);
      applyTotalDurationGroup(interleaved, totalDurationBlock);
      for (size_t i = 0; i < interleaved.size(); i++) {
        blocks.push_back(interleaved[i]);
      }
      return;
    }
    if (rightLane.empty()) {
      addDiagnostic(result, "Expected clock literal in parentheses",
                    rangeFromToken(leftParen));
      return;
    }

    ClockBlockAst leftBlock = blocks.back();
    blocks.pop_back();
    std::vector<ClockBlockAst> leftLane;
    leftLane.push_back(leftBlock);
    std::vector<ClockBlockAst> interleaved =
        interleaveBlocks(leftLane, rightLane);
    for (size_t i = 0; i < interleaved.size(); i++) {
      blocks.push_back(interleaved[i]);
    }
    if (!blocks.empty()) {
      blocks.back().range.end = rightParen.end;
    }
  }

  void parseInterleaveContent(ParseResult& result,
                              std::vector<ClockBlockAst>& sequence,
                              std::vector<std::vector<ClockBlockAst>>& lanes) {
    while (!isAtEnd() && peek().type != TokenType::RightParen) {
      if (matchSeparator()) {
        continue;
      }

      std::vector<ClockBlockAst> lane;
      if (peek().type == TokenType::LeftParen) {
        lane = parseParenthesizedSequenceLane(result);
      } else {
        parseBlock(result, lane);
      }

      if (!lane.empty()) {
        lanes.push_back(lane);
        for (size_t i = 0; i < lane.size(); i++) {
          sequence.push_back(lane[i]);
        }
      }
    }
  }

  std::vector<ClockBlockAst> parseParenthesizedSequenceLane(
      ParseResult& result) {
    Token leftParen = advance();
    std::vector<ClockBlockAst> lane;
    parseSequenceUntil(result, lane, TokenType::RightParen);

    Token rightParen;
    if (peek().type == TokenType::RightParen) {
      rightParen = advance();
    } else {
      addDiagnostic(result, "Expected ')'", rangeFromToken(leftParen));
      rightParen = leftParen;
    }

    ClockBlockAst repeatBlock;
    repeatBlock.range.begin = leftParen.begin;
    repeatBlock.range.end = rightParen.end;
    SourceRange suffixUnitRange;
    ClockUnit suffixUnit = parseGroupUnitSuffix(result, suffixUnitRange);
    parseProbability(result, repeatBlock);
    parseRepeat(result, repeatBlock);
    ClockBlockAst totalDurationBlock;
    parseTotalDuration(result, totalDurationBlock);
    applyGroupModifiers(result, lane, repeatBlock, suffixUnit, suffixUnitRange);
    applyTotalDurationGroup(lane, totalDurationBlock);
    return lane;
  }

  void applyGroupModifiers(ParseResult& result,
                           std::vector<ClockBlockAst>& groupBlocks,
                           const ClockBlockAst& repeatBlock,
                           ClockUnit suffixUnit, SourceRange suffixUnitRange) {
    (void)result;
    for (size_t i = 0; i < groupBlocks.size(); i++) {
      if (suffixUnit != ClockUnit::Unknown) {
        applyUnitToUnitlessBlock(groupBlocks[i], suffixUnit, suffixUnitRange);
      }
      if (repeatBlock.repeatIsDuration) {
        groupBlocks[i].repeatIsDuration = true;
        groupBlocks[i].repeatDuration = repeatBlock.repeatDuration;
      } else {
        groupBlocks[i].repeat *= repeatBlock.repeat;
      }
      if (repeatBlock.repeatRange.end > repeatBlock.repeatRange.begin) {
        groupBlocks[i].repeatRange = repeatBlock.repeatRange;
        if (!groupBlocks[i].repeatValueIsOwn) {
          groupBlocks[i].repeatValueRange = repeatBlock.repeatValueRange;
        }
      }
      if (repeatBlock.probabilityRange.end >
              repeatBlock.probabilityRange.begin &&
          groupBlocks[i].probabilityRange.end <=
              groupBlocks[i].probabilityRange.begin) {
        groupBlocks[i].probability = repeatBlock.probability;
        groupBlocks[i].probabilityRange = repeatBlock.probabilityRange;
      }
    }
  }

  void applyTotalDurationGroup(std::vector<ClockBlockAst>& groupBlocks,
                               const ClockBlockAst& totalDurationBlock) {
    if (!totalDurationBlock.hasTotalDuration || groupBlocks.empty()) {
      return;
    }

    int groupId = totalDurationBlock.totalDurationGroupId;
    if (groupId < 0) {
      groupId = nextTotalDurationGroupId++;
    }
    for (size_t i = 0; i < groupBlocks.size(); i++) {
      groupBlocks[i].hasTotalDuration = true;
      groupBlocks[i].totalDurationIsTickCount =
          totalDurationBlock.totalDurationIsTickCount;
      groupBlocks[i].totalDurationTicks = totalDurationBlock.totalDurationTicks;
      groupBlocks[i].totalDuration = totalDurationBlock.totalDuration;
      groupBlocks[i].totalDurationGroupId = groupId;
      groupBlocks[i].totalDurationRange = totalDurationBlock.totalDurationRange;
      groupBlocks[i].totalDurationValueRange =
          totalDurationBlock.totalDurationValueRange;
    }
  }

  ClockUnit parseGroupUnitSuffix(ParseResult& result, SourceRange& unitRange) {
    if (peek().type != TokenType::Identifier) {
      return ClockUnit::Unknown;
    }

    Token unitToken = advance();
    unitRange = rangeFromToken(unitToken);
    ClockUnit unit = parseClockUnit(unitToken.lexeme);
    if (unit == ClockUnit::Unknown) {
      addDiagnostic(result, "Unknown clock unit", unitRange);
    }
    return unit;
  }

  ClockLiteralAst parseLiteral(ParseResult& result) {
    if (peek().type == TokenType::LeftBrace) {
      return parseRandomRangeLiteral(result);
    }

    Token firstNumber = advance();
    if (peek().type == TokenType::Colon) {
      return parseColonLiteral(result, firstNumber);
    }
    return parseNumericLiteral(result, firstNumber);
  }

  ClockLiteralAst parseNumericLiteral(ParseResult& result,
                                      const Token& valueToken) {
    ClockLiteralAst ast;
    ast.kind = ClockLiteralKind::Numeric;
    ast.unit = ClockUnit::Bpm;
    ast.value = parseDouble(valueToken.lexeme);
    ast.valueLexeme = valueToken.lexeme;
    ast.range.begin = valueToken.begin;

    parseOptionalUnit(result, ast);
    ast.range.end = ast.unitRange.end > 0 ? ast.unitRange.end : valueToken.end;
    return ast;
  }

  ClockLiteralAst parseRandomRangeLiteral(ParseResult& result) {
    Token leftBrace = advance();
    ClockLiteralAst ast;
    ast.kind = ClockLiteralKind::RandomRange;
    ast.unit = ClockUnit::Bpm;
    ast.range.begin = leftBrace.begin;

    while (!isAtEnd() && peek().type != TokenType::RightBrace) {
      if (peek().type != TokenType::Number &&
          peek().type != TokenType::LeftBrace) {
        addDiagnostic(result, "Expected random choice number",
                      rangeFromToken(peek()));
        ast.range.end = peek().end;
        advance();
        return ast;
      }

      std::vector<RandomChoiceAst> choices = parseRandomChoice(result);
      ast.randomChoices.insert(ast.randomChoices.end(), choices.begin(),
                               choices.end());
      if (peek().type == TokenType::Pipe) {
        advance();
        if (peek().type == TokenType::RightBrace) {
          addDiagnostic(result, "Expected random choice after '|'",
                        rangeFromToken(peek()));
          ast.range.end = peek().end;
          return ast;
        }
        continue;
      }

      if (peek().type != TokenType::RightBrace) {
        addDiagnostic(result, "Expected '|' or '}' in random literal",
                      rangeFromToken(peek()));
        ast.range.end = peek().end;
        return ast;
      }
    }

    if (ast.randomChoices.empty()) {
      addDiagnostic(result, "Expected random choice after '{'",
                    rangeFromToken(leftBrace));
      ast.range.end = leftBrace.end;
      return ast;
    }

    Token rightBrace = advance();
    ast.minValue = ast.randomChoices.front().minValue;
    ast.maxValue = ast.randomChoices.front().maxValue;
    ast.minValueLexeme = ast.randomChoices.front().minValueLexeme;
    ast.maxValueLexeme = ast.randomChoices.front().maxValueLexeme;
    parseOptionalUnit(result, ast);
    ast.range.end = ast.unitRange.end > 0 ? ast.unitRange.end : rightBrace.end;
    return ast;
  }

  std::vector<RandomChoiceAst> parseRandomChoice(ParseResult& result) {
    if (peek().type == TokenType::LeftBrace) {
      ClockLiteralAst nested = parseRandomRangeLiteral(result);
      for (size_t i = 0; i < nested.randomChoices.size(); i++) {
        nested.randomChoices[i].range.begin = nested.range.begin;
        nested.randomChoices[i].range.end = nested.range.end;
      }
      return nested.randomChoices;
    }

    std::vector<RandomChoiceAst> choices;
    RandomChoiceAst choice;
    Token minToken = advance();
    choice.minValue = parseDouble(minToken.lexeme);
    choice.maxValue = choice.minValue;
    choice.minValueLexeme = minToken.lexeme;
    choice.maxValueLexeme = minToken.lexeme;
    choice.range.begin = minToken.begin;
    choice.range.end = minToken.end;

    if (peek().type != TokenType::Dash) {
      choices.push_back(choice);
      return choices;
    }

    advance();
    if (peek().type != TokenType::Number) {
      addDiagnostic(result, "Expected range maximum after '-'",
                    rangeFromToken(peek()));
      choices.push_back(choice);
      return choices;
    }

    Token maxToken = advance();
    choice.maxValue = parseDouble(maxToken.lexeme);
    choice.maxValueLexeme = maxToken.lexeme;
    choice.range.end = maxToken.end;
    choices.push_back(choice);
    return choices;
  }

  ClockLiteralAst parseColonLiteral(ParseResult& result,
                                    const Token& minutesToken) {
    Token colon = advance();
    ClockLiteralAst ast;
    ast.kind = ClockLiteralKind::Colon;
    ast.minutesLexeme = minutesToken.lexeme;
    ast.range.begin = minutesToken.begin;

    if (!isIntegerLexeme(minutesToken.lexeme)) {
      addDiagnostic(result, "Colon time minutes must be an integer",
                    rangeFromToken(minutesToken));
    } else {
      ast.minutes = parseInt(minutesToken.lexeme);
    }

    if (peek().type != TokenType::Number) {
      addDiagnostic(result, "Expected seconds after ':'",
                    rangeFromToken(colon));
      ast.range.end = colon.end;
      return ast;
    }

    Token secondsToken = advance();
    ast.secondsLexeme = secondsToken.lexeme;
    if (!isIntegerLexeme(secondsToken.lexeme)) {
      addDiagnostic(result, "Colon time seconds must be an integer",
                    rangeFromToken(secondsToken));
    } else {
      ast.seconds = parseInt(secondsToken.lexeme);
      if (ast.seconds >= 60) {
        addDiagnostic(result, "Colon time seconds must be less than 60",
                      rangeFromToken(secondsToken));
      }
    }

    parseRequiredUnit(result, ast);
    ast.range.end =
        ast.unitRange.end > 0 ? ast.unitRange.end : secondsToken.end;
    return ast;
  }

  void parseOptionalUnit(ParseResult& result, ClockLiteralAst& ast) {
    if (peek().type != TokenType::Identifier) {
      return;
    }

    parseUnitToken(result, ast);
  }

  void parseRequiredUnit(ParseResult& result, ClockLiteralAst& ast) {
    if (peek().type != TokenType::Identifier) {
      addDiagnostic(result, "Expected clock unit", rangeFromToken(peek()));
      return;
    }

    parseUnitToken(result, ast);
  }

  void parseUnitToken(ParseResult& result, ClockLiteralAst& ast) {
    Token unitToken = advance();
    ast.unit = parseClockUnit(unitToken.lexeme);
    ast.unitRange = rangeFromToken(unitToken);
    if (ast.unit == ClockUnit::Unknown) {
      addDiagnostic(result, "Unknown clock unit", rangeFromToken(unitToken));
    }
  }

  void parseProbability(ParseResult& result, ClockBlockAst& block) {
    if (peek().type != TokenType::Question) {
      return;
    }

    Token questionToken = advance();
    block.probability = 50;
    block.probabilityRange.begin = questionToken.begin;
    block.probabilityRange.end = questionToken.end;

    if (peek().type != TokenType::Number || peek().begin != questionToken.end) {
      return;
    }

    Token probabilityToken = advance();
    block.probabilityRange.end = probabilityToken.end;
    if (!isIntegerLexeme(probabilityToken.lexeme)) {
      addDiagnostic(result, "Probability must be an integer",
                    rangeFromToken(probabilityToken));
      return;
    }

    block.probability = parseInt(probabilityToken.lexeme);
    if (block.probability < 0 || block.probability > 100) {
      addDiagnostic(result, "Probability must be between 0 and 100",
                    rangeFromToken(probabilityToken));
    }
  }

  void parseRepeat(ParseResult& result, ClockBlockAst& block) {
    if (peek().type != TokenType::At) {
      return;
    }

    Token atToken = advance();
    if (peek().type != TokenType::Number) {
      addDiagnostic(result, "Expected repeat count after '@'",
                    rangeFromToken(atToken));
      return;
    }

    Token repeatToken = advance();
    block.repeatRange.begin = atToken.begin;
    block.repeatRange.end = repeatToken.end;
    block.repeatValueRange = rangeFromToken(repeatToken);
    block.repeatValueIsOwn = true;

    if (peek().type == TokenType::Identifier) {
      ClockLiteralAst durationAst;
      durationAst.kind = ClockLiteralKind::Numeric;
      durationAst.value = parseDouble(repeatToken.lexeme);
      durationAst.valueLexeme = repeatToken.lexeme;
      durationAst.range.begin = repeatToken.begin;
      parseUnitToken(result, durationAst);
      durationAst.range.end = durationAst.unitRange.end > 0
                                  ? durationAst.unitRange.end
                                  : repeatToken.end;
      block.repeatRange.end = durationAst.range.end;
      block.repeatValueRange = durationAst.range;
      block.repeatIsDuration = true;
      block.repeatDuration = durationAst;
      if (!isDurationUnit(durationAst.unit)) {
        addDiagnostic(result, "Repeat duration must use a time unit",
                      durationAst.unitRange);
      }
      if (durationAst.value <= 0.0) {
        addDiagnostic(result, "Repeat duration must be greater than zero",
                      rangeFromToken(repeatToken));
      }
      return;
    }

    if (!isIntegerLexeme(repeatToken.lexeme)) {
      addDiagnostic(result, "Repeat count must be an integer",
                    rangeFromToken(repeatToken));
      return;
    }

    block.repeat = parseInt(repeatToken.lexeme);
    if (block.repeat <= 0) {
      addDiagnostic(result, "Repeat count must be greater than zero",
                    rangeFromToken(repeatToken));
    }
  }

  void parseTotalDuration(ParseResult& result, ClockBlockAst& block) {
    if (peek().type != TokenType::Hash) {
      return;
    }

    Token hashToken = advance();
    if (peek().type != TokenType::Number) {
      addDiagnostic(result, "Expected total duration after '#'",
                    rangeFromToken(hashToken));
      return;
    }

    Token durationToken = advance();
    block.totalDurationRange.begin = hashToken.begin;
    block.totalDurationRange.end = durationToken.end;
    block.totalDurationValueRange = rangeFromToken(durationToken);
    block.totalDurationGroupId = nextTotalDurationGroupId++;

    if (peek().type != TokenType::Identifier) {
      if (!isIntegerLexeme(durationToken.lexeme)) {
        addDiagnostic(result, "Total tick count must be an integer",
                      rangeFromToken(durationToken));
        return;
      }

      int ticks = parseInt(durationToken.lexeme);
      if (ticks <= 0) {
        addDiagnostic(result, "Total tick count must be greater than zero",
                      rangeFromToken(durationToken));
        return;
      }

      block.hasTotalDuration = true;
      block.totalDurationIsTickCount = true;
      block.totalDurationTicks = ticks;
      return;
    }

    ClockLiteralAst durationAst;
    durationAst.kind = ClockLiteralKind::Numeric;
    durationAst.value = parseDouble(durationToken.lexeme);
    durationAst.valueLexeme = durationToken.lexeme;
    durationAst.range.begin = durationToken.begin;

    parseUnitToken(result, durationAst);
    durationAst.range.end = durationAst.unitRange.end > 0
                                ? durationAst.unitRange.end
                                : durationToken.end;
    block.totalDurationRange.end = durationAst.range.end;
    block.totalDurationValueRange = durationAst.range;
    block.hasTotalDuration = true;
    block.totalDuration = durationAst;

    if (!isDurationUnit(durationAst.unit)) {
      addDiagnostic(result, "Total duration must use a time unit",
                    durationAst.unitRange);
    }
    if (durationAst.value <= 0.0) {
      addDiagnostic(result, "Total duration must be greater than zero",
                    rangeFromToken(durationToken));
    }
  }
};
}  // namespace

ParseResult parseClockLiteral(const std::string& source) {
  return parseClockLiteral(tokenize(source));
}

ParseResult parseClockLiteral(const std::vector<Token>& tokens) {
  Parser parser(tokens);
  return parser.parse();
}

ClockUnit parseClockUnit(const std::string& unit) {
  std::string lowered = lowerCopy(unit);
  if (lowered == "bpm" || lowered == "rpm") {
    return ClockUnit::Bpm;
  }
  if (lowered == "hz" || lowered == "cps") {
    return ClockUnit::Hertz;
  }
  if (lowered == "mhz") {
    return ClockUnit::Millihertz;
  }
  if (lowered == "khz") {
    return ClockUnit::Kilohertz;
  }
  if (lowered == "ms") {
    return ClockUnit::Milliseconds;
  }
  if (lowered == "s" || lowered == "sec" || lowered == "second" ||
      lowered == "seconds") {
    return ClockUnit::Seconds;
  }
  if (lowered == "m" || lowered == "min" || lowered == "mins" ||
      lowered == "minute" || lowered == "minutes") {
    return ClockUnit::Minutes;
  }
  return ClockUnit::Unknown;
}

std::string clockUnitName(ClockUnit unit) {
  switch (unit) {
    case ClockUnit::Bpm:
      return "bpm";
    case ClockUnit::Hertz:
      return "hz";
    case ClockUnit::Millihertz:
      return "mhz";
    case ClockUnit::Kilohertz:
      return "khz";
    case ClockUnit::Milliseconds:
      return "ms";
    case ClockUnit::Seconds:
      return "seconds";
    case ClockUnit::Minutes:
      return "minutes";
    case ClockUnit::Unknown:
      return "unknown";
  }
  return "unknown";
}

}  // namespace language
}  // namespace cloly
