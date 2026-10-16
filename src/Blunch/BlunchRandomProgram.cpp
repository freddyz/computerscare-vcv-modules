#include "BlunchRandomProgram.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <locale>
#include <sstream>
#include <stdexcept>

namespace blunch {
namespace random_program {
namespace {
constexpr size_t maxOutput = 4096;
constexpr int maxDepth = 24, maxSteps = 4096, maxRepeat = 16;

class Expander {
  const Catalogue& catalogue;
  uint32_t state;
  int steps = 0;

  void budget(int depth) {
    if (depth > maxDepth || ++steps > maxSteps)
      throw std::runtime_error("Template expansion limit exceeded");
  }
  double random() {
    uint32_t value = (state += 0x9e3779b9u);
    value = (value ^ (value >> 16)) * 0x85ebca6bu;
    value = (value ^ (value >> 13)) * 0xc2b2ae35u;
    value ^= value >> 16;
    return double(value) / 4294967296.;
  }
  void append(std::string& result, const std::string& value) {
    if (result.size() + value.size() > maxOutput)
      throw std::runtime_error("Template output is too long");
    result += value;
  }
  static void space(const std::string& text, size_t& pos) {
    while (pos < text.size() && (text[pos] == ' ' || text[pos] == '\n' ||
                                 text[pos] == '\t' || text[pos] == '\r'))
      ++pos;
  }
  // Arguments remain unevaluated: pick only expands the chosen branch, and
  // repeat draws fresh values on every iteration.
  static std::string argument(const std::string& text, size_t& pos) {
    space(text, pos);
    if (pos >= text.size() || text[pos] == ')')
      throw std::runtime_error("Missing template argument");
    if (text[pos] == '"') {
      ++pos;
      std::string result;
      while (pos < text.size()) {
        char ch = text[pos++];
        if (ch == '"') return result;
        if (ch == '\\') {
          if (pos == text.size()) break;
          ch = text[pos++];
          if (ch == 'n')
            ch = '\n';
          else if (ch == 't')
            ch = '\t';
          else if (ch != '\\' && ch != '"')
            throw std::runtime_error("Unknown template string escape");
        }
        result += ch;
      }
      throw std::runtime_error("Unterminated template string");
    }
    size_t begin = pos;
    if (text[pos] == '(') {
      int nesting = 0;
      bool quoted = false, escaped = false;
      do {
        char ch = text[pos++];
        if (escaped) {
          escaped = false;
          continue;
        }
        if (ch == '\\') {
          escaped = true;
          continue;
        }
        if (ch == '"') quoted = !quoted;
        if (!quoted) {
          if (ch == '(') ++nesting;
          if (ch == ')') --nesting;
        }
        if (nesting > maxDepth)
          throw std::runtime_error("Template nesting limit exceeded");
        if (nesting == 0) return text.substr(begin, pos - begin);
      } while (pos < text.size());
      throw std::runtime_error("Unclosed template expression");
    }
    while (pos < text.size() && text[pos] != ')' && text[pos] != ' ' &&
           text[pos] != '\n' && text[pos] != '\t' && text[pos] != '\r')
      ++pos;
    return text.substr(begin, pos - begin);
  }
  double number(const std::string& value, int depth) {
    std::istringstream input(expand(value, depth + 1));
    input.imbue(std::locale::classic());
    double result;
    if (!(input >> result) || !std::isfinite(result))
      throw std::runtime_error("Expected a finite template number");
    input >> std::ws;
    if (!input.eof()) throw std::runtime_error("Invalid template number");
    return result;
  }
  int integer(const std::string& value, int depth) {
    double result = number(value, depth);
    if (result != std::floor(result) || std::fabs(result) > 1000000.)
      throw std::runtime_error("Expected a template integer within +/-1000000");
    return int(result);
  }
  static void arity(const std::vector<std::string>& args, size_t min,
                    size_t max) {
    if (args.size() < min || args.size() > max)
      throw std::runtime_error("Incorrect template argument count");
  }
  size_t weighted(const std::vector<double>& weights) {
    double total = 0;
    for (double weight : weights) {
      if (!std::isfinite(weight) || weight < 0)
        throw std::runtime_error(
            "Template weights must be finite and nonnegative");
      total += weight;
    }
    if (!std::isfinite(total) || total <= 0)
      throw std::runtime_error("Template choices need a positive total weight");
    double draw = random() * total;
    for (size_t i = 0; i < weights.size(); ++i) {
      if (draw < weights[i]) return i;
      draw -= weights[i];
    }
    for (size_t i = weights.size(); i > 0; --i)
      if (weights[i - 1] > 0) return i - 1;
    throw std::runtime_error("Empty template choices");
  }
  std::string call(const std::string& text, size_t& pos, int depth) {
    budget(depth);
    ++pos;
    std::string name = argument(text, pos);
    std::vector<std::string> args;
    space(text, pos);
    while (pos < text.size() && text[pos] != ')') {
      if (args.size() >= 64)
        throw std::runtime_error("Too many template arguments");
      args.push_back(argument(text, pos));
      space(text, pos);
    }
    if (pos == text.size())
      throw std::runtime_error("Unclosed template expression");
    ++pos;
    if (name == "nrange" || name == "frange") {
      arity(args, 2, name == "frange" ? 3 : 2);
      double low, high;
      if (name == "nrange") {
        low = integer(args[0], depth);
        high = integer(args[1], depth);
      } else {
        low = number(args[0], depth);
        high = number(args[1], depth);
        if (std::fabs(low) > 1000000. || std::fabs(high) > 1000000.)
          throw std::runtime_error("Template range is too large");
      }
      if (low > high) std::swap(low, high);
      double value = low + random() * (high - low + (name == "nrange" ? 1 : 0));
      std::ostringstream output;
      output.imbue(std::locale::classic());
      if (name == "nrange")
        output << int(std::floor(value));
      else {
        int precision = args.size() == 3 ? integer(args[2], depth) : 2;
        if (precision < 0 || precision > 6)
          throw std::runtime_error("Template precision must be 0..6");
        output << std::fixed << std::setprecision(precision) << value;
      }
      return output.str();
    }
    if (name == "pick") {
      arity(args, 1, 64);
      return expand(args[size_t(random() * args.size())], depth + 1);
    }
    if (name == "wpick") {
      arity(args, 2, 64);
      if (args.size() % 2)
        throw std::runtime_error("wpick needs weight/value pairs");
      std::vector<double> weights;
      for (size_t i = 0; i < args.size(); i += 2)
        weights.push_back(number(args[i], depth));
      return expand(args[weighted(weights) * 2 + 1], depth + 1);
    }
    if (name == "ref") {
      arity(args, 1, 1);
      auto found = catalogue.definitions.find(expand(args[0], depth + 1));
      if (found == catalogue.definitions.end())
        throw std::runtime_error("Unknown template reference: " + args[0]);
      std::vector<double> weights;
      for (const auto& choice : found->second) weights.push_back(choice.weight);
      return expand(found->second[weighted(weights)].text, depth + 1);
    }
    if (name == "repeat") {
      arity(args, 2, 3);
      int count = integer(args[1], depth);
      if (count < 0 || count > maxRepeat)
        throw std::runtime_error("Template repeat count must be 0..16");
      std::string result,
          separator = args.size() == 3 ? expand(args[2], depth + 1) : " ";
      for (int i = 0; i < count; ++i) {
        if (i) append(result, separator);
        append(result, expand(args[0], depth + 1));
      }
      return result;
    }
    throw std::runtime_error("Unknown template function: " + name);
  }

 public:
  Expander(const Catalogue& catalogue, unsigned int seed)
      : catalogue(catalogue), state(seed ? seed : 0x9e3779b9u) {}
  std::string expand(const std::string& text, int depth = 0) {
    budget(depth);
    if (text.size() > maxOutput)
      throw std::runtime_error("Template is too long");
    std::string result;
    size_t pos = 0;
    while (pos < text.size()) {
      if (text[pos] == '(')
        append(result, call(text, pos, depth + 1));
      else if (text[pos] == '\\') {
        if (++pos == text.size())
          throw std::runtime_error("Trailing template escape");
        append(result, text.substr(pos++, 1));
      } else
        append(result, text.substr(pos++, 1));
    }
    return result;
  }
};
}  // namespace

std::string expandTemplate(const Catalogue& catalogue, const std::string& text,
                           unsigned int entropy) {
  return Expander(catalogue, entropy).expand(text);
}
}  // namespace random_program
}  // namespace blunch
