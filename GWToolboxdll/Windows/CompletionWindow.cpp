#include "stdafx.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/Packets/Opcodes.h>

#include <GWCA/Context/CharContext.h>
#include <GWCA/Context/WorldContext.h>

#include <GWCA/GameEntities/Skill.h>
#include <GWCA/GameEntities/Quest.h>
#include <GWCA/GameEntities/Map.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Player.h>
#include <GWCA/GameEntities/Hero.h>

#include <GWCA/GameContainers/GamePos.h>

#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/PlayerMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>

#include <Modules/Resources.h>

#include <Windows/RerollWindow.h>
#include <Windows/CompletionWindow.h>
#include <Windows/TravelWindow.h>

using namespace GW::Constants;
using namespace Missions;

namespace {

	const char* campaign_names[] = {
		"Core",
		"Prophecies",
		"Factions",
		"Nightfall",
		"Eye Of The North",
		"Dungeons"
	};

	const char* CampaignName(const Campaign camp) {
		return campaign_names[static_cast<uint8_t>(camp)];
	}
	const char* hero_names[] = {
		"",
		"Norgu",
		"Goren",
		"Tahlkora",
		"Master of Whispers",
		"Acolyte Jin",
		"Koss",
		"Dunkoro",
		"Acolyte Sousuke",
		"Melonni",
		"Zhed Shadowhoof",
		"General Morgahn",
		"Margrid the Sly",
		"Zenmai",
		"Olias",
		"Razah",
		"M.O.X.",
		"Keiran Thackeray",
		"Jora",
		"Pyre Fierceshot",
		"Anton",
		"Livia",
		"Hayda",
		"Kahmu",
		"Gwen",
		"Xandra",
		"Vekk",
		"Ogden Stonehealer",
		"","","","","","","","",
		"Miku",
		"Zei Ri"
	};

	const wchar_t* GetPlayerName() {
		return GW::GameContext::instance()->character->player_name;
	}

	wchar_t last_player_name[20];

	bool show_as_list = true;

	std::wstring chosen_player_name;
	std::string chosen_player_name_s;

	void LoadTextures(std::vector<MissionImage>& mission_images) {
		Resources::EnsureFolderExists(Resources::GetPath(L"img", L"missions"));
		for (auto& mission_image : mission_images) {
			if (mission_image.texture)
				continue;
			Resources::Instance().LoadTexture(
				&mission_image.texture,
				Resources::GetPath(L"img/missions", mission_image.file_name),
				(WORD)mission_image.resource_id
			);
		}
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
ImVec2 Mission::icon_size = { 48.0f, 48.0f };

static bool ArrayBoolAt(GW::Array<uint32_t>& array, uint32_t index)
{
	uint32_t real_index = index / 32;
	if (real_index >= array.size())
		return false;
	uint32_t shift = index % 32;
	uint32_t flag = 1 << shift;
	return (array[real_index] & flag) != 0;
}
static bool ArrayBoolAt(std::vector<uint32_t>& array, uint32_t index)
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
	GW::Constants::QuestID _zm_quest)
	: outpost(_outpost), zm_quest(_zm_quest), normal_mode_textures(_normal_mode_images), hard_mode_textures(_hard_mode_images) {
	map_to = outpost;
	GW::AreaInfo* map_info = GW::Map::GetMapInfo(outpost);
	if (map_info)
		name.reset(map_info->name_id);
	};

GW::Constants::MapID Mission::GetOutpost() {
	return TravelWindow::GetNearestOutpost(map_to);
}
bool Mission::Draw(IDirect3DDevice9* )
{
	auto texture = GetMissionImage();

	const float scale = ImGui::GetIO().FontGlobalScale;

	ImVec2 s(icon_size.x * scale, icon_size.y * scale);
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

	const ImVec2 cursor_pos = ImGui::GetCursorPos();
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
	ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.f, 0.5f));
	ImGui::PushID((int)outpost);
	if (show_as_list) {
		s.y /= 2.f;
		if (!map_unlocked) {
			ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
		}
		bool clicked = ImGui::IconButton(Name(), (ImTextureID)texture, { s.x * 5.f, s.y }, 0, { s.x / 2.f, s.y });
		if (!map_unlocked) {
			ImGui::PopStyleColor();
		}
		if(clicked) OnClick();
	}
	else {
		if (texture) {
			uv1 = ImGui::CalculateUvCrop(texture, s);
		}
		if (ImGui::ImageButton((ImTextureID)texture, s, uv0, uv1, -1, bg, tint))
			OnClick();
		if (ImGui::IsItemHovered()) ImGui::SetTooltip(Name());
	}
	ImGui::PopID();
	ImGui::PopStyleColor();
	ImGui::PopStyleVar();

	if (is_completed && bonus && show_as_list) {
		const ImVec2 cursor_pos2 = ImGui::GetCursorPos();
		ImVec2 icon_size_scaled = { icon_size.x * ImGui::GetIO().FontGlobalScale,icon_size.y * ImGui::GetIO().FontGlobalScale };
		if (show_as_list) {
			icon_size_scaled.x /= 2.f;
			icon_size_scaled.y /= 2.f;
		}

		const ImColor completed_bg = IM_COL32(0, 0x99, 0, 192);
		const ImColor completed_text = IM_COL32(0xE5, 0xFF, 0xCC, 255);
		ImGui::SetCursorPos(cursor_pos);
		const ImVec2 screen_pos = ImGui::GetCursorScreenPos();
		ImGui::RenderFrame(screen_pos, { screen_pos.x + icon_size_scaled.x, screen_pos.y + icon_size_scaled.y }, completed_bg, false, 0.f);

		ImGui::SetCursorPos(cursor_pos);

		const ImVec2 check_size = ImGui::CalcTextSize(reinterpret_cast<const char*>(ICON_FA_CHECK));
        ImGui::GetWindowDrawList()->AddText({ screen_pos.x + ((icon_size_scaled.x - check_size.x) / 2), screen_pos.y + ((icon_size_scaled.y - check_size.y) / 2) },
			completed_text, reinterpret_cast<const char*>(ICON_FA_CHECK));
		ImGui::SetCursorPos(cursor_pos2);
	}
	return true;
}
const char* Mission::Name() {
	return name.string().c_str();
}
void Mission::OnClick() {
	GW::Constants::MapID travel_to = GetOutpost();
	if (chosen_player_name != GetPlayerName()) {
		RerollWindow::Instance().Reroll(chosen_player_name.data(), travel_to);
		return;
	}
	if (travel_to == GW::Constants::MapID::None) {
		Log::Error("Failed to find nearest outpost");
	}
	else {
		TravelWindow::Instance().Travel(travel_to, GW::Constants::District::Current, 0);
	}
}
void Mission::CheckProgress(const std::wstring& player_name) {
	is_completed = bonus = false;
	auto& completion = CompletionWindow::Instance().character_completion;
	auto found = completion.find(player_name);
	if (found == completion.end())
		return;
	std::vector<uint32_t>* missions_complete = &found->second->mission;
	std::vector<uint32_t>* missions_bonus = &found->second->mission_bonus;
	if (CompletionWindow::Instance().IsHardMode()) {
		missions_complete = &found->second->mission_hm;
		missions_bonus = &found->second->mission_bonus_hm;
	}
	map_unlocked = found->second->maps_unlocked.empty() || ArrayBoolAt(found->second->maps_unlocked, static_cast<uint32_t>(outpost));
	is_completed = ArrayBoolAt(*missions_complete, static_cast<uint32_t>(outpost));
	bonus = ArrayBoolAt(*missions_bonus, static_cast<uint32_t>(outpost));
}
IDirect3DTexture9* Mission::GetMissionImage()
{
	auto* texture_list = &normal_mode_textures;

	if (CompletionWindow::Instance().IsHardMode()) {
		texture_list = &hard_mode_textures;
	}
	uint8_t index = is_completed + 2 * bonus;

	return texture_list->at(index).texture;
}
bool Mission::IsDaily()
{
	return false;
}
bool Mission::HasQuest()
{
	if (zm_quest == GW::Constants::QuestID::None)
		return false;
	auto* quests = GW::PlayerMgr::GetQuestLog();
	if (!quests) 
		return false;
	for (auto& quest : *quests) {
		if (quest.quest_id == zm_quest)
			return true;
	}
	return false;
}

bool Dungeon::IsDaily()
{
	return false;
}
bool Dungeon::HasQuest()
{
	if (zb_quests.empty())
		return false;
	auto* quests = GW::PlayerMgr::GetQuestLog();
	if (!quests)
		return false;
	for (auto& quest : *quests) {
		for (auto& zb : zb_quests) {
			if (quest.quest_id == zb) {
				return true;
			}
		}
	}
	return false;
}


HeroUnlock::HeroUnlock(GW::Constants::HeroID _hero_id)
	: PvESkill(GW::Constants::SkillID::No_Skill, nullptr) {
	skill_id = (GW::Constants::SkillID)_hero_id;
}
void HeroUnlock::CheckProgress(const std::wstring& player_name) {
	is_completed = false;
	auto& skills = CompletionWindow::Instance().character_completion;
	auto found = skills.find(player_name);
	if (found == skills.end())
		return;
	auto& heroes = found->second->heroes;
	is_completed = bonus = std::find(heroes.begin(), heroes.end(), (uint32_t)skill_id) != heroes.end();
}
const char* HeroUnlock::Name() {
	return hero_names[(uint32_t)skill_id];
}
IDirect3DTexture9* HeroUnlock::GetMissionImage()
{
	if (!img_loaded) {
		img_loaded = true;
		auto path = Resources::GetPath(L"img/heros");
		Resources::EnsureFolderExists(path);
		wchar_t local_image[MAX_PATH];
		swprintf(local_image, _countof(local_image), L"%s/hero_%d.jpg", path.c_str(), skill_id);
		char remote_image[128];
		snprintf(remote_image, _countof(remote_image), "https://github.com/HasKha/GWToolboxpp/raw/master/resources/heros/hero_%d.jpg", skill_id);
		Resources::Instance().LoadTexture(&skill_image, local_image, remote_image);
	}
	return skill_image;
}
void HeroUnlock::OnClick() {
	wchar_t* buf = new wchar_t[128];
	swprintf(buf, 128, L"Game_link:Hero_%d", skill_id);
	GW::GameThread::Enqueue([buf]() {
		GuiUtils::OpenWiki(buf);
		delete[] buf;
		});
}

IDirect3DTexture9* PvESkill::GetMissionImage()
{
	return *Resources::GetSkillImage(skill_id);
}
PvESkill::PvESkill(GW::Constants::SkillID _skill_id, const wchar_t* _image_url)
	: Mission(GW::Constants::MapID::None, dummy_var, dummy_var), skill_id(_skill_id), image_url(_image_url) {
	if (_skill_id != GW::Constants::SkillID::No_Skill) {
		GW::Skill* s = GW::SkillbarMgr::GetSkillConstantData(skill_id);
		if (s) {
			name.reset(s->name);
			profession = s->profession;
		}

	}
}
void PvESkill::OnClick() {
	wchar_t* buf = new wchar_t[128];
	swprintf(buf, 128, L"Game_link:Skill_%d", skill_id);
	GW::GameThread::Enqueue([buf]() {
		GuiUtils::OpenWiki(buf);
		delete[] buf;
		});
}
bool PvESkill::Draw(IDirect3DDevice9* device) {
	const ImVec2 cursor_pos = ImGui::GetCursorPos();
	if (!Mission::Draw(device))
		return false;
	if (is_completed && !show_as_list) {
		const ImVec2 cursor_pos2 = ImGui::GetCursorPos();
		ImVec2 icon_size_scaled = { icon_size.x * ImGui::GetIO().FontGlobalScale,icon_size.y * ImGui::GetIO().FontGlobalScale };
		if (show_as_list) {
			icon_size_scaled.x /= 2.f;
			icon_size_scaled.y /= 2.f;
		}

		const ImColor completed_bg = IM_COL32(0, 0x99, 0, 192);
		const ImColor completed_text = IM_COL32(0xE5, 0xFF, 0xCC, 255);
		ImGui::SetCursorPos(cursor_pos);
		const ImVec2 screen_pos = ImGui::GetCursorScreenPos();
		ImGui::RenderFrame(screen_pos, { screen_pos.x + icon_size_scaled.x, screen_pos.y + icon_size_scaled.y }, completed_bg, false, 0.f);

		ImGui::SetCursorPos(cursor_pos);

		const ImVec2 check_size = ImGui::CalcTextSize(reinterpret_cast<const char*>(ICON_FA_CHECK));
        ImGui::GetWindowDrawList()->AddText({screen_pos.x + ((icon_size_scaled.x - check_size.x) / 2.f),
                                                screen_pos.y + ((icon_size_scaled.y - check_size.y) / 2.f)},
            completed_text, reinterpret_cast<const char*>(ICON_FA_CHECK));
		ImGui::SetCursorPos(cursor_pos2);
	}
	return true;
}
void PvESkill::CheckProgress(const std::wstring& player_name) {
	is_completed = false;
	auto& skills = CompletionWindow::Instance().character_completion;
	auto found = skills.find(player_name);
	if (found == skills.end())
		return;
	auto& unlocked = found->second->skills;
	is_completed = bonus = ArrayBoolAt(unlocked, static_cast<uint32_t>(skill_id));
}

