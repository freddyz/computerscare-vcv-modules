#pragma once
inline void renderBeekKnobSamples(const std::string& output,float scale) {
  const std::vector<std::string> names={"Frequency","Cutoff frequency","Resonance","Pulse width","Gain","Frequency modulation","Channel 1 CV","Channel 2 CV","Attack","Decay","Time","Spread","Steps","Octave","Fine"};
  const std::vector<std::string> sources={"Fundamental/VCO","Fundamental/VCF","Fundamental/VCF","Fundamental/VCO","Fundamental/VCA-1","Fundamental/LFO","Fundamental/8vert","Fundamental/8vert","Fundamental/ADSR","Fundamental/ADSR","Fundamental/Delay","computerscare/VolyPector","Fundamental/SEQ3","AudibleInstruments/Plaits","Fundamental/VCO"};
  const std::vector<std::vector<std::string>> patches={
    {"Fundamental/VCO","Fundamental/LFO","Fundamental/VCF"},
    {"Fundamental/VCO","Fundamental/VCO","Fundamental/VCA-1","Fundamental/Delay"},
    {"Fundamental/8vert","Fundamental/ADSR","computerscare/SlolyPit"},
    {"Fundamental/ADSR","Fundamental/Delay","computerscare/VolyPector"},
    {"AudibleInstruments/Plaits","Fundamental/SEQ3","Fundamental/VCO"}
  };
  // Curate the spectrum using real module collections, never forced artwork.
  const int shapes[]={1,6,7,6,1,6,0,3,8,6,6,6,1,6,5};
  const int sides[]={3,0,0,0,4,0,0,0,0,0,0,0,5,0,0};
  const std::vector<std::string> pool={"Fundamental/VCO","Fundamental/LFO","Fundamental/VCF","Fundamental/VCA-1","Fundamental/ADSR","Fundamental/Delay","Fundamental/8vert","Fundamental/SEQ3","computerscare/SlolyPit","AudibleInstruments/Plaits"};
  engine::Module module;module.config(15,0,0,0);
  struct Sheet : widget::Widget {
    std::vector<std::string> names,sources;
    void draw(const DrawArgs& args) override {
      auto font=computerscare::type::font();
      for(int i=0;i<15;++i) {
        float x=(i%5)*150,y=(i/5)*145;bool dark=i%2;
        nvgBeginPath(args.vg);nvgRect(args.vg,x,y,150,145);
        nvgFillColor(args.vg,dark?nvgRGB(37,37,37):nvgRGB(214,214,214));nvgFill(args.vg);
        if(!font)continue;
        nvgFontFaceId(args.vg,font->handle);nvgFontSize(args.vg,13);
        nvgTextAlign(args.vg,NVG_ALIGN_CENTER|NVG_ALIGN_TOP);
        nvgFillColor(args.vg,dark?nvgRGB(235,235,235):nvgRGB(28,28,28));
        nvgTextBox(args.vg,x+7,y+97,136,names[i].c_str(),nullptr);
        nvgFontSize(args.vg,8);nvgFillColor(args.vg,dark?nvgRGB(164,164,164):nvgRGB(94,94,94));
        nvgText(args.vg,x+75,y+131,sources[i].c_str(),nullptr);
      }
      widget::Widget::draw(args);
    }
  } sheet;
  sheet.names=names;sheet.sources=sources;sheet.box.size=Vec(750,435);
  for(int i=0;i<15;++i) {
    bool discrete=i==12||i==13;float maximum=discrete?8.f:1.f;
    auto* q=module.configParam(i,0,maximum,0,names[i]);q->snapEnabled=discrete;
    float value=maximum*float((i*7+3)%19)/18.f;
    module.params[i].setValue(discrete?std::round(value):value);
    auto* knob=new beekapoo::ParametricKnob;
    knob->module=&module;knob->paramId=i;knob->initParamQuantity();knob->snap=discrete;
    int group=i/3;
    auto identity=beekapoo::patchIdentity(patches[group],{{0,group%3,1,0},{1,1,2,group%4}});
    for(unsigned sample=0;;++sample) {
      auto style=beekapoo::controlStyle(names[i],sources[i],identity);
      if(style.shape==shapes[i]&&(!sides[i]||3+style.lobes%3==sides[i]))break;
      if(sample>=4096)throw std::runtime_error("Unable to find knob spectrum family");
      std::vector<std::string> collection={sources[i]};
      uint64_t hash=beekapoo::mixIdentity(sample,i/3);
      for(unsigned m=0;m<2+sample%8;++m) {
        hash=beekapoo::mixIdentity(hash,m+1);
        collection.push_back(pool[hash%pool.size()]);
      }
      identity=beekapoo::patchIdentity(collection,{{0,group%3,1,0},{1,1,2,group%4}});
    }
    knob->setIdentity(names[i],sources[i],identity);knob->setDark(i%2);knob->step();
    float size=68+float((i*3)%5)*3;
    auto* transform=new widget::TransformWidget;
    transform->box=Rect(Vec((i%5)*150+75-size/2,(i/5)*145+48-size/2),Vec(size,size));
    transform->scale(Vec(size/24,size/24));transform->addChild(knob);sheet.addChild(transform);
  }
  capture(&sheet,output+"/knobs-15.png",scale,false);
}
inline void renderBeekKnobNumbers(const std::string& output,float scale) {
  engine::Module module;module.config(12,0,0,0);
  struct Sheet : widget::Widget {
    void draw(const DrawArgs& args) override {
      for(int i=0;i<12;++i){nvgBeginPath(args.vg);nvgRect(args.vg,(i%3)*120,(i/3)*110,120,110);nvgFillColor(args.vg,i%2?nvgRGB(37,37,37):nvgRGB(214,214,214));nvgFill(args.vg);}
      widget::Widget::draw(args);
    }
  } sheet;sheet.box.size=Vec(360,440);
  for(int row=0;row<4;++row) {
    beekapoo::PatchIdentity identity;
    for(int candidate=0;;++candidate) {
      identity=beekapoo::patchIdentity({"Maker "+std::to_string(candidate)+"/VCO"},{});
      if(beekapoo::mixIdentity(identity.manufacturerTypes,0x6e756d626572ull)%4==unsigned(row))break;
    }
    for(int col=0;col<3;++col) {
      int i=row*3+col;
      std::string name=col==0?"Channel "+std::to_string(row+1)+" level":col==1?"Steps":"Channel 16 CV";
      auto* q=module.configParam(i,0,16,0,name);q->snapEnabled=col==1;module.params[i].setValue(9+row);
      auto* knob=new beekapoo::ParametricKnob;knob->module=&module;knob->paramId=i;knob->initParamQuantity();knob->snap=col==1;
      knob->setGroupedNumber(true);
      knob->setIdentity(name,col==1?"Sequencer":"VCA",identity);knob->setDark(i%2);knob->step();
      auto* transform=new widget::TransformWidget;transform->box=Rect(Vec(col*120+24,row*110+19),Vec(72,72));
      transform->scale(Vec(3,3));transform->addChild(knob);sheet.addChild(transform);
    }
  }
  capture(&sheet,output+"/knob-numbers.png",scale,false);
}
inline void renderBeekKnobSpectrum(const std::string& output, float scale) {
  const char* mode=std::getenv("BEEKAPOO_KNOBS_ONLY");
  if(mode&&std::string(mode)=="numbers") {renderBeekKnobNumbers(output,scale);return;}
  if(mode&&std::string(mode)=="15") {renderBeekKnobSamples(output,scale);return;}
  engine::Module module;
  module.config(96,0,0,0);
  struct Sheet : widget::Widget {
    void draw(const DrawArgs& args) override {
      nvgBeginPath(args.vg);nvgRect(args.vg,0,0,box.size.x,box.size.y);
      nvgFillColor(args.vg,nvgRGB(214,214,214));nvgFill(args.vg);
      widget::Widget::draw(args);
    }
  } sheet;
  sheet.box.size=Vec(924,624);
  const char* names[]={"Frequency","Resonance","Cutoff frequency","Pulse width","Decay","Gain","Spread","Shape","Tempo","Drive","Fine","Level"};
  const std::vector<std::vector<std::string>> patches={
    {"Fundamental/VCO","Fundamental/LFO","Fundamental/VCF"},
    {"Fundamental/ADSR","Fundamental/VCMixer","Fundamental/Delay","Fundamental/VCO"},
    {"computerscare/VolyPector","Fundamental/VCO","Fundamental/VCO","AudibleInstruments/Plaits"},
    {"Fundamental/SEQ3","computerscare/SlolyPit","Fundamental/8vert"}
  };
  std::array<int,9> families{};
  for(int i=0;i<96;++i) {
    int row=i/12,col=i%12;
    bool discrete=row>=6;
    float maximum=discrete?float(3+col):1.f;
    auto* q=module.configParam(i,0,maximum,0,names[col]);
    q->snapEnabled=discrete;
    float value=maximum*float((i*7+3)%19)/18.f;
    module.params[i].setValue(discrete?std::round(value):value);
    auto* knob=new beekapoo::ParametricKnob;
    knob->module=&module;knob->paramId=i;knob->initParamQuantity();knob->snap=discrete;
    auto types=patches[row%patches.size()];
    if(row>=4)types.push_back(types.front());
    auto identity=beekapoo::patchIdentity(types,{{0,row%3,1,0},{1,1,2,col%4}});
    knob->setIdentity(names[col],types[col%types.size()],identity);
    knob->step();
    ++families[knob->style.shape];
    for(const auto& point:knob->profile)
      if(point.norm()+knob->style.bevel>11.51f)throw std::runtime_error("Knob silhouette exceeds its hitbox");
    if(std::abs(knob->style.ink-knob->style.color().r)<.2f)throw std::runtime_error("Knob indicator lacks contrast");
    float size=49+float((i*3)%5)*2;
    auto* transform=new widget::TransformWidget;
    transform->box=Rect(Vec(12+col*75+(60-size)/2,12+row*75+(60-size)/2),Vec(size,size));
    transform->scale(Vec(size/24,size/24));transform->addChild(knob);sheet.addChild(transform);
  }
  capture(&sheet,output+"/knobs-only.png",scale,false);
  std::fprintf(stderr,"Knob spectrum: %d circles, %d squares, %d skew squares, %d lopsided, %d polygons, %d lobed, %d notched, %d curved squares, %d split polygons\n",families[6],families[0],families[3],families[5],families[1],families[2],families[4],families[7],families[8]);
}

