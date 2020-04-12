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
  void drawDots(std::vector<Vec> points,NVGcolor fillColor=BLACK,float thickness=1.f) {
    unsigned int n = points.size();
    //nvgSave(vg);
   // nvgBeginPath(vg);
    nvgBeginPath(vg);
    //nvgStrokeColor(vg, fillColor);
    //nvgStrokeWidth(vg, thickness);
    nvgFillColor(vg,fillColor);

    //nvgMoveTo(vg, 0,0);
    for (unsigned int i = 0; i < n; i++) {
      nvgCircle(vg, points[i].x, points[i].y,thickness);
    }

    nvgClosePath(vg);
    nvgFill(vg);
    /*if (thickness > 0) {
      nvgStroke(vg);
    }*/
    //nvgStroke(vg);
   // nvgRestore(vg);
  }
  void drawLines(std::vector<Vec> points, std::vector<Vec> rTheta,NVGcolor strokeColor=BLACK,float thickness=1.f) {
    unsigned int n = points.size();
    nvgSave(vg);
   // nvgBeginPath(vg);
    nvgBeginPath(vg);
    nvgStrokeColor(vg, strokeColor);
    nvgStrokeWidth(vg, thickness);

    //nvgMoveTo(vg, 0,0);
    for (unsigned int i = 0; i < n; i++) {
      nvgMoveTo(vg,points[i].x,points[i].y);
      nvgLineTo(vg,points[i].x+rTheta[i].x*cos(rTheta[i].y),points[i].y+rTheta[i].x*sin(rTheta[i].y));
      //nvgClosePath(vg);
    }
    nvgStroke(vg);
    nvgRestore(vg);
  }
  NVGcolor sincolor(float t,std::vector<int> omega={1,2,3}) {
    return nvgRGB(127*(1+sin(t*omega[0])),127*(1+sin(t*omega[1])),127*(1+sin(t*omega[2])));
  }
};
}
}