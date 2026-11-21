#pragma once
#include <set>
#include <tuple>

#include "BeekapooPatchCatalog.hpp"
#include "rack.hpp"

namespace beekapoo {
struct ModuleKey {
  std::string plugin, model;
  bool operator<(const ModuleKey& b) const {
    return std::tie(plugin, model) < std::tie(b.plugin, b.model);
  }
  bool operator==(const ModuleKey& b) const {
    return plugin == b.plugin && model == b.model;
  }
};
struct ModuleSet {
  std::string id, name;
  std::set<ModuleKey> modules;
};
// Global, UI-thread-only choices. Patches store the selected stable ID, so
// renaming a set does not change which set an existing Beek uses.
struct ModuleSets {
  std::vector<ModuleSet> sets;
  std::string defaultId = "fundamental", path, error;
  explicit ModuleSets(std::string file) : path(std::move(file)) {
    auto* root = json_load_file(path.c_str(), 0, nullptr);
    auto* list = json_object_get(root, "sets");
    for (size_t i = 0; i < json_array_size(list); ++i) {
      auto* item = json_array_get(list, i);
      auto* id = json_string_value(json_object_get(item, "id"));
      auto* name = json_string_value(json_object_get(item, "name"));
      if (!id || !*id || !name || !*name || find(id)) continue;
      ModuleSet set{id, name, {}};
      auto* models = json_object_get(item, "modules");
      for (size_t n = 0; n < json_array_size(models); ++n) {
        auto* model = json_array_get(models, n);
        auto* plugin = json_string_value(json_object_get(model, "plugin"));
        auto* slug = json_string_value(json_object_get(model, "model"));
        if (plugin && *plugin && slug && *slug)
          set.modules.insert({plugin, slug});
      }
      sets.push_back(std::move(set));
    }
    if (!find("fundamental")) {
      ModuleSet initial{"fundamental", "Fundamental", {}};
      for (const auto& spec : beekapoo_patch::library())
        initial.modules.insert({"Fundamental", spec.slug});
      sets.insert(sets.begin(), std::move(initial));
    }
    auto* chosen = json_string_value(json_object_get(root, "default"));
    if (chosen && find(chosen)) defaultId = chosen;
    json_decref(root);
  }
  ModuleSet* find(const std::string& id) {
    for (auto& set : sets)
      if (set.id == id) return &set;
    return nullptr;
  }
  std::string add(bool copy, const std::string& source = "") {
    ModuleSet next;
    do {
      next.id = string::f("set-%016llx", (unsigned long long)random::u64());
    } while (find(next.id));
    if (auto* original = find(source)) {
      if (copy) next.modules = original->modules;
      next.name = copy ? original->name + " copy" : "New set";
    } else
      next.name = "New set";
    auto id = next.id;
    sets.push_back(std::move(next));
    save();
    return id;
  }
  bool remove(const std::string& id) {
    if (id == "fundamental") return false;
    for (auto it = sets.begin(); it != sets.end(); ++it)
      if (it->id == id) {
        sets.erase(it);
        if (defaultId == id) defaultId = "fundamental";
        save();
        return true;
      }
    return false;
  }
  bool save() {
    auto* root = json_pack("{s:i,s:s,s:[]}", "version", 1, "default",
                           defaultId.c_str(), "sets");
    for (const auto& set : sets) {
      auto* item = json_pack("{s:s,s:s,s:[]}", "id", set.id.c_str(), "name",
                             set.name.c_str(), "modules");
      for (const auto& key : set.modules)
        json_array_append_new(
            json_object_get(item, "modules"),
            json_pack("{s:s,s:s}", "plugin", key.plugin.c_str(), "model",
                      key.model.c_str()));
      json_array_append_new(json_object_get(root, "sets"), item);
    }
    auto temporary = path + ".tmp";
    bool written = json_dump_file(root, temporary.c_str(), JSON_INDENT(2)) == 0;
    json_decref(root);
    if (written && system::rename(temporary, path)) {
      error.clear();
      return true;
    }
    system::remove(temporary);
    error = "Could not save module sets";
    WARN("Could not save Beekapoo module sets: %s", path.c_str());
    return false;
  }
};
inline ModuleSets& moduleSets() {
  static ModuleSets value(asset::user("computerscare-beekapoo-sets.json"));
  return value;
}
}  // namespace beekapoo
