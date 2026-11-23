#pragma once
inline void renderBeekButtonSamples(const std::string& output,float scale) {
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
  const char* labels[]={"Hold","Range","Enable","Mode","Mute","Sync","Freeze","Run","Reset","Gate","Loop","Direction","Bypass","Shape","Link","Offset","Retrigger","Clock","Quantize","Polarity","Arm","Trigger","Latch","FM mode"};
  // Generated geometry, with deliberate face/background combinations to
  // inspect the entire contrast range rather than lucky hash-selected fills.
  const float faces[]={.02f,.27f,.52f,.88f};
  for(int i=0;i<24;++i) {
    std::string source=i%3==0?"VCV/VCO":i%3==1?"Valley/Plateau":"Bogaudio/FMOp";
    auto identity=beekapoo::patchIdentity({source,"VCV/LFO","VCV/VCF"},{{0,i%4,1,i%3}});
    bool momentary=i%4==0,discrete=i%4==1;
    auto* q=module.configSwitch(i,0,discrete?3:1,0,labels[i]);
    module.params[i].setValue(discrete?float(i%3):float(i%3==0));
    auto* button=new beekapoo::MappedParam<beekapoo::ExposedSwitch>;
    button->module=&module;button->paramId=i;button->momentary=momentary;button->initParamQuantity();
    float bg=Sheet::background(i%6);
    auto background=nvgRGBf(bg,bg,bg);
    button->setIdentity(labels[i],source,identity,i%6>=3,background);button->setVisualSize(80);
    button->box.pos=Vec((i%6)*120+20,(i/6)*125+12);
    for(auto* child:button->children)if(auto* face=dynamic_cast<beekapoo::ExposedSwitch::Face*>(child)) {
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
    sheet.names.push_back(labels[i]);sheet.addChild(button);
  }
  capture(&sheet,output+"/buttons-contrast-24.png",scale,false);
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
      for(float visualScale:{1.f,2.f,3.2f})for(bool pressed:{false,true}) {
        quantity->setValue(pressed?quantity->getMaxValue():quantity->getMinValue());
        auto shape=face->geometry(pressed);auto area=face->labelArea(shape);
        nvgSave(vg);nvgScale(vg,visualScale,visualScale);
        std::string text=face->displayLabel();auto fitted=face->fitText(vg,text,area);
        float ink[4];nvgTextBounds(vg,fitted.origin.x,fitted.origin.y,text.c_str(),nullptr,ink);
        if(ink[0]<area.pos.x-.05f||ink[1]<area.pos.y-.05f||ink[2]>area.pos.x+area.size.x+.05f||ink[3]>area.pos.y+area.size.y+.05f)
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
  printf("Button ink, connected recessed corners and unclipped words in both states passed\n");
}
