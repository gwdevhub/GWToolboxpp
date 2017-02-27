#include "PconPanel.h"

#include <string>
#include <functional>

#include <GWCA\GWCA.h>
#include <GWCA\Managers\MapMgr.h>
#include <GWCA\Managers\PartyMgr.h>
#include <GWCA\Managers\StoCMgr.h>

#include "ChatLogger.h"
#include "GuiUtils.h"
#include "Windows\MainWindow.h"
#include <OtherModules\Resources.h>

using namespace GW::Constants;

PconPanel* PconPanel::Instance() {
	return MainWindow::Instance()->pcon_panel;
}

PconPanel::PconPanel() {
	Resources::Instance()->LoadTextureAsync(&texture, "cupcake.png", "img");

	const float s = 64.0f; // all icons are 64x64

	pcons.push_back(essence = new PconCons("Essence_of_Celerity.png",
		ImVec2(5 / s , 10 / s), ImVec2(46 / s, 51 / s),
		"Essence of Celerity", ItemID::ConsEssence, SkillID::Essence_of_Celerity_item_effect, 5));

	pcons.push_back(grail = new PconCons("Grail_of_Might.png",
		ImVec2(5 / s, 12 / s), ImVec2(49 / s, 56 / s),
		"Grail of Might", ItemID::ConsGrail, SkillID::Grail_of_Might_item_effect, 5));

	pcons.push_back(armor = new PconCons("Armor_of_Salvation.png",
		ImVec2(0 / s, 2 / s), ImVec2(56 / s, 58 / s),
		"Armor of Salvation", ItemID::ConsArmor, SkillID::Armor_of_Salvation_item_effect, 5));

	pcons.push_back(redrock = new PconGeneric("Red_Rock_Candy.png",
		ImVec2(0 / s, 4 / s), ImVec2(52 / s, 56 / s),
		"Red Rock Candy", ItemID::RRC, SkillID::Red_Rock_Candy_Rush, 5));

	pcons.push_back(bluerock = new PconGeneric("Blue_Rock_Candy.png",
		ImVec2(0 / s, 4 / s), ImVec2(52 / s, 56 / s),
		"Blue Rock Candy", ItemID::BRC, SkillID::Blue_Rock_Candy_Rush, 10));

	pcons.push_back(greenrock = new PconGeneric("Green_Rock_Candy.png",
		ImVec2(0 / s, 4 / s), ImVec2(52 / s, 56 / s),
		"Green Rock Candy", ItemID::GRC, SkillID::Green_Rock_Candy_Rush, 15));

	pcons.push_back(egg = new PconGeneric("Golden_Egg.png",
		ImVec2(1 / s, 8 / s), ImVec2(48 / s, 55 / s),
		"Golden Egg", ItemID::Eggs, SkillID::Golden_Egg_skill, 20));

	pcons.push_back(apple = new PconGeneric("Candy_Apple.png",
		ImVec2(0 / s, 7 / s), ImVec2(50 / s, 57 / s),
		"Candy Apple", ItemID::Apples, SkillID::Candy_Apple_skill, 10));

	pcons.push_back(corn = new PconGeneric("Candy_Corn.png",
		ImVec2(5 / s, 10 / s), ImVec2(48 / s, 53 / s),
		"Candy Corn", ItemID::Corns, SkillID::Candy_Corn_skill, 10));

	pcons.push_back(cupcake = new PconGeneric("Birthday_Cupcake.png",
		ImVec2(1 / s, 5 / s), ImVec2(51 / s, 55 / s),
		"Birthday Cupcake", ItemID::Cupcakes, SkillID::Birthday_Cupcake_skill, 10));

	pcons.push_back(pie = new PconGeneric("Slice_of_Pumpkin_Pie.png",
		ImVec2(0 / s, 7 / s), ImVec2(52 / s, 59 / s),
		"Slice of Pumpkin Pie", ItemID::Pies, SkillID::Pie_Induced_Ecstasy, 10));

	pcons.push_back(warsupply = new PconGeneric("War_Supplies.png",
		ImVec2(0 / s, 0 / s), ImVec2(63/s, 63/s),
		"War Supplies", ItemID::Warsupplies, SkillID::Well_Supplied, 20));

	pcons.push_back(alcohol = new PconAlcohol("Dwarven_Ale.png",
		ImVec2(-5 / s, 1 / s), ImVec2(57 / s, 63 / s),
		"Alcohol", 10));

	pcons.push_back(lunars = new PconLunar("Lunar_Fortune.png",
		ImVec2(1 / s, 4 / s), ImVec2(56 / s, 59 / s),
		"Lunar Fortunes", 10));

	pcons.push_back(city = new PconCity("Sugary_Blue_Drink.png",
		ImVec2(0 / s, 1 / s), ImVec2(61 / s, 62 / s),
		"City speedboost", 20));

	pcons.push_back(kabob = new PconGeneric("Drake_Kabob.png",
		ImVec2(0 / s, 0 / s), ImVec2(64 / s, 64 / s),
		"Drake Kabob", ItemID::Kabobs, SkillID::Drake_Skin, 10));

	pcons.push_back(skalesoup = new PconGeneric("Bowl_of_Skalefin_Soup.png",
		ImVec2(2 / s, 5 / s), ImVec2(51 / s, 54 / s),
		"Bowl of Skalefin Soup", ItemID::SkalefinSoup, SkillID::Skale_Vigor, 10));

	pcons.push_back(pahnai = new PconGeneric("Pahnai_Salad.png",
		ImVec2(0 / s, 5 / s), ImVec2(49 / s, 54 / s),
		"Pahnai Salad", ItemID::PahnaiSalad, SkillID::Pahnai_Salad_item_effect, 10));

	for (Pcon* pcon : pcons) {
		pcon->ScanInventory();
	}

	GW::StoC().AddGameServerEvent<GW::Packet::StoC::P023>(
		[](GW::Packet::StoC::P023* pak) -> bool {
		Pcon::player_id = pak->unk1;
		return false;
	});
}

