#pragma once


struct DrawHelper {

  NVGcontext* vg;
  DrawHelper(NVGcontext* ctx) {
    vg = ctx;
  }
  void drawShape(std::vector<Vec> points, NVGcolor fillColor = COLOR_COMPUTERSCARE_PINK) {
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
    nvgFillColor(vg, fillColor);
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
  void drawDots(std::vector<Vec> points, NVGcolor fillColor = BLACK, float thickness = 1.f) {
    unsigned int n = points.size();
    //nvgSave(vg);
    // nvgBeginPath(vg);
    nvgBeginPath(vg);
    //nvgStrokeColor(vg, fillColor);
    //nvgStrokeWidth(vg, thickness);
    nvgFillColor(vg, fillColor);

    //nvgMoveTo(vg, 0,0);
    for (unsigned int i = 0; i < n; i++) {
      nvgCircle(vg, points[i].x, points[i].y, thickness);
    }

    nvgClosePath(vg);
    nvgFill(vg);
    /*if (thickness > 0) {
      nvgStroke(vg);
    }*/
    //nvgStroke(vg);
    // nvgRestore(vg);
  }
  //void drawLines(std::vector<Vec> points, float length=)
  void drawLines(std::vector<Vec> points, float length = 20, float angle = 0.f, float thickness = 5.f) {
    std::vector<Vec> angles;
    for (unsigned int i = 0; i < points.size(); i++) {
      angles.push_back(Vec(length, angle));
    }
    drawLines(points, angles, COLOR_COMPUTERSCARE_RED, thickness);
  }
  void drawLines(std::vector<Vec> points, std::vector<Vec> rTheta, std::vector<NVGcolor> strokeColors, std::vector<Vec> thicknesses) {
    unsigned int n = points.size();

    // nvgBeginPath(vg);


    //nvgMoveTo(vg, 0,0);
    for (unsigned int i = 0; i < n; i++) {
      nvgSave(vg);
      nvgBeginPath(vg);
      nvgStrokeWidth(vg, thicknesses[i].x);
      nvgStrokeColor(vg, strokeColors[i]);
      nvgMoveTo(vg, points[i].x, points[i].y);
      nvgLineTo(vg, points[i].x + rTheta[i].x * cos(rTheta[i].y), points[i].y + rTheta[i].x * sin(rTheta[i].y));
      nvgStroke(vg);
      nvgRestore(vg);
      //nvgClosePath(vg);
    }


  }
  void drawLines(std::vector<Vec> points, std::vector<Vec> rTheta, NVGcolor strokeColor = COLOR_COMPUTERSCARE_RED, float thickness = 1.f) {
    unsigned int n = points.size();
    nvgSave(vg);
    // nvgBeginPath(vg);
    nvgBeginPath(vg);
    nvgStrokeColor(vg, strokeColor);
    nvgStrokeWidth(vg, thickness);

    //nvgMoveTo(vg, 0,0);
    for (unsigned int i = 0; i < n; i++) {
      nvgMoveTo(vg, points[i].x, points[i].y);
      nvgLineTo(vg, points[i].x + rTheta[i].x * cos(rTheta[i].y), points[i].y + rTheta[i].x * sin(rTheta[i].y));
      //nvgClosePath(vg);
    }
    nvgStroke(vg);
    nvgRestore(vg);
  }
  void drawLines(int n, float dThickness, float dTheta, float dColor = 0.1) {
    nvgSave(vg);
    // nvgBeginPath(vg);

    //nvgMoveTo(vg, 0,0);
    float radius = 100000;
    float thickness = 10;
    for (int i = 0; i < n; i++) {

      NVGcolor color = sincolor(dColor * i / n);
      float angle = dTheta * i / n * 2 * M_PI;
      nvgBeginPath(vg);
      nvgStrokeColor(vg, color);
      nvgStrokeWidth(vg, thickness);

      float x0 = radius * cosf(angle);
      float y0 = radius * sinf(angle);
      float x1 = radius * cosf(angle + M_PI);
      float y1 = radius * sinf(angle + M_PI);

      nvgMoveTo(vg, x0, y0);
      nvgLineTo(vg, x1, y1);
      nvgClosePath(vg);
      nvgStroke(vg);

      thickness *= expf(dThickness);

    }
    nvgRestore(vg);
  }
  void drawField(std::vector<Vec> points, NVGcolor strokeColor = BLACK, float length = 4, float thickness = 1.f) {
    unsigned int n = points.size();
    nvgSave(vg);
    // nvgBeginPath(vg);
    nvgBeginPath(vg);
    nvgStrokeColor(vg, strokeColor);
    nvgStrokeWidth(vg, thickness);

    //nvgMoveTo(vg, 0,0);
    for (unsigned int i = 0; i < n; i++) {
      nvgMoveTo(vg, points[i].x, points[i].y);
      Vec end = points[i].plus(Vec(points[i].y, -points[i].x).normalize().mult(length));
      nvgLineTo(vg, end.x, end.y);
      //nvgClosePath(vg);
    }
    nvgStroke(vg);
    nvgRestore(vg);
  }
  NVGcolor sincolor(float t, std::vector<float> omega = {1, 2, 3}, std::vector<float> phi = {0.f, 5.f, 9.f}) {
    return nvgRGB(127 * (1 + sin(t * omega[0] + phi[0])), 127 * (1 + sin(t * omega[1] + phi[1])), 127 * (1 + sin(t * omega[2] + phi[2])));
  }

};