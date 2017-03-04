#include "PconPanel.h"

#include <string>
#include <functional>

#include <GWCA\GWCA.h>
#include <GWCA\Managers\MapMgr.h>
#include <GWCA\Managers\PartyMgr.h>
#include <GWCA\Managers\StoCMgr.h>
#include <imgui_internal.h>

#include "ChatLogger.h"
#include "GuiUtils.h"
#include "Windows\MainWindow.h"
#include <OtherModules\Resources.h>

using namespace GW::Constants;

void PconPanel::Initialize() {
	ToolboxPanel::Initialize();
	Resources::Instance().LoadTextureAsync(&texture, "cupcake.png", "img");

	const float s = 64.0f; // all icons are 64x64

	pcons.push_back(new PconCons("Essence of Celerity", "essence", "Essence_of_Celerity.png",
		ImVec2(5 / s , 10 / s), ImVec2(46 / s, 51 / s),
		ItemID::ConsEssence, SkillID::Essence_of_Celerity_item_effect, 5));

	pcons.push_back(new PconCons("Grail of Might", "grail", "Grail_of_Might.png",
		ImVec2(5 / s, 12 / s), ImVec2(49 / s, 56 / s),
		ItemID::ConsGrail, SkillID::Grail_of_Might_item_effect, 5));

	pcons.push_back(new PconCons("Armor of Salvation", "armor", "Armor_of_Salvation.png",
		ImVec2(0 / s, 2 / s), ImVec2(56 / s, 58 / s),
		ItemID::ConsArmor, SkillID::Armor_of_Salvation_item_effect, 5));

	pcons.push_back(new PconGeneric("Red Rock Candy", "redrock", "Red_Rock_Candy.png",
		ImVec2(0 / s, 4 / s), ImVec2(52 / s, 56 / s),
		ItemID::RRC, SkillID::Red_Rock_Candy_Rush, 5));

	pcons.push_back(new PconGeneric("Blue Rock Candy", "bluerock", "Blue_Rock_Candy.png",
		ImVec2(0 / s, 4 / s), ImVec2(52 / s, 56 / s),
		ItemID::BRC, SkillID::Blue_Rock_Candy_Rush, 10));

	pcons.push_back(new PconGeneric("Green Rock Candy", "greenrock", "Green_Rock_Candy.png",
		ImVec2(0 / s, 4 / s), ImVec2(52 / s, 56 / s),
		ItemID::GRC, SkillID::Green_Rock_Candy_Rush, 15));

	pcons.push_back(new PconGeneric("Golden Egg", "egg", "Golden_Egg.png",
		ImVec2(1 / s, 8 / s), ImVec2(48 / s, 55 / s),
		ItemID::Eggs, SkillID::Golden_Egg_skill, 20));

	pcons.push_back(new PconGeneric("Candy Apple", "apple", "Candy_Apple.png",
		ImVec2(0 / s, 7 / s), ImVec2(50 / s, 57 / s),
		ItemID::Apples, SkillID::Candy_Apple_skill, 10));

	pcons.push_back(new PconGeneric("Candy Corn", "corn", "Candy_Corn.png",
		ImVec2(5 / s, 10 / s), ImVec2(48 / s, 53 / s),
		ItemID::Corns, SkillID::Candy_Corn_skill, 10));

	pcons.push_back(new PconGeneric("Birthday Cupcake", "cupcake", "Birthday_Cupcake.png",
		ImVec2(1 / s, 5 / s), ImVec2(51 / s, 55 / s),
		ItemID::Cupcakes, SkillID::Birthday_Cupcake_skill, 10));

	pcons.push_back(new PconGeneric("Slice of Pumpkin Pie", "pie", "Slice_of_Pumpkin_Pie.png",
		ImVec2(0 / s, 7 / s), ImVec2(52 / s, 59 / s),
		ItemID::Pies, SkillID::Pie_Induced_Ecstasy, 10));

	pcons.push_back(new PconGeneric("War Supplies", "warsupply", "War_Supplies.png",
		ImVec2(0 / s, 0 / s), ImVec2(63/s, 63/s),
		ItemID::Warsupplies, SkillID::Well_Supplied, 20));

	pcons.push_back(new PconAlcohol("Alcohol", "alcohol", "Dwarven_Ale.png",
		ImVec2(-5 / s, 1 / s), ImVec2(57 / s, 63 / s),
		10));

	pcons.push_back(new PconLunar("Lunar Fortunes", "lunars", "Lunar_Fortune.png",
		ImVec2(1 / s, 4 / s), ImVec2(56 / s, 59 / s),
		10));

	pcons.push_back(new PconCity("City speedboost", "city", "Sugary_Blue_Drink.png",
		ImVec2(0 / s, 1 / s), ImVec2(61 / s, 62 / s),
		20));

	pcons.push_back(new PconGeneric("Drake Kabob", "kabob", "Drake_Kabob.png",
		ImVec2(0 / s, 0 / s), ImVec2(64 / s, 64 / s),
		ItemID::Kabobs, SkillID::Drake_Skin, 10));

	pcons.push_back(new PconGeneric("Bowl of Skalefin Soup", "soup", "Bowl_of_Skalefin_Soup.png",
		ImVec2(2 / s, 5 / s), ImVec2(51 / s, 54 / s),
		ItemID::SkalefinSoup, SkillID::Skale_Vigor, 10));

	pcons.push_back(new PconGeneric("Pahnai Salad", "salad", "Pahnai_Salad.png",
		ImVec2(0 / s, 5 / s), ImVec2(49 / s, 54 / s),
		ItemID::PahnaiSalad, SkillID::Pahnai_Salad_item_effect, 10));

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
	if (!visible) return;
	
	ImGui::SetNextWindowPosCenter(ImGuiSetCond_FirstUseEver);
	if (ImGui::Begin(Name(), &visible)) {
		ImGui::PushStyleColor(ImGuiCol_Text, enabled ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0, 0, 1));
		if (ImGui::Button(enabled ? "Enabled###pconstoggle" : "Disabled###pconstoggle",
			ImVec2(ImGui::GetWindowContentRegionWidth(), 0))) {
			ToggleEnable();
		}
		ImGui::PopStyleColor();
		int j = 0;
		for (unsigned int i = 0; i < pcons.size(); ++i) {
			if (pcons[i]->visible) {
				if (j++ % items_per_row > 0) {
					ImGui::SameLine();
				}
				pcons[i]->Draw(device);
			}
		}
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
	if (GW::Map().GetInstanceType() != GW::Constants::InstanceType::Loading) {
		ImGuiWindow* main = ImGui::FindWindowByName(MainWindow::Instance().Name());
		ImGuiWindow* pcon = ImGui::FindWindowByName(Name());
		if ((pcon == nullptr || pcon->Collapsed || !visible)
			&& (main == nullptr || main->Collapsed || !MainWindow::Instance().visible)) {

			ChatLogger::Log("Pcons %s", enabled ? "enabled" : "disabled");
		}
	}
	if (tick_with_pcons && GW::Map().GetInstanceType() == GW::Constants::InstanceType::Outpost) {
		GW::Partymgr().Tick(enabled);
	}
	return enabled;
}

