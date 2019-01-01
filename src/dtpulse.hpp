
#include <string>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <vector>
#include <algorithm>
#include <typeinfo>
#include <stdexcept>

#ifndef MY_GLOBALS_H
#define MY_GLOBALS_H
extern std::string b64lookup;
extern std::string integerlookup;
extern std::string knoblookup;
extern std::string inputlookup;
extern std::string knobandinputlookup;
#endif

class Token {
	public:
		std::string type;
		std::string value;
		int index;
		Token(std::string t, std::string v);
		Token(std::string t, std::string v, int dex);
		void print();
};
class Parser {
	public:
		Parser(std::string expr);
		std::string expression;
		std::vector<Token> tokens;
		char peekChar();
		char skipAndPeekChar();
		void skipChar();

		Token peekToken();
		Token skipAndPeekToken();
		void setForCookies();
		void setForLaundry();
		void skipToken();
		void setExactValue(Token t);
		void setForExactIntegers(Token t);
		void setForChanceOfIntegers(Token t);
		void setForRandoms(Token t);
		void setForInterleave(Token t,std::vector<std::string> whitelist);
		void setForAtExpand(Token t, std::vector<std::string> whitelist, bool laundryMode);
		void setForSquareBrackets(Token t, std::vector<std::string> whitelist, bool laundryMode);
		void setFinal(Token t, std::vector<std::string> whitelist);
		bool inError;
		std::string parseFloat(Token t);
		std::vector<Token> tokenStack;
		std::vector<float> exactFloats;
		std::vector<std::vector<Token>> randomVector;
		std::vector<Token> atExpandTokens(std::vector<std::vector<Token>> tokenVecVec, int atNum);
		std::vector<Token> countExpandTokens(std::vector<std::vector<Token>> tokenVecVec, int atNum);
	private:
		int currentIndex;
		void ParseExactValue(Token t);
		void ParseExactInteger(Token t);
		void ParseRandomSequence(Token t);
		void ParseInterleave(Token t,std::vector<std::string> whitelist);
		void ParseAtExpand(Token t, std::vector<std::string> whitelist, bool laundryMode);
		void ParseSquareBrackets(Token t, std::vector<std::string> whitelist, bool laundryMode);
		void ParseChanceOfInteger(Token t);
		int ParseAtPart(Token t);
};
class AbsoluteSequence {
	public:
		AbsoluteSequence(std::string expr, std::string lookup);
		AbsoluteSequence();
		void randomizeIndex(int index);
		std::vector<int> indexSequence;
		std::vector<int> workingIndexSequence;
		std::vector<float> exactFloats;
		std::vector<std::vector<int>> randomIndexes;
		std::vector<std::vector<Token>> randomTokens;
		std::vector<Token> tokenStack;
		int readHead;
		int numTokens;
		void print();
		void skipStep();
		int peekStep();
		int peekWorkingStep();
		int skipAndPeek();
		void incrementAndCheck();
		int getReadHead();
		int getCurrentAddressAtReadHead();
		std::string getWorkingStepDisplay();
};
bool matchesAny(std::string val, std::vector<std::string> whitelist);
bool is_digits(const std::string &str);
void padTo(std::string &str, const size_t num, const char paddingChar );
std::vector <int> parseString(std::string expr);
std::vector <int> parseDt(std::string input, int offset, std::string lookup);
std::vector <int> parseLookup(std::string input, int offset, std::string lookup);
std::vector<int> parseEntireString(std::string input,std::string lookup, int type);
std::vector<int> parseStringAsValues(std::string input,std::string lookup);
std::vector<int> parseStringAsTimes(std::string input,std::string lookup);
void printVector(std::vector <int> intVector); 
void printFloatVector(std::vector<float> floatVector);
void printTokenVector(std::vector<std::vector<Token>> tokenVector);
void printTokenVector(std::vector<Token> tokenVector);
std::string splitRecur(std::string input);
void  parseRecur(Token t);
std::string interleaveExpand(std::vector<std::string> blocks);
std::vector<Token> interleaveExpand(std::vector<std::vector<Token>> blocks);
std::string hashExpand(std::string input, int hashnum);
std::string atExpand(std::string input, int atnum, std::string lookup);
std::string countExpand(std::string input, int atnum);
std::string concatVectorFromLookup(std::vector<int> vector, std::string lookup);
std::vector<Token> tokenizeString(std::string input);
bool matchParens(std::string value);
std::string evalToken(std::string input, std::string type,std::vector<Token> tStack);
void whoKnows(std::string input);
void whoKnowsLaundry(std::string input);
std::vector<int> getIndicesFromTokenStack(std::vector<Token> tokens);
std::vector<int> duplicateIntVector(std::vector<int> input);
