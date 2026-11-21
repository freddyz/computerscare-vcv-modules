#pragma once
#include <algorithm>
#include <cstdint>
#include <random>
#include <set>
#include <string>
#include <vector>

// Standalone graph planning. Port numbers belong only to the catalog below;
// no Rack modules, JSON, UI objects or engine state are needed here.
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
};
struct Range {
  int param;
  float low, high;
};
struct ControlInput {
  int port;
  std::vector<Requirement> prerequisites;
  std::vector<int> affectedOutputs;
  ControlInput(int port, std::vector<Requirement> prerequisites = {},
               std::vector<int> outputs = {})
      : port(port), prerequisites(prerequisites), affectedOutputs(outputs) {}
};
struct Spec {
  const char* slug;
  std::vector<ControlInput> modulationInputs;
  std::vector<Output> outputs;
  std::vector<int> parameters;
  std::vector<Range> initial;
};
inline const std::vector<Spec>& library() {
  static const std::vector<Spec> specs = {
      {"VCO",
       {0, 1, 3},
       {{0, Signal::sound, {}},
        {1, Signal::sound, {}},
        {2, Signal::sound, {}},
        {3, Signal::sound, {}}},
       {2, 5},
       {{2, -24, 12}, {4, .02f, .12f}, {5, .25f, .75f}, {6, .05f, .2f}}},
      {"LFO",
       {0, 2, 3, 4},
       {{0, Signal::modulation, {}},
        {1, Signal::modulation, {}},
        {2, Signal::modulation, {}},
        {3, Signal::gate, {}}},
       {2, 5},
       {{2, -3, 2}, {3, .05f, .2f}, {5, .3f, .7f}, {6, .05f, .2f}}},
      {"Noise",
       {},
       {{0, Signal::sound, {}}, {1, Signal::sound, {}}, {2, Signal::sound, {}}},
       {},
       {}},
      {"VCF",
       {0, 1},
       {{0, Signal::sound, {{{3}, Signal::sound}}},
        {1, Signal::sound, {{{3}, Signal::sound}}}},
       {0, 2},
       {{0, .45f, .8f}, {2, .05f, .4f}, {3, .04f, .14f}, {5, .05f, .15f}}},
      {"VCA-1",
       {0},
       {{0, Signal::sound, {{{1}, Signal::sound}}}},
       {0},
       {{0, .5f, .85f}}},
      {"Delay",
       {0, 2},
       {{0, Signal::sound, {{{4}, Signal::sound}}}},
       {0, 1, 3},
       {{0, .45f, .75f},
        {1, .1f, .45f},
        {3, .2f, .55f},
        {4, .02f, .08f},
        {6, .05f, .2f}}},
      {"ADSR",
       {0, 1, 2, 3},
       {{0, Signal::modulation, {{{4}, Signal::gate}}}},
       {0, 1, 2, 3},
       {{0, .15f, .45f}, {1, .2f, .5f}, {2, .3f, .8f}, {3, .2f, .6f}}},
      // Mix needs at least one channel signal. A direct output needs its own
      // channel signal, never just its CV. CV remains optional at unity gain.
      {"VCMixer",
       {{0, {}, {0}},
        {5, {{{1}, Signal::sound}}, {0, 1}},
        {6, {{{2}, Signal::sound}}, {0, 2}},
        {7, {{{3}, Signal::sound}}, {0, 3}},
        {8, {{{4}, Signal::sound}}, {0, 4}}},
       {{0, Signal::sound, {{{1, 2, 3, 4}, Signal::sound}}},
        {1, Signal::sound, {{{1}, Signal::sound}}},
        {2, Signal::sound, {{{2}, Signal::sound}}},
        {3, Signal::sound, {{{3}, Signal::sound}}},
        {4, Signal::sound, {{{4}, Signal::sound}}}},
       {0, 1, 2, 3, 4},
       {{0, .6f, 1}, {1, .7f, 1}, {2, .7f, 1}, {3, .7f, 1}, {4, .7f, 1}}}};
  return specs;
}
struct Endpoint {
  int node, port;
};
inline bool operator==(Endpoint a, Endpoint b) {
  return a.node == b.node && a.port == b.port;
}
struct Cable {
  Endpoint from, to;
};
struct Plan {
  std::vector<int> modules;
  std::vector<Cable> cables;
  std::vector<Endpoint> inputs, outputs, parameters;
};
inline int moduleCount(unsigned value) {
  const int weights[] = {14, 22, 20, 14, 10, 7, 6, 4, 3};
  value %= 100;
  for (int i = 0; i < 9; ++i) {
    if (value < unsigned(weights[i])) return i + 3;
    value -= weights[i];
  }
  return 11;
}
class Planner {
  std::mt19937 random;
  int pick(int n) { return int(random() % unsigned(n)); }
  template <class T>
  void shuffle(std::vector<T>& v) {
    for (int i = int(v.size()) - 1; i > 0; --i) std::swap(v[i], v[pick(i + 1)]);
  }
  const std::vector<Spec>& specs;
  bool useful(const Plan& p, int node, const ControlInput& input) const {
    for (const auto& req : input.prerequisites) {
      bool found = false;
      for (const auto& c : p.cables)
        found |= c.to.node == node &&
                 std::find(req.anyOf.begin(), req.anyOf.end(), c.to.port) !=
                     req.anyOf.end();
      if (!found) return false;
    }
    if (input.affectedOutputs.empty()) return true;
    auto matches = [&](Endpoint e) {
      return e.node == node && std::find(input.affectedOutputs.begin(),
                                         input.affectedOutputs.end(),
                                         e.port) != input.affectedOutputs.end();
    };
    for (auto e : p.outputs)
      if (matches(e)) return true;
    for (const auto& c : p.cables)
      if (matches(c.from)) return true;
    return false;
  }

