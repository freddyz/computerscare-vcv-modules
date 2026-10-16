#pragma once

#include <map>
#include <string>
#include <vector>

namespace blunch {
namespace random_program {

struct Choice {
  double weight;
  std::string text;
  Choice(double weight, const std::string& text) : weight(weight), text(text) {}
};

// Independent of Rack and the clock grammar; usable by other program
// generators.
struct Catalogue {
  std::map<std::string, std::vector<Choice>> definitions;
  std::string startup, randomize;
};

// Throws std::runtime_error on invalid expressions or expansion limits.
std::string expandTemplate(const Catalogue& catalogue, const std::string& text,
                           unsigned int entropy);

}  // namespace random_program
}  // namespace blunch