bool PconPanel::DrawTabButton(IDirect3DDevice9* device) {
	bool clicked = ToolboxPanel::DrawTabButton(device);

	ImGui::PushStyleColor(ImGuiCol_Text, enabled ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0, 0, 1));
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
	if (ImGui::Button(enabled ? "Enabled###pconstoggle" : "Disabled###pconstoggle", 
		ImVec2(ImGui::GetWindowContentRegionWidth(), 0))) {
		ToggleEnable();
	}
	ImGui::PopStyleColor();
	ImGui::PopStyleColor();
	return clicked;
}

void PconPanel::Draw(IDirect3DDevice9* device) {
	ImGui::SetNextWindowPosCenter(ImGuiSetCond_FirstUseEver);
	ImGui::Begin(Name(), &visible);
	for (unsigned int i = 0; i < pcons.size(); ++i) {
		if (i % items_per_row > 0) {
			ImGui::SameLine();
		}
		pcons[i]->Draw(device);
	}
	ImGui::End();
}

void PconPanel::Update() {
	if (current_map_type != GW::Map().GetInstanceType()) {
		current_map_type = GW::Map().GetInstanceType();
		scan_inventory_timer = TIMER_INIT();
	}

	if (scan_inventory_timer > 0 && TIMER_DIFF(scan_inventory_timer) > 2000) {
		scan_inventory_timer = 0;

		for (Pcon* pcon : pcons) {
			pcon->ScanInventory();
		}
	}

	if (!enabled) return;

	for (Pcon* pcon : pcons) {
		pcon->Update();
	}
}

bool PconPanel::SetEnabled(bool b) {
	enabled = b;
	if (!MainWindow::Instance()->visible) {
		ChatLogger::Log("Pcons %s", enabled ? "enabled" : "disabled");
	}
	if (tick_with_pcons && GW::Map().GetInstanceType() == GW::Constants::InstanceType::Outpost) {
		GW::Partymgr().Tick(enabled);
	}
	return enabled;
}

