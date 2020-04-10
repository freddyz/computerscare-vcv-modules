#pragma once

using namespace rack;

struct DrawHelper {

  DrawArgs args;
  DrawHelper(DrawArgs &someArgs) {
    args = someArgs;
  }
  void drawOutline(std::vector<Vec> points,NVGColor color,float thickness) {
    unsigned int n = points.size();
    nvgSave(args.vg);
    nvgBeginPath(args.vg);
    nvgStrokeStyle(args.vg,color);
    nvgStrokeWidth(args.vg,thickness);
    nvgMoveTo(args.vg,points[0].x,points[0].y);

    for(int i = 1; i < n; i++) {
      nvgLineTo(args.vg,points[i].x,points[i].y);
    }

    nvgClosePath(args.vg);
    nvgStroke(args.vg);
    nvgRestore(args.vg);
  }
};
