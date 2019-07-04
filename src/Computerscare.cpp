#include "Computerscare.hpp"

Plugin *pluginInstance;


void init(Plugin *p) {
	pluginInstance = p;

	p->addModel(modelComputerscareDebug);

	p->addModel(modelComputerscarePatchSequencer);
	p->addModel(modelComputerscareLaundrySoup);
	//p->addModel(modelComputerscareILoveCookies);
	p->addModel(modelComputerscareOhPeas);
	//p->addModel(modelComputerscareIso);
	p->addModel(modelComputerscareKnolyPobs);
	p->addModel(modelComputerscareBolyPuttons);
}
