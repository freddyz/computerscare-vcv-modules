#include "golyFunctions.hpp"

Goly::Goly() {
	for (int i = 0; i < 16; i++) {
		currentValues[i] = 0.f;
	}

}
//[A,B,C,D]
void Goly::invoke(int algorithm, std::vector<float> gp, int num = 16) {
	switch (algorithm)
	{
	case 1: // code to be executed if n = 1;
		//linear
		//ip / proportion
		//defaults:[A,B,C,D]=[0,1,1,0]
		//
		// C*((ip-A)*B)+D

		for (int i = 0; i < num; i++) {
			float ip = (float)i / num;
			currentValues[i] = gp[2] * ((ip - gp[0]) * gp[1]) + gp[3];
		}
		break;
	case 2:
		//sigmoid
		for (int i = 0; i < num; i++) {
			float ip = (float)i / num;
			float d = ip - gp[0] - 0.5;
			currentValues[i] = gp[2] / (1 + exp(-d * exp(4 * gp[1]))) + gp[3];
		}
		break;
	case 3:
		//hump
		for (int i = 0; i < num; i++) {
			float ip = (float)i / num;
			float d = ip - gp[0] - 0.5;
			currentValues[i] = gp[2] * exp(-d*d*exp(5*gp[1])) + gp[3];
		}
		break;
	case 4:
		float trigFactor;
		for (int i = 0; i < num; i++) {
			trigFactor = 5.f + ((float) (i) ) * 2.f * M_PI / (16 * expf(gp[0]));
			float val = 0.f;
			for (int j = 1; j < 6; j++) {
				val += gp[j] * sinf(trigFactor * j);
			}
			currentValues[i] = val;
		}
		break;

	default:
		/*for (int i = 0; i < 16; i++) {
			currentValues[i] = 0.f;
		}*/
		int k = 0;

	}
}

