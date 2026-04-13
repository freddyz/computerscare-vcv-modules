#pragma once
#include "rack.hpp"
using namespace rack;

// ── Helpers ──────────────────────────────────────────────────────────────────

// Collect every PortWidget in a module's widget tree (inputs + outputs).
static void collectPorts(widget::Widget* w,
                         std::vector<app::PortWidget*>& out) {
  app::PortWidget* pw = dynamic_cast<app::PortWidget*>(w);
  if (pw) out.push_back(pw);
  for (widget::Widget* child : w->children)
    collectPorts(child, out);
}

// Slump midpoint matching VCV Rack's own cable catenary formula.
static math::Vec cableSlump(math::Vec p1, math::Vec p2) {
  float dist = p1.minus(p2).norm();
  math::Vec avg = p1.plus(p2).div(2.f);
  avg.y += (1.f - settings::cableTension) * (150.f + dist);
  return avg;
}

// ── Cable glow ────────────────────────────────────────────────────────────────
// Draws a fat glowing halo along each complete cable's quadratic-bezier path.
inline void drawWireGlowEffect(NVGcontext* vg, float intensity) {
  if (intensity <= 0.f) return;
  auto cables = APP->scene->rack->getCompleteCables();

  nvgSave(vg);
  nvgLineCap(vg, NVG_ROUND);
  nvgLineJoin(vg, NVG_ROUND);

  for (app::CableWidget* cw : cables) {
    math::Vec op = cw->getOutputPos();
    math::Vec ip = cw->getInputPos();
    math::Vec slump = cableSlump(op, ip);

    // Outer glow (wide, low alpha)
    NVGcolor glow = cw->color;
    glow.a = intensity * 0.35f;
    nvgBeginPath(vg);
    nvgMoveTo(vg, op.x, op.y);
    nvgQuadTo(vg, slump.x, slump.y, ip.x, ip.y);
    nvgStrokeColor(vg, glow);
    nvgStrokeWidth(vg, 18.f * intensity);
    nvgStroke(vg);

    // Inner bright core
    NVGcolor core = cw->color;
    core.a = intensity * 0.75f;
    nvgBeginPath(vg);
    nvgMoveTo(vg, op.x, op.y);
    nvgQuadTo(vg, slump.x, slump.y, ip.x, ip.y);
    nvgStrokeColor(vg, core);
    nvgStrokeWidth(vg, 4.f);
    nvgStroke(vg);
  }
  nvgRestore(vg);
}

// ── Port halo ─────────────────────────────────────────────────────────────────
// Draws animated halo rings around every port on every module.
inline void drawPortHaloEffect(NVGcontext* vg,
                               const std::vector<app::ModuleWidget*>& modules,
                               float intensity) {
  if (intensity <= 0.f) return;
  double t = APP->window->getFrameTime();

  nvgSave(vg);
  for (app::ModuleWidget* mw : modules) {
    std::vector<app::PortWidget*> ports;
    collectPorts(mw, ports);

    for (app::PortWidget* pw : ports) {
      // Port center in moduleContainer space
      math::Vec c = mw->box.pos.plus(pw->box.pos).plus(pw->box.size.div(2.f));

      // Two concentric rings pulsing out of phase with each other
      for (int ring = 0; ring < 2; ring++) {
        float phase = (float)t * 2.2f + ring * (float)M_PI;
        float pulse = 0.5f + 0.5f * std::sin(phase);
        float r = pw->box.size.x * (0.65f + pulse * 0.55f);
        float a = intensity * (0.45f - pulse * 0.35f);

        NVGpaint paint =
            nvgRadialGradient(vg, c.x, c.y, r * 0.3f, r,
                              nvgRGBAf(0.3f, 1.f, 0.8f, a),
                              nvgRGBAf(0.f, 0.f, 0.f, 0.f));
        nvgBeginPath(vg);
        nvgCircle(vg, c.x, c.y, r);
        nvgFillPaint(vg, paint);
        nvgFill(vg);
      }
    }
  }
  nvgRestore(vg);
}
