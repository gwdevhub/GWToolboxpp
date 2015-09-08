#include "BuildPanel.h"
#include "../API/APIMain.h"

using namespace std;
using namespace OSHGui;

void BuildPanel::sendTeamBuild(int buildID) {
	// TODO
}

void BuildPanel::sendPlayerBuild(wstring name, wstring template_, int partyMember, bool showNumbers) {
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

void BuildPanel::LoadIni() {
	// TODO
}

void BuildPanel::BuildUI() {
	SetSize(250, 300);

	Label* label = new Label();
	label->SetText("Build panel under construction");
	AddControl(label);
}

void BuildPanel::MainRoutine() {
	// TODO
}
