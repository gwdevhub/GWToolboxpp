#include "DialogPanel.h"

#include <GWCA\GWCA.h>

#include "Config.h"
#include "MainWindow.h"
#include "GuiUtils.h"
#include "GWToolbox.h"

using namespace OSHGui;
using namespace std;

void DialogPanel::BuildUI() {

	CreateButton(0, 0, 2, L"Four Horseman", QuestAcceptDialog(GW::Constants::QuestID::UW::Planes));
	CreateButton(1, 0, 2, L"Demon Assassin", QuestAcceptDialog(GW::Constants::QuestID::UW::Mnt));
	CreateButton(0, 1, 2, L"Tower of Strength", QuestAcceptDialog(GW::Constants::QuestID::Fow::Tos));
	CreateButton(1, 1, 2, L"Foundry Reward", QuestRewardDialog(GW::Constants::QuestID::Doa::FoundryBreakout));

	CreateButton(0, 2, 4, L"Lab", GW::Constants::DialogID::UwTeleLab);
	CreateButton(1, 2, 4, L"Vale", GW::Constants::DialogID::UwTeleVale);
	CreateButton(2, 2, 4, L"Pits", GW::Constants::DialogID::UwTelePits);
	CreateButton(3, 2, 4, L"Pools", GW::Constants::DialogID::UwTelePools);

	CreateButton(0, 3, 3, L"Planes", GW::Constants::DialogID::UwTelePlanes);
	CreateButton(1, 3, 3, L"Wastes", GW::Constants::DialogID::UwTeleWastes);
	CreateButton(2, 3, 3, L"Mountains", GW::Constants::DialogID::UwTeleMnt);

	for (int i = 0; i < 3; ++i) {
		wstring key = wstring(L"Quest") + to_wstring(i);
		int index = Config::IniReadLong(MainWindow::IniSection(), key.c_str(), 0);
		ComboBox* fav_combo = new ComboBox(this);
		for (int j = 0; j < n_quests; ++j) {
			fav_combo->AddItem(IndexToQuestName(j));
		}
		fav_combo->SetAutoSize(true);
		fav_combo->SetSize(SizeI(GuiUtils::ComputeWidth(GetWidth(), 4, 2), BUTTON_HEIGHT));
		fav_combo->SetLocation(PointI(GuiUtils::ComputeX(GetWidth(), 4, 0),  GuiUtils::ComputeY(i + 4)));
		
		fav_combo->SetSelectedIndex(index);
		fav_combo->GetSelectedIndexChangedEvent() += SelectedIndexChangedEventHandler(
			[fav_combo, key](Control*) {
			Config::IniWriteLong(MainWindow::IniSection(),
				key.c_str(), fav_combo->GetSelectedIndex());
		});
		AddControl(fav_combo);

		Button* take = new Button(this);
		int offset = 12;
		take->SetSize(SizeI(GuiUtils::ComputeWidth(GetWidth(), 4) - offset, BUTTON_HEIGHT));
		take->SetLocation(PointI(GuiUtils::ComputeX(GetWidth(), 4, 2), GuiUtils::ComputeY(i + 4)));
		take->SetText(L"Take");
		take->GetClickEvent() += ClickEventHandler([this, fav_combo](Control*) {
			int index = fav_combo->GetSelectedIndex();
			GW::Agents().Dialog(QuestAcceptDialog(IndexToQuestID(index)));
		});
		AddControl(take);

		Button* complete = new Button(this);
		complete->SetSize(SizeI(GuiUtils::ComputeWidth(GetWidth(), 4) + offset, GuiUtils::ROW_HEIGHT));
		complete->SetLocation(PointI(GuiUtils::ComputeX(GetWidth(), 4, 3) - offset, GuiUtils::ComputeY(i + 4)));
		complete->SetText(L"Complete");
		complete->GetClickEvent() += ClickEventHandler([this, fav_combo](Control*) {
			int index = fav_combo->GetSelectedIndex();
			GW::Agents().Dialog(QuestRewardDialog(IndexToQuestID(index)));
		});
		AddControl(complete);
	}
	
	ComboBox* combo = new ComboBox(this);
	combo->SetLocation(PointI(SPACE, GuiUtils::ComputeY(7)));
	combo->SetSize(SizeI(GuiUtils::ComputeWidth(GetWidth(), 4, 3), GuiUtils::ROW_HEIGHT));
	for (int i = 0; i < n_dialogs; ++i) {
		combo->AddItem(IndexToDialogName(i));
	}
	combo->SetSelectedIndex(0);
	AddControl(combo);

	Button* combo_send = new Button(this);
	combo_send->SetLocation(PointI(GuiUtils::ComputeX(GetWidth(), 4, 3), GuiUtils::ComputeY(7)));
	combo_send->SetSize(SizeI(GuiUtils::ComputeWidth(GetWidth(), 4), GuiUtils::ROW_HEIGHT));
	combo_send->SetText(L"Send");
	combo_send->GetClickEvent() += ClickEventHandler([this, combo](Control*) {
		int index = combo->GetSelectedIndex();
		int id = IndexToDialogID(index);
		GW::Agents().Dialog(id);
	});
	AddControl(combo_send);

	TextBox* textbox = new TextBox(this);
	textbox->SetLocation(PointI(SPACE, GuiUtils::ComputeY(8)));
	textbox->SetSize(SizeI(GuiUtils::ComputeWidth(GetWidth(), 4, 3), GuiUtils::ROW_HEIGHT));
	textbox->GetFocusGotEvent() += FocusGotEventHandler([](Control*) {
		GWToolbox::instance().set_capture_input(true);
	});
	textbox->GetFocusLostEvent() += FocusLostEventHandler([textbox](Control*, Control*) {
		GWToolbox::instance().set_capture_input(false);
		try {
			std::stol(textbox->GetText(), 0, 0);
		} catch (...) {
			textbox->SetText(L"0");
		}
	});
	textbox->GetKeyPressEvent() += KeyPressEventHandler([textbox](Control*,KeyPressEventArgs args){
		if (args.KeyChar == VK_RETURN){
			try {
				long id = std::stol(textbox->GetText(), 0, 0);
				GW::Agents().Dialog(id);
			} catch (...) {}
		}
	});
	AddControl(textbox);

	Button* custom_send = new Button(this);
	custom_send->SetLocation(PointI(GuiUtils::ComputeX(GetWidth(), 4, 3), GuiUtils::ComputeY(8)));
	custom_send->SetSize(SizeI(GuiUtils::ComputeWidth(GetWidth(), 4), BUTTON_HEIGHT));
	custom_send->SetText(L"Send");
	custom_send->GetClickEvent() += ClickEventHandler([textbox](Control*) {
		try {
			long id = std::stol(textbox->GetText(), 0, 0);
			GW::Agents().Dialog(id);
		} catch (...) { }
	});
	AddControl(custom_send);
}

