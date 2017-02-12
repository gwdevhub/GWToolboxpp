#include "DialogPanel.h"

#include <GWCA\GWCA.h>

#include "Config.h"
#include "MainWindow.h"
#include "GuiUtils.h"
#include "GWToolbox.h"

using namespace OSHGui;

void DialogPanel::Draw(IDirect3DDevice9* pDevice) {
	auto DialogButton = [](int x_idx, int x_qty, const char* text, DWORD dialog) -> void {
		if (x_idx != 0) ImGui::SameLine();
		float w = (ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x * (x_qty + 1)) / x_qty;
		if (ImGui::Button(text, ImVec2(w, 0))) {
			GW::Agents().Dialog(dialog);
		}
	};

	ImGui::Begin(Name());
	DialogButton(0, 2, "Four Horseman", QuestAcceptDialog(GW::Constants::QuestID::UW::Planes));
	DialogButton(1, 2, "Demon Assassin", QuestAcceptDialog(GW::Constants::QuestID::UW::Mnt));
	DialogButton(0, 2, "Tower of Strength", QuestAcceptDialog(GW::Constants::QuestID::Fow::Tos));
	DialogButton(1, 2, "Foundry Reward", QuestRewardDialog(GW::Constants::QuestID::Doa::FoundryBreakout));

	DialogButton(0, 4, "Lab", GW::Constants::DialogID::UwTeleLab);
	DialogButton(1, 4, "Vale", GW::Constants::DialogID::UwTeleVale);
	DialogButton(2, 4, "Pits", GW::Constants::DialogID::UwTelePits);
	DialogButton(3, 4, "Pools", GW::Constants::DialogID::UwTelePools);

	DialogButton(0, 3, "Planes", GW::Constants::DialogID::UwTelePlanes);
	DialogButton(1, 3, "Wastes", GW::Constants::DialogID::UwTeleWastes);
	DialogButton(2, 3, "Mountains", GW::Constants::DialogID::UwTeleMnt);

	const int n_quests = 29;
	static const char* const questnames[] = { "UW - Chamber",
		"UW - Wastes",
		"UW - UWG",
		"UW - Mnt",
		"UW - Pits",
		"UW - Planes",
		"UW - Pools",
		"UW - Escort",
		"UW - Restore",
		"UW - Vale",
		"FoW - Defend",
		"FoW - Army Of Darknesses",
		"FoW - WailingLord",
		"FoW - Griffons",
		"FoW - Slaves",
		"FoW - Restore",
		"FoW - Hunt",
		"FoW - Forgemaster",
		"FoW - Tos",
		"FoW - Toc",
		"FoW - Khobay",
		"DoA - Gloom 1: Deathbringer Company",
		"DoA - Gloom 2: Rift Between Us",
		"DoA - Gloom 3: To The Rescue",
		"DoA - City",
		"DoA - Veil 1: Breaching Stygian Veil",
		"DoA - Veil 2: Brood Wars",
		"DoA - Foundry 1: Foundry Breakout",
		"DoA - Foundry 2: Foundry Of Failed Creations" };
	for (int i = 0; i < NUM_DIALOG_FAV_QUESTS; ++i) {
		ImGui::PushID(i);
		ImGui::Combo("", &favindex[i], questnames, n_quests);
		ImGui::SameLine();
		if (ImGui::Button("Take")) {
			GW::Agents().Dialog(QuestAcceptDialog(IndexToQuestID(favindex[i])));
		}
		ImGui::SameLine();
		if (ImGui::Button("Reward")) {
			GW::Agents().Dialog(QuestRewardDialog(IndexToDialogID(favindex[i])));
		}
		ImGui::PopID();
	}

	const int n_dialogs = 15;
	static const char* const dialognames[] = { "Craft fow armor",
		"Prof Change - Warrior",
		"Prof Change - Ranger",
		"Prof Change - Monk",
		"Prof Change - Necro",
		"Prof Change - Mesmer",
		"Prof Change - Elementalist",
		"Prof Change - Assassin",
		"Prof Change - Rirtualist",
		"Prof Change - Paragon",
		"Prof Change - Dervish",
		"Kama -> Docks @ Hahnna",
		"Docks -> Kaineng @ Mhenlo",
		"Docks -> LA Gate @ Mhenlo",
		"LA Gate -> LA @ Neiro" };
	static int dialogindex = 0;
	ImGui::Combo("###dialogcombo", &dialogindex, dialognames, n_dialogs);
	ImGui::SameLine();
	if (ImGui::Button("Send", ImVec2(-1.0f, 0.0f))) {
		GW::Agents().Dialog(IndexToDialogID(dialogindex));
	}

	static bool hex = false;
	static char customdialogbuf[64] = "";
	if (ImGui::Button(hex ? "Hex" : "Dec", ImVec2(32.0f, 0.0f))) {
		try {
			if (hex) { // was hex, convert to dec
				long id = std::stol(customdialogbuf, 0, 16);
				sprintf_s(customdialogbuf, "%d", id);
			} else { // was dec, convert to hex
				long id = std::stol(customdialogbuf, 0, 10);
				sprintf_s(customdialogbuf, "%X", id);
			}
		} catch (...) {}
		hex = !hex;
	}
	ImGui::SameLine();
	ImGuiInputTextFlags flag = (hex ? ImGuiInputTextFlags_CharsHexadecimal : ImGuiInputTextFlags_CharsDecimal);
	ImGui::InputText("###dialoginput", customdialogbuf, 64, flag);
	ImGui::SameLine();
	if (ImGui::Button("Send", ImVec2(-1.0f, 0.0f))) {
		try {
			long id = std::stol(customdialogbuf, 0, hex ? 16 : 10);
			GW::Agents().Dialog(id);
		} catch (...) {}
	}

	ImGui::End();
}

