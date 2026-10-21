#pragma once
#include "../Phlooper/Block.hpp"
#include "../Phlooper/Dropdown.hpp"

namespace computerscare {
// A reusable two-action block. The owner supplies the parameter group and
// action callbacks; input IDs stay with the owning module.
struct RandomBlock : widget::Widget {
  struct Button : phlooper::WarpedDropdown {
    std::function<void()> action;
    widget::SvgWidget icon;
    std::string caption;
    WeakPtr<ui::Tooltip> tooltip;
    bool pressed = false;
    Button(bool wiggle, std::string group) {
      configure(14, 14, 2, 1, .7f, wiggle ? 212 : 211);
      faceColor = nvgRGB(205, 205, 205);
      caption = (wiggle ? "Wiggle " : "Randomize ") + group;
      icon.setSvg(APP->window->loadSvg(asset::plugin(
          pluginInstance, wiggle ? "res/components/rndblk-wiggle.svg"
                                 : "res/components/rndblk-dice.svg")));
    }
    ~Button() override {
      if (auto* t = tooltip.get()) t->requestDelete();
    }
    std::string label() override { return ""; }
    bool isPressed() override { return pressed; }
    void draw(const DrawArgs& args) override {
      phlooper::WarpedDropdown::draw(args);
      nvgSave(args.vg);
      nvgTranslate(args.vg, 1.5f + (pressed ? 3.f : 0.f),
                   1.5f + (pressed ? 1.9f : 0.f));
      icon.draw(args);
      nvgRestore(args.vg);
    }
    void onButton(const event::Button& e) override {
      if (e.button == GLFW_MOUSE_BUTTON_LEFT && e.action == GLFW_PRESS) {
        pressed = true;
        if (action) action();
        e.consume(this);
      }
    }
    void onDragEnd(const event::DragEnd&) override { pressed = false; }
    void onLeave(const event::Leave&) override {
      pressed = false;
      if (auto* t = tooltip.get()) t->requestDelete();
    }
    void onEnter(const event::Enter&) override {
      auto* t = new ui::Tooltip;
      t->text = caption;
      tooltip = t;
      APP->scene->addChild(t);
    }
  };
  RandomBlock(engine::Module* module, int randomInput, int wiggleInput,
              std::string group, std::function<void(bool)> action) {
    box.size = Vec(68, 20);
    for (int i = 0; i < 2; ++i) {
      auto* button = new Button(i == 1, group);
      button->box.pos = Vec(3.f + i * 32.f, 3.f);
      button->action = [=]() {
        if (module) action(i == 1);
      };
      addChild(button);
      addChild(createInputCentered<JackSmallPointingDown>(
          Vec(25.f + i * 32.f, 10.5f), module, i ? wiggleInput : randomInput));
    }
  }
  void draw(const DrawArgs& args) override {
    phlooper::WarpedBlock plaque;
    plaque.configure(Rect(Vec(), box.size), 2.f, 1.f, 210,
                     nvgRGB(200, 200, 200));
    plaque.draw(args);
    widget::Widget::draw(args);
  }
};
}  // namespace computerscare
