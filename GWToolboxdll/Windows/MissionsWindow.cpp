#include "stdafx.h"
#include "MissionsWindow.h"

#include <GWCA/Constants/Constants.h>

#include <GWCA/GameEntities/Quest.h>
#include <GWCA/GameEntities/Map.h>

#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <Modules/Resources.h>

#include <Windows/MissionsWindow.h>
#include <Windows/TravelWindow.h>

using namespace GW::Constants;
using namespace Missions;

namespace {

	const char* campaign_names[] = {
		"Prophecies",
		"Factions",
		"Nightfall",
		"Eye Of The North",
		"Dungeons"
	};

	const char* CampaignName(const Campaign camp) {
		return campaign_names[static_cast<uint8_t>(camp)];
	}
}

Mission::MissionImageList PropheciesMission::normal_mode_images({
	{L"MissionIconIncomplete.png", IDB_Missions_MissionIconIncomplete},
	{L"MissionIconPrimary.png", IDB_Missions_MissionIconPrimary},
	{L"MissionIconBonus.png", IDB_Missions_MissionIconBonus},
	{L"MissionIcon.png", IDB_Missions_MissionIcon},
	});
Mission::MissionImageList PropheciesMission::hard_mode_images({
	{L"HardModeMissionIconIncomplete.png", IDB_Missions_HardModeMissionIconIncomplete},
	{L"HardModeMissionIcon1.png", IDB_Missions_HardModeMissionIcon1},
	{L"HardModeMissionIcon1b.png", IDB_Missions_HardModeMissionIcon1b},
	{L"HardModeMissionIcon2.png", IDB_Missions_HardModeMissionIcon2},
	});

Mission::MissionImageList FactionsMission::normal_mode_images({
	{L"FactionsMissionIconIncomplete.png", IDB_Missions_FactionsMissionIconIncomplete},
	{L"FactionsMissionIconPrimary.png", IDB_Missions_FactionsMissionIconPrimary},
	{L"FactionsMissionIconExpert.png", IDB_Missions_FactionsMissionIconExpert},
	{L"FactionsMissionIcon.png", IDB_Missions_FactionsMissionIcon},
	});
Mission::MissionImageList FactionsMission::hard_mode_images({
	{L"HardModeMissionIconIncomplete.png", IDB_Missions_HardModeMissionIconIncomplete},
	{L"HardModeMissionIcon1.png", IDB_Missions_HardModeMissionIcon1},
	{L"HardModeMissionIcon2.png", IDB_Missions_HardModeMissionIcon2},
	{L"HardModeMissionIcon.png", IDB_Missions_HardModeMissionIcon},
	});

Mission::MissionImageList NightfallMission::normal_mode_images({
	{L"NightfallMissionIconIncomplete.png", IDB_Missions_NightfallMissionIconIncomplete},
	{L"NightfallMissionIconPrimary.png", IDB_Missions_NightfallMissionIconPrimary},
	{L"NightfallMissionIconExpert.png", IDB_Missions_NightfallMissionIconExpert},
	{L"NightfallMissionIcon.png", IDB_Missions_NightfallMissionIcon},
	});
Mission::MissionImageList NightfallMission::hard_mode_images({
	{L"HardModeMissionIconIncomplete.png", IDB_Missions_HardModeMissionIconIncomplete},
	{L"HardModeMissionIcon1.png", IDB_Missions_HardModeMissionIcon1},
	{L"HardModeMissionIcon2.png", IDB_Missions_HardModeMissionIcon2},
	{L"HardModeMissionIcon.png", IDB_Missions_HardModeMissionIcon},
	});

Mission::MissionImageList TormentMission::normal_mode_images({
	{L"NightfallTormentMissionIconIncomplete.png", IDB_Missions_NightfallTormentMissionIconIncomplete},
	{L"NightfallTormentMissionIconPrimary.png", IDB_Missions_NightfallTormentMissionIconPrimary},
	{L"NightfallTormentMissionIconExpert.png", IDB_Missions_NightfallTormentMissionIconExpert},
	{L"NightfallTormentMissionIcon.png", IDB_Missions_NightfallTormentMissionIcon},
	});
Mission::MissionImageList TormentMission::hard_mode_images({
	{L"HardModeMissionIconIncomplete.png", IDB_Missions_HardModeMissionIconIncomplete},
	{L"HardModeMissionIcon1.png", IDB_Missions_HardModeMissionIcon1},
	{L"HardModeMissionIcon2.png", IDB_Missions_HardModeMissionIcon2},
	{L"HardModeMissionIcon.png", IDB_Missions_HardModeMissionIcon},
	});