void DialogPanel::LoadSettings(CSimpleIni* ini) {
	for (int i = 0; i < NUM_DIALOG_FAV_QUESTS; ++i) {
		char key[32];
		sprintf_s(key, "Quest%d", i);
		favindex[i] = ini->GetLongValue(Name(), key, 0);
	}
}

void DialogPanel::SaveSettings(CSimpleIni* ini) {
	for (int i = 0; i < NUM_DIALOG_FAV_QUESTS; ++i) {
		char key[32];
		sprintf_s(key, "Quest%d", i);
		ini->SetLongValue(Name(), key, favindex[i]);
	}
}

DWORD DialogPanel::IndexToQuestID(int index) {
	switch (index) {
	case 0: return GW::Constants::QuestID::UW::Chamber;
	case 1: return GW::Constants::QuestID::UW::Wastes;
	case 2: return GW::Constants::QuestID::UW::UWG;
	case 3: return GW::Constants::QuestID::UW::Mnt;
	case 4: return GW::Constants::QuestID::UW::Pits;
	case 5: return GW::Constants::QuestID::UW::Planes;
	case 6: return GW::Constants::QuestID::UW::Pools;
	case 7: return GW::Constants::QuestID::UW::Escort;
	case 8: return GW::Constants::QuestID::UW::Restore;
	case 9: return GW::Constants::QuestID::UW::Vale;
	case 10: return GW::Constants::QuestID::Fow::Defend;
	case 11: return GW::Constants::QuestID::Fow::ArmyOfDarknesses;
	case 12: return GW::Constants::QuestID::Fow::WailingLord;
	case 13: return GW::Constants::QuestID::Fow::Griffons;
	case 14: return GW::Constants::QuestID::Fow::Slaves;
	case 15: return GW::Constants::QuestID::Fow::Restore;
	case 16: return GW::Constants::QuestID::Fow::Hunt;
	case 17: return GW::Constants::QuestID::Fow::Forgemaster;
	case 18: return GW::Constants::QuestID::Fow::Tos;
	case 19: return GW::Constants::QuestID::Fow::Toc;
	case 20: return GW::Constants::QuestID::Fow::Khobay;
	case 21: return GW::Constants::QuestID::Doa::DeathbringerCompany;
	case 22: return GW::Constants::QuestID::Doa::RiftBetweenUs;
	case 23: return GW::Constants::QuestID::Doa::ToTheRescue;
	case 24: return GW::Constants::QuestID::Doa::City;
	case 25: return GW::Constants::QuestID::Doa::BreachingStygianVeil;
	case 26: return GW::Constants::QuestID::Doa::BroodWars;
	case 27: return GW::Constants::QuestID::Doa::FoundryBreakout;
	case 28: return GW::Constants::QuestID::Doa::FoundryOfFailedCreations;
	default: return 0;
	}
}

DWORD DialogPanel::IndexToDialogID(int index) {
	switch (index) {
	case 0: return GW::Constants::DialogID::FowCraftArmor;
	case 1: return GW::Constants::DialogID::ProfChangeWarrior;
	case 2: return GW::Constants::DialogID::ProfChangeRanger;
	case 3: return GW::Constants::DialogID::ProfChangeMonk;
	case 4: return GW::Constants::DialogID::ProfChangeNecro;
	case 5: return GW::Constants::DialogID::ProfChangeMesmer;
	case 6: return GW::Constants::DialogID::ProfChangeEle;
	case 7: return GW::Constants::DialogID::ProfChangeAssassin;
	case 8: return GW::Constants::DialogID::ProfChangeRitualist;
	case 9: return GW::Constants::DialogID::ProfChangeParagon;
	case 10: return GW::Constants::DialogID::ProfChangeDervish;
	case 11: return GW::Constants::DialogID::FerryKamadanToDocks;
	case 12: return GW::Constants::DialogID::FerryDocksToKaineng;
	case 13: return GW::Constants::DialogID::FerryDocksToLA;
	case 14: return GW::Constants::DialogID::FerryGateToLA;
	default: return 0;
	}
}
