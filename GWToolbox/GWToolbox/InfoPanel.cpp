#include "InfoPanel.h"

#include <string>
#include <cmath>

#include <GWCA\GWCA.h>
#include <GWCA\ItemMgr.h>

#include "GWToolbox.h"
#include "Config.h"
#include "GuiUtils.h"

#include "HealthWindow.h"
#include "DistanceWindow.h"
#include "BondsWindow.h"
#include "PartyDamage.h"

using namespace OSHGui;

InfoPanel::InfoPanel() {
}

void InfoPanel::BuildUI() {

	int full_item_width = GetWidth() - 2 * DefaultBorderPadding;
	int half_item_width = (GetWidth() - 3 * DefaultBorderPadding) / 2;
	int group_height = 42;
	int item_height = 25;
	int item1_x = DefaultBorderPadding;
	int item2_x = half_item_width + 2 * DefaultBorderPadding;

	using namespace OSHGui;

	GroupBox* player = new GroupBox();
	player->SetSize(full_item_width, group_height);
	player->SetLocation(item1_x, DefaultBorderPadding);
	player->SetText(L" Player Position ");
	player->SetBackColor(Drawing::Color::Empty());
	AddControl(player);
	Label* xtext = new Label();
	xtext->SetLocation(DefaultBorderPadding, DefaultBorderPadding);
	xtext->SetText(L"x:");
	player->AddControl(xtext);
	player_x = new Label();
	player_x->SetLocation(xtext->GetRight(), DefaultBorderPadding);
	player->AddControl(player_x);
	Label* ytext = new Label();
	ytext->SetLocation(player->GetWidth() / 2 + DefaultBorderPadding, DefaultBorderPadding);
	ytext->SetText(L"y:");
	player->AddControl(ytext);
	player_y = new Label();
	player_y->SetLocation(ytext->GetRight(), DefaultBorderPadding);
	player->AddControl(player_y);


	GroupBox* target = new GroupBox();
	target->SetSize(half_item_width, group_height);
	target->SetLocation(item1_x, player->GetBottom() + DefaultBorderPadding);
	target->SetText(L" Target ID ");
	target->SetBackColor(Drawing::Color::Empty());
	AddControl(target);
	target_id = new Label();
	target_id->SetLocation(DefaultBorderPadding, DefaultBorderPadding);
	target->AddControl(target_id);

	GroupBox* map = new GroupBox();
	map->SetSize(half_item_width, group_height);
	map->SetLocation(item2_x, player->GetBottom() + DefaultBorderPadding);
	map->SetText(L" Map ID ");
	map->SetBackColor(Drawing::Color::Empty());
	AddControl(map);
	map_id = new Label();
	map_id->SetLocation(DefaultBorderPadding, DefaultBorderPadding);
	map->AddControl(map_id);

	GroupBox* item = new GroupBox();
	item->SetSize(half_item_width, group_height);
	item->SetLocation(item1_x, target->GetBottom() + DefaultBorderPadding);
	item->SetText(L" First Item ID ");
	item->SetBackColor(Drawing::Color::Empty());
	AddControl(item);
	item_id = new Label();
	item_id->SetLocation(DefaultBorderPadding, DefaultBorderPadding);
	item->AddControl(item_id);

	GroupBox* dialog = new GroupBox();
	dialog->SetSize(half_item_width, group_height);
	dialog->SetLocation(item2_x, target->GetBottom() + DefaultBorderPadding);
	dialog->SetText(L" Last Dialog ID ");
	dialog->SetBackColor(Drawing::Color::Empty());
	AddControl(dialog);
	dialog_id = new Label();
	dialog_id->SetLocation(DefaultBorderPadding, DefaultBorderPadding);
	dialog->AddControl(dialog_id);

	CheckBox* bonds = new CheckBox();
	bonds->SetSize(half_item_width, item_height);
	bonds->SetLocation(item1_x, dialog->GetBottom() + DefaultBorderPadding);
	bonds->SetText(L"Bonds Monitor");
	bonds->SetChecked(GWToolbox::instance().config().IniReadBool(
		BondsWindow::IniSection(), BondsWindow::IniKeyShow(), false));
	bonds->GetCheckedChangedEvent() += CheckedChangedEventHandler([bonds](Control*) {
		GWToolbox& tb = GWToolbox::instance();
		bool show = bonds->GetChecked();
		tb.bonds_window().ShowWindow(show);
		tb.config().IniWriteBool(BondsWindow::IniSection(), BondsWindow::IniKeyShow(), show);
	});
	AddControl(bonds);

	CheckBox* targetHp = new CheckBox();
	targetHp->SetSize(half_item_width, item_height);
	targetHp->SetLocation(item1_x, bonds->GetBottom() + DefaultBorderPadding);
	targetHp->SetText(L"Target Health");
	targetHp->SetChecked(GWToolbox::instance().config().IniReadBool(
		HealthWindow::IniSection(), HealthWindow::IniKeyShow(), false));
	targetHp->GetCheckedChangedEvent() += CheckedChangedEventHandler([targetHp](Control*) {
		GWToolbox& tb = GWToolbox::instance();
		bool show = targetHp->GetChecked();
		tb.health_window().ShowWindow(show);
		tb.config().IniWriteBool(HealthWindow::IniSection(), HealthWindow::IniKeyShow(), show);
	});
	AddControl(targetHp);

	CheckBox* distance = new CheckBox();
	distance->SetSize(half_item_width, item_height);
	distance->SetLocation(item1_x, targetHp->GetBottom() + DefaultBorderPadding);
	distance->SetText(L"Target Distance");
	distance->SetChecked(GWToolbox::instance().config().IniReadBool(
		DistanceWindow::IniSection(), DistanceWindow::IniKeyShow(), false));
	distance->GetCheckedChangedEvent() += CheckedChangedEventHandler([distance](Control*) {
		GWToolbox& tb = GWToolbox::instance();
		bool show = distance->GetChecked();
		tb.distance_window().ShowWindow(show);
		tb.config().IniWriteBool(DistanceWindow::IniSection(), DistanceWindow::IniKeyShow(), show);
	});
	AddControl(distance);

	CheckBox* damage = new CheckBox();
	damage->SetSize(half_item_width, item_height);
	damage->SetLocation(item1_x, distance->GetBottom() + DefaultBorderPadding);
	damage->SetText(L"Party Damage");
	damage->SetChecked(GWToolbox::instance().config().IniReadBool(
		PartyDamage::IniSection(), PartyDamage::InikeyShow(), false));
	damage->GetCheckedChangedEvent() += CheckedChangedEventHandler([damage](Control*) {
		GWToolbox& tb = GWToolbox::instance();
		bool show = damage->GetChecked();
		tb.party_damage().ShowWindow(show);
		tb.config().IniWriteBool(PartyDamage::IniSection(), PartyDamage::InikeyShow(), show);
	});
	AddControl(damage);

	CheckBox* timestamps = new CheckBox();
	timestamps->SetSize(half_item_width, item_height);
	timestamps->SetLocation(item2_x, bonds->GetTop());
	timestamps->SetText(L"Timestamps");
	timestamps->SetChecked(GWToolbox::instance().config().IniReadBool(
		MainWindow::IniSection(), MainWindow::IniKeyTimestamps(), true));
	timestamps->GetCheckedChangedEvent() += CheckedChangedEventHandler([timestamps](Control*) {
		bool active = timestamps->GetChecked();
		GWCA::Chat().ToggleTimeStamp(active);
		GWToolbox::instance().config().IniWriteBool(MainWindow::IniSection(), MainWindow::IniKeyTimestamps(), active);
	});
	AddControl(timestamps);

	Button* xunlai = new Button();
	xunlai->SetSize(full_item_width, 30);
	xunlai->SetLocation(item1_x, GetHeight() - xunlai->GetHeight() - DefaultBorderPadding);
	xunlai->SetText(L"Open Xunlai Chest");
	xunlai->GetClickEvent() += ClickEventHandler([](Control*) {
		if (GWCA::Map().GetInstanceType() == GwConstants::InstanceType::Outpost) {
			GWCA::Items().OpenXunlaiWindow();
		}
	});
	AddControl(xunlai);
}

