#include "ComputerscareContrast.hpp"
#include <cassert>
#include <iostream>
using namespace computerscare::contrast;
int main() {
  assert(std::abs(ratio(linear(0),linear(1))-21)<.0001f);
  assert(std::abs(linear(.5f)-.214041f)<.00001f);
  assert(std::abs(luminance(0,1,0)-.7152f)<.00001f);
  bool usedDark=false,usedLight=false,usedMiddle=false;
  for(int face=0;face<256;++face)for(int panel=0;panel<256;++panel) {
    float a=linear(face/255.f),b=linear(panel/255.f);
    float edge=border(a,b);
    assert(edge>=0&&edge<=1);
    assert(ratio(linear(textInk(a)),a)>=4.5f);
    usedDark|=edge<.1f;usedLight|=edge>.9f;usedMiddle|=edge>.1f&&edge<.9f;
    // A coarse independent scan must not find a stronger uniform outline.
    float score=std::min(ratio(linear(edge),a),ratio(linear(edge),b));
    for(float candidate:{0.f,.125f,.25f,.375f,.5f,.625f,.75f,.875f,1.f})
      assert(score+.0001f>=std::min(ratio(linear(candidate),a),ratio(linear(candidate),b)));
    assert(std::abs(border(b,a)-edge)<.0001f);
  }
  assert(usedDark&&usedLight&&usedMiddle);
  for(float r:{0.f,.4f,1.f})for(float g:{0.f,.6f,1.f})for(float b:{0.f,.3f,1.f}) {
    float face=luminance(r,g,b),panel=luminance(b,r,g);
    std::array<float,4> surfaces{{face,panel,face*.6f,face*.9f}};
    float edge=linear(border(surfaces));
    auto weakest=[&](float ink){float score=21;for(float surface:surfaces)score=std::min(score,ratio(ink,surface));return score;};
    for(int sample=0;sample<=256;++sample)
      assert(weakest(edge)+.0001f>=weakest(linear(sample/256.f)));
    std::reverse(surfaces.begin(),surfaces.end());
    assert(std::abs(linear(border(surfaces))-edge)<.0001f);
  }
  std::cout<<"Control contrast: 65536 grayscale pairs, uniform borders across colored facets and text passed\n";
}
