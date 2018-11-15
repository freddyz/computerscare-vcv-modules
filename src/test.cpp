#include "dtpulse.cpp"
std::string knoblookup = "abcdefghijklmnopqrstuvwxy";
int main(int argc, char** argv)
{
				int type = 0;
				std::vector<int> output;
				std::string strResult = "";
				if(argv[2]) {
					type = std::stoi(argv[2]); 
				}
				if(type == 0) {
					output = parseEntireString(argv[1],b64lookup,0);
				}
				else if(type==1) {
					output = parseEntireString(argv[1],knoblookup,1);
				}
				else if(type==2) {
					std::vector <std::string> input;
					for(int i = 0; i < argc-3; i++) {
						input.push_back(argv[i+3]);
					}
					strResult = interleaveExpand(input);
					printf("%s\n",strResult.c_str());	
				}
				printVector(output);
				return 0;
}
void printVector(std::vector <int> intVector) {
				for (std::vector<int>::const_iterator i = intVector.begin(); i != intVector.end(); ++i){
								std::cout << *i << ' ';
				}
				std::cout << std::endl;
}
