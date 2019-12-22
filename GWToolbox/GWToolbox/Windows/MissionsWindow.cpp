#include "stdafx.h"
#include "MissionsWindow.h"

#include <GWCA\Constants\Constants.h>
#include <Modules\Resources.h>

void MissionsWindow::Initialize() {
	ToolboxWindow::Initialize();
	Resources::Instance().LoadTextureAsync(&button_texture, Resources::GetPath(L"img/missions", L"MissionIcon.png"), IDB_Missions_MissionIcon);

	// Prophecies
	missions.push_back(new PropheciesMission(GW::Constants::MapID::The_Great_Northern_Wall));
	missions.push_back(new PropheciesMission(GW::Constants::MapID::Fort_Ranik));
	missions.push_back(new PropheciesMission(GW::Constants::MapID::Ruins_of_Surmia));
	missions.push_back(new PropheciesMission(GW::Constants::MapID::Nolani_Academy));
	missions.push_back(new PropheciesMission(GW::Constants::MapID::Borlis_Pass));
	missions.push_back(new PropheciesMission(GW::Constants::MapID::The_Frost_Gate));
	missions.push_back(new PropheciesMission(GW::Constants::MapID::Gates_of_Kryta));
	missions.push_back(new PropheciesMission(GW::Constants::MapID::DAlessio_Seaboard));
	missions.push_back(new PropheciesMission(GW::Constants::MapID::Divinity_Coast));
	missions.push_back(new PropheciesMission(GW::Constants::MapID::The_Wilds));
	missions.push_back(new PropheciesMission(GW::Constants::MapID::Bloodstone_Fen));
	missions.push_back(new PropheciesMission(GW::Constants::MapID::Aurora_Glade));
	missions.push_back(new PropheciesMission(GW::Constants::MapID::Riverside_Province));
	missions.push_back(new PropheciesMission(GW::Constants::MapID::Sanctum_Cay));
	missions.push_back(new PropheciesMission(GW::Constants::MapID::Dunes_of_Despair));
	missions.push_back(new PropheciesMission(GW::Constants::MapID::Thirsty_River));
	missions.push_back(new PropheciesMission(GW::Constants::MapID::Elona_Reach));
	missions.push_back(new PropheciesMission(GW::Constants::MapID::Augury_Rock_mission));
	missions.push_back(new PropheciesMission(GW::Constants::MapID::The_Dragons_Lair));
	missions.push_back(new PropheciesMission(GW::Constants::MapID::Ice_Caves_of_Sorrow));
	missions.push_back(new PropheciesMission(GW::Constants::MapID::Iron_Mines_of_Moladune));
	missions.push_back(new PropheciesMission(GW::Constants::MapID::Thunderhead_Keep));
	missions.push_back(new PropheciesMission(GW::Constants::MapID::Ring_of_Fire));
	missions.push_back(new PropheciesMission(GW::Constants::MapID::Abaddons_Mouth));
	missions.push_back(new PropheciesMission(GW::Constants::MapID::Hells_Precipice));
	// Factions
	missions.push_back(new FactionsMission(GW::Constants::MapID::Minister_Chos_Estate_outpost_mission));
	missions.push_back(new FactionsMission(GW::Constants::MapID::Zen_Daijun_outpost_mission));
	missions.push_back(new FactionsMission(GW::Constants::MapID::Vizunah_Square_Local_Quarter_outpost));
	missions.push_back(new FactionsMission(GW::Constants::MapID::Vizunah_Square_Foreign_Quarter_outpost));
	missions.push_back(new FactionsMission(GW::Constants::MapID::Nahpui_Quarter_outpost_mission));
	missions.push_back(new FactionsMission(GW::Constants::MapID::Tahnnakai_Temple_outpost_mission));
	missions.push_back(new FactionsMission(GW::Constants::MapID::Arborstone_outpost_mission));
	missions.push_back(new FactionsMission(GW::Constants::MapID::Boreas_Seabed_outpost_mission));
	missions.push_back(new FactionsMission(GW::Constants::MapID::Sunjiang_District_outpost_mission));
	missions.push_back(new FactionsMission(GW::Constants::MapID::The_Eternal_Grove_outpost_mission));
	missions.push_back(new FactionsMission(GW::Constants::MapID::Unwaking_Waters_Kurzick_outpost));
	missions.push_back(new FactionsMission(GW::Constants::MapID::Gyala_Hatchery_outpost_mission));
	missions.push_back(new FactionsMission(GW::Constants::MapID::Unwaking_Waters_Luxon_outpost));
	missions.push_back(new FactionsMission(GW::Constants::MapID::Raisu_Palace_outpost_mission));
	missions.push_back(new FactionsMission(GW::Constants::MapID::Imperial_Sanctum_outpost_mission));
	// Nightfall
	missions.push_back(new NightfallMission(GW::Constants::MapID::Chahbek_Village));
	missions.push_back(new NightfallMission(GW::Constants::MapID::Jokanur_Diggings));
	missions.push_back(new NightfallMission(GW::Constants::MapID::Blacktide_Den));
	missions.push_back(new NightfallMission(GW::Constants::MapID::Consulate_Docks));
	missions.push_back(new NightfallMission(GW::Constants::MapID::Venta_Cemetery));
	missions.push_back(new NightfallMission(GW::Constants::MapID::Kodonur_Crossroads));
	missions.push_back(new NightfallMission(GW::Constants::MapID::Pogahn_Passage));
	missions.push_back(new NightfallMission(GW::Constants::MapID::Rilohn_Refuge));
	missions.push_back(new NightfallMission(GW::Constants::MapID::Moddok_Crevice));
	missions.push_back(new NightfallMission(GW::Constants::MapID::Tihark_Orchard));
	missions.push_back(new NightfallMission(GW::Constants::MapID::Dasha_Vestibule));
	missions.push_back(new NightfallMission(GW::Constants::MapID::Dzagonur_Bastion));
	missions.push_back(new NightfallMission(GW::Constants::MapID::Grand_Court_of_Sebelkeh));
	missions.push_back(new NightfallMission(GW::Constants::MapID::Jennurs_Horde));
	missions.push_back(new NightfallMission(GW::Constants::MapID::Nundu_Bay));
	missions.push_back(new NightfallMission(GW::Constants::MapID::Gate_of_Desolation));
	missions.push_back(new NightfallMission(GW::Constants::MapID::Ruins_of_Morah));
	missions.push_back(new NightfallMission(GW::Constants::MapID::Gate_of_Pain));
	missions.push_back(new NightfallMission(GW::Constants::MapID::Gate_of_Madness));
	missions.push_back(new NightfallMission(GW::Constants::MapID::Abaddons_Gate));
	// Eye of the North - missions
	// Asura
	missions.push_back(new EotNMission(GW::Constants::MapID::Gadds_Encampment_outpost, "Finding the Bloodstone"));
	missions.push_back(new EotNMission(GW::Constants::MapID::Rata_Sum_outpost, "The Elusive Golemancer"));
	missions.push_back(new EotNMission(GW::Constants::MapID::Rata_Sum_outpost, "Genius Operated Living Enchanted Manifestation"));
	// Vanguard
	missions.push_back(new EotNMission(GW::Constants::MapID::Longeyes_Ledge_outpost, "Against the Charr"));
	missions.push_back(new EotNMission(GW::Constants::MapID::Doomlore_Shrine_outpost, "Warband of Brothers"));
	missions.push_back(new EotNMission(GW::Constants::MapID::Doomlore_Shrine_outpost, "Assault on the Stronghold"));
	// Norn
	missions.push_back(new EotNMission(GW::Constants::MapID::Sifhalla_outpost, "Curse of the Nornbear"));
	missions.push_back(new EotNMission(GW::Constants::MapID::Sifhalla_outpost, "Blood Washes Blood"));
	missions.push_back(new EotNMission(GW::Constants::MapID::Olafstead_outpost, "A Gate Too Far"));
	// Destroyers
	missions.push_back(new EotNMission(GW::Constants::MapID::Central_Transfer_Chamber_outpost, "Destruction's Depths"));
	missions.push_back(new EotNMission(GW::Constants::MapID::Central_Transfer_Chamber_outpost, "A Time for Heroes"));

	// Eye of the North - dungeons
	missions.push_back(new Dungeon(GW::Constants::MapID::Doomlore_Shrine_outpost, "Catacombs of Kathandrax"));
	missions.push_back(new Dungeon(GW::Constants::MapID::Doomlore_Shrine_outpost, "Rragar's Menagerie"));
	missions.push_back(new Dungeon(GW::Constants::MapID::Doomlore_Shrine_outpost, "Cathedral of Flames"));
	missions.push_back(new Dungeon(GW::Constants::MapID::Doomlore_Shrine_outpost, "Ooze Pit"));
	missions.push_back(new Dungeon(GW::Constants::MapID::Longeyes_Ledge_outpost, "Darkrime Delves"));
	missions.push_back(new Dungeon(GW::Constants::MapID::Sifhalla_outpost, "Frostmaw's Burrows"));
	missions.push_back(new Dungeon(GW::Constants::MapID::Sifhalla_outpost, "Sepulchre of Dragrimmar"));
	missions.push_back(new Dungeon(GW::Constants::MapID::Olafstead_outpost, "Raven's Point"));
	missions.push_back(new Dungeon(GW::Constants::MapID::Umbral_Grotto_outpost, "Vloxen Excavations"));
	missions.push_back(new Dungeon(GW::Constants::MapID::Gadds_Encampment_outpost, "Bogroot Growths"));
	missions.push_back(new Dungeon(GW::Constants::MapID::Gadds_Encampment_outpost, "Bloodstone Caves"));
	missions.push_back(new Dungeon(GW::Constants::MapID::Vloxs_Falls, "Shards of Orr"));
	missions.push_back(new Dungeon(GW::Constants::MapID::Rata_Sum_outpost, "Oola's Lab"));
	missions.push_back(new Dungeon(GW::Constants::MapID::Rata_Sum_outpost, "Arachni's Haunt"));
	missions.push_back(new Dungeon(GW::Constants::MapID::Umbral_Grotto_outpost, "Slavers' Exile"));
	missions.push_back(new Dungeon(GW::Constants::MapID::Gunnars_Hold_outpost, "Fronis Irontoe's Lair"));
	missions.push_back(new Dungeon(GW::Constants::MapID::Umbral_Grotto_outpost, "Secret Lair of the Snowmen"));
	missions.push_back(new Dungeon(GW::Constants::MapID::Central_Transfer_Chamber_outpost, "Heart of the Shiverpeaks"));
}
void MissionsWindow::Terminate() {
	for (auto m : missions) {
		delete m;
	}
}

void MissionsWindow::Draw(IDirect3DDevice9* pDevice) {
	if (!visible) return;
	// === hotkey panel ===
	ImGui::SetNextWindowPosCenter(ImGuiSetCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(300, 400), ImGuiSetCond_FirstUseEver);
	if (ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {
		if (ImGui::Button("Create Hotkey...", ImVec2(ImGui::GetWindowContentRegionWidth(), 0))) {
			ImGui::OpenPopup("Create Hotkey");
		}
	}
	ImGui::End();
}

void MissionsWindow::DrawSettingInternal() {
	ToolboxWindow::DrawSettingInternal();
}

void MissionsWindow::LoadSettings(CSimpleIni* ini) {
	ToolboxWindow::LoadSettings(ini);
	show_menubutton = ini->GetBoolValue(Name(), VAR_NAME(show_menubutton), true);
}
void MissionsWindow::SaveSettings(CSimpleIni* ini) {
	ToolboxWindow::SaveSettings(ini);
}

//void MissionsWindow::Update(float delta) {
//}
