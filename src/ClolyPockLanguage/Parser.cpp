#include "Parser.hpp"

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

    if (peek().type == TokenType::RightBracket) {
      addDiagnostic(result, "Unexpected ']'", rangeFromToken(peek()));
      advance();
      return;
    }

    if (peek().type == TokenType::LeftBracket) {
      parseBracketBlock(result, blocks);
      return;
    }

    if (peek().type != TokenType::Number) {
      addDiagnostic(result, "Expected clock literal", rangeFromToken(peek()));
      advance();
      return;
    }

    ClockBlockAst block;
    block.literal = parseLiteral(result);
    block.range = block.literal.range;
    parseProbability(result, block);
    parseRepeat(result, block);
    blocks.push_back(block);
  }

  void parseBracketBlock(ParseResult& result,
                         std::vector<ClockBlockAst>& blocks) {
    Token leftBracket = advance();
    std::vector<ClockBlockAst> groupBlocks;

    while (!isAtEnd() && peek().type != TokenType::RightBracket) {
      if (matchSeparator()) {
        continue;
      }
      parseBlock(result, groupBlocks);
    }

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
    parseRepeat(result, repeatBlock);

    if (groupBlocks.empty()) {
      addDiagnostic(result, "Expected clock literal in brackets",
                    rangeFromToken(leftBracket));
      return;
    }

    for (size_t i = 0; i < groupBlocks.size(); i++) {
      if (repeatBlock.repeatIsDuration) {
        groupBlocks[i].repeatIsDuration = true;
        groupBlocks[i].repeatDuration = repeatBlock.repeatDuration;
      } else {
        groupBlocks[i].repeat *= repeatBlock.repeat;
      }
      if (repeatBlock.repeatRange.end > repeatBlock.repeatRange.begin) {
        groupBlocks[i].repeatRange = repeatBlock.repeatRange;
      }
      blocks.push_back(groupBlocks[i]);
    }
  }

  ClockLiteralAst parseLiteral(ParseResult& result) {
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
