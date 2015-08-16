#pragma once

#include "Timer.h"
#include <vector>
#include <string>

class Builds {
private:
	const int amount = 16;
	const int maxPartySize = 12;

	// Send a complete teambuild by index
	void sendTeamBuild(int buildID);

	// Send a specific player build
	void sendPlayerBuild(std::wstring name, std::wstring template_, int partyMember, bool showNumbers);

public:
	void readIni();
	void buildUI();
	void mainRoutine();
};