void InfoPanel::UpdateUI() {
	using namespace GWCA::GW;
	using namespace std;
	
	Agent* player = GWCA::Agents().GetPlayer();
	float x = player ? player->X : 0;
	float y = player ? player->Y : 0;
	if (x != current_player_x || y != current_player_y) {
		current_player_x = x;
		current_player_y = y;
		if (player) {
			player_x->SetText(to_wstring(lroundf(player->X)));
			player_y->SetText(to_wstring(lroundf(player->Y)));
		} else {
			player_x->SetText(L" -");
			player_y->SetText(L" -");
		}
	}

	Agent* target = GWCA::Agents().GetTarget();
	long id = target ? target->PlayerNumber : 0;
	if (id != current_target_id) {
		if (target) {
			target_id->SetText(to_wstring(target->PlayerNumber));
		} else {
			target_id->SetText(L"-");
		}
	}

	if (GWCA::Map().GetMapID() != current_map_id
		|| GWCA::Map().GetInstanceType() != current_map_type) {
		current_map_id = GWCA::Map().GetMapID();
		current_map_type = GWCA::Map().GetInstanceType();
		wstring map = to_wstring(static_cast<int>(current_map_id));
		switch (GWCA::Map().GetInstanceType()) {
		case GwConstants::InstanceType::Explorable: break;
		case GwConstants::InstanceType::Loading: map += L" (loading)"; break;
		case GwConstants::InstanceType::Outpost: map += L" (outpost)"; break;
		}
		map_id->SetText(map);
	}
	

	Bag** bags = GWCA::Items().GetBagArray();
	if (bags) {
		Bag* bag1 = bags[1];
		if (bag1) {
			ItemArray items = bag1->Items;
			if (items.valid()) {
				Item* item = items[0];
				long id = item ? item->ModelId : -1;
				if (current_item_id != id) {
					current_item_id = id;
					if (item) {
						item_id->SetText(to_wstring(item->ModelId));
					} else {
						item_id->SetText(L" -");
					}
				}
			}
		}
	}

	if (current_dialog_id != GWCA::Agents().GetLastDialogId()) {
		static wchar_t dialogtxt[0x10];
		current_dialog_id = GWCA::Agents().GetLastDialogId();
		swprintf_s(dialogtxt, sizeof(wchar_t) * 0x10, L"0x%X", current_dialog_id);
		dialog_id->SetText(dialogtxt);
	}
}