template<class Require>
void testBeekKnobRendering(const std::string& output,float scale,Require require) {
  {
    beekapoo::ParametricKnob a,b;
    auto single=beekapoo::patchIdentity({"Maker/VCO"},{});
    auto multiple=beekapoo::patchIdentity({"Maker/LFO","Maker/LFO","Maker/Filter"},{{0,1,2,3}});
    a.setIdentity("Channel 1 level","Maker/VCO",single);b.setIdentity("Channel 8 CV","Maker/Filter",multiple);
    require(a.numberCorner==b.numberCorner,"Number placement must depend only on the unique manufacturers, not counts, modules, cables or parameter names");
    std::set<unsigned> positions;
    for(int i=0;i<32;++i) {
      a.setIdentity("Steps","Sequencer",beekapoo::patchIdentity({"Maker "+std::to_string(i)+"/VCO"},{}));positions.insert(a.numberCorner);
      auto p=a.numberPosition();require(p.x>=3.5f&&p.x<=20.5f&&p.y>=3.5f&&p.y<=20.5f,"Number corner must fit in its existing layout cell");
    }
    require(positions.size()==4,"Different manufacturer sets must provide all four outside number positions");
  }
  int round=0,elongated=0,total=0;
  std::array<int,4> borders{};
  std::set<int> checkedEdges;
  for(int seed=0;seed<400;++seed) {
    auto patch=beekapoo::patchIdentity({"Maker/VCO","Maker/Filter "+std::to_string(seed)},{});
    for(const char* label:{"Frequency","Cutoff","Resonance","Gain","Attack","Decay","Level","Drive"}) {
      auto style=beekapoo::controlStyle(label,"Maker/VCO",patch);
      auto related=beekapoo::controlStyle(label,"Maker/LFO",patch);
      ++total;round+=style.shape==6;elongated+=std::min(style.aspectX,style.aspectY)<.88f;
      ++borders[style.border];
      if(style.shape==1||style.shape==7||style.shape==8) {
        int sides=style.shape==1?3+style.lobes%3:style.shape;
        if(checkedEdges.insert(sides).second) {
          beekapoo::ParametricKnob knob;knob.setIdentity(label,"Maker/VCO",patch);
          int corners=0;
          for(size_t i=0;i<knob.profile.size();++i) {
            auto a=knob.profile[i],b=knob.profile[(i+1)%knob.profile.size()],c=knob.profile[(i+2)%knob.profile.size()];
            auto u=b-a,v=c-b;
            corners+=std::abs(u.x*v.y-u.y*v.x)>.0001f;
            require(a.norm()+style.bevel<=11.51f,"Straight and hybrid knobs must fit at every rotation");
          }
          if(style.shape==1)require(corners==sides,"Polygons must keep exactly their original straight edges and corners");
          else if(style.shape==8)require(corners==6,"Triangle/hexagon hybrid must retain six straight edges");
          else require(corners>6&&corners<20,"Curved square must retain three straight sides and only one curved side");
        }
      }
      require(style.border==related.border&&style.ridges==related.ridges&&style.borderWidth==related.borderWidth,"Related parameters must share edge treatments across source modules");
      if(style.shape==6)require(style.aspectX==1&&style.aspectY==1,"Normal circles must never be stretched");
    }
    auto a=beekapoo::controlStyle("Channel 1 level","Maker/VCO",patch),b=beekapoo::controlStyle("Channel 4 level","Maker/VCO",patch);
    require(a.shape==b.shape&&a.border==b.border&&a.ridges==b.ridges,"Numbered families must share their shape and rim");
  }
  require(checkedEdges.size()==5,"All polygon and mixed-edge families must occur in the hash spectrum");
  require(round>total*.45&&round<total*.55,"Circles must account for about half the generated styles");
  require(elongated<total*.10,"Oblong controls must remain uncommon");
  for(int count:borders)require(count>total*.1,"All four edge treatments must remain available");
  std::fprintf(stderr,"Knob family checks: %d/%d circles, %d oblong; shared borders passed\n",round,total,elongated);
  engine::Module module;module.config(1,0,0,0);module.configParam(0,0,1,.25f,"Position");
  widget::Widget sheet;sheet.box.size=Vec(300,75);
  std::vector<beekapoo::ParametricKnob*> knobs;
  for(int i=0;i<4;++i) {
    auto* knob=new beekapoo::ParametricKnob;
    knob->module=&module;knob->paramId=0;knob->initParamQuantity();
    knob->setIdentity("Frequency modulation","Fundamental/VCO");
    knob->style.indicator=i;
    knobs.push_back(knob);
    auto* scaled=new widget::TransformWidget;scaled->box=Rect(Vec(12+i*74,12),Vec(50,50));scaled->scale(Vec(2,2));scaled->addChild(knob);sheet.addChild(scaled);
  }
  for(float value:{.12f,.39f,.76f}) {
    module.params[0].setValue(value);
    for(auto* knob:knobs) {
      knob->step();
      // Unlike capture(), this uses a top-level draw. Rack refuses a rotated
      // FramebufferWidget; rendering its children directly would hide that bug.
      APP->window->fbCount()=0;
      auto* vg=APP->window->vg;nvgBeginFrame(vg,100,100,1);
      widget::Widget::DrawArgs args;args.vg=vg;args.clipBox=Rect(Vec(),Vec(24,24));args.fb=nullptr;
      knob->draw(args);nvgEndFrame(vg);
      require(knob->faceBuffer->getImageHandle()>=0&&!knob->faceBuffer->dirty,"Rotated knob body must render through Rack's live framebuffer path");
      for(const auto& point:knob->profile)require(point.norm()+knob->style.bevel<=11.51f,"Whole rotating knob must stay inside its hitbox at every angle");
    }
    capture(&sheet,output+string::f("/knob-markers-%d.png",int(value*100)),scale,false);
  }
  std::puts("Rotating knob bodies, live framebuffer rendering, bounded geometry and marker variants passed");
}
