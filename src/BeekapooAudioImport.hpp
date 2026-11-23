#pragma once
// Translate hardware channels into native source jacks and shared input
// destinations before constructing any Rack modules. No custom relay module.
inline void exposeBeekAudioInterfaces(BeekPatchFile& file, json_t* group) {
  using Endpoint = std::pair<int64_t, int>;
  auto* cables = json_object_get(file.root, "cables");
  auto* nodes = json_object_get(file.root, "modules");
  auto* ports = json_object_get(group, "ports");
  std::map<int64_t, int> capacities;
  for (auto id : file.audio) {
    auto slug = beekJsonString(file.modules.at(id), "model");
    capacities[id] = slug == "AudioInterface2"    ? 2
                     : slug == "AudioInterface"   ? 8
                     : slug == "AudioInterface16" ? 16
                                                  : 64;
  }
  auto endpoint = [](json_t* c, bool input) -> Endpoint {
    return {json_integer_value(
                json_object_get(c, input ? "inputModuleId" : "outputModuleId")),
            int(json_integer_value(
                json_object_get(c, input ? "inputId" : "outputId")))};
  };
  std::map<Endpoint, std::vector<json_t*>> playback;
  std::map<Endpoint, std::vector<std::pair<Endpoint, int64_t>>> capture;
  for (size_t i = 0; i < json_array_size(cables); ++i) {
    auto* c = json_array_get(cables, i);
    for (bool input : {false, true}) {
      auto e = endpoint(c, input);
      if (!capacities.count(e.first)) continue;
      auto* port = json_object_get(c, input ? "inputId" : "outputId");
      if (!json_is_integer(port) || e.second < 0 ||
          e.second >= capacities.at(e.first))
        throw Exception("Invalid Audio interface channel");
    }
    if (capacities.count(endpoint(c, true).first))
      playback[endpoint(c, true)].push_back(c);
  }
  std::set<Endpoint> exposedOutputs;
  auto exposeOutput = [&](Endpoint e) {
    if (exposedOutputs.insert(e).second)
      json_array_append_new(
          ports, json_pack("{s:I,s:i,s:b}", "module", json_int_t(e.first),
                           "port", e.second, "input", false));
  };
  // Most output channels simply expose the source's own output. A true sum
  // (or hardware input routed straight to output) needs a real summing point.
  // 8vert sums native cable stacks, retains polyphony and is unity here.
  size_t summed = 0;
  int64_t adapterId = -1;
  json_t* adapter = nullptr;
  std::map<Endpoint, Endpoint> playbackTargets;
  for (auto id : file.audio)
    for (int channel = 0; channel < capacities.at(id); ++channel) {
      auto it = playback.find({id, channel});
      if (it == playback.end()) continue;
      const auto& stack = it->second;
      auto source = endpoint(stack.front(), false);
      if (stack.size() == 1 && !capacities.count(source.first)) {
        exposeOutput(source);
        continue;
      }
      int slot = summed++ % 8;
      if (!slot) {
        do {
          adapterId = random::u64() % (1ull << 53);
        } while (file.modules.count(adapterId));
        adapter = json_pack("{s:I,s:s,s:s,s:[],s:[i,i]}", "id",
                            json_int_t(adapterId), "plugin", "Fundamental",
                            "model", "8vert", "params", "pos", 0, 0);
        file.modules[adapterId] = adapter;
        json_array_append_new(nodes, adapter);
      }
      json_array_append_new(json_object_get(adapter, "params"),
                            json_pack("{s:i,s:f}", "id", slot, "value", 1.));
      playbackTargets[{id, channel}] = {adapterId, slot};
      exposeOutput({adapterId, slot});
    }
  auto* retained = json_array();
  for (size_t i = 0; i < json_array_size(cables); ++i) {
    auto* c = json_array_get(cables, i);
    auto from = endpoint(c, false), to = endpoint(c, true);
    if (capacities.count(to.first)) {
      auto mapped = playbackTargets.find(to);
      if (mapped == playbackTargets.end())
        continue;  // replaced by a direct exposed source
      to = mapped->second;
    }
    if (capacities.count(from.first)) {
      capture[from].push_back(
          {to, json_integer_value(json_object_get(c, "id"))});
    } else {
      auto* copy = json_deep_copy(c);
      json_object_set_new(copy, "inputModuleId", json_integer(to.first));
      json_object_set_new(copy, "inputId", json_integer(to.second));
      json_array_append_new(retained, copy);
    }
  }
  // Every used capture channel is one independent exposed input, even when
  // several hardware channels happen to feed the same internal destination.
  int route = 0;
  for (size_t i = 0; i < json_array_size(ports); ++i) {
    auto* saved = json_object_get(json_array_get(ports, i), "route");
    if (json_is_integer(saved))
      route = std::max(route, int(json_integer_value(saved)) + 1);
  }
  for (size_t interface = 0; interface < file.audio.size(); ++interface) {
    auto id = file.audio[interface];
    for (int channel = 0; channel < capacities.at(id); ++channel) {
      auto it = capture.find({id, channel});
      if (it == capture.end()) continue;
      if (route >= Module::routeCapacity)
        throw Exception("Too many shared audio inputs");
      const auto& destinations = it->second;
      auto first = destinations.front();
      auto label = "Audio " + std::to_string(capacities.at(id)) + " #" +
                   std::to_string(interface + 1) + " input " +
                   std::to_string(channel + 1);
      auto* p =
          json_pack("{s:I,s:i,s:b,s:i,s:I,s:I,s:s,s:[]}", "module",
                    json_int_t(first.first.first), "port", first.first.second,
                    "input", true, "route", route++, "bridge", json_int_t(-1),
                    "shared", json_int_t(random::u64() % (1ull << 53)), "label",
                    label.c_str(), "targets");
      for (size_t j = 1; j < destinations.size(); ++j) {
        auto e = destinations[j].first;
        json_array_append_new(
            json_object_get(p, "targets"),
            json_pack("{s:I,s:i,s:I}", "module", json_int_t(e.first), "port",
                      e.second, "bridge", json_int_t(-1)));
      }
      json_array_append_new(ports, p);
    }
    file.modules.erase(id);
  }
  for (size_t i = json_array_size(nodes); i-- > 0;)
    if (capacities.count(json_integer_value(
            json_object_get(json_array_get(nodes, i), "id"))))
      json_array_remove(nodes, i);
  json_object_set_new(file.root, "cables", retained);
}