FactionsPvESkill::FactionsPvESkill(GW::Constants::SkillID skill_id)
	: PvESkill(skill_id) {
	GW::Skill* s = GW::SkillbarMgr::GetSkillConstantData(skill_id);
	uint32_t faction_id = 0x6C3D;
	if ((GW::Constants::TitleID)s->title == GW::Constants::TitleID::Luxon) {
		faction_id = 0x6C3E;
	}
	if (s) {
		std::wstring buf;
		buf.resize(32,0);
		GW::UI::UInt32ToEncStr(s->name, buf.data(), buf.size());
		buf.resize(wcslen(buf.data()));
		buf += L"\x2\x108\x107 - \x1\x2";
		buf.resize(wcslen(buf.data()) + 4, 0);
		GW::UI::UInt32ToEncStr(faction_id, buf.data() + buf.size() - 4, 4);
		buf.resize(wcslen(buf.data()) + 1,0);
		name.reset(buf.c_str());
	}
};
bool FactionsPvESkill::Draw(IDirect3DDevice9* device) {
	//icon_size.y *= 2.f;
	bool drawn = PvESkill::Draw(device);
	//icon_size.y /= 2.f;
	return drawn;
}


void EotNMission::CheckProgress(const std::wstring& player_name) {
	is_completed = false;
	auto& completion = CompletionWindow::Instance().character_completion;
	auto found = completion.find(player_name);
	if (found == completion.end())
		return;
	std::vector<uint32_t>* missions_bonus = &found->second->mission_bonus;
	if (CompletionWindow::Instance().IsHardMode()) {
		missions_bonus = &found->second->mission_bonus_hm;
	}
	is_completed = bonus = ArrayBoolAt(*missions_bonus, static_cast<uint32_t>(outpost));
}
IDirect3DTexture9* EotNMission::GetMissionImage()
{
	auto* texture_list = &normal_mode_textures;
	if (CompletionWindow::Instance().IsHardMode()) {
		texture_list = &hard_mode_textures;
	}
	return texture_list->at(is_completed ? 1 : 0).texture;
}

void Vanquish::CheckProgress(const std::wstring& player_name) {
	is_completed = false;
	auto& completion = CompletionWindow::Instance().character_completion;
	auto found = completion.find(player_name);
	if (found == completion.end())
		return;
	auto& unlocked = found->second->vanquishes;
	is_completed = bonus = ArrayBoolAt(unlocked, static_cast<uint32_t>(outpost));
}
IDirect3DTexture9* Vanquish::GetMissionImage()
{
	return hard_mode_textures.at(is_completed).texture;
}


void CompletionWindow::Initialize()
{
	ToolboxWindow::Initialize();

	//Resources::Instance().LoadTexture(&button_texture, Resources::GetPath(L"img/missions", L"MissionIcon.png"), IDB_Missions_MissionIcon);

	missions = {
		{ GW::Constants::Campaign::Prophecies, {} },
		{ GW::Constants::Campaign::Factions, {} },
		{ GW::Constants::Campaign::Nightfall, {} },
		{ GW::Constants::Campaign::EyeOfTheNorth, {} },
		{ GW::Constants::Campaign::BonusMissionPack, {} },
	};
	vanquishes = {
		{ GW::Constants::Campaign::Prophecies, {} },
		{ GW::Constants::Campaign::Factions, {} },
		{ GW::Constants::Campaign::Nightfall, {} },
		{ GW::Constants::Campaign::EyeOfTheNorth, {} },
		{ GW::Constants::Campaign::BonusMissionPack, {} },
	};
	elite_skills = {
		{ GW::Constants::Campaign::Prophecies, {} },
		{ GW::Constants::Campaign::Factions, {} },
		{ GW::Constants::Campaign::Nightfall, {} },
		{ GW::Constants::Campaign::Core, {} },
	};
	 pve_skills = {
		{ GW::Constants::Campaign::Factions, {} },
		{ GW::Constants::Campaign::Nightfall, {} },
		{ GW::Constants::Campaign::EyeOfTheNorth, {} },
		{ GW::Constants::Campaign::Core, {} },
	};
	 heros = {
		{ GW::Constants::Campaign::Factions, {} },
		{ GW::Constants::Campaign::Nightfall, {} },
		{ GW::Constants::Campaign::EyeOfTheNorth, {} }
	 };

	Initialize_Prophecies();
	Initialize_Factions();
	Initialize_Nightfall();
	Initialize_EotN();
	Initialize_Dungeons();

	auto& eskills = elite_skills.at(Campaign::Core);

	eskills.push_back(new PvESkill(GW::Constants::SkillID::Charge, L"3/32/%22Charge%21%22"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Battle_Rage, L"d/dd/Battle_Rage"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Cleave, L"f/f9/Cleave"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Devastating_Hammer, L"3/3a/Devastating_Hammer"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Hundred_Blades, L"6/63/Hundred_Blades"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Seven_Weapon_Stance, L"d/d7/Seven_Weapons_Stance"));

	eskills.push_back(new PvESkill(GW::Constants::SkillID::Together_as_one, L"f/ff/%22Together_as_One%21%22"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Barrage, L"9/93/Barrage"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Escape, L"a/a2/Escape"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Ferocious_Strike, L"5/5e/Ferocious_Strike"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Quick_Shot, L"9/9a/Quick_Shot"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Spike_Trap, L"1/1a/Spike_Trap"));

	eskills.push_back(new PvESkill(GW::Constants::SkillID::Blood_is_Power, L"2/28/Blood_is_Power"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Grenths_Balance, L"5/5e/Grenth%27s_Balance"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Lingering_Curse, L"a/a6/Lingering_Curse"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Plague_Signet, L"b/b1/Plague_Signet"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Soul_Taker, L"4/4e/Soul_Taker"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Tainted_Flesh, L"2/26/Tainted_Flesh"));

	eskills.push_back(new PvESkill(GW::Constants::SkillID::Judgement_Strike, L"6/63/Judgment_Strike"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Martyr, L"a/a9/Martyr"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Shield_of_Regeneration, L"e/eb/Shield_of_Regeneration"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Signet_of_Judgment, L"a/aa/Signet_of_Judgment"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Spell_Breaker, L"0/08/Spell_Breaker"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Word_of_Healing, L"1/17/Word_of_Healing"));

	eskills.push_back(new PvESkill(GW::Constants::SkillID::Crippling_Anguish, L"e/e4/Crippling_Anguish"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Echo, L"a/a0/Echo"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Energy_Drain, L"8/8c/Energy_Drain"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Energy_Surge, L"f/f6/Energy_Surge"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Mantra_of_Recovery, L"0/00/Mantra_of_Recovery"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Time_Ward, L"9/90/Time_Ward"));

	eskills.push_back(new PvESkill(GW::Constants::SkillID::Elemental_Attunement, L"3/34/Elemental_Attunement"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Lightning_Surge, L"a/a1/Lightning_Surge"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Mind_Burn, L"4/4f/Mind_Burn"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Mind_Freeze, L"b/be/Mind_Freeze"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Obsidian_Flesh, L"c/c2/Obsidian_Flesh"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Over_the_Limit, L"5/5a/Over_the_Limit"));

	eskills.push_back(new PvESkill(GW::Constants::SkillID::Shadow_Theft, L"9/91/Shadow_Theft"));

	eskills.push_back(new PvESkill(GW::Constants::SkillID::Weapons_of_Three_Forges, L"0/08/Weapons_of_Three_Forges"));

	eskills.push_back(new PvESkill(GW::Constants::SkillID::Vow_of_Revolution, L"4/48/Vow_of_Revolution"));

	eskills.push_back(new PvESkill(GW::Constants::SkillID::Heroic_Refrain, L"6/6e/Heroic_Refrain"));

	auto& skills = pve_skills.at(Campaign::Core);
	skills.push_back(new PvESkill(GW::Constants::SkillID::Seven_Weapon_Stance, L"d/d7/Seven_Weapons_Stance"));
	skills.push_back(new PvESkill(GW::Constants::SkillID::Together_as_one, L"f/ff/%22Together_as_One%21%22"));
	skills.push_back(new PvESkill(GW::Constants::SkillID::Soul_Taker, L"4/4e/Soul_Taker"));
	skills.push_back(new PvESkill(GW::Constants::SkillID::Judgement_Strike, L"6/63/Judgment_Strike"));
	skills.push_back(new PvESkill(GW::Constants::SkillID::Time_Ward, L"9/90/Time_Ward"));
	skills.push_back(new PvESkill(GW::Constants::SkillID::Over_the_Limit, L"5/5a/Over_the_Limit"));
	skills.push_back(new PvESkill(GW::Constants::SkillID::Shadow_Theft, L"9/91/Shadow_Theft"));
	skills.push_back(new PvESkill(GW::Constants::SkillID::Weapons_of_Three_Forges, L"0/08/Weapons_of_Three_Forges"));
	skills.push_back(new PvESkill(GW::Constants::SkillID::Vow_of_Revolution, L"4/48/Vow_of_Revolution"));
	skills.push_back(new PvESkill(GW::Constants::SkillID::Heroic_Refrain, L"6/6e/Heroic_Refrain"));

	GW::StoC::RegisterPostPacketCallback(&skills_unlocked_stoc_entry, GAME_SMSG_MAPS_UNLOCKED, [](GW::HookStatus*, void*) {
		Instance().ParseCompletionBuffer(CompletionType::Mission);
		Instance().ParseCompletionBuffer(CompletionType::MissionBonus);
		Instance().ParseCompletionBuffer(CompletionType::MissionBonusHM);
		Instance().ParseCompletionBuffer(CompletionType::MissionHM);
		Instance().ParseCompletionBuffer(CompletionType::MapsUnlocked);
		Instance().CheckProgress();
		});
	GW::StoC::RegisterPostPacketCallback(&skills_unlocked_stoc_entry, GAME_SMSG_VANQUISH_PROGRESS, [](GW::HookStatus*, void*) {
		Instance().ParseCompletionBuffer(CompletionType::Vanquishes);
		Instance().CheckProgress();
		});
	GW::StoC::RegisterPostPacketCallback(&skills_unlocked_stoc_entry, GAME_SMSG_VANQUISH_COMPLETE, [](GW::HookStatus*, void*) {
		Instance().ParseCompletionBuffer(CompletionType::Vanquishes);
		Instance().CheckProgress();
		});


	GW::StoC::RegisterPostPacketCallback(&skills_unlocked_stoc_entry, GAME_SMSG_SKILLS_UNLOCKED, [](GW::HookStatus*, void*) {
		Instance().ParseCompletionBuffer(CompletionType::Skills);
		Instance().CheckProgress();
		});
	GW::StoC::RegisterPostPacketCallback(&skills_unlocked_stoc_entry, GAME_SMSG_AGENT_CREATE_PLAYER, [](GW::HookStatus*, void* pak) {
		uint32_t player_number = ((uint32_t*)pak)[1];
		GW::CharContext* c = GW::GameContext::instance()->character;
		if (player_number == c->player_number) {
			GW::Player* me = GW::PlayerMgr::GetPlayerByID(c->player_number);
			if (me) {
				auto comp = Instance().GetCharacterCompletion(c->player_name);
				if (comp)
					comp->profession = (GW::Constants::Profession)me->primary;
				Instance().ParseCompletionBuffer(CompletionType::Heroes);
			}
			Instance().CheckProgress();
		}
		});
	GW::StoC::RegisterPostPacketCallback(&skills_unlocked_stoc_entry, GAME_SMSG_SKILL_UPDATE_SKILL_COUNT_1, [](GW::HookStatus*, void*) {
		Instance().ParseCompletionBuffer(CompletionType::Skills);
		Instance().CheckProgress();
		});
	// Reset chosen player name to be current character on login
	GW::StoC::RegisterPostPacketCallback(&skills_unlocked_stoc_entry, GAME_SMSG_INSTANCE_LOADED, [&](GW::HookStatus*, void*) {
		if (wcscmp(GetPlayerName(), last_player_name) != 0) {
			wcscpy(last_player_name, GetPlayerName());
			chosen_player_name_s.clear();
			chosen_player_name.clear();
		}
		ParseCompletionBuffer(CompletionType::Skills);
		ParseCompletionBuffer(CompletionType::Mission);
		ParseCompletionBuffer(CompletionType::MissionBonus);
		ParseCompletionBuffer(CompletionType::MissionBonusHM);
		ParseCompletionBuffer(CompletionType::MissionHM);
		ParseCompletionBuffer(CompletionType::MapsUnlocked);
		CheckProgress();
		});
	ParseCompletionBuffer(CompletionType::Mission);
	ParseCompletionBuffer(CompletionType::MissionBonus);
	ParseCompletionBuffer(CompletionType::MissionBonusHM);
	ParseCompletionBuffer(CompletionType::MissionHM);
	ParseCompletionBuffer(CompletionType::Skills);
	ParseCompletionBuffer(CompletionType::Vanquishes);
	ParseCompletionBuffer(CompletionType::Heroes);
	ParseCompletionBuffer(CompletionType::MapsUnlocked);
	CheckProgress();
	wcscpy(last_player_name,GetPlayerName());
}




