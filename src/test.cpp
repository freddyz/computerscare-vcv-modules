#include "dtpulse.hpp"
int main(int argc, char** argv)
{
				int type = 0;
				std::vector<int> output;
				std::string strResult = "";
				std::string strParens = "";
				std::vector <std::string> input;
				if(argv[2]) {
					type = std::stoi(argv[2]); 
				}
				if(type == 0) {
					output = parseEntireString(argv[1],b64lookup,0);
					printVector(output);
				}
				else if(type==1) {
					output = parseEntireString(argv[1],knobandinputlookup,1);
					printVector(output);
				}
				else if(type==2) {
					strParens = splitRecur(argv[1]);
					printf("%s\n",strParens.c_str());	
				}
				else if(type==3) {
					for(int i = 0; i < argc-3; i++) {
						input.push_back(argv[i+3]);
					}
					strResult = interleaveExpand(input);
					printf("%s\n",strResult.c_str());
				}
				else if(type==4) {
					output = parseEntireString(argv[1],knobandinputlookup,1);
					strResult = concatVectorFromLookup(output,knobandinputlookup);
					printf("%s\n",strResult.c_str()); 	
				}

				return 0;
}
void printVector(std::vector <int> intVector) {
				for (std::vector<int>::const_iterator i = intVector.begin(); i != intVector.end(); ++i){
								std::cout << *i << ' ';
				}
				std::cout << std::endl;
}
