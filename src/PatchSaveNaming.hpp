#pragma once

#include <array>
#include <string>
#include <unordered_set>
#include <vector>

namespace computerscare {
namespace patch_saves {
// Shared naming policy: edit this list to change branch suffixes.
inline const std::array<const char*, 150>& words() {
  static const std::array<const char*, 150> values = {
      {"pig",    "shrew", "fox",    "otter",  "moth",  "hare",   "wren",
       "badger", "mole",  "finch",  "lynx",   "newt",  "stoat",  "owl",
       "yak",    "toad",  "deer",   "bear",   "wolf",  "boar",   "elk",
       "moose",  "mink",  "vole",   "rat",    "mouse", "bat",    "seal",
       "whale",  "shark", "trout",  "carp",   "pike",  "perch",  "bass",
       "crab",   "goat",  "slug",   "snail",  "ant",   "bee",    "wasp",
       "gnat",   "flea",  "horse",  "dove",   "crow",  "rook",   "swan",
       "duck",   "oak",   "ash",    "elm",    "fir",   "pine",   "yew",
       "birch",  "beech", "alder",  "cedar",  "maple", "hazel",  "holly",
       "willow", "aspen", "poplar", "spruce", "larch", "rowan",  "palm",
       "olive",  "apple", "pear",   "plum",   "peach", "cherry", "lemon",
       "lime",   "fig",   "date",   "fern",   "moss",  "reed",   "rush",
       "sedge",  "grass", "clover", "mint",   "sage",  "thyme",  "basil",
       "dill",   "cress", "flax",   "hemp",   "aloe",  "iris",   "rose",
       "lily",   "aster", "cow",    "dog",    "cat",   "hen",    "ram",
       "ewe",    "ox",    "ape",    "gnu",    "emu",   "ibis",   "gull",
       "tern",   "lark",  "teal",   "coot",   "egret", "gecko",  "skink",
       "orca",   "bay",   "cape",   "cove",   "dune",  "fen",    "glen",
       "gulf",   "hill",  "isle",   "lake",   "mesa",  "moor",   "peak",
       "reef",   "vale",  "cup",    "mug",    "bowl",  "fork",   "spoon",
       "pan",    "pot",   "jar",    "lid",    "mop",   "rug",    "lamp",
       "soap",   "comb",  "wick"}};
  return values;
}
struct NumberedStem {
  std::string base, digits;
};
inline NumberedStem splitNumber(const std::string& stem) {
  size_t end = stem.size();
  while (end && stem[end - 1] >= '0' && stem[end - 1] <= '9') --end;
  return {stem.substr(0, end), stem.substr(end)};
}
inline std::string normalizedNumber(const std::string& digits) {
  size_t first = digits.find_first_not_of('0');
  return first == std::string::npos ? "0" : digits.substr(first);
}
inline bool greaterNumber(const std::string& a, const std::string& b) {
  return a.size() != b.size() ? a.size() > b.size() : a > b;
}
inline std::string incrementNumber(std::string digits) {
  for (size_t i = digits.size(); i > 0; --i) {
    if (digits[i - 1] != '9') {
      ++digits[i - 1];
      return digits;
    }
    digits[i - 1] = '0';
  }
  return "1" + digits;
}
// Existing names are .vcv filenames, not paths. Never infer an earlier branch.
inline std::string nextFilename(const std::string& stem,
                                const std::vector<std::string>& existing) {
  auto source = splitNumber(stem);
  std::string highest = normalizedNumber(source.digits);
  for (const auto& file : existing) {
    if (file.size() < 4 || file.substr(file.size() - 4) != ".vcv") continue;
    auto candidate = splitNumber(file.substr(0, file.size() - 4));
    auto number = normalizedNumber(candidate.digits);
    if (candidate.base == source.base && greaterNumber(number, highest))
      highest = number;
  }
  std::string next = incrementNumber(highest);
  if (next.size() < source.digits.size())
    next.insert(0, source.digits.size() - next.size(), '0');
  return source.base + next + ".vcv";
}
// A root branch ends in one or more dictionary words, with no version number.
// Recognize concatenated suffixes too, e.g. patch2-horseflax.
inline std::string branchBase(const std::string& stem) {
  size_t dash = stem.rfind('-');
  if (dash == std::string::npos || dash == 0 || dash + 1 == stem.size())
    return stem;
  std::string suffix = stem.substr(dash + 1);
  std::vector<bool> matched(suffix.size() + 1, false);
  matched[0] = true;
  for (size_t i = 0; i < suffix.size(); ++i)
    if (matched[i]) {
      for (const char* legacy : {"clam", "tick"}) {
        size_t length = std::char_traits<char>::length(legacy);
        if (suffix.compare(i, length, legacy) == 0) matched[i + length] = true;
      }
      for (const char* word : words()) {
        size_t length = std::char_traits<char>::length(word);
        if (suffix.compare(i, length, word) == 0) matched[i + length] = true;
      }
    }
  return matched.back() ? stem.substr(0, dash) : stem;
}
// Enumerate single words, then pairs, then triples using the dictionary size.
// Rotate by start so each save can begin with a different word.
inline std::string branchSuffix(unsigned attempt, unsigned start = 0) {
  const auto& names = words();
  std::string suffix;
  for (;;) {
    suffix =
        std::string(names[(attempt % names.size() + start) % names.size()]) +
        suffix;
    if (attempt < names.size()) return suffix;
    attempt = attempt / unsigned(names.size()) - 1;
  }
}
inline std::string branchFilename(const std::string& stem, unsigned attempt,
                                  unsigned start = 0) {
  return branchBase(stem) + "-" + branchSuffix(attempt, start) + ".vcv";
}
inline std::string foldedFilename(std::string name) {
  for (char& c : name)
    if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
  return name;
}
class BranchCandidates {
  std::string prefix;
  std::unordered_set<std::string> occupied;
  unsigned attempt = 0, start;

 public:
  BranchCandidates(const std::string& stem,
                   const std::vector<std::string>& existing, unsigned start = 0)
      : prefix(branchBase(stem) + "-"), start(start) {
    occupied.reserve(existing.size() + 1);
    for (const auto& name : existing) occupied.insert(foldedFilename(name));
    // The current root may still be outside the newly created save folder.
    occupied.insert(foldedFilename(stem + ".vcv"));
  }
  std::string next() {
    for (;;) {
      std::string name = prefix + branchSuffix(attempt++, start) + ".vcv";
      if (!occupied.count(foldedFilename(name))) return name;
    }
  }
};
}  // namespace patch_saves
}  // namespace computerscare
