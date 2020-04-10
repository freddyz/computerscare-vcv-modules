#pragma once

using namespace rack;

struct DrawHelper {

  DrawArgs args;
  DrawHelper(DrawArgs &someArgs) {
    args = someArgs;
  }
  void drawShape(std::vector<Vec> points, NVGColor fillColor) {
               drawShape(points,fillColor,COLOR_COMPUTERSCARE_TRANSPARENT,0.f);

  }
  void drawShape(std::vector<Vec> points,NVGColor fillColor,NVGColor strokeColor) {
           drawShape(points,fillColor,strokeColor,1.f);

  }
    
  
   void drawShape(std::vector<Vec> points,NVGColor strokeColor,float thickness) {
     drawShape(points,COLOR_COMPUTERSCARE_TRANSPARENT,strokeColor,thickness);
   }
  void drawShape(std::vector<Vec> points,NVGColor fillColor,NVGColor strokeColor,float thickness) {
    unsigned int n = points.size();
    nvgSave(args.vg);
    nvgBeginPath(args.vg);
    nvgStrokeStyle(args.vg,strokeColor);
    nvgStrokeWidth(args.vg,thickness);
    nvgFillStyle(fillColor);
    nvgMoveTo(args.vg,points[0].x,points[0].y);

    for(int i = 1; i < n; i++) {
      nvgLineTo(args.vg,points[i].x,points[i].y);
    }

    nvgClosePath(args.vg);
    nvgFill(args.vg);
    if(thickness > 0) {
      nvgStroke(args.vg);
    }
    nvgRestore(args.vg);
  }
};
