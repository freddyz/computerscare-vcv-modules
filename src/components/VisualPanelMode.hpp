#pragma once
#include <algorithm>
#include <functional>
#include <vector>

#include "../Computerscare.hpp"
#include "../Phlooper/Dropdown.hpp"
namespace computerscare {
// Move the actual jacks, retaining their identities, cables and original
// parents.
struct PortRail {
  struct Original {
    rack::app::PortWidget* port;
    rack::widget::Widget* parent;
    rack::Rect box;
    std::shared_ptr<rack::window::Svg> svg;
    float svgScale;
  };
  static constexpr int denseRows = 20;
  static constexpr float denseColumn = 15.f;
  std::vector<Original> ports;
  void capture(rack::app::ModuleWidget* owner) {
    ports.clear();
    for (auto* p : owner->getPorts()) {
      auto* svgPort = dynamic_cast<ComputerscareSvgPort*>(p);
      ports.push_back({p, p->parent, p->box,
                       svgPort ? svgPort->sw->svg : nullptr,
                       svgPort ? svgPort->svgScale : 1.f});
    }
    std::sort(
        ports.begin(), ports.end(), [](const Original& a, const Original& b) {
          return a.port->type != b.port->type ? a.port->type < b.port->type
                                              : a.port->portId < b.port->portId;
        });
  }
  void restore() {
    for (auto& p : ports) {
      if (p.port->parent != p.parent) {
        p.port->parent->removeChild(p.port);
        p.parent->addChild(p.port);
      }
      if (auto* svgPort = dynamic_cast<ComputerscareSvgPort*>(p.port)) {
        svgPort->tinyRail = false;
        if (p.svg) svgPort->setSvg(p.svg);
        svgPort->setSvgScale(p.svgScale);
      }
      p.port->box = p.box;
    }
  }
  void arrange(rack::app::ModuleWidget* owner, rack::Vec first, int rows,
               rack::Vec spacing, bool tiny = false) {
    int i = 0;
    for (auto& p : ports) {
      if (p.port->parent != owner) {
        p.port->parent->removeChild(p.port);
        owner->addChild(p.port);
      }
      if (tiny) {
        if (auto* svgPort = dynamic_cast<ComputerscareSvgPort*>(p.port)) {
          svgPort->tinyRail = true;
          svgPort->setSvg(APP->window->loadSvg(rack::asset::plugin(
              pluginInstance,
              p.port->type == rack::engine::Port::INPUT
                  ? "res/components/jack-small-pointing-down.svg"
                  : "res/components/jack-small-pointing-up.svg")));
          svgPort->setSvgScale(.9f);
          svgPort->box.size = svgPort->box.size * .9f;
        }
      }
      p.port->visible = true;
      p.port->box.pos =
          first + rack::Vec((i / rows) * spacing.x, (i % rows) * spacing.y) -
          p.port->box.size * .5f;
      ++i;
    }
  }
};
struct VisualPanelBackdrop : rack::widget::TransparentWidget {
  void draw(const DrawArgs& args) override {
    nvgBeginPath(args.vg);
    nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
    nvgFillColor(args.vg, nvgRGB(222, 222, 222));
    nvgFill(args.vg);
  }
};
struct ReturnToNormalButton : phlooper::WarpedDropdown {
  std::function<void()> action;
  ReturnToNormalButton() {
    configure(51, 17, 3, 2, .7f, 197);
    textSize = 10;
  }
  std::string label() override { return "Normal UI"; }
  bool isPressed() override { return false; }
  void onButton(const rack::event::Button& e) override {
    if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
      if (action) action();
      e.consume(this);
    }
  }
};
struct VisualPanelMode {
  struct Original {
    rack::widget::Widget* child;
    rack::Rect box;
    bool visible;
  };
  std::vector<Original> originals;
  PortRail rail;
  std::function<std::string(rack::app::PortWidget*)> labelFor;
  bool active = false;
  float normalWidth = 0;
  void set(rack::app::ModuleWidget* owner, bool enable,
           const std::vector<rack::widget::Widget*>& keep) {
    if (enable == active) return;
    if (enable) {
      normalWidth = owner->box.size.x;
      originals.clear();
      for (auto* c : owner->children)
        originals.push_back({c, c->box, c->visible});
      rail.capture(owner);
      for (auto& c : originals) c.child->visible = false;
      for (auto* c : keep)
        if (c) c->visible = true;
      int cols = std::max(1, int((rail.ports.size() + PortRail::denseRows - 1) /
                                 PortRail::denseRows));
      owner->box.size.x = normalWidth + float(cols) * PortRail::denseColumn;
      rail.arrange(owner, rack::Vec(normalWidth + 7.5f, 30),
                   PortRail::denseRows, rack::Vec(PortRail::denseColumn, 17.f),
                   true);
    } else {
      rail.restore();
      for (auto& c : originals) {
        c.child->box = c.box;
        c.child->visible = c.visible;
      }
      owner->box.size.x = normalWidth;
    }
    active = enable;
    if (auto* rack = owner->getAncestorOfType<rack::app::RackWidget>())
      rack->requestModulePos(owner, owner->box.pos);
  }
  // Dense rails use the native port tooltip instead of permanent labels.
  void drawLabels(const rack::widget::Widget::DrawArgs&) {}
};
inline void appendVisualPanelMenu(rack::ui::Menu* menu, bool compact,
                                  std::function<void(bool)> set) {
  menu->addChild(rack::createSubmenuItem(
      "Panel", compact ? "Max visual" : "Normal",
      [compact, set](rack::ui::Menu* modes) {
        for (int mode = 0; mode < 2; ++mode)
          modes->addChild(rack::createCheckMenuItem(
              mode ? "Max visual" : "Normal", "",
              [compact, mode]() { return compact == bool(mode); },
              [set, mode]() { set(mode != 0); }));
      }));
}
}  // namespace computerscare
