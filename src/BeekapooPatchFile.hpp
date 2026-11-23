#pragma once
// Standard Rack archives, staged away from the current patch's autosave.
struct BeekTemporaryDirectory {
  std::string path;
  BeekTemporaryDirectory() {
    for (int attempt = 0; attempt < 32; ++attempt) {
      path =
          system::join(system::getTempDirectory(),
                       "computerscare-beek-" + std::to_string(random::u64()));
      if (system::createDirectory(path)) return;
    }
    throw Exception("Could not create temporary Beek directory");
  }
  ~BeekTemporaryDirectory() { system::removeRecursively(path); }
};
inline std::string beekJsonString(json_t* object, const char* key) {
  const char* value = json_string_value(json_object_get(object, key));
  return value ? value : "";
}
inline bool isBeekPatchModule(json_t* node) {
  auto slug = beekJsonString(node, "model");
  return beekJsonString(node, "plugin") == "computerscare" &&
         (slug == "computerscare-beekapoo" ||
          slug == "computerscare-peekaboo" ||
          slug == "computerscare-patch-abstractor");
}
inline bool isBeekAudioInterface(json_t* node) {
  auto slug = beekJsonString(node, "model");
  return (beekJsonString(node, "plugin") == "Core" &&
          (slug == "AudioInterface2" || slug == "AudioInterface" ||
           slug == "AudioInterface16")) ||
         (beekJsonString(node, "plugin") == "Stoermelder-P1" &&
          slug == "AudioInterface64");
}
struct BeekPatchFile {
  BeekTemporaryDirectory temporary;
  json_t* root = nullptr;
  std::map<int64_t, json_t*> modules;
  std::vector<int64_t> beeks, audio;
  explicit BeekPatchFile(const std::string& path) {
    json_error_t error;
    root = json_load_file(path.c_str(), 0, &error);
    if (!root) {
      system::unarchiveToDirectory(path, temporary.path);
      root = json_load_file(system::join(temporary.path, "patch.json").c_str(),
                            0, &error);
    }
    if (!root) throw Exception("Could not read Rack patch: %s", error.text);
    try {
      auto* list = json_object_get(root, "modules");
      if (!json_is_array(list) || !json_array_size(list))
        throw Exception("This file contains no modules");
      for (size_t i = 0; i < json_array_size(list); ++i) {
        auto* node = json_array_get(list, i);
        auto* id = json_object_get(node, "id");
        if (!json_is_integer(id) || json_integer_value(id) < 0 ||
            !modules.emplace(json_integer_value(id), node).second)
          throw Exception("Invalid or duplicate module ID in patch");
        if (isBeekPatchModule(node)) beeks.push_back(json_integer_value(id));
        if (isBeekAudioInterface(node)) audio.push_back(json_integer_value(id));
      }
      std::stable_sort(audio.begin(), audio.end(),
                       [this](int64_t a, int64_t b) {
                         auto* pa = json_object_get(modules.at(a), "pos");
                         auto* pb = json_object_get(modules.at(b), "pos");
                         double ay = json_number_value(json_array_get(pa, 1)),
                                by = json_number_value(json_array_get(pb, 1));
                         if (ay != by) return ay < by;
                         return json_number_value(json_array_get(pa, 0)) <
                                json_number_value(json_array_get(pb, 0));
                       });
      // Selections can contain only the wrapper, with its children embedded.
      for (auto id : beeks) {
        auto* contents = json_object_get(
            json_object_get(modules.at(id), "data"), "contents");
        auto* children = json_object_get(contents, "modules");
        bool added = false;
        for (size_t i = 0; i < json_array_size(children); ++i) {
          auto* child = json_array_get(children, i);
          auto* source = json_object_get(child, "sourceId");
          if (!json_is_integer(source) || json_integer_value(source) < 0)
            throw Exception("Invalid contained module ID");
          added |= modules.emplace(json_integer_value(source), child).second;
        }
        if (added) {
          auto* wires = json_object_get(root, "cables");
          if (!json_is_array(wires)) {
            wires = json_array();
            json_object_set_new(root, "cables", wires);
          }
          auto* inner = json_object_get(contents, "cables");
          for (size_t i = 0; i < json_array_size(inner); ++i)
            json_array_append(wires, json_array_get(inner, i));
        }
      }
      for (const auto& entry : modules)
        if (isBeekAudioInterface(entry.second) &&
            std::find(audio.begin(), audio.end(), entry.first) == audio.end())
          audio.push_back(entry.first);
    } catch (...) {
      json_decref(root);
      root = nullptr;
      throw;
    }
  }
  ~BeekPatchFile() { json_decref(root); }
  BeekPatchFile(const BeekPatchFile&) = delete;
  BeekPatchFile& operator=(const BeekPatchFile&) = delete;
};

