#include "TolyPensor/Matrix.hpp"
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <random>
struct Four {
  float x[4];explicit Four(float v){for(auto& n:x)n=v;}
  static Four load(const float* p){Four v(0);for(int i=0;i<4;++i)v.x[i]=p[i];return v;}
  void store(float* p)const{for(int i=0;i<4;++i)p[i]=x[i];}
  Four operator+(const Four& b)const{Four v(0);for(int i=0;i<4;++i)v.x[i]=x[i]+b.x[i];return v;}
  Four operator*(const Four& b)const{Four v(0);for(int i=0;i<4;++i)v.x[i]=x[i]*b.x[i];return v;}
  Four& operator+=(const Four& b){for(int i=0;i<4;++i)x[i]+=b.x[i];return *this;}
};
void require(bool ok,const char* text){if(!ok){std::fprintf(stderr,"FAIL: %s\n",text);std::exit(1);}}
int main(){
  auto m=std::unique_ptr<toly::Matrix>(new toly::Matrix);
  std::array<int,16> widths; widths.fill(16);
  std::array<float,256> in{},out{};
  for(int i=0;i<256;++i)in[i]=(i-128)*.03125f;
  m->prepare(widths,65535,16);m->process<Four>(in,out);
  require(in==out,"Default routing passes each matching port and channel exactly");
  require(m->activePacks()==256,"Identity routing compiles only active source packs");
  m->scale[toly::InPort][0]=2;m->offset[toly::OutPort][0]=3;m->changed();
  m->prepare(widths,65535,16);m->process<Four>(in,out);
  require(out[0]==in[0]*2+3,"Affine cache refreshes when knobs change with unchanged connections");
  m->scale[toly::InPort][0]=1;m->offset[toly::OutPort][0]=0;m->changed();
  std::mt19937 rng(47);std::uniform_real_distribution<float> value(-1,1);
  for(int i=0;i<toly::cells;++i)m->set(i,value(rng));
  for(int axis=0;axis<4;++axis)for(int i=0;i<16;++i){m->scale[axis][i]=value(rng);m->offset[axis][i]=value(rng);}
  m->changed();
  for(int poly : {1,7,16})for(unsigned mask : {0u,1u,0xaaaaU,65535u}){
    for(int ip=0;ip<16;++ip)widths[ip]=mask==65535u?16:(ip*7+poly)%17;
    m->prepare(widths,mask,poly);m->process<Four>(in,out);
    for(int op=0;op<16;++op)for(int oc=0;oc<16;++oc){
      double expected=0;
      if((mask&(1u<<op))&&oc<poly){
        for(int ip=0;ip<16;++ip)for(int ic=0;ic<widths[ip];++ic){
          float source=in[toly::signal(ip,ic)]*m->scale[0][ip]*m->scale[1][ic]+m->offset[0][ip]+m->offset[1][ic];
          expected+=source*m->get(toly::cell(ip,ic,op,oc));
        }
        expected=expected*m->scale[3][oc]*m->scale[2][op]+m->offset[3][oc]+m->offset[2][op];
      }
      require(std::abs(out[toly::signal(op,oc)]-expected)<.0001,
              "Sparse vector packs match an independent scalar sum with partial polyphony and all four affine stages");
    }
  }
  std::array<int,4> fixed{{2,5,7,11}};
  for(int row=0;row<4;++row)for(int col=0;col<4;++col)if(row!=col)
    for(int a=0;a<16;++a)for(int b=0;b<16;++b){auto expected=fixed;expected[row]=a;expected[col]=b;require(toly::indices(toly::sliceCell(fixed,row,a,col,b))==expected,"Every contextual grid maps the chosen tensor axes without swapping coordinates");}
  for(int i=0;i<toly::cells;++i)m->set(i,0);m->changed();m->prepare(widths,65535,16);
  require(m->activePacks()==0,"Zero routes consume no audio multiply packs");
  std::puts("Toly Pensor tensor addressing, sparse/vector reference, polyphony, affine stages, and zero-route tests passed");
}
