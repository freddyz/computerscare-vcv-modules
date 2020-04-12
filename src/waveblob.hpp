#pragma once

namespace rack {
namespace app {
struct Waveblob {

  std::vector<Vec> trigs;
  Points points;
  int numPoints;
  Waveblob(int n=200) {
    numPoints=n;
    makeTrigs();
  }
  void makeTrigs() {
    trigs.resize(numPoints);
    float omega=2*M_PI/numPoints;
    for(int i = 0; i < numPoints) {
      numPoints.push_back(Vec(cos(omega*i),sin(omega*i)));
    }
  }
  Points eval(float time) {
    //points.
  }

 
};
}
}