void CompletionWindow::Initialize_Prophecies()
{
	LoadTextures(PropheciesMission::normal_mode_images);
	LoadTextures(PropheciesMission::hard_mode_images);

	auto& prophecies_missions = missions.at(Campaign::Prophecies);
	prophecies_missions.push_back(new PropheciesMission(
		MapID::The_Great_Northern_Wall, QuestID::ZaishenMission_The_Great_Northern_Wall));
	prophecies_missions.push_back(new PropheciesMission(
		MapID::Fort_Ranik, QuestID::ZaishenMission_Fort_Ranik));
	prophecies_missions.push_back(new PropheciesMission(
		MapID::Ruins_of_Surmia, QuestID::ZaishenMission_Ruins_of_Surmia));
	prophecies_missions.push_back(new PropheciesMission(
		MapID::Nolani_Academy, QuestID::ZaishenMission_Nolani_Academy));
	prophecies_missions.push_back(new PropheciesMission(
		MapID::Borlis_Pass, QuestID::ZaishenMission_Borlis_Pass));
	prophecies_missions.push_back(new PropheciesMission(
		MapID::The_Frost_Gate, QuestID::ZaishenMission_The_Frost_Gate));
	prophecies_missions.push_back(new PropheciesMission(
		MapID::Gates_of_Kryta, QuestID::ZaishenMission_Gates_of_Kryta));
	prophecies_missions.push_back(new PropheciesMission(
		MapID::DAlessio_Seaboard, QuestID::ZaishenMission_DAlessio_Seaboard));
	prophecies_missions.push_back(new PropheciesMission(
		MapID::Divinity_Coast, QuestID::ZaishenMission_Divinity_Coast));
	prophecies_missions.push_back(new PropheciesMission(
		MapID::The_Wilds, QuestID::ZaishenMission_The_Wilds));
	prophecies_missions.push_back(new PropheciesMission(
		MapID::Bloodstone_Fen, QuestID::ZaishenMission_Bloodstone_Fen));
	prophecies_missions.push_back(new PropheciesMission(
		MapID::Aurora_Glade, QuestID::ZaishenMission_Aurora_Glade));
	prophecies_missions.push_back(new PropheciesMission(
		MapID::Riverside_Province, QuestID::ZaishenMission_Riverside_Province));
	prophecies_missions.push_back(new PropheciesMission(
		MapID::Sanctum_Cay, QuestID::ZaishenMission_Sanctum_Cay));
	prophecies_missions.push_back(new PropheciesMission(
		MapID::Dunes_of_Despair, QuestID::ZaishenMission_Dunes_of_Despair));
	prophecies_missions.push_back(new PropheciesMission(
		MapID::Thirsty_River, QuestID::ZaishenMission_Thirsty_River));
	prophecies_missions.push_back(new PropheciesMission(
		MapID::Elona_Reach, QuestID::ZaishenMission_Elona_Reach));
	prophecies_missions.push_back(new PropheciesMission(
		MapID::Augury_Rock_mission, QuestID::ZaishenMission_Augury_Rock));
	prophecies_missions.push_back(new PropheciesMission(
		MapID::The_Dragons_Lair, QuestID::ZaishenMission_The_Dragons_Lair));
	prophecies_missions.push_back(new PropheciesMission(
		MapID::Ice_Caves_of_Sorrow, QuestID::ZaishenMission_Ice_Caves_of_Sorrow));
	prophecies_missions.push_back(new PropheciesMission(
		MapID::Iron_Mines_of_Moladune, QuestID::ZaishenMission_Iron_Mines_of_Moladune));
	prophecies_missions.push_back(new PropheciesMission(
		MapID::Thunderhead_Keep, QuestID::ZaishenMission_Thunderhead_Keep));
	prophecies_missions.push_back(new PropheciesMission(
		MapID::Ring_of_Fire, QuestID::ZaishenMission_Ring_of_Fire));
	prophecies_missions.push_back(new PropheciesMission(
		MapID::Abaddons_Mouth, QuestID::ZaishenMission_Abaddons_Mouth));
	prophecies_missions.push_back(new PropheciesMission(
		MapID::Hells_Precipice, QuestID::ZaishenMission_Hells_Precipice));

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

	auto& eskills = elite_skills.at(Campaign::Prophecies);
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Victory_Is_Mine, L"0/0b/%22Victory_Is_Mine%21%22"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Backbreaker, L"c/c1/Backbreaker"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Bulls_Charge, L"f/fb/Bull%27s_Charge"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Defy_Pain, L"1/16/Defy_Pain"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Dwarven_Battle_Stance, L"b/b9/Dwarven_Battle_Stance"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Earth_Shaker, L"a/ac/Earth_Shaker"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Eviscerate, L"3/30/Eviscerate"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Flourish, L"9/97/Flourish"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Gladiators_Defense, L"8/8e/Gladiator%27s_Defense"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Skull_Crack, L"c/c2/Skull_Crack"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Warriors_Endurance, L"c/c2/Warrior%27s_Endurance"));

	eskills.push_back(new PvESkill(GW::Constants::SkillID::Crippling_Shot, L"d/da/Crippling_Shot"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Greater_Conflagration, L"5/50/Greater_Conflagration"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Incendiary_Arrows, L"9/90/Incendiary_Arrows"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Marksmans_Wager, L"5/50/Marksman%27s_Wager"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Melandrus_Arrows, L"4/45/Melandru%27s_Arrows"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Melandrus_Resilience, L"b/b0/Melandru%27s_Resilience"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Oath_Shot, L"b/b6/Oath_Shot"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Poison_Arrow, L"f/fd/Poison_Arrow"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Practiced_Stance, L"a/ae/Practiced_Stance"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Punishing_Shot, L"9/96/Punishing_Shot"));

	eskills.push_back(new PvESkill(GW::Constants::SkillID::Aura_of_the_Lich, L"5/5e/Aura_of_the_Lich"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Feast_of_Corruption, L"4/47/Feast_of_Corruption"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Life_Transfer, L"4/49/Life_Transfer"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Offering_of_Blood, L"e/e6/Offering_of_Blood"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Order_of_the_Vampire, L"3/39/Order_of_the_Vampire"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Soul_Leech, L"0/0b/Soul_Leech"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Spiteful_Spirit, L"0/00/Spiteful_Spirit"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Virulence, L"4/4a/Virulence"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Well_of_Power, L"7/74/Well_of_Power"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Wither, L"a/a0/Wither"));

	eskills.push_back(new PvESkill(GW::Constants::SkillID::Amity, L"e/ee/Amity"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Aura_of_Faith, L"f/f7/Aura_of_Faith"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Healing_Hands, L"0/0b/Healing_Hands"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Life_Barrier, L"c/ca/Life_Barrier"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Mark_of_Protection, L"1/1b/Mark_of_Protection"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Peace_and_Harmony, L"f/f7/Peace_and_Harmony"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Restore_Condition, L"a/ac/Restore_Condition"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Shield_of_Deflection, L"1/1a/Shield_of_Deflection"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Shield_of_Judgment, L"e/e8/Shield_of_Judgment"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Unyielding_Aura, L"e/e6/Unyielding_Aura"));

	eskills.push_back(new PvESkill(GW::Constants::SkillID::Fevered_Dreams, L"7/72/Fevered_Dreams"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Illusionary_Weaponry, L"a/ab/Illusionary_Weaponry"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Ineptitude, L"6/6d/Ineptitude"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Keystone_Signet, L"3/31/Keystone_Signet"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Mantra_of_Recall, L"a/a0/Mantra_of_Recall"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Migraine, L"a/ad/Migraine"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Panic, L"f/f7/Panic"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Power_Block, L"5/5e/Power_Block"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Signet_of_Midnight, L"e/eb/Signet_of_Midnight"));

	eskills.push_back(new PvESkill(GW::Constants::SkillID::Ether_Prodigy, L"3/30/Ether_Prodigy"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Ether_Renewal, L"9/97/Ether_Renewal"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Glimmering_Mark, L"8/82/Glimmering_Mark"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Glyph_of_Energy, L"c/c1/Glyph_of_Energy"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Glyph_of_Renewal, L"8/8a/Glyph_of_Renewal"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Mind_Shock, L"2/29/Mind_Shock"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Mist_Form, L"f/f3/Mist_Form"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Thunderclap, L"b/bc/Thunderclap"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Ward_Against_Harm, L"3/3f/Ward_Against_Harm"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Water_Trident, L"e/ee/Water_Trident"));
}
void CompletionWindow::Initialize_Factions()
{
	LoadTextures(FactionsMission::normal_mode_images);
	LoadTextures(FactionsMission::hard_mode_images);

	auto& factions_missions = missions.at(Campaign::Factions);
	factions_missions.push_back(new FactionsMission(
		MapID::Minister_Chos_Estate_outpost_mission, QuestID::ZaishenMission_Minister_Chos_Estate));
	factions_missions.push_back(new FactionsMission(
		MapID::Zen_Daijun_outpost_mission, QuestID::ZaishenMission_Zen_Daijun));
	factions_missions.push_back(new FactionsMission(
		MapID::Vizunah_Square_Local_Quarter_outpost, QuestID::ZaishenMission_Vizunah_Square));
	factions_missions.push_back(new FactionsMission(
		MapID::Vizunah_Square_Foreign_Quarter_outpost, QuestID::ZaishenMission_Vizunah_Square));
	factions_missions.push_back(new FactionsMission(
		MapID::Nahpui_Quarter_outpost_mission, QuestID::ZaishenMission_Nahpui_Quarter));
	factions_missions.push_back(new FactionsMission(
		MapID::Tahnnakai_Temple_outpost_mission, QuestID::ZaishenMission_Tahnnakai_Temple));
	factions_missions.push_back(new FactionsMission(
		MapID::Arborstone_outpost_mission, QuestID::ZaishenMission_Arborstone));
	factions_missions.push_back(new FactionsMission(
		MapID::Boreas_Seabed_outpost_mission, QuestID::ZaishenMission_Boreas_Seabed));
	factions_missions.push_back(new FactionsMission(
		MapID::Sunjiang_District_outpost_mission, QuestID::ZaishenMission_Sunjiang_District));
	factions_missions.push_back(new FactionsMission(
		MapID::The_Eternal_Grove_outpost_mission, QuestID::ZaishenMission_The_Eternal_Grove));
	factions_missions.push_back(new FactionsMission(
		MapID::Gyala_Hatchery_outpost_mission, QuestID::ZaishenMission_Gyala_Hatchery));
	factions_missions.push_back(new FactionsMission(
		MapID::Unwaking_Waters_Kurzick_outpost, QuestID::ZaishenMission_Unwaking_Waters));
	factions_missions.push_back(new FactionsMission(
		MapID::Unwaking_Waters_Luxon_outpost, QuestID::ZaishenMission_Unwaking_Waters));
	factions_missions.push_back(new FactionsMission(
		MapID::Raisu_Palace_outpost_mission, QuestID::ZaishenMission_Raisu_Palace));
	factions_missions.push_back(new FactionsMission(
		MapID::Imperial_Sanctum_outpost_mission, QuestID::ZaishenMission_Imperial_Sanctum));

	LoadTextures(Vanquish::hard_mode_images);

	auto& this_vanquishes = vanquishes.at(Campaign::Factions);
	this_vanquishes.push_back(new Vanquish(MapID::Haiju_Lagoon,QuestID::ZaishenVanquish_Haiju_Lagoon));
	this_vanquishes.push_back(new Vanquish(MapID::Jaya_Bluffs, QuestID::ZaishenVanquish_Jaya_Bluffs));
	this_vanquishes.push_back(new Vanquish(MapID::Kinya_Province, QuestID::ZaishenVanquish_Kinya_Province));
	this_vanquishes.push_back(new Vanquish(MapID::Minister_Chos_Estate_explorable, QuestID::ZaishenVanquish_Minister_Chos_Estate));
	this_vanquishes.push_back(new Vanquish(MapID::Panjiang_Peninsula, QuestID::ZaishenVanquish_Panjiang_Peninsula));
	this_vanquishes.push_back(new Vanquish(MapID::Saoshang_Trail, QuestID::ZaishenVanquish_Saoshang_Trail));
	this_vanquishes.push_back(new Vanquish(MapID::Sunqua_Vale, QuestID::ZaishenVanquish_Sunqua_Vale));
	this_vanquishes.push_back(new Vanquish(MapID::Zen_Daijun_explorable, QuestID::ZaishenVanquish_Zen_Daijun));
	this_vanquishes.push_back(new Vanquish(MapID::Bukdek_Byway, QuestID::ZaishenVanquish_Bukdek_Byway));
	this_vanquishes.push_back(new Vanquish(MapID::Nahpui_Quarter_explorable, QuestID::ZaishenVanquish_Nahpui_Quarter));
	this_vanquishes.push_back(new Vanquish(MapID::Pongmei_Valley, QuestID::ZaishenVanquish_Pongmei_Valley));
	this_vanquishes.push_back(new Vanquish(MapID::Raisu_Palace, QuestID::ZaishenVanquish_Raisu_Palace));
	this_vanquishes.push_back(new Vanquish(MapID::Shadows_Passage, QuestID::ZaishenVanquish_Shadows_Passage));
	this_vanquishes.push_back(new Vanquish(MapID::Shenzun_Tunnels, QuestID::ZaishenVanquish_Shenzun_Tunnels));
	this_vanquishes.push_back(new Vanquish(MapID::Sunjiang_District_explorable, QuestID::ZaishenVanquish_Sunjiang_District));
	this_vanquishes.push_back(new Vanquish(MapID::Tahnnakai_Temple_explorable, QuestID::ZaishenVanquish_Tahnnakai_Temple));
	this_vanquishes.push_back(new Vanquish(MapID::Wajjun_Bazaar, QuestID::ZaishenVanquish_Wajjun_Bazaar));
	this_vanquishes.push_back(new Vanquish(MapID::Xaquang_Skyway, QuestID::ZaishenVanquish_Xaquang_Skyway));
	this_vanquishes.push_back(new Vanquish(MapID::Arborstone_explorable));
	this_vanquishes.push_back(new Vanquish(MapID::Drazach_Thicket, QuestID::ZaishenVanquish_Drazach_Thicket));
	this_vanquishes.push_back(new Vanquish(MapID::Ferndale, QuestID::ZaishenVanquish_Ferndale));
	this_vanquishes.push_back(new Vanquish(MapID::Melandrus_Hope));
	this_vanquishes.push_back(new Vanquish(MapID::Morostav_Trail, QuestID::ZaishenVanquish_Morostav_Trail));
	this_vanquishes.push_back(new Vanquish(MapID::Mourning_Veil_Falls, QuestID::ZaishenVanquish_Mourning_Veil_Falls));
	this_vanquishes.push_back(new Vanquish(MapID::The_Eternal_Grove, QuestID::ZaishenVanquish_The_Eternal_Grove));
	this_vanquishes.push_back(new Vanquish(MapID::Archipelagos));
	this_vanquishes.push_back(new Vanquish(MapID::Boreas_Seabed_explorable, QuestID::ZaishenVanquish_Boreas_Seabed));
	this_vanquishes.push_back(new Vanquish(MapID::Gyala_Hatchery, QuestID::ZaishenVanquish_Gyala_Hatchery));
	this_vanquishes.push_back(new Vanquish(MapID::Maishang_Hills, QuestID::ZaishenVanquish_Maishang_Hills));
	this_vanquishes.push_back(new Vanquish(MapID::Mount_Qinkai));
	this_vanquishes.push_back(new Vanquish(MapID::Rheas_Crater, QuestID::ZaishenVanquish_Rheas_Crater));
	this_vanquishes.push_back(new Vanquish(MapID::Silent_Surf, QuestID::ZaishenVanquish_Silent_Surf));
	this_vanquishes.push_back(new Vanquish(MapID::Unwaking_Waters, QuestID::ZaishenVanquish_Unwaking_Waters));

	auto& skills = pve_skills.at(Campaign::Factions);
	skills.push_back(new FactionsPvESkill(GW::Constants::SkillID::Save_Yourselves_kurzick));
	skills.push_back(new FactionsPvESkill(GW::Constants::SkillID::Save_Yourselves_luxon));
	
	skills.push_back(new FactionsPvESkill(GW::Constants::SkillID::Aura_of_Holy_Might_kurzick));
	skills.push_back(new FactionsPvESkill(GW::Constants::SkillID::Aura_of_Holy_Might_luxon));

	skills.push_back(new FactionsPvESkill(GW::Constants::SkillID::Elemental_Lord_kurzick));
	skills.push_back(new FactionsPvESkill(GW::Constants::SkillID::Elemental_Lord_luxon));

	skills.push_back(new FactionsPvESkill(GW::Constants::SkillID::Ether_Nightmare_kurzick));
	skills.push_back(new FactionsPvESkill(GW::Constants::SkillID::Ether_Nightmare_luxon));

	skills.push_back(new FactionsPvESkill(GW::Constants::SkillID::Selfless_Spirit_kurzick));
	skills.push_back(new FactionsPvESkill(GW::Constants::SkillID::Selfless_Spirit_luxon));

	skills.push_back(new FactionsPvESkill(GW::Constants::SkillID::Shadow_Sanctuary_kurzick));
	skills.push_back(new FactionsPvESkill(GW::Constants::SkillID::Shadow_Sanctuary_luxon));

	skills.push_back(new FactionsPvESkill(GW::Constants::SkillID::Signet_of_Corruption_kurzick));
	skills.push_back(new FactionsPvESkill(GW::Constants::SkillID::Signet_of_Corruption_luxon));

	skills.push_back(new FactionsPvESkill(GW::Constants::SkillID::Spear_of_Fury_kurzick));
	skills.push_back(new FactionsPvESkill(GW::Constants::SkillID::Spear_of_Fury_luxon));

	skills.push_back(new FactionsPvESkill(GW::Constants::SkillID::Summon_Spirits_kurzick));
	skills.push_back(new FactionsPvESkill(GW::Constants::SkillID::Summon_Spirits_luxon));

	skills.push_back(new FactionsPvESkill(GW::Constants::SkillID::Triple_Shot_kurzick));
	skills.push_back(new FactionsPvESkill(GW::Constants::SkillID::Triple_Shot_luxon));

	auto& eskills = elite_skills.at(Campaign::Factions);
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Coward, L"9/9c/%22Coward%21%22"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Auspicious_Parry, L"0/09/Auspicious_Parry"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Dragon_Slash, L"6/6a/Dragon_Slash"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Enraged_Smash, L"a/a6/Enraged_Smash"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Forceful_Blow, L"b/b0/Forceful_Blow"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Primal_Rage, L"d/d8/Primal_Rage"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Quivering_Blade, L"8/8f/Quivering_Blade"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Shove, L"b/bb/Shove"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Triple_Chop, L"a/a5/Triple_Chop"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Whirling_Axe, L"1/1b/Whirling_Axe"));

	eskills.push_back(new PvESkill(GW::Constants::SkillID::Archers_Signet, L"b/b2/Archer%27s_Signet"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Broad_Head_Arrow, L"5/51/Broad_Head_Arrow"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Enraged_Lunge, L"4/43/Enraged_Lunge"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Equinox, L"d/db/Equinox"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Famine, L"d/d4/Famine"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Glass_Arrows, L"3/3a/Glass_Arrows"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Heal_as_One, L"9/97/Heal_as_One"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Lacerate, L"f/fa/Lacerate"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Melandrus_Shot, L"a/ac/Melandru%27s_Shot"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Trappers_Focus, L"7/7c/Trapper%27s_Focus"));

	eskills.push_back(new PvESkill(GW::Constants::SkillID::Animate_Flesh_Golem, L"7/71/Animate_Flesh_Golem"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Cultists_Fervor, L"b/bc/Cultist%27s_Fervor"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Discord, L"6/64/Discord"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Icy_Veins, L"8/89/Icy_Veins"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Order_of_Apostasy, L"9/91/Order_of_Apostasy"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Soul_Bind, L"6/6c/Soul_Bind"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Spoil_Victor, L"7/76/Spoil_Victor"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Vampiric_Spirit, L"5/51/Vampiric_Spirit"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Wail_of_Doom, L"d/d8/Wail_of_Doom"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Weaken_Knees, L"6/6b/Weaken_Knees"));

	eskills.push_back(new PvESkill(GW::Constants::SkillID::Air_of_Enchantment, L"b/bb/Air_of_Enchantment"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Blessed_Light, L"4/40/Blessed_Light"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Boon_Signet, L"a/a7/Boon_Signet"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Empathic_Removal, L"2/20/Empathic_Removal"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Healing_Burst, L"6/6d/Healing_Burst"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Healing_Light, L"4/45/Healing_Light"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Life_Sheath, L"d/de/Life_Sheath"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Ray_of_Judgment, L"e/e4/Ray_of_Judgment"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Withdraw_Hexes, L"8/85/Withdraw_Hexes"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Word_of_Censure, L"6/6b/Word_of_Censure"));

	eskills.push_back(new PvESkill(GW::Constants::SkillID::Arcane_Languor, L"d/d5/Arcane_Languor"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Expel_Hexes, L"4/44/Expel_Hexes"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Lyssas_Aura, L"c/c1/Lyssa%27s_Aura"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Power_Leech, L"b/b8/Power_Leech"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Psychic_Distraction, L"7/70/Psychic_Distraction"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Psychic_Instability, L"d/db/Psychic_Instability"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Recurring_Insecurity, L"4/42/Recurring_Insecurity"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Shared_Burden, L"7/72/Shared_Burden"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Shatter_Storm, L"5/5e/Shatter_Storm"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Stolen_Speed, L"f/f0/Stolen_Speed"));

	eskills.push_back(new PvESkill(GW::Constants::SkillID::Double_Dragon, L"7/7a/Double_Dragon"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Energy_Boon, L"0/00/Energy_Boon"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Gust, L"a/a1/Gust"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Mirror_of_Ice, L"3/33/Mirror_of_Ice"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Ride_the_Lightning, L"f/fc/Ride_the_Lightning"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Second_Wind, L"f/f9/Second_Wind"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Shatterstone, L"3/36/Shatterstone"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Shockwave, L"1/18/Shockwave"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Star_Burst, L"5/5a/Star_Burst"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Unsteady_Ground, L"b/bf/Unsteady_Ground"));

	eskills.push_back(new PvESkill(GW::Constants::SkillID::Assassins_Promise, L"8/80/Assassin%27s_Promise"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Aura_of_Displacement, L"b/b9/Aura_of_Displacement"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Beguiling_Haze, L"9/9b/Beguiling_Haze"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Dark_Apostasy, L"a/ad/Dark_Apostasy"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Flashing_Blades, L"3/38/Flashing_Blades"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Locusts_Fury, L"5/5d/Locust%27s_Fury"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Moebius_Strike, L"4/49/Moebius_Strike"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Palm_Strike, L"c/c5/Palm_Strike"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Seeping_Wound, L"9/96/Seeping_Wound"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Shadow_Form, L"e/e4/Shadow_Form"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Shadow_Shroud, L"c/cf/Shadow_Shroud"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Shroud_of_Silence, L"d/d0/Shroud_of_Silence"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Siphon_Strength, L"6/64/Siphon_Strength"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Temple_Strike, L"9/9b/Temple_Strike"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Way_of_the_Empty_Palm, L"9/98/Way_of_the_Empty_Palm"));

	eskills.push_back(new PvESkill(GW::Constants::SkillID::Attuned_Was_Songkai, L"2/24/Attuned_Was_Songkai"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Clamor_of_Souls, L"4/4d/Clamor_of_Souls"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Consume_Soul, L"7/70/Consume_Soul"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Defiant_Was_Xinrae, L"3/3c/Defiant_Was_Xinrae"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Grasping_Was_Kuurong, L"f/f2/Grasping_Was_Kuurong"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Preservation, L"b/b9/Preservation"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Ritual_Lord, L"1/15/Ritual_Lord"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Signet_of_Spirits, L"f/f7/Signet_of_Spirits"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Soul_Twisting, L"2/24/Soul_Twisting"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Spirit_Channeling, L"0/0b/Spirit_Channeling"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Spirit_Light_Weapon, L"a/a3/Spirit_Light_Weapon"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Tranquil_Was_Tanasen, L"3/34/Tranquil_Was_Tanasen"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Vengeful_Was_Khanhei, L"3/36/Vengeful_Was_Khanhei"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Wanderlust, L"c/cb/Wanderlust"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Weapon_of_Quickening, L"7/78/Weapon_of_Quickening"));

	auto& h = heros.at(Campaign::Factions);
	h.push_back(new HeroUnlock(GW::Constants::HeroID::Miku));
	h.push_back(new HeroUnlock(GW::Constants::HeroID::ZeiRi));
}
void CompletionWindow::Initialize_Nightfall()
{
	LoadTextures(NightfallMission::normal_mode_images);
	LoadTextures(NightfallMission::hard_mode_images);
	LoadTextures(TormentMission::normal_mode_images);
	LoadTextures(TormentMission::hard_mode_images);

	auto& nightfall_missions = missions.at(Campaign::Nightfall);
	nightfall_missions.push_back(new NightfallMission(
		MapID::Chahbek_Village, QuestID::ZaishenMission_Chahbek_Village));
	nightfall_missions.push_back(new NightfallMission(
		MapID::Jokanur_Diggings, QuestID::ZaishenMission_Jokanur_Diggings));
	nightfall_missions.push_back(new NightfallMission(
		MapID::Blacktide_Den, QuestID::ZaishenMission_Blacktide_Den));
	nightfall_missions.push_back(new NightfallMission(
		MapID::Consulate_Docks, QuestID::ZaishenMission_Consulate_Docks));
	nightfall_missions.push_back(new NightfallMission(
		MapID::Venta_Cemetery, QuestID::ZaishenMission_Venta_Cemetery));
	nightfall_missions.push_back(new NightfallMission(
		MapID::Kodonur_Crossroads, QuestID::ZaishenMission_Kodonur_Crossroads));
	nightfall_missions.push_back(new NightfallMission(
		MapID::Pogahn_Passage, QuestID::ZaishenMission_Pogahn_Passage));
	nightfall_missions.push_back(new NightfallMission(
		MapID::Rilohn_Refuge, QuestID::ZaishenMission_Rilohn_Refuge));
	nightfall_missions.push_back(new NightfallMission(
		MapID::Moddok_Crevice, QuestID::ZaishenMission_Moddok_Crevice));
	nightfall_missions.push_back(new NightfallMission(
		MapID::Tihark_Orchard, QuestID::ZaishenMission_Tihark_Orchard));
	nightfall_missions.push_back(new NightfallMission(
		MapID::Dasha_Vestibule, QuestID::ZaishenMission_Dasha_Vestibule));
	nightfall_missions.push_back(new NightfallMission(
		MapID::Dzagonur_Bastion, QuestID::ZaishenMission_Dzagonur_Bastion));
	nightfall_missions.push_back(new NightfallMission(
		MapID::Grand_Court_of_Sebelkeh, QuestID::ZaishenMission_Grand_Court_of_Sebelkeh));
	nightfall_missions.push_back(new NightfallMission(
		MapID::Jennurs_Horde, QuestID::ZaishenMission_Jennurs_Horde));
	nightfall_missions.push_back(new NightfallMission(
		MapID::Nundu_Bay, QuestID::ZaishenMission_Nundu_Bay));
	nightfall_missions.push_back(new NightfallMission(
		MapID::Gate_of_Desolation, QuestID::ZaishenMission_Gate_of_Desolation));
	nightfall_missions.push_back(new NightfallMission(
		MapID::Ruins_of_Morah, QuestID::ZaishenMission_Ruins_of_Morah));
	nightfall_missions.push_back(new TormentMission(
		MapID::Gate_of_Pain, QuestID::ZaishenMission_Gate_of_Pain));
	nightfall_missions.push_back(new TormentMission(
		MapID::Gate_of_Madness, QuestID::ZaishenMission_Gate_of_Madness));
	nightfall_missions.push_back(new TormentMission(
		MapID::Abaddons_Gate, QuestID::ZaishenMission_Abaddons_Gate));

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

	auto& skills = pve_skills.at(Campaign::Nightfall);
	skills.push_back(new PvESkill(GW::Constants::SkillID::Theres_Nothing_to_Fear));
	skills.push_back(new PvESkill(GW::Constants::SkillID::Lightbringer_Signet, L"4/43/Lightbringer_Signet"));
	skills.push_back(new PvESkill(GW::Constants::SkillID::Lightbringers_Gaze,  L"c/c6/Lightbringer%27s_Gaze"));
	skills.push_back(new PvESkill(GW::Constants::SkillID::Critical_Agility,  L"e/e8/Critical_Agility"));
	skills.push_back(new PvESkill(GW::Constants::SkillID::Cry_of_Pain,  L"9/93/Cry_of_Pain"));
	skills.push_back(new PvESkill(GW::Constants::SkillID::Eternal_Aura, L"a/ab/Eternal_Aura"));
	skills.push_back(new PvESkill(GW::Constants::SkillID::Intensity, L"d/dc/Intensity"));
	skills.push_back(new PvESkill(GW::Constants::SkillID::Necrosis, L"9/99/Necrosis"));
	skills.push_back(new PvESkill(GW::Constants::SkillID::Never_Rampage_Alone,  L"d/d1/Never_Rampage_Alone"));
	skills.push_back(new PvESkill(GW::Constants::SkillID::Seed_of_Life,  L"7/74/Seed_of_Life"));
	skills.push_back(new PvESkill(GW::Constants::SkillID::Sunspear_Rebirth_Signet, L"e/e0/Sunspear_Rebirth_Signet"));
	skills.push_back(new PvESkill(GW::Constants::SkillID::Vampirism, L"5/59/Vampirism"));
	skills.push_back(new PvESkill(GW::Constants::SkillID::Whirlwind_Attack, L"2/2c/Whirlwind_Attack"));

	auto& eskills = elite_skills.at(Campaign::Nightfall);
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Youre_All_Alone, L"f/f1/%22You%27re_All_Alone%21%22"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Charging_Strike, L"a/ad/Charging_Strike"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Crippling_Slash, L"9/93/Crippling_Slash"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Decapitate, L"9/98/Decapitate"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Headbutt, L"6/64/Headbutt"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Magehunter_Strike, L"6/69/Magehunter_Strike"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Magehunters_Smash, L"1/11/Magehunter%27s_Smash"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Rage_of_the_Ntouka, L"1/11/Rage_of_the_Ntouka"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Soldiers_Stance, L"1/1b/Soldier%27s_Stance"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Steady_Stance, L"4/47/Steady_Stance"));

	eskills.push_back(new PvESkill(GW::Constants::SkillID::Burning_Arrow, L"d/da/Burning_Arrow"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Experts_Dexterity, L"5/58/Expert%27s_Dexterity"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Infuriating_Heat, L"b/bb/Infuriating_Heat"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Magebane_Shot, L"7/70/Magebane_Shot"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Prepared_Shot, L"3/3d/Prepared_Shot"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Quicksand, L"9/9a/Quicksand"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Rampage_as_One, L"2/25/Rampage_as_One"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Scavengers_Focus, L"c/c2/Scavenger%27s_Focus"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Smoke_Trap, L"f/f5/Smoke_Trap"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Strike_as_One, L"4/43/Strike_as_One"));

	eskills.push_back(new PvESkill(GW::Constants::SkillID::Contagion, L"1/14/Contagion"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Corrupt_Enchantment, L"b/bd/Corrupt_Enchantment"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Depravity, L"a/ae/Depravity"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Jagged_Bones, L"8/85/Jagged_Bones"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Order_of_Undeath, L"6/6e/Order_of_Undeath"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Pain_of_Disenchantment, L"1/11/Pain_of_Disenchantment"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Ravenous_Gaze, L"4/40/Ravenous_Gaze"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Reapers_Mark, L"c/c8/Reaper%27s_Mark"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Signet_of_Suffering, L"e/e8/Signet_of_Suffering"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Toxic_Chill, L"d/d4/Toxic_Chill"));

	eskills.push_back(new PvESkill(GW::Constants::SkillID::Balthazars_Pendulum, L"e/ec/Balthazar%27s_Pendulum"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Defenders_Zeal, L"b/b8/Defender%27s_Zeal"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Divert_Hexes, L"e/e7/Divert_Hexes"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Glimmer_of_Light, L"e/e8/Glimmer_of_Light"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Healers_Boon, L"f/f6/Healer%27s_Boon"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Healers_Covenant, L"9/9c/Healer%27s_Covenant"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Light_of_Deliverance, L"1/1f/Light_of_Deliverance"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Scribes_Insight, L"c/c6/Scribe%27s_Insight"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Signet_of_Removal, L"a/af/Signet_of_Removal"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Zealous_Benediction, L"e/ea/Zealous_Benediction"));

	eskills.push_back(new PvESkill(GW::Constants::SkillID::Air_of_Disenchantment, L"8/89/Air_of_Disenchantment"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Enchanters_Conundrum, L"d/d1/Enchanter%27s_Conundrum"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Extend_Conditions, L"5/59/Extend_Conditions"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Hex_Eater_Vortex, L"7/7e/Hex_Eater_Vortex"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Power_Flux, L"0/03/Power_Flux"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Signet_of_Illusions, L"1/1f/Signet_of_Illusions"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Simple_Thievery, L"1/11/Simple_Thievery"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Symbols_of_Inspiration, L"a/a4/Symbols_of_Inspiration"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Tease, L"5/5d/Tease"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Visions_of_Regret, L"9/93/Visions_of_Regret"));

	eskills.push_back(new PvESkill(GW::Constants::SkillID::Blinding_Surge, L"a/a4/Blinding_Surge"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Ether_Prism, L"3/39/Ether_Prism"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Icy_Shackles, L"d/d1/Icy_Shackles"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Invoke_Lightning, L"9/93/Invoke_Lightning"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Master_of_Magic, L"3/35/Master_of_Magic"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Mind_Blast, L"3/3d/Mind_Blast"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Sandstorm, L"7/75/Sandstorm"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Savannah_Heat, L"5/50/Savannah_Heat"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Searing_Flames, L"3/39/Searing_Flames"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Stone_Sheath, L"9/9e/Stone_Sheath"));

	eskills.push_back(new PvESkill(GW::Constants::SkillID::Assault_Enchantments, L"d/d7/Assault_Enchantments"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Foxs_Promise, L"e/ed/Fox%27s_Promise"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Golden_Skull_Strike, L"0/05/Golden_Skull_Strike"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Hidden_Caltrops, L"c/c2/Hidden_Caltrops"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Mark_of_Insecurity, L"2/24/Mark_of_Insecurity"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Shadow_Meld, L"1/10/Shadow_Meld"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Shadow_Prison, L"7/7f/Shadow_Prison"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Shattering_Assault, L"d/d4/Shattering_Assault"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Wastrels_Collapse, L"5/5e/Wastrel%27s_Collapse"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Way_of_the_Assassin, L"7/74/Way_of_the_Assassin"));

	eskills.push_back(new PvESkill(GW::Constants::SkillID::Caretakers_Charge, L"a/ad/Caretaker%27s_Charge"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Destructive_Was_Glaive, L"4/41/Destructive_Was_Glaive"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Offering_of_Spirit, L"1/14/Offering_of_Spirit"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Reclaim_Essence, L"b/bb/Reclaim_Essence"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Signet_of_Ghostly_Might, L"b/bf/Signet_of_Ghostly_Might"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Spirits_Strength, L"f/f9/Spirit%27s_Strength"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Weapon_of_Fury, L"2/2d/Weapon_of_Fury"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Weapon_of_Remedy, L"1/18/Weapon_of_Remedy"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Wielders_Zeal, L"4/47/Wielder%27s_Zeal"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Xinraes_Weapon, L"3/33/Xinrae%27s_Weapon"));

	eskills.push_back(new PvESkill(GW::Constants::SkillID::Arcane_Zeal, L"a/ad/Arcane_Zeal"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Avatar_of_Balthazar, L"9/93/Avatar_of_Balthazar"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Avatar_of_Dwayna, L"e/e0/Avatar_of_Dwayna"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Avatar_of_Grenth, L"1/13/Avatar_of_Grenth"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Avatar_of_Lyssa, L"e/e5/Avatar_of_Lyssa"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Avatar_of_Melandru , L"f/ff/Avatar_of_Melandru"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Ebon_Dust_Aura, L"3/39/Ebon_Dust_Aura"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Grenths_Grasp, L"4/4a/Grenth%27s_Grasp"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Onslaught, L"d/d8/Onslaught"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Pious_Renewal, L"3/36/Pious_Renewal"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Reapers_Sweep, L"e/e6/Reaper%27s_Sweep"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Vow_of_Silence, L"f/f6/Vow_of_Silence"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Vow_of_Strength, L"b/b9/Vow_of_Strength"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Wounding_Strike, L"3/35/Wounding_Strike"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Zealous_Vow, L"0/03/Zealous_Vow"));

	eskills.push_back(new PvESkill(GW::Constants::SkillID::Incoming, L"d/d5/%22Incoming%21%22"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Its_Just_a_Flesh_Wound, L"6/69/%22It%27s_Just_a_Flesh_Wound.%22"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::The_Power_Is_Yours, L"0/05/%22The_Power_Is_Yours%21%22"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Angelic_Bond, L"6/6e/Angelic_Bond"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Anthem_of_Fury, L"4/49/Anthem_of_Fury"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Anthem_of_Guidance, L"4/4e/Anthem_of_Guidance"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Cautery_Signet, L"1/17/Cautery_Signet"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Crippling_Anthem, L"2/23/Crippling_Anthem"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Cruel_Spear, L"9/9b/Cruel_Spear"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Defensive_Anthem, L"5/58/Defensive_Anthem"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Focused_Anger, L"c/c0/Focused_Anger"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Soldiers_Fury, L"c/ca/Soldier%27s_Fury"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Song_of_Purification, L"5/57/Song_of_Purification"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Song_of_Restoration, L"9/9b/Song_of_Restoration"));
	eskills.push_back(new PvESkill(GW::Constants::SkillID::Stunning_Strike, L"a/a6/Stunning_Strike"));

	auto& h = heros.at(Campaign::Nightfall);
	h.push_back(new HeroUnlock(GW::Constants::HeroID::AcolyteJin));
	h.push_back(new HeroUnlock(GW::Constants::HeroID::AcolyteSousuke));
	h.push_back(new HeroUnlock(GW::Constants::HeroID::Dunkoro));
	h.push_back(new HeroUnlock(GW::Constants::HeroID::GeneralMorgahn));
	h.push_back(new HeroUnlock(GW::Constants::HeroID::Goren));
	h.push_back(new HeroUnlock(GW::Constants::HeroID::Koss));
	h.push_back(new HeroUnlock(GW::Constants::HeroID::MargridTheSly));
	h.push_back(new HeroUnlock(GW::Constants::HeroID::MasterOfWhispers));
	h.push_back(new HeroUnlock(GW::Constants::HeroID::Melonni));
	h.push_back(new HeroUnlock(GW::Constants::HeroID::MOX));
	h.push_back(new HeroUnlock(GW::Constants::HeroID::Norgu));
	h.push_back(new HeroUnlock(GW::Constants::HeroID::Olias));
	h.push_back(new HeroUnlock(GW::Constants::HeroID::Razah));
	h.push_back(new HeroUnlock(GW::Constants::HeroID::Tahlkora));
	h.push_back(new HeroUnlock(GW::Constants::HeroID::Zenmai));
	h.push_back(new HeroUnlock(GW::Constants::HeroID::ZhedShadowhoof));
}
void CompletionWindow::Initialize_EotN()
{
	LoadTextures(EotNMission::normal_mode_images);
	LoadTextures(EotNMission::hard_mode_images);
	auto& eotn_missions = missions.at(Campaign::EyeOfTheNorth);
	// Asura
	eotn_missions.push_back(new EotNMission(MapID::Finding_the_Bloodstone_mission));
	eotn_missions.push_back(new EotNMission(MapID::The_Elusive_Golemancer_mission));
	eotn_missions.push_back(new EotNMission(MapID::Genius_Operated_Living_Enchanted_Manifestation_mission,QuestID::ZaishenMission_G_O_L_E_M));
	// Vanguard
	eotn_missions.push_back(new EotNMission(MapID::Against_the_Charr_mission));
	eotn_missions.push_back(new EotNMission(MapID::Warband_of_brothers_mission));
	eotn_missions.push_back(new EotNMission(MapID::Assault_on_the_Stronghold_mission, QuestID::ZaishenMission_Assault_on_the_Stronghold));
	// Norn
	eotn_missions.push_back(new EotNMission(MapID::Curse_of_the_Nornbear_mission, QuestID::ZaishenMission_Curse_of_the_Nornbear));
	eotn_missions.push_back(new EotNMission(MapID::A_Gate_Too_Far_mission, QuestID::ZaishenMission_A_Gate_Too_Far));
	eotn_missions.push_back(new EotNMission(MapID::Blood_Washes_Blood_mission));
	// Destroyers
	eotn_missions.push_back(new EotNMission(MapID::Destructions_Depths_mission,QuestID::ZaishenMission_Destructions_Depths));
	eotn_missions.push_back(new EotNMission(MapID::A_Time_for_Heroes_mission, QuestID::ZaishenMission_A_Time_for_Heroes));

	LoadTextures(Vanquish::hard_mode_images);

	auto& this_vanquishes = vanquishes.at(Campaign::EyeOfTheNorth);
	this_vanquishes.push_back(new Vanquish(MapID::Bjora_Marches, QuestID::ZaishenVanquish_Bjora_Marches));
	this_vanquishes.push_back(new Vanquish(MapID::Drakkar_Lake, QuestID::ZaishenVanquish_Drakkar_Lake));
	this_vanquishes.push_back(new Vanquish(MapID::Ice_Cliff_Chasms, QuestID::ZaishenVanquish_Ice_Cliff_Chasms));
	this_vanquishes.push_back(new Vanquish(MapID::Jaga_Moraine, QuestID::ZaishenVanquish_Jaga_Moraine));
	this_vanquishes.push_back(new Vanquish(MapID::Norrhart_Domains, QuestID::ZaishenVanquish_Norrhart_Domains));
	this_vanquishes.push_back(new Vanquish(MapID::Varajar_Fells, QuestID::ZaishenVanquish_Varajar_Fells));
	this_vanquishes.push_back(new Vanquish(MapID::Dalada_Uplands, QuestID::ZaishenVanquish_Dalada_Uplands));
	this_vanquishes.push_back(new Vanquish(MapID::Grothmar_Wardowns, QuestID::ZaishenVanquish_Grothmar_Wardowns));
	this_vanquishes.push_back(new Vanquish(MapID::Sacnoth_Valley, QuestID::ZaishenVanquish_Sacnoth_Valley));
	this_vanquishes.push_back(new Vanquish(MapID::Alcazia_Tangle));
	this_vanquishes.push_back(new Vanquish(MapID::Arbor_Bay, QuestID::ZaishenVanquish_Arbor_Bay));
	this_vanquishes.push_back(new Vanquish(MapID::Magus_Stones, QuestID::ZaishenVanquish_Magus_Stones));
	this_vanquishes.push_back(new Vanquish(MapID::Riven_Earth, QuestID::ZaishenVanquish_Riven_Earth));
	this_vanquishes.push_back(new Vanquish(MapID::Sparkfly_Swamp, QuestID::ZaishenVanquish_Sparkfly_Swamp));
	this_vanquishes.push_back(new Vanquish(MapID::Verdant_Cascades, QuestID::ZaishenVanquish_Verdant_Cascades));

	auto& skills = pve_skills.at(Campaign::EyeOfTheNorth);
	skills.push_back(new PvESkill(GW::Constants::SkillID::Air_of_Superiority, L"9/9f/Air_of_Superiority"));
	skills.push_back(new PvESkill(GW::Constants::SkillID::Asuran_Scan, L"a/a0/Asuran_Scan"));
	skills.push_back(new PvESkill(GW::Constants::SkillID::Mental_Block,  L"e/ed/Mental_Block"));
	skills.push_back(new PvESkill(GW::Constants::SkillID::Mindbender, L"c/c0/Mindbender"));
	skills.push_back(new PvESkill(GW::Constants::SkillID::Pain_Inverter, L"9/91/Pain_Inverter"));
	skills.push_back(new PvESkill(GW::Constants::SkillID::Radiation_Field,  L"3/31/Radiation_Field"));
	skills.push_back(new PvESkill(GW::Constants::SkillID::Smooth_Criminal, L"3/33/Smooth_Criminal"));
	skills.push_back(new PvESkill(GW::Constants::SkillID::Summon_Ice_Imp, L"2/2a/Summon_Ice_Imp"));
	skills.push_back(new PvESkill(GW::Constants::SkillID::Summon_Mursaat, L"6/61/Summon_Mursaat"));
	skills.push_back(new PvESkill(GW::Constants::SkillID::Summon_Naga_Shaman, L"f/f0/Summon_Naga_Shaman"));
	skills.push_back(new PvESkill(GW::Constants::SkillID::Summon_Ruby_Djinn, L"a/a0/Summon_Ruby_Djinn"));
	skills.push_back(new PvESkill(GW::Constants::SkillID::Technobabble, L"0/0a/Technobabble"));

	skills.push_back(new PvESkill(GW::Constants::SkillID::By_Urals_Hammer, L"d/df/%22By_Ural%27s_Hammer%21%22"));
	skills.push_back(new PvESkill(GW::Constants::SkillID::Dont_Trip, L"c/c1/%22Don%27t_Trip%21%22"));
	skills.push_back(new PvESkill(GW::Constants::SkillID::Alkars_Alchemical_Acid, L"4/43/Alkar%27s_Alchemical_Acid"));
	skills.push_back(new PvESkill(GW::Constants::SkillID::Black_Powder_Mine, L"5/50/Black_Powder_Mine"));
	skills.push_back(new PvESkill(GW::Constants::SkillID::Brawling_Headbutt , L"b/be/Brawling_Headbutt"));
	skills.push_back(new PvESkill(GW::Constants::SkillID::Breath_of_the_Great_Dwarf, L"0/0e/Breath_of_the_Great_Dwarf"));
	skills.push_back(new PvESkill(GW::Constants::SkillID::Drunken_Master, L"b/b3/Drunken_Master"));
	skills.push_back(new PvESkill(GW::Constants::SkillID::Dwarven_Stability, L"4/4c/Dwarven_Stability"));
	skills.push_back(new PvESkill(GW::Constants::SkillID::Ear_Bite, L"c/c6/Ear_Bite"));
	skills.push_back(new PvESkill(GW::Constants::SkillID::Great_Dwarf_Armor, L"e/e5/Great_Dwarf_Armor"));
	skills.push_back(new PvESkill(GW::Constants::SkillID::Great_Dwarf_Weapon, L"a/ab/Great_Dwarf_Weapon"));
	skills.push_back(new PvESkill(GW::Constants::SkillID::Light_of_Deldrimor, L"1/11/Light_of_Deldrimor"));
	skills.push_back(new PvESkill(GW::Constants::SkillID::Low_Blow, L"8/86/Low_Blow"));
	skills.push_back(new PvESkill(GW::Constants::SkillID::Snow_Storm, L"a/a0/Snow_Storm"));

	skills.push_back(new PvESkill(GW::Constants::SkillID::Deft_Strike, L"6/62/Deft_Strike"));
	skills.push_back(new PvESkill(GW::Constants::SkillID::Ebon_Battle_Standard_of_Courage, L"5/53/Ebon_Battle_Standard_of_Courage"));
	skills.push_back(new PvESkill(GW::Constants::SkillID::Ebon_Battle_Standard_of_Honor, L"5/51/Ebon_Battle_Standard_of_Honor"));
	skills.push_back(new PvESkill(GW::Constants::SkillID::Ebon_Battle_Standard_of_Wisdom, L"e/eb/Ebon_Battle_Standard_of_Wisdom"));
	skills.push_back(new PvESkill(GW::Constants::SkillID::Ebon_Escape, L"b/bb/Ebon_Escape"));
	skills.push_back(new PvESkill(GW::Constants::SkillID::Ebon_Vanguard_Assassin_Support, L"0/03/Ebon_Vanguard_Assassin_Support"));
	skills.push_back(new PvESkill(GW::Constants::SkillID::Ebon_Vanguard_Sniper_Support, L"1/16/Ebon_Vanguard_Sniper_Support"));
	skills.push_back(new PvESkill(GW::Constants::SkillID::Signet_of_Infection, L"6/66/Signet_of_Infection"));
	skills.push_back(new PvESkill(GW::Constants::SkillID::Sneak_Attack, L"8/87/Sneak_Attack"));
	skills.push_back(new PvESkill(GW::Constants::SkillID::Tryptophan_Signet, L"7/70/Tryptophan_Signet"));
	skills.push_back(new PvESkill(GW::Constants::SkillID::Weakness_Trap, L"0/0d/Weakness_Trap"));
	skills.push_back(new PvESkill(GW::Constants::SkillID::Winds, L"0/0e/Winds"));

	skills.push_back(new PvESkill(GW::Constants::SkillID::Dodge_This, L"4/4b/%22Dodge_This%21%22"));
	skills.push_back(new PvESkill(GW::Constants::SkillID::Finish_Him, L"6/61/%22Finish_Him%21%22"));
	skills.push_back(new PvESkill(GW::Constants::SkillID::I_Am_Unstoppable, L"e/ed/%22I_Am_Unstoppable%21%22"));
	skills.push_back(new PvESkill(GW::Constants::SkillID::I_Am_the_Strongest, L"e/ec/%22I_Am_the_Strongest%21%22"));
	skills.push_back(new PvESkill(GW::Constants::SkillID::You_Are_All_Weaklings, L"a/a4/%22You_Are_All_Weaklings%21%22"));
	skills.push_back(new PvESkill(GW::Constants::SkillID::You_Move_Like_a_Dwarf, L"6/6a/%22You_Move_Like_a_Dwarf%21%22"));
	skills.push_back(new PvESkill(GW::Constants::SkillID::A_Touch_of_Guile, L"2/2d/A_Touch_of_Guile"));
	skills.push_back(new PvESkill(GW::Constants::SkillID::Club_of_a_Thousand_Bears, L"d/dc/Club_of_a_Thousand_Bears"));
	skills.push_back(new PvESkill(GW::Constants::SkillID::Feel_No_Pain, L"f/fe/Feel_No_Pain"));
	skills.push_back(new PvESkill(GW::Constants::SkillID::Raven_Blessing, L"0/0a/Raven_Blessing"));
	skills.push_back(new PvESkill(GW::Constants::SkillID::Ursan_Blessing, L"7/7b/Ursan_Blessing"));
	skills.push_back(new PvESkill(GW::Constants::SkillID::Volfen_Blessing, L"b/b2/Volfen_Blessing"));

	auto& h = heros.at(Campaign::EyeOfTheNorth);
	h.push_back(new HeroUnlock(GW::Constants::HeroID::Anton));
	h.push_back(new HeroUnlock(GW::Constants::HeroID::Gwen));
	h.push_back(new HeroUnlock(GW::Constants::HeroID::Hayda));
	h.push_back(new HeroUnlock(GW::Constants::HeroID::Jora));
	h.push_back(new HeroUnlock(GW::Constants::HeroID::Kahmu));
	h.push_back(new HeroUnlock(GW::Constants::HeroID::KeiranThackeray));
	h.push_back(new HeroUnlock(GW::Constants::HeroID::Livia));
	h.push_back(new HeroUnlock(GW::Constants::HeroID::Ogden));
	h.push_back(new HeroUnlock(GW::Constants::HeroID::PyreFierceshot));
	h.push_back(new HeroUnlock(GW::Constants::HeroID::Vekk));
	h.push_back(new HeroUnlock(GW::Constants::HeroID::Xandra));

}
void CompletionWindow::Initialize_Dungeons()
{
	LoadTextures(Dungeon::normal_mode_images);
	LoadTextures(Dungeon::hard_mode_images);
	auto& dungeons = missions.at(Campaign::BonusMissionPack);
	dungeons.push_back(new Dungeon(
		MapID::Catacombs_of_Kathandrax_Level_1, GW::Constants::QuestID::ZaishenBounty_Ilsundur_Lord_of_Fire));
	dungeons.push_back(new Dungeon(
		MapID::Rragars_Menagerie_Level_1, GW::Constants::QuestID::ZaishenBounty_Rragar_Maneater));
	dungeons.push_back(new Dungeon(
		MapID::Cathedral_of_Flames_Level_1, GW::Constants::QuestID::ZaishenBounty_Murakai_Lady_of_the_Night));
	dungeons.push_back(new Dungeon(
		MapID::Ooze_Pit_mission));
	dungeons.push_back(new Dungeon(
		MapID::Darkrime_Delves_Level_1));
dungeons.push_back(new Dungeon(
	MapID::Frostmaws_Burrows_Level_1));
dungeons.push_back(new Dungeon(
	MapID::Sepulchre_of_Dragrimmar_Level_1, GW::Constants::QuestID::ZaishenBounty_Remnant_of_Antiquities));
dungeons.push_back(new Dungeon(
	MapID::Ravens_Point_Level_1, GW::Constants::QuestID::ZaishenBounty_Plague_of_Destruction));
dungeons.push_back(new Dungeon(
	MapID::Vloxen_Excavations_Level_1, GW::Constants::QuestID::ZaishenBounty_Zoldark_the_Unholy));
dungeons.push_back(new Dungeon(
	MapID::Bogroot_Growths_Level_1));
dungeons.push_back(new Dungeon(
	MapID::Bloodstone_Caves_Level_1));
dungeons.push_back(new Dungeon(
	MapID::Shards_of_Orr_Level_1, GW::Constants::QuestID::ZaishenBounty_Fendi_Nin));
dungeons.push_back(new Dungeon(
	MapID::Oolas_Lab_Level_1, GW::Constants::QuestID::ZaishenBounty_TPS_Regulator_Golem));
dungeons.push_back(new Dungeon(
	MapID::Arachnis_Haunt_Level_1, GW::Constants::QuestID::ZaishenBounty_Arachni));
dungeons.push_back(new Dungeon(
	MapID::Slavers_Exile_Level_1, {
		GW::Constants::QuestID::ZaishenBounty_Forgewight, 
		GW::Constants::QuestID::ZaishenBounty_Selvetarm,
		GW::Constants::QuestID::ZaishenBounty_Justiciar_Thommis, 
		GW::Constants::QuestID::ZaishenBounty_Rand_Stormweaver, 
		GW::Constants::QuestID::ZaishenBounty_Duncan_the_Black }));
dungeons.push_back(new Dungeon(
	MapID::Fronis_Irontoes_Lair_mission, { GW::Constants::QuestID::ZaishenBounty_Fronis_Irontoe }));
dungeons.push_back(new Dungeon(
	MapID::Secret_Lair_of_the_Snowmen));
dungeons.push_back(new Dungeon(
	MapID::Heart_of_the_Shiverpeaks_Level_1, { GW::Constants::QuestID::ZaishenBounty_Magmus }));
}


