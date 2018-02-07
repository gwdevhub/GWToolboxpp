#include "PconsWindow.h"

#include <string>
#include <functional>

#include <GWCA\Managers\AgentMgr.h>
#include <GWCA\Managers\MapMgr.h>
#include <GWCA\Managers\PartyMgr.h>
#include <GWCA\Managers\StoCMgr.h>
#include <GWCA\Managers\ChatMgr.h>
#include <imgui_internal.h>

#include <logger.h>
#include "GuiUtils.h"
#include "Windows\MainWindow.h"
#include <Modules\Resources.h>

using namespace GW::Constants;

void PconsWindow::Initialize() {
	ToolboxWindow::Initialize();
	Resources::Instance().LoadTextureAsync(&button_texture, Resources::GetPath("img/icons", "cupcake.png"), IDB_Icon_Cupcake);

	const float s = 64.0f; // all icons are 64x64

	pcons.push_back(new PconCons("Essence of Celerity", "essence", "Essence_of_Celerity.png", IDB_Pcons_Essence,
		ImVec2(5 / s , 10 / s), ImVec2(46 / s, 51 / s),
		ItemID::ConsEssence, SkillID::Essence_of_Celerity_item_effect, 5));

	pcons.push_back(new PconCons("Grail of Might", "grail", "Grail_of_Might.png", IDB_Pcons_Grail,
		ImVec2(5 / s, 12 / s), ImVec2(49 / s, 56 / s),
		ItemID::ConsGrail, SkillID::Grail_of_Might_item_effect, 5));

	pcons.push_back(new PconCons("Armor of Salvation", "armor", "Armor_of_Salvation.png", IDB_Pcons_Armor,
		ImVec2(0 / s, 2 / s), ImVec2(56 / s, 58 / s),
		ItemID::ConsArmor, SkillID::Armor_of_Salvation_item_effect, 5));

	pcons.push_back(new PconGeneric("Red Rock Candy", "redrock", "Red_Rock_Candy.png", IDB_Pcons_RedRock,
		ImVec2(0 / s, 4 / s), ImVec2(52 / s, 56 / s),
		ItemID::RRC, SkillID::Red_Rock_Candy_Rush, 5));

	pcons.push_back(new PconGeneric("Blue Rock Candy", "bluerock", "Blue_Rock_Candy.png", IDB_Pcons_BlueRock,
		ImVec2(0 / s, 4 / s), ImVec2(52 / s, 56 / s),
		ItemID::BRC, SkillID::Blue_Rock_Candy_Rush, 10));

	pcons.push_back(new PconGeneric("Green Rock Candy", "greenrock", "Green_Rock_Candy.png", IDB_Pcons_GreenRock,
		ImVec2(0 / s, 4 / s), ImVec2(52 / s, 56 / s),
		ItemID::GRC, SkillID::Green_Rock_Candy_Rush, 15));

	pcons.push_back(new PconGeneric("Golden Egg", "egg", "Golden_Egg.png", IDB_Pcons_Egg,
		ImVec2(1 / s, 8 / s), ImVec2(48 / s, 55 / s),
		ItemID::Eggs, SkillID::Golden_Egg_skill, 20));

	pcons.push_back(new PconGeneric("Candy Apple", "apple", "Candy_Apple.png", IDB_Pcons_Apple,
		ImVec2(0 / s, 7 / s), ImVec2(50 / s, 57 / s),
		ItemID::Apples, SkillID::Candy_Apple_skill, 10));

	pcons.push_back(new PconGeneric("Candy Corn", "corn", "Candy_Corn.png", IDB_Pcons_Corn,
		ImVec2(5 / s, 10 / s), ImVec2(48 / s, 53 / s),
		ItemID::Corns, SkillID::Candy_Corn_skill, 10));

	pcons.push_back(new PconGeneric("Birthday Cupcake", "cupcake", "Birthday_Cupcake.png", IDB_Pcons_Cupcake,
		ImVec2(1 / s, 5 / s), ImVec2(51 / s, 55 / s),
		ItemID::Cupcakes, SkillID::Birthday_Cupcake_skill, 10));

	pcons.push_back(new PconGeneric("Slice of Pumpkin Pie", "pie", "Slice_of_Pumpkin_Pie.png", IDB_Pcons_Pie,
		ImVec2(0 / s, 7 / s), ImVec2(52 / s, 59 / s),
		ItemID::Pies, SkillID::Pie_Induced_Ecstasy, 10));

	pcons.push_back(new PconGeneric("War Supplies", "warsupply", "War_Supplies.png", IDB_Pcons_WarSupplies,
		ImVec2(0 / s, 0 / s), ImVec2(63/s, 63/s),
		ItemID::Warsupplies, SkillID::Well_Supplied, 20));

	pcons.push_back(pcon_alcohol = new PconAlcohol("Alcohol", "alcohol", "Dwarven_Ale.png", IDB_Pcons_Ale,
		ImVec2(-5 / s, 1 / s), ImVec2(57 / s, 63 / s),
		10));

	pcons.push_back(new PconLunar("Lunar Fortunes", "lunars", "Lunar_Fortune.png", IDB_Pcons_Lunar,
		ImVec2(1 / s, 4 / s), ImVec2(56 / s, 59 / s),
		10));

	pcons.push_back(new PconCity("City speedboost", "city", "Sugary_Blue_Drink.png", IDB_Pcons_BlueDrink,
		ImVec2(0 / s, 1 / s), ImVec2(61 / s, 62 / s),
		20));

	pcons.push_back(new PconGeneric("Drake Kabob", "kabob", "Drake_Kabob.png", IDB_Pcons_Kabob,
		ImVec2(0 / s, 0 / s), ImVec2(64 / s, 64 / s),
		ItemID::Kabobs, SkillID::Drake_Skin, 10));

	pcons.push_back(new PconGeneric("Bowl of Skalefin Soup", "soup", "Bowl_of_Skalefin_Soup.png", IDB_Pcons_Soup,
		ImVec2(2 / s, 5 / s), ImVec2(51 / s, 54 / s),
		ItemID::SkalefinSoup, SkillID::Skale_Vigor, 10));

	pcons.push_back(new PconGeneric("Pahnai Salad", "salad", "Pahnai_Salad.png", IDB_Pcons_Salad,
		ImVec2(0 / s, 5 / s), ImVec2(49 / s, 54 / s),
		ItemID::PahnaiSalad, SkillID::Pahnai_Salad_item_effect, 10));

	for (Pcon* pcon : pcons) {
		pcon->ScanInventory();
	}

	GW::StoC::AddCallback<GW::Packet::StoC::P023>(
		[](GW::Packet::StoC::P023* pak) -> bool {
		Pcon::player_id = pak->unk1;
		return false;
	});
	GW::StoC::AddCallback<GW::Packet::StoC::P053_AddExternalBond>(
		[](GW::Packet::StoC::P053_AddExternalBond* pak) -> bool {
		if (PconAlcohol::suppress_lunar_skills
			&& pak->caster_id == GW::Agents::GetPlayerId()
			&& pak->receiver_id == 0
			&& (pak->skill_id == (DWORD)GW::Constants::SkillID::Spiritual_Possession
				|| pak->skill_id == (DWORD)GW::Constants::SkillID::Lucky_Aura)) {
			//printf("blocked skill %d\n", pak->skill_id);
			return true;
		}
		return false;
	});
	GW::StoC::AddCallback<GW::Packet::StoC::P095>(
		[&](GW::Packet::StoC::P095* pak) -> bool {
		PconAlcohol::alcohol_level = pak->level;
		//printf("Level = %d, tint = %d\n", pak->level, pak->tint);
		if (enabled) pcon_alcohol->Update();
		return PconAlcohol::suppress_drunk_effect;
	});

	GW::StoC::AddCallback<GW::Packet::StoC::P147>(
		[](GW::Packet::StoC::P147 * pak) -> bool {
		if (PconAlcohol::suppress_drunk_emotes
			&& pak->agent_id == GW::Agents::GetPlayerId()
			&& pak->unk1 == 22) {

			if (pak->unk2 == 0x33E807E5) pak->unk2 = 0; // kneel
			if (pak->unk2 == 0x313AC9D1) pak->unk2 = 0; // bored
			if (pak->unk2 == 0x3033596A) pak->unk2 = 0; // moan
			if (pak->unk2 == 0x305A7EF2) pak->unk2 = 0; // flex
			if (pak->unk2 == 0x74999B06) pak->unk2 = 0; // fistshake
			if (pak->unk2 == 0x30446E61) pak->unk2 = 0; // roar
		}
		return false;
	});
	GW::StoC::AddCallback<GW::Packet::StoC::P229>(
		[](GW::Packet::StoC::P229* pak) -> bool {
		if (PconAlcohol::suppress_drunk_emotes
			&& pak->agent_id == GW::Agents::GetPlayerId()
			&& pak->state & 0x2000) { 

			pak->state ^= 0x2000; 
		}
		return false;
	});
	GW::StoC::AddCallback<GW::Packet::StoC::P153>(
		[](GW::Packet::StoC::P153* pak) -> bool {
		if (!PconAlcohol::suppress_drunk_text) return false;
		wchar_t* m = pak->message;
		if (m[0] == 0x8CA && m[1] == 0xA4F7 && m[2] == 0xF552 && m[3] == 0xA32) return true; // i love you man!
		if (m[0] == 0x8CB && m[1] == 0xE20B && m[2] == 0x9835 && m[3] == 0x4C75) return true; // I'm the king of the world!
		if (m[0] == 0x8CC && m[1] == 0xFA4D && m[2] == 0xF068 && m[3] == 0x393) return true; // I think I need to sit down
		if (m[0] == 0x8CD && m[1] == 0xF2C2 && m[2] == 0xBBAD && m[3] == 0x1EAD) return true; // I think I'm gonna be sick
		if (m[0] == 0x8CE && m[1] == 0x85E5 && m[2] == 0xF726 && m[3] == 0x68B1) return true; // Oh no, not again
		if (m[0] == 0x8CF && m[1] == 0xEDD3 && m[2] == 0xF2B9 && m[3] == 0x3F34) return true; // It's spinning...
		if (m[0] == 0x8D0 && m[1] == 0xF056 && m[2] == 0xE7AD && m[3] == 0x7EE6) return true; // Everyone stop shouting!
		if (m[0] == 0x8101 && m[1] == 0x6671 && m[2] == 0xCBF8 && m[3] == 0xE717) return true; // "BE GONE!"
		if (m[0] == 0x8101 && m[1] == 0x6672 && m[2] == 0xB0D6 && m[3] == 0xCE2F) return true; // "Soon you will all be crushed."
		if (m[0] == 0x8101 && m[1] == 0x6673 && m[2] == 0xDAA5 && m[3] == 0xD0A1) return true; // "You are no match for my almighty power."
		if (m[0] == 0x8101 && m[1] == 0x6674 && m[2] == 0x8BF9 && m[3] == 0x8C19) return true; // "Such fools to think you can attack me here. Come closer so you can see the face of your doom!"
		if (m[0] == 0x8101 && m[1] == 0x6675 && m[2] == 0x996D && m[3] == 0x87BA) return true; // "No one can stop me, let alone you puny mortals!"
		if (m[0] == 0x8101 && m[1] == 0x6676 && m[2] == 0xBAFA && m[3] == 0x8E15) return true; // "You are messing with affairs that are beyond your comprehension. Leave now and I may let you live!"
		if (m[0] == 0x8101 && m[1] == 0x6677 && m[2] == 0xA186 && m[3] == 0xF84C) return true; // "His blood has returned me to my mortal body."
		if (m[0] == 0x8101 && m[1] == 0x6678 && m[2] == 0xD2ED && m[3] == 0xE693) return true; // "I have returned!"
		if (m[0] == 0x8101 && m[1] == 0x6679 && m[2] == 0xA546 && m[3] == 0xF24A) return true; // "Abaddon will feast on your eyes!"
		if (m[0] == 0x8101 && m[1] == 0x667A && m[2] == 0xB477 && m[3] == 0xA79A) return true; // "Abaddon's sword has been drawn. He sends me back to you with tokens of renewed power!"
		if (m[0] == 0x8101 && m[1] == 0x667B && m[2] == 0x8FBB && m[3] == 0xC739) return true; // "Are you the Keymaster?"
		if (m[0] == 0x8101 && m[1] == 0x667C && m[2] == 0xFE50 && m[3] == 0xC173) return true; // "Human sacrifice. Dogs and cats living together. Mass hysteria!"
		if (m[0] == 0x8101 && m[1] == 0x667D && m[2] == 0xBBC6 && m[3] == 0xAC9E) return true; // "Take me now, subcreature."'
		if (m[0] == 0x8101 && m[1] == 0x667E && m[2] == 0xCD71 && m[3] == 0xDEE3) return true; // "We must prepare for the coming of Banjo the Clown, God of Puppets."
		if (m[0] == 0x8101 && m[1] == 0x667F && m[2] == 0xE823 && m[3] == 0x9435) return true; // "This house is clean."
		if (m[0] == 0x8101 && m[1] == 0x6680 && m[2] == 0x82FC && m[3] == 0xDCEC) return true;
		if (m[0] == 0x8101 && m[1] == 0x6681 && m[2] == 0xC86C && m[3] == 0xB975) return true; // "Mommy? Where are you? I can't find you. I can't. I'm afraid of the light, mommy. I'm afraid of the light."
		if (m[0] == 0x8101 && m[1] == 0x6682 && m[2] == 0xE586 && m[3] == 0x9311) return true; // "Get away from my baby!"
		if (m[0] == 0x8101 && m[1] == 0x6683 && m[2] == 0xA949 && m[3] == 0xE643) return true; // "This house has many hearts."'
		if (m[0] == 0x8101 && m[1] == 0x6684 && m[2] == 0xB765 && m[3] == 0x93F1) return true; // "As a boy I spent much time in these lands."
		if (m[0] == 0x8101 && m[1] == 0x6685 && m[2] == 0xEDE0 && m[3] == 0xAF1D) return true; // "I see dead people."
		if (m[0] == 0x8101 && m[1] == 0x6686 && m[2] == 0xD356 && m[3] == 0xDC69) return true; // "Do you like my fish balloon? Can you hear it singing to you...?"
		if (m[0] == 0x8101 && m[1] == 0x6687 && m[2] == 0xEA3C && m[3] == 0x96F0) return true; // "4...Itchy...Tasty..."
		if (m[0] == 0x8101 && m[1] == 0x6688 && m[2] == 0xCBDD && m[3] == 0xB1CF) return true; // "Gracious me, was I raving? Please forgive me. I'm mad."
		if (m[0] == 0x8101 && m[1] == 0x6689 && m[2] == 0xE770 && m[3] == 0xEEA4) return true; // "Keep away. The sow is mine."
		if (m[0] == 0x8101 && m[1] == 0x668A && m[2] == 0x885F && m[3] == 0xE61D) return true; // "All is well. I'm not insane."
		if (m[0] == 0x8101 && m[1] == 0x668B && m[2] == 0xCCDD && m[3] == 0x88AA) return true; // "I like how they've decorated this place. The talking lights are a nice touch."
		if (m[0] == 0x8101 && m[1] == 0x668C && m[2] == 0x8873 && m[3] == 0x9A16) return true; // "There's a reason there's a festival ticket in my ear. I'm trying to lure the evil spirits out of my head."
		if (m[0] == 0x8101 && m[1] == 0x668D && m[2] == 0xAF68 && m[3] == 0xF84A) return true; // "And this is where I met the Lich. He told me to burn things."
		if (m[0] == 0x8101 && m[1] == 0x668E && m[2] == 0xFE43 && m[3] == 0x9CB3) return true; // "When I grow up, I want to be a principal or a caterpillar."
		if (m[0] == 0x8101 && m[1] == 0x668F && m[2] == 0xDAFF && m[3] == 0x903E) return true; // "Oh boy, sleep! That's where I'm a Luxon."
		if (m[0] == 0x8101 && m[1] == 0x6690 && m[2] == 0xA1F5 && m[3] == 0xD15F) return true; // "My cat's breath smells like cat food."
		if (m[0] == 0x8101 && m[1] == 0x6691 && m[2] == 0xAE54 && m[3] == 0x8EC6) return true; // "My cat's name is Mittens."
		if (m[0] == 0x8101 && m[1] == 0x6692 && m[2] == 0xDFBB && m[3] == 0xD674) return true; // "Then the healer told me that BOTH my eyes were lazy. And that's why it was the best summer ever!"
		if (m[0] == 0x8101 && m[1] == 0x6693 && m[2] == 0xAC9F && m[3] == 0xDCBE) return true; // "Go, banana!"
		if (m[0] == 0x8101 && m[1] == 0x6694 && m[2] == 0x9ACA && m[3] == 0xC746) return true; // "It's a trick. Get an axe."
		if (m[0] == 0x8101 && m[1] == 0x6695 && m[2] == 0x8ED8 && m[3] == 0xD572) return true; // "Klaatu...barada...necktie?"
		if (m[0] == 0x8101 && m[1] == 0x6696 && m[2] == 0xE883 && m[3] == 0xFED7) return true; // "You're disgusting, but I love you!"
		if (m[0] == 0x8101 && m[1] == 0x68BA && m[2] == 0xA875 && m[3] == 0xA785) return true; // "Cross over, children. All are welcome. All welcome. Go into the light. There is peace and serenity in the light."
		//printf("m[0] == 0x%X && m[1] == 0x%X && m[2] == 0x%X && m[3] == 0x%X\n", m[0], m[1], m[2], m[3]);
		return false;
	});

	GW::Chat::CreateCommand(L"pcons",
		[this](int argc, LPWSTR *argv) {
		if (argc <= 1) {
			ToggleEnable();
		} else { // we are ignoring parameters after the first
			std::wstring arg1 = GuiUtils::ToLower(argv[1]);
			if (arg1 == L"on") {
				SetEnabled(true);
			} else if (arg1 == L"off") {
				SetEnabled(false);
			} else {
				Log::Error("Invalid argument '%ls', please use /pcons [|on|off]", argv[1]);
			}
		}
	});
}

