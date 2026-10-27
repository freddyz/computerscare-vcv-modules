#include "ComputerscareTextEditorHistory.hpp"
#include "Blunch/ClockPhase.hpp"
#include "Blunch/ControlCv.hpp"
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
  {
    ComputerscareTextEditorHistory sessions;
    ComputerscareTextEditorHistory::Stack edits,redos;
    int first,second;sessions.switchState(&first,edits,redos);
    ComputerscareTextEditorSnapshot large;large.text=std::string(20000,'a');edits.push_back(large);
    const char* buffer=edits.back().text.data();
    for(int turn=0;turn<100;++turn){
      sessions.switchState(&second,edits,redos);assert(edits.empty());
      sessions.switchState(&first,edits,redos);
      assert(edits.size()==1&&edits.back().text.data()==buffer);
    }
  }
  computerscare::blunch::ClockPhase phase;
  phase.process(.1,true);assert(phase.phase()==0);
  phase.process(.5,true);phase.process(.25,false);assert(phase.phase()==.5f);
  phase.process(.25,true);assert(phase.phase()==0);
  phase.process(1,false);assert(phase.phase()==1);
  assert(computerscare::blunch::timeScaleWithCv(0,1)==2);
  assert(computerscare::blunch::timeScaleWithCv(0,-1)==.5f);
  assert(computerscare::blunch::timeScaleWithCv(5,10)==32);
  assert(!computerscare::blunch::usesSeed(computerscare::blunch::seedWithCv(2,-3)));
  assert(computerscare::blunch::usesSeed(computerscare::blunch::seedWithCv(-2,3)));
  assert(!computerscare::blunch::usesSeed(0));
  std::puts("text editor undo and clock phase tests passed");
}
