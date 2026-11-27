#pragma once
#include <cmath>
#include <map>
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
  std::map<ModuleKey, std::pair<float, float>> positions;
};
// Global, UI-thread-only choices. Patches store the selected stable ID, so
// renaming a set does not change which set an existing Beek uses.
struct ModuleSets {
  std::vector<ModuleSet> sets;
  std::string defaultId = "default", path, error;
  static bool isReadOnly(const std::string& id) { return id == "default"; }
  explicit ModuleSets(std::string file) : path(std::move(file)) {
    auto* root = json_load_file(path.c_str(), 0, nullptr);
    auto* list = json_object_get(root, "sets");
    for (size_t i = 0; i < json_array_size(list); ++i) {
      auto* item = json_array_get(list, i);
      auto* id = json_string_value(json_object_get(item, "id"));
      auto* name = json_string_value(json_object_get(item, "name"));
      if (!id || !*id || !name || !*name || isReadOnly(id) || find(id))
        continue;
      ModuleSet set{id, name, {}};
      auto* models = json_object_get(item, "modules");
      for (size_t n = 0; n < json_array_size(models); ++n) {
        auto* model = json_array_get(models, n);
        auto* plugin = json_string_value(json_object_get(model, "plugin"));
        auto* slug = json_string_value(json_object_get(model, "model"));
        if (plugin && *plugin && slug && *slug) {
          ModuleKey key{plugin, slug};
          set.modules.insert(key);
          auto* x = json_object_get(model, "x");
          auto* y = json_object_get(model, "y");
          if (json_is_number(x) && json_is_number(y)) {
            double px = json_number_value(x), py = json_number_value(y);
            if (std::isfinite(px) && std::isfinite(py) && px >= 0 && py >= 0 &&
                px <= 100000 && py <= 100000)
              set.positions[key] = {float(px), float(py)};
          }
        }
      }
      sets.push_back(std::move(set));
    }
    if (!find("fundamental")) {
      ModuleSet initial{"fundamental", "Fundamental", {}};
      for (const auto& spec : beekapoo_patch::library())
        initial.modules.insert({"Fundamental", spec.slug});
      sets.insert(sets.begin(), std::move(initial));
    }
    ModuleSet standard{"default", "Default", {}};
    for (const auto& spec : beekapoo_patch::library())
      standard.modules.insert({"Fundamental", spec.slug});
    sets.insert(sets.begin(), std::move(standard));
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
      if (copy) {
        next.modules = original->modules;
        next.positions = original->positions;
      }
      next.name = copy ? original->name + " copy" : "New set";
    } else
      next.name = "New set";
    auto id = next.id;
    sets.push_back(std::move(next));
    save();
    return id;
  }
  bool remove(const std::string& id) {
    if (isReadOnly(id) || id == "fundamental") return false;
    for (auto it = sets.begin(); it != sets.end(); ++it)
      if (it->id == id) {
        sets.erase(it);
        if (defaultId == id) defaultId = "default";
        save();
        return true;
      }
    return false;
  }
  bool save() {
    auto* root = json_pack("{s:i,s:s,s:[]}", "version", 1, "default",
                           defaultId.c_str(), "sets");
    for (const auto& set : sets) {
      // Built-in membership always comes from the shipped standard list.
      if (isReadOnly(set.id)) continue;
      auto* item = json_pack("{s:s,s:s,s:[]}", "id", set.id.c_str(), "name",
                             set.name.c_str(), "modules");
      for (const auto& key : set.modules) {
        auto* model = json_pack("{s:s,s:s}", "plugin", key.plugin.c_str(),
                                "model", key.model.c_str());
        auto p = set.positions.find(key);
        if (p != set.positions.end()) {
          json_object_set_new(model, "x", json_real(p->second.first));
          json_object_set_new(model, "y", json_real(p->second.second));
        }
        json_array_append_new(json_object_get(item, "modules"), model);
      }
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
