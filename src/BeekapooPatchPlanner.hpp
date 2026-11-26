#pragma once
#include <algorithm>
#include <cstdint>
#include <random>
#include <set>
#include <string>
#include <vector>

#include "BeekapooPatchCatalog.hpp"

// Standalone graph planning. Port numbers belong only to the separate catalog;
// no Rack modules, JSON, UI objects or engine state are needed here.
namespace beekapoo_patch {
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
struct ExposureCounts {
  int inputs, outputs, parameters;
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
  bool useful(const Plan& p, int node, const Control& control) const {
    return controlUseful(
        control,
        [&](int input) {
          for (const auto& c : p.cables)
            if (c.to == Endpoint{node, input}) return true;
          return false;
        },
        [&](int output) {
          for (auto e : p.outputs)
            if (e == Endpoint{node, output}) return true;
          for (const auto& c : p.cables)
            if (c.from == Endpoint{node, output}) return true;
          return false;
        });
  }
  int chooseOutput(const Spec& spec, bool needsInput) {
    int priority = -1;
    std::vector<int> choices;
    for (int i = 0; i < int(spec.outputs.size()); ++i) {
      const auto& out = spec.outputs[i];
      if (!out.requirements.empty() != needsInput) continue;
      if (out.priority > priority) {
        priority = out.priority;
        choices.clear();
      }
      if (out.priority == priority) choices.push_back(i);
    }
    return choices.empty() ? -1 : choices[pick(int(choices.size()))];
  }

