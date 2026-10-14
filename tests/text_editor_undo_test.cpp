#include "ComputerscareTextEditorHistory.hpp"
#include "Blunch/ClockPhase.hpp"
#include <cassert>
#include <cstdio>
int main() {
  ComputerscareTextEditorHistory history;
  ComputerscareTextEditorHistory::Stack undo,redo;
  int a,b;
  ComputerscareTextEditorSnapshot initial,edit,other,restored;
  initial.text="initial";edit.text="edit";other.text="other";
  history.switchState(&a,undo,redo);undo.push_back(initial);
  assert(!history.switchState(&a,undo,redo)&&undo.size()==1);
  assert(ComputerscareTextEditorHistory::move(undo,redo,edit,restored)&&restored.text=="initial");
  assert(ComputerscareTextEditorHistory::move(redo,undo,initial,restored)&&restored.text=="edit");
  history.switchState(&b,undo,redo);assert(undo.empty());undo.push_back(other);
  history.switchState(&a,undo,redo);
  assert(ComputerscareTextEditorHistory::move(undo,redo,edit,restored)&&restored.text=="initial");
  history.switchState(&b,undo,redo);
  assert(undo.size()==1&&undo.back().text=="other");
  computerscare::blunch::ClockPhase phase;
  phase.process(.1,true);assert(phase.phase()==0);
  phase.process(.5,true);phase.process(.25,false);assert(phase.phase()==.5f);
  phase.process(.25,true);assert(phase.phase()==0);
  phase.process(1,false);assert(phase.phase()==1);
  std::puts("text editor undo and clock phase tests passed");
}
