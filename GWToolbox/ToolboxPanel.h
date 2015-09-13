#pragma once

#include "../include/OSHGui/OSHGui.hpp"

class ToolboxPanel : public OSHGui::Panel {
public:
	virtual void BuildUI() = 0;
};
