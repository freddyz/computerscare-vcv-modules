#pragma once
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <string>
#include <vector>

#include "BeekapooIdentity.hpp"
namespace beekapoo {
struct LabelAlias {
  const char* full;
  const char* shortName;
  const char* compact;
  LabelAlias(const char* full, const char* shortName,
             const char* compact = nullptr)
      : full(full), shortName(shortName), compact(compact) {}
};
inline const std::vector<LabelAlias>& labelAliases() {
  static const std::vector<LabelAlias> aliases = {
      {"voltage controlled amplifier", "VCA"},
      {"voltage controlled oscillator", "VCO"},
      {"low frequency oscillator", "LFO"},
      {"linear frequency modulation", "Lin FM", "LFM"},
      {"exponential frequency modulation", "Exp FM", "EFM"},
      {"amplitude modulation", "AM"},
      {"phase modulation", "PM"},
      {"ring modulation", "Ring Mod", "RM"},
      {"end of attack", "EOA"},
      {"end of decay", "EOD"},
      {"end of release", "EOR"},
      {"end of cycle", "EOC"},
      {"end of rise", "EOR"},
      {"end of fall", "EOF"},
      {"track and hold", "T&H"},
      {"sample & hold", "S&H"},
      {"sample rate", "Sample Rate", "SR"},
      {"bit depth", "Bits"},
      {"slew rate", "Slew"},
      {"pitch bend", "Bend"},
      {"aftertouch", "Aftertouch", "AT"},
      {"note velocity", "Velocity", "Vel"},
      {"high shelf", "Hi Shelf", "HS"},
      {"low shelf", "Lo Shelf", "LS"},
      {"band reject", "Notch"},
      {"all pass", "AP"},
      {"allpass", "AP"},
      {"dry / wet", "Mix"},
      {"voltage per octave", "V/Oct"},
      {"1v/oct", "V/Oct"},
      {"clockwise", "CW"},
      {"counterclockwise", "CCW"},
      {"clock", "Clock", "Clk"},
      {"trigger", "Trig", "Trg"},
      {"reset", "Reset", "Rst"},
      {"retrigger", "Retrig", "Rtrg"},
      {"synchronize", "Sync"},
      {"synchronization", "Sync"},
      {"input", "In"},
      {"output", "Out"},
      {"left", "L"},
      {"right", "R"},
      {"channels", "Ch"},
      {"octave", "Oct"},
      {"semitone", "Semi", "St"},
      {"semitones", "Semi", "St"},
      {"detune", "Detune", "Det"},
      {"coarse", "Coarse", "Crs"},
      {"tuning", "Tune"},
      {"hertz", "Hz"},
      {"milliseconds", "ms"},
      {"seconds", "s"},
      {"beats per minute", "BPM"},
      {"decibels", "dB"},
      {"level", "Level", "Lvl"},
      {"volume", "Vol"},
      {"amount", "Amount", "Amt"},
      {"depth", "Depth", "Dep"},
      {"position", "Pos"},
      {"length", "Length", "Len"},
      {"duration", "Dur"},
      {"division", "Div"},
      {"multiply", "Mult"},
      {"multiplier", "Mult"},
      {"offset", "Offset", "Ofs"},
      {"threshold", "Thresh", "Thr"},
      {"compression", "Comp"},
      {"ratio", "Ratio", "Rat"},
      {"balance", "Bal"},
      {"probability", "Prob", "Prb"},
      {"random", "Rand", "Rnd"},
      {"randomize", "Rand", "Rnd"},
      {"pattern", "Patt", "Pat"},
      {"sequence", "Seq"},
      {"sequencer", "Seq"},
      {"density", "Dens", "Dns"},
      {"rotation", "Rotate", "Rot"},
      {"reverse", "Rev"},
      {"forward", "Fwd"},
      {"delay", "Delay", "Dly"},
      {"reverb", "Reverb", "Rvb"},
      {"freeze", "Freeze", "Frz"},
      {"width", "Width", "Wdth"},
      {"wavetable", "WT"},
      {"waveform", "Wave", "Wav"},
      {"triangle", "Tri"},
      {"square", "Sqr"},
      {"sawtooth", "Saw"},
      {"sinusoid", "Sine"},
      {"unipolar", "Uni"},
      {"bipolar", "Bi"},
      {"positive", "Pos"},
      {"negative", "Neg"},
      {"inverted", "Inv"},
      {"invert", "Inv"},
      {"external", "Ext"},
      {"internal", "Int"},
      {"polyphonic", "Poly"},
      {"monophonic", "Mono"},
      {"sample", "Sample", "Smp"},
      {"select", "Sel"},
      {"selection", "Sel"},
      {"track", "Track", "Trk"},
      {"stage", "Stage", "Stg"},
      {"harmonic", "Harm"},
      {"harmonics", "Harm"},
      {"quantize", "Quant"},
      {"quantizer", "Quant"},
      {"velocity", "Vel"},
      {"gate length", "Gate Len", "G Len"},
      {"1v/octave pitch", "Pitch"},
      {"1 v/octave pitch", "Pitch"},
      {"1v/octave", "V/Oct"},
      {"channel", "Ch"},
      {"frequency modulation", "FM"},
      {"frequency", "Freq", "Fq"},
      {"cutoff frequency", "Cutoff Freq", "Cutoff"},
      {"cutoff frequence", "Cutoff Freq", "Cutoff"},
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
      {"cutoff", "Cutoff", "Cut"},
      {"resonance", "Res"},
      {"amplitude", "Amp"},
      {"attenuverter", "Attv"},
      {"attenuation", "Atten"},
      {"attack", "Atk"},
      {"decay", "Dec"},
      {"sustain", "Sus"},
      {"release", "Rel"},
      {"feedback", "Fdbk", "FB"},
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
inline std::string abbreviatedLabel(const std::string& full,
                                    bool compact = false) {
  std::string lower = full;
  std::transform(lower.begin(), lower.end(), lower.begin(),
                 [](unsigned char c) { return char(std::tolower(c)); });
  std::string result;
  for (size_t pos = 0; pos < full.size();) {
    const LabelAlias* best = nullptr;
    size_t length = 0;
    for (const auto& alias : labelAliases()) {
      size_t n = std::string(alias.full).size(), end = pos + n;
      if (n <= length || lower.compare(pos, n, alias.full) != 0) continue;
      if ((pos && std::isalnum(static_cast<unsigned char>(lower[pos - 1]))) ||
          (end < lower.size() &&
           std::isalnum(static_cast<unsigned char>(lower[end]))))
        continue;
      best = &alias;
      length = n;
    }
    if (!best) {
      result += full[pos++];
      continue;
    }
    result += compact && best->compact ? best->compact : best->shortName;
    pos += length;
    if (std::string(best->full) == "channel") {
      size_t number = pos;
      while (number < full.size() &&
             std::isspace(static_cast<unsigned char>(full[number])))
        ++number;
      if (number < full.size() &&
          std::isdigit(static_cast<unsigned char>(full[number])))
        pos = number;
    }
  }
  return result;
}
// Normalize separators only between words; retain signed values, ratios,
// units and UTF-8 bytes. Matching remains longest-phrase and whole-word only.
inline std::string normalizedLabel(std::string text) {
  for (size_t i = 0; i < text.size(); ++i) {
    auto letter = [](unsigned char c) { return c < 128 && std::isalpha(c); };
    if (text[i] == '_' || (text[i] == '-' && i && i + 1 < text.size() &&
                           letter(text[i - 1]) && letter(text[i + 1])))
      text[i] = ' ';
  }
  std::string result;
  for (unsigned char c : text) {
    if (c < 128 && std::isspace(c)) {
      if (!result.empty() && result.back() != ' ') result += ' ';
    } else
      result += char(c);
  }
  if (!result.empty() && result.back() == ' ') result.pop_back();
  return result;
}
inline std::string compactLabelNumbers(std::string text) {
  // "Ch 12" / "Ch #12" -> "Ch12", likewise In/Out/Track/Stage/Step.
  for (const auto* prefix : {"Ch", "In", "Out", "Trk", "Stg", "Step"}) {
    std::string key = std::string(prefix) + " ";
    size_t pos = 0;
    while ((pos = text.find(key, pos)) != std::string::npos) {
      size_t first = pos + key.size(), n = first;
      if (n < text.size() && text[n] == '#') ++n;
      while (n < text.size() && text[n] == ' ') ++n;
      if (n < text.size() && std::isdigit(static_cast<unsigned char>(text[n])))
        text.erase(first - 1, n - first + 1);
      pos += std::string(prefix).size();
    }
  }
  return text;
}
inline std::vector<std::string> labelAlternatives(const std::string& full,
                                                  bool port = false) {
  std::vector<std::string> result;
  auto add = [&](std::string text) {
    text = normalizedLabel(text);
    if (!text.empty() &&
        std::find(result.begin(), result.end(), text) == result.end())
      result.push_back(text);
  };
  add(full);
  std::string name = normalizedLabel(full);
  // Some plugin labels contain whole instructions. Keep the instructions in
  // the tooltip; only trim explanatory clauses, never L/R or unit suffixes.
  size_t prose = name.find(" - ");
  if (prose != std::string::npos) {
    auto tail = name.substr(prose + 3);
    if (tail.find(' ') != std::string::npos &&
        (tail.find("when ") != std::string::npos ||
         tail.find("overrides") != std::string::npos))
      name.resize(prose);
  }
  auto colon = name.find(": ");
  if (colon != std::string::npos && colon > 0) name.resize(colon);
  add(name);
  auto shortName = compactLabelNumbers(abbreviatedLabel(name));
  add(shortName);
  auto tight = compactLabelNumbers(abbreviatedLabel(name, true));
  add(tight);
  if (port) {
    // The port already supplies "input/output" context, but a lone In/Out
    // or numbered In3/Out4 is still a useful name and must remain intact.
    for (const auto* direction : {" In", " Out"}) {
      std::string suffix = direction;
      if (tight.size() > suffix.size() &&
          tight.compare(tight.size() - suffix.size(), suffix.size(), suffix) ==
              0)
        tight.resize(tight.size() - suffix.size());
    }
    add(tight);
  }
  if (tight.compare(0, 2, "Ch") == 0) {
    size_t end = 2;
    while (end < tight.size() &&
           std::isdigit(static_cast<unsigned char>(tight[end])))
      ++end;
    std::string suffix = tight.substr(end);
    std::transform(suffix.begin(), suffix.end(), suffix.begin(),
                   [](unsigned char c) { return char(std::tolower(c)); });
    if (end > 2 && (suffix == " cv" || suffix == "cv"))
      add("CV" + tight.substr(2, end - 2));
  }
  // Unknown vocabulary: remove internal vowels first, then use initials only
  // at the smallest sizes. Never discard a channel index, sign or unit.
  std::string consonants;
  bool wordStart = true;
  for (unsigned char c : tight) {
    bool letter = c < 128 && std::isalpha(c);
    bool vowel = std::string("aeiouAEIOU").find(char(c)) != std::string::npos;
    if (!letter || wordStart || !vowel) consonants += char(c);
    wordStart = !letter;
  }
  add(consonants);
  std::string initials;
  const std::vector<std::string> keep = {
      "FM",  "AM",  "PM",  "PW",  "RM",  "CV",  "LFO",  "VCO", "VCA",
      "EOC", "EOR", "EOA", "EOF", "EOD", "S&H", "T&H",  "LP",  "HP",
      "BP",  "AP",  "Hz",  "ms",  "dB",  "BPM", "V/Oct"};
  for (size_t start = 0; start < tight.size();) {
    size_t end = tight.find(' ', start);
    if (end == std::string::npos) end = tight.size();
    auto word = tight.substr(start, end - start);
    if (!initials.empty()) initials += ' ';
    if (std::find(keep.begin(), keep.end(), word) != keep.end())
      initials += word;
    else {
      bool first = true;
      for (unsigned char c : word) {
        bool letter = c < 128 && std::isalpha(c);
        if (!letter || first) initials += char(c);
        if (letter) first = false;
      }
    }
    start = end + 1;
  }
  add(initials);
  // A dense single-line alternative retains the exact same information.
  initials.erase(std::remove(initials.begin(), initials.end(), ' '),
                 initials.end());
  add(initials);
  return result;
}
// ASCII labels change case; leave UTF-8 names intact. Module multiplicity
// determines the choice, never cable changes or external patching.
inline std::string labelCase(std::string text, uint64_t moduleHash) {
  for (char& c : text) {
    unsigned char value = static_cast<unsigned char>(c);
    if (value < 128)
      c = char(moduleHash & 1 ? std::toupper(value) : std::tolower(value));
  }
  return text;
}
enum class TitleCapitalization { Lower, Upper, Title, Mixed };
inline TitleCapitalization titleCapitalization(uint64_t moduleHash) {
  // A separate salt avoids correlating the title with parameter-label case.
  // 34% lower, 32% upper, 32% title case, 2% mixed. Stable across reloads.
  auto draw = mixIdentity(moduleHash, 0x7469746c652d6361ull) % 100;
  return draw < 34   ? TitleCapitalization::Lower
         : draw < 66 ? TitleCapitalization::Upper
         : draw < 98 ? TitleCapitalization::Title
                     : TitleCapitalization::Mixed;
}
inline std::string titleCase(std::string text, uint64_t moduleHash) {
  auto mode = titleCapitalization(moduleHash);
  bool start = true;
  for (size_t i = 0; i < text.size(); ++i) {
    unsigned char c = static_cast<unsigned char>(text[i]);
    bool letter = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
    if (letter) {
      bool upper = mode == TitleCapitalization::Upper ||
                   (mode == TitleCapitalization::Title && start) ||
                   (mode == TitleCapitalization::Mixed &&
                    (mixIdentity(moduleHash, i + 1) & 1));
      text[i] = upper ? char(c >= 'a' && c <= 'z' ? c - 'a' + 'A' : c)
                      : char(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
      start = false;
    } else if (c >= 128 || (c >= '0' && c <= '9')) {
      start = false;  // Preserve UTF-8 bytes and numeric suffixes verbatim.
    } else if (c != '\'') {
      start = true;
    }
  }
  return text;
}
}  // namespace beekapoo
