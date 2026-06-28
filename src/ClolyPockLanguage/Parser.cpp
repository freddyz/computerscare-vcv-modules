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

class Parser {
 public:
  explicit Parser(const std::vector<Token>& tokens) : tokens(tokens) {}

  ParseResult parse() {
    ParseResult result;

    if (peek().type == TokenType::End) {
      addDiagnostic(result, "Expected clock literal", rangeFromToken(peek()));
      return result;
    }

    if (peek().type == TokenType::Invalid) {
      addDiagnostic(result, "Invalid token", rangeFromToken(peek()));
      advance();
      consumeTrailing(result);
      return result;
    }

    if (peek().type != TokenType::Number) {
      addDiagnostic(result, "Expected number", rangeFromToken(peek()));
      consumeTrailing(result);
      return result;
    }

    Token firstNumber = advance();
    if (peek().type == TokenType::Colon) {
      parseColonLiteral(result, firstNumber);
    } else {
      parseNumericLiteral(result, firstNumber);
    }

    consumeTrailing(result);
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

  void parseNumericLiteral(ParseResult& result, const Token& valueToken) {
    result.ast.kind = ClockLiteralKind::Numeric;
    result.ast.value = parseDouble(valueToken.lexeme);
    result.ast.valueLexeme = valueToken.lexeme;
    result.ast.range.begin = valueToken.begin;

    parseUnit(result);
    result.ast.range.end = result.ast.unitRange.end > 0
                               ? result.ast.unitRange.end
                               : valueToken.end;
  }

  void parseColonLiteral(ParseResult& result, const Token& minutesToken) {
    Token colon = advance();
    result.ast.kind = ClockLiteralKind::Colon;
    result.ast.minutesLexeme = minutesToken.lexeme;
    result.ast.range.begin = minutesToken.begin;

    if (!isIntegerLexeme(minutesToken.lexeme)) {
      addDiagnostic(result, "Colon time minutes must be an integer",
                    rangeFromToken(minutesToken));
    } else {
      result.ast.minutes = parseInt(minutesToken.lexeme);
    }

    if (peek().type != TokenType::Number) {
      addDiagnostic(result, "Expected seconds after ':'",
                    rangeFromToken(colon));
      return;
    }

    Token secondsToken = advance();
    result.ast.secondsLexeme = secondsToken.lexeme;
    if (!isIntegerLexeme(secondsToken.lexeme)) {
      addDiagnostic(result, "Colon time seconds must be an integer",
                    rangeFromToken(secondsToken));
    } else {
      result.ast.seconds = parseInt(secondsToken.lexeme);
      if (result.ast.seconds >= 60) {
        addDiagnostic(result, "Colon time seconds must be less than 60",
                      rangeFromToken(secondsToken));
      }
    }

    parseUnit(result);
    result.ast.range.end = result.ast.unitRange.end > 0
                               ? result.ast.unitRange.end
                               : secondsToken.end;
  }

  void parseUnit(ParseResult& result) {
    if (peek().type != TokenType::Identifier) {
      addDiagnostic(result, "Expected clock unit", rangeFromToken(peek()));
      return;
    }

    Token unitToken = advance();
    result.ast.unit = parseClockUnit(unitToken.lexeme);
    result.ast.unitRange = rangeFromToken(unitToken);
    if (result.ast.unit == ClockUnit::Unknown) {
      addDiagnostic(result, "Unknown clock unit", rangeFromToken(unitToken));
    }
  }

  void consumeTrailing(ParseResult& result) {
    while (!isAtEnd()) {
      addDiagnostic(result, "Unexpected trailing input",
                    rangeFromToken(peek()));
      advance();
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
  if (lowered == "bpm") {
    return ClockUnit::Bpm;
  }
  if (lowered == "hz") {
    return ClockUnit::Hertz;
  }
  if (lowered == "ms") {
    return ClockUnit::Milliseconds;
  }
  if (lowered == "sec" || lowered == "second" || lowered == "seconds") {
    return ClockUnit::Seconds;
  }
  if (lowered == "min" || lowered == "minute" || lowered == "minutes") {
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
