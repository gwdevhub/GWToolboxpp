#include "stdafx.h"
#include "MissionsWindow.h"

#include <GWCA/Constants/Constants.h>
#include <Modules/Resources.h>

using namespace GW::Constants;
using namespace Missions;


void MissionsWindow::Initialize()
{
	ToolboxWindow::Initialize();
	Resources::Instance().LoadTextureAsync(&button_texture, Resources::GetPath(L"img/missions", L"MissionIcon.png"), IDB_Missions_MissionIcon);

	Initialize_Prophecies();
	Initialize_Factions();
	Initialize_Nightfall();
	Initialize_EotN();
	Initialize_Dungeons();

}


void MissionsWindow::Initialize_Prophecies()
{
	auto& prophecies_missions = missions.at(Campaign::Prophecies);
	prophecies_missions.push_back(new PropheciesMission(
		MapID::The_Great_Northern_Wall));
	prophecies_missions.push_back(new PropheciesMission(
		MapID::Fort_Ranik, QuestID::ZaishenMission::Fort_Ranik));
	prophecies_missions.push_back(new PropheciesMission(
		MapID::Ruins_of_Surmia, QuestID::ZaishenMission::Ruins_of_Surmia));
	prophecies_missions.push_back(new PropheciesMission(
		MapID::Nolani_Academy));
	prophecies_missions.push_back(new PropheciesMission(
		MapID::Borlis_Pass, QuestID::ZaishenMission::Borlis_Pass));
	prophecies_missions.push_back(new PropheciesMission(
		MapID::The_Frost_Gate, QuestID::ZaishenMission::The_Frost_Gate));
	prophecies_missions.push_back(new PropheciesMission(
		MapID::Gates_of_Kryta, QuestID::ZaishenMission::Gates_of_Kryta));
	prophecies_missions.push_back(new PropheciesMission(
		MapID::DAlessio_Seaboard, QuestID::ZaishenMission::DAlessio_Seaboard));
	prophecies_missions.push_back(new PropheciesMission(
		MapID::Divinity_Coast, QuestID::ZaishenMission::Divinity_Coast));
	prophecies_missions.push_back(new PropheciesMission(
		MapID::The_Wilds, QuestID::ZaishenMission::The_Wilds));
	prophecies_missions.push_back(new PropheciesMission(
		MapID::Bloodstone_Fen, QuestID::ZaishenMission::Bloodstone_Fen));
	prophecies_missions.push_back(new PropheciesMission(
		MapID::Aurora_Glade));
	prophecies_missions.push_back(new PropheciesMission(
		MapID::Riverside_Province, QuestID::ZaishenMission::Riverside_Province));
	prophecies_missions.push_back(new PropheciesMission(
		MapID::Sanctum_Cay, QuestID::ZaishenMission::Sanctum_Cay));
	prophecies_missions.push_back(new PropheciesMission(
		MapID::Dunes_of_Despair, QuestID::ZaishenMission::Dunes_of_Despair));
	prophecies_missions.push_back(new PropheciesMission(
		MapID::Thirsty_River, QuestID::ZaishenMission::Thirsty_River));
	prophecies_missions.push_back(new PropheciesMission(
		MapID::Elona_Reach, QuestID::ZaishenMission::Elona_Reach));
	prophecies_missions.push_back(new PropheciesMission(
		MapID::Augury_Rock_mission, QuestID::ZaishenMission::Augury_Rock));
	prophecies_missions.push_back(new PropheciesMission(
		MapID::The_Dragons_Lair, QuestID::ZaishenMission::The_Dragons_Lair));
	prophecies_missions.push_back(new PropheciesMission(
		MapID::Ice_Caves_of_Sorrow));
	prophecies_missions.push_back(new PropheciesMission(
		MapID::Iron_Mines_of_Moladune, QuestID::ZaishenMission::Iron_Mines_of_Moladune));
	prophecies_missions.push_back(new PropheciesMission(
		MapID::Thunderhead_Keep, QuestID::ZaishenMission::Thunderhead_Keep));
	prophecies_missions.push_back(new PropheciesMission(
		MapID::Ring_of_Fire, QuestID::ZaishenMission::Ring_of_Fire));
	prophecies_missions.push_back(new PropheciesMission(
		MapID::Abaddons_Mouth, QuestID::ZaishenMission::Abaddons_Mouth));
	prophecies_missions.push_back(new PropheciesMission(
		MapID::Hells_Precipice, QuestID::ZaishenMission::Hells_Precipice));
}


