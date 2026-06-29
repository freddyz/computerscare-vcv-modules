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
  requireInvalid("120?1.5", "decimal probability invalid");
  requireInvalid("120?101", "probability over 100 invalid");
  requireInvalid("120?-1", "negative probability invalid");
  requireInvalid("[120 90", "missing right bracket invalid");
  requireInvalid("[120,100]?50", "group probability invalid");
}
}  // namespace

int main() {
  testTokenizer();
  testAst();
  testProgramAst();
  testProgramEvaluation();
  testEvaluator();
  testInvalidInputs();
  std::puts("cloly pock language tests passed");
  return 0;
}
