#include "InfoPanel.h"

#include <string>
#include <cmath>

#include "GWCA\APIMain.h"

#include "GWToolbox.h"
#include "Config.h"
#include "HealthWindow.h"
#include "DistanceWindow.h"
#include "BondsWindow.h"
#include "GuiUtils.h"

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
	bonds->SetSize(full_item_width, item_height);
	bonds->SetLocation(item1_x, dialog->GetBottom() + DefaultBorderPadding);
	bonds->SetText(L"Show Bonds Monitor");
	bonds->SetChecked(GWToolbox::instance()->config().iniReadBool(
		BondsWindow::IniSection(), BondsWindow::IniKeyShow(), false));
	bonds->GetCheckedChangedEvent() += CheckedChangedEventHandler([bonds](Control*) {
		GWToolbox* tb = GWToolbox::instance();
		bool show = bonds->GetChecked();
		tb->bonds_window().Show(show);
		tb->config().iniWriteBool(BondsWindow::IniSection(), BondsWindow::IniKeyShow(), show);
	});
	AddControl(bonds);

	CheckBox* targetHp = new CheckBox();
	targetHp->SetSize(full_item_width, item_height);
	targetHp->SetLocation(item1_x, bonds->GetBottom() + DefaultBorderPadding);
	targetHp->SetText(L"Show Target Health");
	targetHp->SetChecked(GWToolbox::instance()->config().iniReadBool(
		HealthWindow::IniSection(), HealthWindow::IniKeyShow(), false));
	targetHp->GetCheckedChangedEvent() += CheckedChangedEventHandler([targetHp](Control*) {
		GWToolbox* tb = GWToolbox::instance();
		bool show = targetHp->GetChecked();
		tb->health_window().Show(show);
		tb->config().iniWriteBool(HealthWindow::IniSection(), HealthWindow::IniKeyShow(), show);
	});
	AddControl(targetHp);

	CheckBox* distance = new CheckBox();
	distance->SetSize(full_item_width, item_height);
	distance->SetLocation(item1_x, targetHp->GetBottom() + DefaultBorderPadding);
	distance->SetText(L"Show Target Distance");
	distance->SetChecked(GWToolbox::instance()->config().iniReadBool(
		DistanceWindow::IniSection(), DistanceWindow::IniKeyShow(), false));
	distance->GetCheckedChangedEvent() += CheckedChangedEventHandler([distance](Control*) {
		GWToolbox* tb = GWToolbox::instance();
		bool show = distance->GetChecked();
		tb->distance_window().Show(show);
		tb->config().iniWriteBool(DistanceWindow::IniSection(), DistanceWindow::IniKeyShow(), show);
	});
	AddControl(distance);

	Button* xunlai = new Button();
	xunlai->SetSize(full_item_width, 30);
	xunlai->SetLocation(item1_x, GetHeight() - xunlai->GetHeight() - DefaultBorderPadding);
	xunlai->SetText(L"Open Xunlai Chest");
	xunlai->GetClickEvent() += ClickEventHandler([](Control*) {
		if (GWCA::Api().Map().GetInstanceType() == GwConstants::InstanceType::Outpost) {
			GWCA::Api().Items().OpenXunlaiWindow();
		}
	});
	AddControl(xunlai);
}

void InfoPanel::UpdateUI() {
	using namespace GWAPI::GW;
	using namespace std;

	

	GWAPI::GWCA api;
	
	Agent* player = api().Agents().GetPlayer();
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

	Agent* target = api().Agents().GetTarget();
	long id = target ? target->PlayerNumber : 0;
	if (id != current_target_id) {
		if (target) {
			target_id->SetText(to_wstring(target->PlayerNumber));
		} else {
			target_id->SetText(L"-");
		}
	}

	if (api().Map().GetMapID() != current_map_id
		|| api().Map().GetInstanceType() != current_map_type) {
		current_map_id = api().Map().GetMapID();
		current_map_type = api().Map().GetInstanceType();
		wstring map = to_wstring(static_cast<int>(current_map_id));
		switch (api().Map().GetInstanceType()) {
		case GwConstants::InstanceType::Explorable: break;
		case GwConstants::InstanceType::Loading: map += L" (loading)"; break;
		case GwConstants::InstanceType::Outpost: map += L" (outpost)"; break;
		}
		map_id->SetText(map);
	}
	

	Bag** bags = api().Items().GetBagArray();
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

	if (current_dialog_id != api().Agents().GetLastDialogId()) {
		static wchar_t dialogtxt[0x10];
		current_dialog_id = api().Agents().GetLastDialogId();
		swprintf_s(dialogtxt, sizeof(wchar_t) * 0x10, L"0x%X", current_dialog_id);
		dialog_id->SetText(dialogtxt);
	}
}