void PconPanel::LoadSettingInternal(CSimpleIni* ini) {
	essence->enabled = ini->GetBoolValue(Name(), "essence");
	grail->enabled = ini->GetBoolValue(Name(), "grail");
	armor->enabled = ini->GetBoolValue(Name(), "armor");
	alcohol->enabled = ini->GetBoolValue(Name(), "alcohol");
	redrock->enabled = ini->GetBoolValue(Name(), "redrock");
	bluerock->enabled = ini->GetBoolValue(Name(), "bluerock");
	greenrock->enabled = ini->GetBoolValue(Name(), "greenrock");
	pie->enabled = ini->GetBoolValue(Name(), "pie");
	cupcake->enabled = ini->GetBoolValue(Name(), "cupcake");
	apple->enabled = ini->GetBoolValue(Name(), "apple");
	corn->enabled = ini->GetBoolValue(Name(), "corn");
	egg->enabled = ini->GetBoolValue(Name(), "egg");
	kabob->enabled = ini->GetBoolValue(Name(), "kabob");
	warsupply->enabled = ini->GetBoolValue(Name(), "warsupply");
	lunars->enabled = ini->GetBoolValue(Name(), "lunars");
	skalesoup->enabled = ini->GetBoolValue(Name(), "skalesoup");
	pahnai->enabled = ini->GetBoolValue(Name(), "pahnai");
	city->enabled = ini->GetBoolValue(Name(), "city");
	
	tick_with_pcons = ini->GetBoolValue(Name(), "tick_with_pcons", true);
	items_per_row = ini->GetLongValue(Name(), "items_per_row", 3);
	Pcon::delay = ini->GetLongValue(Name(), "pcons_delay", 5000);
	Pcon::size = (float)ini->GetDoubleValue(Name(), "pconsize", 46.0);
	Pcon::disable_when_not_found = ini->GetBoolValue(Name(), "disable_when_not_found", true);
}

void PconPanel::SaveSettingInternal(CSimpleIni* ini) {
	ini->SetBoolValue(Name(), "essence", essence->enabled);
	ini->SetBoolValue(Name(), "grail", grail->enabled);
	ini->SetBoolValue(Name(), "armor", armor->enabled);
	ini->SetBoolValue(Name(), "alcohol", alcohol->enabled);
	ini->SetBoolValue(Name(), "redrock", redrock->enabled);
	ini->SetBoolValue(Name(), "bluerock", bluerock->enabled);
	ini->SetBoolValue(Name(), "greenrock", greenrock->enabled);
	ini->SetBoolValue(Name(), "pie", pie->enabled);
	ini->SetBoolValue(Name(), "cupcake", cupcake->enabled);
	ini->SetBoolValue(Name(), "apple", apple->enabled);
	ini->SetBoolValue(Name(), "corn", corn->enabled);
	ini->SetBoolValue(Name(), "egg", egg->enabled);
	ini->SetBoolValue(Name(), "kabob", kabob->enabled);
	ini->SetBoolValue(Name(), "warsupply", warsupply->enabled);
	ini->SetBoolValue(Name(), "lunars", lunars->enabled);
	ini->SetBoolValue(Name(), "skalesoup", skalesoup->enabled);
	ini->SetBoolValue(Name(), "pahnai", pahnai->enabled);
	ini->SetBoolValue(Name(), "city", city->enabled);

	ini->SetBoolValue(Name(), "tick_with_pcons", tick_with_pcons);
	ini->SetLongValue(Name(), "items_per_row", items_per_row);
	ini->SetLongValue(Name(), "pcons_delay", Pcon::delay);
	ini->SetDoubleValue(Name(), "pconsize", Pcon::size);
	ini->SetBoolValue(Name(), "disable_when_not_found", Pcon::disable_when_not_found);
}

void PconPanel::DrawSettingInternal() {
	ImGui::Checkbox("Tick with pcons", &tick_with_pcons);
	ImGui::ShowHelp("Enabling or disabling pcons will also Tick or Untick in party list");
	ImGui::SliderInt("Items per row", &items_per_row, 1, 18);
	ImGui::DragFloat("Pcon Size", &Pcon::size, 1.0f, 10.0f, 0.0f);
	ImGui::ShowHelp("Size of each Pcon icon in the interface");
	if (Pcon::size <= 1.0f) Pcon::size = 1.0f;
	ImGui::SliderInt("Pcons delay", &Pcon::delay, 0, 5000);
	ImGui::ShowHelp(
		"This value is used to prevent Toolbox from using the \n"
		"same pcon twice, before it gets activated.\n"
		"Decrease this value if you have good ping and you die a lot.");
	ImGui::Checkbox("Disable when not found", &Pcon::disable_when_not_found);
}
