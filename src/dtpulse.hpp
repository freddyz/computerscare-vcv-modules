
#include <string>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <vector>
#include <algorithm>

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
		void skipToken();
		void setExpression(Token t);
		void setForRandoms(Token t);
		std::string parseNumber(Token t);
		std::vector<Token> tokenStack;
		std::vector<float> exactFloats;
		std::vector<std::vector<Token>> randomVector;
	private:
		int currentIndex;
		void ParseExactValue(Token t);
		void ParseRandomSequence(Token t);
};
class AbsoluteSequence {
	public:
		AbsoluteSequence(std::string expr, std::string lookup);
		std::vector<int> indexSequence;
		std::vector<float> exactFloats;
		std::vector<std::vector<int>> randomIndexes;
		std::vector<std::vector<Token>> randomTokens;
		void print();
};
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
std::string splitRecur(std::string input);
std::string interleaveExpand(std::vector<std::string> blocks);
std::string hashExpand(std::string input, int hashnum);
std::string atExpand(std::string input, int atnum, std::string lookup);
std::string countExpand(std::string input, int atnum);
std::string concatVectorFromLookup(std::vector<int> vector, std::string lookup);
std::vector<Token> tokenizeString(std::string input);
bool matchParens(std::string value);
std::string evalToken(std::string input, std::string type,std::vector<Token> tStack);
void whoKnows(std::string input);
