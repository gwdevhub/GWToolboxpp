#include "Builds.h"
#include "../API/APIMain.h"

using namespace std;

void Builds::sendTeamBuild(int buildID) {
	// TODO
}

void Builds::sendPlayerBuild(wstring name, wstring template_, int partyMember, bool showNumbers) {
	if (name.empty()) return;
	if (template_.empty()) return;

	wstring msg = wstring(L"[");
	if (showNumbers) {
		msg.append(to_wstring(partyMember));
		msg.append(L" - ");
	}
	msg.append(L";");
	msg.append(template_);
	msg.append(L"]");

	GWAPI::GWAPIMgr::GetInstance()->Chat->SendChat(msg.c_str(), L'#');
}

void Builds::readIni() {
	// TODO
}

void Builds::buildUI() {
	// TODO
}

void Builds::mainRoutine() {
	// TODO
}
