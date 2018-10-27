#include "dtpulse.hpp"
std::string b64lookup = "123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ&$0";

std::vector<int> parseEntireString(std::string input,std::string lookup) {
        std::vector<int> absoluteSequence;
        absoluteSequence.resize(0);
        if(input.empty()) {
          absoluteSequence.push_back(0);
          return absoluteSequence;
        }
        std::vector<int> commaVec;
        std::vector<std::string> atVec;
        std::vector<std::string> offsetVec;

        std::string commaseg;
        std::string atseg;
        std::string offsetseg;
        std::string atlhs;
        std::string commalhs;
        
        int atnum;
        int offsetnum;

        std::stringstream inputstream(input);
        std::stringstream atstream(input);
        std::stringstream offsetstream(input);
        while(std::getline(inputstream,commaseg,',')) {
          if(commaseg.empty()) {
            absoluteSequence.push_back(0);
          }
          else {
              std::stringstream atstream(commaseg);
              atVec.resize(0);
              while(std::getline(atstream,atseg,'@')) {
                atVec.push_back(atseg);
              }
              atnum  = atVec.size() > 1 ? std::stoi(atVec[1]) : -1;
              if(atVec[0].empty() && atnum > 0) {
                for(int i = 0; i < atnum; i++) {
                  absoluteSequence.push_back(0);
                }
              }
              else if(atVec[0].empty() && atnum == -1) {
                absoluteSequence.push_back(0);
              }
              else {
                std::stringstream offsetstream(atVec[0]);
                offsetVec.resize(0);
                while(std::getline(offsetstream,offsetseg,'-')) {
                  offsetVec.push_back(offsetseg);
                }
                if(offsetVec[0].empty()) {
                  absoluteSequence.push_back(0);
                }
                else {
                  offsetnum  = offsetVec.size() > 1 ? std::stoi(offsetVec[1]) : 0;
                  commaVec.resize(0);
                  commaVec = parseDt(atExpand(offsetVec[0],atnum,lookup),offsetnum,lookup); 
                  absoluteSequence.insert(absoluteSequence.end(),commaVec.begin(),commaVec.end());
                }
              }
            }
          }
        return absoluteSequence;
}
std::vector<int> parseDt(std::string input, int offset, std::string lookup) {
  std::vector <int> absoluteSequence;
  
  std::vector <int> sequenceSums;
  absoluteSequence.resize(0);
  sequenceSums.push_back(0);
    int numSteps = 0;
    int mappedIndex = 0;
    int currentVal = 0;
    
    for(unsigned int i = 0; i < input.length(); i++) {
      currentVal = lookup.find(input[i]) + 1;
        if (currentVal != 0) {
            numSteps += currentVal;
            sequenceSums.push_back(numSteps);
        }
    }

    absoluteSequence.resize(numSteps);
    for(unsigned i = 0; i < sequenceSums.size() - 1; i++) {
      mappedIndex = (sequenceSums[i] + offset ) % numSteps;
      absoluteSequence[mappedIndex] = 1;
    }
    return absoluteSequence;
}
std::string atExpand(std::string input, int atnum, std::string lookup) {
  std::string output="";
  int length = input.length();
  int total = 0;
  int index = 0;
  int lookupVal;
  if(atnum == -1) {
    return input;
  }
  else if(atnum ==0) {
    return "";
  }
  while(total < atnum) {
    lookupVal = b64lookup.find(input[index]) + 1;
    lookupVal = lookupVal == 0 ? 1 : lookupVal;
    if(total + lookupVal <= atnum) {
      output += lookup[lookupVal-1];
      total += lookupVal;
    }
    else {
      output += b64lookup[atnum-total - 1];
      total = atnum;
    }
    index++;
    index = index%length;
  }
  return output;
}

std::string hashExpand(std::string input, int hashnum) {
  std::string output="";
  int length = input.length();
  for(int i = 0; i < hashnum; i++) {
    for(int j = 0; j< length; j++) {
      output += input[j];
    }
  }
  return output;
}