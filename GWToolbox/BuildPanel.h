#pragma once

#include "ToolboxPanel.h"

#include "Timer.h"
#include <vector>
#include <string>

class BuildPanel : public ToolboxPanel {
private:
	const int amount = 16;
	const int maxPartySize = 12;

	// Send a complete teambuild by index
	void sendTeamBuild(int buildID);

	// Send a specific player build
	void sendPlayerBuild(std::wstring name, std::wstring template_, int partyMember, bool showNumbers);

public:
	void LoadIni();
	void BuildUI() override;
	void MainRoutine();
};
