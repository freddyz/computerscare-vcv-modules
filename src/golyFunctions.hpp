#include <math.h>

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <typeinfo>
#include <vector>

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
  float currentValues[16];

  Goly();
  void invoke(int algorithm, std::vector<float> gp);
  void invoke(int algorithm, std::vector<float> gp, int num);
};
