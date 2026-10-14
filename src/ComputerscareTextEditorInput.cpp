#include <algorithm>

#include "Blunch/BlunchKeyboardShortcuts.hpp"
#include "ComputerscareTextEditor.hpp"
#include "ComputerscareTextEditorLogic.hpp"

void ComputerscareTextEditor::onEnter(const EnterEvent& e) {
  ui::TextField::onEnter(e);
  static GLFWcursor* editorCursor = nullptr;
  if (!editorCursor) {
    editorCursor = glfwCreateStandardCursor(GLFW_IBEAM_CURSOR);
  }
  glfwSetCursor(APP->window->win, editorCursor);
}

void ComputerscareTextEditor::onLeave(const LeaveEvent& e) {
  ui::TextField::onLeave(e);
  glfwSetCursor(APP->window->win, nullptr);
}

void ComputerscareTextEditor::onButton(const ButtonEvent& e) {
  if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
    APP->event->setSelectedWidget(this);
    int position =
        std::max(0, std::min(getTextPosition(e.pos), (int)text.size()));
    lockingWordSelection = false;
    cursor = selection = position;
    lastPrimaryClickPos = e.pos;
    if (state) {
      state->cursor = cursor;
      state->selection = selection;
    }
    e.consume(this);
    lastSnapshot = captureSnapshot();
    return;
  }
  ui::TextField::onButton(e);
  lastSnapshot = captureSnapshot();
}

void ComputerscareTextEditor::onDoubleClick(const DoubleClickEvent& e) {
  selectWordAtPosition(std::max(
      0, std::min(getTextPosition(lastPrimaryClickPos), (int)text.size())));
  lockingWordSelection = true;
  if (state) {
    state->cursor = cursor;
    state->selection = selection;
  }
  lastSnapshot = captureSnapshot();
  e.consume(this);
}

void ComputerscareTextEditor::onDragHover(const DragHoverEvent& e) {
  if (lockingWordSelection && e.origin == this) {
    e.consume(this);
    return;
  }
  ui::TextField::onDragHover(e);
}

void ComputerscareTextEditor::onDragEnd(const DragEndEvent& e) {
  lockingWordSelection = false;
  ui::TextField::onDragEnd(e);
}

void ComputerscareTextEditor::onSelectText(const SelectTextEvent& e) {
  if (readOnly) {
    e.consume(this);
    return;
  }

  ComputerscareTextEditorSnapshot before = captureSnapshot();
  handlingTrackedInput = true;
  ui::TextField::onSelectText(e);
  handlingTrackedInput = false;
  if (text != before.text) {
    pushUndoSnapshot(before);
    redoStack.clear();
  }
  lastSnapshot = captureSnapshot();
}

void ComputerscareTextEditor::onSelectKey(const SelectKeyEvent& e) {
  if (e.action == GLFW_PRESS || e.action == GLFW_REPEAT) {
    int mods = e.mods & RACK_MOD_MASK;
    computerscare::blunch::KeyboardShortcutOptions shortcutOptions;
    shortcutOptions.submitOnEnter = submitOnEnter;
    shortcutOptions.openOnEnter = openOnEnter;
    shortcutOptions.stopShortcutEnabled = stopShortcutEnabled;
    if (computerscare::blunch::handleKeyboardShortcut(
            e.key, e.keyName, e.mods, e.action, shortcutOptions, commands)) {
      e.consume(this);
      return;
    }
    if (e.key == GLFW_KEY_UP) {
      moveCursorToAdjacentLogicalLine(-1, (e.mods & GLFW_MOD_SHIFT) != 0);
      if (state) {
        state->cursor = cursor;
        state->selection = selection;
      }
      e.consume(this);
      return;
    }
    if (e.key == GLFW_KEY_DOWN) {
      moveCursorToAdjacentLogicalLine(1, (e.mods & GLFW_MOD_SHIFT) != 0);
      if (state) {
        state->cursor = cursor;
        state->selection = selection;
      }
      e.consume(this);
      return;
    }
    if ((e.key == GLFW_KEY_LEFT || e.key == GLFW_KEY_RIGHT) &&
        (e.mods & (GLFW_MOD_CONTROL | GLFW_MOD_SUPER | GLFW_MOD_ALT)) == 0) {
      int nextCursor =
          e.key == GLFW_KEY_LEFT
              ? computerscare::text_editor::moveOffsetLeft(text, cursor)
              : computerscare::text_editor::moveOffsetRight(text, cursor);
      cursor = nextCursor;
      if ((e.mods & GLFW_MOD_SHIFT) == 0) {
        selection = cursor;
      }
      if (state) {
        state->cursor = cursor;
        state->selection = selection;
      }
      lastSnapshot = captureSnapshot();
      e.consume(this);
      return;
    }
    if (readOnly) {
      e.consume(this);
      return;
    }
    bool isZ = e.key == GLFW_KEY_Z || e.keyName == "z";
    bool isY = e.key == GLFW_KEY_Y || e.keyName == "y";
    if ((isZ && mods == (RACK_MOD_CTRL | GLFW_MOD_SHIFT)) ||
        (isY && mods == RACK_MOD_CTRL)) {
      redo();
      e.consume(this);
      return;
    }
    if (isZ && mods == RACK_MOD_CTRL) {
      undo();
      e.consume(this);
      return;
    }
  }

  ComputerscareTextEditorSnapshot before = captureSnapshot();
  handlingTrackedInput = true;
  ui::TextField::onSelectKey(e);
  handlingTrackedInput = false;
  if (text != before.text) {
    pushUndoSnapshot(before);
    redoStack.clear();
  }
  lastSnapshot = captureSnapshot();
}

void ComputerscareTextEditor::selectWordAtPosition(int position) {
  computerscare::text_editor::SelectionRange range =
      computerscare::text_editor::wordRangeAtOffset(text, position);
  selection = range.begin;
  cursor = range.end;
}

void ComputerscareTextEditor::moveCursorToAdjacentLogicalLine(
    int direction, bool extendSelection) {
  int currentLine = getCursorLine();
  int targetLine = std::max(0, currentLine + direction);
  int targetStart = getLineStartPosition(targetLine);
  int targetEnd = getLineEndPosition(targetLine);
  int column = getCursorColumn();

  cursor = std::max(targetStart, std::min(targetStart + column, targetEnd));
  if (!extendSelection) {
    selection = cursor;
  }
  lastSnapshot = captureSnapshot();
}