#include "BeekapooAudioImport.hpp"
#include "BeekapooWholePatch.hpp"

inline BeekImportDialog* makeBeekImportDialog(
    const BeekPatchFile& file, const std::string& path,
    std::function<void(int64_t, bool)> apply, bool entirePatchAudio = false) {
  std::vector<BeekImportChoice> choices;
  std::string title;
  if (!file.beeks.empty() && !entirePatchAudio) {
    title = "Import Beek";
    for (size_t i = 0; i < file.beeks.size(); ++i) {
      auto id = file.beeks[i];
      auto* group = json_object_get(
          json_object_get(file.modules.at(id), "data"), "group");
      auto count = json_array_size(json_object_get(group, "members"));
      choices.emplace_back("Beek " + std::to_string(i + 1),
                           std::to_string(count) + " contained modules", id);
    }
    choices.emplace_back("Entire patch", "Combine all Beeks and other modules.",
                         beekEntirePatch);
  } else if (!file.audio.empty()) {
    title = "Import patch audio";
    choices.emplace_back("Expose",
                         "Remove the Audio interfaces and expose all connected "
                         "inputs and outputs.",
                         entirePatchAudio ? beekEntirePatch : -1, true);
    choices.emplace_back("Keep",
                         "Keep the original Audio modules and their device "
                         "connections inside the Beekapoo subpatch.",
                         entirePatchAudio ? beekEntirePatch : -1);
  } else
    return nullptr;
  return new BeekImportDialog(title, system::getFilename(path),
                              std::move(choices), std::move(apply));
}

void Beekapoo::saveBeek(const std::string& path) {
  if (members.empty()) throw Exception("There are no modules in this Beek");
  prepareContainedSave();
  saveGroup();
  BeekTemporaryDirectory temporary;
  auto* root = json_object();
  DEFER({ json_decref(root); });
  json_object_set_new(root, "version", json_string(APP_VERSION.c_str()));
  json_object_set_new(root, "zoom", json_real(1.));
  json_object_set_new(root, "gridOffset", json_pack("[i,i]", 0, 0));
  auto* list = json_array();
  json_object_set_new(root, "modules", list);
  auto* owner = toJson();
  json_array_append_new(list, owner);
  // Work on detached JSON, never mutate the running group's saved state.
  auto* data = json_deep_copy(json_object_get(owner, "data"));
  json_object_set_new(owner, "data", data);
  auto* group = json_object_get(data, "group");
  json_object_del(data, "contents");
  json_object_del(group, "editing");
  json_object_set_new(group, "expanded", json_false());
  json_object_set_new(group, "hasEditPlacement", json_false());
  json_object_set_new(owner, "pos", json_pack("[i,i]", 0, 0));
  std::set<int64_t> included{module->id};
  Vec origin(INFINITY, INFINITY);
  for (const auto& member : members) {
    included.insert(member.id);
    origin.x = std::min(origin.x, member.position.x);
    origin.y = std::min(origin.y, member.position.y);
  }
  for (const auto& member : members) {
    auto* w = find(member.id);
    if (!w) throw Exception("A contained module is missing");
    auto* node = w->toJson();
    Vec position = (member.position - origin) / RACK_GRID_SIZE + Vec(0, 1);
    json_object_set_new(
        node, "pos",
        json_pack("[f,f]", double(position.x), double(position.y)));
    json_array_append_new(list, node);
  }
  auto* savedMembers = json_object_get(group, "members");
  for (size_t i = 0; i < json_array_size(savedMembers); ++i) {
    auto* item = json_array_get(savedMembers, i);
    json_object_set_new(
        item, "x",
        json_real(json_number_value(json_object_get(item, "x")) - origin.x));
    json_object_set_new(
        item, "y",
        json_real(json_number_value(json_object_get(item, "y")) - origin.y +
                  RACK_GRID_HEIGHT));
  }
  auto* cables = json_array();
  json_object_set_new(root, "cables", cables);
  for (auto* cable : homeRack->getCompleteCables())
    if (included.count(cable->cable->inputModule->id) &&
        included.count(cable->cable->outputModule->id)) {
      auto* node = cable->cable->toJson();
      cable->mergeJson(node);
      json_array_append_new(cables, node);
    }
  for (size_t i = 0; i < json_array_size(list); ++i) {
    auto* node = json_array_get(list, i);
    for (const char* key : {"leftModuleId", "rightModuleId"})
      if (!included.count(json_integer_value(json_object_get(node, key))))
        json_object_del(node, key);
    auto* m =
        APP->engine->getModule(json_integer_value(json_object_get(node, "id")));
    auto storage = m->getPatchStorageDirectory();
    if (!system::isDirectory(storage)) continue;
    auto target =
        system::join(temporary.path, "modules", std::to_string(m->id));
    system::createDirectories(system::getDirectory(target));
    if (!system::copy(storage, target))
      throw Exception("Could not copy module assets");
  }
  if (json_dump_file(root, system::join(temporary.path, "patch.json").c_str(),
                     JSON_INDENT(2)) != 0)
    throw Exception("Could not write Beek patch");
  // Write beside the destination, so a failed archive never truncates a
  // previous good export. Rename is on the same filesystem.
  auto pending = path + "." + std::to_string(random::u64()) + ".tmp";
  DEFER({ system::remove(pending); });
  system::archiveDirectory(pending, temporary.path, 1);
  if (!system::rename(pending, path))
    throw Exception("Could not save Beek patch");
  status = "Saved Beek: " + system::getFilename(path);
}

