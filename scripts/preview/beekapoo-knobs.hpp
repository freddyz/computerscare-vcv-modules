#pragma once
inline void renderBeekKnobSpectrum(const std::string& output, float scale) {
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
  std::array<int,7> families{};
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
  std::fprintf(stderr,"Knob spectrum: %d circles, %d squares, %d skew squares, %d lopsided, %d polygons, %d lobed, %d notched\n",families[6],families[0],families[3],families[5],families[1],families[2],families[4]);
}

template<class Require>
void testBeekKnobRendering(const std::string& output,float scale,Require require) {
  int round=0,elongated=0,total=0;
  std::array<int,4> borders{};
  for(int seed=0;seed<400;++seed) {
    auto patch=beekapoo::patchIdentity({"Maker/VCO","Maker/Filter "+std::to_string(seed)},{});
    for(const char* label:{"Frequency","Cutoff","Resonance","Gain","Attack","Decay","Level","Drive"}) {
      auto style=beekapoo::controlStyle(label,"Maker/VCO",patch);
      auto related=beekapoo::controlStyle(label,"Maker/LFO",patch);
      ++total;round+=style.shape==6;elongated+=std::min(style.aspectX,style.aspectY)<.88f;
      ++borders[style.border];
      require(style.border==related.border&&style.ridges==related.ridges&&style.borderWidth==related.borderWidth,"Related parameters must share edge treatments across source modules");
      if(style.shape==6)require(style.aspectX==1&&style.aspectY==1,"Normal circles must never be stretched");
    }
    auto a=beekapoo::controlStyle("Channel 1 level","Maker/VCO",patch),b=beekapoo::controlStyle("Channel 4 level","Maker/VCO",patch);
    require(a.shape==b.shape&&a.border==b.border&&a.ridges==b.ridges,"Numbered families must share their shape and rim");
  }
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
