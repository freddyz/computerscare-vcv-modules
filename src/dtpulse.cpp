#include "dtpulse.hpp"

std::string b64lookup = "123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ&$0";
std::string integerlookup = "0123456789";
std::string knoblookup = "abcdefghijklmnopqrstuvwxyz";
std::string inputlookup= "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
std::string knobandinputlookup="abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
bool matchesAny(std::string val, std::vector<std::string> whitelist) {
	bool result = false;
	std::vector<std::string>::iterator it;
				for(it = whitelist.begin(); it != whitelist.end(); it++ )    {
					result = result || (*it == val);
				}
	return result;		
}
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

  for(int i = 0; i < length; i++) {
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
std::vector<Token> interleaveExpand(std::vector<std::vector<Token>> blocks) {
  // take a vector of strings and return a string interleave
  // somewhat like bash shell expansion
	// ["a","b","cd"] --> "abcabd"
	// ["ab","cde"] ----> "acbdaebcadbe"
	std::vector<Token> output;
	std::vector<int> indices;
	std::vector<int> lengths;
	int outerIndex = 0;
	int outerLength = blocks.size();
	int steps = 0;
	bool allAtZero = false;
	for(int i = 0; i < outerLength; i++) {
		indices.push_back(0);
		lengths.push_back(blocks[i].size());
	}
	while(outerLength && ((!allAtZero && steps < 6000 ) || steps == 0)) {
		if(lengths[outerIndex]) {
	  	output.push_back(blocks[outerIndex][indices[outerIndex]]);
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
	for (unsigned int i = 0; i < vector.size(); i++){
			output+=lookup[vector[i]];
		}
	return output;
}
void printFloatVector(std::vector<float> floatVector) {
	for(unsigned int i = 0; i < floatVector.size(); i++) {
		printf("floatVector[%i]: %f\n",i,floatVector[i]);
	}
}
void printTokenVector(std::vector<std::vector<Token>> tokenVector) {
  for(unsigned int i = 0; i < tokenVector.size(); i++) {
    printf("tokenVector[%i]: ",i);
    for(unsigned int j = 0; j < tokenVector[i].size(); j++) {
      printf("%i ",tokenVector[i][j].index);
    }
    printf("\n");
    
  }
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
	AbsoluteSequence abs = AbsoluteSequence(input,knobandinputlookup);
	abs.print();
  printf("  indexSequence:\n");
  printVector(abs.indexSequence);
  printf("  workingIndexSequence:\n");
  printVector(abs.workingIndexSequence);
  srand (time(NULL));
	printf("  iteration:\n");
  for(int j = 0; j < 13; j++) {
    abs.incrementAndCheck();
    printVector(abs.workingIndexSequence);
  }
}
void whoKnowsLaundry(std::string input) {
	LaundrySoupSequence laundry = LaundrySoupSequence(input);
 
  laundry.print();
	printf("  iteration:\n");
  for(int j = 0; j < 13; j++) {
    laundry.incrementAndCheck();
    printVector(laundry.workingPulseSequence);
  }

}
LaundrySoupSequence::LaundrySoupSequence() {
	LaundrySoupSequence("");
}
LaundrySoupSequence::LaundrySoupSequence(std::string expr) {
	std::vector<Token> defaultStack;
  defaultStack.push_back(Token("Error", "error",-1));
	if(expr != "") {
		Parser p = Parser(expr);
		p.setForLaundry();
  	if(p.inError || !p.tokenStack.size()) { 
   	 	tokenStack = defaultStack;
      inError=true;
  	}
		else {
  		tokenStack = p.tokenStack;
      inError=false;
		}
	}
	else {
		tokenStack = defaultStack;
    inError=false;
	}
	pulseSequence = makePulseSequence(tokenStack);
	workingPulseSequence = duplicateIntVector(pulseSequence);
	numSteps = (int) pulseSequence.size();
	readHead = -1;
}
void LaundrySoupSequence::print() {
  printf("  Laundry tokenStack:\n");
  printTokenVector(tokenStack);
  printf("  Laundry pulseSequence:\n");
  printVector(pulseSequence);
}
std::vector<int> LaundrySoupSequence::makePulseSequence(std::vector<Token> tokens) {
	std::vector<int> output = {};
	int thisVal;
	int thisGate;
	for(unsigned int i = 0; i < tokens.size(); i++) {
		thisVal = tokens[i].duration;
		thisGate = (tokens[i].type=="ChanceOfInteger" ? 2 : (tokens[i].value=="0" ? 0 : 1)); 
		output.push_back(thisGate);
		for(int j = 1; j<thisVal; j++) {
			output.push_back(0);	
		}
	}
	return output;
}
void LaundrySoupSequence::skipStep() {
  readHead++;
  readHead %= numSteps;
}
int LaundrySoupSequence::skipAndPeek() {
  skipStep();
  return peekStep();
}
int LaundrySoupSequence::peekStep() {
  return pulseSequence[readHead];
}
int LaundrySoupSequence::peekWorkingStep() {
	return (readHead >=0) ? workingPulseSequence[readHead] : 0;
}
void LaundrySoupSequence::incrementAndCheck() {
	if(skipAndPeek() == 2) {
    randomizePulseValue(readHead);
	}
}
bool LaundrySoupSequence::atLastStep() {
  return (readHead == (numSteps -1 ));
}
void LaundrySoupSequence::randomizePulseValue(int index) {
	workingPulseSequence[index] = (rand() % 2);
}
AbsoluteSequence::AbsoluteSequence() {
  AbsoluteSequence("a",knobandinputlookup);
}
AbsoluteSequence::AbsoluteSequence(std::string expr, std::string lookup) {
	std::vector<Token> defaultStack;
  defaultStack.push_back(Token("Error", "error",-1));
   srand (time(NULL));
  //expr = expr=="" ? "a" : expr;
	if(expr != "") {
		Parser p = Parser(expr);
		p.setForCookies();
  	exactFloats = p.exactFloats;
  	randomTokens=p.randomVector;
  	if(p.inError || !p.tokenStack.size()) { 
   	 	tokenStack = defaultStack;
      inError = true;
  	}
		else {
  		tokenStack = p.tokenStack;
      inError=false;
		}
	}
	else {
		tokenStack = defaultStack;
    inError=false;
	}
  numTokens = tokenStack.size();
	indexSequence = getIndicesFromTokenStack(tokenStack);
	workingIndexSequence = duplicateIntVector(indexSequence);
  readHead = -1;//((int)indexSequence.size())-1 ;
}
void AbsoluteSequence::randomizeIndex(int index) {
	int randomTokenIndex = indexSequence[index] - 78;
	std::vector<int> myRandomTokens = getIndicesFromTokenStack(randomTokens[randomTokenIndex]);
	int size = myRandomTokens.size();
  if(size) {
    //random one from those enclosed by the {}
    workingIndexSequence[index] = myRandomTokens[rand() % (myRandomTokens.size())];
  }
  else {
     //random address ie: a-z,A-Z
    workingIndexSequence[index] = rand() % 26;
  }
}
void AbsoluteSequence::skipStep() {
  readHead++;
  readHead %= indexSequence.size();
}
int AbsoluteSequence::skipAndPeek() {
  skipStep();
  return peekStep();
}
int AbsoluteSequence::peekStep() {
  return indexSequence[readHead];
}
int AbsoluteSequence::peekWorkingStep() {
  return (readHead >=0) ? workingIndexSequence[readHead] : 0;
}
void AbsoluteSequence::incrementAndCheck() {
  if(skipAndPeek()>=78) {
    randomizeIndex(readHead);
  }
}
std::string AbsoluteSequence::getWorkingStepDisplay() {
  int stepIndex = peekWorkingStep();
  if(stepIndex < 0) {
    return "?";
  }
  else if(stepIndex < 52) {
    std::string str(1,knobandinputlookup[stepIndex]);
    return str;
  }
  else {
		return std::to_string((long double) exactFloats[stepIndex - 52]).substr(0,4);
  }
}

std::vector<int> getIndicesFromTokenStack(std::vector<Token> tokens) {
	std::vector<int> output;
	for(unsigned int i = 0; i < tokens.size(); i++) {
		output.push_back(tokens[i].index);
	}
	return output;
}
std::vector<int> duplicateIntVector(std::vector<int> input) {
	std::vector<int> output;
	for(unsigned int i = 0; i < input.size(); i++) {
		output.push_back(input[i]);
	}
	return output;
}
void printTokenVector(std::vector<Token> tokenVector) {

  for(unsigned int i = 0; i < tokenVector.size(); i++) {
    tokenVector[i].print();
  }
}
void AbsoluteSequence::print() {
	printFloatVector(exactFloats);
  printTokenVector(randomTokens);
	printf("  stack:\n");
	printTokenVector(tokenStack);
}
Token::Token(std::string t, std::string v) {
	type = t;
	value = v;
	index = -1;
	duration =1;
}
Token::Token(std::string t, std::string v, int dex) {
  type = t;
  value = v;
  index = dex;
	duration=1;
}
Token::Token(std::string t, std::string v, int dex, int dur) {
  type = t;
  value = v;
  index = dex;
	duration = dur;
}

Parser::Parser(std::string expr) {
	tokens = tokenizeString(expr);
	expression=expr;
  inError = false;
}
Parser::Parser() {
	Parser("");
}
void Parser::setForLaundry() {
	//whitelists
  std::vector<std::string> laundryInterleaveAny = {"Letter","Integer","ChanceOfInteger","Digit","LeftParen","RightParen"};
  std::vector<std::string> laundryAtExpandAny = {"Letter","Digit","ChanceOfInteger","Integer"};
  std::vector<std::string> laundrySquareAny = {"Letter","Digit","ChanceOfInteger","Integer", "Comma"};
  std::vector<std::string> laundryFinalAny = {"Letter","Digit","ChanceOfInteger","Integer"};
 
  if(tokens.size() > 0) {
		currentIndex=0;
		replaceLettersWithNumbers(tokens[0]);
		currentIndex=0;
		tokens=tokenStack;
		tokenStack={};

    setForExactIntegers(tokens[0]);
    if(!inError) {
      currentIndex=0;
      tokens=tokenStack;
			tokenStack = {};
			setForChanceOfIntegers(peekToken());
      if(!inError) {
        currentIndex=0;
        tokens=tokenStack;
				tokenStack = {};
      	setForInterleave(peekToken(),laundryInterleaveAny);
        if(!inError) {
          currentIndex=0;
          tokens=tokenStack;
          tokenStack = {};
          setForAtExpand(peekToken(),laundryAtExpandAny, true);

          if(!inError) {
              currentIndex = 0;
              tokens=tokenStack;
              tokenStack = {};
              setForSquareBrackets(peekToken(),laundrySquareAny,true);

              if(!inError) {
                currentIndex = 0;
                tokens=tokenStack;
                tokenStack = {};
                setFinal(peekToken(),laundryFinalAny);
              }
            }
        }
      }
		}
	}
}
void Parser::setForCookies() {

	std::vector<std::string> interleaveAny = {"Letter","ExactValue","RandomSequence","LeftParen","RightParen"};
  std::vector<std::string> atExpandAny = {"Letter" ,"RandomSequence","ExactValue"};
 std::vector<std::string> cookiesSquareAny = {"ExactValue","Letter","RandomSequence","Comma"};
    std::vector<std::string> cookiesFinalAny = {"Letter","ExactValue","RandomSequence","Zero"};

  if(tokens.size() > 0) {
  		currentIndex=0;
      setExactValue(tokens[0]);

      if(!inError) {
        currentIndex=0;
        tokens=tokenStack;
        tokenStack = {};
        setForRandoms(peekToken());
        if(!inError) {
      		currentIndex = 0;
      		tokens = tokenStack;
      		tokenStack={};
      		setForInterleave(peekToken(),interleaveAny);
      		
          if(!inError) {
        		currentIndex = 0;
        		tokens = tokenStack;
        		tokenStack = {};
        		setForAtExpand(peekToken(),atExpandAny, false);

						if(!inError) {
							currentIndex = 0;
							tokens=tokenStack;
							tokenStack = {};
							setForSquareBrackets(peekToken(),cookiesSquareAny,false);
							if(!inError) {
								currentIndex = 0;
								tokens=tokenStack;
								tokenStack = {};
								setFinal(peekToken(),cookiesFinalAny);
							}
						}
          }
        }
      }
    }
	}
void Parser::setFinal(Token t, std::vector<std::string> whitelist) {
	while (t.type!="NULL") {
		if(matchesAny(t.type,whitelist)) {
			tokenStack.push_back(t);
		}
		else if(t.type=="Comma") {
			
		}
		else {
			inError = true;
			break;
		}
		t = skipAndPeekToken();
	}
}

void Parser::setExactValue(Token t) {
	while (t.type!="NULL") {
		ParseExactValue(t);	
		if(peekToken().type !="NULL") {
			tokenStack.push_back(peekToken());
		}
		t = skipAndPeekToken();
	}
}
void Parser::setForRandoms(Token t) {
  while (t.type!="NULL") {
    ParseRandomSequence(t); 
    if(peekToken().type !="NULL") {
      tokenStack.push_back(peekToken());
    }
    t = skipAndPeekToken();
  }
}
void Parser::setForInterleave(Token t,std::vector<std::string> whitelist) {
  while (t.type!="NULL") {
    ParseInterleave(t,whitelist); 
    if(peekToken().type !="NULL") {
      tokenStack.push_back(peekToken());
    }
    t = skipAndPeekToken();
  }
}
void Parser::setForAtExpand(Token t, std::vector<std::string> whitelist, bool laundryMode) {
  while (t.type!="NULL") {
    ParseAtExpand(t, whitelist, laundryMode); 
    if(peekToken().type !="NULL") {
      tokenStack.push_back(peekToken());
    }
    t = skipAndPeekToken();
  }
}
void Parser::setForSquareBrackets(Token t, std::vector<std::string> whitelist, bool laundryMode) {
  while (t.type!="NULL") {
    ParseSquareBrackets(t,whitelist, laundryMode); 
    if(peekToken().type !="NULL") {
      tokenStack.push_back(peekToken());
    }
    t = skipAndPeekToken();
  }
}
void Parser::setForExactIntegers(Token t) {
	while(t.type!="NULL") {
		ParseExactInteger(t);
		if(peekToken().type !="NULL") {
			tokenStack.push_back(peekToken());
		}
		t = skipAndPeekToken();
	}
}
void Parser::replaceLettersWithNumbers(Token t) {
	std::string letterVal;
	int intVal;
	while(t.type!="NULL") {
		if(t.type =="Letter") {
			intVal= b64lookup.find(t.value)+1;
			letterVal=std::to_string(static_cast<long long>(intVal));
			tokenStack.push_back(Token("Integer",letterVal,-1,intVal));
		}
		else if(t.type=="Digit") {
			tokenStack.push_back(Token("Digit",t.value,-1,t.value=="0" ? 1 : std::stoi(t.value)));
		}
		else {
			tokenStack.push_back(t);
		}
		t = skipAndPeekToken();
	}
}

void Parser::setForChanceOfIntegers(Token t) {
	while(t.type!="NULL") {
		ParseChanceOfInteger(t);
		if(peekToken().type !="NULL") {
			tokenStack.push_back(peekToken());
		}
		t = skipAndPeekToken();
	}
}

void Parser::ParseChanceOfInteger(Token t) {
	Token last = Token("NULL","-1");
	if(t.type=="Integer" || t.type=="Digit") {
		last = t;
		t = skipAndPeekToken();
		if(t.type=="Question") {
			tokenStack.push_back(Token("ChanceOfInteger",last.value,-1,std::stoi(last.value)));	
			t=skipAndPeekToken();
		}
		else {
			tokenStack.push_back(last);
		}
		setForChanceOfIntegers(t);
	}
}
void Parser::ParseExactInteger(Token t) {
  if(t.type=="LeftAngle") {
    t=skipAndPeekToken();
		std::string num="";
		while(t.type=="Digit") {
			num+=t.value;
			t = skipAndPeekToken();
		}
		if(t.type=="RightAngle") {
			num=(num=="") ? "1" : num;
			t=skipAndPeekToken();
			tokenStack.push_back(Token("Integer",num,-1,std::stoi(num)));				
			setForExactIntegers(t);
		}
		else {
			inError=true;
		}
	} // not a LeftAngle, dont do shit
}

void Parser::ParseExactValue(Token t) {
  if(t.type=="LeftAngle") {
    t=skipAndPeekToken();
  	std::string num="";	
		if(t.type=="Minus") {
			num+="-";
			t=skipAndPeekToken();
		}
		if(t.type=="Digit" || t.type=="Period") {
			num += parseFloat(t);
			t=peekToken();
			if(!inError && t.type=="RightAngle") {
				skipToken();
				int sizeInt = static_cast<int>(exactFloats.size());
				num = ((num.length() == 0) || num=="." || num=="-") ? "0" : num;
				tokenStack.push_back(Token("ExactValue",num,sizeInt + 52));
				exactFloats.push_back(std::stof(num));	
 				setExactValue(peekToken());
			} 
			else {
				inError = true;
			}
		}
		else {
			inError=true;
		}
  } // not a LeftAngle, dont do shit
}
void Parser::ParseRandomSequence(Token t) {
  std::vector<Token> proposedRandomVector;
  if(t.type=="LeftCurly") {
    t=skipAndPeekToken();
    std::string num=""; 
    while(t.type=="Letter" || t.type=="ExactValue") {
      if(t.type=="Letter") {
        proposedRandomVector.push_back(Token(t.type,t.value,knobandinputlookup.find(t.value)));
        t=skipAndPeekToken();
      }
      if(t.type=="ExactValue") {
        proposedRandomVector.push_back(Token("ExactValue",t.value,t.index));
        t=skipAndPeekToken();
      }
      t=peekToken();
    }
    if(t.type=="RightCurly") {
      skipToken();
      randomVector.push_back(proposedRandomVector);  
			int sizeInt = static_cast<int>(randomVector.size());
      int myIndex = 52 + 26 + sizeInt -1;
			std::string stringDex = std::to_string(static_cast<long long>(myIndex));
      tokenStack.push_back(Token("RandomSequence",stringDex,myIndex));
    }
    else {
      inError = true;
    }
    ParseRandomSequence(peekToken());
  } // not a LeftCurly, dont do shit
}
void Parser::ParseInterleave(Token t,std::vector<std::string> whitelist) {
	std::vector<std::vector<std::vector<Token>>> stackVec;
  std::vector<Token> tempStack;
  std::vector<Token> output;
	stackVec.push_back({});
  int parenCount = 0;
		
	while(matchesAny(t.type,whitelist)) {
		if(t.type=="LeftParen") {
			stackVec.push_back({});
      parenCount++;
		}
		else if(t.type=="RightParen") {
			//evaluate top of stack
      
      if(parenCount > 0) {
        parenCount--;
  			tempStack = interleaveExpand(stackVec.back()); 
        //pop top of stack
        	stackVec.pop_back();
  			if(stackVec.size() > 0) {
  				//push this evaluated vector<Token> to new top
  				stackVec.back().push_back(tempStack);
  			}
  			else {
  				inError=true;	
  			}
      }
      else {
        inError=true;
      }
		}
		//Letter, ExactValue, or RandomSequence
		else { 
			stackVec.back().push_back({t});	
		}
		t=skipAndPeekToken();	
	}
  if(parenCount == 0) {
	 output = interleaveExpand(stackVec.back());
    tokenStack.insert(tokenStack.end(),output.begin(),output.end());
  }
}
void Parser::ParseAtExpand(Token t, std::vector<std::string> whitelist, bool laundryMode) {
	// for letter,{},<> followed by an optional "@" and an integer
	// ab@3  da@2  cad  
	std::vector<std::vector<Token>> tokenVec;
	std::vector<Token> proposedTokens;
	tokenVec.push_back({});
	int atNum = -1;
	if(matchesAny(t.type,whitelist)) {
		while(matchesAny(t.type,whitelist)) {
			tokenVec.back().push_back(t);	
			t = skipAndPeekToken();
		}
		atNum = ParseAtPart(t);
    if(laundryMode) {
      proposedTokens = atExpandTokens(tokenVec,atNum);
    }
    else {
		  proposedTokens = countExpandTokens(tokenVec,atNum);
    }
		tokenStack.insert(tokenStack.end(),proposedTokens.begin(),proposedTokens.end());
	}
}
void Parser::ParseSquareBrackets(Token t,std::vector<std::string> whitelist, bool laundryMode) {
  std::vector<Token> proposedTokens;
	std::vector<std::vector<Token>> insideOfBrackets;
	int atNum;
	if(t.type=="LeftSquare") {
		t=skipAndPeekToken();
		insideOfBrackets.push_back({});
		while(matchesAny(t.type,whitelist)) {
			if(t.type=="Comma") {
				insideOfBrackets.push_back({});
			}
			else {
				insideOfBrackets.back().push_back(t);
			}
			t=skipAndPeekToken();
		}
		if(t.type=="RightSquare") {
			t = skipAndPeekToken();
			atNum = ParseAtPart(t);
      if(laundryMode) {
        proposedTokens = atExpandTokens(insideOfBrackets,atNum);
      }
      else {
			 proposedTokens = countExpandTokens(insideOfBrackets,atNum);
      }
  		tokenStack.insert(tokenStack.end(),proposedTokens.begin(),proposedTokens.end());
		}
		else {
			inError = true;
		}
	}
}
std::vector<Token> Parser::atExpandTokens(std::vector<std::vector<Token>> tokenVecVec, int atNum) {
  std::vector<Token> output = {};
  Token thisToken = Token("error","error");
  int sum = 0;
  int thisLength=0;
  int innerDex = 0;
	std::string newType;
  std::string thisVal;
  for(unsigned int i = 0; i < tokenVecVec.size(); i++) {

    if(atNum == -1) {
      output.insert( output.end(), tokenVecVec[i].begin(), tokenVecVec[i].end() );
    }
    else {
      int sectionSize = (int) tokenVecVec[i].size();
      innerDex = 0;
      sum=0;
      while(sum < atNum) {
        thisToken = tokenVecVec[i].size() ? tokenVecVec[i][innerDex % sectionSize] : Token("Integer","0",-1,1);
        if( thisToken.type=="Letter") {
          thisLength = b64lookup.find(thisToken.value)+1;
					newType="Integer";
        }
        else if(thisToken.type =="Digit" || thisToken.type=="Integer" || thisToken.type=="ChanceOfInteger") {
					newType = thisToken.type;
          thisLength = thisToken.duration; 
        }

        if((atNum - sum) < thisLength) {
          thisLength = (atNum-sum);
        }
				thisVal = (thisToken.value=="0") ? "0" :std::to_string(static_cast<long long>(thisLength));
        sum += thisLength;
        output.push_back(Token(newType,thisVal,-1,thisLength));
        innerDex++;
      }
    }
  }
  return output;
}
std::vector<Token> Parser::countExpandTokens(std::vector<std::vector<Token>> tokenVecVec, int atNum) {
	std::vector<Token> output;
	for(unsigned int i=0; i < tokenVecVec.size(); i++) { 
    if(atNum > -1) {
  		int sizeMod = (int) tokenVecVec[i].size();
  		atNum = atNum==-1 ? sizeMod : atNum;
  		if(sizeMod > 0 ) {
  			for(int j = 0; j < atNum; j++) {
  				if(tokenVecVec[i].size()) {
  					output.push_back(tokenVecVec[i][j % sizeMod]);
  				}
  				else { //tokenVecVec[i].size()==0
  					//output.push_back(Token("Zero",""));
  				}
  			}
  		}
  		else { //sizeMod <= 0
  			output.push_back(Token("Zero",""));
  		}
    }
    else {
      output.insert( output.end(), tokenVecVec[i].begin(), tokenVecVec[i].end() );
    }
	}
	return output;
}
int Parser::ParseAtPart(Token t) {
	std::string atString = "";
	int atNum = -1;
	if(t.type=="At") {
		t=skipAndPeekToken();
		while(t.type=="Digit") {
			atString+=t.value;
			t=skipAndPeekToken();
		}
    atNum  = atString != "" ? std::stoi(atString) : -1;
	}
	return atNum;
}
char Parser::peekChar() {
	if (currentIndex < (int) expression.size()) return expression[currentIndex];
	return 0;
}
Token Parser::peekToken() {
	if (currentIndex < (int) tokens.size()) return tokens[currentIndex];
	return Token("NULL","NULL");
}
void Parser::skipToken() {
	currentIndex++;
	}
void Parser::skipChar() {
	currentIndex++;
}
char Parser::skipAndPeekChar() {
	skipChar();
	return peekChar();
}
Token Parser::skipAndPeekToken() {
	skipToken();
	return peekToken();
}
std::string Parser::parseFloat(Token t)
{
    std::string number = "";
    if (t.type != "Period")
    {
        // parse before '.'
        while (t.type!="NULL" && t.type=="Digit" && t.type != "Period" ) {
            number += t.value;
            t = skipAndPeekToken();
        }
    }
    if (t.type=="Period")
    {
        // parse after '.'
        number += t.value;
        t = skipAndPeekToken();
        if (t.type!="NULL" && t.type == "Digit") {
            while (t.type!="NULL" && t.type=="Digit" ) {
                number += t.value;
                t = skipAndPeekToken();
            }
        } else {
					inError = true;
          printf("Expected digit after '.', number: %s\n",number.c_str());
        }
    }
    return number;
}
void Token::print() {
	printf("type: %s   value: %s   index: %i  dur:%i\n",type.c_str(),value.c_str(),index,duration);
}
std::vector<Token> tokenizeString(std::string input) {
	std::vector<Token> stack;
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
			stack.push_back(Token("Letter",token,knobandinputlookup.find(token)));
		}
    else if(integerlookup.find(token) != -1) {
			stack.push_back(Token("Digit",token));
		}
    else stack.push_back(Token("Unknown",token));
	}
	return stack;
}
void whoKnowsQuantize(std::string input) {
	Quantizer q = Quantizer("2212221",12,0);
	float in = std::stof(input);
	printf("closest: %f\n",q.quantize(in));
	printf("even   : %f\n",q.quantizeEven(in));
}
Quantizer::Quantizer() {
  Quantizer("2212221",12,0);
}
Quantizer::Quantizer(std::string intervals, int divisions, int trans) {
	scaleParser = Parser(intervals);
	//printTokenVector(scaleParser.tokens);
	numDivisions = divisions;
	transpose=trans;
	fTranspose = (float)transpose/(float)numDivisions;
	mappedValues = generateMappedValues();
	numSteps = (int) mappedValues.size();
  printf("  mappedValues:\n");
	printFloatVector(mappedValues);
} 
std::vector<float> Quantizer::generateMappedValues() {
	std::vector<float> output;
	float sum = 0.f;
	float fNumDivisions = (float)numDivisions;
	float currentVal = 0.f;
	std::vector<Token> stack = scaleParser.tokens;
	output.push_back(0.f);
	for(unsigned int i = 0; i < stack.size(); i++) {
		if(stack[i].type=="Digit") {
			sum += std::stof(stack[i].value);
			currentVal = sum/fNumDivisions; 
			output.push_back(currentVal);
		}
	}
	return output;
}
float Quantizer::findEvenSpacingValue(float input, std::vector<float> allowedValues) {
	return allowedValues[floor(input*allowedValues.size())];
}
float Quantizer::findEvenSpacingImpure(float input) {
  return mappedValues[floor(input*numSteps)];
}
float Quantizer::findClosestValue(float input,std::vector<float> allowedValues) {
	float closestValue = 10.f;
	float smallestDiff = 10.f;
	float thisDiff = 0.f;
	int size = (int)allowedValues.size();
	for(int i = 0; i < size; i++) {
		thisDiff = fabs(input - allowedValues[i]);	
		if(thisDiff < smallestDiff) {
			closestValue = allowedValues[i];
			smallestDiff = thisDiff;
		}
	}
	return closestValue;
}
float Quantizer::quantize(float input) {
	float octavePart = floor(input);
	float fractionalPart = input-octavePart;
	float quantizedFractional = findClosestValue(fractionalPart,mappedValues);
	float quantizedPreTranspose = octavePart + quantizedFractional;
	float quantizedVal = quantizedPreTranspose + fTranspose;
	return quantizedVal; 
}
float Quantizer::quantizeEven(float input) {
	float octavePart = floor(input);
	float fractionalPart = input-octavePart;
	float quantizedFractional = findEvenSpacingImpure(fractionalPart);
	float quantizedPreTranspose = octavePart + quantizedFractional;
	float quantizedVal = quantizedPreTranspose + fTranspose;
	return quantizedVal; 
}
