#pragma once

namespace rack {
namespace app {
struct DrawHelper {

  NVGcontext* vg;
  DrawHelper(NVGcontext* ctx) {
    vg = ctx;
  }
  void drawShape(std::vector<Vec> points, NVGcolor fillColor=COLOR_COMPUTERSCARE_PINK) {
    drawShape(points, fillColor, COLOR_COMPUTERSCARE_TRANSPARENT, 0.f);
  }
  void drawShape(std::vector<Vec> points, NVGcolor fillColor, NVGcolor strokeColor) {
    drawShape(points, fillColor, strokeColor, 1.f);
  }
  void drawShape(std::vector<Vec> points, NVGcolor strokeColor, float thickness) {
    drawShape(points, COLOR_COMPUTERSCARE_TRANSPARENT, strokeColor, thickness);
  }
  void drawShape(std::vector<Vec> points, NVGcolor fillColor, NVGcolor strokeColor, float thickness) {
    unsigned int n = points.size();
    nvgSave(vg);
    nvgBeginPath(vg);
    nvgStrokeColor(vg, strokeColor);
    nvgStrokeWidth(vg, thickness);
    nvgFillColor(vg,fillColor);
    nvgMoveTo(vg, points[0].x, points[0].y);

    for (unsigned int i = 1; i < n; i++) {
      nvgLineTo(vg, points[i].x, points[i].y);
    }

    nvgClosePath(vg);
    nvgFill(vg);
    if (thickness > 0) {
      nvgStroke(vg);
    }
    nvgRestore(vg);
  }
};
}
}