bool Beekapoo::loadBeek(const std::string& path, int64_t sourceOwner,
                        bool convertAudio) {
  BeekPatchFile file(path);
  if (sourceOwner == beekEntirePatch && !file.beeks.empty()) {
    sourceOwner = combineBeekPatch(file, toJson(), convertAudio);
    convertAudio = false;
  }
  if (sourceOwner == -1 && file.beeks.size() == 1)
    sourceOwner = file.beeks.front();
  if (sourceOwner == -1 && file.beeks.size() > 1)
    throw Exception("Choose which Beek to load");
  if (sourceOwner >= 0 && !file.modules.count(sourceOwner))
    throw Exception("The chosen Beek is not in this file");
  auto* snapshot = sourceOwner >= 0 && file.modules.count(sourceOwner)
                       ? json_deep_copy(file.modules.at(sourceOwner))
                       : toJson();
  DEFER({ json_decref(snapshot); });
  auto* data = json_object_get(snapshot, "data");
  if (!json_is_object(data)) throw Exception("Invalid Beek settings");
  if (getModule<Module>()->hasCustomPatchName()) {
    json_object_set_new(
        data, "name", json_string(getModule<Module>()->getPatchName().c_str()));
    json_object_set_new(data, "customName", json_true());
  }
  json_t* group;
  std::set<int64_t> selected;
  if (sourceOwner >= 0) {
    if (!isBeekPatchModule(snapshot))
      throw Exception("The chosen module is not a Beekapoo");
    group = json_object_get(data, "group");
    auto* list = json_object_get(group, "members");
    if (!json_is_array(list)) throw Exception("Missing Beek membership");
    for (size_t i = 0; i < json_array_size(list); ++i) {
      auto* id = json_object_get(json_array_get(list, i), "id");
      if (!json_is_integer(id) || json_integer_value(id) < 0 ||
          !selected.insert(json_integer_value(id)).second)
        throw Exception("Invalid or duplicate Beek member");
    }
  } else {
    group = json_object();
    json_object_set_new(data, "group", group);
    json_object_set_new(group, "owner", json_integer(-1));
    for (const auto& item : file.modules) selected.insert(item.first);
    json_object_set_new(group, "members", json_array());
    json_object_set_new(group, "ports", json_array());
    json_object_set_new(group, "parameters", json_array());
    if (file.audio.empty())
      json_object_set_new(group, "exposeUnpatched", json_true());
  }
  if (convertAudio) {
    if (sourceOwner >= 0 || file.audio.empty())
      throw Exception(
          "Audio conversion requires an ordinary patch with Audio interfaces");
    exposeBeekAudioInterfaces(file, group);
    selected.clear();
    for (const auto& item : file.modules) selected.insert(item.first);
  }
  if (selected.empty()) throw Exception("No contained modules to load");
  auto* contents = json_object();
  json_object_set_new(data, "contents", contents);
  auto* nodes = json_array();
  json_object_set_new(contents, "modules", nodes);
  std::string missing;
  int index = 0;
  for (auto id : selected) {
    if (!file.modules.count(id))
      throw Exception("A Beek member is absent from this patch");
    auto* node = file.modules.at(id);
    if (isBeekPatchModule(node))
      throw Exception("Nested Beekapoos are not supported");
    if (!plugin::modelFromJson(node))
      missing += "\n" + beekJsonString(node, "plugin") + " / " +
                 beekJsonString(node, "model");
    auto* copy = json_deep_copy(node);
    json_object_set_new(copy, "sourceId", json_integer(id));
    json_array_append_new(nodes, copy);
    if (sourceOwner < 0) {
      auto* pos = json_object_get(node, "pos");
      double x =
          json_is_array(pos)
              ? json_number_value(json_array_get(pos, 0)) * RACK_GRID_WIDTH
              : index * 90.;
      double y =
          json_is_array(pos)
              ? json_number_value(json_array_get(pos, 1)) * RACK_GRID_HEIGHT
              : 0.;
      if (!std::isfinite(x) || !std::isfinite(y))
        throw Exception("Invalid module position");
      json_array_append_new(
          json_object_get(group, "members"),
          json_pack("{s:I,s:f,s:f}", "id", id, "x", x, "y", y));
      ++index;
    }
  }
  if (!missing.empty())
    throw Exception("Required modules are not installed:%s", missing.c_str());
  auto* cables = json_array();
  json_object_set_new(contents, "cables", cables);
  auto* originalCables = json_object_get(file.root, "cables");
  auto allowed = [&](int64_t id) {
    return selected.count(id) || (sourceOwner >= 0 && id == sourceOwner);
  };
  for (size_t i = 0; i < json_array_size(originalCables); ++i) {
    auto* cable = json_array_get(originalCables, i);
    if (allowed(json_integer_value(json_object_get(cable, "inputModuleId"))) &&
        allowed(json_integer_value(json_object_get(cable, "outputModuleId"))))
      json_array_append(cables, cable);
  }
  json_object_del(group, "editing");
  json_object_set_new(group, "expanded", json_false());
  // Use the native preset transaction; importContents joins its child module
  // and cable history to this one replacement action.
  prepareContainedSave();
  if (!pasteJsonAction(snapshot))
    throw Exception("Could not load Beek settings");
  auto* pending = getModule<Module>()->takeRestore();
  DEFER({ json_decref(pending); });
  if (!pending) throw Exception("Could not load Beek state");
  bool loaded = importContents(
      pending, system::join(file.temporary.path, "modules"), sourceOwner);
  if (loaded) status = "Loaded Beek: " + system::getFilename(path);
  return loaded;
}

