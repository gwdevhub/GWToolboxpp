#include "../API/APIMain.h"
#include "../include/OSHGui/OSHGui.hpp"
#include "../include/OSHGui/Drawing/Direct3D9/Direct3D9Renderer.hpp"

using namespace OSHGui;

class GWToolbox {

public:
	GWToolbox(HMODULE mod) : m_dllmodule(mod) {};

	void exec();
	void main();
	void destroy();
	bool isActive();

private:
	bool m_Active;
	HMODULE m_dllmodule;

	static void threadStarter(GWToolbox* ptr);
};
