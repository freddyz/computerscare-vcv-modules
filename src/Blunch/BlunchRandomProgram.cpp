#include "BlunchRandomProgram.hpp"

#include <sstream>

namespace blunch {
namespace random_program {
namespace {

static unsigned int nextEntropy(unsigned int& entropy) {
  entropy = entropy * 1664525u + 1013904223u;
  return entropy;
}

static int pick(unsigned int& entropy, int count) {
  if (count <= 0) {
    return 0;
  }
  return (int)(nextEntropy(entropy) % (unsigned int)count);
}

static const char* pickTempo(unsigned int& entropy) {
  static const char* tempos[] = {"72",  "80",  "90",  "96",  "100", "108",
                                 "120", "128", "135", "144", "150", "160"};
  return tempos[pick(entropy, 12)];
}

static int pickRepeat(unsigned int& entropy) {
  static const int repeats[] = {2, 3, 4, 6, 8};
  return repeats[pick(entropy, 5)];
}

static std::string tempoLiteral(unsigned int& entropy) {
  return std::string(pickTempo(entropy)) + "bpm";
}

static std::string tempoSequence(unsigned int& entropy, int count) {
  std::stringstream stream;
  stream << "[";
  for (int i = 0; i < count; i++) {
    if (i > 0) {
      stream << " ";
    }
    stream << pickTempo(entropy);
  }
  stream << "]@" << pickRepeat(entropy);
  return stream.str();
}

}  // namespace

std::string generateMusicalClockProgram(unsigned int entropy) {
  switch (pick(entropy, 5)) {
    case 0:
      return tempoLiteral(entropy);
    case 1:
      return tempoSequence(entropy, 3);
    case 2:
      return tempoSequence(entropy, 4);
    case 3:
      return tempoLiteral(entropy) + "@" + std::to_string(pickRepeat(entropy)) +
             " " + tempoLiteral(entropy) + "@" +
             std::to_string(pickRepeat(entropy));
    default:
      return "{" + std::string(pickTempo(entropy)) + "|" +
             std::string(pickTempo(entropy)) + "|" +
             std::string(pickTempo(entropy)) + "}bpm";
  }
}

}  // namespace random_program
}  // namespace blunch