Mission::MissionImageList EotNMission::normal_mode_images({
	{L"EOTNMissionIncomplete.png", IDB_Missions_EOTNMissionIncomplete},
	{L"EOTNMission.png", IDB_Missions_EOTNMission},
	});
Mission::MissionImageList EotNMission::hard_mode_images({
	{L"EOTNHardModeMissionIncomplete.png", IDB_Missions_EOTNHardModeMissionIncomplete},
	{L"EOTNHardModeMission.png", IDB_Missions_EOTNHardModeMission},
	});

Mission::MissionImageList Dungeon::normal_mode_images({
	{L"EOTNDungeonIncomplete.png", IDB_Missions_EOTNDungeonIncomplete},
	{L"EOTNDungeon.png", IDB_Missions_EOTNDungeon},
	});
Mission::MissionImageList Dungeon::hard_mode_images({
	{L"EOTNHardModeDungeonIncomplete.png", IDB_Missions_EOTNHardModeDungeonIncomplete},
	{L"EOTNDungeon.png", IDB_Missions_EOTNDungeon},
	});


Color Mission::is_daily_bg_color = Colors::ARGB(102, 0, 255, 0);
Color Mission::has_quest_bg_color = Colors::ARGB(102, 0, 150, 0);
float Mission::icon_size = 48.0f;

static bool ArrayBoolAt(GW::Array<uint32_t>& array, uint32_t index)
{
	uint32_t real_index = index / 32;
	if (real_index >= array.size())
		return false;
	uint32_t shift = index % 32;
	uint32_t flag = 1 << shift;
	return (array[real_index] & flag) != 0;
}


Mission::Mission(GW::Constants::MapID _outpost,
	const Mission::MissionImageList& normal_mode_images,
	const Mission::MissionImageList& hard_mode_images,
	uint32_t _zm_quest)
	: outpost(_outpost), zm_quest(_zm_quest)
{
	normal_mode_textures.assign(normal_mode_images.size(), nullptr);
	hard_mode_textures.assign(hard_mode_images.size(), nullptr);
	for (size_t i = 0; i < normal_mode_images.size(); i++) {
		const auto& normal_image_pair = normal_mode_images.at(i);
		Resources::EnsureFolderExists(Resources::GetPath(L"img", L"missions"));
		Resources::Instance().LoadTextureAsync(
			&normal_mode_textures.at(i),
			Resources::GetPath(L"img/missions", normal_image_pair.first),
			(WORD)normal_image_pair.second
		);

		const auto& hard_image_pair = hard_mode_images.at(i);
		Resources::Instance().LoadTextureAsync(
			&hard_mode_textures.at(i),
			Resources::GetPath(L"img/missions", hard_image_pair.first),
			(WORD)hard_image_pair.second
		);
	}
}


void Mission::Draw(IDirect3DDevice9* )
{
	auto texture = GetMissionImage();
	if (texture == nullptr) return;

	const float scale = ImGui::GetIO().FontGlobalScale;

	ImVec2 s(icon_size * scale, icon_size * scale);
	ImVec4 bg = ImVec4(0, 0, 0, 0);
	if (IsDaily()) {
		bg = ImColor(is_daily_bg_color);
	}
	else if (HasQuest()) {
		bg = ImColor(has_quest_bg_color);
	}
	ImVec4 tint(1, 1, 1, 1);
	ImVec2 uv0 = ImVec2(0, 0);
	ImVec2 uv1 = ImVec2(1, 1);

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
	if (ImGui::ImageButton((ImTextureID)texture, s, uv0, uv1, -1, bg, tint)) {
		TravelWindow::Instance().Travel(outpost, GW::Constants::District::Current, 0);
	}
	if (ImGui::IsItemHovered()) ImGui::SetTooltip(Name().c_str());
	ImGui::PopStyleColor();
}


IDirect3DTexture9* Mission::GetMissionImage()
{
	bool hardmode = GW::PartyMgr::GetIsPartyInHardMode();
	GW::WorldContext* ctx = GW::GameContext::instance()->world;
	auto* missions_complete = &ctx->missions_completed;
	auto* missions_bonus = &ctx->missions_bonus;
	auto* texture_list = &normal_mode_textures;

	if (hardmode) {
		missions_complete = &ctx->missions_completed_hm;
		missions_bonus = &ctx->missions_bonus_hm;
		texture_list = &hard_mode_textures;
	}


	bool complete = ArrayBoolAt(*missions_complete, static_cast<uint32_t>(outpost));
	bool bonus = ArrayBoolAt(*missions_bonus, static_cast<uint32_t>(outpost));
	uint8_t index = complete + 2 * bonus;

	return texture_list->at(index);
}
bool Mission::IsCompleted() {
	bool hardmode = GW::PartyMgr::GetIsPartyInHardMode();
	GW::WorldContext* ctx = GW::GameContext::instance()->world;
	auto* missions_complete = &ctx->missions_completed;
	auto* missions_bonus = &ctx->missions_bonus;

	if (hardmode) {
		missions_complete = &ctx->missions_completed_hm;
		missions_bonus = &ctx->missions_bonus_hm;
	}
	return ArrayBoolAt(*missions_complete, static_cast<uint32_t>(outpost)) && ArrayBoolAt(*missions_bonus, static_cast<uint32_t>(outpost));
}


