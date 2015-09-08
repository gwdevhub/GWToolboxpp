#pragma once

#include "../include/OSHGui/OSHGui.hpp"

#include "Timer.h"
#include <vector>
#include <string>

class BuildPanel : public OSHGui::Panel {
private:
	const int amount = 16;
	const int maxPartySize = 12;

	// Send a complete teambuild by index
	void sendTeamBuild(int buildID);

	// Send a specific player build
	void sendPlayerBuild(std::wstring name, std::wstring template_, int partyMember, bool showNumbers);

public:
	void LoadIni();
	void BuildUI();
	void MainRoutine();
};