 public:
  explicit Planner(uint32_t seed, const std::vector<Spec>& catalog = library())
      : random(seed), specs(catalog) {}
  Plan make(int count = 0) {
    count = count ? std::max(3, std::min(11, count)) : moduleCount(random());
    std::vector<int> sources, processors, envelopes;
    for (int i = 0; i < int(specs.size()); ++i) {
      bool free = false, sound = false, gate = false;
      for (const auto& out : specs[i].outputs) {
        free |= out.requirements.empty();
        for (const auto& req : out.requirements) {
          sound |= req.signal == Signal::sound;
          gate |= req.signal == Signal::gate;
        }
      }
      if (free)
        sources.push_back(i);
      else if (gate && !sound)
        envelopes.push_back(i);
      else if (sound && !gate)
        processors.push_back(i);
    }
    if (sources.empty() || processors.empty()) return {};
    for (int attempt = 0; attempt < 256; ++attempt) {
      Plan p;
      std::set<std::pair<int, int>> occupied;
      auto wire = [&](Endpoint a, Endpoint b) {
        if (a.node == b.node || !occupied.insert({b.node, b.port}).second)
          return false;
        p.cables.push_back({a, b});
        return true;
      };
      p.modules.push_back(sources[pick(int(sources.size()))]);
      auto chooseOutput = [&](int node) {
        const auto& out = specs[p.modules[node]].outputs;
        return Endpoint{node, out[pick(int(out.size()))].port};
      };
      Endpoint main = chooseOutput(0);
      p.outputs = {main};
      for (int i = 1; i < count; ++i) {
        std::vector<Endpoint> targets, gates;
        for (int j = 0; j < i; ++j) {
          for (const auto& in : specs[p.modules[j]].modulationInputs)
            if (!occupied.count({j, in.port}) && useful(p, j, in))
              targets.push_back({j, in.port});
          for (const auto& out : specs[p.modules[j]].outputs)
            if (out.signal == Signal::gate) gates.push_back({j, out.port});
        }
        // Extra generators modulate a live path. Otherwise extend that path
        // through a processor. No node is added as unused decoration.
        bool modulator = i > 1 && !targets.empty() && pick(100) < 45;
        if (modulator) {
          auto choices = sources;
          if (!gates.empty())
            choices.insert(choices.end(), envelopes.begin(), envelopes.end());
          int type = choices[pick(int(choices.size()))];
          p.modules.push_back(type);
          Endpoint output = chooseOutput(i);
          for (const auto& out : specs[type].outputs)
            if (out.port == output.port)
              for (const auto& req : out.requirements)
                wire(gates[pick(int(gates.size()))],
                     {i, req.anyOf[pick(int(req.anyOf.size()))]});
          wire(output, targets[pick(int(targets.size()))]);
        } else {
          int type = processors[pick(int(processors.size()))];
          p.modules.push_back(type);
          const auto& outs = specs[type].outputs;
          const auto& chosen = outs[pick(int(outs.size()))];
          for (const auto& req : chosen.requirements)
            wire(main, {i, req.anyOf[pick(int(req.anyOf.size()))]});
          main = {i, chosen.port};
          p.outputs = {main};
        }
      }
      for (int i = 0; i < count; ++i) {
        for (const auto& in : specs[p.modules[i]].modulationInputs)
          if (!occupied.count({i, in.port}) && useful(p, i, in))
            p.inputs.push_back({i, in.port});
        if (!specs[p.modules[i]].parameters.empty()) {
          const auto& v = specs[p.modules[i]].parameters;
          p.parameters.push_back({i, v[pick(int(v.size()))]});
        }
      }
      if (p.inputs.size() < 3) continue;
      shuffle(p.inputs);
      p.inputs.resize(3);
      shuffle(p.parameters);
      if (p.parameters.size() > 4) p.parameters.resize(4);
      if (validate(p).empty()) return p;
    }
    return {};
  }
  std::string validate(const Plan& p) const {
    if (p.modules.empty()) return "Empty patch";
    for (int type : p.modules)
      if (type < 0 || type >= int(specs.size())) return "Unknown module";
    auto validNode = [&](Endpoint e) {
      return e.node >= 0 && e.node < int(p.modules.size()) && e.port >= 0;
    };
    auto output = [&](Endpoint e) -> const Output* {
      if (!validNode(e)) return nullptr;
      for (const auto& o : specs[p.modules[e.node]].outputs)
        if (o.port == e.port) return &o;
      return nullptr;
    };
    std::set<std::pair<int, int>> occupied;
    for (const auto& c : p.cables) {
      if (!output(c.from) || !validNode(c.to) || c.from.node == c.to.node)
        return "Invalid cable";
      if (!occupied.insert({c.to.node, c.to.port}).second)
        return "Stacked input";
    }
    for (auto e : p.inputs)
      if (!validNode(e) || occupied.count({e.node, e.port}))
        return "Invalid exposed input";
    // Validate requirements for every output actually used, including the
    // external output. A cable into a CV alone cannot make a mixer live.
    std::vector<Endpoint> used = p.outputs;
    for (const auto& c : p.cables) used.push_back(c.from);
    for (auto e : used) {
      const auto* o = output(e);
      if (!o) return "Invalid output";
      for (const auto& r : o->requirements) {
        bool found = false;
        for (const auto& c : p.cables)
          if (c.to.node == e.node && std::find(r.anyOf.begin(), r.anyOf.end(),
                                               c.to.port) != r.anyOf.end()) {
            const auto* source = output(c.from);
            found |= source && (r.signal != Signal::gate ||
                                source->signal == Signal::gate);
          }
        if (!found) return "Missing required input";
      }
    }
    std::set<int> live;
    for (auto e : p.outputs) live.insert(e.node);
    for (size_t pass = 0; pass < p.modules.size(); ++pass)
      for (const auto& c : p.cables)
        if (live.count(c.to.node)) live.insert(c.from.node);
    if (live.size() != p.modules.size())
      return "Module has no path to an exposed output";
    return "";
  }
};
}  // namespace beekapoo_patch
