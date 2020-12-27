#include "golyFunctions.hpp"

Goly::Goly() {
	for (int i = 0; i < 16; i++) {
		currentValues[i] = 0.f;
	}

}
//[A,B,C,D]
/*
			std::vector<float> golyParams = 
			{
		gp[0]=	params[IN_OFFSET].getValue(),  //-1,1
		gp[1]=	params[IN_SCALE].getValue(), //-2,2
		gp[2]=	arams[OUT_SCALE].getValue(),  //-20, 20
		gp[3]=	params[OUT_OFFSET].getValue()}; // -10,10


*/
void Goly::invoke(int algorithm, std::vector<float> gp, int num = 16) {
	float trigFactor = 2*M_PI / num;
	switch (algorithm)
	{
	case 0: // code to be executed if n = 1;
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
	case 1:
		//sigmoid
		for (int i = 0; i < num; i++) {
			float ip = (float)i / num;
			float d = ip - gp[0] - 0.5;
			currentValues[i] = gp[2] / (1 + exp(-d * exp(-4 * gp[1]+6))) + gp[3];
		}
		break;
	case 2:
		//hump
		for (int i = 0; i < num; i++) {
			float ip = (float)i / num;
			float d = ip - gp[0] - 0.5;
			currentValues[i] = gp[2] * exp(-d*d*exp(-5*gp[1]+7)) + gp[3];
		}
		break;
	case 3:
		//sine wave
		for (int i = 0; i < num; i++) {
			float ip = (float)i / num;
			float d = trigFactor*(ip - gp[0]);
			currentValues[i] = gp[2] * (1+sinf(d*exp(-2*gp[1]+2)))/2 + gp[3];
		}
		break;
	case 4:
		//pseudo random
		for(int i = 0; i < num; i++) {
			float ip = (float) i / num;
			float d = trigFactor*(ip-gp[0]);
			currentValues[i] = gp[2]*(4 + sinf(d*29-3+16*gp[1]) + sinf(-d*24-2+39*gp[1]) + sinf(d*17-1-27*gp[1])+sinf(d*109+12.2-17*gp[1]))/8 + gp[3];
		}
	
	default:
		int k = 0;

	}
}

