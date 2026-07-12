#include "BlunchLanguage/BlunchLanguage.hpp"
#include "Blunch/BlunchRandomProgram.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace lang = blunch::language;

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

void requireRange(lang::SourceRange range, int begin, int end,
                  const char* message) {
  require(range.begin == begin, message);
  require(range.end == end, message);
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

void requireRandomRangeAst(const std::string& source, double minValue,
                           double maxValue, lang::ClockUnit unit,
                           const char* message) {
  lang::ParseResult result = lang::parseClockLiteral(source);
  require(result.ok(), message);
  require(result.ast.kind == lang::ClockLiteralKind::RandomRange, message);
  requireNear(result.ast.minValue, minValue, message);
  requireNear(result.ast.maxValue, maxValue, message);
  require(result.ast.unit == unit, message);
}

void requireExternalClockAst(const std::string& source, char clock,
                             const char* message) {
  lang::ParseResult result = lang::parseClockLiteral(source);
  require(result.ok(), message);
  require(result.ast.kind == lang::ClockLiteralKind::ExternalClock, message);
  require(result.ast.externalClock == clock, message);
}

void requireRandomChoice(const lang::ClockLiteralAst& ast, size_t index,
                         double minValue, double maxValue,
                         const char* message) {
  require(index < ast.randomChoices.size(), message);
  requireNear(ast.randomChoices[index].minValue, minValue, message);
  requireNear(ast.randomChoices[index].maxValue, maxValue, message);
}

void requireProgramValues(const lang::ParseResult& result,
                          const std::vector<double>& values,
                          const char* message) {
  require(result.program.blocks.size() == values.size(), message);
  for (size_t i = 0; i < values.size(); i++) {
    requireNear(result.program.blocks[i].literal.value, values[i], message);
  }
}

void requireDurationRepeat(const lang::ClockBlockAst& block, double value,
                           lang::ClockUnit unit, int begin, int end,
                           const char* message) {
  require(block.repeatIsDuration, message);
  require(block.repeatDuration.kind == lang::ClockLiteralKind::Numeric,
          message);
  requireNear(block.repeatDuration.value, value, message);
  require(block.repeatDuration.unit == unit, message);
  requireRange(block.repeatRange, begin, end, message);
}

void requireTotalDurationGroup(const lang::ClockBlockAst& block, int groupId,
                               double value, lang::ClockUnit unit, int begin,
                               int end, const char* message) {
  require(block.hasTotalDuration, message);
  require(block.totalDurationGroupId == groupId, message);
  require(block.totalDuration.kind == lang::ClockLiteralKind::Numeric,
          message);
  requireNear(block.totalDuration.value, value, message);
  require(block.totalDuration.unit == unit, message);
  requireRange(block.totalDurationRange, begin, end, message);
}

void requireTotalTickGroup(const lang::ClockBlockAst& block, int groupId,
                           int ticks, int begin, int end,
                           const char* message) {
  require(block.hasTotalDuration, message);
  require(block.totalDurationIsTickCount, message);
  require(block.totalDurationGroupId == groupId, message);
  require(block.totalDurationTicks == ticks, message);
  requireRange(block.totalDurationRange, begin, end, message);
}

void requireRandomChoiceUnit(const lang::ClockLiteralAst& ast, size_t index,
                             lang::ClockUnit unit, const char* message) {
  require(index < ast.randomChoices.size(), message);
  require(ast.randomChoices[index].unit == unit, message);
}

void requireRandomChoiceExternalClock(const lang::ClockLiteralAst& ast,
                                      size_t index, char clock,
                                      const char* message) {
  require(index < ast.randomChoices.size(), message);
  require(ast.randomChoices[index].externalClockChoice, message);
  require(ast.randomChoices[index].externalClock == clock, message);
}

void requireRandomChoiceRest(const lang::ClockLiteralAst& ast, size_t index,
                             const char* message) {
  require(index < ast.randomChoices.size(), message);
  require(ast.randomChoices[index].restChoice, message);
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

  tokens = lang::tokenize("[x y]@5");
  require(tokens.size() == 7, "[x y]@5 token count");
  requireToken(tokens[0], lang::TokenType::LeftBracket, "[", 0, 1,
               "[x y]@5 token 0");
  requireToken(tokens[1], lang::TokenType::Identifier, "x", 1, 2,
               "[x y]@5 token 1");
  requireToken(tokens[2], lang::TokenType::Identifier, "y", 3, 4,
               "[x y]@5 token 2");
  requireToken(tokens[3], lang::TokenType::RightBracket, "]", 4, 5,
               "[x y]@5 token 3");
  requireToken(tokens[4], lang::TokenType::At, "@", 5, 6,
               "[x y]@5 token 4");
  requireToken(tokens[5], lang::TokenType::Number, "5", 6, 7,
               "[x y]@5 token 5");

  tokens = lang::tokenize("[120 90]@6");
  require(tokens.size() == 7, "[120 90]@6 token count");
  requireToken(tokens[0], lang::TokenType::LeftBracket, "[", 0, 1,
               "[120 90]@6 token 0");
  requireToken(tokens[1], lang::TokenType::Number, "120", 1, 4,
               "[120 90]@6 token 1");
  requireToken(tokens[2], lang::TokenType::Number, "90", 5, 7,
               "[120 90]@6 token 2");
  requireToken(tokens[3], lang::TokenType::RightBracket, "]", 7, 8,
               "[120 90]@6 token 3");
  requireToken(tokens[4], lang::TokenType::At, "@", 8, 9,
               "[120 90]@6 token 4");
  requireToken(tokens[5], lang::TokenType::Number, "6", 9, 10,
               "[120 90]@6 token 5");

  tokens = lang::tokenize("[244 [9hz,8hz]]@9");
  require(tokens.size() == 13, "[244 [9hz,8hz]]@9 token count");
  requireToken(tokens[0], lang::TokenType::LeftBracket, "[", 0, 1,
               "[244 [9hz,8hz]]@9 token 0");
  requireToken(tokens[1], lang::TokenType::Number, "244", 1, 4,
               "[244 [9hz,8hz]]@9 token 1");
  requireToken(tokens[2], lang::TokenType::LeftBracket, "[", 5, 6,
               "[244 [9hz,8hz]]@9 token 2");
  requireToken(tokens[3], lang::TokenType::Number, "9", 6, 7,
               "[244 [9hz,8hz]]@9 token 3");
  requireToken(tokens[4], lang::TokenType::Identifier, "hz", 7, 9,
               "[244 [9hz,8hz]]@9 token 4");
  requireToken(tokens[5], lang::TokenType::Comma, ",", 9, 10,
               "[244 [9hz,8hz]]@9 token 5");
  requireToken(tokens[6], lang::TokenType::Number, "8", 10, 11,
               "[244 [9hz,8hz]]@9 token 6");
  requireToken(tokens[7], lang::TokenType::Identifier, "hz", 11, 13,
               "[244 [9hz,8hz]]@9 token 7");
  requireToken(tokens[8], lang::TokenType::RightBracket, "]", 13, 14,
               "[244 [9hz,8hz]]@9 token 8");
  requireToken(tokens[9], lang::TokenType::RightBracket, "]", 14, 15,
               "[244 [9hz,8hz]]@9 token 9");
  requireToken(tokens[10], lang::TokenType::At, "@", 15, 16,
               "[244 [9hz,8hz]]@9 token 10");
  requireToken(tokens[11], lang::TokenType::Number, "9", 16, 17,
               "[244 [9hz,8hz]]@9 token 11");
  require(tokens[12].type == lang::TokenType::End,
          "[244 [9hz,8hz]]@9 end token");

  tokens = lang::tokenize("188?50");
  require(tokens.size() == 4, "188?50 token count");
  requireToken(tokens[0], lang::TokenType::Number, "188", 0, 3,
               "188?50 token 0");
  requireToken(tokens[1], lang::TokenType::Question, "?", 3, 4,
               "188?50 token 1");
  requireToken(tokens[2], lang::TokenType::Number, "50", 4, 6,
               "188?50 token 2");

  tokens = lang::tokenize("120@2s 90@3sec");
  require(tokens.size() == 9, "120@2s 90@3sec token count");
  requireToken(tokens[0], lang::TokenType::Number, "120", 0, 3,
               "120@2s token 0");
  requireToken(tokens[1], lang::TokenType::At, "@", 3, 4,
               "120@2s token 1");
  requireToken(tokens[2], lang::TokenType::Number, "2", 4, 5,
               "120@2s token 2");
  requireToken(tokens[3], lang::TokenType::Identifier, "s", 5, 6,
               "120@2s token 3");
  requireToken(tokens[4], lang::TokenType::Number, "90", 7, 9,
               "90@3sec token 0");
  requireToken(tokens[5], lang::TokenType::At, "@", 9, 10,
               "90@3sec token 1");
  requireToken(tokens[6], lang::TokenType::Number, "3", 10, 11,
               "90@3sec token 2");
  requireToken(tokens[7], lang::TokenType::Identifier, "sec", 11, 14,
               "90@3sec token 3");

  tokens = lang::tokenize("(3hz 4hz)#9s");
  require(tokens.size() == 10, "total duration token count");
  requireToken(tokens[0], lang::TokenType::LeftParen, "(", 0, 1,
               "total duration token 0");
  requireToken(tokens[1], lang::TokenType::Number, "3", 1, 2,
               "total duration token 1");
  requireToken(tokens[2], lang::TokenType::Identifier, "hz", 2, 4,
               "total duration token 2");
  requireToken(tokens[3], lang::TokenType::Number, "4", 5, 6,
               "total duration token 3");
  requireToken(tokens[4], lang::TokenType::Identifier, "hz", 6, 8,
               "total duration token 4");
  requireToken(tokens[5], lang::TokenType::RightParen, ")", 8, 9,
               "total duration token 5");
  requireToken(tokens[6], lang::TokenType::Hash, "#", 9, 10,
               "total duration token 6");
  requireToken(tokens[7], lang::TokenType::Number, "9", 10, 11,
               "total duration token 7");
  requireToken(tokens[8], lang::TokenType::Identifier, "s", 11, 12,
               "total duration token 8");

  tokens = lang::tokenize("2hz@4 ~3hz@4 ~@3s");
  require(tokens.size() == 14, "rest token count");
  requireToken(tokens[0], lang::TokenType::Number, "2", 0, 1,
               "rest token 0");
  requireToken(tokens[4], lang::TokenType::Tilde, "~", 6, 7,
               "rest token 4");
  requireToken(tokens[9], lang::TokenType::Tilde, "~", 13, 14,
               "rest token 9");
  requireToken(tokens[10], lang::TokenType::At, "@", 14, 15,
               "rest token 10");

  tokens = lang::tokenize("2hz (2hz 4hz)");
  require(tokens.size() == 9, "interleave token count");
  requireToken(tokens[0], lang::TokenType::Number, "2", 0, 1,
               "interleave token 0");
  requireToken(tokens[1], lang::TokenType::Identifier, "hz", 1, 3,
               "interleave token 1");
  requireToken(tokens[2], lang::TokenType::LeftParen, "(", 4, 5,
               "interleave token 2");
  requireToken(tokens[3], lang::TokenType::Number, "2", 5, 6,
               "interleave token 3");
  requireToken(tokens[4], lang::TokenType::Identifier, "hz", 6, 8,
               "interleave token 4");
  requireToken(tokens[5], lang::TokenType::Number, "4", 9, 10,
               "interleave token 5");
  requireToken(tokens[6], lang::TokenType::Identifier, "hz", 10, 12,
               "interleave token 6");
  requireToken(tokens[7], lang::TokenType::RightParen, ")", 12, 13,
               "interleave token 7");

  tokens = lang::tokenize("{3-5}hz");
  require(tokens.size() == 7, "random range token count");
  requireToken(tokens[0], lang::TokenType::LeftBrace, "{", 0, 1,
               "random range token 0");
  requireToken(tokens[1], lang::TokenType::Number, "3", 1, 2,
               "random range token 1");
  requireToken(tokens[2], lang::TokenType::Dash, "-", 2, 3,
               "random range token 2");
  requireToken(tokens[3], lang::TokenType::Number, "5", 3, 4,
               "random range token 3");
  requireToken(tokens[4], lang::TokenType::RightBrace, "}", 4, 5,
               "random range token 4");
  requireToken(tokens[5], lang::TokenType::Identifier, "hz", 5, 7,
               "random range token 5");

  tokens = lang::tokenize("{3|2-10}hz");
  require(tokens.size() == 9, "random choice range token count");
  requireToken(tokens[0], lang::TokenType::LeftBrace, "{", 0, 1,
               "random choice range token 0");
  requireToken(tokens[1], lang::TokenType::Number, "3", 1, 2,
               "random choice range token 1");
  requireToken(tokens[2], lang::TokenType::Pipe, "|", 2, 3,
               "random choice range token 2");
  requireToken(tokens[3], lang::TokenType::Number, "2", 3, 4,
               "random choice range token 3");
  requireToken(tokens[4], lang::TokenType::Dash, "-", 4, 5,
               "random choice range token 4");
  requireToken(tokens[5], lang::TokenType::Number, "10", 5, 7,
               "random choice range token 5");
  requireToken(tokens[6], lang::TokenType::RightBrace, "}", 7, 8,
               "random choice range token 6");
  requireToken(tokens[7], lang::TokenType::Identifier, "hz", 8, 10,
               "random choice range token 7");

  tokens = lang::tokenize("{3|2|{6|9}}hz");
  require(tokens.size() == 13, "nested random choice token count");
  requireToken(tokens[0], lang::TokenType::LeftBrace, "{", 0, 1,
               "nested random choice token 0");
  requireToken(tokens[4], lang::TokenType::Pipe, "|", 4, 5,
               "nested random choice token 4");
  requireToken(tokens[5], lang::TokenType::LeftBrace, "{", 5, 6,
               "nested random choice token 5");
  requireToken(tokens[10], lang::TokenType::RightBrace, "}", 10, 11,
               "nested random choice token 10");
  requireToken(tokens[11], lang::TokenType::Identifier, "hz", 11, 13,
               "nested random choice token 11");
}

void testAst() {
  requireNumericAst("120", 120.0, lang::ClockUnit::Bpm, "120 ast");
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
  requireRandomRangeAst("{3-5}hz", 3.0, 5.0, lang::ClockUnit::Hertz,
                        "{3-5}hz ast");
  requireRandomRangeAst("{3-3}", 3.0, 3.0, lang::ClockUnit::Bpm,
                        "{3-3} ast");
  requireRandomRangeAst("{4-2}s", 4.0, 2.0, lang::ClockUnit::Seconds,
                        "{4-2}s ast");
  requireExternalClockAst("x", 'x', "x external clock ast");
  requireExternalClockAst("W", 'w', "W external clock ast");

  lang::ParseResult result = lang::parseClockLiteral("{3|2}hz");
  require(result.ok(), "{3|2}hz ast parses");
  require(result.ast.kind == lang::ClockLiteralKind::RandomRange,
          "{3|2}hz random kind");
  require(result.ast.randomChoices.size() == 2, "{3|2}hz choice count");
  requireRandomChoice(result.ast, 0, 3.0, 3.0, "{3|2}hz choice 0");
  requireRandomChoice(result.ast, 1, 2.0, 2.0, "{3|2}hz choice 1");
  require(result.ast.unit == lang::ClockUnit::Hertz, "{3|2}hz unit");

  result = lang::parseClockLiteral("{3|2-10}hz");
  require(result.ok(), "{3|2-10}hz ast parses");
  require(result.ast.randomChoices.size() == 2, "{3|2-10}hz choice count");
  requireRandomChoice(result.ast, 0, 3.0, 3.0, "{3|2-10}hz choice 0");
  requireRandomChoice(result.ast, 1, 2.0, 10.0, "{3|2-10}hz choice 1");
  require(result.ast.unit == lang::ClockUnit::Hertz, "{3|2-10}hz unit");

  result = lang::parseClockLiteral("{211ms|3hz}");
  require(result.ok(), "{211ms|3hz} ast parses");
  require(result.ast.randomChoices.size() == 2, "{211ms|3hz} choice count");
  requireRandomChoice(result.ast, 0, 211.0, 211.0,
                      "{211ms|3hz} choice 0");
  requireRandomChoiceUnit(result.ast, 0, lang::ClockUnit::Milliseconds,
                          "{211ms|3hz} choice 0 unit");
  requireRandomChoice(result.ast, 1, 3.0, 3.0, "{211ms|3hz} choice 1");
  requireRandomChoiceUnit(result.ast, 1, lang::ClockUnit::Hertz,
                          "{211ms|3hz} choice 1 unit");

  result = lang::parseClockLiteral("{.211s|3hz}");
  require(result.ok(), "{.211s|3hz} ast parses");
  require(result.ast.randomChoices.size() == 2, "{.211s|3hz} choice count");
  requireRandomChoice(result.ast, 0, 0.211, 0.211,
                      "{.211s|3hz} choice 0");
  requireRandomChoiceUnit(result.ast, 0, lang::ClockUnit::Seconds,
                          "{.211s|3hz} choice 0 unit");
  requireRandomChoice(result.ast, 1, 3.0, 3.0, "{.211s|3hz} choice 1");
  requireRandomChoiceUnit(result.ast, 1, lang::ClockUnit::Hertz,
                          "{.211s|3hz} choice 1 unit");

  result = lang::parseClockLiteral("{3|2|{6|9}}hz");
  require(result.ok(), "{3|2|{6|9}}hz ast parses");
  require(result.ast.randomChoices.size() == 4,
          "{3|2|{6|9}}hz choice count");
  requireRandomChoice(result.ast, 0, 3.0, 3.0,
                      "{3|2|{6|9}}hz choice 0");
  requireRandomChoice(result.ast, 1, 2.0, 2.0,
                      "{3|2|{6|9}}hz choice 1");
  requireRandomChoice(result.ast, 2, 6.0, 6.0,
                      "{3|2|{6|9}}hz choice 2");
  requireRandomChoice(result.ast, 3, 9.0, 9.0,
                      "{3|2|{6|9}}hz choice 3");
  require(result.ast.unit == lang::ClockUnit::Hertz,
          "{3|2|{6|9}}hz unit");

  result = lang::parseClockLiteral("{x|y}@4");
  require(result.ok(), "{x|y}@4 ast parses");
  require(result.ast.kind == lang::ClockLiteralKind::RandomRange,
          "{x|y}@4 random kind");
  require(result.ast.randomChoices.size() == 2, "{x|y}@4 choice count");
  requireRandomChoiceExternalClock(result.ast, 0, 'x', "{x|y}@4 choice 0");
  requireRandomChoiceExternalClock(result.ast, 1, 'y', "{x|y}@4 choice 1");

  result = lang::parseClockLiteral("{295bpm#3|195bpm@1s}");
  require(result.ok(), "{295bpm#3|195bpm@1s} ast parses");
  require(result.ast.kind == lang::ClockLiteralKind::RandomRange,
          "{295bpm#3|195bpm@1s} random kind");
  require(result.ast.randomChoices.size() == 2,
          "{295bpm#3|195bpm@1s} choice count");
  requireRandomChoice(result.ast, 0, 295.0, 295.0,
                      "{295bpm#3|195bpm@1s} choice 0");
  requireRandomChoiceUnit(result.ast, 0, lang::ClockUnit::Bpm,
                          "{295bpm#3|195bpm@1s} choice 0 unit");
  require(result.ast.randomChoices[0].hasTotalDurationModifier,
          "{295bpm#3|195bpm@1s} choice 0 total");
  require(result.ast.randomChoices[0].totalDurationIsTickCount,
          "{295bpm#3|195bpm@1s} choice 0 total ticks");
  require(result.ast.randomChoices[0].totalDurationTicks == 3,
          "{295bpm#3|195bpm@1s} choice 0 total count");
  requireRange(result.ast.randomChoices[0].totalDurationRange, 7, 9,
               "{295bpm#3|195bpm@1s} choice 0 total range");
  requireRandomChoice(result.ast, 1, 195.0, 195.0,
                      "{295bpm#3|195bpm@1s} choice 1");
  requireRandomChoiceUnit(result.ast, 1, lang::ClockUnit::Bpm,
                          "{295bpm#3|195bpm@1s} choice 1 unit");
  require(result.ast.randomChoices[1].hasRepeatModifier,
          "{295bpm#3|195bpm@1s} choice 1 repeat");
  require(result.ast.randomChoices[1].repeatIsDuration,
          "{295bpm#3|195bpm@1s} choice 1 repeat duration");
  requireNear(result.ast.randomChoices[1].repeatDurationSeconds, 1.0,
              "{295bpm#3|195bpm@1s} choice 1 duration seconds");
  requireRange(result.ast.randomChoices[1].repeatRange, 16, 19,
               "{295bpm#3|195bpm@1s} choice 1 repeat range");

  result = lang::parseClockLiteral("{1.33hz|~2.0hz|4.25hz}");
  require(result.ok(), "{1.33hz|~2.0hz|4.25hz} ast parses");
  require(result.ast.randomChoices.size() == 3,
          "{1.33hz|~2.0hz|4.25hz} choice count");
  requireRandomChoice(result.ast, 1, 2.0, 2.0,
                      "{1.33hz|~2.0hz|4.25hz} rest choice value");
  requireRandomChoiceUnit(result.ast, 1, lang::ClockUnit::Hertz,
                          "{1.33hz|~2.0hz|4.25hz} rest choice unit");
  requireRandomChoiceRest(result.ast, 1,
                          "{1.33hz|~2.0hz|4.25hz} rest choice");

  result = lang::parseClockLiteral(
      "{{3.3333hz|~4.hz}|{1.0hz|1.75hz|3.333333hz}|{1.75hz|~1.5hz|"
      "4.25hz}}");
  require(result.ok(), "nested random rest choices parse");
  require(result.ast.randomChoices.size() == 8,
          "nested random rest choice count");
  requireRandomChoiceRest(result.ast, 1, "nested random first rest choice");
  requireRandomChoiceRest(result.ast, 6, "nested random second rest choice");
}

void testProgramAst() {
  lang::ParseResult result = lang::parseClockLiteral("90bpm 55bpm");
  require(result.ok(), "90bpm 55bpm program parses");
  require(result.program.blocks.size() == 2, "90bpm 55bpm block count");
  requireNear(result.program.blocks[0].literal.value, 90.0,
              "90bpm 55bpm first value");
  requireNear(result.program.blocks[1].literal.value, 55.0,
              "90bpm 55bpm second value");
  require(result.program.blocks[0].repeat == 1, "90bpm repeat");
  require(result.program.blocks[1].repeat == 1, "55bpm repeat");

  result = lang::parseClockLiteral("90bpm@4 2hz@5");
  require(result.ok(), "90bpm@4 2hz@5 program parses");
  require(result.program.blocks.size() == 2, "90bpm@4 2hz@5 block count");
  require(result.program.blocks[0].repeat == 4, "90bpm@4 repeat");
  require(result.program.blocks[1].repeat == 5, "2hz@5 repeat");
  requireRange(result.program.blocks[0].literal.range, 0, 5,
               "90bpm@4 first literal range");
  requireRange(result.program.blocks[0].repeatRange, 5, 7,
               "90bpm@4 first repeat range");
  requireRange(result.program.blocks[1].literal.range, 8, 11,
               "2hz@5 second literal range");
  requireRange(result.program.blocks[1].repeatRange, 11, 13,
               "2hz@5 second repeat range");

  result = lang::parseClockLiteral("[120 90 133.3]@6");
  require(result.ok(), "[120 90 133.3]@6 program parses");
  require(result.program.blocks.size() == 3, "[120 90 133.3]@6 block count");
  requireNear(result.program.blocks[0].literal.value, 120.0,
              "[120 90 133.3]@6 first value");
  requireNear(result.program.blocks[1].literal.value, 90.0,
              "[120 90 133.3]@6 second value");
  requireNear(result.program.blocks[2].literal.value, 133.3,
              "[120 90 133.3]@6 third value");
  require(result.program.blocks[0].repeat == 6, "[120]@6 repeat");
  require(result.program.blocks[1].repeat == 6, "[90]@6 repeat");
  require(result.program.blocks[2].repeat == 6, "[133.3]@6 repeat");
  requireRange(result.program.blocks[0].literal.range, 1, 4,
               "[120 90 133.3]@6 first literal range");
  requireRange(result.program.blocks[1].literal.range, 5, 7,
               "[120 90 133.3]@6 second literal range");
  requireRange(result.program.blocks[2].literal.range, 8, 13,
               "[120 90 133.3]@6 third literal range");
  requireRange(result.program.blocks[0].repeatRange, 14, 16,
               "[120 90 133.3]@6 first repeat range");
  requireRange(result.program.blocks[1].repeatRange, 14, 16,
               "[120 90 133.3]@6 second repeat range");
  requireRange(result.program.blocks[2].repeatRange, 14, 16,
               "[120 90 133.3]@6 third repeat range");

  result = lang::parseClockLiteral("[244 [9hz,8hz]]@9");
  require(result.ok(), "[244 [9hz,8hz]]@9 program parses");
  require(result.program.blocks.size() == 3, "[244 [9hz,8hz]]@9 block count");
  requireNear(result.program.blocks[0].literal.value, 244.0,
              "[244 [9hz,8hz]]@9 first value");
  requireNear(result.program.blocks[1].literal.value, 9.0,
              "[244 [9hz,8hz]]@9 second value");
  requireNear(result.program.blocks[2].literal.value, 8.0,
              "[244 [9hz,8hz]]@9 third value");
  require(result.program.blocks[0].literal.unit == lang::ClockUnit::Bpm,
          "[244 [9hz,8hz]]@9 first unit defaults bpm");
  require(result.program.blocks[1].literal.unit == lang::ClockUnit::Hertz,
          "[244 [9hz,8hz]]@9 second unit hz");
  require(result.program.blocks[2].literal.unit == lang::ClockUnit::Hertz,
          "[244 [9hz,8hz]]@9 third unit hz");
  require(result.program.blocks[0].repeat == 9,
          "[244 [9hz,8hz]]@9 first repeat");
  require(result.program.blocks[1].repeat == 9,
          "[244 [9hz,8hz]]@9 second repeat");
  require(result.program.blocks[2].repeat == 9,
          "[244 [9hz,8hz]]@9 third repeat");
  requireRange(result.program.blocks[0].literal.range, 1, 4,
               "[244 [9hz,8hz]]@9 first literal range");
  requireRange(result.program.blocks[1].literal.range, 6, 9,
               "[244 [9hz,8hz]]@9 second literal range");
  requireRange(result.program.blocks[2].literal.range, 10, 13,
               "[244 [9hz,8hz]]@9 third literal range");
  requireRange(result.program.blocks[0].repeatRange, 15, 17,
               "[244 [9hz,8hz]]@9 first repeat range");
  requireRange(result.program.blocks[1].repeatRange, 15, 17,
               "[244 [9hz,8hz]]@9 second repeat range");
  requireRange(result.program.blocks[2].repeatRange, 15, 17,
               "[244 [9hz,8hz]]@9 third repeat range");

  result = lang::parseClockLiteral("188? 188?50 188?100 188?0");
  require(result.ok(), "probability program parses");
  require(result.program.blocks.size() == 4, "probability block count");
  require(result.program.blocks[0].probability == 50,
          "bare question probability defaults 50");
  require(result.program.blocks[1].probability == 50,
          "explicit probability 50");
  require(result.program.blocks[2].probability == 100,
          "explicit probability 100");
  require(result.program.blocks[3].probability == 0,
          "explicit probability 0");
  requireRange(result.program.blocks[0].probabilityRange, 3, 4,
               "bare question probability range");
  requireRange(result.program.blocks[1].probabilityRange, 8, 11,
               "explicit probability range");

  result = lang::parseClockLiteral("[120?50,100?0]@4");
  require(result.ok(), "bracket probability program parses");
  require(result.program.blocks.size() == 2, "bracket probability count");
  require(result.program.blocks[0].probability == 50,
          "bracket first probability");
  require(result.program.blocks[1].probability == 0,
          "bracket second probability");
  require(result.program.blocks[0].repeat == 4, "bracket first repeat");
  require(result.program.blocks[1].repeat == 4, "bracket second repeat");

  result = lang::parseClockLiteral("120@2s 90@3sec 60@250ms");
  require(result.ok(), "duration repeat program parses");
  require(result.program.blocks.size() == 3, "duration repeat block count");
  requireDurationRepeat(result.program.blocks[0], 2.0,
                        lang::ClockUnit::Seconds, 3, 6,
                        "120@2s duration repeat");
  requireDurationRepeat(result.program.blocks[1], 3.0,
                        lang::ClockUnit::Seconds, 9, 14,
                        "90@3sec duration repeat");
  requireDurationRepeat(result.program.blocks[2], 250.0,
                        lang::ClockUnit::Milliseconds, 17, 23,
                        "60@250ms duration repeat");

  result = lang::parseClockLiteral("[120 90]@2s");
  require(result.ok(), "bracket duration repeat parses");
  require(result.program.blocks.size() == 2, "bracket duration repeat count");
  requireDurationRepeat(result.program.blocks[0], 2.0,
                        lang::ClockUnit::Seconds, 8, 11,
                        "bracket first duration repeat");
  requireDurationRepeat(result.program.blocks[1], 2.0,
                        lang::ClockUnit::Seconds, 8, 11,
                        "bracket second duration repeat");

  result = lang::parseClockLiteral("[2s 1s .4s .6s]@4s");
  require(result.ok(), "bracket per-item duration repeat parses");
  require(result.program.blocks.size() == 4,
          "bracket per-item duration repeat count");
  for (size_t i = 0; i < result.program.blocks.size(); i++) {
    requireDurationRepeat(result.program.blocks[i], 4.0,
                          lang::ClockUnit::Seconds, 15, 18,
                          "bracket per-item duration repeat value");
  }

  result = lang::parseClockLiteral("2hz@{5|9}");
  require(result.ok(), "random repeat count parses");
  require(result.program.blocks.size() == 1, "random repeat count block count");
  require(result.program.blocks[0].repeatIsRandom,
          "random repeat count marked random");
  require(!result.program.blocks[0].repeatIsDuration,
          "random repeat count is not duration");
  requireRange(result.program.blocks[0].repeatRange, 3, 9,
               "random repeat count range");
  require(result.program.blocks[0].repeatRandom.randomChoices.size() == 2,
          "random repeat count choices");
  requireRandomChoice(result.program.blocks[0].repeatRandom, 0, 5.0, 5.0,
                      "random repeat count choice 0");
  requireRandomChoice(result.program.blocks[0].repeatRandom, 1, 9.0, 9.0,
                      "random repeat count choice 1");

  result = lang::parseClockLiteral("[.4s .7s?]@{2-4}s");
  require(result.ok(), "random repeat duration parses");
  require(result.program.blocks.size() == 2,
          "random repeat duration block count");
  for (size_t i = 0; i < result.program.blocks.size(); i++) {
    require(result.program.blocks[i].repeatIsRandom,
            "random repeat duration marked random");
    require(result.program.blocks[i].repeatIsDuration,
            "random repeat duration marked duration");
    requireRange(result.program.blocks[i].repeatRange, 10, 17,
                 "random repeat duration range");
    require(result.program.blocks[i].repeatRandom.randomChoices.size() == 1,
            "random repeat duration choices");
    requireRandomChoice(result.program.blocks[i].repeatRandom, 0, 2.0, 4.0,
                        "random repeat duration choice");
    requireRandomChoiceUnit(result.program.blocks[i].repeatRandom, 0,
                            lang::ClockUnit::Seconds,
                            "random repeat duration unit");
  }

  result = lang::parseClockLiteral("[2s 1s .4s .6s]#4s");
  require(result.ok(), "bracket total duration parses");
  require(result.program.blocks.size() == 4, "bracket total duration count");
  for (size_t i = 0; i < result.program.blocks.size(); i++) {
    requireTotalDurationGroup(result.program.blocks[i], 0, 4.0,
                              lang::ClockUnit::Seconds, 15, 18,
                              "bracket total duration group");
    require(!result.program.blocks[i].repeatIsDuration,
            "bracket total duration keeps child duration");
  }

  result = lang::parseClockLiteral("(3hz 4hz)#9s");
  require(result.ok(), "paren total duration parses");
  require(result.program.blocks.size() == 2, "paren total duration count");
  requireNear(result.program.blocks[0].literal.value, 3.0,
              "paren total duration first value");
  requireNear(result.program.blocks[1].literal.value, 4.0,
              "paren total duration second value");
  requireTotalDurationGroup(result.program.blocks[0], 0, 9.0,
                            lang::ClockUnit::Seconds, 9, 12,
                            "paren total duration first group");
  requireTotalDurationGroup(result.program.blocks[1], 0, 9.0,
                            lang::ClockUnit::Seconds, 9, 12,
                            "paren total duration second group");
  require(!result.program.blocks[0].repeatIsDuration,
          "paren total duration first keeps normal timing");
  require(!result.program.blocks[1].repeatIsDuration,
          "paren total duration second keeps normal timing");

  result = lang::parseClockLiteral("2hz (3hz 4hz)#9s");
  require(result.ok(), "preceded paren total duration parses");
  requireProgramValues(result, {2.0, 3.0, 4.0},
                       "preceded paren total duration values");
  require(!result.program.blocks[0].hasTotalDuration,
          "preceded paren total duration first outside group");
  requireTotalDurationGroup(result.program.blocks[1], 0, 9.0,
                            lang::ClockUnit::Seconds, 13, 16,
                            "preceded paren total duration first group");
  requireTotalDurationGroup(result.program.blocks[2], 0, 9.0,
                            lang::ClockUnit::Seconds, 13, 16,
                            "preceded paren total duration second group");

  result = lang::parseClockLiteral("(3hz 4hz)#9");
  require(result.ok(), "paren total tick count parses");
  require(result.program.blocks.size() == 2, "paren total tick count count");
  requireTotalTickGroup(result.program.blocks[0], 0, 9, 9, 11,
                        "paren total tick count first group");
  requireTotalTickGroup(result.program.blocks[1], 0, 9, 9, 11,
                        "paren total tick count second group");
  require(!result.program.blocks[0].repeatIsDuration,
          "paren total tick count first keeps normal timing");
  require(!result.program.blocks[1].repeatIsDuration,
          "paren total tick count second keeps normal timing");

  result = lang::parseClockLiteral("2hz (3hz 4hz)#9");
  require(result.ok(), "preceded paren total tick count parses");
  requireProgramValues(result, {2.0, 3.0, 4.0},
                       "preceded paren total tick count values");
  require(!result.program.blocks[0].hasTotalDuration,
          "preceded paren total tick count first outside group");
  requireTotalTickGroup(result.program.blocks[1], 0, 9, 13, 15,
                        "preceded paren total tick count first group");
  requireTotalTickGroup(result.program.blocks[2], 0, 9, 13, 15,
                        "preceded paren total tick count second group");

  result = lang::parseClockLiteral("2hz#{5|9}");
  require(result.ok(), "random total tick count parses");
  require(result.program.blocks.size() == 1,
          "random total tick count block count");
  require(result.program.blocks[0].totalDurationIsRandom,
          "random total tick count marked random");
  require(result.program.blocks[0].totalDurationRandom.randomChoices.size() ==
              2,
          "random total tick count choices");
  requireRange(result.program.blocks[0].totalDurationRange, 3, 9,
               "random total tick count range");
  requireRandomChoice(result.program.blocks[0].totalDurationRandom, 0, 5.0,
                      5.0, "random total tick count choice 0");
  requireRandomChoice(result.program.blocks[0].totalDurationRandom, 1, 9.0,
                      9.0, "random total tick count choice 1");

  result = lang::parseClockLiteral("[3hz 2hz]#{9s|14}");
  require(result.ok(), "mixed random total duration parses");
  require(result.program.blocks.size() == 2,
          "mixed random total duration block count");
  for (size_t i = 0; i < result.program.blocks.size(); i++) {
    require(result.program.blocks[i].totalDurationIsRandom,
            "mixed random total duration marked random");
    require(result.program.blocks[i].totalDurationRandom.randomChoices.size() ==
                2,
            "mixed random total duration choices");
    requireRange(result.program.blocks[i].totalDurationRange, 9, 17,
                 "mixed random total duration range");
    requireRandomChoice(result.program.blocks[i].totalDurationRandom, 0, 9.0,
                        9.0, "mixed random total duration seconds choice");
    requireRandomChoiceUnit(result.program.blocks[i].totalDurationRandom, 0,
                            lang::ClockUnit::Seconds,
                            "mixed random total duration seconds unit");
    requireRandomChoice(result.program.blocks[i].totalDurationRandom, 1, 14.0,
                        14.0, "mixed random total duration tick choice");
  }

  result = lang::parseClockLiteral("({3|2|{6|9}}hz@7 ~(1 2 3)s)#12s");
  require(result.ok(), "nested random rest total duration parses");
  require(result.program.blocks.size() == 6,
          "nested random rest total duration count");
  for (size_t i = 0; i < result.program.blocks.size(); i++) {
    requireTotalDurationGroup(result.program.blocks[i], 0, 12.0,
                              lang::ClockUnit::Seconds, 27, 31,
                              "nested random rest total duration group");
  }

  result = lang::parseClockLiteral("2hz@4 ~3hz@4 ~@3s");
  require(result.ok(), "rest program parses");
  require(result.program.blocks.size() == 3, "rest block count");
  require(!result.program.blocks[0].rest, "first block is not rest");
  require(result.program.blocks[1].rest, "clocked rest block");
  require(result.program.blocks[1].repeat == 4, "clocked rest repeat");
  requireRange(result.program.blocks[1].restRange, 6, 7,
               "clocked rest range");
  requireRange(result.program.blocks[1].range, 6, 12,
               "clocked rest block range");
  require(result.program.blocks[2].rest, "duration-only rest block");
  require(result.program.blocks[2].literal.kind ==
              lang::ClockLiteralKind::Empty,
          "duration-only rest has no literal");
  requireDurationRepeat(result.program.blocks[2], 3.0,
                        lang::ClockUnit::Seconds, 14, 17,
                        "duration-only rest duration");

  result = lang::parseClockLiteral("~[120 90]@2");
  require(result.ok(), "rest bracket parses");
  require(result.program.blocks.size() == 2, "rest bracket count");
  require(result.program.blocks[0].rest, "rest bracket first block");
  require(result.program.blocks[1].rest, "rest bracket second block");
  require(result.program.blocks[0].repeat == 2, "rest bracket first repeat");
  require(result.program.blocks[1].repeat == 2, "rest bracket second repeat");

  result = lang::parseClockLiteral("({3|2|{6|9}}hz@7 ~(1 2 3)s)");
  require(result.ok(), "nested random rest interleave parses");
  require(result.program.blocks.size() == 6,
          "nested random rest interleave block count");
  for (size_t i = 0; i < result.program.blocks.size(); i += 2) {
    require(result.program.blocks[i].literal.kind ==
                lang::ClockLiteralKind::RandomRange,
            "nested random rest interleave random kind");
    require(result.program.blocks[i].literal.randomChoices.size() == 4,
            "nested random rest interleave random choices");
    require(result.program.blocks[i].repeat == 7,
            "nested random rest interleave repeat");
    require(!result.program.blocks[i].rest,
            "nested random rest interleave random not rest");
  }
  for (size_t i = 1; i < result.program.blocks.size(); i += 2) {
    require(result.program.blocks[i].rest,
            "nested random rest interleave rest block");
    require(result.program.blocks[i].literal.unit == lang::ClockUnit::Seconds,
            "nested random rest interleave rest seconds");
  }

  result = lang::parseClockLiteral("2hz (2hz 4hz)");
  require(result.ok(), "interleave program parses");
  requireProgramValues(result, {2.0, 2.0, 2.0, 4.0},
                       "interleave block values");
  require(result.program.blocks.size() == 4, "interleave block count");
  requireNear(result.program.blocks[0].literal.value, 2.0,
              "interleave block 0 value");
  requireNear(result.program.blocks[1].literal.value, 2.0,
              "interleave block 1 value");
  requireNear(result.program.blocks[2].literal.value, 2.0,
              "interleave block 2 value");
  requireNear(result.program.blocks[3].literal.value, 4.0,
              "interleave block 3 value");
  requireRange(result.program.blocks[0].literal.range, 0, 3,
               "interleave block 0 range");
  requireRange(result.program.blocks[1].literal.range, 5, 8,
               "interleave block 1 range");
  requireRange(result.program.blocks[2].literal.range, 0, 3,
               "interleave block 2 range");
  requireRange(result.program.blocks[3].literal.range, 9, 12,
               "interleave block 3 range");

  result = lang::parseClockLiteral("(1hz 2hz)");
  require(result.ok(), "outer paren group parses");
  require(result.program.blocks.size() == 2, "outer paren group block count");
  requireNear(result.program.blocks[0].literal.value, 1.0,
              "outer paren first value");
  requireNear(result.program.blocks[1].literal.value, 2.0,
              "outer paren second value");
  require(result.program.blocks[0].literal.unit == lang::ClockUnit::Hertz,
          "outer paren first unit");
  require(result.program.blocks[1].literal.unit == lang::ClockUnit::Hertz,
          "outer paren second unit");

  result = lang::parseClockLiteral("(2 1)hz");
  require(result.ok(), "outer paren suffix hz parses");
  require(result.program.blocks.size() == 2, "outer paren suffix block count");
  requireNear(result.program.blocks[0].literal.value, 2.0,
              "outer paren suffix first value");
  requireNear(result.program.blocks[1].literal.value, 1.0,
              "outer paren suffix second value");
  require(result.program.blocks[0].literal.unit == lang::ClockUnit::Hertz,
          "outer paren suffix first hz");
  require(result.program.blocks[1].literal.unit == lang::ClockUnit::Hertz,
          "outer paren suffix second hz");

  result = lang::parseClockLiteral("(2 1 (3 2)@2)hz");
  require(result.ok(), "nested paren suffix repeat parses");
  requireProgramValues(result, {2.0, 1.0, 3.0, 2.0, 1.0, 2.0},
                       "nested paren suffix repeat values");
  requireNear(result.program.blocks[0].literal.value, 2.0,
              "nested paren suffix repeat block 0 value");
  requireNear(result.program.blocks[1].literal.value, 1.0,
              "nested paren suffix repeat block 1 value");
  requireNear(result.program.blocks[2].literal.value, 3.0,
              "nested paren suffix repeat block 2 value");
  requireNear(result.program.blocks[3].literal.value, 2.0,
              "nested paren suffix repeat block 3 value");
  requireNear(result.program.blocks[4].literal.value, 1.0,
              "nested paren suffix repeat block 4 value");
  requireNear(result.program.blocks[5].literal.value, 2.0,
              "nested paren suffix repeat block 5 value");
  require(result.program.blocks[0].literal.unit == lang::ClockUnit::Hertz,
          "nested paren suffix repeat block 0 hz");
  require(result.program.blocks[1].literal.unit == lang::ClockUnit::Hertz,
          "nested paren suffix repeat block 1 hz");
  require(result.program.blocks[2].literal.unit == lang::ClockUnit::Hertz,
          "nested paren suffix repeat block 2 hz");
  require(result.program.blocks[3].literal.unit == lang::ClockUnit::Hertz,
          "nested paren suffix repeat block 3 hz");
  require(result.program.blocks[4].literal.unit == lang::ClockUnit::Hertz,
          "nested paren suffix repeat block 4 hz");
  require(result.program.blocks[5].literal.unit == lang::ClockUnit::Hertz,
          "nested paren suffix repeat block 5 hz");
  require(result.program.blocks[2].repeat == 2,
          "nested paren suffix repeat block 2 repeat");
  require(result.program.blocks[5].repeat == 2,
          "nested paren suffix repeat block 5 repeat");

  result = lang::parseClockLiteral("(3 4 (5 4 3))hz");
  require(result.ok(), "spread interleave parses");
  requireProgramValues(result, {3.0, 4.0, 5.0, 3.0, 4.0, 4.0, 3.0, 4.0,
                                3.0},
                       "spread interleave values");
  for (size_t i = 0; i < result.program.blocks.size(); i++) {
    require(result.program.blocks[i].literal.unit == lang::ClockUnit::Hertz,
            "spread interleave unit");
  }

  result = lang::parseClockLiteral("(3 3 (2 1 5))hz?69");
  require(result.ok(), "spread interleave suffix probability parses");
  requireProgramValues(result, {3.0, 3.0, 2.0, 3.0, 3.0, 1.0, 3.0, 3.0,
                                5.0},
                       "spread interleave suffix probability values");
  for (size_t i = 0; i < result.program.blocks.size(); i++) {
    require(result.program.blocks[i].literal.unit == lang::ClockUnit::Hertz,
            "spread interleave suffix probability unit");
    require(result.program.blocks[i].probability == 69,
            "spread interleave suffix probability value");
    requireRange(result.program.blocks[i].probabilityRange, 15, 18,
                 "spread interleave suffix probability range");
  }

  result = lang::parseClockLiteral("(3@2 (3 1)@2 (2 1 5))hz?80");
  require(result.ok(), "spread interleave suffix probability parses");
  requireProgramValues(result, {3.0, 3.0, 2.0, 3.0, 1.0, 1.0, 3.0, 3.0,
                                5.0},
                       "spread interleave suffix probability values");
  for (size_t i = 0; i < result.program.blocks.size(); i++) {
    require(result.program.blocks[i].literal.unit == lang::ClockUnit::Hertz,
            "spread interleave suffix probability unit");
    require(result.program.blocks[i].probability == 80,
            "spread interleave suffix probability value");
    requireRange(result.program.blocks[i].probabilityRange, 23, 26,
                 "spread interleave suffix probability range");
  }
  require(result.program.blocks[0].repeat == 2,
          "spread interleave suffix probability block 0 repeat");
  require(result.program.blocks[1].repeat == 2,
          "spread interleave suffix probability block 1 repeat");
  require(result.program.blocks[2].repeat == 1,
          "spread interleave suffix probability block 2 repeat");
  require(result.program.blocks[3].repeat == 2,
          "spread interleave suffix probability block 3 repeat");
  require(result.program.blocks[4].repeat == 2,
          "spread interleave suffix probability block 4 repeat");
  require(result.program.blocks[5].repeat == 1,
          "spread interleave suffix probability block 5 repeat");
  require(result.program.blocks[6].repeat == 2,
          "spread interleave suffix probability block 6 repeat");
  require(result.program.blocks[7].repeat == 2,
          "spread interleave suffix probability block 7 repeat");
  require(result.program.blocks[8].repeat == 1,
          "spread interleave suffix probability block 8 repeat");

  result = lang::parseClockLiteral("(3 (5 4 3))hz");
  require(result.ok(), "single lane spread interleave parses");
  requireProgramValues(result, {3.0, 5.0, 3.0, 4.0, 3.0, 3.0},
                       "single lane spread interleave values");

  result = lang::parseClockLiteral("((1 2) (3 4 5))hz");
  require(result.ok(), "two nested lane spread interleave parses");
  requireProgramValues(result, {1.0, 3.0, 2.0, 4.0, 1.0, 5.0},
                       "two nested lane spread interleave values");

  result = lang::parseClockLiteral("([1 2] (3 4 5))hz");
  require(result.ok(), "bracket lane spread interleave parses");
  requireProgramValues(result, {1.0, 3.0, 2.0, 4.0, 1.0, 5.0},
                       "bracket lane spread interleave values");

  result = lang::parseClockLiteral("120 (3hz [4hz 5hz])");
  require(result.ok(), "nested interleave program parses");
  require(result.program.blocks.size() == 6, "nested interleave block count");
  requireRange(result.program.blocks[0].literal.range, 0, 3,
               "nested interleave block 0 range");
  requireRange(result.program.blocks[1].literal.range, 5, 8,
               "nested interleave block 1 range");
  requireRange(result.program.blocks[2].literal.range, 0, 3,
               "nested interleave block 2 range");
  requireRange(result.program.blocks[3].literal.range, 10, 13,
               "nested interleave block 3 range");
  requireRange(result.program.blocks[4].literal.range, 0, 3,
               "nested interleave block 4 range");
  requireRange(result.program.blocks[5].literal.range, 14, 17,
               "nested interleave block 5 range");

  result = lang::parseClockLiteral("[3 2 1]s");
  require(result.ok(), "bracket suffix seconds parses");
  require(result.program.blocks.size() == 3, "bracket suffix count");
  require(result.program.blocks[0].literal.unit == lang::ClockUnit::Seconds,
          "bracket suffix first seconds");
  require(result.program.blocks[1].literal.unit == lang::ClockUnit::Seconds,
          "bracket suffix second seconds");
  require(result.program.blocks[2].literal.unit == lang::ClockUnit::Seconds,
          "bracket suffix third seconds");
  requireRange(result.program.blocks[0].literal.range, 1, 2,
               "bracket suffix first range");

  result = lang::parseClockLiteral("[3 2hz 1]s");
  require(result.ok(), "mixed bracket suffix parses");
  require(result.program.blocks.size() == 3, "mixed bracket suffix count");
  require(result.program.blocks[0].literal.unit == lang::ClockUnit::Seconds,
          "mixed bracket suffix first seconds");
  require(result.program.blocks[1].literal.unit == lang::ClockUnit::Hertz,
          "mixed bracket suffix explicit hz wins");
  require(result.program.blocks[2].literal.unit == lang::ClockUnit::Seconds,
          "mixed bracket suffix third seconds");

  result = lang::parseClockLiteral("[{3-5} 2]hz");
  require(result.ok(), "bracket suffix random range parses");
  require(result.program.blocks.size() == 2,
          "bracket suffix random range count");
  require(result.program.blocks[0].literal.kind ==
              lang::ClockLiteralKind::RandomRange,
          "bracket suffix random range kind");
  require(result.program.blocks[0].literal.unit == lang::ClockUnit::Hertz,
          "bracket suffix random range hz");
  require(result.program.blocks[1].literal.unit == lang::ClockUnit::Hertz,
          "bracket suffix numeric hz");

  result = lang::parseClockLiteral(
      "[{(1.75 ~4.66 1.33 ~3. 2.666666 ~5)|(~4.75 ~2 ~1 "
      "1.75)}]hz#7");
  require(result.ok(), "random sequence choices parse");
  require(result.program.blocks.size() == 6,
          "random sequence choice block count");
  for (size_t i = 0; i < result.program.blocks.size(); i++) {
    require(result.program.blocks[i].literal.kind ==
                lang::ClockLiteralKind::RandomRange,
            "random sequence choice literal kind");
    require(result.program.blocks[i].literal.randomChoices.size() == 2,
            "random sequence choice count");
    require(result.program.blocks[i].literal.unit == lang::ClockUnit::Hertz,
            "random sequence choice suffix unit");
    requireTotalTickGroup(result.program.blocks[i], 0, 7, 58, 60,
                          "random sequence total tick group");
  }
  requireRandomChoice(result.program.blocks[0].literal, 0, 1.75, 1.75,
                      "random sequence row 0 choice 0");
  requireRandomChoice(result.program.blocks[0].literal, 1, 4.75, 4.75,
                      "random sequence row 0 choice 1");
  requireRandomChoiceRest(result.program.blocks[0].literal, 1,
                          "random sequence row 0 rest choice");
  requireRandomChoice(result.program.blocks[4].literal, 0, 2.666666,
                      2.666666, "random sequence row 4 choice 0");
  requireRandomChoice(result.program.blocks[4].literal, 1, 4.75, 4.75,
                      "random sequence row 4 wrapped choice 1");

  result = lang::parseClockLiteral("[x y]@8");
  require(result.ok(), "external clock bracket parses");
  require(result.program.blocks.size() == 2, "external clock block count");
  require(result.program.blocks[0].literal.kind ==
              lang::ClockLiteralKind::ExternalClock,
          "external clock first kind");
  require(result.program.blocks[0].literal.externalClock == 'x',
          "external clock first id");
  require(result.program.blocks[0].repeat == 8, "external clock first repeat");
  require(result.program.blocks[1].literal.kind ==
              lang::ClockLiteralKind::ExternalClock,
          "external clock second kind");
  require(result.program.blocks[1].literal.externalClock == 'y',
          "external clock second id");
  require(result.program.blocks[1].repeat == 8, "external clock second repeat");

  result = lang::parseClockLiteral("x@3s");
  require(result.ok(), "external clock duration repeat parses");
  require(result.program.blocks.size() == 1,
          "external clock duration repeat block count");
  require(result.program.blocks[0].literal.kind ==
              lang::ClockLiteralKind::ExternalClock,
          "external clock duration repeat kind");
  require(result.program.blocks[0].literal.externalClock == 'x',
          "external clock duration repeat id");
  requireDurationRepeat(result.program.blocks[0], 3.0,
                        lang::ClockUnit::Seconds, 1, 4,
                        "external clock duration repeat range");

  result = lang::parseClockLiteral("x@5 y@3s");
  require(result.ok(), "external clock count then duration parses");
  require(result.program.blocks.size() == 2,
          "external clock count then duration block count");
  require(result.program.blocks[0].literal.kind ==
              lang::ClockLiteralKind::ExternalClock,
          "external clock count then duration first kind");
  require(result.program.blocks[0].literal.externalClock == 'x',
          "external clock count then duration first id");
  require(result.program.blocks[0].repeat == 5,
          "external clock count then duration first repeat");
  require(result.program.blocks[1].literal.kind ==
              lang::ClockLiteralKind::ExternalClock,
          "external clock count then duration second kind");
  require(result.program.blocks[1].literal.externalClock == 'y',
          "external clock count then duration second id");
  requireDurationRepeat(result.program.blocks[1], 3.0,
                        lang::ClockUnit::Seconds, 5, 8,
                        "external clock count then duration second repeat");

  result = lang::parseClockLiteral("3hz@6x");
  require(result.ok(), "repeat counted by external clock parses");
  require(result.program.blocks.size() == 1,
          "repeat counted by external clock block count");
  requireNear(result.program.blocks[0].literal.value, 3.0,
              "repeat counted by external clock value");
  require(result.program.blocks[0].literal.unit == lang::ClockUnit::Hertz,
          "repeat counted by external clock unit");
  require(result.program.blocks[0].repeat == 6,
          "repeat counted by external clock repeat");
  require(result.program.blocks[0].repeatUsesExternalClock,
          "repeat counted by external clock flag");
  require(result.program.blocks[0].repeatExternalClock == 'x',
          "repeat counted by external clock id");
  requireRange(result.program.blocks[0].repeatRange, 3, 6,
               "repeat counted by external clock range");

  result = lang::parseClockLiteral("x@3x");
  require(result.ok(), "external literal counted by external clock parses");
  require(result.program.blocks.size() == 1,
          "external literal counted by external clock block count");
  require(result.program.blocks[0].literal.kind ==
              lang::ClockLiteralKind::ExternalClock,
          "external literal counted by external clock kind");
  require(result.program.blocks[0].literal.externalClock == 'x',
          "external literal counted by external clock literal id");
  require(result.program.blocks[0].repeat == 3,
          "external literal counted by external clock repeat");
  require(result.program.blocks[0].repeatUsesExternalClock,
          "external literal counted by external clock flag");
  require(result.program.blocks[0].repeatExternalClock == 'x',
          "external literal counted by external clock repeat id");
  requireRange(result.program.blocks[0].repeatRange, 1, 4,
               "external literal counted by external clock repeat range");

  result = lang::parseClockLiteral("3hz@x");
  require(result.ok(), "bare external repeat clock parses");
  require(result.program.blocks.size() == 1,
          "bare external repeat clock block count");
  require(result.program.blocks[0].repeat == 1,
          "bare external repeat clock repeat");
  require(result.program.blocks[0].repeatUsesExternalClock,
          "bare external repeat clock flag");
  require(result.program.blocks[0].repeatExternalClock == 'x',
          "bare external repeat clock id");
  requireRange(result.program.blocks[0].repeatRange, 3, 5,
               "bare external repeat clock range");

  result = lang::parseClockLiteral("3hz#3y");
  require(result.ok(), "total counted by external clock parses");
  require(result.program.blocks.size() == 1,
          "total counted by external clock block count");
  require(result.program.blocks[0].hasTotalDuration,
          "total counted by external clock total flag");
  require(result.program.blocks[0].totalDurationIsTickCount,
          "total counted by external clock tick flag");
  require(result.program.blocks[0].totalDurationTicks == 3,
          "total counted by external clock ticks");
  require(result.program.blocks[0].totalDurationUsesExternalClock,
          "total counted by external clock flag");
  require(result.program.blocks[0].totalDurationExternalClock == 'y',
          "total counted by external clock id");
  requireRange(result.program.blocks[0].totalDurationRange, 3, 6,
               "total counted by external clock range");

  result = lang::parseClockLiteral("3hz#y");
  require(result.ok(), "bare external total clock parses");
  require(result.program.blocks.size() == 1,
          "bare external total clock block count");
  require(result.program.blocks[0].hasTotalDuration,
          "bare external total clock total flag");
  require(result.program.blocks[0].totalDurationIsTickCount,
          "bare external total clock tick flag");
  require(result.program.blocks[0].totalDurationTicks == 1,
          "bare external total clock ticks");
  require(result.program.blocks[0].totalDurationUsesExternalClock,
          "bare external total clock flag");
  require(result.program.blocks[0].totalDurationExternalClock == 'y',
          "bare external total clock id");
  requireRange(result.program.blocks[0].totalDurationRange, 3, 5,
               "bare external total clock range");

  result = lang::parseClockLiteral("[w 122bpm]@3z");
  require(result.ok(), "group repeat counted by external clock parses");
  require(result.program.blocks.size() == 2,
          "group repeat counted by external clock block count");
  require(result.program.blocks[0].literal.kind ==
              lang::ClockLiteralKind::ExternalClock,
          "group repeat counted by external clock first kind");
  require(result.program.blocks[0].literal.externalClock == 'w',
          "group repeat counted by external clock first id");
  requireNear(result.program.blocks[1].literal.value, 122.0,
              "group repeat counted by external clock second value");
  for (int i = 0; i < 2; i++) {
    require(result.program.blocks[i].repeat == 3,
            "group repeat counted by external clock repeat");
    require(result.program.blocks[i].repeatUsesExternalClock,
            "group repeat counted by external clock flag");
    require(result.program.blocks[i].repeatExternalClock == 'z',
            "group repeat counted by external clock id");
  }

  result = lang::parseClockLiteral("[w 122bpm]@z");
  require(result.ok(), "group bare repeat counted by external clock parses");
  require(result.program.blocks.size() == 2,
          "group bare repeat counted by external clock block count");
  for (int i = 0; i < 2; i++) {
    require(result.program.blocks[i].repeat == 1,
            "group bare repeat counted by external clock repeat");
    require(result.program.blocks[i].repeatUsesExternalClock,
            "group bare repeat counted by external clock flag");
    require(result.program.blocks[i].repeatExternalClock == 'z',
            "group bare repeat counted by external clock id");
  }

  result = lang::parseClockLiteral("{x|y}@4");
  require(result.ok(), "external clock random parses");
  require(result.program.blocks.size() == 1, "external clock random count");
  require(result.program.blocks[0].literal.kind ==
              lang::ClockLiteralKind::RandomRange,
          "external clock random kind");
  requireRandomChoiceExternalClock(result.program.blocks[0].literal, 0, 'x',
                                   "external clock random first choice");
  requireRandomChoiceExternalClock(result.program.blocks[0].literal, 1, 'y',
                                   "external clock random second choice");
  require(result.program.blocks[0].repeat == 4,
          "external clock random repeat");
}

void testProgramEvaluation() {
  lang::ParseResult result = lang::parseClockLiteral("90bpm 55bpm");
  require(result.ok(), "90bpm 55bpm evaluates parse");
  lang::EvaluationResult eval =
      lang::evaluateClockLiteral(result.program.blocks[0].literal);
  require(eval.ok(), "90bpm block evaluates");
  requireNear(eval.spec.hz, 1.5, "90bpm block hz");

  eval = lang::evaluateClockLiteral(result.program.blocks[1].literal);
  require(eval.ok(), "55bpm block evaluates");
  requireNear(eval.spec.hz, 55.0 / 60.0, "55bpm block hz");

  result = lang::parseClockLiteral("[120 90 133.3]@6");
  require(result.ok(), "[120 90 133.3]@6 evaluates parse");
  eval = lang::evaluateClockLiteral(result.program.blocks[0].literal);
  require(eval.ok(), "120 bracket block evaluates");
  requireNear(eval.spec.hz, 2.0, "120 bracket block hz");

  eval = lang::evaluateClockLiteral(result.program.blocks[1].literal);
  require(eval.ok(), "90 bracket block evaluates");
  requireNear(eval.spec.hz, 1.5, "90 bracket block hz");

  eval = lang::evaluateClockLiteral(result.program.blocks[2].literal);
  require(eval.ok(), "133.3 bracket block evaluates");
  requireNear(eval.spec.hz, 133.3 / 60.0, "133.3 bracket block hz");

  result = lang::parseClockLiteral("[244 [9hz,8hz]]@9");
  require(result.ok(), "[244 [9hz,8hz]]@9 evaluates parse");
  eval = lang::evaluateClockLiteral(result.program.blocks[0].literal);
  require(eval.ok(), "244 nested block evaluates");
  requireNear(eval.spec.hz, 244.0 / 60.0, "244 nested block hz");

  eval = lang::evaluateClockLiteral(result.program.blocks[1].literal);
  require(eval.ok(), "9hz nested block evaluates");
  requireNear(eval.spec.hz, 9.0, "9hz nested block hz");

  eval = lang::evaluateClockLiteral(result.program.blocks[2].literal);
  require(eval.ok(), "8hz nested block evaluates");
  requireNear(eval.spec.hz, 8.0, "8hz nested block hz");

  result = lang::parseClockLiteral("{3-5}hz");
  require(result.ok(), "{3-5}hz evaluates parse");
  eval = lang::evaluateClockLiteralWithValue(result.ast, 4.0);
  require(eval.ok(), "{3-5}hz sampled value evaluates");
  requireNear(eval.spec.hz, 4.0, "{3-5}hz sampled hz");

  result = lang::parseClockLiteral("{2-4}s");
  require(result.ok(), "{2-4}s evaluates parse");
  eval = lang::evaluateClockLiteralWithValue(result.ast, 3.0);
  require(eval.ok(), "{2-4}s sampled value evaluates");
  requireNear(eval.spec.periodSeconds, 3.0, "{2-4}s sampled period");

  result = lang::parseClockLiteral("{4-2}s");
  require(result.ok(), "{4-2}s evaluates parse");
  eval = lang::evaluateClockLiteralWithValue(result.ast, 3.0);
  require(eval.ok(), "{4-2}s sampled value evaluates");
  requireNear(eval.spec.periodSeconds, 3.0, "{4-2}s sampled period");

  result = lang::parseClockLiteral("{3|2-10}hz");
  require(result.ok(), "{3|2-10}hz evaluates parse");
  eval = lang::evaluateClockLiteralWithValue(result.ast, 7.0);
  require(eval.ok(), "{3|2-10}hz sampled value evaluates");
  requireNear(eval.spec.hz, 7.0, "{3|2-10}hz sampled hz");
}

void testEvaluator() {
  lang::ClockSpec spec = requireEvaluates("120");
  requireNear(spec.hz, 2.0, "120 unitless hz");
  requireNear(spec.periodSeconds, 0.5, "120 unitless period");

  spec = requireEvaluates("120bpm");
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
  requireInvalid("120bananas", "unknown unit invalid");
  requireInvalid("1.2.3hz", "malformed number invalid");
  requireInvalid("3.5:23 minutes", "decimal colon minutes invalid");
  requireInvalid("3:90 minutes", "colon seconds >= 60 invalid");
  requireInvalid("120bpm !", "trailing junk invalid");
  requireInvalid("hz", "missing number invalid");
  requireInvalid("120bpm@1.5", "decimal repeat invalid");
  requireInvalid("120bpm@2hz", "frequency repeat duration invalid");
  requireInvalid("120bpm@2bpm", "bpm repeat duration invalid");
  requireInvalid("120bpm@0s", "zero repeat duration invalid");
  requireInvalid("120bpm@{1.5|2}", "decimal random repeat count invalid");
  requireInvalid("120bpm@{0|2}", "zero random repeat count invalid");
  requireInvalid("120bpm@{2hz|3hz}", "frequency random repeat invalid");
  requireInvalid("(3hz 4hz)#1.5", "decimal total tick count invalid");
  requireInvalid("(3hz 4hz)#0", "zero total tick count invalid");
  requireInvalid("(3hz 4hz)#{1.5|2}",
                 "decimal random total tick count invalid");
  requireInvalid("(3hz 4hz)#{0|2}", "zero random total tick count invalid");
  requireInvalid("(3hz 4hz)#9hz", "frequency total duration invalid");
  requireInvalid("(3hz 4hz)#{9hz|14}", "frequency random total invalid");
  requireInvalid("(3hz 4hz)#0s", "zero total duration invalid");
  requireInvalid("~@3", "bare count rest invalid");
  requireInvalid("120?1.5", "decimal probability invalid");
  requireInvalid("120?101", "probability over 100 invalid");
  requireInvalid("120?-1", "negative probability invalid");
  requireInvalid("[120 90", "missing right bracket invalid");
  requireInvalid("[120,100]?50", "group probability invalid");
  requireInvalid("[3 2]bananas", "unknown bracket suffix unit invalid");
  requireInvalid("2hz (2hz 4hz", "missing right paren invalid");
  requireInvalid("2hz)", "unexpected right paren invalid");
  requireInvalid("{3-}hz", "range missing maximum invalid");
  requireInvalid("{3-5hz", "range missing right brace invalid");
  requireInvalid("{0-3}hz", "range zero endpoint invalid");
  requireInvalid("{3|}hz", "random choice missing after pipe invalid");
  requireInvalid("{|3}hz", "random choice missing before pipe invalid");
  requireInvalid("{3|{}}hz", "nested random empty choice invalid");
  requireInvalid("{x-y}", "external clock random range invalid");
  requireInvalid("{120bpm@0s|90bpm}", "zero random choice repeat invalid");
  requireInvalid("{120bpm#0s|90bpm}", "zero random choice total invalid");
  requireInvalid("q", "unknown bare identifier invalid");
}

void testRandomProgramGenerator() {
  for (unsigned int entropy = 0; entropy < 128; entropy++) {
    std::string program =
        blunch::random_program::generateMusicalClockProgram(entropy);
    lang::ParseResult result = lang::parseClockLiteral(program);
    require(result.ok(), "generated musical clock program parses");
    require(!result.program.blocks.empty(),
            "generated musical clock program has blocks");
  }
}
}  // namespace

int main() {
  testTokenizer();
  testAst();
  testProgramAst();
  testProgramEvaluation();
  testEvaluator();
  testInvalidInputs();
  testRandomProgramGenerator();
  std::puts("blunch language tests passed");
  return 0;
}
