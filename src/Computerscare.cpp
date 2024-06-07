#include "Computerscare.hpp"

Plugin *pluginInstance;

void init(Plugin *p) {
	pluginInstance = p;

	p->addModel(modelComputerscareDebug);
	p->addModel(modelComputerscarePatchSequencer);
	p->addModel(modelComputerscareLaundrySoup);
	p->addModel(modelComputerscareILoveCookies);
	p->addModel(modelComputerscareOhPeas);

	p->addModel(modelComputerscareKnolyPobs);
	p->addModel(modelComputerscareBolyPuttons);
	p->addModel(modelComputerscareRolyPouter);
	p->addModel(modelComputerscareTolyPools);
	p->addModel(modelComputerscareSolyPequencer);

	p->addModel(modelComputerscareFolyPace);
	p->addModel(modelComputerscareBlank);
	p->addModel(modelComputerscareBlankExpander);
	p->addModel(modelComputerscareStolyFickPigure);

	p->addModel(modelComputerscareGolyPenerator);
	p->addModel(modelComputerscareMolyPatrix);
	p->addModel(modelComputerscareHorseADoodleDoo);

	p->addModel(modelComputerscareTolyPoolsV2);
}
