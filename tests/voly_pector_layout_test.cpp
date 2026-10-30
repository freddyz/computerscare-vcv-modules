#include "VolyPectorLayout.hpp"
#include <cstdio>
#include <cstdlib>
using namespace computerscare::voly::layout;
void require(bool ok,const char* why){if(!ok){std::fprintf(stderr,"%s\n",why);std::abort();}}
bool near(float a,float b){return std::abs(a-b)<.002f;}
bool overlap(Rect a,Rect b){return a.x<b.x+b.width-.001f&&a.x+a.width>b.x+.001f&&a.y<b.y+b.height-.001f&&a.y+a.height>b.y+.001f;}
int main(){
  for(int divisions=1;divisions<=5;++divisions)for(int count=1;count<=16;++count){
    auto result=calculate(divisions,count);require(result.valid,"Every audited count has a layout");
    if(divisions<=4)require(result.diameter>=7,"Supported views retain usable knob size");
    for(int group=0;group<divisions;++group){const auto& d=result.groups[group];
      require(near(d.bounds.y,group*264.f/divisions)&&near(d.bounds.height,264.f/divisions),"Divisions have equal height");
      for(int i=0;i<count;++i){auto a=d.values[i];
        require(a.x>=0&&a.y>=d.bounds.y&&a.x+a.width<=54.001f&&a.y+a.height<=d.bounds.y+d.bounds.height+.001f,"Knob fits its division");
        require(a.y-labelHeightRatio*result.diameter>=d.bounds.y+(result.controls?controlsTop:titleTop)-.001f,"Native label and slant clear the header/controls");
        for(int j=i+1;j<count;++j)require(!overlap(a,d.values[j]),"Value knobs do not overlap");
        if(result.controls)require(!overlap(a,d.offset)&&!overlap(a,d.scale),"Values clear original-sized offset/scale");
      }
      for(int i=0;i<count;++i){
        int col=i%result.columns;
        const auto& v=d.values[i];
        require(near(v.x+v.width*.5f,1.f+(col+.5f)*52.f/result.columns),"Incomplete rows retain fixed columns");
        if(col)require(near(v.y-d.values[i-1].y,-columnRiseRatio*result.diameter),"Each column rises evenly to the right");
        if(i>=result.columns)require(near(v.x,d.values[i-result.columns].x),"Rows retain the same column positions");
      }
    }
  }
  require(calculate(3,6).columns==2,"Three divisions with six values use two columns by three rows");
  require(calculate(2,8).columns==2&&calculate(2,9).columns==3,"Two divisions switch from two to three columns after eight");
  require(calculate(4,6).columns==3&&calculate(4,10).columns==4,"Shorter divisions use wider grids");
  require(calculate(2,5).diameter>=22,"Five values retain large knobs in two divisions");
  auto four=calculate(2,4);require(four.groups[0].values[2].y-four.groups[0].values[0].y<=30.001f,"Sparse rows do not spread apart");
  require(!calculate(0,6).valid&&!calculate(6,6).valid&&!calculate(2,17).valid,"Invalid layout inputs are rejected");
  std::puts("Voly fixed grid, even slope, native bounds and equal divisions passed");
}