void CompletionWindow::Terminate()
{
	GW::StoC::RemoveCallback(GAME_SMSG_MAPS_UNLOCKED, &skills_unlocked_stoc_entry);
	GW::StoC::RemoveCallback(GAME_SMSG_VANQUISH_PROGRESS, &skills_unlocked_stoc_entry);
	GW::StoC::RemoveCallback(GAME_SMSG_VANQUISH_COMPLETE, &skills_unlocked_stoc_entry);
	GW::StoC::RemoveCallback(GAME_SMSG_SKILLS_UNLOCKED, &skills_unlocked_stoc_entry);
	GW::StoC::RemoveCallback(GAME_SMSG_INSTANCE_LOADED, &skills_unlocked_stoc_entry);
	GW::StoC::RemoveCallback(GAME_SMSG_SKILL_UPDATE_SKILL_COUNT_1, &skills_unlocked_stoc_entry);
	auto clear_vec = [](auto& vec) {
		for (auto& c : vec)
			for (auto i : c.second)
				delete i;
		vec.clear();
	};

	clear_vec(missions);
	clear_vec(vanquishes);
	clear_vec(pve_skills);
	clear_vec(elite_skills);
	clear_vec(heros);

	for (const auto& camp : character_completion)
		delete camp.second;
	character_completion.clear();
}
void CompletionWindow::Draw(IDirect3DDevice9* device)
{
	if (hom_achievements_status == 0) {
		character_completion[hom_achievements.character_name]->hom_code = hom_achievements.hom_code;
		hom_achievements_status = 0xf;
	}
	if (!visible) return;

	// TODO Button at the top to go to current daily
	ImGui::SetNextWindowCenter(ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(768, 768), ImGuiCond_FirstUseEver);

	if (!ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {
		ImGui::End();
		return;
	}
	constexpr float tabs_per_row = 4.f;
	const ImVec2 tab_btn_size = { ImGui::GetContentRegionAvail().x / tabs_per_row, 0.f };

	const std::wstring* sel = 0;
	if (chosen_player_name_s.empty()) {
		chosen_player_name = GetPlayerName();
		chosen_player_name_s = GuiUtils::WStringToString(chosen_player_name);
		CheckProgress();
	}

	const float gscale = ImGui::GetIO().FontGlobalScale;
	ImGui::Text("Choose Character");
	ImGui::SameLine();
	ImGui::PushItemWidth(200.f * gscale);
	if (ImGui::BeginCombo("##completion_character_select", chosen_player_name_s.c_str())) // The second parameter is the label previewed before opening the combo.
	{
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 2.f, 8.f });
		bool is_selected = false;
		for (auto& it : character_completion) {
			is_selected = false;
			if (!sel && chosen_player_name == it.first) {
				is_selected = true;
				sel = &it.first;
			}

			if (ImGui::Selectable(it.second->name_str.c_str(), is_selected)) {
				chosen_player_name = it.first;
				chosen_player_name_s = it.second->name_str;
				CheckProgress();
			}
		}
		ImGui::PopStyleVar();
		ImGui::EndCombo();
	}
	ImGui::PopItemWidth();
