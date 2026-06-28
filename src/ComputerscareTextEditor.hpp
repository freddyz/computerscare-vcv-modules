#pragma once

#include <string>
#include <vector>

#include "Computerscare.hpp"

struct ComputerscareTextHighlight {
  int begin = 0;
  int end = 0;
  NVGcolor foreground = nvgRGB(0xd8, 0xf3, 0xec);
  NVGcolor background = nvgRGBA(0x00, 0x00, 0x00, 0x00);
  NVGcolor border = nvgRGBA(0x00, 0x00, 0x00, 0x00);
  NVGcolor progressColor = nvgRGBA(0x00, 0x00, 0x00, 0x00);
  bool hasForeground = false;
  bool hasBackground = true;
  bool hasBorder = false;
  bool hasProgress = false;
  bool fullLine = false;
  float progress = 0.f;
};

struct ComputerscareTextEditorState {
  std::string text;
  bool dirty = false;
  int submitCount = 0;
  int cancelCount = 0;
  int switchViewCount = 0;
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

struct ComputerscareTextEditorSnapshot {
  std::string text;
  int cursor = 0;
  int selection = 0;
};

struct ComputerscareTextEditor : ui::TextField {
  ComputerscareTextEditorState* state = nullptr;
  ComputerscareTextEditorStyle style;
  size_t maxUndoDepth = 128;
  bool submitOnEnter = false;

  ComputerscareTextEditor();

  void setState(ComputerscareTextEditorState* editorState);
  void step() override;
  void draw(const DrawArgs& args) override;
  void drawLayer(const DrawArgs& args, int layer) override;
  void onButton(const ButtonEvent& e) override;
  void onSelectText(const SelectTextEvent& e) override;
  void onSelectKey(const SelectKeyEvent& e) override;
  void onChange(const ChangeEvent& e) override;
  int getTextPosition(Vec mousePos) override;
  int getCursorLine() const;
  void setCursorLine(int line);

 protected:
  bool suppressChangeTracking = false;
  bool handlingTrackedInput = false;
  ComputerscareTextEditorSnapshot lastSnapshot;
  std::vector<ComputerscareTextEditorSnapshot> undoStack;
  std::vector<ComputerscareTextEditorSnapshot> redoStack;

  ComputerscareTextEditorSnapshot captureSnapshot() const;
  void restoreSnapshot(const ComputerscareTextEditorSnapshot& snapshot);
  void pushUndoSnapshot(const ComputerscareTextEditorSnapshot& snapshot);
  void clearHistory();
  void undo();
  void redo();
  std::shared_ptr<Font> loadEditorFont();
  void drawHighlightBackgrounds(const DrawArgs& args);
  void drawEditorText(const DrawArgs& args);
  void drawHighlightForegrounds(const DrawArgs& args);
  void drawHighlightDecorations(const DrawArgs& args);
  void drawCursor(const DrawArgs& args);
  void drawHighlightSpan(const DrawArgs& args,
                         const ComputerscareTextHighlight& highlight, int mode);
};
