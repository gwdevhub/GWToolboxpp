#include "DialogPanel.h"
#include "../API/APIMain.h"
#include "GWToolbox.h"
#include "Config.h"

using namespace OSHGui;
using namespace GWAPI;
using namespace std;

DialogPanel::DialogPanel() {
}

void DialogPanel::BuildUI() {
	SetSize(WIDTH, HEIGHT);

	CreateButton(0, 0, 2, "Four Horseman", QuestAcceptDialog(GwConstants::QuestID::UW::Planes));
	CreateButton(1, 0, 2, "Demon Assassin", QuestAcceptDialog(GwConstants::QuestID::UW::Mnt));
	CreateButton(0, 1, 2, "Tower of Strength", QuestAcceptDialog(GwConstants::QuestID::Fow::Tos));
	CreateButton(1, 1, 2, "Foundry Reward", QuestRewardDialog(GwConstants::QuestID::Doa::FoundryBreakout));

	CreateButton(0, 2, 4, "Lab", GwConstants::DialogID::UwTeleLab);
	CreateButton(1, 2, 4, "Vale", GwConstants::DialogID::UwTeleVale);
	CreateButton(2, 2, 4, "Pits", GwConstants::DialogID::UwTelePits);
	CreateButton(3, 2, 4, "Pools", GwConstants::DialogID::UwTelePools);

	CreateButton(0, 3, 3, "Planes", GwConstants::DialogID::UwTelePlanes);
	CreateButton(1, 3, 3, "Wastes", GwConstants::DialogID::UwTeleWastes);
	CreateButton(2, 3, 3, "Mountains", GwConstants::DialogID::UwTeleMnt);

	for (int i = 0; i < 3; ++i) {
		wstring key = wstring(L"Quest") + to_wstring(i);
		int index = GWToolbox::instance()->config()->iniReadLong(MainWindow::IniSection(), key.c_str(), 0);
		ComboBox* fav_combo = new ComboBox();
		fav_combo->SetSize((WIDTH - 4 * SPACE) * 2 / 4, BUTTON_HEIGHT);
		fav_combo->SetLocation(DefaultBorderPadding, SPACE * 2 + (BUTTON_HEIGHT + SPACE) * (i + 4));
		for (int i = 0; i < n_quests; ++i) {
			fav_combo->AddItem(IndexToQuestName(i));
		}
		fav_combo->SetSelectedIndex(index);
		fav_combo->GetSelectedIndexChangedEvent() += SelectedIndexChangedEventHandler(
			[fav_combo, key](Control*) {
			GWToolbox::instance()->config()->iniWriteLong(MainWindow::IniSection(),
				key.c_str(), fav_combo->GetSelectedIndex());
		});
		AddControl(fav_combo);

		Button* take = new Button();
		take->SetSize((WIDTH - 3 * SPACE) / 4, fav_combo->GetHeight());
		take->SetLocation(fav_combo->GetRight() + SPACE, fav_combo->GetTop());
		take->SetText("Take");
		take->GetClickEvent() += ClickEventHandler([this, fav_combo](Control*) {
			int index = fav_combo->GetSelectedIndex();
			GWAPIMgr::GetInstance()->Agents->Dialog(QuestAcceptDialog(IndexToQuestID(index)));
		});
		AddControl(take);

		Button* complete = new Button();
		complete->SetSize((WIDTH - 3 * SPACE) / 4, fav_combo->GetHeight());
		complete->SetLocation(take->GetRight() + SPACE, fav_combo->GetTop());
		complete->SetText("Complete");
		complete->GetClickEvent() += ClickEventHandler([this, fav_combo](Control*) {
			int index = fav_combo->GetSelectedIndex();
			GWAPIMgr::GetInstance()->Agents->Dialog(QuestRewardDialog(IndexToQuestID(index)));
		});
		AddControl(complete);
	}

	ComboBox* combo = new ComboBox();
	combo->SetLocation(SPACE, SPACE * 3 + (BUTTON_HEIGHT + SPACE) * 7);
	combo->SetSize((WIDTH - SPACE * 3) * 3 / 4, BUTTON_HEIGHT);
	for (int i = 0; i < n_dialogs; ++i) {
		combo->AddItem(IndexToDialogName(i));
	}
	combo->SetSelectedIndex(0);
	AddControl(combo);

	Button* combo_send = new Button();
	combo_send->SetLocation(combo->GetRight() + SPACE, combo->GetTop());
	combo_send->SetSize((WIDTH - 3 * SPACE) / 4, BUTTON_HEIGHT);
	combo_send->SetText("Send");
	combo_send->GetClickEvent() += ClickEventHandler([this, combo](Control*) {
		int index = combo->GetSelectedIndex();
		int id = IndexToDialogID(index);
		GWAPIMgr::GetInstance()->Agents->Dialog(id);
	});
	AddControl(combo_send);

	TextBox* textbox = new TextBox();
	textbox->SetLocation(SPACE, SPACE * 3 + (BUTTON_HEIGHT + SPACE) * 8);
	textbox->SetSize((WIDTH - SPACE * 3) * 3 / 4, BUTTON_HEIGHT);
	textbox->GetFocusGotEvent() += FocusGotEventHandler([](Control*) {
		GWToolbox::instance()->set_capture_input(true);
	});
	textbox->GetFocusLostEvent() += FocusLostEventHandler([textbox](Control*, Control*) {
		GWToolbox::instance()->set_capture_input(false);
		try {
			std::stol(textbox->GetText(), 0, 0);
		} catch (...) {
			textbox->SetText("0");
		}
	});
	textbox->GetKeyPressEvent() += KeyPressEventHandler([textbox](Control*,KeyPressEventArgs args){
		if (args.KeyChar == VK_RETURN){
			try {
				long id = std::stol(textbox->GetText(), 0, 0);
				GWAPIMgr::GetInstance()->Agents->Dialog(id);
			} catch (...) {}
		}
	});
	AddControl(textbox);

	Button* custom_send = new Button();
	custom_send->SetLocation(textbox->GetRight() + SPACE, textbox->GetTop());
	custom_send->SetSize((WIDTH - 3 * SPACE) / 4, BUTTON_HEIGHT);
	custom_send->SetText("Send");
	custom_send->GetClickEvent() += ClickEventHandler([textbox](Control*) {
		try {
			long id = std::stol(textbox->GetText(), 0, 0);
			GWAPIMgr::GetInstance()->Agents->Dialog(id);
		} catch (...) { }
	});
	AddControl(custom_send);
}