#if 1
	ImGui::SameLine();
	if (ImGui::Button("Change") && wcscmp(GetPlayerName(), chosen_player_name.c_str()) != 0)
		RerollWindow::Instance().Reroll(chosen_player_name.data(),false,false);
#endif
	ImGui::SameLine(ImGui::GetContentRegionAvail().x - (200.f * gscale));
	ImGui::Checkbox("View as list", &show_as_list);
	ImGui::SameLine();
	if(ImGui::Checkbox("Hard mode", &hard_mode)) {
		CheckProgress();
	}
	ImGui::Separator();
	ImGui::BeginChild("completion_scroll");
	float single_item_width = Mission::icon_size.x;
	if (show_as_list)
		single_item_width *= 5.f;
	int missions_per_row = (int)std::floor(ImGui::GetContentRegionAvail().x / (ImGui::GetIO().FontGlobalScale * single_item_width + (ImGui::GetStyle().ItemSpacing.x)));
	const float checkbox_offset = ImGui::GetContentRegionAvail().x - 200.f * ImGui::GetIO().FontGlobalScale;
	auto draw_missions = [missions_per_row, device](auto& camp_missions) {
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
		ImGui::Columns(static_cast<int>(missions_per_row), "###completion_section_cols", false);
		size_t items_per_col = (size_t)ceil(camp_missions.size() / static_cast<float>(missions_per_row));
		size_t col_count = 0;
		for (size_t i = 0; i < camp_missions.size(); i++) {
			if (camp_missions[i]->Draw(device)) {
				col_count++;
				if (col_count == items_per_col) {
					ImGui::NextColumn();
					col_count = 0;
				}
			}
		}
		ImGui::Columns(1);
		ImGui::PopStyleVar();
	};
	auto sort = [](auto& camp_missions) {
		bool can_sort = true;
		for (size_t i = 0; i < camp_missions.size() && can_sort; i++) {
			if (!camp_missions[i]->Name()[0])
				can_sort = false;
		}
		if (!can_sort)
			return false;
		std::ranges::sort(
			camp_missions,
			[](Missions::Mission* lhs, Missions::Mission* rhs) {
				return strcmp(lhs->Name(), rhs->Name()) < 0;
			});
		return true;
	};
	if (pending_sort) {
		bool sorted = true;
		for (auto it = missions.begin(); sorted && it != missions.end(); it++) {
			sorted = sort(it->second);
		}
		for (auto it = vanquishes.begin(); sorted && it != vanquishes.end(); it++) {
			sorted = sort(it->second);
		}
		/*for (auto it = pve_skills.begin(); sorted && it != pve_skills.end(); it++) {
			sorted = sort(it->second);
		}
		for (auto it = elite_skills.begin(); sorted && it != elite_skills.end(); it++) {
			sorted = sort(it->second);
		}*/
		for (auto it = heros.begin(); sorted && it != heros.end(); it++) {
			sorted = sort(it->second);
		}
		if (sorted)
			pending_sort = false;
	}

	ImGui::Text("Missions");
	ImGui::SameLine(checkbox_offset);
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 0,0 });
	ImGui::Checkbox("Hide completed missions", &hide_completed_missions);
	ImGui::PopStyleVar();
	for (auto& camp : missions) {
		auto& camp_missions = camp.second;
		size_t completed = 0;
		std::vector<Missions::Mission*> filtered;
		for (size_t i = 0; i < camp_missions.size(); i++) {
			if (camp_missions[i]->is_completed) {
				completed++;
				if (hide_completed_missions)
					continue;
			}
			filtered.push_back(camp_missions[i]);
		}
		char label[128];
		snprintf(label, _countof(label), "%s (%d of %d completed) - %.0f%%###campaign_missions_%d", CampaignName(camp.first), completed, camp_missions.size(), ((float)completed / (float)camp_missions.size()) * 100.f, camp.first);
		if (ImGui::CollapsingHeader(label)) {
			draw_missions(filtered);
		}
	}
	ImGui::Text("Vanquishes");
	ImGui::SameLine(checkbox_offset);
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 0,0 });
	ImGui::Checkbox("Hide completed vanquishes", &hide_completed_vanquishes);
	ImGui::PopStyleVar();
	for (auto& camp : vanquishes) {
		auto& camp_missions = camp.second;
		if (!camp_missions.size())
			continue;
		size_t completed = 0;
		std::vector<Missions::Mission*> filtered;
		for (size_t i = 0; i < camp_missions.size(); i++) {
			if (camp_missions[i]->is_completed) {
				completed++;
				if (hide_completed_vanquishes)
					continue;
			}
			filtered.push_back(camp_missions[i]);
		}
		char label[128];
		snprintf(label, _countof(label), "%s (%d of %d completed) - %.0f%%###campaign_vanquishes_%d", CampaignName(camp.first), completed, camp_missions.size(), ((float)completed / (float)camp_missions.size()) * 100.f, camp.first);
		if (ImGui::CollapsingHeader(label)) {
			draw_missions(filtered);
		}
	}


	auto skills_title = [&, checkbox_offset](const char* title) {
		ImGui::PushID(title);
		ImGui::Text(title);
		ImGui::ShowHelp("Guild Wars only shows skills learned for the current primary/secondary profession.\n\n"
			"GWToolbox remembers skills learned for other professions,\nbut is only able to update this info when you switch to that profession.");
		ImGui::SameLine(checkbox_offset);
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 0,0 });
		ImGui::Checkbox("Hide learned skills", &hide_unlocked_skills);
		ImGui::PopStyleVar();
		ImGui::PopID();
	};
	skills_title("Elite Skills");
	for (auto& camp : elite_skills) {
		auto& camp_missions = camp.second;
		size_t completed = 0;
		std::vector<Missions::Mission*> filtered;
		for (size_t i = 0; i < camp_missions.size(); i++) {
			if (camp_missions[i]->is_completed) {
				completed++;
				if (hide_unlocked_skills)
					continue;
			}
			filtered.push_back(camp_missions[i]);
		}
		char label[128];
		snprintf(label, _countof(label), "%s (%d of %d completed) - %.0f%%###campaign_eskills_%d", CampaignName(camp.first), completed, camp_missions.size(), ((float)completed / (float)camp_missions.size()) * 100.f, camp.first);
		if (ImGui::CollapsingHeader(label)) {
			draw_missions(filtered);
		}
	}
	skills_title("PvE Skills");
	for (auto& camp : pve_skills) {
		auto& camp_missions = camp.second;
		size_t completed = 0;
		std::vector<Missions::Mission*> filtered;
		for (size_t i = 0; i < camp_missions.size(); i++) {
			if (camp_missions[i]->is_completed) {
				completed++;
				if (hide_unlocked_skills)
					continue;
			}
			filtered.push_back(camp_missions[i]);
		}
		char label[128];
		snprintf(label, _countof(label), "%s (%d of %d completed) - %.0f%%###campaign_skills_%d", CampaignName(camp.first), completed, camp_missions.size(), ((float)completed / (float)camp_missions.size()) * 100.f, camp.first);
		if (ImGui::CollapsingHeader(label)) {
			draw_missions(filtered);
		}
	}
	ImGui::Text("Heroes");
	for (auto& camp : heros) {
		auto& camp_missions = camp.second;
		size_t completed = 0;
		for (size_t i = 0; i < camp_missions.size(); i++) {
			if (camp_missions[i]->is_completed) {
				completed++;
			}
		}
		char label[128];
		snprintf(label, _countof(label), "%s (%d of %d completed) - %.0f%%###campaign_heros_%d", CampaignName(camp.first), completed, camp_missions.size(), ((float)completed / (float)camp_missions.size()) * 100.f, camp.first);
		if (ImGui::CollapsingHeader(label)) {
			draw_missions(camp_missions);
		}
	}
	ImGui::Text("Hall of Monuments");
	auto hom = character_completion[chosen_player_name]->hom_achievements;
	// Devotion
	uint32_t completed = 0;
	if (hom) {
		for (size_t i = 0; i < _countof(hom->devotion_points); i++) {
			completed += hom->devotion_points[i];
		}
	}
	char label[128];
	snprintf(label, _countof(label), "%s (%d of %d completed) - %.0f%%###devotion_points", "Devotion", completed, DevotionPoints::TotalAvailable, ((float)completed / (float)DevotionPoints::TotalAvailable) * 100.f);
	if (ImGui::CollapsingHeader(label)) {
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
		ImGui::Columns(static_cast<int>(missions_per_row), "###completion_section_cols", false);
		size_t items_per_col = (size_t)ceil(_countof(hom->devotion_points) / static_cast<float>(missions_per_row));
		size_t col_count = 0;
		for (size_t i = 0; i < _countof(hom->devotion_points); i++) {
			// TODO: Get encoded name for hom points.
			col_count++;
			if (col_count == items_per_col) {
				ImGui::NextColumn();
				col_count = 0;
			}
		}
		ImGui::Columns(1);
		ImGui::PopStyleVar();
	}
	ImGui::EndChild();
	ImGui::End();
}
void CompletionWindow::DrawSettingInternal()
{
	ToolboxWindow::DrawSettingInternal();
}

