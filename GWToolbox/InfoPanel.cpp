#include "InfoPanel.h"

#include <string>
#include <cmath>

#include "../API/APIMain.h"
#include "GWToolbox.h"
#include "Config.h"
#include "HealthWindow.h"

using namespace OSHGui;

InfoPanel::InfoPanel() {
}

void InfoPanel::BuildUI() {
	int width = 250;
	int full_item_width = width - 2 * DefaultBorderPadding;
	int half_item_width = (width - 3 * DefaultBorderPadding) / 2;
	int group_height = 42;
	int item_height = 30;
	int item1_x = DefaultBorderPadding;
	int item2_x = half_item_width + 2 * DefaultBorderPadding;
	SetSize(250, 300);

	using namespace OSHGui;

	GroupBox* player = new GroupBox();
	player->SetSize(full_item_width, group_height);
	player->SetLocation(item1_x, DefaultBorderPadding);
	player->SetText(" Player Position ");
	player->SetBackColor(Drawing::Color::Empty());
	AddControl(player);
	Label* xtext = new Label();
	xtext->SetLocation(DefaultBorderPadding, DefaultBorderPadding);
	xtext->SetText("x:");
	player->AddControl(xtext);
	player_x = new Label();
	player_x->SetLocation(xtext->GetRight(), DefaultBorderPadding);
	player->AddControl(player_x);
	Label* ytext = new Label();
	ytext->SetLocation(player->GetWidth() / 2 + DefaultBorderPadding, DefaultBorderPadding);
	ytext->SetText("y:");
	player->AddControl(ytext);
	player_y = new Label();
	player_y->SetLocation(ytext->GetRight(), DefaultBorderPadding);
	player->AddControl(player_y);


	GroupBox* target = new GroupBox();
	target->SetSize(half_item_width, group_height);
	target->SetLocation(item1_x, player->GetBottom() + DefaultBorderPadding);
	target->SetText(" Target ID ");
	target->SetBackColor(Drawing::Color::Empty());
	AddControl(target);
	target_id = new Label();
	target_id->SetLocation(DefaultBorderPadding, DefaultBorderPadding);
	target->AddControl(target_id);

	GroupBox* map = new GroupBox();
	map->SetSize(half_item_width, group_height);
	map->SetLocation(item2_x, player->GetBottom() + DefaultBorderPadding);
	map->SetText(" Map ID ");
	map->SetBackColor(Drawing::Color::Empty());
	AddControl(map);
	map_id = new Label();
	map_id->SetLocation(DefaultBorderPadding, DefaultBorderPadding);
	map->AddControl(map_id);

	GroupBox* item = new GroupBox();
	item->SetSize(half_item_width, group_height);
	item->SetLocation(item1_x, target->GetBottom() + DefaultBorderPadding);
	item->SetText(" First Item ID ");
	item->SetBackColor(Drawing::Color::Empty());
	AddControl(item);
	item_id = new Label();
	item_id->SetLocation(DefaultBorderPadding, DefaultBorderPadding);
	item->AddControl(item_id);

	GroupBox* dialog = new GroupBox();
	dialog->SetSize(half_item_width, group_height);
	dialog->SetLocation(item2_x, target->GetBottom() + DefaultBorderPadding);
	dialog->SetText(" Last Dialog ID ");
	dialog->SetBackColor(Drawing::Color::Empty());
	AddControl(dialog);
	dialog_id = new Label();
	dialog_id->SetLocation(DefaultBorderPadding, DefaultBorderPadding);
	dialog->AddControl(dialog_id);

	CheckBox* bonds = new CheckBox();
	bonds->SetSize(full_item_width, item_height);
	bonds->SetLocation(item1_x, dialog->GetBottom() + DefaultBorderPadding);
	bonds->SetText("Show Bonds Monitor");
	bonds->GetCheckedChangedEvent() += CheckedChangedEventHandler([bonds](Control*) {
		// TODO
	});
	AddControl(bonds);

	CheckBox* targetHp = new CheckBox();
	targetHp->SetSize(full_item_width, item_height);
	targetHp->SetLocation(item1_x, bonds->GetBottom() + DefaultBorderPadding);
	targetHp->SetText("Show Target Health");
	targetHp->GetCheckedChangedEvent() += CheckedChangedEventHandler([targetHp](Control*) {
		GWToolbox* tb = GWToolbox::instance();
		bool show = targetHp->GetChecked();
		if (tb->health_window()) {
			tb->health_window()->Show(show);
		}
		tb->config()->iniWriteBool(HealthWindow::IniSection(), HealthWindow::IniKeyShow(), show);
	});
	targetHp->SetChecked(GWToolbox::instance()->config()->iniReadBool(
		HealthWindow::IniSection(), HealthWindow::IniKeyShow(), false));
	AddControl(targetHp);

	CheckBox* distance = new CheckBox();
	distance->SetSize(full_item_width, item_height);
	distance->SetLocation(item1_x, targetHp->GetBottom() + DefaultBorderPadding);
	distance->SetText("Show Target Distance");
	distance->GetCheckedChangedEvent() += CheckedChangedEventHandler([distance](Control*) {
		// TODO
	});
	AddControl(distance);

	Button* xunlai = new Button();
	xunlai->SetSize(full_item_width, item_height);
	xunlai->SetLocation(item1_x, distance->GetBottom() + DefaultBorderPadding);
	xunlai->SetText("Open Xunlai Chest");
	xunlai->GetClickEvent() += ClickEventHandler([](Control*) {
		if (GWAPI::GWAPIMgr::GetInstance()->Map->GetInstanceType() == GwConstants::InstanceType::Outpost) {
			GWAPI::GWAPIMgr::GetInstance()->Items->OpenXunlaiWindow();
		}
	});
	AddControl(xunlai);

}

