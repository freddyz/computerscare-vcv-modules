#include "golyFunctions.hpp"


Goly::Goly() {
	for (int i = 0; i < 16; i++) {
		currentValues[i] = 0.f;
	}

}
void Goly::invoke(int algorithm, std::vector<float> gp) {
	switch (algorithm)
	{
	case 0: // code to be executed if n = 1;
		for (int i = 0; i < 16; i++) {
			currentValues[i] = gp[0]+gp[1]*i/16.f+gp[2]*i*i/8.f;
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

