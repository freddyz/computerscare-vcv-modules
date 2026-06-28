#pragma once

#include <string>
#include <vector>

#include "Computerscare.hpp"

struct ComputerscareTextHighlight {
  int begin = 0;
  int end = 0;
  NVGcolor foreground = nvgRGB(0xd8, 0xf3, 0xec);
  NVGcolor background = nvgRGBA(0x00, 0x00, 0x00, 0x00);
  bool hasForeground = false;
  bool hasBackground = true;
};

struct ComputerscareTextEditorState {
  std::string text;
  bool dirty = false;
  std::vector<ComputerscareTextHighlight> highlights;
};

struct ComputerscareTextEditorStyle {
  std::string fontPath = "res/fonts/ShareTechMono-Regular.ttf";
  NVGcolor backgroundColor = nvgRGB(0x07, 0x09, 0x0a);
  NVGcolor borderColor = nvgRGB(0x24, 0xc9, 0xa6);
  NVGcolor textColor = nvgRGB(0xd8, 0xf3, 0xec);
  NVGcolor selectionColor = nvgRGBA(0x24, 0xc9, 0xa6, 0x85);
  NVGcolor placeholderColor = nvgRGBA(0xd8, 0xf3, 0xec, 0x61);
  float cornerRadius = 3.f;
};

struct ComputerscareTextEditor : ui::TextField {
  ComputerscareTextEditorState* state = nullptr;
  ComputerscareTextEditorStyle style;

  ComputerscareTextEditor();

  void setState(ComputerscareTextEditorState* editorState);
  void step() override;
  void draw(const DrawArgs& args) override;
  void drawLayer(const DrawArgs& args, int layer) override;
  void onButton(const ButtonEvent& e) override;
  void onChange(const ChangeEvent& e) override;
  int getTextPosition(Vec mousePos) override;

 protected:
  std::shared_ptr<Font> loadEditorFont();
  void drawHighlightBackgrounds(const DrawArgs& args);
  void drawEditorText(const DrawArgs& args);
  void drawHighlightForegrounds(const DrawArgs& args);
  void drawHighlightSpan(const DrawArgs& args,
                         const ComputerscareTextHighlight& highlight,
                         bool foreground);
};