void InfoPanel::UpdateUI() {
	using namespace GWAPI::GW;
	using namespace std;

	

	GWAPI::GWAPIMgr* api = GWAPI::GWAPIMgr::GetInstance();
	
	Agent* player = api->Agents->GetPlayer();
	float x = player ? player->X : 0;
	float y = player ? player->Y : 0;
	if (x != current_player_x || y != current_player_y) {
		current_player_x = x;
		current_player_y = y;
		if (player) {
			player_x->SetText(to_string(lroundf(player->X)));
			player_y->SetText(to_string(lroundf(player->Y)));
		} else {
			player_x->SetText(" -");
			player_y->SetText(" -");
		}
	}
	

	Agent* target = api->Agents->GetTarget();
	long id = target ? target->PlayerNumber : 0;
	if (id != current_target_id) {
		if (target) {
			target_id->SetText(to_string(target->PlayerNumber));
		} else {
			target_id->SetText("-");
		}
	}

	if (api->Map->GetMapID() != current_map_id
		|| api->Map->GetInstanceType() != current_map_type) {
		current_map_id = api->Map->GetMapID();
		current_map_type = api->Map->GetInstanceType();
		string map = to_string(static_cast<int>(current_map_id));
		switch (api->Map->GetInstanceType()) {
		case GwConstants::InstanceType::Explorable: break;
		case GwConstants::InstanceType::Loading: map += " (loading)"; break;
		case GwConstants::InstanceType::Outpost: map += " (outpost)"; break;
		}
		map_id->SetText(map);
	}
	

	Bag** bags = api->Items->GetBagArray();
	if (bags) {
		Bag* bag1 = bags[1];
		if (bag1) {
			ItemArray items = bag1->Items;
			if (items.IsValid()) {
				Item* item = items[0];
				long id = item ? item->ModelId : -1;
				if (current_item_id != id) {
					current_item_id = id;
					if (item) {
						item_id->SetText(to_string(item->ModelId));
					} else {
						item_id->SetText(" -");
					}
				}
			}
		}
	}

	if (current_dialog_id != api->Agents->GetLastDialogId()) {
		static char dialogtxt[0x10];
		current_dialog_id = api->Agents->GetLastDialogId();
		sprintf_s(dialogtxt, "0x%X", current_dialog_id);
		dialog_id->SetText(dialogtxt);
	}
}
