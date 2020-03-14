#include "golyFunctions.hpp"


Goly::Goly() {
	for (int i = 0; i < 16; i++) {
		currentValues[i] = 0.f;
	}

}
//[A,B,C,D]
void Goly::invoke(int algorithm, std::vector<float> gp) {
	switch (algorithm)
	{
	case 0: // code to be executed if n = 1;
		//linear
		//defaults:[A,B,C,D]=[1,0,1,0]
		// A*((x-D)*C)+B
		for (int i = 0; i < 16; i++) {
			currentValues[i] = gp[0]*((i-gp[3])*gp[2])+gp[1];
		}
		break;
	case 1:
		float trigFactor;
		for (int i = 0; i < 16; i++) {
			trigFactor = 5.f+((float) (i) )*2.f*M_PI/(16*expf(gp[0]));
			float val =0.f;
			for(int j = 1; j<6; j++) {
				val += gp[j]*sinf(trigFactor*j);
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

