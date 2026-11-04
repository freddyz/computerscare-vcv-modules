#include "DevWatcherToolbar.hpp"

#include <osdialog.h>

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <functional>
#include <history.hpp>
#include <patch.hpp>
#include <stdexcept>

#include "Computerscare.hpp"
#include "PatchSaveNaming.hpp"

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
  WatcherButton* saveNext = nullptr;
  WatcherButton* saveBranch = nullptr;
  bool folderMode = true;
  static const char* folderMarker() {
    return ".computerscare-patch-folder.json";
  }
  std::string settingsPath() {
    return asset::user("computerscare-patch-saves.json");
  }
  void writeJson(const std::string& path, json_t* value) {
    std::string temporary = path + ".tmp";
    int result = json_dump_file(value, temporary.c_str(), JSON_INDENT(2));
    json_decref(value);
    if (result != 0 || !system::rename(temporary, path)) {
      system::remove(temporary);
      throw std::runtime_error("Could not write " + path);
    }
  }
  bool managedFolder(const std::string& directory) {
    std::string path = system::join(directory, folderMarker());
    if (!system::exists(path)) return false;
    json_error_t error;
    json_t* root = json_load_file(path.c_str(), 0, &error);
    bool valid = root && json_integer_value(json_object_get(
                             root, "computerscarePatchFolder")) == 1;
    if (root) json_decref(root);
    if (!valid)
      throw std::runtime_error("Unrecognized patch-folder marker: " + path);
    return true;
  }
  std::string saveDirectory(const std::string& source, bool create) {
    std::string directory = system::getDirectory(source);
    if (!folderMode) return directory;
    if (managedFolder(directory)) return directory;
    std::string stem = system::getStem(source);
    // Also reuse a manually created patchxyz/patchxyz.vcv folder.
    if (system::getFilename(directory) != stem)
      directory = system::join(directory, stem);
    if (create) {
      if (!system::isDirectory(directory) &&
          !system::createDirectories(directory))
        throw std::runtime_error("Could not create patch folder: " + directory);
      if (!managedFolder(directory)) {
        json_t* marker = json_object();
        json_object_set_new(marker, "computerscarePatchFolder",
                            json_integer(1));
        json_object_set_new(marker, "initialFile",
                            json_string(system::getFilename(source).c_str()));
        writeJson(system::join(directory, folderMarker()), marker);
      }
    }
    return directory;
  }
  void showError(const std::string& message) {
    WARN("Patch save: %s", message.c_str());
    osdialog_message(OSDIALOG_WARNING, OSDIALOG_OK, message.c_str());
  }
  void openOptions() {
    auto* menu = createMenu();
    menu->addChild(createMenuLabel("Patch saves"));
    menu->addChild(createCheckMenuItem(
        "Save in patch folder", "", [this]() { return folderMode; },
        [this]() {
          try {
            json_t* settings = json_object();
            json_object_set_new(settings, "folderMode",
                                json_boolean(!folderMode));
            writeJson(settingsPath(), settings);
            folderMode = !folderMode;
          } catch (const std::exception& error) {
            showError(error.what());
          }
        }));
    if (!APP->patch->path.empty()) {
      try {
        std::string directory = saveDirectory(APP->patch->path, false);
        menu->addChild(
            createMenuLabel("Folder: " + system::getFilename(directory)));
        menu->addChild(createMenuItem("Copy folder path", "", [directory]() {
          glfwSetClipboardString(APP->window->win, directory.c_str());
        }));
      } catch (const std::exception& error) {
        menu->addChild(createMenuLabel(error.what()));
      }
    }
    menu->addChild(new MenuSeparator);
    menu->addChild(createMenuLabel("Save version: increment trailing number"));
    menu->addChild(createMenuLabel("Save branch: sibling or new word"));
  }
  void savePatch(bool branch) {
    if (APP->patch->path.empty()) APP->patch->saveDialog();
    const std::string source = APP->patch->path;
    if (source.empty()) return;
    auto* button = branch ? saveBranch : saveNext;
    int previousSavedIndex = APP->history->savedIndex;
    try {
      std::string directory = saveDirectory(source, true);
      std::string stem = system::getStem(source), target;
      // One directory scan supplies both naming strategies.
      std::vector<std::string> existing;
      for (const auto& path : system::getEntries(directory))
        existing.push_back(system::getFilename(path));
      if (branch) {
        computerscare::patch_saves::BranchCandidates candidates(
            stem, existing,
            random::u32() %
                unsigned(computerscare::patch_saves::words().size()));
        do {
          target = system::join(directory, candidates.next());
        } while (system::exists(target));
      } else {
        target = system::join(
            directory,
            computerscare::patch_saves::nextFilename(stem, existing));
        // Also respect case-insensitive filesystem collisions and directories.
        while (system::exists(target))
          target =
              system::join(directory, computerscare::patch_saves::nextFilename(
                                          system::getStem(target), existing));
      }
      // Match Rack Save As: serialize a clean history state; restore on
      // failure.
      APP->history->setSaved();
      APP->patch->save(target);
      APP->patch->path = target;
      APP->patch->pushRecentPath(target);
      button->text = branch ? "Save branch" : "Save version";
    } catch (const std::exception& error) {
      APP->history->savedIndex = previousSavedIndex;
      button->text = "Save failed";
      showError(error.what());
    }
  }
  WatcherToolbar(const std::string& path) : control(path) {
    json_error_t error;
    json_t* settings = json_load_file(settingsPath().c_str(), 0, &error);
    json_t* mode = settings ? json_object_get(settings, "folderMode") : nullptr;
    if (json_is_boolean(mode)) folderMode = json_boolean_value(mode);
    if (settings) json_decref(settings);
    box.size = Vec(635, 28);
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
    saveNext = new WatcherButton;
    saveNext->text = "Save version";
    saveNext->box = Rect(Vec(368, 4), Vec(95, 20));
    saveNext->action = [this]() { savePatch(false); };
    addChild(saveNext);
    saveBranch = new WatcherButton;
    saveBranch->text = "Save branch";
    saveBranch->box = Rect(Vec(468, 4), Vec(92, 20));
    saveBranch->action = [this]() { savePatch(true); };
    addChild(saveBranch);
    auto* options = new WatcherButton;
    options->text = "Options";
    options->box = Rect(Vec(565, 4), Vec(62, 20));
    options->action = [this]() { openOptions(); };
    addChild(options);
  }
  void onButton(const ButtonEvent& e) override {
    if (e.button == GLFW_MOUSE_BUTTON_RIGHT && e.action == GLFW_PRESS) {
      openOptions();
      e.consume(this);
      return;
    }
    widget::OpaqueWidget::onButton(e);
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