void MissionsWindow::Initialize_Factions()
{
	auto& factions_missions = missions.at(Campaign::Factions);
	factions_missions.push_back(new FactionsMission(
		MapID::Minister_Chos_Estate_outpost_mission));
	factions_missions.push_back(new FactionsMission(
		MapID::Zen_Daijun_outpost_mission, QuestID::ZaishenMission::Zen_Daijun));
	factions_missions.push_back(new FactionsMission(
		MapID::Vizunah_Square_Local_Quarter_outpost));
	factions_missions.push_back(new FactionsMission(
		MapID::Vizunah_Square_Foreign_Quarter_outpost));
	factions_missions.push_back(new FactionsMission(
		MapID::Nahpui_Quarter_outpost_mission, QuestID::ZaishenMission::Naphui_Quarter));
	factions_missions.push_back(new FactionsMission(
		MapID::Tahnnakai_Temple_outpost_mission));
	factions_missions.push_back(new FactionsMission(
		MapID::Arborstone_outpost_mission));
	factions_missions.push_back(new FactionsMission(
		MapID::Boreas_Seabed_outpost_mission, QuestID::ZaishenMission::Boreas_Seabed));
	factions_missions.push_back(new FactionsMission(
		MapID::Sunjiang_District_outpost_mission));
	factions_missions.push_back(new FactionsMission(
		MapID::The_Eternal_Grove_outpost_mission, QuestID::ZaishenMission::The_Eternal_Grove));
	factions_missions.push_back(new FactionsMission(
		MapID::Gyala_Hatchery_outpost_mission, QuestID::ZaishenMission::Gyala_Hatchery));
	factions_missions.push_back(new FactionsMission(
		MapID::Unwaking_Waters_Kurzick_outpost, QuestID::ZaishenMission::Unwaking_Waters));
	factions_missions.push_back(new FactionsMission(
		MapID::Unwaking_Waters_Luxon_outpost, QuestID::ZaishenMission::Unwaking_Waters));
	factions_missions.push_back(new FactionsMission(
		MapID::Raisu_Palace_outpost_mission, QuestID::ZaishenMission::Raisu_Palace));
	factions_missions.push_back(new FactionsMission(
		MapID::Imperial_Sanctum_outpost_mission, QuestID::ZaishenMission::Imperial_Sanctum));
}


void MissionsWindow::Initialize_Nightfall()
{
	auto& nightfall_missions = missions.at(Campaign::Nightfall);
	nightfall_missions.push_back(new NightfallMission(
		MapID::Chahbek_Village, QuestID::ZaishenMission::Chabek_Village));
	nightfall_missions.push_back(new NightfallMission(
		MapID::Jokanur_Diggings, QuestID::ZaishenMission::Jokanur_Diggings));
	nightfall_missions.push_back(new NightfallMission(
		MapID::Blacktide_Den));
	nightfall_missions.push_back(new NightfallMission(
		MapID::Consulate_Docks, QuestID::ZaishenMission::Consulate_Docks));
	nightfall_missions.push_back(new NightfallMission(
		MapID::Venta_Cemetery, QuestID::ZaishenMission::Venta_Cemetary));
	nightfall_missions.push_back(new NightfallMission(
		MapID::Kodonur_Crossroads, QuestID::ZaishenMission::Kodonur_Crossroads));
	nightfall_missions.push_back(new NightfallMission(
		MapID::Pogahn_Passage, QuestID::ZaishenMission::Pogahn_Passage));
	nightfall_missions.push_back(new NightfallMission(
		MapID::Rilohn_Refuge));
	nightfall_missions.push_back(new NightfallMission(
		MapID::Moddok_Crevice, QuestID::ZaishenMission::Moddok_Crevice));
	nightfall_missions.push_back(new NightfallMission(
		MapID::Tihark_Orchard));
	nightfall_missions.push_back(new NightfallMission(
		MapID::Dasha_Vestibule));
	nightfall_missions.push_back(new NightfallMission(
		MapID::Dzagonur_Bastion, QuestID::ZaishenMission::Dzagonur_Bastion));
	nightfall_missions.push_back(new NightfallMission(
		MapID::Grand_Court_of_Sebelkeh, QuestID::ZaishenMission::Grand_Court_of_Sebelkeh));
	nightfall_missions.push_back(new NightfallMission(
		MapID::Jennurs_Horde, QuestID::ZaishenMission::Jennurs_Horde));
	nightfall_missions.push_back(new NightfallMission(
		MapID::Nundu_Bay, QuestID::ZaishenMission::Nundu_Bay));
	nightfall_missions.push_back(new NightfallMission(
		MapID::Gate_of_Desolation));
	nightfall_missions.push_back(new NightfallMission(
		MapID::Ruins_of_Morah, QuestID::ZaishenMission::Ruins_of_Morah));
	nightfall_missions.push_back(new TormentMission(
		MapID::Gate_of_Pain));
	nightfall_missions.push_back(new TormentMission(
		MapID::Gate_of_Madness));
	nightfall_missions.push_back(new TormentMission(
		MapID::Abaddons_Gate, QuestID::ZaishenMission::Abaddons_Gate));
}


