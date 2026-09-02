#include "plugin.hpp"

Plugin* pluginInstance;

void init(Plugin* p) {
	pluginInstance = p;

	p->addModel(modelFlowerChild);
	p->addModel(modelShapedResonator);
	p->addModel(modelSuperLove);
}