void CompletionWindow::LoadSettings(CSimpleIni* ini)
{
	ToolboxWindow::LoadSettings(ini);
	CSimpleIni* completion_ini = new CSimpleIni(false, false, false);
	completion_ini->LoadFile(Resources::GetPath(completion_ini_filename).c_str());
	std::string ini_str;
	std::wstring name_ws;
	const char* ini_section;

	hide_unlocked_skills = ini->GetBoolValue(Name(), VAR_NAME(hide_unlocked_skills), hide_unlocked_skills);
	hide_completed_vanquishes = ini->GetBoolValue(Name(), VAR_NAME(hide_completed_vanquishes), hide_completed_vanquishes);
	hide_completed_missions = ini->GetBoolValue(Name(), VAR_NAME(hide_completed_missions), hide_completed_missions);

	auto read_ini_to_buf = [&](CompletionType type, const char* section) {
		char ini_key_buf[64];
		snprintf(ini_key_buf, _countof(ini_key_buf), "%s_length", section);
		int len = completion_ini->GetLongValue(ini_section, ini_key_buf, 0);
		if (len < 1)
			return;
		snprintf(ini_key_buf, _countof(ini_key_buf), "%s_values", section);
		std::string val = completion_ini->GetValue(ini_section, ini_key_buf, "");
		if (val.empty())
			return;
		std::vector<uint32_t> completion_buf(len);
		ASSERT(GuiUtils::IniToArray(val, completion_buf.data(), len));
		ParseCompletionBuffer(type, name_ws.data(), completion_buf.data(), completion_buf.size());
	};

	CSimpleIni::TNamesDepend entries;
	completion_ini->GetAllSections(entries);
	for (CSimpleIni::Entry& entry : entries) {
		ini_section = entry.pItem;
		name_ws = GuiUtils::StringToWString(ini_section);
		
		read_ini_to_buf(CompletionType::Mission, "mission");
		read_ini_to_buf(CompletionType::MissionBonus, "mission_bonus");
		read_ini_to_buf(CompletionType::MissionHM, "mission_hm");
		read_ini_to_buf(CompletionType::MissionBonusHM, "mission_bonus_hm");
		read_ini_to_buf(CompletionType::Skills, "skills");
		read_ini_to_buf(CompletionType::Vanquishes, "vanquishes");
		read_ini_to_buf(CompletionType::Heroes, "heros");
		read_ini_to_buf(CompletionType::MapsUnlocked, "maps_unlocked");

		Completion* c = GetCharacterCompletion(name_ws.data());
		if(c)
			c->profession = (GW::Constants::Profession)completion_ini->GetLongValue(ini_section, "profession", 0);
	}
	CheckProgress();
}
CompletionWindow* CompletionWindow::CheckProgress() {
	for (auto& camp : pve_skills) {
		for (auto& skill : camp.second) {
			skill->CheckProgress(chosen_player_name);
		}
	}
	for (auto& camp : elite_skills) {
		for (auto& skill : camp.second) {
			skill->CheckProgress(chosen_player_name);
		}
	}
	for (auto& camp : missions) {
		for (auto& skill : camp.second) {
			skill->CheckProgress(chosen_player_name);
		}
	}
	for (auto& camp : vanquishes) {
		for (auto& skill : camp.second) {
			skill->CheckProgress(chosen_player_name);
		}
	}
	for (auto& camp : heros) {
		for (auto& skill : camp.second) {
			skill->CheckProgress(chosen_player_name);
		}
	}
	hom_achievements_status = 0xf;
	auto& cc = CompletionWindow::Instance().character_completion;
	if (cc.contains(chosen_player_name)) {
		if (!cc[chosen_player_name]->hom_achievements)
			cc[chosen_player_name]->hom_achievements = new HallOfMonumentsAchievements();
		HallOfMonumentsModule::Instance().AsyncGetAccountAchievements(chosen_player_name.c_str(), cc[chosen_player_name]->hom_achievements);
	}
	return this;
}
void CompletionWindow::SaveSettings(CSimpleIni* ini)
{
	ToolboxWindow::SaveSettings(ini);
	CSimpleIni* completion_ini = new CSimpleIni(false, false, false);
	std::string ini_str;
	std::string* name;
	Completion* char_comp;

	ini->SetBoolValue(Name(), VAR_NAME(show_as_list), show_as_list);
	ini->SetBoolValue(Name(), VAR_NAME(hide_unlocked_skills), hide_unlocked_skills);
	ini->SetBoolValue(Name(), VAR_NAME(hide_completed_vanquishes), hide_completed_vanquishes);
	ini->SetBoolValue(Name(), VAR_NAME(hide_completed_missions), hide_completed_missions);

	auto write_buf_to_ini = [completion_ini](const char* section, std::vector<uint32_t>* read, std::string& ini_str,std::string* name) {
		char ini_key_buf[64];
		snprintf(ini_key_buf, _countof(ini_key_buf), "%s_length", section);
		completion_ini->SetLongValue(name->c_str(), ini_key_buf, read->size());
		ASSERT(GuiUtils::ArrayToIni(read->data(), read->size(), &ini_str));
		snprintf(ini_key_buf, _countof(ini_key_buf), "%s_values", section);
		completion_ini->SetValue(name->c_str(), ini_key_buf, ini_str.c_str());
	};

	for (auto& char_unlocks : character_completion) {
		
		char_comp = char_unlocks.second;
		name = &char_comp->name_str;
		completion_ini->SetLongValue(name->c_str(), "profession", (uint32_t)char_comp->profession);
		write_buf_to_ini("mission", &char_comp->mission, ini_str, name);
		write_buf_to_ini("mission_bonus", &char_comp->mission_bonus, ini_str, name);
		write_buf_to_ini("mission_hm", &char_comp->mission_hm, ini_str, name);
		write_buf_to_ini("mission_bonus_hm", &char_comp->mission_bonus_hm, ini_str, name);
		write_buf_to_ini("skills", &char_comp->skills, ini_str, name);
		write_buf_to_ini("vanquishes", &char_comp->vanquishes, ini_str, name);
		write_buf_to_ini("heros", &char_comp->heroes, ini_str, name);
		write_buf_to_ini("maps_unlocked", &char_comp->maps_unlocked, ini_str, name);
		completion_ini->SetValue(name->c_str(), "hom_code", char_comp->hom_code.c_str());
	}
	completion_ini->SaveFile(Resources::GetPath(completion_ini_filename).c_str());
	delete completion_ini;
}

