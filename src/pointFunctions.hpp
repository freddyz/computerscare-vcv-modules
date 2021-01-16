#pragma once

namespace rack {
namespace app {
struct Points {

  std::vector<Vec> points;
  Points() {

  }
  void linear(int num, Vec offset, Vec slope) {
    points.resize(num);
    for (int i = 0; i < num; i++) {
      points[i] = Vec(offset.x + slope.x * i / num, offset.y + slope.y * i / num);
    }
  }
  void grid(int numx, int numy, Vec size) {
    int N = numx * numy;
    points.resize(N);
    for (int i = 0; i < N; i++) {
      points[i] = Vec((i % numx) * size.x, (i - i % numx) / numx * size.y);
    }
  }
  void triangle(Vec lengths, Vec angles) {
    points.resize(3);
    points[0] = Vec(0, 0);
    Vec b = Vec(lengths.x * cos(angles.x), lengths.x * sin(angles.x));
    Vec cMinusB = Vec(lengths.y * cos(angles.y), lengths.y * sin(angles.y));
    points[1] = b;
    points[2] = b.plus(cMinusB);
  }
  void spray(int n) {
    points.resize(n);
    for (int i = 0; i < n; i++) {
      points[i] = Vec(random::normal(), random::normal());
    }
  }
  void offset(Vec dz) {
    for (unsigned int i = 0; i < points.size(); i++) {
      points[i] = points[i].plus(dz);
    }
  }
  void scale(Vec s) {
    for (unsigned int i = 0; i < points.size(); i++) {
      points[i] = points[i].mult(s);
    }
  }
  void waveBlob() {

  }
  void wtf(float buf[16][512]) {
    int n = buf[0][0] * (1 + buf[1][0]);
    for (int i = 0; i < n; i++) {
      points.push_back(Vec(i, i));
    }
  }
  std::vector<Vec> get() {
    return points;
  }
};
}
}