bool Mission::IsDaily()
{
	return false;
}

std::string& Mission::Name() {
	if (!enc_mission_name[0]) {
		GW::AreaInfo* map_info = GW::Map::GetMapInfo(outpost);
		if (map_info && GW::UI::UInt32ToEncStr(map_info->name_id, enc_mission_name, _countof(enc_mission_name)))
			GW::UI::AsyncDecodeStr(enc_mission_name, &mission_name);
	}
	if (!mission_name.empty() && mission_name_s.empty()) {
		mission_name_s = GuiUtils::WStringToString(mission_name);
	}
	return mission_name_s;
}

bool Mission::HasQuest()
{
	GW::WorldContext* ctx = GW::GameContext::instance()->world;
	const auto& quests = ctx->quest_log;
	for (size_t i = 0; i < quests.size(); i++) {
		GW::Quest q = quests[i];
		if (zm_quest != 0 && q.quest_id == zm_quest) {
			return true;
		}
	}
	return false;
}


IDirect3DTexture9* EotNMission::GetMissionImage()
{
	//TODO do these work like proph, factions, nf?
	bool hardmode = GW::PartyMgr::GetIsPartyInHardMode();
	auto& texture_list = normal_mode_textures;
	if (hardmode) {
		texture_list = hard_mode_textures;
	}
	return texture_list.at(0);
}


std::string EotNMission::Name() {
	std::string label(name);
	label += "\n(";
	label += Mission::Name();
	label += ")";
	return label;
}


bool Dungeon::IsDaily()
{
	return false;
}


bool Dungeon::HasQuest()
{
	GW::WorldContext* ctx = GW::GameContext::instance()->world;
	const auto& quests = ctx->quest_log;
	for (size_t i = 0; i < quests.size(); i++) {
		const GW::Quest& q = quests[i];
		for (auto& zb : zb_quests) {
			if (zb != 0 && q.quest_id == zb) {
				return true;
			}
		}
	}
	return false;
}

