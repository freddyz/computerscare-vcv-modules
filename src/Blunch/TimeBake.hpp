#pragma once
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <locale>
#include <map>
#include <sstream>
#include <stdexcept>

#include "../BlunchLanguage/BlunchLanguage.hpp"

namespace blunch {
namespace time_bake {
using namespace language;
struct Edit {
  int begin, end;
  std::string text;
};
struct Result {
  std::string text;
  std::vector<Edit> edits;
  int position(int old) const {
    int shift = 0;
    for (const auto& e : edits) {
      if (old <= e.begin) break;
      if (old < e.end)
        return e.begin + shift + std::min(old - e.begin, int(e.text.size()));
      shift += int(e.text.size()) - (e.end - e.begin);
    }
    return old + shift;
  }
};
inline bool duration(ClockUnit u) {
  return u == ClockUnit::Seconds || u == ClockUnit::Milliseconds ||
         u == ClockUnit::Minutes;
}
inline std::string number(double value) {
  if (!std::isfinite(value) || value <= 0)
    throw std::runtime_error("Time remap produced an invalid value");
  std::ostringstream rounded;
  rounded.imbue(std::locale::classic());
  rounded << std::setprecision(6) << value;
  std::istringstream input(rounded.str());
  input.imbue(std::locale::classic());
  double six;
  input >> six;
  int decimals = std::max(0, 5 - int(std::floor(std::log10(six))));
  std::ostringstream out;
  out.imbue(std::locale::classic());
  out << std::fixed << std::setprecision(decimals) << six;
  std::string text = out.str();
  if (text.find('.') != std::string::npos) {
    while (text.back() == '0') text.pop_back();
    if (text.back() == '.') text.pop_back();
  }
  return text;
}
inline Result line(const std::string& source, ClockUnit unit, double factor) {
  if (source.find_first_not_of(" \t\r\n") == std::string::npos) {
    Result result;
    result.text = source;
    return result;
  }
  auto parsed = parseClockLiteral(source, unit);
  if (!parsed.ok())
    throw std::runtime_error("Correct invalid formulas before baking Time");
  auto tokens = tokenize(source);
  std::map<int, Edit> edits;
  auto put = [&](int begin, int end, const std::string& text) {
    if (end <= begin) return;
    auto existing = edits.find(begin);
    if (existing != edits.end() &&
        (existing->second.end != end || existing->second.text != text))
      throw std::runtime_error("Conflicting Time remap ranges");
    edits[begin] = {begin, end, text};
  };
  auto value = [&](SourceRange range, double amount, ClockUnit u) {
    if (range.end <= range.begin) return;
    put(range.begin, range.end,
        number(duration(u) ? amount / factor : amount * factor));
  };
  auto timed = [&](SourceRange range) {
    for (const auto& token : tokens)
      if (token.type == TokenType::Number && token.begin >= range.begin &&
          token.end <= range.end) {
        put(token.begin, token.end, number(std::stod(token.lexeme) / factor));
        break;
      }
  };
  auto literal = [&](const ClockLiteralAst& ast, bool durationOnly) {
    if (ast.kind == ClockLiteralKind::Numeric) {
      if (durationOnly && !duration(ast.unit)) return;
      SourceRange r;
      r.begin = ast.range.begin;
      r.end = r.begin + int(ast.valueLexeme.size());
      value(r, ast.value, ast.unit);
    } else if (ast.kind == ClockLiteralKind::Colon) {
      put(ast.range.begin, ast.range.end,
          number((ast.minutes * 60. + ast.seconds) / factor) + "s");
    } else if (ast.kind == ClockLiteralKind::RandomRange) {
      for (const auto& choice : ast.randomChoices) {
        ClockUnit u =
            choice.unit == ClockUnit::Unknown ? ast.unit : choice.unit;
        if (!choice.externalClockChoice && (!durationOnly || duration(u))) {
          value(choice.minValueRange, choice.minValue, u);
          value(choice.maxValueRange, choice.maxValue, u);
        }
        if (choice.hasRepeatModifier && choice.repeatIsDuration &&
            !choice.repeatUsesExternalClock)
          timed(choice.repeatValueRange);
        if (choice.hasTotalDurationModifier &&
            !choice.totalDurationIsTickCount &&
            !choice.totalDurationUsesExternalClock)
          timed(choice.totalDurationValueRange);
      }
    }
  };
  for (const auto& block : parsed.program.blocks) {
    literal(block.literal, false);
    if (block.repeatIsDuration && !block.repeatUsesExternalClock) {
      if (block.repeatIsSequence)
        for (const auto& item : block.repeatSequence) literal(item, true);
      else if (block.repeatIsRandom)
        literal(block.repeatRandom, true);
      else
        literal(block.repeatDuration, true);
    }
    if (block.hasTotalDuration && !block.totalDurationIsTickCount &&
        !block.totalDurationUsesExternalClock) {
      if (block.totalDurationIsRandom)
        literal(block.totalDurationRandom, true);
      else
        literal(block.totalDuration, true);
    }
  }
  Result result;
  result.text = source;
  for (const auto& item : edits) result.edits.push_back(item.second);
  for (auto it = result.edits.rbegin(); it != result.edits.rend(); ++it)
    result.text.replace(it->begin, it->end - it->begin, it->text);
  if (!parseClockLiteral(result.text, unit).ok())
    throw std::runtime_error("Remapped formula is outside the supported range");
  return result;
}
inline Result page(const std::string& source, ClockUnit unit, double factor) {
  Result result;
  int start = 0;
  do {
    auto end = source.find('\n', start);
    if (end == std::string::npos) end = source.size();
    auto mapped = line(source.substr(start, end - start), unit, factor);
    result.text += mapped.text;
    for (auto e : mapped.edits) {
      e.begin += start;
      e.end += start;
      result.edits.push_back(e);
    }
    if (end == source.size()) break;
    result.text += '\n';
    start = int(end) + 1;
  } while (true);
  return result;
}
inline void scale(ClockSpec& spec, double factor) {
  spec.bpm *= factor;
  spec.hz *= factor;
  spec.periodSeconds /= factor;
}
}  // namespace time_bake
}  // namespace blunch
