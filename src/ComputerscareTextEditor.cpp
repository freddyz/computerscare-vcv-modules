#include "ComputerscareTextEditor.hpp"

#include <algorithm>

#include "ComputerscareTextEditorLogic.hpp"

ComputerscareTextEditor::ComputerscareTextEditor() {
  multiline = true;
  placeholder = "";
  lastSnapshot = captureSnapshot();
}

void ComputerscareTextEditor::setState(
    ComputerscareTextEditorState* editorState) {
  state = editorState;
  syncFromState();
}

void ComputerscareTextEditor::syncFromState() {
  if (!state) {
    return;
  }

  suppressChangeTracking = true;
  setText(state->text);
  suppressChangeTracking = false;
  cursor = std::max(0, std::min(state->cursor, (int)text.size()));
  selection = std::max(0, std::min(state->selection, (int)text.size()));
  state->cursor = cursor;
  state->selection = selection;
  state->dirty = false;
  clearHistory();
  lastSnapshot = captureSnapshot();
}

void ComputerscareTextEditor::step() {
  ui::TextField::step();
  if (state && state->dirty) {
    syncFromState();
  } else if (state) {
    state->cursor = cursor;
    state->selection = selection;
  }
}

void ComputerscareTextEditor::onChange(const ChangeEvent& e) {
  if (!suppressChangeTracking && !handlingTrackedInput &&
      text != lastSnapshot.text) {
    pushUndoSnapshot(lastSnapshot);
    redoStack.clear();
  }
  if (state) {
    state->text = text;
    state->cursor = cursor;
    state->selection = selection;
  }
  lastSnapshot = captureSnapshot();
}

int ComputerscareTextEditor::getCursorLine() const {
  return computerscare::text_editor::lineForOffset(text, cursor);
}

void ComputerscareTextEditor::setCursorLine(int line) {
  setCursorLineEdge(line, false);
}

void ComputerscareTextEditor::setCursorLineEdge(int line, bool end) {
  cursor = computerscare::text_editor::lineStartPosition(text, line);
  if (end) {
    cursor = computerscare::text_editor::lineEndPosition(text, line);
  }
  selection = cursor;
  if (state) {
    state->cursor = cursor;
    state->selection = selection;
  }
  lastSnapshot = captureSnapshot();
}

ComputerscareTextEditorSnapshot ComputerscareTextEditor::captureSnapshot()
    const {
  ComputerscareTextEditorSnapshot snapshot;
  snapshot.text = text;
  snapshot.cursor = cursor;
  snapshot.selection = selection;
  return snapshot;
}

void ComputerscareTextEditor::restoreSnapshot(
    const ComputerscareTextEditorSnapshot& snapshot) {
  suppressChangeTracking = true;
  text = snapshot.text;
  cursor = std::max(0, std::min(snapshot.cursor, (int)text.size()));
  selection = std::max(0, std::min(snapshot.selection, (int)text.size()));
  if (state) {
    state->text = text;
    state->cursor = cursor;
    state->selection = selection;
  }
  suppressChangeTracking = false;
  lastSnapshot = captureSnapshot();
}

void ComputerscareTextEditor::pushUndoSnapshot(
    const ComputerscareTextEditorSnapshot& snapshot) {
  if (!undoStack.empty() && undoStack.back().text == snapshot.text &&
      undoStack.back().cursor == snapshot.cursor &&
      undoStack.back().selection == snapshot.selection) {
    return;
  }
  undoStack.push_back(snapshot);
  if (undoStack.size() > maxUndoDepth) {
    undoStack.erase(undoStack.begin());
  }
}

void ComputerscareTextEditor::clearHistory() {
  undoStack.clear();
  redoStack.clear();
}

void ComputerscareTextEditor::undo() {
  if (undoStack.empty()) {
    return;
  }
  redoStack.push_back(captureSnapshot());
  ComputerscareTextEditorSnapshot snapshot = undoStack.back();
  undoStack.pop_back();
  restoreSnapshot(snapshot);
}

void ComputerscareTextEditor::redo() {
  if (redoStack.empty()) {
    return;
  }
  undoStack.push_back(captureSnapshot());
  ComputerscareTextEditorSnapshot snapshot = redoStack.back();
  redoStack.pop_back();
  restoreSnapshot(snapshot);
}

int ComputerscareTextEditor::getLineStartPosition(int line) const {
  return computerscare::text_editor::lineStartPosition(text, line);
}

int ComputerscareTextEditor::getLineEndPosition(int line) const {
  return computerscare::text_editor::lineEndPosition(text, line);
}

int ComputerscareTextEditor::getCursorColumn() const {
  int line = getCursorLine();
  int lineStart = getLineStartPosition(line);
  int cursorPosition = std::max(0, std::min(cursor, (int)text.size()));
  return std::max(0, cursorPosition - lineStart);
}
