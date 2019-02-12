#include "Computerscare.hpp"

Plugin *computerscarePluginInstance;


void init(Plugin *p) {
	computerscarePluginInstance = p;

	//p->addModel(modelComputerscareDebug);
	//p->addModel(modelComputerscarePatchSequencer);
	//p->addModel(modelComputerscareLaundrySoup);
	//p->addModel(modelComputerscareILoveCookies);
	//p->addModel(modelComputerscareOhPeas);
	p->addModel(modelComputerscareIso);
}
