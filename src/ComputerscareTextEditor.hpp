#pragma once

#include <vector>

#include "Computerscare.hpp"
#include "ComputerscareTextEditorTypes.hpp"

struct ComputerscareTextEditor : ui::TextField {
  ComputerscareTextEditorState* state = nullptr;
  ComputerscareTextEditorCommands commands;
  ComputerscareTextEditorStyle style;
  size_t maxUndoDepth = 128;
  bool submitOnEnter = false;
  bool openOnEnter = false;
  bool stopOnSemicolon = false;
  bool readOnly = false;

  ComputerscareTextEditor();

  void setState(ComputerscareTextEditorState* editorState);
  void syncFromState();
  void step() override;
  void draw(const DrawArgs& args) override;
  void drawLayer(const DrawArgs& args, int layer) override;
  void onEnter(const EnterEvent& e) override;
  void onLeave(const LeaveEvent& e) override;
  void onButton(const ButtonEvent& e) override;
  void onDoubleClick(const DoubleClickEvent& e) override;
  void onDragHover(const DragHoverEvent& e) override;
  void onDragEnd(const DragEndEvent& e) override;
  void onSelectText(const SelectTextEvent& e) override;
  void onSelectKey(const SelectKeyEvent& e) override;
  void onChange(const ChangeEvent& e) override;
  int getTextPosition(Vec mousePos) override;
  int getCursorLine() const;
  void setCursorLine(int line);
  void setCursorLineEdge(int line, bool end);

 protected:
  bool suppressChangeTracking = false;
  bool handlingTrackedInput = false;
  bool lockingWordSelection = false;
  Vec lastPrimaryClickPos = Vec(0.f, 0.f);
  ComputerscareTextEditorSnapshot lastSnapshot;
  std::vector<ComputerscareTextEditorSnapshot> undoStack;
  std::vector<ComputerscareTextEditorSnapshot> redoStack;

  ComputerscareTextEditorSnapshot captureSnapshot() const;
  void restoreSnapshot(const ComputerscareTextEditorSnapshot& snapshot);
  void pushUndoSnapshot(const ComputerscareTextEditorSnapshot& snapshot);
  void clearHistory();
  void undo();
  void redo();
  void selectWordAtPosition(int position);
  int getLineStartPosition(int line) const;
  int getLineEndPosition(int line) const;
  int getCursorColumn() const;
  void moveCursorToAdjacentLogicalLine(int direction, bool extendSelection);
  std::shared_ptr<Font> loadEditorFont();
  float getFontScaleX() const;
  float getFontScaleY() const;
  Vec getScaledBoxSize() const;
  void applyTextStyle(NVGcontext* vg, std::shared_ptr<Font> font);
  void drawHighlightBackgrounds(const DrawArgs& args);
  void drawEditorText(const DrawArgs& args);
  void drawHighlightForegrounds(const DrawArgs& args);
  void drawHighlightDecorations(const DrawArgs& args);
  void drawCursor(const DrawArgs& args);
  void drawHighlightSpan(const DrawArgs& args,
                         const ComputerscareTextHighlight& highlight, int mode);
};
