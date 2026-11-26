#include "DevWatcherToolbar.hpp"

#include <osdialog.h>

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <functional>
#include <history.hpp>
#include <map>
#include <patch.hpp>
#include <stdexcept>

#ifdef ARCH_MAC
#include <dispatch/dispatch.h>
#endif

#include "Computerscare.hpp"
#include "PatchSaveNaming.hpp"

namespace {
struct WatcherButton : ui::Button {
  std::function<void()> action;
  bool enabled = true;
  int icon = 0;
  WeakPtr<ui::Tooltip> tip;
  ~WatcherButton() override { clearTip(); }
  void clearTip() {
    if (tip) tip->requestDelete();
    tip = nullptr;
  }
  void onEnter(const EnterEvent& e) override {
    ui::Button::onEnter(e);
    if (!icon) return;
    auto* t = new ui::Tooltip;
    t->text = text;
    tip = t;
    APP->scene->addChild(t);
  }
  void onLeave(const LeaveEvent& e) override {
    clearTip();
    ui::Button::onLeave(e);
  }
  void onAction(const ActionEvent&) override {
    if (enabled) action();
  }
  void draw(const DrawArgs& args) override {
    nvgSave(args.vg);
    if (!enabled) nvgGlobalAlpha(args.vg, .4f);
    std::string label = text;
    if (icon) text.clear();
    ui::Button::draw(args);
    text = label;
    if (icon) {
      auto* vg = args.vg;
      nvgTranslate(vg, box.size.x * .5f, box.size.y * .5f);
      nvgStrokeColor(vg, nvgRGB(222, 227, 225));
      nvgFillColor(vg, nvgRGB(222, 227, 225));
      nvgStrokeWidth(vg, 1.3f);
      nvgBeginPath(vg);
      if (icon == 1) {
        nvgRect(vg, -4, -5, 2.5f, 10);
        nvgRect(vg, 1.5f, -5, 2.5f, 10);
        nvgFill(vg);
      } else if (icon == 2) {
        nvgMoveTo(vg, -3, -5);
        nvgLineTo(vg, 5, 0);
        nvgLineTo(vg, -3, 5);
        nvgClosePath(vg);
        nvgFill(vg);
      } else if (icon == 3) {
        nvgMoveTo(vg, -5, -5);
        nvgLineTo(vg, 3, -5);
        nvgLineTo(vg, 5, -3);
        nvgLineTo(vg, 5, 5);
        nvgLineTo(vg, -5, 5);
        nvgClosePath(vg);
        nvgRect(vg, -2, -5, 4, 4);
        nvgRect(vg, -3, 1, 6, 4);
        nvgStroke(vg);
      } else if (icon == 4) {
        nvgMoveTo(vg, -5, 0);
        nvgLineTo(vg, 5, 0);
        nvgStroke(vg);
      } else {
        nvgRect(vg, -5, -5, 10, 10);
        nvgStroke(vg);
      }
    }
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
  WatcherButton *options = nullptr, *sizeToggle = nullptr;
  bool folderMode = true, minimal = false;
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
            json_object_set_new(settings, "minimalToolbar",
                                json_boolean(minimal));
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
    minimal = json_is_true(json_object_get(settings, "minimalToolbar"));
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
    options = new WatcherButton;
    options->text = "Options";
    options->box = Rect(Vec(565, 4), Vec(62, 20));
    options->action = [this]() { openOptions(); };
    addChild(options);
    sizeToggle = new WatcherButton;
    sizeToggle->action = [this] {
      minimal = !minimal;
      layout();
      try {
        auto* settings = json_object();
        json_object_set_new(settings, "folderMode", json_boolean(folderMode));
        json_object_set_new(settings, "minimalToolbar", json_boolean(minimal));
        writeJson(settingsPath(), settings);
      } catch (const std::exception& error) {
        showError(error.what());
      }
    };
    addChild(sizeToggle);
    layout();
  }
  void layout() {
    box.size = Vec(minimal ? 94 : 663, 28);
    pause->box = Rect(Vec(minimal ? 22 : 205, 4), Vec(minimal ? 20 : 75, 20));
    saveNext->box =
        Rect(Vec(minimal ? 46 : 368, 4), Vec(minimal ? 20 : 95, 20));
    sizeToggle->box = Rect(Vec(minimal ? 70 : 635, 4), Vec(20, 20));
    sizeToggle->icon = minimal ? 5 : 4;
    sizeToggle->text = minimal ? "Expand toolbar" : "Minimize toolbar";
    sizeToggle->clearTip();
    for (auto* b : {rebuild, saveBranch, options}) {
      b->visible = !minimal;
      b->clearTip();
    }
    pause->icon =
        minimal ? (state == "paused" || state == "queued" ? 2 : 1) : 0;
    saveNext->icon = minimal ? 3 : 0;
    if (parent)
      box.pos.x = std::max(4.f, parent->box.size.x - box.size.x - 10.f);
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
    if (parent)
      box.pos =
          Vec(std::max(4.f, parent->box.size.x - box.size.x - 10.f), 34.f);
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
      pause->icon =
          minimal ? (state == "paused" || state == "queued" ? 2 : 1) : 0;
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
    if (!minimal) nvgText(args.vg, 22, 14, caption.c_str(), nullptr);
    widget::OpaqueWidget::draw(args);
  }
};
std::map<app::Scene*, WeakPtr<WatcherToolbar>> toolbars;
#ifdef ARCH_MAC
bool startupRetryQueued = false;
void retryAfterUiStartup() {
  if (startupRetryQueued) return;
  startupRetryQueued = true;
  // GLFW services the main queue while polling Cocoa events. This runs on
  // Rack's UI thread, after plugin init, even with no computerscare widgets.
  dispatch_after_f(dispatch_time(DISPATCH_TIME_NOW, 10 * NSEC_PER_MSEC),
                   dispatch_get_main_queue(), nullptr, [](void*) {
                     startupRetryQueued = false;
                     installDevWatcherToolbar();
                   });
}
#endif
}  // namespace
void installDevWatcherToolbar() {
  // Plugin init runs before Rack creates its UI. Never cache an initially
  // absent environment or a detached widget forever.
  const char* value = std::getenv("VCVMON_CONTROL");
  static bool loggedMissing = false, loggedDeferred = false;
  if (!value || !*value) {
    if (settings::devMode && !loggedMissing) {
      INFO("vcvmon toolbar inactive: VCVMON_CONTROL is not set");
      loggedMissing = true;
    }
    return;
  }
  loggedMissing = false;
  if (!APP || !APP->scene || !APP->window || !APP->event) {
#ifdef ARCH_MAC
    retryAfterUiStartup();
#endif
    if (!loggedDeferred) {
      INFO("vcvmon toolbar waiting for Rack UI (control: %s)", value);
      loggedDeferred = true;
    }
    return;
  }
  auto* scene = APP->scene;
  auto* existing = toolbars[scene].get();
  if (existing && !existing->requestedDelete &&
      (!existing->parent || existing->parent == scene)) {
    if (!existing->parent) {
      scene->addChild(existing);
      INFO("vcvmon toolbar reattached to Rack UI");
    }
    existing->show();
    if (existing->control != value) {
      existing->control = value;
      existing->nextPoll = 0;
      INFO("vcvmon toolbar control updated: %s", value);
    }
    return;
  }
  for (auto it = toolbars.begin(); it != toolbars.end();)
    if (!it->second)
      it = toolbars.erase(it);
    else
      ++it;
  std::string control(value);
  auto* widget = new WatcherToolbar(control);
  toolbars[scene] = widget;
  scene->addChild(widget);
  INFO("vcvmon toolbar attached to Rack UI (control: %s)", value);
}
