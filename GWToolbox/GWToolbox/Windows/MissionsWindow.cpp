#include "stdafx.h"
#include "MissionsWindow.h"

#include <GWCA\Constants\Constants.h>
#include <Modules\Resources.h>

using namespace Missions;

void MissionsWindow::Initialize()
{
	ToolboxWindow::Initialize();
	Resources::Instance().LoadTextureAsync(&button_texture, Resources::GetPath(L"img/missions", L"MissionIcon.png"), IDB_Missions_MissionIcon);

	// Prophecies
	auto& prophecies_missions = missions.at(Campaign::Prophecies);
	prophecies_missions.push_back(new PropheciesMission(GW::Constants::MapID::The_Great_Northern_Wall));
	prophecies_missions.push_back(new PropheciesMission(GW::Constants::MapID::Fort_Ranik));
	prophecies_missions.push_back(new PropheciesMission(GW::Constants::MapID::Ruins_of_Surmia));
	prophecies_missions.push_back(new PropheciesMission(GW::Constants::MapID::Nolani_Academy));
	prophecies_missions.push_back(new PropheciesMission(GW::Constants::MapID::Borlis_Pass));
	prophecies_missions.push_back(new PropheciesMission(GW::Constants::MapID::The_Frost_Gate));
	prophecies_missions.push_back(new PropheciesMission(GW::Constants::MapID::Gates_of_Kryta));
	prophecies_missions.push_back(new PropheciesMission(GW::Constants::MapID::DAlessio_Seaboard));
	prophecies_missions.push_back(new PropheciesMission(GW::Constants::MapID::Divinity_Coast));
	prophecies_missions.push_back(new PropheciesMission(GW::Constants::MapID::The_Wilds));
	prophecies_missions.push_back(new PropheciesMission(GW::Constants::MapID::Bloodstone_Fen));
	prophecies_missions.push_back(new PropheciesMission(GW::Constants::MapID::Aurora_Glade));
	prophecies_missions.push_back(new PropheciesMission(GW::Constants::MapID::Riverside_Province));
	prophecies_missions.push_back(new PropheciesMission(GW::Constants::MapID::Sanctum_Cay));
	prophecies_missions.push_back(new PropheciesMission(GW::Constants::MapID::Dunes_of_Despair));
	prophecies_missions.push_back(new PropheciesMission(GW::Constants::MapID::Thirsty_River));
	prophecies_missions.push_back(new PropheciesMission(GW::Constants::MapID::Elona_Reach));
	prophecies_missions.push_back(new PropheciesMission(GW::Constants::MapID::Augury_Rock_mission));
	prophecies_missions.push_back(new PropheciesMission(GW::Constants::MapID::The_Dragons_Lair));
	prophecies_missions.push_back(new PropheciesMission(GW::Constants::MapID::Ice_Caves_of_Sorrow));
	prophecies_missions.push_back(new PropheciesMission(GW::Constants::MapID::Iron_Mines_of_Moladune));
	prophecies_missions.push_back(new PropheciesMission(GW::Constants::MapID::Thunderhead_Keep));
	prophecies_missions.push_back(new PropheciesMission(GW::Constants::MapID::Ring_of_Fire));
	prophecies_missions.push_back(new PropheciesMission(GW::Constants::MapID::Abaddons_Mouth));
	prophecies_missions.push_back(new PropheciesMission(GW::Constants::MapID::Hells_Precipice));
	// Factions
	auto& factions_missions = missions.at(Campaign::Factions);
	factions_missions.push_back(new FactionsMission(GW::Constants::MapID::Minister_Chos_Estate_outpost_mission));
	factions_missions.push_back(new FactionsMission(GW::Constants::MapID::Zen_Daijun_outpost_mission));
	factions_missions.push_back(new FactionsMission(GW::Constants::MapID::Vizunah_Square_Local_Quarter_outpost));
	factions_missions.push_back(new FactionsMission(GW::Constants::MapID::Vizunah_Square_Foreign_Quarter_outpost));
	factions_missions.push_back(new FactionsMission(GW::Constants::MapID::Nahpui_Quarter_outpost_mission));
	factions_missions.push_back(new FactionsMission(GW::Constants::MapID::Tahnnakai_Temple_outpost_mission));
	factions_missions.push_back(new FactionsMission(GW::Constants::MapID::Arborstone_outpost_mission));
	factions_missions.push_back(new FactionsMission(GW::Constants::MapID::Boreas_Seabed_outpost_mission));
	factions_missions.push_back(new FactionsMission(GW::Constants::MapID::Sunjiang_District_outpost_mission));
	factions_missions.push_back(new FactionsMission(GW::Constants::MapID::The_Eternal_Grove_outpost_mission));
	factions_missions.push_back(new FactionsMission(GW::Constants::MapID::Unwaking_Waters_Kurzick_outpost));
	factions_missions.push_back(new FactionsMission(GW::Constants::MapID::Gyala_Hatchery_outpost_mission));
	factions_missions.push_back(new FactionsMission(GW::Constants::MapID::Unwaking_Waters_Luxon_outpost));
	factions_missions.push_back(new FactionsMission(GW::Constants::MapID::Raisu_Palace_outpost_mission));
	factions_missions.push_back(new FactionsMission(GW::Constants::MapID::Imperial_Sanctum_outpost_mission));
	// Nightfall
	auto& nightfall_missions = missions.at(Campaign::Nightfall);
	nightfall_missions.push_back(new NightfallMission(GW::Constants::MapID::Chahbek_Village));
	nightfall_missions.push_back(new NightfallMission(GW::Constants::MapID::Jokanur_Diggings));
	nightfall_missions.push_back(new NightfallMission(GW::Constants::MapID::Blacktide_Den));
	nightfall_missions.push_back(new NightfallMission(GW::Constants::MapID::Consulate_Docks));
	nightfall_missions.push_back(new NightfallMission(GW::Constants::MapID::Venta_Cemetery));
	nightfall_missions.push_back(new NightfallMission(GW::Constants::MapID::Kodonur_Crossroads));
	nightfall_missions.push_back(new NightfallMission(GW::Constants::MapID::Pogahn_Passage));
	nightfall_missions.push_back(new NightfallMission(GW::Constants::MapID::Rilohn_Refuge));
	nightfall_missions.push_back(new NightfallMission(GW::Constants::MapID::Moddok_Crevice));
	nightfall_missions.push_back(new NightfallMission(GW::Constants::MapID::Tihark_Orchard));
	nightfall_missions.push_back(new NightfallMission(GW::Constants::MapID::Dasha_Vestibule));
	nightfall_missions.push_back(new NightfallMission(GW::Constants::MapID::Dzagonur_Bastion));
	nightfall_missions.push_back(new NightfallMission(GW::Constants::MapID::Grand_Court_of_Sebelkeh));
	nightfall_missions.push_back(new NightfallMission(GW::Constants::MapID::Jennurs_Horde));
	nightfall_missions.push_back(new NightfallMission(GW::Constants::MapID::Nundu_Bay));
	nightfall_missions.push_back(new NightfallMission(GW::Constants::MapID::Gate_of_Desolation));
	nightfall_missions.push_back(new NightfallMission(GW::Constants::MapID::Ruins_of_Morah));
	nightfall_missions.push_back(new TormentMission(GW::Constants::MapID::Gate_of_Pain));
	nightfall_missions.push_back(new TormentMission(GW::Constants::MapID::Gate_of_Madness));
	nightfall_missions.push_back(new TormentMission(GW::Constants::MapID::Abaddons_Gate));
	// Eye of the North - missions
	auto& eotn_missions = missions.at(Campaign::EyeOfTheNorth);
	// Asura
	eotn_missions.push_back(new EotNMission(GW::Constants::MapID::Gadds_Encampment_outpost, "Finding the Bloodstone"));
	eotn_missions.push_back(new EotNMission(GW::Constants::MapID::Rata_Sum_outpost, "The Elusive Golemancer"));
	eotn_missions.push_back(new EotNMission(GW::Constants::MapID::Rata_Sum_outpost, "Genius Operated Living Enchanted Manifestation"));
	// Vanguard
	eotn_missions.push_back(new EotNMission(GW::Constants::MapID::Longeyes_Ledge_outpost, "Against the Charr"));
	eotn_missions.push_back(new EotNMission(GW::Constants::MapID::Doomlore_Shrine_outpost, "Warband of Brothers"));
	eotn_missions.push_back(new EotNMission(GW::Constants::MapID::Doomlore_Shrine_outpost, "Assault on the Stronghold"));
	// Norn
	eotn_missions.push_back(new EotNMission(GW::Constants::MapID::Sifhalla_outpost, "Curse of the Nornbear"));
	eotn_missions.push_back(new EotNMission(GW::Constants::MapID::Sifhalla_outpost, "Blood Washes Blood"));
	eotn_missions.push_back(new EotNMission(GW::Constants::MapID::Olafstead_outpost, "A Gate Too Far"));
	// Destroyers
	eotn_missions.push_back(new EotNMission(GW::Constants::MapID::Central_Transfer_Chamber_outpost, "Destruction's Depths"));
	eotn_missions.push_back(new EotNMission(GW::Constants::MapID::Central_Transfer_Chamber_outpost, "A Time for Heroes"));

	// Eye of the North - dungeons
	auto& dungeons = missions.at(Campaign::Dungeon);
	dungeons.push_back(new Dungeon(GW::Constants::MapID::Doomlore_Shrine_outpost, "Catacombs of Kathandrax"));
	dungeons.push_back(new Dungeon(GW::Constants::MapID::Doomlore_Shrine_outpost, "Rragar's Menagerie"));
	dungeons.push_back(new Dungeon(GW::Constants::MapID::Doomlore_Shrine_outpost, "Cathedral of Flames"));
	dungeons.push_back(new Dungeon(GW::Constants::MapID::Doomlore_Shrine_outpost, "Ooze Pit"));
	dungeons.push_back(new Dungeon(GW::Constants::MapID::Longeyes_Ledge_outpost, "Darkrime Delves"));
	dungeons.push_back(new Dungeon(GW::Constants::MapID::Sifhalla_outpost, "Frostmaw's Burrows"));
	dungeons.push_back(new Dungeon(GW::Constants::MapID::Sifhalla_outpost, "Sepulchre of Dragrimmar"));
	dungeons.push_back(new Dungeon(GW::Constants::MapID::Olafstead_outpost, "Raven's Point"));
	dungeons.push_back(new Dungeon(GW::Constants::MapID::Umbral_Grotto_outpost, "Vloxen Excavations"));
	dungeons.push_back(new Dungeon(GW::Constants::MapID::Gadds_Encampment_outpost, "Bogroot Growths"));
	dungeons.push_back(new Dungeon(GW::Constants::MapID::Gadds_Encampment_outpost, "Bloodstone Caves"));
	dungeons.push_back(new Dungeon(GW::Constants::MapID::Vloxs_Falls, "Shards of Orr"));
	dungeons.push_back(new Dungeon(GW::Constants::MapID::Rata_Sum_outpost, "Oola's Lab"));
	dungeons.push_back(new Dungeon(GW::Constants::MapID::Rata_Sum_outpost, "Arachni's Haunt"));
	dungeons.push_back(new Dungeon(GW::Constants::MapID::Umbral_Grotto_outpost, "Slavers' Exile"));
	dungeons.push_back(new Dungeon(GW::Constants::MapID::Gunnars_Hold_outpost, "Fronis Irontoe's Lair"));
	dungeons.push_back(new Dungeon(GW::Constants::MapID::Umbral_Grotto_outpost, "Secret Lair of the Snowmen"));
	dungeons.push_back(new Dungeon(GW::Constants::MapID::Central_Transfer_Chamber_outpost, "Heart of the Shiverpeaks"));
}


void MissionsWindow::Terminate()
{
	for (auto camp : missions) {
		for (auto m : camp.second) {
			delete m;
		}
	}
}


void MissionsWindow::Draw(IDirect3DDevice9* device)
{
	if (!visible) return;
	ImGui::SetNextWindowPosCenter(ImGuiSetCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(350, 400), ImGuiSetCond_FirstUseEver);
	if (ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {
		for (auto camp : missions) {
			ImGui::CollapsingHeader(CampaignName(camp.first));
			auto& camp_missions = camp.second;
			for (size_t i = 0; i < camp_missions.size(); i++) {
				if (i % missions_per_row > 0) {
					ImGui::SameLine();
				}
				camp_missions.at(i)->Draw(device, icon_size);
			}
		}
	}


	ImGui::End();
}

void MissionsWindow::DrawSettingInternal()
{
	ToolboxWindow::DrawSettingInternal();
}

void MissionsWindow::LoadSettings(CSimpleIni* ini)
{
	ToolboxWindow::LoadSettings(ini);
	show_menubutton = ini->GetBoolValue(Name(), VAR_NAME(show_menubutton), true);
}

void MissionsWindow::SaveSettings(CSimpleIni* ini)
{
	ToolboxWindow::SaveSettings(ini);
}
