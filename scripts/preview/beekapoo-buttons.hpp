#pragma once
inline void renderBeekButtonSamples(const std::string& output,float scale) {
  std::set<std::string> vocabularies;
  for(int i=0;i<256;++i) {
    auto identity=beekapoo::patchIdentity({"Maker "+std::to_string(i)+"/Button"},{});
    auto words=beekapoo::toggleWords(identity);
    auto rewired=identity;rewired.wiring^=123;
    auto same=beekapoo::toggleWords(rewired);
    if(words.on!=same.on||words.off!=same.off)throw std::runtime_error("Toggle vocabulary must stay stable when only connections change");
    vocabularies.insert(words.on+"/"+words.off);
  }
  if(vocabularies.size()!=15)throw std::runtime_error("Toggle fingerprints must cover five word pairs and three capitalizations");
  struct CustomState : engine::ParamQuantity {
    std::string getDisplayValueString() override {return getValue()>getMinValue()?"Running":"Stopped";}
  };
  struct NumericState : engine::ParamQuantity {
    std::string getDisplayValueString() override {return getValue()>getMinValue()?"1":"0";}
  };
  engine::Module module;module.config(24,0,0,0);
  struct Sheet : widget::Widget {
    std::vector<std::string> names;
    static float background(int column) {
      const float shades[]={.95f,.8f,.66f,.32f,.18f,.06f};return shades[column];
    }
    void draw(const DrawArgs& args) override {
      auto font=computerscare::type::font();
      for(int i=0;i<24;++i) {
        float x=(i%6)*120,y=(i/6)*125,bg=background(i%6);
        nvgBeginPath(args.vg);nvgRect(args.vg,x,y,120,125);
        nvgFillColor(args.vg,nvgRGBf(bg,bg,bg));nvgFill(args.vg);
        float ink=computerscare::contrast::textInk(computerscare::contrast::linear(bg));
        if(font){nvgFontFaceId(args.vg,font->handle);nvgFontSize(args.vg,12);nvgTextAlign(args.vg,NVG_ALIGN_CENTER|NVG_ALIGN_TOP);nvgFillColor(args.vg,nvgRGBf(ink,ink,ink));nvgText(args.vg,x+60,y+102,names[i].c_str(),nullptr);}
      }
      widget::Widget::draw(args);
    }
  } sheet;
  sheet.box.size=Vec(720,500);
  const char* labels[]={"Run","Range","Direction","Mode","Synchronize clock","Sync","Freeze","Run","Reset","Gate","Loop","Direction","Randomize pattern","Shape","Link","Offset","Retrigger","Clock","Quantize","Polarity","Arm","Trigger","Latch","FM mode"};
  // Generated geometry, with deliberate face/background combinations to
  // inspect the entire contrast range rather than lucky hash-selected fills.
  const float faces[]={.02f,.27f,.52f,.88f};
  for(int i=0;i<24;++i) {
    std::string source=i%3==0?"VCV/VCO":i%3==1?"Valley/Plateau":"Bogaudio/FMOp";
    auto identity=beekapoo::patchIdentity({source,"VCV/LFO",i%2?"VCV/VCF":"Bogaudio/VCA",std::string("Maker ")+std::to_string(i/4)+"/Control"},{{0,i%4,1,i%3}});
    bool momentary=i%4==0,discrete=i%4==1;
    engine::ParamQuantity* q=module.configSwitch(i,0,discrete?3:1,0,labels[i]);
    if(i==2)q=module.configSwitch(i,0,1,0,labels[i],{"Reverse","Forward"});
    if(i==5)q=module.configSwitch(i,0,3,0,labels[i],{"Free","Clock","Gate","Reset"});
    if(i==7)q=module.configParam<CustomState>(i,0,1,0,labels[i]);
    if(i==15)q=module.configParam<NumericState>(i,0,1,0,labels[i]);
    if(i==18)q=module.configParam(i,0,1,0,labels[i]);
    if(i==7||i==15||i==18)q->snapEnabled=true;
    module.params[i].setValue(discrete?float(i%3):float(i%3==0));
    auto* button=new beekapoo::MappedParam<beekapoo::ExposedSwitch>;
    button->module=&module;button->paramId=i;button->momentary=momentary;button->initParamQuantity();
    float bg=Sheet::background(i%6);
    auto background=nvgRGBf(bg,bg,bg);
    button->setIdentity(labels[i],source,identity,i%6>=3,background);button->setVisualSize(80);
    button->box.pos=Vec((i%6)*120+20,(i/6)*125+12);
    for(auto* child:button->children)if(auto* face=dynamic_cast<beekapoo::ExposedSwitch::Face*>(child)) {
      if(momentary && (face->label()!=q->getLabel() || !face->showLabel || face->upDepth<4.f))
        throw std::runtime_error("Momentary push caps must show their parameter name");
      if(!momentary && (face->label().empty() || !face->showLabel))
        throw std::runtime_error("Toggle and multi-position buttons must retain their value labels");
      float gray=faces[i/6];face->faceColor=nvgRGBf(gray,gray,gray);face->updateContrast(background);
      for(int shade=0;shade<phlooper::WarpedDropdown::SHADE_COUNT;++shade) {
        auto fill=face->shadeColor(phlooper::WarpedDropdown::Shade(shade));
        float surface=computerscare::contrast::luminance(fill.r,fill.g,fill.b);
        if(computerscare::contrast::ratio(computerscare::contrast::linear(face->textInks[shade]),surface)<4.5f)
          throw std::runtime_error("Button text must contrast in raised and recessed states");
      }
      if(face->borderColor.r>phlooper::WarpedDropdown::brightestBorder+.0001f)
        throw std::runtime_error("Button outline exceeds the soft-gray ceiling");
      if(i/6>=2&&face->borderColor.r>.001f)
        throw std::runtime_error("Medium and light button faces should retain dark outlines on these backgrounds");
    }
    if(!q->snapEnabled)throw std::runtime_error("Button rendering must preserve native switch policy");
    sheet.names.push_back(momentary?"":labels[i]);sheet.addChild(button);
  }
  capture(&sheet,output+"/buttons-contrast-24.png",scale,false);
  for(int i=0;i<24;++i) {
    auto* q=module.getParamQuantity(i);
    module.params[i].setValue(module.params[i].getValue()>q->getMinValue()?q->getMinValue():q->getMaxValue());
  }
  capture(&sheet,output+"/buttons-opposite-states.png",scale,false);
  // Exercise the exact generated glyphs in both states at native and enlarged
  // scales. Their measured ink must fit inside the face, not merely its advance.
  auto* vg=APP->window->vg;
  nvgBeginFrame(vg,720,500,1);
  for(auto* child:sheet.children) {
    auto* button=dynamic_cast<beekapoo::MappedParam<beekapoo::ExposedSwitch>*>(child);
    if(!button)continue;
    auto* quantity=button->getParamQuantity();float saved=quantity->getValue();
    for(auto* item:button->children)if(auto* face=dynamic_cast<beekapoo::ExposedSwitch::Face*>(item)) {
      auto font=face->labelFont();nvgFontFaceId(vg,font->handle);
      std::map<float,std::string> momentaryWords;
      for(float visualScale:{1.f,2.f,3.2f})for(bool pressed:{false,true}) {
        quantity->setValue(pressed?quantity->getMaxValue():quantity->getMinValue());
        if(!button->momentary) {
          bool custom=button->paramId==2||button->paramId==5||button->paramId==7||button->paramId==15;
          std::string expected=custom?quantity->getDisplayValueString():quantity->getRange()<=1?(pressed?button->words.on:button->words.off):std::to_string(int(std::round(quantity->getValue())));
          if(face->label()!=expected)throw std::runtime_error("Native switch/custom labels must win over fallback words, including numeric custom labels");
        }
        auto shape=face->geometry(pressed);auto area=face->labelArea(shape);
        nvgSave(vg);nvgScale(vg,visualScale,visualScale);
        std::string text=face->displayLabel();
        if(button->momentary) {
          if(pressed&&momentaryWords[visualScale]!=text)throw std::runtime_error("Momentary abbreviation must not change when pressed");
          momentaryWords[visualScale]=text;
          if(button->paramId==0&&visualScale>=3.2f&&text!=computerscare::type::alternates("Run",face->shapeSeed))throw std::runtime_error("Roomy momentary buttons must retain short full names");
        }
        auto fitted=face->fitText(vg,text,area);
        float ink[4];nvgTextBounds(vg,fitted.origin.x,fitted.origin.y,text.c_str(),nullptr,ink);
        if(!text.empty() && (ink[0]<area.pos.x-.05f||ink[1]<area.pos.y-.05f||ink[2]>area.pos.x+area.size.x+.05f||ink[3]>area.pos.y+area.size.y+.05f))
          throw std::runtime_error(string::f("Generated button word clipped: %s state %d scale %.1f area %.3f %.3f %.3f %.3f ink %.3f %.3f %.3f %.3f",quantity->getLabel().c_str(),pressed,visualScale,area.pos.x,area.pos.y,area.pos.x+area.size.x,area.pos.y+area.size.y,ink[0],ink[1],ink[2],ink[3]));
        nvgRestore(vg);
        if(pressed) {
          auto onEdge=[](Vec a,Vec b,Vec p){Vec d=b-a,v=p-a;return std::abs(d.x*v.y-d.y*v.x)<.001f;};
          if(!onEdge(shape.back[1],shape.back[2],shape.face[1])||!onEdge(shape.back[3],shape.back[2],shape.face[3]))
            throw std::runtime_error("Recessed button corners must meet their socket edges");
        }
      }
    }
    quantity->setValue(saved);
  }
  nvgEndFrame(vg);
  printf("Native switch/custom labels, unnamed fallbacks, momentary abbreviations and unclipped words in both states passed\n");
}
