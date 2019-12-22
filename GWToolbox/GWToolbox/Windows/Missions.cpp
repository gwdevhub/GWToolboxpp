#include "stdafx.h"
#include "Missions.h"
#include "TravelWindow.h"

#include <GWCA/Managers/PartyMgr.h>
#include "logger.h"

using namespace Missions;

const Mission::MissionImageList PropheciesMission::normal_mode_images({
	{L"MissionIconIncomplete.png", IDB_Missions_MissionIconIncomplete},
	{L"MissionIconPrimary.png", IDB_Missions_MissionIconPrimary},
	{L"MissionIconBonus.png", IDB_Missions_MissionIconBonus},
	{L"MissionIcon.png", IDB_Missions_MissionIcon},
});
const Mission::MissionImageList PropheciesMission::hard_mode_images({
	{L"HardModeMissionIconIncomplete.png", IDB_Missions_HardModeMissionIconIncomplete},
	{L"HardModeMissionIcon1.png", IDB_Missions_HardModeMissionIcon1},
	{L"HardModeMissionIcon1b.png", IDB_Missions_HardModeMissionIcon1b},
	{L"HardModeMissionIcon2.png", IDB_Missions_HardModeMissionIcon2},
});

const Mission::MissionImageList FactionsMission::normal_mode_images({
	{L"FactionsMissionIconIncomplete.png", IDB_Missions_FactionsMissionIconIncomplete},
	{L"FactionsMissionIconPrimary.png", IDB_Missions_FactionsMissionIconPrimary},
	{L"FactionsMissionIconExpert.png", IDB_Missions_FactionsMissionIconExpert},
	{L"FactionsMissionIcon.png", IDB_Missions_FactionsMissionIcon},
});
const Mission::MissionImageList FactionsMission::hard_mode_images({
	{L"HardModeMissionIconIncomplete.png", IDB_Missions_HardModeMissionIconIncomplete},
	{L"HardModeMissionIcon1.png", IDB_Missions_HardModeMissionIcon1},
	{L"HardModeMissionIcon2.png", IDB_Missions_HardModeMissionIcon2},
	{L"HardModeMissionIcon.png", IDB_Missions_HardModeMissionIcon},
});

const Mission::MissionImageList NightfallMission::normal_mode_images({
	{L"NightfallMissionIconIncomplete.png", IDB_Missions_NightfallMissionIconIncomplete},
	{L"NightfallMissionIconPrimary.png", IDB_Missions_NightfallMissionIconPrimary},
	{L"NightfallMissionIconExpert.png", IDB_Missions_NightfallMissionIconExpert},
	{L"NightfallMissionIcon.png", IDB_Missions_NightfallMissionIcon},
});
const Mission::MissionImageList NightfallMission::hard_mode_images({
	{L"HardModeMissionIconIncomplete.png", IDB_Missions_HardModeMissionIconIncomplete},
	{L"HardModeMissionIcon1.png", IDB_Missions_HardModeMissionIcon1},
	{L"HardModeMissionIcon2.png", IDB_Missions_HardModeMissionIcon2},
	{L"HardModeMissionIcon.png", IDB_Missions_HardModeMissionIcon},
});

const Mission::MissionImageList TormentMission::normal_mode_images({
	{L"NightfallTormentMissionIconIncomplete.png", IDB_Missions_NightfallTormentMissionIconIncomplete},
	{L"NightfallTormentMissionIconPrimary.png", IDB_Missions_NightfallTormentMissionIconPrimary},
	{L"NightfallTormentMissionIconExpert.png", IDB_Missions_NightfallTormentMissionIconExpert},
	{L"NightfallTormentMissionIcon.png", IDB_Missions_NightfallTormentMissionIcon},
});
const Mission::MissionImageList TormentMission::hard_mode_images({
	{L"HardModeMissionIconIncomplete.png", IDB_Missions_HardModeMissionIconIncomplete},
	{L"HardModeMissionIcon1.png", IDB_Missions_HardModeMissionIcon1},
	{L"HardModeMissionIcon2.png", IDB_Missions_HardModeMissionIcon2},
	{L"HardModeMissionIcon.png", IDB_Missions_HardModeMissionIcon},
});

const Mission::MissionImageList EotNMission::normal_mode_images({
	{L"EOTNMissionIncomplete.png", IDB_Missions_EOTNMissionIncomplete},
	{L"EOTNMission.png", IDB_Missions_EOTNMission},
});
const Mission::MissionImageList EotNMission::hard_mode_images({
	{L"EOTNHardModeMissionIncomplete.png", IDB_Missions_EOTNHardModeMissionIncomplete},
	{L"EOTNHardModeMission.png", IDB_Missions_EOTNHardModeMission},
});

const Mission::MissionImageList Dungeon::normal_mode_images({
	{L"EOTNDungeonIncomplete.png", IDB_Missions_EOTNDungeonIncomplete},
	{L"EOTNDungeon.png", IDB_Missions_EOTNDungeon},
});
const Mission::MissionImageList Dungeon::hard_mode_images({
	{L"EOTNHardModeDungeonIncomplete.png", IDB_Missions_EOTNHardModeDungeonIncomplete},
	{L"EOTNDungeon.png", IDB_Missions_EOTNDungeon},
});


const char* const Missions::CampaignName(const Campaign camp) {
	switch (camp) {
	case Campaign::Prophecies:
		return "Prophecies";
	case Campaign::Factions:
		return "Factions";
	case Campaign::Nightfall:
		return "Nightfall";
	case Campaign::EyeOfTheNorth:
		return "Eye of the North";
	case Campaign::Dungeon:
		return "Dungeons";
	}
	return "INVALID CAMPIGN";
}

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
				 const Mission::MissionImageList& hard_mode_images)
	: outpost(_outpost)
{
	normal_mode_textures.assign(normal_mode_images.size(), nullptr);
	hard_mode_textures.assign(hard_mode_images.size(), nullptr);
	for (size_t i = 0; i < normal_mode_images.size(); i++){
		const auto& normal_image_pair = normal_mode_images.at(i);
		Resources::Instance().LoadTextureAsync(
			&normal_mode_textures.at(i),
			Resources::GetPath(L"img/missions", normal_image_pair.first),
			normal_image_pair.second
		);

		const auto& hard_image_pair = hard_mode_images.at(i);
		Resources::Instance().LoadTextureAsync(
			&hard_mode_textures.at(i),
			Resources::GetPath(L"img/missions", hard_image_pair.first),
			hard_image_pair.second
		);
	}
}


void Mission::Draw(IDirect3DDevice9* device, float icon_size)
{
	auto texture = GetMissionImage();
	if (texture == nullptr) return;

	ImVec2 s(icon_size, icon_size);
	ImVec4 bg = ImVec4(0, 0, 0, 0);
	ImVec4 tint(1, 1, 1, 1);
	ImVec2 uv0 = ImVec2(0, 0);
	ImVec2 uv1 = ImVec2(1, 1);
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
	if (ImGui::ImageButton((ImTextureID)texture, s, uv0, uv1, -1, bg, tint)) {
		TravelWindow::Travel(outpost, GW::Constants::District::Current, 0);
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


IDirect3DTexture9* EotNMission::GetMissionImage()
{
	bool hardmode = GW::PartyMgr::GetIsPartyInHardMode();
	auto& texture_list = normal_mode_textures;
	if (hardmode) {
		texture_list = hard_mode_textures;
	}
	return texture_list.at(1);
}


const std::string EotNMission::Name() {
	std::string label(name);
	label += "\n(";
	label += Mission::Name();
	label += ")";
	return label;
}