bool PconsWindow::DrawTabButton(IDirect3DDevice9* device, 
	bool show_icon, bool show_text) {

	bool clicked = ToolboxWindow::DrawTabButton(device, show_icon, show_text);

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

void PconsWindow::Draw(IDirect3DDevice9* device) {
	if (!visible) return;

	bool alcohol_enabled_before = pcon_alcohol->enabled;
	
	ImGui::SetNextWindowPosCenter(ImGuiSetCond_FirstUseEver);
	if (ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {
		if (show_enable_button) {
			ImGui::PushStyleColor(ImGuiCol_Text, enabled ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0, 0, 1));
			if (ImGui::Button(enabled ? "Enabled###pconstoggle" : "Disabled###pconstoggle",
				ImVec2(ImGui::GetWindowContentRegionWidth(), 0))) {
				ToggleEnable();
			}
			ImGui::PopStyleColor();
		}
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

	if (!alcohol_enabled_before && pcon_alcohol->enabled) {
		CheckIfWeJustEnabledAlcoholWithLunarsOn();
	}
}

void PconsWindow::Update(float delta) {
	if (current_map_type != GW::Map::GetInstanceType()) {
		current_map_type = GW::Map::GetInstanceType();
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

bool PconsWindow::SetEnabled(bool b) {
	enabled = b;
	if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Loading) {
		ImGuiWindow* main = ImGui::FindWindowByName(MainWindow::Instance().Name());
		ImGuiWindow* pcon = ImGui::FindWindowByName(Name());
		if ((pcon == nullptr || pcon->Collapsed || !visible)
			&& (main == nullptr || main->Collapsed || !MainWindow::Instance().visible)) {

			Log::Info("Pcons %s", enabled ? "enabled" : "disabled");
		}
	}
	if (tick_with_pcons && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost) {
		GW::PartyMgr::Tick(enabled);
	}
	CheckIfWeJustEnabledAlcoholWithLunarsOn();
	return enabled;
}

void PconsWindow::CheckIfWeJustEnabledAlcoholWithLunarsOn() {
	if (enabled
		&& GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable
		&& pcon_alcohol->enabled
		&& Pcon::alcohol_level == 5) {
		// we just re-enabled pcons and we need to pop alcohol, but the alcohol level 
		// is 5 already, which means it's very likely that we have Spiritual Possession on.
		// Force usage of alcohol to be sure.
		// Note: if we're dead this will fail and alcohol will never be used.
		pcon_alcohol->ForceUse();
	}
}

void PconsWindow::LoadSettings(CSimpleIni* ini) {
	ToolboxWindow::LoadSettings(ini);
	show_menubutton = ini->GetBoolValue(Name(), VAR_NAME(show_menubutton), true);

	for (Pcon* pcon : pcons) {
		pcon->LoadSettings(ini, Name());
	}
	
	tick_with_pcons = ini->GetBoolValue(Name(), VAR_NAME(tick_with_pcons), true);
	items_per_row = ini->GetLongValue(Name(), VAR_NAME(items_per_row), 3);
	Pcon::pcons_delay = ini->GetLongValue(Name(), VAR_NAME(pcons_delay), 5000);
	Pcon::lunar_delay = ini->GetLongValue(Name(), VAR_NAME(lunar_delay), 500);
	Pcon::size = (float)ini->GetDoubleValue(Name(), "pconsize", 46.0);
	Pcon::disable_when_not_found = ini->GetBoolValue(Name(), VAR_NAME(disable_when_not_found), true);
	Pcon::enabled_bg_color = Colors::Load(ini, Name(), VAR_NAME(enabled_bg_color), Pcon::enabled_bg_color);
	ini->SetBoolValue(Name(), VAR_NAME(show_enable_button), show_enable_button);
	Pcon::suppress_drunk_effect = ini->GetBoolValue(Name(), VAR_NAME(suppress_drunk_effect), false);
	Pcon::suppress_drunk_text = ini->GetBoolValue(Name(), VAR_NAME(suppress_drunk_text), false);
	Pcon::suppress_drunk_emotes = ini->GetBoolValue(Name(), VAR_NAME(suppress_drunk_emotes), false);
	Pcon::suppress_lunar_skills = ini->GetBoolValue(Name(), VAR_NAME(suppress_lunar_skills), false);
}

void PconsWindow::SaveSettings(CSimpleIni* ini) {
	ToolboxWindow::SaveSettings(ini);

	for (Pcon* pcon : pcons) {
		pcon->SaveSettings(ini, Name());
	}

	ini->SetBoolValue(Name(), VAR_NAME(tick_with_pcons), tick_with_pcons);
	ini->SetLongValue(Name(), VAR_NAME(items_per_row), items_per_row);
	ini->SetLongValue(Name(), VAR_NAME(pcons_delay), Pcon::pcons_delay);
	ini->SetLongValue(Name(), VAR_NAME(lunar_delay), Pcon::lunar_delay);
	ini->SetDoubleValue(Name(), "pconsize", Pcon::size);
	ini->SetBoolValue(Name(), VAR_NAME(disable_when_not_found), Pcon::disable_when_not_found);
	Colors::Save(ini, Name(), VAR_NAME(enabled_bg_color), Pcon::enabled_bg_color);
	ini->SetBoolValue(Name(), VAR_NAME(show_enable_button), show_enable_button);

	ini->SetBoolValue(Name(), VAR_NAME(suppress_drunk_effect), Pcon::suppress_drunk_effect);
	ini->SetBoolValue(Name(), VAR_NAME(suppress_drunk_text), Pcon::suppress_drunk_text);
	ini->SetBoolValue(Name(), VAR_NAME(suppress_drunk_emotes), Pcon::suppress_drunk_emotes);
	ini->SetBoolValue(Name(), VAR_NAME(suppress_lunar_skills), Pcon::suppress_lunar_skills);
}

void PconsWindow::DrawSettingInternal() {
	ImGui::Separator();
	ImGui::Text("Functionality:");
	ImGui::Checkbox("Tick with pcons", &tick_with_pcons);
	ImGui::ShowHelp("Enabling or disabling pcons will also Tick or Untick in party list");
	ImGui::Checkbox("Disable when not found", &Pcon::disable_when_not_found);
	ImGui::ShowHelp("Toolbox will disable a pcon if it is not found in the inventory");
	ImGui::SliderInt("Pcons delay", &Pcon::pcons_delay, 100, 5000, "%.0f milliseconds");
	ImGui::ShowHelp(
		"After using a pcon, toolbox will not use it again for this amount of time.\n"
		"It is needed to prevent toolbox from using a pcon twice, before it activates.\n"
		"Decrease the value if you have good ping and you die a lot.");
	ImGui::SliderInt("Lunars delay", &Pcon::lunar_delay, 100, 500, "%.0f milliseconds");
	if (ImGui::TreeNode("Thresholds")) {
		ImGui::Text("When you have less than this amount:\n-The number in the interface becomes yellow.\n-Warning message is displayed when zoning into outpost.");
		for (Pcon* pcon : pcons) {
			ImGui::SliderInt(pcon->chat, &pcon->threshold, 0, 250);
		}
		ImGui::TreePop();
	}

	ImGui::Separator();
	ImGui::Text("Interface:");
	ImGui::SliderInt("Items per row", &items_per_row, 1, 18);
	ImGui::DragFloat("Pcon Size", &Pcon::size, 1.0f, 10.0f, 0.0f);
	ImGui::ShowHelp("Size of each Pcon icon in the interface");
	Colors::DrawSetting("Enabled-Background", &Pcon::enabled_bg_color);
	if (Pcon::size <= 1.0f) Pcon::size = 1.0f;
	if (ImGui::TreeNode("Visibility")) {
		ImGui::Checkbox("Enable/Disable button", &show_enable_button);
		for (Pcon* pcon : pcons) {
			ImGui::Checkbox(pcon->chat, &pcon->visible);
		}
		ImGui::TreePop();
	}

	ImGui::Separator();
	ImGui::Text("Lunars and Alcohol");
	ImGui::Text("Current drunk level: %d", Pcon::alcohol_level);
	ImGui::Checkbox("Suppress lunar and drunk post-processing effects", &Pcon::suppress_drunk_effect);
	ImGui::ShowHelp("Will actually disable any *change*, so make sure you're not drunk already when enabling this!");
	ImGui::Checkbox("Suppress lunar and drunk text", &Pcon::suppress_drunk_text);
	ImGui::ShowHelp("Will hide drunk and lunars messages on top of your and other characters");
	ImGui::Checkbox("Suppress drunk emotes", &Pcon::suppress_drunk_emotes);
	ImGui::ShowHelp("Important:\n"
		"This feature is experimental and might crash your game.\n"
		"Using level 1 alcohol instead of this is recommended for preventing drunk emotes.\n"
		"This will prevent kneel, bored, moan, flex, fistshake and roar.\n");
	ImGui::Checkbox("Hide Spiritual Possession and Lucky Aura", &Pcon::suppress_lunar_skills);
	ImGui::ShowHelp("Will hide the skills in your effect monitor");
}
