#pragma once
#include <algorithm>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

// Rack-independent generation data. Port dependencies come from the
// per-manufacturer JSON tables; parameter policy remains separate.
namespace beekapoo_patch {
struct Requirement {
  // All groups must be satisfied. Within a group, any one input is enough.
  std::vector<int> anyOf;
};
struct Output {
  int port;
  std::vector<Requirement> requirements;
  int priority;
  Output(int port, std::vector<Requirement> requirements, int priority = 0)
      : port(port), requirements(requirements), priority(priority) {}
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
  std::vector<Requirement> affectedOutputs;
  Control(int id, std::vector<Requirement> prerequisites = {},
          std::vector<Requirement> outputs = {})
      : id(id), prerequisites(prerequisites), affectedOutputs(outputs) {}
};
// Required input groups are ANDed; each group's input choices are ORed.
// affectedOutputs uses the same AND-of-OR groups for used outputs.
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
  for (const auto& group : control.affectedOutputs) {
    bool met = false;
    for (int output : group.anyOf) met |= outputUsed(output);
    if (!met) return false;
  }
  return true;
}
struct Spec {
  std::string slug;
  std::vector<Control> modulationInputs;
  std::vector<Output> outputs;
  std::vector<Control> parameters;
  std::vector<Range> initial;
  std::vector<Control> inputRules;
};

// Input rules inherit the requirements of the outputs they affect. This is
// why Delay CV does not need to repeat OUT's dependency on IN in the JSON.
template <class InputConnected, class OutputUsed>
inline bool portUseful(const Spec& spec, const Control& control,
                       InputConnected inputConnected, OutputUsed outputUsed) {
  std::set<int> active;
  std::function<bool(int)> readyInput = [&](int id) {
    if (!inputConnected(id) || !active.insert(id).second) return false;
    bool ready = true;
    for (const auto& input : spec.inputRules)
      if (input.id == id) ready &= controlUseful(input, readyInput, outputUsed);
    active.erase(id);
    return ready;
  };
  bool anyOutputUsed = false;
  for (const auto& output : spec.outputs)
    anyOutputUsed |= outputUsed(output.port);
  auto readyOutput = [&](int id) {
    // An unpatched captured module may still be grown or have controls exposed.
    // Explicit output dependencies (such as PWM -> Square) remain mandatory.
    if (!outputUsed(id) && (anyOutputUsed || !control.affectedOutputs.empty()))
      return false;
    for (const auto& output : spec.outputs)
      if (output.port == id) {
        for (const auto& group : output.requirements) {
          bool met = false;
          for (int input : group.anyOf) met |= readyInput(input);
          if (!met) return false;
        }
        return true;
      }
    return false;
  };
  if (!controlUseful(control, readyInput, readyOutput)) return false;
  for (const auto& output : spec.outputs)
    if (readyOutput(output.port)) return true;
  return false;
}
// Plan prerequisites before connecting a processor. No voltage/signal types.
// All groups must hold; choose one usable alternative from each group.
template <class Pick>
inline bool requiredInputs(const Spec& spec, const Output& output, Pick pick,
                           std::vector<int>& result) {
  std::set<int> active, connected;
  std::function<bool(int)> add;
  std::function<bool(const std::vector<Requirement>&)> satisfy;
  satisfy = [&](const std::vector<Requirement>& groups) {
    for (const auto& group : groups) {
      if (group.anyOf.empty()) return false;
      bool met = false;
      for (int port : group.anyOf) met |= connected.count(port) != 0;
      if (met) continue;
      size_t start = size_t(pick(int(group.anyOf.size())));
      for (size_t i = 0; i < group.anyOf.size() && !met; ++i) {
        auto before = connected;
        auto size = result.size();
        met = add(group.anyOf[(start + i) % group.anyOf.size()]);
        if (!met) {
          connected = before;
          result.resize(size);
        }
      }
      if (!met) return false;
    }
    return true;
  };
  add = [&](int port) {
    if (connected.count(port)) return true;
    if (!active.insert(port).second) return false;
    bool valid = true;
    for (const auto& rule : spec.inputRules)
      if (rule.id == port) {
        for (const auto& group : rule.affectedOutputs)
          valid &= std::find(group.anyOf.begin(), group.anyOf.end(),
                             output.port) != group.anyOf.end();
        if (valid) valid = satisfy(rule.prerequisites);
      }
    active.erase(port);
    if (valid) {
      connected.insert(port);
      result.push_back(port);
    }
    return valid;
  };
  return satisfy(output.requirements);
}
#include "BeekapooPortRules.inc"
// Parameter choices and initialization are generation policy, not port rules.
// Keep their established ranges and candidate inputs when migrating topology.
inline const std::vector<Spec>& library() {
  static const std::vector<Spec> specs = [] {
    std::vector<Spec> result;
    auto add = [&](const char* slug, std::vector<int> inputs,
                   std::vector<Control> parameters,
                   std::vector<Range> initial) {
      Spec s = portLibrary().at({"Fundamental", slug});
      s.modulationInputs.erase(
          std::remove_if(s.modulationInputs.begin(), s.modulationInputs.end(),
                         [&](const Control& c) {
                           return std::find(inputs.begin(), inputs.end(),
                                            c.id) == inputs.end();
                         }),
          s.modulationInputs.end());
      s.parameters = std::move(parameters);
      s.initial = std::move(initial);
      result.push_back(std::move(s));
    };
    add("VCO", {0, 1, 3}, {2, {5, {}, {{{3}}}}},
        {{2, -24, 12},
         Range::modulation(4),
         {5, .25f, .75f},
         Range::modulation(6)});
    add("LFO", {0, 2, 3, 4}, {2, {5, {}, {{{3}}}}},
        {{2, -3, 2},
         Range::modulation(3),
         {5, .3f, .7f},
         Range::modulation(6)});
    add("Noise", {}, {}, {});
    add("VCF", {0, 1}, {0, 2},
        {{0, .45f, .8f},
         {2, .05f, .4f},
         Range::modulation(3),
         Range::modulation(5),
         Range::modulation(6)});
    add("VCA-1", {0}, {0}, {{0, .5f, .85f}});
    add("Delay", {0, 2}, {0, 1, 3},
        {{0, .45f, .75f},
         {1, .1f, .45f},
         {3, .2f, .55f},
         Range::modulation(4),
         Range::modulation(5),
         Range::modulation(6),
         Range::modulation(7)});
    add("ADSR", {0, 1, 2, 3}, {0, 1, 2, 3},
        {{0, .15f, .45f},
         {1, .2f, .5f},
         {2, .3f, .8f},
         {3, .2f, .6f},
         Range::modulation(4),
         Range::modulation(5),
         Range::modulation(6),
         Range::modulation(7)});
    add("VCMixer", {0, 5, 6, 7, 8},
        {{0, {}, {{{0}}}},
         {1, {{{1}}}, {{{0, 1}}}},
         {2, {{{2}}}, {{{0, 2}}}},
         {3, {{{3}}}, {{{0, 3}}}},
         {4, {{{4}}}, {{{0, 4}}}}},
        {{0, .6f, 1}, {1, .7f, 1}, {2, .7f, 1}, {3, .7f, 1}, {4, .7f, 1}});
    return result;
  }();
  return specs;
}
}  // namespace beekapoo_patch
