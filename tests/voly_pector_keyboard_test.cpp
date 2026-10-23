#include "VolyPectorKeyboard.hpp"
#include <cstdio>
#include <cstdlib>
void require(bool ok){if(!ok)std::abort();}
int main(){
  using namespace computerscare::voly;
  for(int i=0;i<16;++i){auto lower=focusKey('a'+i),upper=focusKey('A'+i);require(lower.band==i&&upper.band==i&&lower.channel==-1);}
  for(int i=0;i<15;++i){auto key=focusKey(channelKey(i));require(key.channel==i&&key.band==-1);}
  require(!focusKey('U').valid()&&!focusKey('Z').valid()&&!focusKey(' ').valid()&&channelKey(15)==0);
  std::puts("Voly Pector band/channel keyboard mappings passed");
}
