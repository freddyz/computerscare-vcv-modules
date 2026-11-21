#pragma once
#include <vector>

// Declarative Fundamental generation rules. These IDs match Fundamental's
// public parameter/port IDs; comments name them so the rules can be edited
// without digging through the planner. No Rack, engine, or UI dependencies.
namespace beekapoo_patch {
enum class Signal { sound, modulation, gate };
struct Requirement {
  // All groups must be satisfied. Within a group, any one input is enough.
  std::vector<int> anyOf;
  Signal signal;
};
struct Output {
  int port;
  Signal signal;
  std::vector<Requirement> requirements;
  int priority;
  Output(int port, Signal signal, std::vector<Requirement> requirements,
         int priority = 0)
      : port(port),
        signal(signal),
        requirements(requirements),
        priority(priority) {}
};
struct Range {
  int param;
  float low, high;
  bool randomPolarity;
  Range(int param, float low, float high, bool randomPolarity = false)
      : param(param), low(low), high(high), randomPolarity(randomPolarity) {}
  static Range modulation(int param) { return {param, .6f, 1.f, true}; }
  // Modulation uses two separate bands, avoiding near-zero CV depth.
  float sample(float uniform) const {
    bool negative = randomPolarity && uniform < .5f;
    if (randomPolarity) uniform = 2.f * uniform - (negative ? 0.f : 1.f);
    float value = low + uniform * (high - low);
    return negative ? -value : value;
  }
};
struct Control {
  int id;
  std::vector<Requirement> prerequisites;
  std::vector<int> affectedOutputs;
  Control(int id, std::vector<Requirement> prerequisites = {},
          std::vector<int> outputs = {})
      : id(id), prerequisites(prerequisites), affectedOutputs(outputs) {}
};
// Required input groups are ANDed; each group's input choices are ORed.
// affectedOutputs means any listed output must be connected or exposed.
// Empty lists impose no restriction. Control IDs refer to the enclosing
// modulationInputs or parameters field, not to Rack objects.
template <class InputConnected, class OutputUsed>
inline bool controlUseful(const Control& control, InputConnected inputConnected,
                          OutputUsed outputUsed) {
  for (const auto& requirement : control.prerequisites) {
    bool met = false;
    for (int input : requirement.anyOf) met |= inputConnected(input);
    if (!met) return false;
  }
  if (control.affectedOutputs.empty()) return true;
  for (int output : control.affectedOutputs)
    if (outputUsed(output)) return true;
  return false;
}
struct Spec {
  const char* slug;
  std::vector<Control> modulationInputs;
  std::vector<Output> outputs;
  std::vector<Control> parameters;
  std::vector<Range> initial;
};
inline const std::vector<Spec>& library() {
  static const std::vector<Spec> specs = {
      // VCO inputs: pitch 0, FM 1, sync 2, PWM 3. Outputs: sin/tri/saw/square
      // 0..3.
      // Params: frequency 2, FM depth 4, pulse width 5, PWM depth 6.
      // Pulse width only affects the square output, including its CV input.
      {"VCO",
       {0, 1, {3, {}, {3}}},
       {{0, Signal::sound, {}},
        {1, Signal::sound, {}},
        {2, Signal::sound, {}},
        {3, Signal::sound, {}}},
       {2, {5, {}, {3}}},
       {{2, -24, 12},
        Range::modulation(4),
        {5, .25f, .75f},
        Range::modulation(6)}},
      // LFO inputs: FM 0, reset 2, PWM 3, clock 4. Outputs: sin/tri/saw/square
      // 0..3.
      // Params: frequency 2, FM depth 3, pulse width 5, PWM depth 6.
      {"LFO",
       {0, 2, {3, {}, {3}}, 4},
       {{0, Signal::modulation, {}},
        {1, Signal::modulation, {}},
        {2, Signal::modulation, {}},
        {3, Signal::gate, {}}},
       {2, {5, {}, {3}}},
       {{2, -3, 2}, Range::modulation(3), {5, .3f, .7f}, Range::modulation(6)}},
      // Noise: white/pink/red outputs 0..2; no parameters or inputs.
      {"Noise",
       {},
       {{0, Signal::sound, {}}, {1, Signal::sound, {}}, {2, Signal::sound, {}}},
       {},
       {}},
      // VCF: cutoff/resonance CV 0/1 require audio 3; LP/HP outputs 0/1.
      // Params: cutoff 0, resonance 2, cutoff/resonance/drive CV depths 3/5/6.
      {"VCF",
       {{0, {{{3}, Signal::sound}}}, {1, {{{3}, Signal::sound}}}},
       {{0, Signal::sound, {{{3}, Signal::sound}}},
        {1, Signal::sound, {{{3}, Signal::sound}}}},
       {0, 2},
       {{0, .45f, .8f},
        {2, .05f, .4f},
        Range::modulation(3),
        Range::modulation(5),
        Range::modulation(6)}},
      // VCA: CV 0 requires signal 1; output 0; level parameter 0.
      {"VCA-1",
       {{0, {{{1}, Signal::sound}}}},
       {{0, Signal::sound, {{{1}, Signal::sound}}}},
       {0},
       {{0, .5f, .85f}}},
      // Delay: time/tone CV 0/2 require audio 4; Mix output 0.
      // Params: time 0, feedback 1, mix 3, time/feedback/tone/mix CV
      // depths 4..7.
      {"Delay",
       {{0, {{{4}, Signal::sound}}}, {2, {{{4}, Signal::sound}}}},
       {{0, Signal::sound, {{{4}, Signal::sound}}}},
       {0, 1, 3},
       {{0, .45f, .75f},
        {1, .1f, .45f},
        {3, .2f, .55f},
        Range::modulation(4),
        Range::modulation(5),
        Range::modulation(6),
        Range::modulation(7)}},
      // ADSR: A/D/S/R CV 0..3 require gate 4; envelope output 0.
      // Params: A/D/S/R 0..3, corresponding CV depths 4..7.
      {"ADSR",
       {{0, {{{4}, Signal::gate}}},
        {1, {{{4}, Signal::gate}}},
        {2, {{{4}, Signal::gate}}},
        {3, {{{4}, Signal::gate}}}},
       {{0, Signal::modulation, {{{4}, Signal::gate}}}},
       {0, 1, 2, 3},
       {{0, .15f, .45f},
        {1, .2f, .5f},
        {2, .3f, .8f},
        {3, .2f, .6f},
        Range::modulation(4),
        Range::modulation(5),
        Range::modulation(6),
        Range::modulation(7)}},
      // VCA Mix: Mix CV 0, channel signals 1..4, channel CVs 5..8;
      // Mix output/level 0, channel outputs/levels 1..4.
      // Priority 1 makes Mix the first output used. Direct outputs remain
      // available for later growth once Mix is connected. Only connected
      // channels offer their CV and level; the output must also be in use.
      {"VCMixer",
       {{0, {}, {0}},
        {5, {{{1}, Signal::sound}}, {0, 1}},
        {6, {{{2}, Signal::sound}}, {0, 2}},
        {7, {{{3}, Signal::sound}}, {0, 3}},
        {8, {{{4}, Signal::sound}}, {0, 4}}},
       {{0, Signal::sound, {{{1, 2, 3, 4}, Signal::sound}}, 1},
        {1, Signal::sound, {{{1}, Signal::sound}}},
        {2, Signal::sound, {{{2}, Signal::sound}}},
        {3, Signal::sound, {{{3}, Signal::sound}}},
        {4, Signal::sound, {{{4}, Signal::sound}}}},
       {{0, {}, {0}},
        {1, {{{1}, Signal::sound}}, {0, 1}},
        {2, {{{2}, Signal::sound}}, {0, 2}},
        {3, {{{3}, Signal::sound}}, {0, 3}},
        {4, {{{4}, Signal::sound}}, {0, 4}}},
       {{0, .6f, 1}, {1, .7f, 1}, {2, .7f, 1}, {3, .7f, 1}, {4, .7f, 1}}}};
  return specs;
}
}  // namespace beekapoo_patch
