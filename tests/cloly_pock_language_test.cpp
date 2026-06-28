#include "ClolyPockLanguage/ClolyPockLanguage.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace lang = cloly::language;

namespace {
void require(bool condition, const char* message) {
  if (!condition) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    std::exit(1);
  }
}

void requireNear(double actual, double expected, const char* message) {
  if (std::fabs(actual - expected) > 0.000001) {
    std::fprintf(stderr, "FAIL: %s expected %.9f got %.9f\n", message,
                 expected, actual);
    std::exit(1);
  }
}

void requireToken(const lang::Token& token, lang::TokenType type,
                  const std::string& lexeme, int begin, int end,
                  const char* message) {
  require(token.type == type, message);
  require(token.lexeme == lexeme, message);
  require(token.begin == begin, message);
  require(token.end == end, message);
}

void requireNumericAst(const std::string& source, double value,
                       lang::ClockUnit unit, const char* message) {
  lang::ParseResult result = lang::parseClockLiteral(source);
  require(result.ok(), message);
  require(result.ast.kind == lang::ClockLiteralKind::Numeric, message);
  requireNear(result.ast.value, value, message);
  require(result.ast.unit == unit, message);
}

void requireColonAst(const std::string& source, int minutes, int seconds,
                     lang::ClockUnit unit, const char* message) {
  lang::ParseResult result = lang::parseClockLiteral(source);
  require(result.ok(), message);
  require(result.ast.kind == lang::ClockLiteralKind::Colon, message);
  require(result.ast.minutes == minutes, message);
  require(result.ast.seconds == seconds, message);
  require(result.ast.unit == unit, message);
}

lang::ClockSpec requireEvaluates(const std::string& source) {
  lang::ParseResult parse = lang::parseClockLiteral(source);
  require(parse.ok(), "parse should succeed before evaluation");
  lang::EvaluationResult eval = lang::evaluateClockLiteral(parse.ast);
  require(eval.ok(), "evaluation should succeed");
  return eval.spec;
}

void requireInvalid(const std::string& source, const char* message) {
  lang::ParseResult parse = lang::parseClockLiteral(source);
  if (parse.ok()) {
    lang::EvaluationResult eval = lang::evaluateClockLiteral(parse.ast);
    require(!eval.ok(), message);
  } else {
    require(!parse.diagnostics.empty(), message);
  }
}

void testTokenizer() {
  std::vector<lang::Token> tokens = lang::tokenize("120 BPM");
  require(tokens.size() == 3, "120 BPM token count");
  requireToken(tokens[0], lang::TokenType::Number, "120", 0, 3,
               "120 BPM first token");
  requireToken(tokens[1], lang::TokenType::Identifier, "BPM", 4, 7,
               "120 BPM second token");
  require(tokens[2].type == lang::TokenType::End, "120 BPM end token");

  tokens = lang::tokenize("3:23 minutes");
  require(tokens.size() == 5, "3:23 minutes token count");
  requireToken(tokens[0], lang::TokenType::Number, "3", 0, 1,
               "3:23 minutes token 0");
  requireToken(tokens[1], lang::TokenType::Colon, ":", 1, 2,
               "3:23 minutes token 1");
  requireToken(tokens[2], lang::TokenType::Number, "23", 2, 4,
               "3:23 minutes token 2");
  requireToken(tokens[3], lang::TokenType::Identifier, "minutes", 5, 12,
               "3:23 minutes token 3");

  tokens = lang::tokenize("21.1hz");
  require(tokens.size() == 3, "21.1hz token count");
  requireToken(tokens[0], lang::TokenType::Number, "21.1", 0, 4,
               "21.1hz first token");
  requireToken(tokens[1], lang::TokenType::Identifier, "hz", 4, 6,
               "21.1hz second token");

  tokens = lang::tokenize(".3hz");
  require(tokens.size() == 3, ".3hz token count");
  requireToken(tokens[0], lang::TokenType::Number, ".3", 0, 2,
               ".3hz first token");
  requireToken(tokens[1], lang::TokenType::Identifier, "hz", 2, 4,
               ".3hz second token");

  tokens = lang::tokenize("500mhz");
  require(tokens.size() == 3, "500mhz token count");
  requireToken(tokens[0], lang::TokenType::Number, "500", 0, 3,
               "500mhz first token");
  requireToken(tokens[1], lang::TokenType::Identifier, "mhz", 3, 6,
               "500mhz second token");
}

