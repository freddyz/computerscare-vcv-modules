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
			if(stackVec.size() > 0) {
			//push this evaluated string to new top
			stackVec.back().push_back(tempString);
			}
			else {
				return "";
			}
    }
    else {
			stackVec.back().push_back(c);
    }
  }
	std::vector<std::string> last = stackVec.back();
  output = interleaveExpand(last);
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
	while(outerLength && ((!allAtZero && steps < 6000 ) || steps == 0)) {
		if(lengths[outerIndex]) {
	  	output+=blocks[outerIndex][indices[outerIndex]];
			indices[outerIndex]++;
			indices[outerIndex]%=lengths[outerIndex];
		}
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

bool matchParens(std::string value) {
    std::string c="";
    int parensCount=0;
    int squareCount=0;
    int curlyCount=0;
    int angleCount=0;
    bool theyMatch=true;
    for(unsigned int i = 0; i < value.length(); i++) {
      c = value[i];
      if(c=="(") {
        parensCount+=1;
      }
      else if(c==")") {
        parensCount-=1;
      }
      if(c=="[") {
        squareCount+=1;
      }
      else if(c=="]") {
        squareCount-=1;
      }
      if(c=="{") {
        curlyCount+=1;
      }
      else if(c=="}") {
        curlyCount-=1;
      }
      if(c=="<") {
        angleCount+=1;
      }
      else if(c==">") {
        angleCount-=1;
      }
    }
    theyMatch = (parensCount==0) && (squareCount ==0) && (curlyCount==0) && (angleCount==0);
    return theyMatch;
  }
void whoKnows(std::string input) {
  //std::vector<Token> tStack = tokenizeString(input);
  //return evalToken("","Integer",tStack);
	AbsoluteSequence abs = AbsoluteSequence(input,knobandinputlookup);
	abs.print();
}

AbsoluteSequence::AbsoluteSequence(std::string expr, std::string lookup) {
	Parser p = Parser(expr);
	indexSequence = parseEntireString(expr,lookup,1);
}
void AbsoluteSequence::print() {
	printVector(indexSequence);
}
Token::Token(std::string t, std::string v) {
	type = t;
	val = v;
}
Parser::Parser(std::string expr) {
	setExpression(expr);
	//tokens = tokenizeString(expr);
}
void Parser::setExpression(std::string expr) {
	expression=expr;
	currentIndex=0;
	char c;
	while ((c = peekChar())) {
		switch (c) {
			default:
				if((c >= '0' && c <= '9') || c == '.') {
					printf("num:%s\n",parseNumber(c).c_str());
				}
				else {
					
				}
		}
		skipChar();
	}

}
char Parser::peekChar() {
	if (currentIndex < (int) expression.size()) return expression[currentIndex];
	return 0;
}
void Parser::skipChar() {
	currentIndex++;
	}
char Parser::skipAndPeekChar() {
	skipChar();
	return peekChar();
}
std::string Parser::parseNumber(char c)
{
    std::string number;
    if (c != '.')
    {
        // parse before '.'
        while (c != 0 && c >= '0' && c <= '9' && c != '.' ) {
            number += c;
            c = skipAndPeekChar();
        }
    }
    if (c == '.')
    {
        // parse after '.'
        number += c;
        c = skipAndPeekChar();
        if (c != 0 && c >= '0' && c <= '9') {
            while (c != 0 && c >= '0' && c <= '9' ) {
                number += c;
                c = skipAndPeekChar();
            }
        } else {
            printf("Expected digit after '.', number: %s\n",number.c_str());
        }
    }
    return number;
}
void Token::print() {
	printf("type:%s, val:%s\n",type.c_str(),val.c_str());
}
std::vector<Token> tokenizeString(std::string input) {
	std::vector<Token> stack;
	int stringIndex=0;
	for(unsigned int i = 0; i < input.length(); i++) {
		std::string token(1,input[i]);
    if(token=="(") stack.push_back(Token("LeftParen",token));
    else if(token== ")") stack.push_back(Token("RightParen",token));
    else if(token== "[") stack.push_back(Token("LeftSquare",token));
    else if(token== "]") stack.push_back(Token("RightSquare",token));
    else if(token== "{") stack.push_back(Token("LeftCurly",token));
    else if(token== "}") stack.push_back(Token("RightCurly",token));
    else if(token== "<") stack.push_back(Token("LeftAngle",token));
    else if(token== ">") stack.push_back(Token("RightAngle",token));
    else if(token== "@") stack.push_back(Token("At",token));
    else if(token== ",") stack.push_back(Token("Comma",token));
    else if(token== "+") stack.push_back(Token("Plus",token));
    else if(token== "-") stack.push_back(Token("Minus",token));
    else if(token== "*") stack.push_back(Token("Asterix",token));
    else if(token== "/") stack.push_back(Token("Backslash",token));
    else if(token== " ") stack.push_back(Token("Whitespace",token));
    else if(token== ".") stack.push_back(Token("Period",token));
    else if(token== "!") stack.push_back(Token("Bang",token));
    else if(token== "?") stack.push_back(Token("Question",token));
    else if(token== "#") stack.push_back(Token("Hash",token));
    else if(token== "^") stack.push_back(Token("Caret",token));
    else if(token== ":") stack.push_back(Token("Colon",token));
    else if(token== ";") stack.push_back(Token("Semicolon",token));
    else if(token== "|") stack.push_back(Token("Pipe",token));
    else if(knobandinputlookup.find(token) != -1) {
			stack.push_back(Token("Letter",token));
		}
    else if(integerlookup.find(token) != -1) {
			stack.push_back(Token("Integer",token));
		}
    else stack.push_back(Token("Unknown",token));
	}
	return stack;
}

std::string evalToken(std::string input,std::string type, std::vector<Token> tStack) {
  std::string output = input;
  Token peek = Token("Unknown","~");
  if(tStack.size()) {
    peek = tStack.front();
    if(type=="Integer") {
      tStack.erase(tStack.begin());
      if(peek.type=="Integer") output = evalToken(output+peek.val,"Integer",tStack);
    }
    else if(type=="Letter") {
      tStack.erase(tStack.begin());
      if(peek.type=="Letter") output = evalToken(output+peek.val,"Letter",tStack);
    }
    else if(type=="LeftCurly") {
      tStack.erase(tStack.begin());
      if(peek.type=="Letter") output = output + evalToken(output+peek.val,"rLetter",tStack);
      else if(peek.type=="Integer") output = output+evalToken(peek.val,"rInt",tStack);
    }
    else if(type=="rLetter") {
      //if(peek.type=="Letter") output = 
    }

  }
  return output;
}

