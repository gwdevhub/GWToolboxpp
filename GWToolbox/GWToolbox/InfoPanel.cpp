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
#include "Minimap.h"

using namespace OSHGui;

void InfoPanel::BuildUI() {

	int full_item_width = GuiUtils::ComputeWidth(GetWidth(), 1);
	int half_item_width = GuiUtils::ComputeWidth(GetWidth(), 2);
	int group_height = 42;
	int item1_x = GuiUtils::ComputeX(GetWidth(), 2, 0);
	int item2_x = GuiUtils::ComputeX(GetWidth(), 2, 1);

	using namespace OSHGui;

	GroupBox* player = new GroupBox(this);
	player->SetSize(SizeI(full_item_width, group_height));
	player->SetLocation(PointI(item1_x, Padding));
	player->SetText(L" Player Position ");
	player->SetBackColor(Drawing::Color::Empty());
	AddControl(player);
	Label* xtext = new Label(player->GetContainer());
	xtext->SetLocation(PointI(Padding, Padding));
	xtext->SetText(L"x: ");
	player->AddControl(xtext);
	player_x = new Label(player->GetContainer());
	player_x->SetLocation(PointI(xtext->GetRight(), Padding));
	player->AddControl(player_x);
	Label* ytext = new Label(player->GetContainer());
	ytext->SetLocation(PointI(player->GetWidth() / 2 + Padding, Padding));
	ytext->SetText(L"y: ");
	player->AddControl(ytext);
	player_y = new Label(player->GetContainer());
	player_y->SetLocation(PointI(ytext->GetRight(), Padding));
	player->AddControl(player_y);

	GroupBox* target = new GroupBox(this);
	target->SetSize(SizeI(half_item_width, group_height));
	target->SetLocation(PointI(item1_x, player->GetBottom() + Padding));
	target->SetText(L" Target ID ");
	target->SetBackColor(Drawing::Color::Empty());
	AddControl(target);
	target_id = new Label(target->GetContainer());
	target_id->SetLocation(PointI(Padding, Padding));
	target->AddControl(target_id);

	GroupBox* map = new GroupBox(this);
	map->SetSize(SizeI(half_item_width, group_height));
	map->SetLocation(PointI(item2_x, player->GetBottom() + Padding));
	map->SetText(L" Map ID ");
	map->SetBackColor(Drawing::Color::Empty());
	AddControl(map);
	map_id = new Label(map->GetContainer());
	map_id->SetLocation(PointI(Padding, Padding));
	map->AddControl(map_id);

	GroupBox* item = new GroupBox(this);
	item->SetSize(SizeI(half_item_width, group_height));
	item->SetLocation(PointI(item1_x, target->GetBottom() + Padding));
	item->SetText(L" First Item ID ");
	item->SetBackColor(Drawing::Color::Empty());
	AddControl(item);
	item_id = new Label(item->GetContainer());
	item_id->SetLocation(PointI(Padding, Padding));
	item->AddControl(item_id);

	GroupBox* dialog = new GroupBox(this);
	dialog->SetSize(SizeI(half_item_width, group_height));
	dialog->SetLocation(PointI(item2_x, target->GetBottom() + Padding));
	dialog->SetText(L" Last Dialog ID ");
	dialog->SetBackColor(Drawing::Color::Empty());
	AddControl(dialog);
	dialog_id = new Label(dialog->GetContainer());
	dialog_id->SetLocation(PointI(Padding, Padding));
	dialog->AddControl(dialog_id);

	CheckBox* bonds = new CheckBox(this);
	bonds->SetText(L"Bonds Monitor");
	bonds->SetWidth(half_item_width);
	bonds->SetLocation(PointI(item1_x, dialog->GetBottom() + Padding));
	bonds->SetChecked(GWToolbox::instance().config().IniReadBool(
		BondsWindow::IniSection(), BondsWindow::IniKeyShow(), false));
	bonds->GetCheckedChangedEvent() += CheckedChangedEventHandler([bonds](Control*) {
		GWToolbox& tb = GWToolbox::instance();
		bool show = bonds->GetChecked();
		tb.bonds_window().SetVisible(show);
		tb.config().IniWriteBool(BondsWindow::IniSection(), BondsWindow::IniKeyShow(), show);
	});
	AddControl(bonds);

	CheckBox* targetHp = new CheckBox(this);
	targetHp->SetText(L"Target Health");
	targetHp->SetWidth(half_item_width);
	targetHp->SetLocation(PointI(item1_x, bonds->GetBottom() + Padding));
	targetHp->SetChecked(GWToolbox::instance().config().IniReadBool(
		HealthWindow::IniSection(), HealthWindow::IniKeyShow(), false));
	targetHp->GetCheckedChangedEvent() += CheckedChangedEventHandler([targetHp](Control*) {
		GWToolbox& tb = GWToolbox::instance();
		bool show = targetHp->GetChecked();
		tb.health_window().SetViewable(show);
		tb.config().IniWriteBool(HealthWindow::IniSection(), HealthWindow::IniKeyShow(), show);
	});
	AddControl(targetHp);

	CheckBox* distance = new CheckBox(this);
	distance->SetText(L"Target Distance");
	distance->SetWidth(half_item_width);
	distance->SetLocation(PointI(item1_x, targetHp->GetBottom() + Padding));
	distance->SetChecked(GWToolbox::instance().config().IniReadBool(
		DistanceWindow::IniSection(), DistanceWindow::IniKeyShow(), false));
	distance->GetCheckedChangedEvent() += CheckedChangedEventHandler([distance](Control*) {
		GWToolbox& tb = GWToolbox::instance();
		bool show = distance->GetChecked();
		tb.distance_window().SetViewable(show);
		tb.config().IniWriteBool(DistanceWindow::IniSection(), DistanceWindow::IniKeyShow(), show);
	});
	AddControl(distance);

	CheckBox* damage = new CheckBox(this);
	damage->SetText(L"Party Damage");
	damage->SetWidth(half_item_width);
	damage->SetLocation(PointI(item1_x, distance->GetBottom() + Padding));
	damage->SetChecked(GWToolbox::instance().config().IniReadBool(
		PartyDamage::IniSection(), PartyDamage::InikeyShow(), false));
	damage->GetCheckedChangedEvent() += CheckedChangedEventHandler([damage](Control*) {
		GWToolbox& tb = GWToolbox::instance();
		bool show = damage->GetChecked();
		tb.party_damage().SetVisible(show);
		tb.config().IniWriteBool(PartyDamage::IniSection(), PartyDamage::InikeyShow(), show);
	});
	AddControl(damage);

	CheckBox* minimap = new CheckBox(this);
	minimap->SetText(L"Minimap");
	minimap->SetWidth(half_item_width);
	minimap->SetLocation(PointI(item2_x, dialog->GetBottom() + Padding));
	minimap->SetChecked(GWToolbox::instance().config().IniReadBool(
		Minimap::IniSection(), Minimap::InikeyShow(), false));
	minimap->GetCheckedChangedEvent() += CheckedChangedEventHandler([minimap](Control*) {
		GWToolbox& tb = GWToolbox::instance();
		bool show = minimap->GetChecked();
		tb.minimap().SetVisible(show);
		tb.config().IniWriteBool(Minimap::IniSection(), Minimap::InikeyShow(), show);
	});
	AddControl(minimap);

	Button* xunlai = new Button(this);
	xunlai->SetSize(SizeI(full_item_width, GuiUtils::BUTTON_HEIGHT));
	xunlai->SetLocation(PointI(item1_x, GetHeight() - xunlai->GetHeight() - Padding));
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
			player_x->SetText(to_wstring(lroundf(x)));
			player_y->SetText(to_wstring(lroundf(y)));
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
