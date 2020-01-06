#include "golyFunctions.hpp"


Goly::Goly() {
	for (int i = 0; i < 16; i++) {
		currentValues[i] = 0.f;
	}

}
void Goly::invoke(int algorithm, std::vector<float> params) {
	switch (algorithm)
	{
	case 0: // code to be executed if n = 1;
		for (int i = 0; i < 16; i++) {
			currentValues[i] = params[0];
		}
		break;

	case 1:

		for (int i = 0; i < 16; i++) {
			currentValues[i] = (float) i * (params[0]);
		}
		break;

	default:
		for (int i = 0; i < 16; i++) {
			currentValues[i] = (float) i ;
		}

	}
}