void MissionsWindow::Initialize_EotN()
{
	auto& eotn_missions = missions.at(Campaign::EyeOfTheNorth);
	// Asura
	eotn_missions.push_back(new EotNMission(
		MapID::Gadds_Encampment_outpost, "Finding the Bloodstone"));
	eotn_missions.push_back(new EotNMission(
		MapID::Rata_Sum_outpost, "The Elusive Golemancer"));
	eotn_missions.push_back(new EotNMission(
		MapID::Rata_Sum_outpost, "Genius Operated Living Enchanted Manifestation",
		QuestID::ZaishenMission::G_O_L_E_M));
	// Vanguard
	eotn_missions.push_back(new EotNMission(
		MapID::Longeyes_Ledge_outpost, "Against the Charr"));
	eotn_missions.push_back(new EotNMission(
		MapID::Doomlore_Shrine_outpost, "Warband of Brothers"));
	eotn_missions.push_back(new EotNMission(
		MapID::Doomlore_Shrine_outpost, "Assault on the Stronghold",
		QuestID::ZaishenMission::Assault_on_the_Stronghold));
	// Norn
	eotn_missions.push_back(new EotNMission(
		MapID::Sifhalla_outpost, "Curse of the Nornbear", QuestID::ZaishenMission::Curse_of_the_Nornbear));
	eotn_missions.push_back(new EotNMission(
		MapID::Olafstead_outpost, "A Gate Too Far", QuestID::ZaishenMission::A_Gate_Too_Far));
	eotn_missions.push_back(new EotNMission(
		MapID::Sifhalla_outpost, "Blood Washes Blood"));
	// Destroyers
	eotn_missions.push_back(new EotNMission(
		MapID::Central_Transfer_Chamber_outpost, "Destruction's Depths",
		QuestID::ZaishenMission::Destructions_Depths));
	eotn_missions.push_back(new EotNMission(
		MapID::Central_Transfer_Chamber_outpost, "A Time for Heroes",
		QuestID::ZaishenMission::A_Time_for_Heroes));
}


void MissionsWindow::Initialize_Dungeons()
{
	using namespace QuestID::ZaishenBounty;
	auto& dungeons = missions.at(Campaign::Dungeon);
	dungeons.push_back(new Dungeon(
		MapID::Doomlore_Shrine_outpost, "Catacombs of Kathandrax", Ilsundur_Lord_of_Fire));
	dungeons.push_back(new Dungeon(
		MapID::Doomlore_Shrine_outpost, "Rragar's Menagerie", Rragar_Maneater));
	dungeons.push_back(new Dungeon(
		MapID::Doomlore_Shrine_outpost, "Cathedral of Flames", Murakai_Lady_of_the_Night));
	dungeons.push_back(new Dungeon(
		MapID::Doomlore_Shrine_outpost, "Ooze Pit"));
	dungeons.push_back(new Dungeon(
		MapID::Longeyes_Ledge_outpost, "Darkrime Delves"));
	dungeons.push_back(new Dungeon(
		MapID::Sifhalla_outpost, "Frostmaw's Burrows"));
	dungeons.push_back(new Dungeon(
		MapID::Sifhalla_outpost, "Sepulchre of Dragrimmar", Remnant_of_Antiquities));
	dungeons.push_back(new Dungeon(
		MapID::Olafstead_outpost, "Raven's Point", Plague_of_Destruction));
	dungeons.push_back(new Dungeon(
		MapID::Umbral_Grotto_outpost, "Vloxen Excavations", Zoldark_the_Unholy));
	dungeons.push_back(new Dungeon(
		MapID::Gadds_Encampment_outpost, "Bogroot Growths"));
	dungeons.push_back(new Dungeon(
		MapID::Gadds_Encampment_outpost, "Bloodstone Caves"));
	dungeons.push_back(new Dungeon(
		MapID::Vloxs_Falls, "Shards of Orr", Fendi_Nin));
	dungeons.push_back(new Dungeon(
		MapID::Rata_Sum_outpost, "Oola's Lab", TPS_Regulator_Golem));
	dungeons.push_back(new Dungeon(
		MapID::Rata_Sum_outpost, "Arachni's Haunt", Arachni));
	dungeons.push_back(new Dungeon(
		MapID::Umbral_Grotto_outpost, "Slavers' Exile", {
			Forgewight, Selvetarm, /*Justiciar_Thommis,*/ Rand_Stormweaver, Duncan_the_Black }));
	dungeons.push_back(new Dungeon(
		MapID::Gunnars_Hold_outpost, "Fronis Irontoe's Lair", {QuestID::ZaishenBounty::Fronis_Irontoe}));
	dungeons.push_back(new Dungeon(
		MapID::Umbral_Grotto_outpost, "Secret Lair of the Snowmen"));
	dungeons.push_back(new Dungeon(
		MapID::Central_Transfer_Chamber_outpost, "Heart of the Shiverpeaks", {QuestID::ZaishenBounty::Magmus}));
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

	// TODO Button at the top to go to current daily

	ImGui::SetNextWindowPosCenter(ImGuiSetCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(350, 300), ImGuiSetCond_FirstUseEver);
	if (ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {
		for (auto camp : missions) {
			if (ImGui::CollapsingHeader(CampaignName(camp.first))) {
				auto& camp_missions = camp.second;
				for (size_t i = 0; i < camp_missions.size(); i++) {
					if (i % missions_per_row > 0) {
						ImGui::SameLine();
					}
					camp_missions.at(i)->Draw(device);
				}
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
