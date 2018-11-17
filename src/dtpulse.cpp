#include "dtpulse.hpp"

std::string b64lookup = "123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ&$0";
std::string integerlookup = "0123456789";
std::string knoblookup = "abcdefghijklmnopqrstuvwxyz";
std::string inputlookup= "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
std::string knobandinputlookup="abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

bool is_digits(const std::string &str)
{
    return str.find_first_not_of(integerlookup) == std::string::npos;
}
void padTo(std::string &str, const size_t num, const char paddingChar = ' ')
{
    if(num > str.size())
        str.insert(0, num - str.size(), paddingChar);
}
std::vector<int> parseStringAsTimes(std::string input, std::string lookup) {
// "113" -> {1,1,1,0,0}
  return parseEntireString(input,lookup,0);
}

std::vector<int> parseStringAsValues(std::string input, std::string lookup) {
  // "113" -> {1,1,3}
	return parseEntireString(input,lookup,1);
}

std::vector<int> parseEntireString(std::string input,std::string lookup,int type) {
        std::vector<int> absoluteSequence;
        absoluteSequence.resize(0);
				bool noNumbers = true;
				for(unsigned int i = 0; i < input.length(); i++) {
					noNumbers= noNumbers && (lookup.find(input[i]) == std::string::npos);
				}
        if(noNumbers) {
          absoluteSequence.push_back(0);
          return absoluteSequence;
        }
        std::vector<int> commaVec;
        std::vector<std::string> atVec;
        std::vector<std::string> offsetVec;

				std::string interleaved;
        std::string commaseg;
        std::string atseg;
        std::string offsetseg;
        std::string atlhs;
        std::string commalhs;
        
        int atnum=-1;
        int offsetnum=0;

        std::stringstream inputstream(input);
        std::stringstream atstream(input);
        std::stringstream offsetstream(input);
        while(std::getline(inputstream,commaseg,',')) {
          if(commaseg.empty()) {
            //absoluteSequence.push_back(0);
          }
          else {
              std::stringstream atstream(commaseg);
              atVec.resize(0);
              while(std::getline(atstream,atseg,'@')) {
                atVec.push_back(atseg);
              }
              atnum  = (atVec.size() > 1 && is_digits(atVec[1]) )? std::stoi(atVec[1]) : -1;
              if(atVec[0].empty() && atnum > 0) {
                for(int i = 0; i < atnum; i++) {
                  absoluteSequence.push_back(0);
                }
              }
              else if(atVec[0].empty() && atnum == -1) {
                absoluteSequence.push_back(0);
              }
							else if(atnum ==0) {
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
                  offsetnum  = (offsetVec.size() > 1  && is_digits(offsetVec[1]))? std::stoi(offsetVec[1]) : 0;
                  commaVec.resize(0);
									interleaved = splitRecur(offsetVec[0]); 
									// below may be the only line that has to change for a by value parse
                  if(type==0) {
                    commaVec = parseDt(atExpand(interleaved,atnum,lookup),offsetnum,lookup); 
                  }
                  else {
                    commaVec = parseLookup(countExpand(interleaved,atnum),offsetnum,lookup);
                  }
                  
                  absoluteSequence.insert(absoluteSequence.end(),commaVec.begin(),commaVec.end());
                }
              }
            }
          }
        return absoluteSequence;
}
std::vector<int> parseLookup(std::string input, int offset, std::string lookup) {
  std::vector<int> absoluteSequence;
  int currentVal;
	int mappedIndex=0;
	int length = input.length();
  absoluteSequence.resize(0);

  for(unsigned int i = 0; i < length; i++) {
		mappedIndex = (i + offset) % length;
    currentVal = lookup.find(input[mappedIndex]);
    absoluteSequence.push_back(currentVal);
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
std::string splitRecur(std::string input) {
	std::vector<std::vector<std::string>> stackVec;
  std::string tempString;
  std::string output;
  std::string c;
	stackVec.push_back({});
  for(unsigned int i = 0; i < input.length(); i++) {
    c = input[i];
    if(c == "(") {
			stackVec.push_back({});
    }
    else if(c == ")") {
			//evaluate top of stack
			tempString = interleaveExpand(stackVec.back()); 
			//pop top of stack
			stackVec.pop_back();
			//push this evaluated string to new top
			stackVec.back().push_back(tempString);
    }
    else {
			stackVec.back().push_back(c);
    }

  }
  output = interleaveExpand(stackVec[0]);
  return output;
}

std::string interleaveExpand(std::vector<std::string> blocks) {
  // take a vector of strings and return a string interleave
  // somewhat like bash shell expansion
	// ["a","b","cd"] --> "abcabd"
	// ["ab","cde"] ----> "acbdaebcadbe"
	std::vector<int> indices;
	std::vector<int> lengths;
	int outerIndex = 0;
	int outerLength = blocks.size();
	int steps = 0;
	bool allAtZero = false;
	std::string output="";
	for(int i = 0; i < outerLength; i++) {
		indices.push_back(0);
		lengths.push_back(blocks[i].length());
	}
	while((!allAtZero && steps < 6000 ) || steps == 0) {
	  output+=blocks[outerIndex][indices[outerIndex]];
		indices[outerIndex]++;
		indices[outerIndex]%=lengths[outerIndex];
		outerIndex++;
		outerIndex%=outerLength;
		steps++;
		allAtZero = outerIndex==0;

		for(int i = 0; i < outerLength; i++) {
			allAtZero &= (indices[i] == 0);	
		}
	}
	return output;
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

std::string countExpand(std::string input, int atnum) {
  std::string output="";
  int length = input.length();
  int index = 0;
  if(atnum == -1) {
    return input;
  }
  else if(atnum == 0) {
    return "";
  }
  for(index = 0; index < atnum; index++) {
    output += input[index % length];
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
std::string concatVectorFromLookup(std::vector<int> vector, std::string lookup) {
	std::string output="";
	for (int i = 0; i < vector.size(); i++){
			output+=lookup[vector[i]];
		}
	return output;
}
