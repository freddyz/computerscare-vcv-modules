#include <string>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <vector>
#include <algorithm>
#include <typeinfo>
#include <stdexcept>
#include <math.h>

#ifndef MY_GLOBALS_H
#define MY_GLOBALS_H
extern std::string b64lookup;
extern std::string integerlookup;
extern std::string knoblookup;
extern std::string inputlookup;
extern std::string knobandinputlookup;
#endif

class Goly {
	public:
		std::string type;
		std::string value;
		int index;
		int duration;
		Goly(std::string t, std::string v);
		
};

int myHorse(int x, int p);
