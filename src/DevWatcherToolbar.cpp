#include "DevWatcherToolbar.hpp"

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <functional>

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
  WatcherToolbar(const std::string& path) : control(path) {
    box.size = Vec(370, 28);
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
