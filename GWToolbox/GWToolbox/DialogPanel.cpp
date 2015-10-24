#include "DialogPanel.h"

#include "GWCA\APIMain.h"

#include "GWToolbox.h"
#include "Config.h"
#include "MainWindow.h"
#include "GuiUtils.h"

using namespace OSHGui;
using namespace GWAPI;
using namespace std;

DialogPanel::DialogPanel() {
}

void DialogPanel::BuildUI() {

	CreateButton(0, 0, 2, L"Four Horseman", QuestAcceptDialog(GwConstants::QuestID::UW::Planes));
	CreateButton(1, 0, 2, L"Demon Assassin", QuestAcceptDialog(GwConstants::QuestID::UW::Mnt));
	CreateButton(0, 1, 2, L"Tower of Strength", QuestAcceptDialog(GwConstants::QuestID::Fow::Tos));
	CreateButton(1, 1, 2, L"Foundry Reward", QuestRewardDialog(GwConstants::QuestID::Doa::FoundryBreakout));

	CreateButton(0, 2, 4, L"Lab", GwConstants::DialogID::UwTeleLab);
	CreateButton(1, 2, 4, L"Vale", GwConstants::DialogID::UwTeleVale);
	CreateButton(2, 2, 4, L"Pits", GwConstants::DialogID::UwTelePits);
	CreateButton(3, 2, 4, L"Pools", GwConstants::DialogID::UwTelePools);

	CreateButton(0, 3, 3, L"Planes", GwConstants::DialogID::UwTelePlanes);
	CreateButton(1, 3, 3, L"Wastes", GwConstants::DialogID::UwTeleWastes);
	CreateButton(2, 3, 3, L"Mountains", GwConstants::DialogID::UwTeleMnt);

	for (int i = 0; i < 3; ++i) {
		wstring key = wstring(L"Quest") + to_wstring(i);
		int index = GWToolbox::instance()->config().iniReadLong(MainWindow::IniSection(), key.c_str(), 0);
		ComboBox* fav_combo = new ComboBox();
		fav_combo->SetSize(GuiUtils::ComputeWidth(GetWidth(), 4, 2), BUTTON_HEIGHT);
		fav_combo->SetLocation(GuiUtils::ComputeX(GetWidth(), 4, 0),  GuiUtils::ComputeY(i + 4));
		for (int j = 0; j < n_quests; ++j) {
			fav_combo->AddItem(IndexToQuestName(j));
		}
		fav_combo->SetSelectedIndex(index);
		fav_combo->GetSelectedIndexChangedEvent() += SelectedIndexChangedEventHandler(
			[fav_combo, key](Control*) {
			GWToolbox::instance()->config().iniWriteLong(MainWindow::IniSection(),
				key.c_str(), fav_combo->GetSelectedIndex());
		});
		AddControl(fav_combo);

		Button* take = new Button();
		take->SetSize(GuiUtils::ComputeWidth(GetWidth(), 4), BUTTON_HEIGHT);
		take->SetLocation(GuiUtils::ComputeX(GetWidth(), 4, 2), GuiUtils::ComputeY(i + 4));
		take->SetText(L"Take");
		take->GetClickEvent() += ClickEventHandler([this, fav_combo](Control*) {
			int index = fav_combo->GetSelectedIndex();
			GWCA::Api().Agents().Dialog(QuestAcceptDialog(IndexToQuestID(index)));
		});
		AddControl(take);

		Button* complete = new Button();
		complete->SetSize(GuiUtils::ComputeWidth(GetWidth(), 4), GuiUtils::ROW_HEIGHT);
		complete->SetLocation(GuiUtils::ComputeX(GetWidth(), 4, 3), GuiUtils::ComputeY(i + 4));
		complete->SetText(L"Complete");
		complete->GetClickEvent() += ClickEventHandler([this, fav_combo](Control*) {
			int index = fav_combo->GetSelectedIndex();
			GWCA::Api().Agents().Dialog(QuestRewardDialog(IndexToQuestID(index)));
		});
		AddControl(complete);
	}
	
	ComboBox* combo = new ComboBox();
	combo->SetLocation(SPACE, GuiUtils::ComputeY(7));
	combo->SetSize(GuiUtils::ComputeWidth(GetWidth(), 4, 3), GuiUtils::ROW_HEIGHT);
	for (int i = 0; i < n_dialogs; ++i) {
		combo->AddItem(IndexToDialogName(i));
	}
	combo->SetSelectedIndex(0);
	AddControl(combo);

	Button* combo_send = new Button();
	combo_send->SetLocation(GuiUtils::ComputeX(GetWidth(), 4, 3), GuiUtils::ComputeY(7));
	combo_send->SetSize(GuiUtils::ComputeWidth(GetWidth(), 4), GuiUtils::ROW_HEIGHT);
	combo_send->SetText(L"Send");
	combo_send->GetClickEvent() += ClickEventHandler([this, combo](Control*) {
		int index = combo->GetSelectedIndex();
		int id = IndexToDialogID(index);
		GWCA::Api().Agents().Dialog(id);
	});
	AddControl(combo_send);

	TextBox* textbox = new TextBox();
	textbox->SetLocation(SPACE, GuiUtils::ComputeY(8));
	textbox->SetSize(GuiUtils::ComputeWidth(GetWidth(), 4, 3), GuiUtils::ROW_HEIGHT);
	textbox->GetFocusGotEvent() += FocusGotEventHandler([](Control*) {
		GWToolbox::instance()->set_capture_input(true);
	});
	textbox->GetFocusLostEvent() += FocusLostEventHandler([textbox](Control*, Control*) {
		GWToolbox::instance()->set_capture_input(false);
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
				GWCA::Api().Agents().Dialog(id);
			} catch (...) {}
		}
	});
	AddControl(textbox);

	Button* custom_send = new Button();
	custom_send->SetLocation(GuiUtils::ComputeX(GetWidth(), 4, 3), GuiUtils::ComputeY(8));
	custom_send->SetSize(GuiUtils::ComputeWidth(GetWidth(), 4), BUTTON_HEIGHT);
	custom_send->SetText(L"Send");
	custom_send->GetClickEvent() += ClickEventHandler([textbox](Control*) {
		try {
			long id = std::stol(textbox->GetText(), 0, 0);
			GWCA::Api().Agents().Dialog(id);
		} catch (...) { }
	});
	AddControl(custom_send);
}