void DialogPanel::CreateButton(int grid_x, int grid_y, int hor_amount,
	std::string text, DWORD dialog) {

	const int width = (WIDTH - (hor_amount + 1) * SPACE) / hor_amount;
	const int x = SPACE + grid_x * (width + SPACE);
	const int y = SPACE + grid_y * (BUTTON_HEIGHT + SPACE);

	Button* button = new Button();
	button->SetText(text);
	button->SetSize(width, BUTTON_HEIGHT);
	button->SetLocation(x, y);
	button->GetClickEvent() += ClickEventHandler([dialog](Control*) {
		GWAPIMgr::GetInstance()->Agents->Dialog(dialog);
	});
	AddControl(button);
}

std::string DialogPanel::IndexToQuestName(int index) {
	switch (index) {
	case 0: return "UW - Chamber";
	case 1: return "UW - Wastes";
	case 2: return "UW - UWG";
	case 3: return "UW - Mnt";
	case 4: return "UW - Pits";
	case 5: return "UW - Planes";
	case 6: return "UW - Pools";
	case 7: return "UW - Escort";
	case 8: return "UW - Restore";
	case 9: return "UW - Vale";
	case 10: return "FoW - Defend";
	case 11: return "FoW - Army Of Darknesses";
	case 12: return "FoW - WailingLord";
	case 13: return "FoW - Griffons";
	case 14: return "FoW - Slaves";
	case 15: return "FoW - Restore";
	case 16: return "FoW - Hunt";
	case 17: return "FoW - Forgemaster";
	case 18: return "FoW - Tos";
	case 19: return "FoW - Toc";
	case 20: return "FoW - Khobay";
	case 21: return "DoA - Gloom 1: Deathbringer Company";
	case 22: return "DoA - Gloom 2: Rift Between Us";
	case 23: return "DoA - Gloom 3: To The Rescue";
	case 24: return "DoA - City";
	case 25: return "DoA - Veil 1: Breaching Stygian Veil";
	case 26: return "DoA - Veil 2: Brood Wars";
	case 27: return "DoA - Foundry 1: Foundry Breakout";
	case 28: return "DoA - Foundry 2: Foundry Of Failed Creations";
	default: return "";
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

string DialogPanel::IndexToDialogName(int index) {
	switch (index) {
	case 0: return "Craft fow armor";
	case 1: return "Profession Change - W";
	case 2: return "Profession Change - R";
	case 3: return "Profession Change - Mo";
	case 4: return "Profession Change - N";
	case 5: return "Profession Change - Me";
	case 6: return "Profession Change - E";
	case 7: return "Profession Change - A";
	case 8: return "Profession Change - Rt";
	case 9: return "Profession Change - P";
	case 10: return "Profession Change - D";
	default: return "";
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