void testAst() {
  requireNumericAst("120bpm", 120.0, lang::ClockUnit::Bpm, "120bpm ast");
  requireNumericAst("45rpm", 45.0, lang::ClockUnit::Bpm, "45rpm ast");
  requireNumericAst("33Hz", 33.0, lang::ClockUnit::Hertz, "33Hz ast");
  requireNumericAst("8cps", 8.0, lang::ClockUnit::Hertz, "8cps ast");
  requireNumericAst("21.1hz", 21.1, lang::ClockUnit::Hertz, "21.1hz ast");
  requireNumericAst(".3hz", 0.3, lang::ClockUnit::Hertz, ".3hz ast");
  requireNumericAst("500mhz", 500.0, lang::ClockUnit::Millihertz,
                    "500mhz ast");
  requireNumericAst("1.5khz", 1.5, lang::ClockUnit::Kilohertz, "1.5khz ast");
  requireNumericAst("40ms", 40.0, lang::ClockUnit::Milliseconds, "40ms ast");
  requireNumericAst("2.4 seconds", 2.4, lang::ClockUnit::Seconds,
                    "2.4 seconds ast");
  requireNumericAst("3s", 3.0, lang::ClockUnit::Seconds, "3s ast");
  requireNumericAst("12sec", 12.0, lang::ClockUnit::Seconds, "12sec ast");
  requireNumericAst("2m", 2.0, lang::ClockUnit::Minutes, "2m ast");
  requireNumericAst("4mins", 4.0, lang::ClockUnit::Minutes, "4mins ast");
  requireColonAst("3:23 minutes", 3, 23, lang::ClockUnit::Minutes,
                  "3:23 minutes ast");
}

void testEvaluator() {
  lang::ClockSpec spec = requireEvaluates("120bpm");
  requireNear(spec.hz, 2.0, "120bpm hz");
  requireNear(spec.periodSeconds, 0.5, "120bpm period");

  spec = requireEvaluates("45rpm");
  requireNear(spec.hz, 0.75, "45rpm hz");
  requireNear(spec.periodSeconds, 60.0 / 45.0, "45rpm period");

  spec = requireEvaluates("33Hz");
  requireNear(spec.hz, 33.0, "33Hz hz");
  requireNear(spec.periodSeconds, 1.0 / 33.0, "33Hz period");

  spec = requireEvaluates("8cps");
  requireNear(spec.hz, 8.0, "8cps hz");
  requireNear(spec.periodSeconds, 0.125, "8cps period");

  spec = requireEvaluates(".3hz");
  requireNear(spec.hz, 0.3, ".3hz hz");
  requireNear(spec.periodSeconds, 1.0 / 0.3, ".3hz period");

  spec = requireEvaluates("500mhz");
  requireNear(spec.hz, 0.5, "500mhz hz");
  requireNear(spec.periodSeconds, 2.0, "500mhz period");

  spec = requireEvaluates("1.5khz");
  requireNear(spec.hz, 1500.0, "1.5khz hz");
  requireNear(spec.periodSeconds, 1.0 / 1500.0, "1.5khz period");

  spec = requireEvaluates("40ms");
  requireNear(spec.periodSeconds, 0.04, "40ms period");

  spec = requireEvaluates("2.4 seconds");
  requireNear(spec.periodSeconds, 2.4, "2.4 seconds period");

  spec = requireEvaluates("3s");
  requireNear(spec.periodSeconds, 3.0, "3s period");

  spec = requireEvaluates("2m");
  requireNear(spec.periodSeconds, 120.0, "2m period");

  spec = requireEvaluates("4mins");
  requireNear(spec.periodSeconds, 240.0, "4mins period");

  spec = requireEvaluates("3:23 minutes");
  requireNear(spec.periodSeconds, 203.0, "3:23 minutes period");
}

void testInvalidInputs() {
  requireInvalid("120", "missing unit invalid");
  requireInvalid("120bananas", "unknown unit invalid");
  requireInvalid("1.2.3hz", "malformed number invalid");
  requireInvalid("3.5:23 minutes", "decimal colon minutes invalid");
  requireInvalid("3:90 minutes", "colon seconds >= 60 invalid");
  requireInvalid("120bpm extra", "trailing junk invalid");
  requireInvalid("hz", "missing number invalid");
}
}  // namespace

int main() {
  testTokenizer();
  testAst();
  testEvaluator();
  testInvalidInputs();
  std::puts("cloly pock language tests passed");
  return 0;
}
