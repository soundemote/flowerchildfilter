#include "plugin.hpp"

Plugin* pluginInstance;

void init(Plugin* p) {
	pluginInstance = p;

	p->addModel(modelSuperLove);
	// Flower Child and Shaped Resonator live in src/modules/ until they ship.
}
