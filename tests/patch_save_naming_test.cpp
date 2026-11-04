#include "PatchSaveNaming.hpp"
#include <cstdio>
#include <cstdlib>
#include <set>
using namespace computerscare::patch_saves;
void require(bool ok, const char* message) {
  if(!ok){std::fprintf(stderr,"%s\n",message);std::exit(1);}
}
int main() {
  require(nextFilename("patchxyz",{})=="patchxyz1.vcv","Append a number to an unnumbered patch");
  require(nextFilename("test1",{})=="test2.vcv","Increment a trailing number without stripping the folder name");
  require(nextFilename("horse2-pig",{})=="horse2-pig1.vcv","Numbered saves retain the branch suffix");
  require(nextFilename("horse2-pig1",{})=="horse2-pig2.vcv","Increment the outermost trailing number only");
  require(nextFilename("horse2-pig",{"horse2-pig1.vcv","horse2-pig3.vcv","horse9.vcv","horse2-fox99.vcv"})=="horse2-pig4.vcv","Use highest sibling number plus one, not gaps or another branch");
  require(nextFilename("123",{})=="124.vcv","Entirely numeric filenames increment normally");
  require(nextFilename("take009",{})=="take010.vcv","Preserve leading-zero width");
  require(nextFilename("x99999999999999999999",{})=="x100000000000000000000.vcv","Large numeric suffixes cannot overflow");
  require(branchFilename("horse2-pig1",0)=="horse2-pig1-pig.vcv","Branches append to the full current name");
  const unsigned count=unsigned(words().size());
  std::set<std::string> names;
  for(unsigned i=0;i<count;++i){const char* word=words()[i];require(word&&std::string(word).size()<=6,"Branch words are present and short");names.insert(word);}
  require(names.size()==150,"Exactly 150 unique branch words");
  require(branchFilename("horse",unsigned(words().size()))=="horse-pigpig.vcv","Exhausted word set concatenates words instead of numbers");
  require(branchBase("patch2-pig")=="patch2"&&branchBase("patch2-horseflax")=="patch2","Root branches recognize single and concatenated word suffixes");
  require(branchBase("patch2-pig1")=="patch2-pig1"&&branchBase("my-project")=="my-project","Numbered versions and unknown suffixes remain literal");
  require(branchFilename("patch2-pig",1)=="patch2-shrew.vcv","Root branch saves create siblings");
  BranchCandidates outside("patch2-pig",{},0);require(outside.next()=="patch2-shrew.vcv","An outside root is not selected again inside its new folder");
  BranchCandidates folded("patch2",{"PATCH2-PIG.VCV"},0);require(folded.next()=="patch2-shrew.vcv","Filename index respects ASCII case conflicts");
  std::vector<std::string> occupied;
  for(unsigned i=0;i<count;++i)occupied.push_back(branchFilename("patch2",i));
  BranchCandidates allWords("patch2-pig",occupied,0);require(allWords.next()=="patch2-pigpig.vcv","All occupied single words select a two-word name");
  for(unsigned i=count;i<count+count*count;++i)occupied.push_back(branchFilename("patch2",i));
  require(!names.count("clam")&&names.count("horse")&&names.count("goat"),"Removed word is not generated and animal replacements are present");
  require(branchBase("patch2-clam")=="patch2","Old removed-word filenames still create siblings");
  BranchCandidates allPairs("patch2",occupied,0);require(allPairs.next()=="patch2-pigpigpig.vcv","Exhausted pairs select a three-word name without disk probes");
  std::set<std::string> branches;
  for(unsigned i=0;i<300;++i)branches.insert(branchFilename("horse",i,73));
  require(branches.size()==300,"Branch retries do not repeat filenames");
  std::puts("Patch save names, branch preservation, collisions, numeric suffixes and 150-word list passed");
}
