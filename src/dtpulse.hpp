
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
class AbsoluteSequence {
	public:
		AbsoluteSequence(std::string expr, std::string lookup);
		std::vector<int> indexSequence;
		std::vector<float> exactFloats;
		std::vector<std::vector<int>> randoms;
		void print();
};
class Token {
	public:
		std::string type;
		std::string val;
		Token(std::string t, std::string v);
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
		void setExpression(std::string expr);
		std::string parseNumber(char c);
	private:
		int currentIndex;
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
