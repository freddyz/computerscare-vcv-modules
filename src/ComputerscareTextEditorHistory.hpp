#pragma once
#include <map>
#include <string>
#include <vector>
struct ComputerscareTextEditorSnapshot {
  std::string text;
  int cursor = 0, selection = 0;
};
struct ComputerscareTextEditorHistory {
  using Stack = std::vector<ComputerscareTextEditorSnapshot>;
  struct History {
    Stack undo, redo;
  };
  std::map<void*, History> views;
  void* bound = nullptr;
  bool switchState(void* state, Stack& undo, Stack& redo) {
    if (state == bound) return false;
    if (bound) {
      auto& previous = views[bound];
      previous.undo.swap(undo);
      previous.redo.swap(redo);
    }
    bound = state;
    auto found = views.find(state);
    if (found == views.end()) {
      undo.clear();
      redo.clear();
    } else {
      undo.swap(found->second.undo);
      redo.swap(found->second.redo);
    }
    return true;
  }
  static bool move(Stack& from, Stack& to,
                   const ComputerscareTextEditorSnapshot& current,
                   ComputerscareTextEditorSnapshot& restored) {
    if (from.empty()) return false;
    to.push_back(current);
    restored = from.back();
    from.pop_back();
    return true;
  }
};
