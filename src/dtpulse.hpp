#include <string>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <vector>
#include <algorithm>
bool is_digits(const std::string &str);
std::vector <int> parseString(std::string expr);
std::vector <int> parseDt(std::string input, int offset, std::string lookup);
std::vector<int> parseEntireString(std::string input,std::string lookup);
std::vector<int> parseStringAsValues(std::string input,std::string lookup);
void printVector(std::vector <int> intVector); 
std::string hashExpand(std::string input, int hashnum);
std::string atExpand(std::string input, int atnum, std::string lookup);