CompletionWindow::Completion* CompletionWindow::GetCharacterCompletion(const wchar_t* character_name, bool create_if_not_found) {
	Completion* this_character_completion = 0;
	auto found = character_completion.find(character_name);
	if (found == character_completion.end()) {
		if (create_if_not_found) {
			this_character_completion = new Completion();
			this_character_completion->name_str = GuiUtils::WStringToString(character_name);
			character_completion[character_name] = this_character_completion;
		}
	}
	else {
		this_character_completion = found->second;
	}
	return this_character_completion;
}

CompletionWindow* CompletionWindow::ParseCompletionBuffer(CompletionType type, wchar_t* character_name, uint32_t* buffer, size_t len) {
	bool from_game = false;
	if (!character_name) {
		from_game = true;
		GW::GameContext* g = GW::GameContext::instance();
		if (!g) return this;
		GW::CharContext* c = g->character;
		if (!c) return this;
		GW::WorldContext* w = g->world;
		if (!w) return this;
		character_name = c->player_name;
		switch (type) {
		case CompletionType::Mission:
			buffer = w->missions_completed.m_buffer;
			len = w->missions_completed.m_size;
			break;
		case CompletionType::MissionBonus:
			buffer = w->missions_bonus.m_buffer;
			len = w->missions_bonus.m_size;
			break;
		case CompletionType::MissionHM:
			buffer = w->missions_completed_hm.m_buffer;
			len = w->missions_completed_hm.m_size;
			break;
		case CompletionType::MissionBonusHM:
			buffer = w->missions_bonus_hm.m_buffer;
			len = w->missions_bonus_hm.m_size;
			break;
		case CompletionType::Skills:
			buffer = w->unlocked_character_skills.m_buffer;
			len = w->unlocked_character_skills.m_size;
			break;
		case CompletionType::Vanquishes:
			buffer = w->vanquished_areas.m_buffer;
			len = w->vanquished_areas.m_size;
			break;
		case CompletionType::Heroes:
			buffer = (uint32_t*)w->hero_info.m_buffer;
			len = w->hero_info.m_size;
			break;
		case CompletionType::MapsUnlocked:
			buffer = (uint32_t*)w->unlocked_map.m_buffer;
			len = w->unlocked_map.m_size;
			break;
		default:
			ASSERT("Invalid CompletionType" && false);
		}
	}
	Completion* this_character_completion = GetCharacterCompletion(character_name,true);
	std::vector<uint32_t>* write_buf = 0;
	switch (type) {
	case CompletionType::Mission:
		write_buf = &this_character_completion->mission;
		break;
	case CompletionType::MissionBonus:
		write_buf = &this_character_completion->mission_bonus;
		break;
	case CompletionType::MissionHM:
		write_buf = &this_character_completion->mission_hm;
		break;
	case CompletionType::MissionBonusHM:
		write_buf = &this_character_completion->mission_bonus_hm;
		break;
	case CompletionType::Skills:
		write_buf = &this_character_completion->skills;
		break;
	case CompletionType::Vanquishes:
		write_buf = &this_character_completion->vanquishes;
		break;
	case CompletionType::Heroes: {
		write_buf = &this_character_completion->heroes;
		if (from_game) {
			// Writing from game memory, not from file
			std::vector<uint32_t>& write = *write_buf;
			GW::HeroInfo* hero_arr = (GW::HeroInfo*)buffer;
			if (write.size() < len) {
				write.resize(len, 0);
			}
			for (size_t i = 0; i < len; i++) {
				write[i] = hero_arr[i].hero_id;
			}
			return this;
		}
	} break;
	case CompletionType::MapsUnlocked:
		write_buf = &this_character_completion->maps_unlocked;
		break;
	default:
		ASSERT("Invalid CompletionType" && false);
	}
	std::vector<uint32_t>& write = *write_buf;
	if (write.size() < len) {
		write.resize(len,0);
	}
	for (size_t i = 0; i < len; i++) {
		write[i] |= buffer[i];
	}
	return this;
}

