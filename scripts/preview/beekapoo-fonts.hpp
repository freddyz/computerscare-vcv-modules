#pragma once
template<class Require>
void renderBeekFonts(const std::string& output,float scale,Require require) {
  using namespace computerscare::type;
  auto* vg=APP->window->vg;
  for(auto face:{Face::Text,Face::Display}) {
    auto loaded=font(face);
    require(loaded&&loaded->handle!=APP->window->uiFont->handle,"Both bundled fonts must load through Rack");
    nvgSave(vg);nvgFontFaceId(vg,loaded->handle);nvgFontSize(vg,24);
    float width=-1;
    std::string phrase="Frequency CV3 0123456789 ÀÉñø ÷±%";
    for(int variant=0;variant<4;++variant) {
      auto text=alternates(phrase,0,variant);float bounds[4];
      float next=nvgTextBounds(vg,0,0,text.c_str(),nullptr,bounds);
      require(std::isfinite(next)&&next>200,"Variant glyphs must have real text metrics");
      require(width<0||std::abs(next-width)<.01f,"Changing a glyph alternate must not alter the text advance");width=next;
    }
    nvgRestore(vg);
  }
  require(alternates(" \t\n\r\xC2\xA0")==" \t\n\r\xC2\xA0","Variant encoding must preserve wrapping whitespace");
  require(alternates("🎛 日本語")=="🎛 日本語","Unsupported characters must retain their Unicode fallback");
  Text cache;std::string source="MOSS / ReED 7";
  auto first=cache.get(source,73);auto* storage=cache.glyphs.data();
  require(cache.get(source,73)==first&&cache.glyphs.data()==storage,"Unchanged captions must reuse their cached glyph strings");
  require(source=="MOSS / ReED 7"&&cache.source==source,"Font alternates must never rewrite saved/clipboard text");
  require(alternates("aaaaAAAA",1)!=alternates("aaaaAAAA",9),"A different identity must select different glyph alternates");
  // Cursor and mouse hit testing use the exact same font as the native field.
  Field field;field.box.size=Vec(400,25);field.setText("Frequency 123");
  for(float x:{5.f,20.f,45.f,80.f,120.f,200.f}) {
    bndSetFont(handle());int expected=bndTextFieldTextPosition(vg,0,0,400,25,-1,field.text.c_str(),x,12);
    bndSetFont(APP->window->uiFont->handle);
    require(field.getTextPosition(Vec(x,12))==expected,"Name field hit testing must use the custom font, not Rack's default metrics");
  }
  struct Specimen:widget::Widget {
    Face face;
    void draw(const DrawArgs& args) override {
      auto* vg=args.vg;
      for(int dark=0;dark<2;++dark) {
        nvgSave(vg);nvgTranslate(vg,dark*460.f,0);
        nvgBeginPath(vg);nvgRect(vg,0,0,460,640);
        nvgFillColor(vg,dark?nvgRGB(30,30,30):nvgRGB(226,226,224));nvgFill(vg);
        nvgFontFaceId(vg,handle(face));nvgTextAlign(vg,NVG_ALIGN_LEFT|NVG_ALIGN_TOP);
        nvgFillColor(vg,dark?nvgRGB(236,236,233):nvgRGB(30,30,30));
        auto line=[&](const std::string& source,float size,float y,int variant=-1) {
          auto text=alternates(source,421,variant);nvgFontSize(vg,size);nvgText(vg,25,y,text.c_str(),nullptr);
        };
        line(face==Face::Text?"Computerscare Text":"Computerscare Display",32,24);
        line("ABCDEFGHIJKLMNOPQRSTUVWXYZ",21,83);
        line("abcdefghijklmnopqrstuvwxyz",24,120);
        line("0123456789  !? @#% & +-= / () [] {}",23,158);
        line("Four outlines per character",16,210);
        for(int variant=0;variant<4;++variant)
          line("Aa Bb Gg Rr 0O 1Il 4 8 &",30,242+variant*46,variant);
        line("Frequency  Cutoff  Resonance",22,440);
        line("Pulse Width   Channel 3 CV",22,478);
        line("moss / reed 7 — TIN HINGE",26,518);
        line("FM  V/Oct  CV3  100%  Edit  Eject",13,571);
        line("Ångström  déjà vu  ±12 dB  440 Hz",15,601);
        nvgRestore(vg);
      }
    }
  } specimen;
  specimen.box.size=Vec(920,640);
  for(auto face:{Face::Text,Face::Display}) {
    specimen.face=face;
    capture(&specimen,output+(face==Face::Text?"/font-text.png":"/font-display.png"),scale,false);
  }
  std::puts("Custom fonts: both faces, four equal-width alternates, Unicode/whitespace, cached labels and name-field hit testing passed");
}
