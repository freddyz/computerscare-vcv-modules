
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
#endif

bool is_digits(const std::string &str);
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