void PconPanel::LoadSettings(CSimpleIni* ini) {
	ToolboxPanel::LoadSettings(ini);

	for (Pcon* pcon : pcons) {
		pcon->LoadSettings(ini, Name());
	}
	
	tick_with_pcons = ini->GetBoolValue(Name(), "tick_with_pcons", true);
	items_per_row = ini->GetLongValue(Name(), "items_per_row", 3);
	Pcon::pcons_delay = ini->GetLongValue(Name(), "pcons_delay", 5000);
	Pcon::lunar_delay = ini->GetLongValue(Name(), "lunar_delay", 500);
	Pcon::size = (float)ini->GetDoubleValue(Name(), "pconsize", 46.0);
	Pcon::disable_when_not_found = ini->GetBoolValue(Name(), "disable_when_not_found", true);
}

void PconPanel::SaveSettings(CSimpleIni* ini) {
	ToolboxPanel::SaveSettings(ini);

	for (Pcon* pcon : pcons) {
		pcon->SaveSettings(ini, Name());
	}

	ini->SetBoolValue(Name(), "tick_with_pcons", tick_with_pcons);
	ini->SetLongValue(Name(), "items_per_row", items_per_row);
	ini->SetLongValue(Name(), "pcons_delay", Pcon::pcons_delay);
	ini->SetLongValue(Name(), "lunar_delay", Pcon::lunar_delay);
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
	ImGui::SliderInt("Pcons delay", &Pcon::pcons_delay, 100, 5000);
	ImGui::ShowHelp(
		"This value is used to prevent Toolbox from using the \n"
		"same pcon twice, before it gets activated.\n"
		"Decrease this value if you have good ping and you die a lot.");
	ImGui::SliderInt("Lunars delay", &Pcon::lunar_delay, 100, 500);
	ImGui::Checkbox("Disable when not found", &Pcon::disable_when_not_found);
	ImGui::ShowHelp("Toolbx will disable a pcon if it is not found in the inventory");
	if (ImGui::TreeNode("Thresholds")) {
		ImGui::Text("When you have less than this amount:\n-The number in the interface becomes yellow.\n-Warning message is displayed when zoning into outpost.");
		for (Pcon* pcon : pcons) {
			ImGui::SliderInt(pcon->chat, &pcon->threshold, 0, 250);
		}
		ImGui::TreePop();
	}
	if (ImGui::TreeNode("Visibility")) {
		for (Pcon* pcon : pcons) {
			ImGui::Checkbox(pcon->chat, &pcon->visible);
		}
		ImGui::TreePop();
	}
}
