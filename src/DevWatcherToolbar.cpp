#include "DevWatcherToolbar.hpp"

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <functional>
#include <iomanip>
#include <map>
#include <patch.hpp>
#include <sstream>

#include "Computerscare.hpp"

namespace {
struct WatcherButton : ui::Button {
  std::function<void()> action;
  bool enabled = true;
  void onAction(const ActionEvent&) override {
    if (enabled) action();
  }
  void draw(const DrawArgs& args) override {
    nvgSave(args.vg);
    if (!enabled) nvgGlobalAlpha(args.vg, .4f);
    ui::Button::draw(args);
    nvgRestore(args.vg);
  }
};
struct WatcherToolbar : widget::OpaqueWidget {
  std::string control, state = "offline";
  double nextPoll = 0.;
  WatcherButton* pause = nullptr;
  WatcherButton* rebuild = nullptr;
  WatcherButton* saveCopy = nullptr;
  WatcherButton* saveVariation = nullptr;
  std::map<std::string, std::string> copyParents, variationParents;
  std::string copyParentStore() {
    return asset::user("computerscare-copy-parents.json");
  }
  void saveVersion(bool variation) {
    if (APP->patch->path.empty()) APP->patch->saveDialog();
    const std::string source = APP->patch->path;
    if (source.empty()) return;
    std::string parent =
        copyParents.count(source) ? copyParents[source] : source;
    if (!copyParents.count(source)) {
      std::string stem = system::getStem(source);
      while (!stem.empty() && stem.back() >= '0' && stem.back() <= '9')
        stem.pop_back();
      if (!stem.empty())
        parent = system::join(system::getDirectory(source), stem + ".vcv");
    }
    const std::string branch =
        variationParents.count(source) ? variationParents[source] : source;
    std::string target;
    static const char* words[] = {
        "pig",  "shrew", "fox",  "otter", "moth",  "hare", "wren", "badger",
        "mole", "finch", "lynx", "newt",  "stoat", "owl",  "yak",  "toad"};
    unsigned first = 1;
    if (!variation) {
      std::string rootStem = system::getStem(parent),
                  branchStem = system::getStem(branch);
      if (branchStem.compare(0, rootStem.size(), rootStem) == 0) {
        const char* digits = branchStem.c_str() + rootStem.size();
        unsigned long current = std::strtoul(digits, nullptr, 10);
        if (current < 1000000) first = unsigned(current) + 1;
      }
    }
    for (unsigned number = first;; ++number) {
      std::string suffix;
      if (variation) {
        unsigned index = number - 1;
        suffix = std::string("-") + words[index % 16];
        if (index >= 16) suffix += std::to_string(index / 16 + 1);
      } else
        suffix = std::to_string(number);
      const auto& base = variation ? branch : parent;
      target = system::join(system::getDirectory(base),
                            system::getStem(base) + suffix + ".vcv");
      if (!system::exists(target)) break;
    }
    auto* button = variation ? saveVariation : saveCopy;
    try {
      APP->patch->save(target);
      copyParents[source] = parent;
      copyParents[target] = parent;
      if (variation) variationParents[target] = branch;
      json_t* root = json_object();
      auto store = [&](const char* name,
                       const std::map<std::string, std::string>& entries) {
        json_t* values = json_object();
        for (const auto& entry : entries)
          json_object_set_new(values, entry.first.c_str(),
                              json_string(entry.second.c_str()));
        json_object_set_new(root, name, values);
      };
      store("parents", copyParents);
      store("variations", variationParents);
      if (json_dump_file(root, copyParentStore().c_str(), JSON_INDENT(2)) != 0)
        WARN("Could not store patch parent names");
      json_decref(root);
      APP->patch->path = target;
      APP->patch->pushRecentPath(target);
      APP->history->setSaved();
      button->text = variation ? "Save variation" : "Save version";
    } catch (const std::exception& error) {
      WARN("Save patch version failed: %s", error.what());
      button->text = "Save failed";
    }
  }
  WatcherToolbar(const std::string& path) : control(path) {
    json_error_t error;
    json_t* parents = json_load_file(copyParentStore().c_str(), 0, &error);
    auto load = [&](json_t* values,
                    std::map<std::string, std::string>& entries) {
      if (!json_is_object(values)) return;
      const char* key;
      json_t* value;
      json_object_foreach(values, key, value) if (json_is_string(value))
          entries[key] = json_string_value(value);
    };
    load(json_object_get(parents, "parents")
             ? json_object_get(parents, "parents")
             : parents,
         copyParents);
    load(json_object_get(parents, "variations"), variationParents);
    if (parents) json_decref(parents);
    box.size = Vec(580, 28);
    pause = new WatcherButton;
    pause->box = Rect(Vec(205, 4), Vec(75, 20));
    pause->action = [this]() {
      command(state == "paused" || state == "queued" ? "resume" : "pause");
    };
    addChild(pause);
    rebuild = new WatcherButton;
    rebuild->text = "Rebuild";
    rebuild->box = Rect(Vec(285, 4), Vec(78, 20));
    rebuild->action = [this]() { command("rebuild"); };
    addChild(rebuild);
    saveCopy = new WatcherButton;
    saveCopy->text = "Save version";
    saveCopy->box = Rect(Vec(368, 4), Vec(95, 20));
    saveCopy->action = [this]() { saveVersion(false); };
    addChild(saveCopy);
    saveVariation = new WatcherButton;
    saveVariation->text = "Save variation";
    saveVariation->box = Rect(Vec(468, 4), Vec(105, 20));
    saveVariation->action = [this]() { saveVersion(true); };
    addChild(saveVariation);
  }
  void command(const std::string& value) {
    if (state == "offline" || state == "stopped" || state == "building") return;
    std::string temporary = control + ".command.tmp";
    {
      std::ofstream file(temporary);
      file << value << '\n';
      if (!file) return;
    }
    std::rename(temporary.c_str(), (control + ".command").c_str());
  }
  void step() override {
    box.pos = Vec(std::max(4.f, parent->box.size.x - box.size.x - 10.f), 34.f);
    double now = system::getTime();
    if (now >= nextPoll) {
      nextPoll = now + .25;
      std::ifstream file(control + ".status");
      long long heartbeat = 0;
      std::string current;
      file >> current >> heartbeat;
      auto age = std::time(nullptr) - heartbeat;
      state = file && age >= 0 && age < (current == "building" ? 600 : 5)
                  ? current
                  : "offline";
      pause->text = state == "paused" || state == "queued" ? "Resume" : "Pause";
      pause->enabled = rebuild->enabled =
          state != "offline" && state != "stopped" && state != "building";
    }
    widget::OpaqueWidget::step();
  }
  void draw(const DrawArgs& args) override {
    nvgBeginPath(args.vg);
    nvgRoundedRect(args.vg, 0, 0, box.size.x, box.size.y, 3);
    nvgFillColor(args.vg, nvgRGBA(25, 29, 31, 245));
    nvgFill(args.vg);
    const char* label = state == "watching"   ? "Watching"
                        : state == "paused"   ? "Paused"
                        : state == "queued"   ? "Changes queued"
                        : state == "building" ? "Building"
                        : state == "failed"   ? "Build failed"
                        : state == "stopped"  ? "Stopped"
                                              : "Offline";
    nvgBeginPath(args.vg);
    nvgCircle(args.vg, 12, 14, 3);
    nvgFillColor(args.vg, state == "watching" ? nvgRGB(36, 201, 166)
                          : state == "failed" ? nvgRGB(245, 93, 86)
                                              : nvgRGB(238, 202, 105));
    nvgFill(args.vg);
    auto font = APP->window->uiFont;
    nvgFontFaceId(args.vg, font->handle);
    nvgFontSize(args.vg, 12);
    nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgFillColor(args.vg, nvgRGB(230, 233, 232));
    std::string caption = std::string("vcvmon  ") + label;
    nvgText(args.vg, 22, 14, caption.c_str(), nullptr);
    widget::OpaqueWidget::draw(args);
  }
};
WeakPtr<WatcherToolbar> toolbar;
}  // namespace
void installDevWatcherToolbar() {
  static const std::string control = []() {
    const char* value = std::getenv("VCVMON_CONTROL");
    return value ? std::string(value) : std::string();
  }();
  if (control.empty() || toolbar.get() || !APP || !APP->scene || !APP->window ||
      !APP->event)
    return;
  auto* widget = new WatcherToolbar(control);
  toolbar = widget;
  APP->scene->addChild(widget);
}