void MissionsWindow::Initialize()
{
	ToolboxWindow::Initialize();
	//Resources::Instance().LoadTextureAsync(&button_texture, Resources::GetPath(L"img/missions", L"MissionIcon.png"), IDB_Missions_MissionIcon);

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
		MapID::The_Great_Northern_Wall, QuestID::ZaishenMission::The_Great_Northern_Wall));
	prophecies_missions.push_back(new PropheciesMission(
		MapID::Fort_Ranik, QuestID::ZaishenMission::Fort_Ranik));
	prophecies_missions.push_back(new PropheciesMission(
		MapID::Ruins_of_Surmia, QuestID::ZaishenMission::Ruins_of_Surmia));
	prophecies_missions.push_back(new PropheciesMission(
		MapID::Nolani_Academy, QuestID::ZaishenMission::Nolani_Academy));
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
		MapID::Aurora_Glade, QuestID::ZaishenMission::Aurora_Glade));
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
		MapID::Ice_Caves_of_Sorrow, QuestID::ZaishenMission::Ice_Caves_of_Sorrow));
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
		MapID::Minister_Chos_Estate_outpost_mission, QuestID::ZaishenMission::Minister_Chos_Estate));
	factions_missions.push_back(new FactionsMission(
		MapID::Zen_Daijun_outpost_mission, QuestID::ZaishenMission::Zen_Daijun));
	factions_missions.push_back(new FactionsMission(
		MapID::Vizunah_Square_Local_Quarter_outpost, QuestID::ZaishenMission::Vizunah_Square));
	factions_missions.push_back(new FactionsMission(
		MapID::Vizunah_Square_Foreign_Quarter_outpost, QuestID::ZaishenMission::Vizunah_Square));
	factions_missions.push_back(new FactionsMission(
		MapID::Nahpui_Quarter_outpost_mission, QuestID::ZaishenMission::Nahpui_Quarter));
	factions_missions.push_back(new FactionsMission(
		MapID::Tahnnakai_Temple_outpost_mission, QuestID::ZaishenMission::Tahnnakai_Temple));
	factions_missions.push_back(new FactionsMission(
		MapID::Arborstone_outpost_mission, QuestID::ZaishenMission::Arborstone));
	factions_missions.push_back(new FactionsMission(
		MapID::Boreas_Seabed_outpost_mission, QuestID::ZaishenMission::Boreas_Seabed));
	factions_missions.push_back(new FactionsMission(
		MapID::Sunjiang_District_outpost_mission, QuestID::ZaishenMission::Sunjiang_District));
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
		MapID::Chahbek_Village, QuestID::ZaishenMission::Chahbek_Village));
	nightfall_missions.push_back(new NightfallMission(
		MapID::Jokanur_Diggings, QuestID::ZaishenMission::Jokanur_Diggings));
	nightfall_missions.push_back(new NightfallMission(
		MapID::Blacktide_Den, QuestID::ZaishenMission::Blacktide_Den));
	nightfall_missions.push_back(new NightfallMission(
		MapID::Consulate_Docks, QuestID::ZaishenMission::Consulate_Docks));
	nightfall_missions.push_back(new NightfallMission(
		MapID::Venta_Cemetery, QuestID::ZaishenMission::Venta_Cemetery));
	nightfall_missions.push_back(new NightfallMission(
		MapID::Kodonur_Crossroads, QuestID::ZaishenMission::Kodonur_Crossroads));
	nightfall_missions.push_back(new NightfallMission(
		MapID::Pogahn_Passage, QuestID::ZaishenMission::Pogahn_Passage));
	nightfall_missions.push_back(new NightfallMission(
		MapID::Rilohn_Refuge, QuestID::ZaishenMission::Rilohn_Refuge));
	nightfall_missions.push_back(new NightfallMission(
		MapID::Moddok_Crevice, QuestID::ZaishenMission::Moddok_Crevice));
	nightfall_missions.push_back(new NightfallMission(
		MapID::Tihark_Orchard, QuestID::ZaishenMission::Tihark_Orchard));
	nightfall_missions.push_back(new NightfallMission(
		MapID::Dasha_Vestibule, QuestID::ZaishenMission::Dasha_Vestibule));
	nightfall_missions.push_back(new NightfallMission(
		MapID::Dzagonur_Bastion, QuestID::ZaishenMission::Dzagonur_Bastion));
	nightfall_missions.push_back(new NightfallMission(
		MapID::Grand_Court_of_Sebelkeh, QuestID::ZaishenMission::Grand_Court_of_Sebelkeh));
	nightfall_missions.push_back(new NightfallMission(
		MapID::Jennurs_Horde, QuestID::ZaishenMission::Jennurs_Horde));
	nightfall_missions.push_back(new NightfallMission(
		MapID::Nundu_Bay, QuestID::ZaishenMission::Nundu_Bay));
	nightfall_missions.push_back(new NightfallMission(
		MapID::Gate_of_Desolation, QuestID::ZaishenMission::Gate_of_Desolation));
	nightfall_missions.push_back(new NightfallMission(
		MapID::Ruins_of_Morah, QuestID::ZaishenMission::Ruins_of_Morah));
	nightfall_missions.push_back(new TormentMission(
		MapID::Gate_of_Pain, QuestID::ZaishenMission::Gate_of_Pain));
	nightfall_missions.push_back(new TormentMission(
		MapID::Gate_of_Madness, QuestID::ZaishenMission::Gate_of_Madness));
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
		MapID::Gunnars_Hold_outpost, "Fronis Irontoe's Lair", { QuestID::ZaishenBounty::Fronis_Irontoe }));
	dungeons.push_back(new Dungeon(
		MapID::Umbral_Grotto_outpost, "Secret Lair of the Snowmen"));
	dungeons.push_back(new Dungeon(
		MapID::Central_Transfer_Chamber_outpost, "Heart of the Shiverpeaks", { QuestID::ZaishenBounty::Magmus }));
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
	ImGui::SetNextWindowCenter(ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(400, 0), ImGuiCond_FirstUseEver);

	if (ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {

		int missions_per_row = (int)std::floor(ImGui::GetContentRegionAvail().x / (ImGui::GetIO().FontGlobalScale * Mission::icon_size + (ImGui::GetStyle().ItemSpacing.x)));
		for (auto& camp : missions) {
			auto& camp_missions = camp.second;
			size_t completed = 0;
			for (size_t i = 0; i < camp_missions.size(); i++) {
				if (camp_missions[i]->IsCompleted())
					completed++;
			}
			char label[128];
			snprintf(label, _countof(label), "%s (%d of %d completed)", CampaignName(camp.first), completed, camp_missions.size());
			if (ImGui::CollapsingHeader(label)) {
				ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
				for (size_t i = 0; i < camp_missions.size(); i++) {
					if (i % missions_per_row > 0) {
						ImGui::SameLine();
					}
					camp_missions.at(i)->Draw(device);
				}
				ImGui::PopStyleVar();
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