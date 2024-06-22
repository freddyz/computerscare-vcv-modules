#include "Computerscare.hpp"

Plugin *pluginInstance;

void init(Plugin *p) {
	pluginInstance = p;

	//Platinum
	p->addModel(modelComputerscareDebug);
	p->addModel(modelComputerscarePatchSequencer);
	p->addModel(modelComputerscareLaundrySoup);
	p->addModel(modelComputerscareILoveCookies);
	p->addModel(modelComputerscareOhPeas);
	p->addModel(modelComputerscareHorseADoodleDoo);

	//Compoly Utilities
	p->addModel(modelComputerscareNomplexPumbers);

	//Poly Utilities
	p->addModel(modelComputerscareKnolyPobs);
	p->addModel(modelComputerscareBolyPuttons);
	p->addModel(modelComputerscareRolyPouter);
	p->addModel(modelComputerscareTolyPools);
	p->addModel(modelComputerscareSolyPequencer);
	p->addModel(modelComputerscareGolyPenerator);
	p->addModel(modelComputerscareMolyPatrix);
	p->addModel(modelComputerscareTolyPoolsV2);

	//Visual
	p->addModel(modelComputerscareFolyPace);
	p->addModel(modelComputerscareBlank);
	p->addModel(modelComputerscareBlankExpander);
	p->addModel(modelComputerscareStolyFickPigure);
	p->addModel(modelComputerscareGlolyPitch);
	
}