void DialogPanel::CreateButton(int grid_x, int grid_y, int hor_amount,
	std::wstring text, DWORD dialog) {

	Button* button = new Button();
	button->SetText(text);
	button->SetSize(GuiUtils::ComputeWidth(GetWidth(), hor_amount), BUTTON_HEIGHT);
	button->SetLocation(GuiUtils::ComputeX(GetWidth(), hor_amount, grid_x), GuiUtils::ComputeY(grid_y));
	button->GetClickEvent() += ClickEventHandler([dialog](Control*) {
		GWCA::Api().Agents().Dialog(dialog);
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
	case 0: return GwConstants::QuestID::UW::Chamber;
	case 1: return GwConstants::QuestID::UW::Wastes;
	case 2: return GwConstants::QuestID::UW::UWG;
	case 3: return GwConstants::QuestID::UW::Mnt;
	case 4: return GwConstants::QuestID::UW::Pits;
	case 5: return GwConstants::QuestID::UW::Planes;
	case 6: return GwConstants::QuestID::UW::Pools;
	case 7: return GwConstants::QuestID::UW::Escort;
	case 8: return GwConstants::QuestID::UW::Restore;
	case 9: return GwConstants::QuestID::UW::Vale;
	case 10: return GwConstants::QuestID::Fow::Defend;
	case 11: return GwConstants::QuestID::Fow::ArmyOfDarknesses;
	case 12: return GwConstants::QuestID::Fow::WailingLord;
	case 13: return GwConstants::QuestID::Fow::Griffons;
	case 14: return GwConstants::QuestID::Fow::Slaves;
	case 15: return GwConstants::QuestID::Fow::Restore;
	case 16: return GwConstants::QuestID::Fow::Hunt;
	case 17: return GwConstants::QuestID::Fow::Forgemaster;
	case 18: return GwConstants::QuestID::Fow::Tos;
	case 19: return GwConstants::QuestID::Fow::Toc;
	case 20: return GwConstants::QuestID::Fow::Khobay;
	case 21: return GwConstants::QuestID::Doa::DeathbringerCompany;
	case 22: return GwConstants::QuestID::Doa::RiftBetweenUs;
	case 23: return GwConstants::QuestID::Doa::ToTheRescue;
	case 24: return GwConstants::QuestID::Doa::City;
	case 25: return GwConstants::QuestID::Doa::BreachingStygianVeil;
	case 26: return GwConstants::QuestID::Doa::BroodWars;
	case 27: return GwConstants::QuestID::Doa::FoundryBreakout;
	case 28: return GwConstants::QuestID::Doa::FoundryOfFailedCreations;
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
	default: return L"";
	}
}

DWORD DialogPanel::IndexToDialogID(int index) {
	switch (index) {
	case 0: return GwConstants::DialogID::FowCraftArmor;
	case 1: return GwConstants::DialogID::ProfChangeWarrior;
	case 2: return GwConstants::DialogID::ProfChangeRanger;
	case 3: return GwConstants::DialogID::ProfChangeMonk;
	case 4: return GwConstants::DialogID::ProfChangeNecro;
	case 5: return GwConstants::DialogID::ProfChangeMesmer;
	case 6: return GwConstants::DialogID::ProfChangeEle;
	case 7: return GwConstants::DialogID::ProfChangeAssassin;
	case 8: return GwConstants::DialogID::ProfChangeRitualist;
	case 9: return GwConstants::DialogID::ProfChangeParagon;
	case 10: return GwConstants::DialogID::ProfChangeDervish;
	default: return 0;
	}
}