void DialogPanel::CreateButton(int grid_x, int grid_y, int hor_amount,
	std::wstring text, DWORD dialog) {

	Button* button = new Button(this);
	button->SetText(text);
	button->SetSize(SizeI(GuiUtils::ComputeWidth(GetWidth(), hor_amount), BUTTON_HEIGHT));
	button->SetLocation(PointI(GuiUtils::ComputeX(GetWidth(), hor_amount, grid_x), GuiUtils::ComputeY(grid_y)));
	button->GetClickEvent() += ClickEventHandler([dialog](Control*) {
		GW::Agents().Dialog(dialog);
	});
	AddControl(button);
}

std::wstring DialogPanel::IndexToQuestName(int index) {
	switch (index) {
	case 0: return L"UW - Chamber";
	case 1: return L"UW - Wastes";
	case 2: return L"UW - UWG";
	case 3: return L"UW - Mnt";
	case 4: return L"UW - Pits";
	case 5: return L"UW - Planes";
	case 6: return L"UW - Pools";
	case 7: return L"UW - Escort";
	case 8: return L"UW - Restore";
	case 9: return L"UW - Vale";
	case 10: return L"FoW - Defend";
	case 11: return L"FoW - Army Of Darknesses";
	case 12: return L"FoW - WailingLord";
	case 13: return L"FoW - Griffons";
	case 14: return L"FoW - Slaves";
	case 15: return L"FoW - Restore";
	case 16: return L"FoW - Hunt";
	case 17: return L"FoW - Forgemaster";
	case 18: return L"FoW - Tos";
	case 19: return L"FoW - Toc";
	case 20: return L"FoW - Khobay";
	case 21: return L"DoA - Gloom 1: Deathbringer Company";
	case 22: return L"DoA - Gloom 2: Rift Between Us";
	case 23: return L"DoA - Gloom 3: To The Rescue";
	case 24: return L"DoA - City";
	case 25: return L"DoA - Veil 1: Breaching Stygian Veil";
	case 26: return L"DoA - Veil 2: Brood Wars";
	case 27: return L"DoA - Foundry 1: Foundry Breakout";
	case 28: return L"DoA - Foundry 2: Foundry Of Failed Creations";
	default: return L"";
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

wstring DialogPanel::IndexToDialogName(int index) {
	switch (index) {
	case 0: return L"Craft fow armor";
	case 1: return L"Profession Change - W";
	case 2: return L"Profession Change - R";
	case 3: return L"Profession Change - Mo";
	case 4: return L"Profession Change - N";
	case 5: return L"Profession Change - Me";
	case 6: return L"Profession Change - E";
	case 7: return L"Profession Change - A";
	case 8: return L"Profession Change - Rt";
	case 9: return L"Profession Change - P";
	case 10: return L"Profession Change - D";
	case 11: return L"Kama -> Docks @ Hahnna";
	case 12: return L"Docks -> Kaineng @ Mhenlo";
	case 13: return L"Docks -> LA Gate @ Mhenlo";
	case 14: return L"LA Gate -> LA @ Neiro";
	default: return L"";
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
