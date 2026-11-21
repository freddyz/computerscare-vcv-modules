#pragma once
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace beekapoo {
inline uint64_t identityHash(const std::string& text) {
  uint64_t h = 14695981039346656037ull;
  for (unsigned char c : text) {
    h ^= c;
    h *= 1099511628211ull;
  }
  return h;
}
inline std::string identityRecord(const std::vector<std::string>& values) {
  std::string result;
  for (const auto& value : values)
    result += std::to_string(value.size()) + ":" + value;
  return result;
}
struct StyleCable {
  int from, out, to, in;
  std::string outsideFrom, outsideTo;
  StyleCable(int source, int output, int target, int input,
             std::string sourceType = "", std::string targetType = "")
      : from(source),
        out(output),
        to(target),
        in(input),
        outsideFrom(std::move(sourceType)),
        outsideTo(std::move(targetType)) {}
};
struct PatchIdentity {
  uint64_t modules = 0, types = 0, wiring = 0;
  uint64_t manufacturers = 0, manufacturerTypes = 0;
  bool operator==(const PatchIdentity& b) const {
    return modules == b.modules && types == b.types && wiring == b.wiring &&
           manufacturers == b.manufacturers &&
           manufacturerTypes == b.manufacturerTypes;
  }
  bool operator!=(const PatchIdentity& b) const { return !(*this == b); }
};
inline PatchIdentity patchIdentity(const std::vector<std::string>& modules,
                                   const std::vector<StyleCable>& allCables) {
  std::vector<StyleCable> cables;
  for (const auto& c : allCables)
    if (c.from >= 0 && c.to >= 0 && c.from < int(modules.size()) &&
        c.to < int(modules.size()))
      cables.push_back(c);
  PatchIdentity result;
  auto sorted = modules;
  std::sort(sorted.begin(), sorted.end());
  result.modules = identityHash(identityRecord(sorted));
  sorted.erase(std::unique(sorted.begin(), sorted.end()), sorted.end());
  result.types = identityHash(identityRecord(sorted));
  // Module descriptors begin with manufacturer, separated from the model by
  // a unit separator. Slash-separated names are also accepted by pure fixtures.
  std::vector<std::string> makers;
  for (const auto& type : modules) {
    size_t end = type.find('\x1f');
    if (end == std::string::npos) end = type.find('/');
    makers.push_back(type.substr(0, end));
  }
  std::sort(makers.begin(), makers.end());
  result.manufacturers = identityHash(identityRecord(makers));
  makers.erase(std::unique(makers.begin(), makers.end()), makers.end());
  result.manufacturerTypes = identityHash(identityRecord(makers));
  std::vector<uint64_t> signatures;
  for (const auto& type : modules) signatures.push_back(identityHash(type));
  auto endpoint = [&](int node, const std::string& outside) {
    return node >= 0 && node < int(signatures.size())
               ? std::to_string(signatures[node])
               : outside;
  };
  std::vector<std::vector<size_t>> adjacent(modules.size());
  for (size_t i = 0; i < cables.size(); ++i) {
    if (cables[i].from >= 0 && cables[i].from < int(modules.size()))
      adjacent[cables[i].from].push_back(i);
    if (cables[i].to >= 0 && cables[i].to < int(modules.size()) &&
        cables[i].to != cables[i].from)
      adjacent[cables[i].to].push_back(i);
  }
  // Refine duplicate module instances from their typed, port-labelled
  // neighbourhoods. IDs, insertion order and rack coordinates are excluded.
  for (int pass = 0; pass < std::min(8, int(modules.size())); ++pass) {
    auto next = signatures;
    for (int i = 0; i < int(modules.size()); ++i) {
      std::vector<std::string> incident;
      for (auto index : adjacent[i]) {
        const auto& cable = cables[index];
        if (cable.from == i)
          incident.push_back(identityRecord(
              {cable.from == cable.to ? "self-out" : "out",
               std::to_string(cable.out), endpoint(cable.to, cable.outsideTo),
               std::to_string(cable.in)}));
        if (cable.to == i)
          incident.push_back(
              identityRecord({cable.from == cable.to ? "self-in" : "in",
                              std::to_string(cable.in),
                              endpoint(cable.from, cable.outsideFrom),
                              std::to_string(cable.out)}));
      }
      std::sort(incident.begin(), incident.end());
      next[i] =
          identityHash(identityRecord({modules[i], identityRecord(incident)}));
    }
    signatures.swap(next);
  }
  std::vector<std::string> connections;
  for (const auto& cable : cables)
    connections.push_back(identityRecord(
        {endpoint(cable.from, cable.outsideFrom), std::to_string(cable.out),
         endpoint(cable.to, cable.outsideTo), std::to_string(cable.in)}));
  std::sort(connections.begin(), connections.end());
  std::vector<bool> visited(modules.size(), false);
  std::vector<std::string> components;
  for (int start = 0; start < int(modules.size()); ++start)
    if (!visited[start]) {
      std::vector<int> pending{start};
      std::vector<std::string> members;
      visited[start] = true;
      while (!pending.empty()) {
        int node = pending.back();
        pending.pop_back();
        members.push_back(modules[node]);
        for (auto edge : adjacent[node]) {
          int next =
              cables[edge].from == node ? cables[edge].to : cables[edge].from;
          if (next >= 0 && next < int(modules.size()) && !visited[next]) {
            visited[next] = true;
            pending.push_back(next);
          }
        }
      }
      std::sort(members.begin(), members.end());
      components.push_back(identityRecord(members));
    }
  std::sort(components.begin(), components.end());
  result.wiring = identityHash(identityRecord({std::to_string(result.modules),
                                               identityRecord(connections),
                                               identityRecord(components)}));
  return result;
}
inline uint64_t mixIdentity(uint64_t a, uint64_t b) {
  uint64_t h = a ^ (b + 0x9e3779b97f4a7c15ull + (a << 6) + (a >> 2));
  h ^= h >> 30;
  h *= 0xbf58476d1ce4e5b9ull;
  h ^= h >> 27;
  h *= 0x94d049bb133111ebull;
  return h ^ (h >> 31);
}
inline unsigned patchPalette(const PatchIdentity& patch) {
  return unsigned(
      mixIdentity(
          mixIdentity(patch.manufacturerTypes, patch.manufacturers),
          mixIdentity(patch.types, mixIdentity(patch.modules, patch.wiring))) %
      6);
}
}  // namespace beekapoo
