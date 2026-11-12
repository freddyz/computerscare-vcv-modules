#include <cstdlib>
#include <iostream>
#include "Blunch/TimeBake.hpp"
using namespace blunch::time_bake;
void expect(const std::string& source,const std::string& wanted,double factor=2,ClockUnit unit=ClockUnit::Bpm) {
  try {
    auto got=page(source,unit,factor);
    if(got.text!=wanted){std::cerr<<source<<" -> "<<got.text<<" (wanted "<<wanted<<")\n";std::exit(1);}
  }catch(const std::exception& e){std::cerr<<source<<": "<<e.what()<<"\n";std::exit(1);}
}
int main() {
  expect("100bpm 2hz 500ms .5s 1min 1:30min","200bpm 4hz 250ms 0.25s 0.5min 45s");
  expect("[100 120]bpm@(2 4)#3","[200 240]bpm@(2 4)#3");
  expect("(100 (120 140))bpm","(200 (240 280))bpm");
  expect("{100|120}bpm {100-120}bpm","{200|240}bpm {200-240}bpm");
  expect("{100ms|2hz}","{50ms|4hz}");
  expect("{{100ms|2hz}|300ms}","{{50ms|4hz}|150ms}");
  expect("120bpm?75@3 60bpm??25","240bpm?75@3 120bpm??25");
  expect("120bpm@(2s (3s 4s))","240bpm@(1s (1.5s 2s))");
  expect("120bpm@{2|4}s","240bpm@{1|2}s");
  expect("(120 180)bpm#4s","(240 360)bpm#2s");
  expect("{120bpm@3s|240bpm#2s}","{240bpm@1.5s|480bpm#1s}");
  expect("w@3\nx#4\nw@4s\nx#2s\n~@2s","w@3\nx#4\nw@2s\nx#1s\n~@1s");
  expect("![100 200]ms $120bpm?50","![50 100]ms $240bpm?50");
  expect("100bpm","90bpm",.9);
  expect("100bpm","186.607bpm",std::pow(2.,.9));
  expect("0.000001hz","0.000002hz");
  expect("8\n\n120bpm","16\n\n240bpm");
  expect("8","4",2,ClockUnit::Seconds);
  auto mapped=page("100bpm\n500ms",ClockUnit::Bpm,.5);
  if(mapped.position(7)!=6 || mapped.position(12)!=12){std::cerr<<"Caret remap failed\n";return 1;}
  bool rejected=false;try{page("120bpm\n{",ClockUnit::Bpm,2);}catch(const std::exception&){rejected=true;}
  if(!rejected)return 1;
  std::cout<<"Blunch Time bake units, ranges, nested patterns, modifiers, precision and caret mapping passed\n";
}
