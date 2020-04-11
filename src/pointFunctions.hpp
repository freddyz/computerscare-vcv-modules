#pragma once

namespace rack {
namespace app {
struct Points {

  std::vector<Vec> points;
  Points() {
    
  }

  void grid(int numx,int numy, Vec size) {
    int N = numx*numy;
    points.resize(N);
    for(int i = 0; i < N; i++) {
      points[i] = Vec((i%numx)*size.x,(i-i%numx)/numx*size.y);
    }
  }
  void triangle(Vec lengths, Vec angles) {
    points.resize(3);
    points[0] = Vec(0,0);
    Vec b = Vec(lengths.x*cos(angles.x),lengths.x*sin(angles.x));
    Vec cMinusB = Vec(lengths.y*cos(angles.y),lengths.y*sin(angles.y));
    points[1] = b;
    points[2] = b.plus(cMinusB);
  }
  void offset(Vec dz) {
    for(unsigned int i = 0; i < points.size(); i++) {
      points[i] = points[i].plus(dz);
    }
  }
  std::vector<Vec> get() {
    return points;
  }
};
}
}