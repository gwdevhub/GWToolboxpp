#include "stdafx.h"
#include "MissionsWindow.h"

#include <GWCA/Constants/Constants.h>

#include <GWCA/GameEntities/Quest.h>
#include <GWCA/GameEntities/Map.h>
#include <GWCA/GameContainers/GamePos.h>

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
Mission::MissionImageList Vanquish::hard_mode_images({
	{L"VanquishIncomplete.png", IDB_Missions_VanquishIncomplete},
	{L"Vanquish.png", IDB_Missions_Vanquish},
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
	const Mission::MissionImageList& _normal_mode_images,
	const Mission::MissionImageList& _hard_mode_images,
	uint32_t _zm_quest)
	: outpost(_outpost), zm_quest(_zm_quest), normal_mode_textures(_normal_mode_images), hard_mode_textures(_hard_mode_images) {
	map_to = outpost;
	};

GW::Constants::MapID Mission::GetOutpost() {
	GW::AreaInfo* this_map = GW::Map::GetMapInfo(map_to);
	GW::AreaInfo* nearest = nullptr;
	GW::AreaInfo* map_info = nullptr;
	float nearest_distance = FLT_MAX;
	GW::Constants::MapID nearest_map_id = GW::Constants::MapID::None;
	GW::Vec2f this_pos = { (float)this_map->icon_start_x,(float)this_map->icon_start_y };
	if (!this_pos.x) {
		this_pos = { (float)this_map->x,(float)this_map->y };
	}
	for (size_t i = 0; i < static_cast<size_t>(GW::Constants::MapID::Count); i++) {
		map_info = GW::Map::GetMapInfo(static_cast<GW::Constants::MapID>(i));
		if (!map_info || !map_info->thumbnail_id || !map_info->GetIsOnWorldMap())
			continue;
		if (map_info->campaign != this_map->campaign || map_info->region == GW::Region_Presearing)
			continue;
		switch (map_info->type) {
		case GW::RegionType::RegionType_City:
		case GW::RegionType::RegionType_CompetitiveMission:
		case GW::RegionType::RegionType_CooperativeMission:
		case GW::RegionType::RegionType_EliteMission:
		case GW::RegionType::RegionType_MissionOutpost:
		case GW::RegionType::RegionType_Outpost:
			break;
		default:
			continue;
		}
		if (!TravelWindow::Instance().IsMapUnlocked(static_cast<GW::Constants::MapID>(i)))
			continue;
		float dist = GW::GetDistance(this_pos, { (float)map_info->icon_start_x,(float)map_info->icon_start_y });
		if (dist < nearest_distance) {
			nearest_distance = dist;
			nearest = map_info;
			nearest_map_id = static_cast<GW::Constants::MapID>(i);
		}
	}
	return nearest_map_id;
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
	ImGui::PushID((int)outpost);
	if (ImGui::ImageButton((ImTextureID)texture, s, uv0, uv1, -1, bg, tint)) {
		GW::Constants::MapID travel_to = GetOutpost();
		if (travel_to == GW::Constants::MapID::None) {
			Log::Error("Failed to find nearest outpost");
		}
		else {
			TravelWindow::Instance().Travel(travel_to, GW::Constants::District::Current, 0);
		}
		
	}
	ImGui::PopID();
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

	return texture_list->at(index).texture;
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
bool EotNMission::IsCompleted() {
	bool hardmode = GW::PartyMgr::GetIsPartyInHardMode();
	GW::WorldContext* ctx = GW::GameContext::instance()->world;
	auto* missions_bonus = &ctx->missions_bonus;

	if (hardmode) {
		missions_bonus = &ctx->missions_bonus_hm;
	}
	return ArrayBoolAt(*missions_bonus, static_cast<uint32_t>(outpost));
}
bool Vanquish::IsCompleted() {
	return ArrayBoolAt(GW::GameContext::instance()->world->vanquished_areas, static_cast<uint32_t>(outpost));
}
IDirect3DTexture9* Vanquish::GetMissionImage()
{
	return hard_mode_textures.at(ArrayBoolAt(GW::GameContext::instance()->world->vanquished_areas, static_cast<uint32_t>(outpost))).texture;
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
	auto* texture_list = &normal_mode_textures;
	GW::WorldContext* ctx = GW::GameContext::instance()->world;
	auto* missions_complete = &ctx->missions_bonus;
	if (hardmode) {
		texture_list = &hard_mode_textures;
		missions_complete = &ctx->missions_bonus_hm;
	}
	uint32_t index = ArrayBoolAt(*missions_complete, static_cast<uint32_t>(outpost)) ? 1 : 0;
	return texture_list->at(index).texture;
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


void LoadTextures(std::vector<MissionImage>& mission_images) {
	Resources::EnsureFolderExists(Resources::GetPath(L"img", L"missions"));
	for (auto& mission_image : mission_images) {
		if (mission_image.texture)
			continue;
		Resources::Instance().LoadTextureAsync(
			&mission_image.texture,
			Resources::GetPath(L"img/missions", mission_image.file_name),
			(WORD)mission_image.resource_id
		);
	}
}

void MissionsWindow::Initialize_Prophecies()
{
	LoadTextures(PropheciesMission::normal_mode_images);
	LoadTextures(PropheciesMission::hard_mode_images);

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

	LoadTextures(Vanquish::hard_mode_images);

	auto& prophecies_vanquishes = vanquishes.at(Campaign::Prophecies);
	prophecies_vanquishes.push_back(new Vanquish(MapID::Pockmark_Flats));
	prophecies_vanquishes.push_back(new Vanquish(MapID::Old_Ascalon));
	prophecies_vanquishes.push_back(new Vanquish(MapID::Regent_Valley));
	prophecies_vanquishes.push_back(new Vanquish(MapID::Ascalon_Foothills));
	prophecies_vanquishes.push_back(new Vanquish(MapID::Diessa_Lowlands));
	prophecies_vanquishes.push_back(new Vanquish(MapID::Dragons_Gullet));
	prophecies_vanquishes.push_back(new Vanquish(MapID::Eastern_Frontier));
	prophecies_vanquishes.push_back(new Vanquish(MapID::Flame_Temple_Corridor));
	prophecies_vanquishes.push_back(new Vanquish(MapID::The_Breach));
	prophecies_vanquishes.push_back(new Vanquish(MapID::Anvil_Rock));
	prophecies_vanquishes.push_back(new Vanquish(MapID::Deldrimor_Bowl));
	prophecies_vanquishes.push_back(new Vanquish(MapID::Griffons_Mouth));
	prophecies_vanquishes.push_back(new Vanquish(MapID::Iron_Horse_Mine));
	prophecies_vanquishes.push_back(new Vanquish(MapID::Travelers_Vale));
	prophecies_vanquishes.push_back(new Vanquish(MapID::Cursed_Lands));
	prophecies_vanquishes.push_back(new Vanquish(MapID::Kessex_Peak));
	prophecies_vanquishes.push_back(new Vanquish(MapID::Majestys_Rest));
	prophecies_vanquishes.push_back(new Vanquish(MapID::Nebo_Terrace));
	prophecies_vanquishes.push_back(new Vanquish(MapID::North_Kryta_Province));
	prophecies_vanquishes.push_back(new Vanquish(MapID::Scoundrels_Rise));
	prophecies_vanquishes.push_back(new Vanquish(MapID::Stingray_Strand));
	prophecies_vanquishes.push_back(new Vanquish(MapID::Talmark_Wilderness));
	prophecies_vanquishes.push_back(new Vanquish(MapID::Tears_of_the_Fallen));
	prophecies_vanquishes.push_back(new Vanquish(MapID::The_Black_Curtain));
	prophecies_vanquishes.push_back(new Vanquish(MapID::Twin_Serpent_Lakes));
	prophecies_vanquishes.push_back(new Vanquish(MapID::Watchtower_Coast));
	prophecies_vanquishes.push_back(new Vanquish(MapID::Dry_Top));
	prophecies_vanquishes.push_back(new Vanquish(MapID::Ettins_Back));
	prophecies_vanquishes.push_back(new Vanquish(MapID::Mamnoon_Lagoon));
	prophecies_vanquishes.push_back(new Vanquish(MapID::Reed_Bog));
	prophecies_vanquishes.push_back(new Vanquish(MapID::Sage_Lands));
	prophecies_vanquishes.push_back(new Vanquish(MapID::Silverwood));
	prophecies_vanquishes.push_back(new Vanquish(MapID::Tangle_Root));
	prophecies_vanquishes.push_back(new Vanquish(MapID::The_Falls));
	prophecies_vanquishes.push_back(new Vanquish(MapID::Diviners_Ascent));
	prophecies_vanquishes.push_back(new Vanquish(MapID::Prophets_Path));
	prophecies_vanquishes.push_back(new Vanquish(MapID::Salt_Flats));
	prophecies_vanquishes.push_back(new Vanquish(MapID::Skyward_Reach));
	prophecies_vanquishes.push_back(new Vanquish(MapID::The_Arid_Sea));
	prophecies_vanquishes.push_back(new Vanquish(MapID::The_Scar));
	prophecies_vanquishes.push_back(new Vanquish(MapID::Vulture_Drifts));
	prophecies_vanquishes.push_back(new Vanquish(MapID::Dreadnoughts_Drift));
	prophecies_vanquishes.push_back(new Vanquish(MapID::Frozen_Forest));
	prophecies_vanquishes.push_back(new Vanquish(MapID::Grenths_Footprint));
	prophecies_vanquishes.push_back(new Vanquish(MapID::Ice_Floe));
	prophecies_vanquishes.push_back(new Vanquish(MapID::Icedome));
	prophecies_vanquishes.push_back(new Vanquish(MapID::Lornars_Pass));
	prophecies_vanquishes.push_back(new Vanquish(MapID::Mineral_Springs));
	prophecies_vanquishes.push_back(new Vanquish(MapID::Snake_Dance));
	prophecies_vanquishes.push_back(new Vanquish(MapID::Spearhead_Peak));
	prophecies_vanquishes.push_back(new Vanquish(MapID::Talus_Chute));
	prophecies_vanquishes.push_back(new Vanquish(MapID::Tascas_Demise));
	prophecies_vanquishes.push_back(new Vanquish(MapID::Witmans_Folly));
	prophecies_vanquishes.push_back(new Vanquish(MapID::Perdition_Rock));
}


void MissionsWindow::Initialize_Factions()
{
	LoadTextures(FactionsMission::normal_mode_images);
	LoadTextures(FactionsMission::hard_mode_images);

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

	LoadTextures(Vanquish::hard_mode_images);

	auto& this_vanquishes = vanquishes.at(Campaign::Factions);
	this_vanquishes.push_back(new Vanquish(MapID::Haiju_Lagoon));
	this_vanquishes.push_back(new Vanquish(MapID::Jaya_Bluffs));
	this_vanquishes.push_back(new Vanquish(MapID::Kinya_Province));
	this_vanquishes.push_back(new Vanquish(MapID::Minister_Chos_Estate_explorable));
	this_vanquishes.push_back(new Vanquish(MapID::Panjiang_Peninsula));
	this_vanquishes.push_back(new Vanquish(MapID::Saoshang_Trail));
	this_vanquishes.push_back(new Vanquish(MapID::Sunqua_Vale));
	this_vanquishes.push_back(new Vanquish(MapID::Zen_Daijun_explorable));
	this_vanquishes.push_back(new Vanquish(MapID::Bukdek_Byway));
	this_vanquishes.push_back(new Vanquish(MapID::Nahpui_Quarter_explorable));
	this_vanquishes.push_back(new Vanquish(MapID::Pongmei_Valley));
	this_vanquishes.push_back(new Vanquish(MapID::Raisu_Palace));
	this_vanquishes.push_back(new Vanquish(MapID::Shadows_Passage));
	this_vanquishes.push_back(new Vanquish(MapID::Shenzun_Tunnels));
	this_vanquishes.push_back(new Vanquish(MapID::Sunjiang_District_explorable));
	this_vanquishes.push_back(new Vanquish(MapID::Tahnnakai_Temple_explorable));
	this_vanquishes.push_back(new Vanquish(MapID::Wajjun_Bazaar));
	this_vanquishes.push_back(new Vanquish(MapID::Xaquang_Skyway));
	this_vanquishes.push_back(new Vanquish(MapID::Arborstone_explorable));
	this_vanquishes.push_back(new Vanquish(MapID::Drazach_Thicket));
	this_vanquishes.push_back(new Vanquish(MapID::Ferndale));
	this_vanquishes.push_back(new Vanquish(MapID::Melandrus_Hope));
	this_vanquishes.push_back(new Vanquish(MapID::Morostav_Trail));
	this_vanquishes.push_back(new Vanquish(MapID::Mourning_Veil_Falls));
	this_vanquishes.push_back(new Vanquish(MapID::The_Eternal_Grove));
	this_vanquishes.push_back(new Vanquish(MapID::Archipelagos));
	this_vanquishes.push_back(new Vanquish(MapID::Boreas_Seabed_explorable));
	this_vanquishes.push_back(new Vanquish(MapID::Gyala_Hatchery));
	this_vanquishes.push_back(new Vanquish(MapID::Maishang_Hills));
	this_vanquishes.push_back(new Vanquish(MapID::Mount_Qinkai));
	this_vanquishes.push_back(new Vanquish(MapID::Rheas_Crater));
	this_vanquishes.push_back(new Vanquish(MapID::Silent_Surf));
	this_vanquishes.push_back(new Vanquish(MapID::Unwaking_Waters));
}


void MissionsWindow::Initialize_Nightfall()
{
	LoadTextures(NightfallMission::normal_mode_images);
	LoadTextures(NightfallMission::hard_mode_images);

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

	LoadTextures(Vanquish::hard_mode_images);

	auto& this_vanquishes = vanquishes.at(Campaign::Nightfall);
	this_vanquishes.push_back(new Vanquish(MapID::Cliffs_of_Dohjok));
	this_vanquishes.push_back(new Vanquish(MapID::Fahranur_The_First_City));
	this_vanquishes.push_back(new Vanquish(MapID::Issnur_Isles));
	this_vanquishes.push_back(new Vanquish(MapID::Lahtenda_Bog));
	this_vanquishes.push_back(new Vanquish(MapID::Mehtani_Keys));
	this_vanquishes.push_back(new Vanquish(MapID::Plains_of_Jarin));
	this_vanquishes.push_back(new Vanquish(MapID::Zehlon_Reach));
	this_vanquishes.push_back(new Vanquish(MapID::Arkjok_Ward));
	this_vanquishes.push_back(new Vanquish(MapID::Bahdok_Caverns));
	this_vanquishes.push_back(new Vanquish(MapID::Barbarous_Shore));
	this_vanquishes.push_back(new Vanquish(MapID::Dejarin_Estate));
	this_vanquishes.push_back(new Vanquish(MapID::Gandara_the_Moon_Fortress));
	this_vanquishes.push_back(new Vanquish(MapID::Jahai_Bluffs));
	this_vanquishes.push_back(new Vanquish(MapID::Marga_Coast));
	this_vanquishes.push_back(new Vanquish(MapID::Sunward_Marches));
	this_vanquishes.push_back(new Vanquish(MapID::The_Floodplain_of_Mahnkelon));
	this_vanquishes.push_back(new Vanquish(MapID::Turais_Procession));
	this_vanquishes.push_back(new Vanquish(MapID::Forum_Highlands));
	this_vanquishes.push_back(new Vanquish(MapID::Garden_of_Seborhin));
	this_vanquishes.push_back(new Vanquish(MapID::Holdings_of_Chokhin));
	this_vanquishes.push_back(new Vanquish(MapID::Resplendent_Makuun));
	this_vanquishes.push_back(new Vanquish(MapID::The_Hidden_City_of_Ahdashim));
	this_vanquishes.push_back(new Vanquish(MapID::The_Mirror_of_Lyss));
	this_vanquishes.push_back(new Vanquish(MapID::Vehjin_Mines));
	this_vanquishes.push_back(new Vanquish(MapID::Vehtendi_Valley));
	this_vanquishes.push_back(new Vanquish(MapID::Wilderness_of_Bahdza));
	this_vanquishes.push_back(new Vanquish(MapID::Yatendi_Canyons));
	this_vanquishes.push_back(new Vanquish(MapID::Crystal_Overlook));
	this_vanquishes.push_back(new Vanquish(MapID::Jokos_Domain));
	this_vanquishes.push_back(new Vanquish(MapID::Poisoned_Outcrops));
	this_vanquishes.push_back(new Vanquish(MapID::The_Alkali_Pan));
	this_vanquishes.push_back(new Vanquish(MapID::The_Ruptured_Heart));
	this_vanquishes.push_back(new Vanquish(MapID::The_Shattered_Ravines));
	this_vanquishes.push_back(new Vanquish(MapID::The_Sulfurous_Wastes));
}


void MissionsWindow::Initialize_EotN()
{
	LoadTextures(EotNMission::normal_mode_images);
	LoadTextures(EotNMission::hard_mode_images);
	auto& eotn_missions = missions.at(Campaign::EyeOfTheNorth);
	// Asura
	eotn_missions.push_back(new EotNMission(MapID::Finding_the_Bloodstone_mission));
	eotn_missions.push_back(new EotNMission(MapID::The_Elusive_Golemancer_mission));
	eotn_missions.push_back(new EotNMission(MapID::Genius_Operated_Living_Enchanted_Manifestation_mission,QuestID::ZaishenMission::G_O_L_E_M));
	// Vanguard
	eotn_missions.push_back(new EotNMission(MapID::Against_the_Charr_mission));
	eotn_missions.push_back(new EotNMission(MapID::Warband_of_brothers_mission));
	eotn_missions.push_back(new EotNMission(MapID::Assault_on_the_Stronghold_mission, QuestID::ZaishenMission::Assault_on_the_Stronghold));
	// Norn
	eotn_missions.push_back(new EotNMission(MapID::Curse_of_the_Nornbear_mission, QuestID::ZaishenMission::Curse_of_the_Nornbear));
	eotn_missions.push_back(new EotNMission(MapID::A_Gate_Too_Far_mission, QuestID::ZaishenMission::A_Gate_Too_Far));
	eotn_missions.push_back(new EotNMission(MapID::Blood_Washes_Blood_mission));
	// Destroyers
	eotn_missions.push_back(new EotNMission(MapID::Destructions_Depths_mission,QuestID::ZaishenMission::Destructions_Depths));
	eotn_missions.push_back(new EotNMission(MapID::A_Time_for_Heroes_mission, QuestID::ZaishenMission::A_Time_for_Heroes));

	LoadTextures(Vanquish::hard_mode_images);

	auto& this_vanquishes = vanquishes.at(Campaign::EyeOfTheNorth);
	this_vanquishes.push_back(new Vanquish(MapID::Bjora_Marches));
	this_vanquishes.push_back(new Vanquish(MapID::Drakkar_Lake));
	this_vanquishes.push_back(new Vanquish(MapID::Ice_Cliff_Chasms));
	this_vanquishes.push_back(new Vanquish(MapID::Jaga_Moraine));
	this_vanquishes.push_back(new Vanquish(MapID::Norrhart_Domains));
	this_vanquishes.push_back(new Vanquish(MapID::Varajar_Fells));
	this_vanquishes.push_back(new Vanquish(MapID::Dalada_Uplands));
	this_vanquishes.push_back(new Vanquish(MapID::Grothmar_Wardowns));
	this_vanquishes.push_back(new Vanquish(MapID::Sacnoth_Valley));
	this_vanquishes.push_back(new Vanquish(MapID::Alcazia_Tangle));
	this_vanquishes.push_back(new Vanquish(MapID::Arbor_Bay));
	this_vanquishes.push_back(new Vanquish(MapID::Magus_Stones));
	this_vanquishes.push_back(new Vanquish(MapID::Riven_Earth));
	this_vanquishes.push_back(new Vanquish(MapID::Sparkfly_Swamp));
	this_vanquishes.push_back(new Vanquish(MapID::Verdant_Cascades));
}


void MissionsWindow::Initialize_Dungeons()
{
	LoadTextures(Dungeon::normal_mode_images);
	LoadTextures(Dungeon::hard_mode_images);
	using namespace QuestID::ZaishenBounty;
	auto& dungeons = missions.at(Campaign::Dungeon);
	dungeons.push_back(new Dungeon(
		MapID::Catacombs_of_Kathandrax_Level_1, Ilsundur_Lord_of_Fire));
	dungeons.push_back(new Dungeon(
		MapID::Rragars_Menagerie_Level_1, Rragar_Maneater));
	dungeons.push_back(new Dungeon(
		MapID::Cathedral_of_Flames_Level_1, Murakai_Lady_of_the_Night));
	dungeons.push_back(new Dungeon(
		MapID::Ooze_Pit_mission));
	dungeons.push_back(new Dungeon(
		MapID::Darkrime_Delves_Level_1));
	dungeons.push_back(new Dungeon(
		MapID::Frostmaws_Burrows_Level_1));
	dungeons.push_back(new Dungeon(
		MapID::Sepulchre_of_Dragrimmar_Level_1, Remnant_of_Antiquities));
	dungeons.push_back(new Dungeon(
		MapID::Ravens_Point_Level_1, Plague_of_Destruction));
	dungeons.push_back(new Dungeon(
		MapID::Vloxen_Excavations_Level_1 , Zoldark_the_Unholy));
	dungeons.push_back(new Dungeon(
		MapID::Bogroot_Growths_Level_1));
	dungeons.push_back(new Dungeon(
		MapID::Bloodstone_Caves_Level_1));
	dungeons.push_back(new Dungeon(
		MapID::Shards_of_Orr_Level_1, Fendi_Nin));
	dungeons.push_back(new Dungeon(
		MapID::Oolas_Lab_Level_1, TPS_Regulator_Golem));
	dungeons.push_back(new Dungeon(
		MapID::Arachnis_Haunt_Level_1, Arachni));
	dungeons.push_back(new Dungeon(
		MapID::Slavers_Exile_Level_1, {
			Forgewight, Selvetarm, /*Justiciar_Thommis,*/ Rand_Stormweaver, Duncan_the_Black }));
	dungeons.push_back(new Dungeon(
		MapID::Fronis_Irontoes_Lair_mission, { QuestID::ZaishenBounty::Fronis_Irontoe }));
	dungeons.push_back(new Dungeon(
		MapID::Secret_Lair_of_the_Snowmen));
	dungeons.push_back(new Dungeon(
		MapID::Heart_of_the_Shiverpeaks_Level_1, { QuestID::ZaishenBounty::Magmus }));
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
			snprintf(label, _countof(label), "%s (%d of %d completed) - %.0f%%", CampaignName(camp.first), completed, camp_missions.size(), ((float)completed / (float)camp_missions.size()) * 100.f);
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
		ImGui::Text("Vanquishes");
		for (auto& camp : vanquishes) {
			auto& camp_missions = camp.second;
			if (!camp_missions.size())
				break;
			size_t completed = 0;
			for (size_t i = 0; i < camp_missions.size(); i++) {
				if (camp_missions[i]->IsCompleted())
					completed++;
			}
			char label[128];
			snprintf(label, _countof(label), "%s (%d of %d completed) - %.0f%%", CampaignName(camp.first), completed, camp_missions.size(), ((float)completed / (float)camp_missions.size()) * 100.f);
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