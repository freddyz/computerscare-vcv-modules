#include "Computerscare.hpp"

namespace rack {
namespace app {

ComputerscareSvgPort::ComputerscareSvgPort() {
  fb = new widget::FramebufferWidget;
  addChild(fb);

  shadow = new CircularShadow;
  fb->addChild(shadow);
  // Avoid breakage if plugins fail to call setSvg()
  // In that case, just disable the shadow.
  shadow->box.size = math::Vec(0, 0);

  sw = new widget::SvgWidget;
  fb->addChild(sw);
}

void ComputerscareSvgPort::draw(const DrawArgs& args) {
  if (svgScale == 1.f) {
    PortWidget::draw(args);
    return;
  }

  math::Vec center = box.size.mult(0.5f);
  nvgSave(args.vg);
  nvgTranslate(args.vg, center.x, center.y);
  nvgScale(args.vg, svgScale, svgScale);
  nvgTranslate(args.vg, -center.x, -center.y);
  PortWidget::draw(args);
  nvgRestore(args.vg);
}

void ComputerscareSvgPort::setSvg(std::shared_ptr<Svg> svg) {
  sw->setSvg(svg);
  fb->box.size = sw->box.size;
  box.size = sw->box.size;
  shadow->box.size = math::Vec(0, 0);
  // Move shadow downward by 10%
  shadow->box.pos = math::Vec(0, sw->box.size.y * 0.10);
  // shadow->box = shadow->box.grow(math::Vec(2, 2));
  fb->dirty = true;
}

void ComputerscareSvgPort::setSvgScale(float scale) {
  svgScale = scale;
  fb->dirty = true;
}

}  // namespace app
}  // namespace rack
