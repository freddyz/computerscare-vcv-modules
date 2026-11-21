#pragma once
#include <algorithm>
#include <cctype>
#include <string>
#include <vector>
namespace beekapoo {
struct LabelAlias {
  const char* full;
  const char* shortName;
};
inline const std::vector<LabelAlias>& labelAliases() {
  static const std::vector<LabelAlias> aliases = {
      {"frequency", "Freq"},
      {"cutoff frequency", "Cutoff"},
      {"pulse width modulation", "PW"},
      {"pulse width", "PW"},
      {"high pass filter", "HP"},
      {"highpass filter", "HP"},
      {"high pass", "HP"},
      {"low pass filter", "LP"},
      {"lowpass filter", "LP"},
      {"low pass", "LP"},
      {"band pass filter", "BP"},
      {"bandpass filter", "BP"},
      {"band pass", "BP"},
      {"resonance", "Res"},
      {"amplitude", "Amp"},
      {"attenuverter", "Attv"},
      {"attenuation", "Atten"},
      {"attack", "Atk"},
      {"decay", "Dec"},
      {"sustain", "Sus"},
      {"release", "Rel"},
      {"feedback", "Fdbk"},
      {"modulation", "Mod"},
      {"envelope", "Env"},
      {"exponential", "Exp"},
      {"linear", "Lin"},
      {"oscillator", "Osc"},
      {"transpose", "Trans"},
      {"fine tune", "Fine"},
      {"sample and hold", "S&H"},
      {"volts per octave", "V/oct"},
      {"stereo width", "Width"},
      {"dry/wet", "Mix"}};
  return aliases;
}
inline std::string abbreviatedLabel(const std::string& full) {
  std::string lower = full;
  std::transform(lower.begin(), lower.end(), lower.begin(),
                 [](unsigned char c) { return char(std::tolower(c)); });
  // Long phrases win over component words. Match whole terms, preserving
  // channel numbers and meaningful suffixes rather than arbitrary clipping.
  const LabelAlias* best = nullptr;
  size_t bestPos = 0, bestLength = 0;
  for (const auto& alias : labelAliases()) {
    size_t pos = lower.find(alias.full);
    if (pos == std::string::npos) continue;
    size_t end = pos + std::string(alias.full).size();
    if ((pos && std::isalnum(static_cast<unsigned char>(lower[pos - 1]))) ||
        (end < lower.size() &&
         std::isalnum(static_cast<unsigned char>(lower[end]))))
      continue;
    if (end - pos > bestLength) {
      best = &alias;
      bestPos = pos;
      bestLength = end - pos;
    }
  }
  if (best) {
    std::string result = full;
    result.replace(bestPos, bestLength, best->shortName);
    return result;
  }
  return full;
}
}  // namespace beekapoo