void Beekapoo::saveBeekDialog() {
  osdialog_filters* filters =
      osdialog_filters_parse("VCV Rack patch (.vcv):vcv");
  DEFER({ osdialog_filters_free(filters); });
  char* selected = osdialog_file(OSDIALOG_SAVE, nullptr, "Beek.vcv", filters);
  if (!selected) return;
  std::string path = selected;
  std::free(selected);
  if (system::getExtension(path) != ".vcv") path += ".vcv";
  try {
    saveBeek(path);
  } catch (const std::exception& e) {
    osdialog_message(OSDIALOG_WARNING, OSDIALOG_OK, e.what());
  }
}
void Beekapoo::loadBeekDialog() {
  osdialog_filters* filters =
      osdialog_filters_parse("Rack patch or selection:vcv,vcvs");
  DEFER({ osdialog_filters_free(filters); });
  char* selected = osdialog_file(OSDIALOG_OPEN, nullptr, nullptr, filters);
  if (!selected) return;
  std::string path = selected;
  std::free(selected);
  try {
    BeekPatchFile file(path);
    WeakPtr<Beekapoo> safe = this;
    auto apply = [safe, path](int64_t id, bool audio) {
      if (!safe) return;
      if (!safe->members.empty() &&
          !osdialog_message(
              OSDIALOG_WARNING, OSDIALOG_OK_CANCEL,
              "Replace this Beek's contents? This can be undone."))
        return;
      try {
        if (!safe->loadBeek(path, id, audio))
          osdialog_message(OSDIALOG_WARNING, OSDIALOG_OK, safe->status.c_str());
      } catch (const std::exception& e) {
        osdialog_message(OSDIALOG_WARNING, OSDIALOG_OK, e.what());
      }
    };
    auto choose = [safe, path, apply](int64_t id, bool audio) {
      if (!safe) return;
      try {
        if (id == beekEntirePatch) {
          BeekPatchFile selected(path);
          if (auto* dialog =
                  makeBeekImportDialog(selected, path, apply, true)) {
            APP->scene->addChild(dialog);
            dialog->step();
            return;
          }
        }
        apply(id, audio);
      } catch (const std::exception& e) {
        osdialog_message(OSDIALOG_WARNING, OSDIALOG_OK, e.what());
      }
    };
    if (auto* dialog = makeBeekImportDialog(file, path, choose)) {
      APP->scene->addChild(dialog);
      dialog->step();
    } else
      apply(file.beeks.empty() ? -1 : file.beeks.front(), false);
  } catch (const std::exception& e) {
    osdialog_message(OSDIALOG_WARNING, OSDIALOG_OK, e.what());
  }
}