 public:
  explicit Planner(uint32_t seed, const std::vector<Spec>& catalog = library())
      : random(seed), specs(catalog) {}
  Plan make(int count = 0, const ExposureCounts* exact = nullptr) {
    count = count ? std::max(3, std::min(11, count)) : moduleCount(random());
    if (exact) {
      if (exact->inputs < 0 || exact->outputs < 0 || exact->parameters < 0)
        return {};
      // Dense existing panels may need more processors than a fresh Beek.
      count = std::max(
          count, std::max((exact->parameters + 3) / 4,
                          std::max((exact->inputs + 1) / 2, exact->outputs)));
    }
    std::vector<int> sources, processors;
    for (int i = 0; i < int(specs.size()); ++i) {
      bool free = false, required = false;
      for (const auto& out : specs[i].outputs) {
        free |= out.requirements.empty();
        required |= !out.requirements.empty();
      }
      if (free) sources.push_back(i);
      if (required) processors.push_back(i);
    }
    if (sources.empty()) return {};
    for (int attempt = 0; attempt < 256; ++attempt) {
      // Retry with more capacity rather than silently changing the panel.
      if (exact && attempt && attempt % 8 == 0) ++count;
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
        const auto& spec = specs[p.modules[node]];
        int index = this->chooseOutput(spec, false);
        return Endpoint{node, index < 0 ? -1 : spec.outputs[index].port};
      };
      Endpoint main = chooseOutput(0);
      p.outputs = {main};
      for (int i = 1; i < count; ++i) {
        std::vector<Endpoint> targets;
        for (int j = 0; j < i; ++j) {
          for (const auto& in : specs[p.modules[j]].modulationInputs)
            if (!occupied.count({j, in.id}) && useful(p, j, in))
              targets.push_back({j, in.id});
        }
        // Extra generators modulate a live path. Otherwise extend that path
        // through a processor. No node is added as unused decoration.
        bool modulator = !targets.empty() &&
                         (processors.empty() || (i > 1 && pick(100) < 45));
        if (!modulator && processors.empty()) break;
        if (modulator) {
          auto choices = sources;
          int type = choices[pick(int(choices.size()))];
          p.modules.push_back(type);
          Endpoint output = chooseOutput(i);
          wire(output, targets[pick(int(targets.size()))]);
        } else {
          int type = processors[pick(int(processors.size()))];
          p.modules.push_back(type);
          int outputIndex = this->chooseOutput(specs[type], true);
          if (outputIndex < 0) break;
          const auto& chosen = specs[type].outputs[outputIndex];
          std::vector<int> needed;
          if (!requiredInputs(
                  specs[type], chosen, [this](int n) { return pick(n); },
                  needed))
            break;
          for (int input : needed) wire(main, {i, input});
          main = {i, chosen.port};
          p.outputs = {main};
        }
      }
      if (int(p.modules.size()) != count) continue;
      if (exact && exact->outputs > 1) {
        std::vector<Endpoint> additional;
        for (int i = 0; i < count; ++i)
          for (const auto& out : specs[p.modules[i]].outputs) {
            Endpoint e{i, out.port};
            if (e == main) continue;
            bool ready = true;
            for (const auto& req : out.requirements) {
              bool connected = false;
              for (int input : req.anyOf)
                connected |= occupied.count({i, input});
              ready &= connected;
            }
            if (ready) additional.push_back(e);
          }
        shuffle(additional);
        if (int(additional.size()) < exact->outputs - 1) continue;
        p.outputs.insert(p.outputs.end(), additional.begin(),
                         additional.begin() + exact->outputs - 1);
      }
      if (exact && exact->outputs == 0) p.outputs.clear();
      for (int i = 0; i < count; ++i) {
        for (const auto& in : specs[p.modules[i]].modulationInputs)
          if (!occupied.count({i, in.id}) && useful(p, i, in))
            p.inputs.push_back({i, in.id});
        std::vector<int> eligible;
        for (const auto& param : specs[p.modules[i]].parameters)
          if (useful(p, i, param)) eligible.push_back(param.id);
        if (exact) {
          for (int id : eligible) p.parameters.push_back({i, id});
        } else if (!eligible.empty())
          p.parameters.push_back({i, eligible[pick(int(eligible.size()))]});
      }
      int inputCount =
          exact ? exact->inputs
                : (attempt < 128 ? 3 : std::min(3, int(p.inputs.size())));
      int paramCount =
          exact ? exact->parameters : std::min(4, int(p.parameters.size()));
      if (int(p.inputs.size()) < inputCount ||
          int(p.parameters.size()) < paramCount)
        continue;
      shuffle(p.inputs);
      p.inputs.resize(inputCount);
      shuffle(p.parameters);
      p.parameters.resize(paramCount);
      if (!validate(p).empty()) continue;
      // A deliberately output-free panel still gets a connected inner patch.
      if (exact && exact->outputs == 0) p.outputs.clear();
      return p;
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
      const auto& inputs = specs[p.modules[c.to.node]].inputRules;
      if (!inputs.empty() &&
          std::none_of(inputs.begin(), inputs.end(), [&](const Control& rule) {
            return rule.id == c.to.port;
          }))
        return "Unknown input";
      if (!occupied.insert({c.to.node, c.to.port}).second)
        return "Stacked input";
    }
    for (auto e : p.inputs)
      if (!validNode(e) || occupied.count({e.node, e.port}))
        return "Invalid exposed input";
    for (const auto& c : p.cables)
      for (const auto& rule : specs[p.modules[c.to.node]].inputRules)
        if (rule.id == c.to.port && !useful(p, c.to.node, rule))
          return "Unused control input";
    for (auto e : p.inputs) {
      bool found = false;
      for (const auto& rule : specs[p.modules[e.node]].modulationInputs)
        if (rule.id == e.port && useful(p, e.node, rule)) found = true;
      if (!found) return "Unused exposed input";
    }
    for (auto e : p.parameters) {
      if (!validNode(e)) return "Invalid exposed parameter";
      bool found = false;
      for (const auto& rule : specs[p.modules[e.node]].parameters)
        if (rule.id == e.port && useful(p, e.node, rule)) found = true;
      if (!found) return "Unused exposed parameter";
    }
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
            found |= output(c.from) != nullptr;
          }
        if (!found) return "Missing required input";
      }
    }
    std::set<int> live;
    for (auto e : p.outputs) live.insert(e.node);
    for (size_t pass = 0; pass < p.modules.size(); ++pass)
      for (const auto& c : p.cables)
        if (live.count(c.to.node)) live.insert(c.from.node);
    if (!p.outputs.empty() && live.size() != p.modules.size())
      return "Module has no path to an exposed output";
    return "";
  }
};
}  // namespace beekapoo_patch
