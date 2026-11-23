#pragma once
// Flatten saved containers before instantiating anything. Source module IDs
// and assets remain intact until the normal importer remaps them together.
constexpr int64_t beekEntirePatch = -2;

inline int64_t combineBeekPatch(BeekPatchFile& file, json_t* snapshot,
                                bool convertAudio) {
  DEFER({ json_decref(snapshot); });
  int64_t owner = 0;
  while (file.modules.count(owner)) ++owner;
  auto* data = json_object_get(snapshot, "data");
  auto* group = json_object();
  json_object_set_new(data, "group", group);
  json_object_del(data, "contents");
  json_object_set_new(group, "owner", json_integer(owner));
  for (const char* key : {"members", "ports", "parameters", "randomLocks",
                          "panelCables", "internalCables", "attenuatedCables"})
    json_object_set_new(group, key, json_array());
  std::set<int64_t> wrappers(file.beeks.begin(), file.beeks.end()), children;
  std::map<std::pair<int64_t, int>, int> routes;
  int nextRoute = 0;
  for (auto id : file.beeks) {
    auto* saved =
        json_object_get(json_object_get(file.modules.at(id), "data"), "group");
    auto* members = json_object_get(saved, "members");
    if (!json_is_array(members)) throw Exception("Missing Beek membership");
    for (size_t i = 0; i < json_array_size(members); ++i) {
      auto member =
          json_integer_value(json_object_get(json_array_get(members, i), "id"));
      if (!file.modules.count(member) || wrappers.count(member) ||
          !children.insert(member).second)
        throw Exception("Invalid or overlapping Beek membership");
    }
    for (const char* key : {"ports", "parameters", "randomLocks", "panelCables",
                            "internalCables", "attenuatedCables"}) {
      auto* entries = json_object_get(saved, key);
      for (size_t i = 0; i < json_array_size(entries); ++i) {
        auto* entry = json_deep_copy(json_array_get(entries, i));
        json_array_append_new(json_object_get(group, key), entry);
        if (std::string(key) != "ports") continue;
        auto* route = json_object_get(entry, "route");
        if (!json_is_true(json_object_get(entry, "input")) ||
            !json_is_integer(route) || json_integer_value(route) < 0)
          continue;
        int old = json_integer_value(route);
        if (old >= Module::routeCapacity ||
            nextRoute >= Module::routeCapacity ||
            !routes.emplace(std::make_pair(id, old), nextRoute).second)
          throw Exception("Invalid or too many shared input routes");
        json_object_set_new(entry, "route", json_integer(nextRoute++));
      }
    }
  }
  auto* cables = json_object_get(file.root, "cables");
  for (size_t i = 0; i < json_array_size(cables); ++i) {
    auto* c = json_array_get(cables, i);
    auto input = json_integer_value(json_object_get(c, "inputModuleId"));
    auto output = json_integer_value(json_object_get(c, "outputModuleId"));
    if (wrappers.count(input)) {
      int port = json_integer_value(json_object_get(c, "inputId"));
      auto route = routes.find({input, port - Module::inputBase});
      if (route == routes.end())
        throw Exception(
            "Entire patch cannot combine patched Beek action inputs. "
            "Disconnect those action cables in the source patch first.");
      json_object_set_new(c, "inputModuleId", json_integer(owner));
      json_object_set_new(c, "inputId",
                          json_integer(Module::inputBase + route->second));
    }
    if (wrappers.count(output)) {
      int port = json_integer_value(json_object_get(c, "outputId"));
      auto route = routes.find({output, port});
      if (route == routes.end()) throw Exception("Invalid Beek input bridge");
      // The importer recreates these private bridge cables from the mappings.
      json_object_set_new(c, "outputModuleId", json_integer(owner));
      json_object_set_new(c, "outputId", json_integer(route->second));
    }
  }
  // Known MIDI mapping schemas may target legacy input gain parameters. Keep
  // those attached to the same route after merging the containers.
  for (const auto& item : file.modules) {
    auto* maps = json_object_get(json_object_get(item.second, "data"), "maps");
    for (size_t i = 0; i < json_array_size(maps); ++i) {
      auto* mapping = json_array_get(maps, i);
      auto target = json_integer_value(json_object_get(mapping, "moduleId"));
      if (!wrappers.count(target)) continue;
      int param = json_integer_value(json_object_get(mapping, "paramId"));
      auto route = routes.find({target, param - Module::INPUT_GAIN_BASE});
      if (route == routes.end())
        throw Exception(
            "Entire patch cannot combine MIDI mappings to Beek controls.");
      json_object_set_new(mapping, "moduleId", json_integer(owner));
      json_object_set_new(
          mapping, "paramId",
          json_integer(Module::INPUT_GAIN_BASE + route->second));
    }
  }
  for (auto id : wrappers) file.modules.erase(id);
  if (convertAudio) {
    std::set<int64_t> audio(file.audio.begin(), file.audio.end());
    for (const char* key : {"ports", "parameters", "randomLocks"}) {
      auto* entries = json_object_get(group, key);
      for (size_t i = json_array_size(entries); i-- > 0;)
        if (audio.count(json_integer_value(
                json_object_get(json_array_get(entries, i), "module"))))
          json_array_remove(entries, i);
    }
    exposeBeekAudioInterfaces(file, group);
  }
  for (const auto& item : file.modules) {
    auto* pos = json_object_get(item.second, "pos");
    double x = json_number_value(json_array_get(pos, 0)) * RACK_GRID_WIDTH;
    double y = json_number_value(json_array_get(pos, 1)) * RACK_GRID_HEIGHT;
    if (!std::isfinite(x) || !std::isfinite(y))
      throw Exception("Invalid module position");
    json_array_append_new(json_object_get(group, "members"),
                          json_pack("{s:I,s:f,s:f}", "id",
                                    json_int_t(item.first), "x", x, "y", y));
  }
  // Expose unused ports on previously unwrapped modules, as for ordinary
  // patch imports. Existing Beek mappings and locks have been copied above.
  if (!convertAudio && file.audio.empty()) {
    json_object_set_new(group, "exposeUnpatched", json_true());
    auto* unwrapped = json_array();
    for (const auto& item : file.modules)
      if (!children.count(item.first))
        json_array_append_new(unwrapped, json_integer(item.first));
    json_object_set_new(group, "exposeUnpatchedModules", unwrapped);
  }
  json_object_set_new(snapshot, "id", json_integer(owner));
  json_array_append(json_object_get(file.root, "modules"), snapshot);
  file.modules.emplace(owner, snapshot);
  return owner;
}
