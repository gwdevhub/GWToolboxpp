#include "stdafx.h"


#include <GWCA/Packets/Opcodes.h>

#include <GWCA/Context/CharContext.h>
#include <GWCA/Context/WorldContext.h>
#include <GWCA/Context/GameContext.h>

#include <GWCA/GameEntities/Skill.h>
#include <GWCA/GameEntities/Quest.h>
#include <GWCA/GameEntities/Map.h>
#include <GWCA/GameEntities/Player.h>
#include <GWCA/GameEntities/Hero.h>

#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Managers/PlayerMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>

#include <Modules/Resources.h>

#include <Windows/RerollWindow.h>
#include <Windows/CompletionWindow.h>
#include <Windows/CompletionWindow_Constants.h>
#include <Windows/TravelWindow.h>

#include <Color.h>
#include <Modules/DialogModule.h>
#include <GWCA/Context/PreGameContext.h>
#include <GWCA/Managers/QuestMgr.h>

using namespace GW::Constants;
using namespace Missions;
using namespace CompletionWindow_Constants;

namespace {

    bool ArrayBoolAt(GW::Array<uint32_t>& array, uint32_t index)
    {
        uint32_t real_index = index / 32;
        if (real_index >= array.size())
            return false;
        uint32_t shift = index % 32;
        uint32_t flag = 1 << shift;
        return (array[real_index] & flag) != 0;
    }
    bool ArrayBoolAt(std::vector<uint32_t>& array, uint32_t index)
    {
        uint32_t real_index = index / 32;
        if (real_index >= array.size())
            return false;
        uint32_t shift = index % 32;
        uint32_t flag = 1 << shift;
        return (array[real_index] & flag) != 0;
    }
    void ArrayBoolSet(std::vector<uint32_t>& array, uint32_t index, bool is_set = true)
    {
        uint32_t real_index = (uint32_t)index / 32;
        if (real_index >= array.size()) {
            array.resize(real_index + 1, 0);
        }
        uint32_t shift = (uint32_t)index % 32;
        uint32_t flag = 1u << shift;
        if (is_set) {
            array[real_index] |= flag;
        }
        else {
            array[real_index] ^= flag;
        }
    }

    const wchar_t* GetAccountEmail() {
        const auto c = GW::GetCharContext();
        return c && *c->player_email ? c->player_email : nullptr;
    }

    const wchar_t* GetPlayerName() {
        auto c = GW::GetCharContext();
        return c && *c->player_name ? c->player_name : nullptr;
    }

    wchar_t last_player_name[20];

    bool show_as_list = true;

    std::wstring chosen_player_name;
    std::string chosen_player_name_s;

    bool hide_unlocked_achievements = false;
    bool hide_unlocked_skills = false;
    bool hide_completed_vanquishes = false;
    bool hide_completed_missions = false;
    bool hide_collected_hats = false;

    bool pending_sort = true;
    const char* completion_ini_filename = "character_completion.ini";

    bool hard_mode = false;

    bool IsHardMode() { return hard_mode; }
    enum CompletionType : uint8_t
    {
        Skills,
        Mission,
        MissionBonus,
        MissionHM,
        MissionBonusHM,
        Vanquishes,
        Heroes,
        MapsUnlocked,
        MinipetsUnlocked,
        FestivalHats
    };


    std::unordered_map<std::wstring, CharacterCompletion*> character_completion;
    GW::HookEntry skills_unlocked_stoc_entry;

    std::map<GW::Constants::Campaign, std::vector<Missions::Mission*>> missions;
    std::map<GW::Constants::Campaign, std::vector<Missions::Mission*>> vanquishes;
    std::map<GW::Constants::Campaign, std::vector<Missions::PvESkill*>> elite_skills;
    std::map<GW::Constants::Campaign, std::vector<Missions::PvESkill*>> pve_skills;
    std::map<GW::Constants::Campaign, std::vector<Missions::HeroUnlock*>> heros;
    std::vector<Missions::FestivalHat*> festival_hats;
    std::vector<Missions::MinipetAchievement*> minipets;
    std::vector<Missions::WeaponAchievement*> hom_weapons;
    std::vector<Missions::ArmorAchievement*> hom_armor;
    std::vector<Missions::CompanionAchievement*> hom_companions;
    std::vector<Missions::HonorAchievement*> hom_titles;
    bool minipets_sorted = false;
    HallOfMonumentsAchievements hom_achievements;
    int hom_achievements_status = 0xf;

    CompletionWindow& Instance() {
        return CompletionWindow::Instance();
    }

    HallOfMonumentsAchievements* GetCharacterHom(const std::wstring& player_name) {
        const auto cc = Instance().GetCharacterCompletion(player_name.c_str(),false);
        return cc ? &cc->hom_achievements : nullptr;
    }

    void OnCycleDisplayedMinipetsButton(const GW::UI::DialogButtonInfo* button) {
        if (wcsncmp(button->message, L"\x8102\x2B96\xA802\xD212\x380C", 5) != 0)
            return; // Not "Cycle displayed minipets"
        const wchar_t* dialog_body = DialogModule::Instance().GetDialogBody();
        if (!(dialog_body && wcsncmp(dialog_body, L"\x8102\x2B9D\xDE1D\xB19F\x52DD", 5) == 0))
            return; // Not devotion dialog "Miniatures on display"
        const std::wregex displayed_miniatures(L"\x2\x102\x2([^\x102\x2]+)");
        std::wsmatch m;
        std::wstring subject(dialog_body);
        std::wstring msg;
        auto cc = character_completion[GetPlayerName()];
        auto& minipets_unlocked = cc->minipets_unlocked;
        minipets_unlocked.clear();
        while (std::regex_search(subject, m, displayed_miniatures)) {
            std::wstring miniature_encoded_name(m[1].str());
            for (size_t i = 0; i < _countof(encoded_minipet_names); i++) {
                if (encoded_minipet_names[i] == miniature_encoded_name) {
                    ArrayBoolSet(minipets_unlocked, i, true);
                    break;
                }
            }
            subject = m.suffix().str();
        }
        const std::wregex available_miniatures(L"\x2\x109\x2([^\x109\x2]+)");
        while (std::regex_search(subject, m, available_miniatures)) {
            std::wstring miniature_encoded_name(m[1].str());
            for (size_t i = 0; i < _countof(encoded_minipet_names); i++) {
                if (encoded_minipet_names[i] == miniature_encoded_name) {
                    ArrayBoolSet(minipets_unlocked, i, true);
                    break;
                }
            }
            subject = m.suffix().str();
        }
        Instance().CheckProgress();
    }
    void OnFestivalHatButton(const GW::UI::DialogButtonInfo* button) {
        if (wcsncmp(button->message, L"\x8101\x62E2\xAD6D\x82EB\x4C26 ", 5) != 0)
            return; // Not "Lets talk about something else"
        const wchar_t* dialog_body = DialogModule::Instance().GetDialogBody();
        if (!(dialog_body && wcsncmp(dialog_body, L"\x8101\x62E3\x8BAE\xA150\x1329", 5) == 0))
            return; // Not "Which festival hat would you like me to make you?"

        const auto& buttons = DialogModule::Instance().GetDialogButtons();
        auto cc = character_completion[GetPlayerName()];
        auto& unlocked = cc->festival_hats;
        for (const auto btn : buttons) {
            for (size_t i = 0; i < _countof(encoded_festival_hat_names);i++) {
                if (wcsstr(btn->message, encoded_festival_hat_names[i])) {
                    ArrayBoolSet(unlocked, i, true);
                    break;
                }
            }
        }
        Instance().CheckProgress();
    }

    // Check for "Cycle displayed minipets" button - if present, this is our hom dialog!
    void OnDialogButton(GW::HookStatus*, GW::UI::UIMessage message_id, void* wparam, void*) {
        ASSERT(message_id == GW::UI::UIMessage::kDialogButton);
        GW::UI::DialogButtonInfo* button = (GW::UI::DialogButtonInfo*)wparam;
        OnCycleDisplayedMinipetsButton(button);
        OnFestivalHatButton(button);
    }

    // Flag miniature as unlocked for current character when dedicated
    void OnSendDialog(GW::HookStatus*, GW::UI::UIMessage message_id, void* wparam, void*) {
        ASSERT(message_id == GW::UI::UIMessage::kSendDialog);
        if (GW::Map::GetMapID() != MapID::Hall_of_Monuments)
            return;
        uint32_t dialog_id = (uint32_t)wparam;
        const auto& available_dialogs = DialogModule::Instance().GetDialogButtons();
        const auto this_dialog_button = std::ranges::find_if(available_dialogs, [dialog_id](auto d) { return d->dialog_id == dialog_id; });
        if (this_dialog_button == available_dialogs.end())
            return;
        std::wregex miniature_displayed_regex(L"\x8102\x2B91\xDAA2\xD19F\x32DB\x10A([^\x1]+)");
        std::wsmatch m;
        std::wstring subject((*this_dialog_button)->message);
        std::wstring msg;
        auto cc = character_completion[GetPlayerName()];
        auto& minipets_unlocked = cc->minipets_unlocked;
        if (!std::regex_search(subject, m, miniature_displayed_regex))
            return;
        std::wstring miniature_encoded_name(m[1].str());
        for (size_t i = 0; i < _countof(encoded_minipet_names); i++) {
            if (encoded_minipet_names[i] == miniature_encoded_name) {
                ArrayBoolSet(minipets_unlocked, i, true);
                Instance().CheckProgress();
                break;
            }
        }
    }

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

	void OnHomLoaded(HallOfMonumentsAchievements* result) {
		if (result->state != HallOfMonumentsAchievements::State::Done) {
            Log::LogW(L"Failed to load Hall of Monuments achievements for %s", result->character_name.c_str());
            return;
		}
        //Log::InfoW(L"Loaded Hom for %s", result->character_name);
        CompletionWindow::Instance().CheckProgress();
	}

	void FetchHom(HallOfMonumentsAchievements* out = nullptr) {
        if (!out)
        {
            const auto player_name = GetPlayerName();
            const auto cc = CompletionWindow::Instance().GetCharacterCompletion(player_name, true);
            if (!cc) return;
            out = &cc->hom_achievements;
		}
        if (!out->isLoading())
        {
            HallOfMonumentsModule::AsyncGetAccountAchievements(out->character_name, out, OnHomLoaded);
		}

	}

    bool ParseCompletionBuffer(CompletionType type, wchar_t* character_name = nullptr, uint32_t* buffer = nullptr, size_t len = 0)
    {
        bool from_game = false;
        if (!character_name) {
            from_game = true;
            GW::GameContext* g = GW::GetGameContext();
            if (!g)
                return false;
            GW::CharContext* c = g->character;
            if (!c)
                return false;
            GW::WorldContext* w = g->world;
            if (!w)
                return false;
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
                default: ASSERT("Invalid CompletionType" && false);
            }
        }
        const auto this_character_completion = Instance().GetCharacterCompletion(character_name, true);
        std::vector<uint32_t>* write_buf = 0;
        switch (type) {
            case CompletionType::Mission: write_buf = &this_character_completion->mission; break;
            case CompletionType::MissionBonus: write_buf = &this_character_completion->mission_bonus; break;
            case CompletionType::MissionHM: write_buf = &this_character_completion->mission_hm; break;
            case CompletionType::MissionBonusHM: write_buf = &this_character_completion->mission_bonus_hm; break;
            case CompletionType::Skills: write_buf = &this_character_completion->skills; break;
            case CompletionType::Vanquishes: write_buf = &this_character_completion->vanquishes; break;
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
                    return true;
                }
            } break;
            case CompletionType::MapsUnlocked: write_buf = &this_character_completion->maps_unlocked; break;
            case CompletionType::MinipetsUnlocked: write_buf = &this_character_completion->minipets_unlocked; break;
            case CompletionType::FestivalHats: write_buf = &this_character_completion->festival_hats; break;
            default: ASSERT("Invalid CompletionType" && false);
        }
        std::vector<uint32_t>& write = *write_buf;
        if (write.size() < len) {
            write.resize(len, 0);
        }
        for (size_t i = 0; i < len; i++) {
            write[i] |= buffer[i];
        }
        return true;
    }

    struct AccountInfo {
        std::wstring email;
        std::vector<std::wstring> characters;
    };
    bool only_show_account_chars = true;
    // Check login screen; assign missing characters to email account
    void RefreshAccountCharacters() {
        const auto email = GetAccountEmail();
        if (!email) return;
        const auto p = GW::GetPreGameContext();
        if (p) {
            for (const auto& character : p->chars) {
                auto cc = CompletionWindow::Instance().GetCharacterCompletion(character.character_name, true);
                cc->account = email;
            }
        }
        const auto pn = GetPlayerName();
        if (pn) {
            auto cc = CompletionWindow::Instance().GetCharacterCompletion(pn);
            if (cc) cc->account = email;
        }

    }

    void OnPostCheckUIState(GW::HookStatus* , GW::UI::UIMessage, void* , void* state) {
        if(state && *(uint32_t*)state == 2)
            RefreshAccountCharacters();
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



Mission::Mission(MapID _outpost,
                 const MissionImageList& _normal_mode_images,
                 const MissionImageList& _hard_mode_images,
                 QuestID _zm_quest)
    : outpost(_outpost), zm_quest(_zm_quest), normal_mode_textures(_normal_mode_images), hard_mode_textures(_hard_mode_images) {
    map_to = outpost;
    GW::AreaInfo* map_info = GW::Map::GetMapInfo(outpost);
    if (map_info)
        name.reset(map_info->name_id);
    };

MapID Mission::GetOutpost() {
    return TravelWindow::GetNearestOutpost(map_to);
}
bool Mission::Draw(IDirect3DDevice9* )
{
    const auto texture = GetMissionImage();

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
    MapID travel_to = GetOutpost();
    if (chosen_player_name != GetPlayerName()) {
        RerollWindow::Instance().Reroll(chosen_player_name.data(), travel_to);
        return;
    }
    if (travel_to == MapID::None) {
        Log::Error("Failed to find nearest outpost");
    }
    else {
        TravelWindow::Instance().Travel(travel_to, District::Current, 0);
    }
}
void Mission::CheckProgress(const std::wstring& player_name) {
    is_completed = bonus = false;
    const auto& completion = character_completion;
    if (!completion.contains(player_name))
        return;
    const auto& player_completion = completion.at(player_name);
    std::vector<uint32_t>* missions_complete = &player_completion->mission;
    std::vector<uint32_t>* missions_bonus = &player_completion->mission_bonus;
    if (hard_mode) {
        missions_complete = &player_completion->mission_hm;
        missions_bonus = &player_completion->mission_bonus_hm;
    }
    map_unlocked = player_completion->maps_unlocked.empty() || ArrayBoolAt(player_completion->maps_unlocked, static_cast<uint32_t>(outpost));
    is_completed = ArrayBoolAt(*missions_complete, static_cast<uint32_t>(outpost));
    bonus = ArrayBoolAt(*missions_bonus, static_cast<uint32_t>(outpost));
}
IDirect3DTexture9* Mission::GetMissionImage()
{
    auto* texture_list = &normal_mode_textures;

    if (hard_mode) {
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
    return GW::QuestMgr::GetQuest(zm_quest) != nullptr;
}

bool Dungeon::IsDaily()
{
    return false;
}
bool Dungeon::HasQuest()
{
    for (auto& zb : zb_quests) {
        if (GW::QuestMgr::GetQuest(zb)) {
            return true;
        }
    }
    return false;
}


HeroUnlock::HeroUnlock(HeroID _hero_id)
	: PvESkill(SkillID::No_Skill) {
	skill_id = (SkillID)_hero_id;
}
void HeroUnlock::CheckProgress(const std::wstring& player_name) {
    is_completed = false;
    const auto& skills = character_completion;
    if (!skills.contains(player_name))
        return;
    auto& heroes = skills.at(player_name)->heroes;
    is_completed = bonus = std::ranges::find(heroes, static_cast<uint32_t>(skill_id)) != heroes.end();
}
const char* HeroUnlock::Name() {
    return hero_names[(uint32_t)skill_id];
}
IDirect3DTexture9* HeroUnlock::GetMissionImage()
{
	if (!image) {
		image = new IDirect3DTexture9*;
        *image = nullptr;
		const auto path = Resources::GetPath(L"img/heros");
		Resources::EnsureFolderExists(path);
		wchar_t local_image[MAX_PATH];
		swprintf(local_image, _countof(local_image), L"%s/hero_%d.jpg", path.c_str(), skill_id);
		char remote_image[128];
		snprintf(remote_image, _countof(remote_image), "https://github.com/HasKha/GWToolboxpp/raw/master/resources/heros/hero_%d.jpg", skill_id);
		Resources::Instance().LoadTexture(image, local_image, remote_image);
	}
	return *image;
}
void HeroUnlock::OnClick() {
    wchar_t buf[128];
    swprintf(buf, 128, L"Game_link:Hero_%d", skill_id);
    GW::GameThread::Enqueue([buf]() {
        GuiUtils::OpenWiki(buf);
        });
}

ItemAchievement::ItemAchievement(size_t _encoded_name_index, const wchar_t* encoded_name)
	: PvESkill(SkillID::No_Skill) {
	encoded_name_index = _encoded_name_index;
    name.language(GW::Constants::TextLanguage::English);
	name.reset(encoded_name);
}
const char* ItemAchievement::Name() {
    return name.string().c_str();
}
IDirect3DTexture9* ItemAchievement::GetMissionImage()
{
    if (!name.wstring().empty()) {
        if (name.wstring() == L"Brown Rabbit") {
            return *Resources::GetItemImage(L"Brown Rabbit (miniature)");
        }
        if (name.wstring() == L"Oppressor's Bow") {
            return *Resources::GetItemImage(L"Oppressor's Longbow");
        }
        if (name.wstring() == L"Tormented Bow") {
            return *Resources::GetItemImage(L"Tormented Longbow");
        }
        if (name.wstring() == L"Destroyer Bow") {
            return *Resources::GetItemImage(L"Destroyer Longbow");
        }
        return *Resources::GetItemImage(name.wstring());
    }
    return nullptr;
}
void ItemAchievement::OnClick() {
    GW::GameThread::Enqueue([url = name.wstring()] {
        GuiUtils::OpenWiki(url);
    });
}

IDirect3DTexture9* PvESkill::GetMissionImage()
{
	if (!image)
		image = Resources::GetSkillImage(skill_id);
	return *image;
}
PvESkill::PvESkill(SkillID _skill_id)
	: Mission(MapID::None, dummy_var, dummy_var), skill_id(_skill_id) {
	if (_skill_id != SkillID::No_Skill) {
        const auto skill = GW::SkillbarMgr::GetSkillConstantData(skill_id);
		if (skill) {
			name.reset(skill->name);
			profession = skill->profession;
		}
    }
}
void PvESkill::OnClick() {
    const auto wtf = std::format(L"Game_link:Skill_{}", static_cast<std::underlying_type_t<SkillID>>(skill_id));
    // revert this dumb shit once Microsoft fixes the weird bug
    GW::GameThread::Enqueue([url = wtf] {
        GuiUtils::OpenWiki(url);
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

        const ImVec2 check_size = ImGui::CalcTextSize(ICON_FA_CHECK);
        ImGui::GetWindowDrawList()->AddText({screen_pos.x + (icon_size_scaled.x - check_size.x) / 2.f,
                                                screen_pos.y + (icon_size_scaled.y - check_size.y) / 2.f},
                                                completed_text, ICON_FA_CHECK);
        ImGui::SetCursorPos(cursor_pos2);
    }
    return true;
}
void PvESkill::CheckProgress(const std::wstring& player_name) {
    is_completed = false;
    const auto& skills = character_completion;
    if (!skills.contains(player_name))
        return;
    auto& unlocked = skills.at(player_name)->skills;
    is_completed = bonus = ArrayBoolAt(unlocked, static_cast<uint32_t>(skill_id));
}

FactionsPvESkill::FactionsPvESkill(SkillID skill_id)
    : PvESkill(skill_id) {
    GW::Skill* s = GW::SkillbarMgr::GetSkillConstantData(skill_id);
    uint32_t faction_id = 0x6C3D;
    if ((TitleID)s->title == TitleID::Luxon) {
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
    const auto& completion = character_completion;
    if (!completion.contains(player_name))
        return;
    std::vector<uint32_t>* missions_bonus = &completion.at(player_name)->mission_bonus;
    if (hard_mode) {
        missions_bonus = &completion.at(player_name)->mission_bonus_hm;
    }
    is_completed = bonus = ArrayBoolAt(*missions_bonus, static_cast<uint32_t>(outpost));
}
IDirect3DTexture9* EotNMission::GetMissionImage()
{
    auto* texture_list = &normal_mode_textures;
    if (hard_mode) {
        texture_list = &hard_mode_textures;
    }
    return texture_list->at(is_completed ? 1 : 0).texture;
}

void Vanquish::CheckProgress(const std::wstring& player_name) {
    is_completed = false;
    const auto& completion = character_completion;
    if (!completion.contains(player_name))
        return;
    auto& unlocked = completion.at(player_name)->vanquishes;
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
		{Campaign::Prophecies, {} },
		{Campaign::Factions, {} },
		{Campaign::Nightfall, {} },
		{Campaign::EyeOfTheNorth, {} },
		{Campaign::BonusMissionPack, {} },
	};
	vanquishes = {
		{Campaign::Prophecies, {} },
		{Campaign::Factions, {} },
		{Campaign::Nightfall, {} },
		{Campaign::EyeOfTheNorth, {} },
		{Campaign::BonusMissionPack, {} },
	};
	elite_skills = {
		{Campaign::Prophecies, {} },
		{Campaign::Factions, {} },
		{Campaign::Nightfall, {} },
		{Campaign::Core, {} },
	};
    pve_skills = {
        {Campaign::Factions, {} },
        {Campaign::Nightfall, {} },
        {Campaign::EyeOfTheNorth, {} },
        {Campaign::Core, {} },
	};
    heros = {
        {Campaign::Factions, {} },
        {Campaign::Nightfall, {} },
        {Campaign::EyeOfTheNorth, {} }
    };
    for (size_t i = 0; i < _countof(encoded_minipet_names); i++) {
        minipets.push_back(new MinipetAchievement(i,encoded_minipet_names[i]));
    }
    for (size_t i = 0; i < _countof(encoded_festival_hat_names); i++) {
        festival_hats.push_back(new FestivalHat(i, encoded_festival_hat_names[i]));
    }
    for (size_t i = 0; i < _countof(encoded_weapon_names); i++) {
        hom_weapons.push_back(new WeaponAchievement(i, encoded_weapon_names[i]));
    }

    hom_armor.push_back(new ArmorAchievement(hom_armor.size(), L"\x108\x107" "Elite Canthan Armor\x1","Elementalist_Elite_Canthan_armor_m.jpg"));
    hom_armor.push_back(new ArmorAchievement(hom_armor.size(), L"\x108\x107" "Elite Exotic Armor\x1", "Assassin_Exotic_armor_m.jpg"));
    hom_armor.push_back(new ArmorAchievement(hom_armor.size(), L"\x108\x107" "Elite Kurzick Armor\x1", "Warrior_Elite_Kurzick_armor_m.jpg"));
    hom_armor.push_back(new ArmorAchievement(hom_armor.size(), L"\x108\x107" "Elite Luxon Armor\x1", "Monk_Elite_Luxon_armor_f.jpg"));
    hom_armor.push_back(new ArmorAchievement(hom_armor.size(), L"\x108\x107" "Imperial Ascended Armor\x1", "Ritualist_Elite_Imperial_armor_f.jpg"));
    hom_armor.push_back(new ArmorAchievement(hom_armor.size(), L"\x108\x107" "Ancient Armor\x1", "Assassin_Ancient_armor_f.jpg"));
    hom_armor.push_back(new ArmorAchievement(hom_armor.size(), L"\x108\x107" "Elite Sunspear Armor\x1", "Dervish_Elite_Sunspear_armor_m.jpg"));
    hom_armor.push_back(new ArmorAchievement(hom_armor.size(), L"\x108\x107" "Vabbian Armor\x1", "Mesmer_Vabbian_armor_f.jpg"));
    hom_armor.push_back(new ArmorAchievement(hom_armor.size(), L"\x108\x107" "Primeval Armor\x1", "Necromancer_Primeval_armor_m.jpg"));
    hom_armor.push_back(new ArmorAchievement(hom_armor.size(), L"\x108\x107" "Asuran Armor\x1", "Paragon_Asuran_armor_m.jpg"));
    hom_armor.push_back(new ArmorAchievement(hom_armor.size(), L"\x108\x107" "Norn Armor\x1", "Ritualist_Norn_armor_m.jpg"));
    hom_armor.push_back(new ArmorAchievement(hom_armor.size(), L"\x108\x107" "Silver Eagle Armor\x1", "Warrior_Silver_Eagle_armor_f.jpg"));
    hom_armor.push_back(new ArmorAchievement(hom_armor.size(), L"\x108\x107" "Monument Armor\x1", "Monk_Monument_armor_m.jpg"));
    hom_armor.push_back(new ArmorAchievement(hom_armor.size(), L"\x108\x107" "Obsidian Armor\x1", "Elementalist_Obsidian_armor_m.jpg"));
    hom_armor.push_back(new ArmorAchievement(hom_armor.size(), L"\x108\x107" "Granite Citadel Elite Armor\x1", "Ranger_Elite_Fur-Lined_armor_m.jpg"));
    hom_armor.push_back(new ArmorAchievement(hom_armor.size(), L"\x108\x107" "Granite Citadel Exclusive Armor\x1", "Elementalist_Elite_Iceforged_armor_f.jpg"));
    hom_armor.push_back(new ArmorAchievement(hom_armor.size(), L"\x108\x107" "Granite Citadel Ascended Armor\x1", "Warrior_Elite_Platemail_armor_m.jpg"));
    hom_armor.push_back(new ArmorAchievement(hom_armor.size(), L"\x108\x107" "Marhan's Grotto Elite Armor\x1", "Ranger_Elite_Druid_armor_f.jpg"));
    hom_armor.push_back(new ArmorAchievement(hom_armor.size(), L"\x108\x107" "Marhan's Grotto Exclusive Armor\x1", "Elementalist_Elite_Stormforged_armor_f.jpg"));
    hom_armor.push_back(new ArmorAchievement(hom_armor.size(), L"\x108\x107" "Marhan's Grotto Ascended Armor\x1", "Warrior_Elite_Templar_armor_m.jpg"));

    ASSERT(hom_armor.size() == (size_t)ResilienceDetail::Count);

    hom_companions.push_back(new CompanionAchievement(hom_companions.size(), L"\x108\x107" "Zenmai\x1","Zenmai_statue.jpg"));
    hom_companions.push_back(new CompanionAchievement(hom_companions.size(), L"\x108\x107" "Norgu\x1","Norgu_statue.jpg"));
    hom_companions.push_back(new CompanionAchievement(hom_companions.size(), L"\x108\x107" "Goren\x1","Goren_statue.jpg"));
    hom_companions.push_back(new CompanionAchievement(hom_companions.size(), L"\x108\x107" "Zhed Shadowhoof\x1","Zhed_Shadowhoof_statue.jpg"));
    hom_companions.push_back(new CompanionAchievement(hom_companions.size(), L"\x108\x107" "General Morgahn\x1","General_Morgahn_statue.jpg"));
    hom_companions.push_back(new CompanionAchievement(hom_companions.size(), L"\x108\x107" "Margrid The Sly\x1","Margrid_the_Sly_statue.jpg"));
    hom_companions.push_back(new CompanionAchievement(hom_companions.size(), L"\x108\x107" "Tahlkora\x1","Tahlkora_statue.jpg"));
    hom_companions.push_back(new CompanionAchievement(hom_companions.size(), L"\x108\x107" "Razah\x1","Razah_statue.jpg"));
    hom_companions.push_back(new CompanionAchievement(hom_companions.size(), L"\x108\x107" "Master Of Whispers\x1","Master_of_Whispers_statue.jpg"));
    hom_companions.push_back(new CompanionAchievement(hom_companions.size(), L"\x108\x107" "Koss\x1","Koss_statue.jpg"));
    hom_companions.push_back(new CompanionAchievement(hom_companions.size(), L"\x108\x107" "Dunkoro\x1","Dunkoro_statue.jpg"));
    hom_companions.push_back(new CompanionAchievement(hom_companions.size(), L"\x108\x107" "Melonni\x1","Melonni_statue.jpg"));
    hom_companions.push_back(new CompanionAchievement(hom_companions.size(), L"\x108\x107" "Acolyte Jin\x1","Acolyte_Jin_statue.jpg"));
    hom_companions.push_back(new CompanionAchievement(hom_companions.size(), L"\x108\x107" "Acolyte Sousuke\x1","Acolyte_Sousuke_statue.jpg"));
    hom_companions.push_back(new CompanionAchievement(hom_companions.size(), L"\x108\x107" "Vekk\x1","Vekk_statue.jpg"));
    hom_companions.push_back(new CompanionAchievement(hom_companions.size(), L"\x108\x107" "Livia\x1", "Livia_statue.jpg"));
    hom_companions.push_back(new CompanionAchievement(hom_companions.size(), L"\x108\x107" "Hayda\x1", "Hayda_statue.jpg"));
    hom_companions.push_back(new CompanionAchievement(hom_companions.size(), L"\x108\x107" "Ogden Stonehealer\x1", "Ogden_Stonehealer_statue.jpg"));
    hom_companions.push_back(new CompanionAchievement(hom_companions.size(), L"\x108\x107" "Pyre Fierceshot\x1", "Pyre_Fierceshot_statue.jpg"));
    hom_companions.push_back(new CompanionAchievement(hom_companions.size(), L"\x108\x107" "Jora\x1", "Jora_statue.jpg"));
    hom_companions.push_back(new CompanionAchievement(hom_companions.size(), L"\x108\x107" "Kahmu\x1", "Kahmu_statue.jpg"));
    hom_companions.push_back(new CompanionAchievement(hom_companions.size(), L"\x108\x107" "Xandra\x1", "Xandra_statue.jpg"));
    hom_companions.push_back(new CompanionAchievement(hom_companions.size(), L"\x108\x107" "Anton\x1", "Anton_statue.jpg"));
    hom_companions.push_back(new CompanionAchievement(hom_companions.size(), L"\x108\x107" "Gwen\x1", "Gwen_statue.jpg"));
    hom_companions.push_back(new CompanionAchievement(hom_companions.size(), L"\x108\x107" "Animal Companion\x1", "Animal_Companion_statue.jpg"));
    hom_companions.push_back(new CompanionAchievement(hom_companions.size(), L"\x108\x107" "Black Moa\x1", "Black_Moa_statue.jpg"));
    hom_companions.push_back(new CompanionAchievement(hom_companions.size(), L"\x108\x107" "Imperial Phoenix\x1", "Phoenix_statue.jpg"));
    hom_companions.push_back(new CompanionAchievement(hom_companions.size(), L"\x108\x107" "Black Widow Spider\x1", "Black_Widow_statue.jpg"));
    hom_companions.push_back(new CompanionAchievement(hom_companions.size(), L"\x108\x107" "Olias\x1", "Olias_statue.jpg"));
    hom_companions.push_back(new CompanionAchievement(hom_companions.size(), L"\x108\x107" "MOX\x1", "M.O.X._statue.jpg"));

    ASSERT(hom_companions.size() == (size_t)FellowshipDetail::Count);

    size_t hom_titles_index = 0;
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Champion\x1","Eternal_Champion.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Commander\x1","Eternal_Commander.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Skillz\x1","Eternal_Skillz.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Gladiator\x1","Eternal_Gladiator.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Hero\x1","Eternal_Hero.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Lightbringer\x1","Eternal_Lightbringer.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Bookah\x1","Eternal_Bookah.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Delver\x1","Eternal_Delver.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Slayer\x1","Eternal_Slayer.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Ebon Vanguard Agent\x1","Eternal_Ebon_Vanguard_Agent.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Defender of Ascalon\x1","Eternal_Defender_of_Ascalon.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Tyrian Cartographer\x1","Eternal_Tyrian_Cartographer.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Guardian of Tyria\x1","Eternal_Guardian_of_Tyria.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Protector of Tyria\x1","Eternal_Protector_of_Tyria.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Tyrian Skill Hunter\x1","Eternal_Tyrian_Skill_Hunter.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Tyrian Vanquisher\x1","Eternal_Tyrian_Vanquisher.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Canthan Cartographer\x1","Eternal_Canthan_Cartographer.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Guardian of Cantha\x1","Eternal_Guardian_of_Cantha.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Protector of Cantha\x1","Eternal_Protector_of_Cantha.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Canthan Skill Hunter\x1","Eternal_Canthan_Skill_Hunter.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Canthan Vanquisher\x1","Eternal_Canthan_Vanquisher.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Savior of the Kurzicks\x1","Eternal_Savior_of_the_Kurzicks.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Savior of the Luxons\x1","Eternal_Savior_of_the_Luxons.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Elonian Cartographer\x1","Eternal_Elonian_Cartographer.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Guardian of Elona\x1","Eternal_Guardian_of_Elona.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Protector of Elona\x1","Eternal_Protector_of_Elona.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Elonian Skill Hunter\x1","Eternal_Elonian_Skill_Hunter.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Elonian Vanquisher\x1","Eternal_Elonian_Vanquisher.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Ale Hound\x1","Eternal_Ale_Hound.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Party Animal\x1","Eternal_Party_Animal.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Master of the North\x1","Eternal_Master_of_the_North.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Legendary Cartographer\x1","Eternal_Legendary_Cartographer.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Legendary Guardian\x1","Eternal_Legendary_Guardian.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Legendary Skill Hunter\x1","Eternal_Legendary_Skill_Hunter.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Legendary Vanquisher\x1","Eternal_Legendary_Vanquisher.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Fortune\x1","Eternal_Fortune.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Sweet Tooth\x1","Eternal_Sweet_Tooth.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Spearmarshal\x1","Eternal_Spearmarshal.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Survivor\x1","Eternal_Survivor.jpg"));
    hom_titles_index++; // NB: This is the character based survivor title, which isn't used anymore.
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Treasure Hunter\x1","Eternal_Treasure_Hunter.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Misfortune\x1","Eternal_Misfortune.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Source of Wisdom\x1","Eternal_Source_of_Wisdom.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Hero of Tyria\x1","Eternal_Hero_of_Tyria.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Hero of Cantha\x1","Eternal_Hero_of_Cantha.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Hero of Elona\x1","Eternal_Hero_of_Elona.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Conqueror of Sorrow's Furnace\x1","Eternal_Conqueror_of_Sorrow's_Furnace.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Conqueror of The Deep\x1","Eternal_Conqueror_of_the_Deep.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Conqueror of Urgoz's Warren\x1","Eternal_Conqueror_of_Urgoz's_Warren.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Conqueror of The Fissure of Woe\x1","Eternal_Conqueror_of_the Fissure of Woe.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Conqueror of The Underworld\x1","Eternal_Conqueror_of_the Underworld.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Conqueror of The Domain of Anguish\x1","Eternal_Conqueror_of_the_Domain_of_Anguish.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Zaishen Supporter\x1","Eternal_Zaishen_Supporter.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Codex Disciple\x1","Eternal_Codex_Disciple.png"));

    ASSERT(hom_titles_index == (size_t)HonorDetail::Count);

    Initialize_Prophecies();
    Initialize_Factions();
    Initialize_Nightfall();
    Initialize_EotN();
    Initialize_Dungeons();

    auto& eskills = elite_skills.at(Campaign::Core);

    eskills.push_back(new PvESkill(SkillID::Charge));
    eskills.push_back(new PvESkill(SkillID::Battle_Rage));
    eskills.push_back(new PvESkill(SkillID::Cleave));
    eskills.push_back(new PvESkill(SkillID::Devastating_Hammer));
    eskills.push_back(new PvESkill(SkillID::Hundred_Blades));
    eskills.push_back(new PvESkill(SkillID::Seven_Weapon_Stance));

    eskills.push_back(new PvESkill(SkillID::Together_as_one));
    eskills.push_back(new PvESkill(SkillID::Barrage));
    eskills.push_back(new PvESkill(SkillID::Escape));
    eskills.push_back(new PvESkill(SkillID::Ferocious_Strike));
    eskills.push_back(new PvESkill(SkillID::Quick_Shot));
    eskills.push_back(new PvESkill(SkillID::Spike_Trap));

    eskills.push_back(new PvESkill(SkillID::Blood_is_Power));
    eskills.push_back(new PvESkill(SkillID::Grenths_Balance));
    eskills.push_back(new PvESkill(SkillID::Lingering_Curse));
    eskills.push_back(new PvESkill(SkillID::Plague_Signet));
    eskills.push_back(new PvESkill(SkillID::Soul_Taker));
    eskills.push_back(new PvESkill(SkillID::Tainted_Flesh));

    eskills.push_back(new PvESkill(SkillID::Judgement_Strike));
    eskills.push_back(new PvESkill(SkillID::Martyr));
    eskills.push_back(new PvESkill(SkillID::Shield_of_Regeneration));
    eskills.push_back(new PvESkill(SkillID::Signet_of_Judgment));
    eskills.push_back(new PvESkill(SkillID::Spell_Breaker));
    eskills.push_back(new PvESkill(SkillID::Word_of_Healing));

    eskills.push_back(new PvESkill(SkillID::Crippling_Anguish));
    eskills.push_back(new PvESkill(SkillID::Echo));
    eskills.push_back(new PvESkill(SkillID::Energy_Drain));
    eskills.push_back(new PvESkill(SkillID::Energy_Surge));
    eskills.push_back(new PvESkill(SkillID::Mantra_of_Recovery));
    eskills.push_back(new PvESkill(SkillID::Time_Ward));

    eskills.push_back(new PvESkill(SkillID::Elemental_Attunement));
    eskills.push_back(new PvESkill(SkillID::Lightning_Surge));
    eskills.push_back(new PvESkill(SkillID::Mind_Burn));
    eskills.push_back(new PvESkill(SkillID::Mind_Freeze));
    eskills.push_back(new PvESkill(SkillID::Obsidian_Flesh));
    eskills.push_back(new PvESkill(SkillID::Over_the_Limit));

    eskills.push_back(new PvESkill(SkillID::Shadow_Theft));

    eskills.push_back(new PvESkill(SkillID::Weapons_of_Three_Forges));

    eskills.push_back(new PvESkill(SkillID::Vow_of_Revolution));

    eskills.push_back(new PvESkill(SkillID::Heroic_Refrain));

    auto& skills = pve_skills.at(Campaign::Core);
    skills.push_back(new PvESkill(SkillID::Seven_Weapon_Stance));
    skills.push_back(new PvESkill(SkillID::Together_as_one));
    skills.push_back(new PvESkill(SkillID::Soul_Taker));
    skills.push_back(new PvESkill(SkillID::Judgement_Strike));
    skills.push_back(new PvESkill(SkillID::Time_Ward));
    skills.push_back(new PvESkill(SkillID::Over_the_Limit));
    skills.push_back(new PvESkill(SkillID::Shadow_Theft));
    skills.push_back(new PvESkill(SkillID::Weapons_of_Three_Forges));
    skills.push_back(new PvESkill(SkillID::Vow_of_Revolution));
    skills.push_back(new PvESkill(SkillID::Heroic_Refrain));

    GW::StoC::RegisterPostPacketCallback(&skills_unlocked_stoc_entry, GAME_SMSG_MAPS_UNLOCKED, [](GW::HookStatus*, void*) {
        ParseCompletionBuffer(CompletionType::Mission);
        ParseCompletionBuffer(CompletionType::MissionBonus);
        ParseCompletionBuffer(CompletionType::MissionBonusHM);
        ParseCompletionBuffer(CompletionType::MissionHM);
        ParseCompletionBuffer(CompletionType::MapsUnlocked);
        Instance().CheckProgress();
        });
    GW::StoC::RegisterPostPacketCallback(&skills_unlocked_stoc_entry, GAME_SMSG_VANQUISH_PROGRESS, [](GW::HookStatus*, void*) {
        ParseCompletionBuffer(CompletionType::Vanquishes);
        Instance().CheckProgress();
        });
    GW::StoC::RegisterPostPacketCallback(&skills_unlocked_stoc_entry, GAME_SMSG_VANQUISH_COMPLETE, [](GW::HookStatus*, void*) {
        ParseCompletionBuffer(CompletionType::Vanquishes);
        Instance().CheckProgress();
        });


    GW::StoC::RegisterPostPacketCallback(&skills_unlocked_stoc_entry, GAME_SMSG_SKILLS_UNLOCKED, [](GW::HookStatus*, void*) {
        ParseCompletionBuffer(CompletionType::Skills);
	    Instance().CheckProgress();
	    });
    GW::StoC::RegisterPostPacketCallback(&skills_unlocked_stoc_entry, GAME_SMSG_AGENT_CREATE_PLAYER, [](GW::HookStatus*, void* pak) {
	    uint32_t player_number = ((uint32_t*)pak)[1];
	    const auto c = GW::GetCharContext();
        if (!(c && player_number == c->player_number))
            return;
		const auto me = GW::PlayerMgr::GetPlayerByID(c->player_number);
        if (!me)
            return;
        const auto comp = Instance().GetCharacterCompletion(c->player_name);
		if (comp)
			comp->profession = (Profession)me->primary;
        ParseCompletionBuffer(CompletionType::Heroes);
		Instance().CheckProgress();
	    });
    GW::StoC::RegisterPostPacketCallback(&skills_unlocked_stoc_entry, GAME_SMSG_SKILL_UPDATE_SKILL_COUNT_1, [](GW::HookStatus*, void*) {
        ParseCompletionBuffer(CompletionType::Skills);
	    Instance().CheckProgress();
	    });
    // Reset chosen player name to be current character on login
    GW::StoC::RegisterPostPacketCallback(&skills_unlocked_stoc_entry, GAME_SMSG_INSTANCE_LOADED, [&](GW::HookStatus*, void*) {
	    if (wcscmp(GetPlayerName(), last_player_name) != 0) {
		    wcscpy(last_player_name, GetPlayerName());
		    chosen_player_name_s.clear();
		    chosen_player_name.clear();
            FetchHom();
	    }
        ParseCompletionBuffer(CompletionType::Skills);
        ParseCompletionBuffer(CompletionType::Mission);
        ParseCompletionBuffer(CompletionType::MissionBonus);
        ParseCompletionBuffer(CompletionType::MissionBonusHM);
        ParseCompletionBuffer(CompletionType::MissionHM);
        ParseCompletionBuffer(CompletionType::MapsUnlocked);
        RefreshAccountCharacters();
	    CheckProgress();
	    });

    RegisterUIMessageCallback(&skills_unlocked_stoc_entry, GW::UI::UIMessage::kDialogButton, OnDialogButton);
    RegisterUIMessageCallback(&skills_unlocked_stoc_entry, GW::UI::UIMessage::kSendDialog, OnSendDialog);

    ParseCompletionBuffer(CompletionType::Mission);
    ParseCompletionBuffer(CompletionType::MissionBonus);
    ParseCompletionBuffer(CompletionType::MissionBonusHM);
    ParseCompletionBuffer(CompletionType::MissionHM);
    ParseCompletionBuffer(CompletionType::Skills);
    ParseCompletionBuffer(CompletionType::Vanquishes);
    ParseCompletionBuffer(CompletionType::Heroes);
    ParseCompletionBuffer(CompletionType::MapsUnlocked);
    CheckProgress();
    const wchar_t* player_name = GetPlayerName();
    if(player_name)
        wcscpy(last_player_name,player_name);

    RegisterUIMessageCallback(&skills_unlocked_stoc_entry, GW::UI::UIMessage::kCheckUIState, OnPostCheckUIState,0x8000);

    RefreshAccountCharacters();
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
	eskills.push_back(new PvESkill(SkillID::Victory_Is_Mine));
	eskills.push_back(new PvESkill(SkillID::Backbreaker));
	eskills.push_back(new PvESkill(SkillID::Bulls_Charge));
	eskills.push_back(new PvESkill(SkillID::Defy_Pain));
	eskills.push_back(new PvESkill(SkillID::Dwarven_Battle_Stance));
	eskills.push_back(new PvESkill(SkillID::Earth_Shaker));
	eskills.push_back(new PvESkill(SkillID::Eviscerate));
	eskills.push_back(new PvESkill(SkillID::Flourish));
	eskills.push_back(new PvESkill(SkillID::Gladiators_Defense));
	eskills.push_back(new PvESkill(SkillID::Skull_Crack));
	eskills.push_back(new PvESkill(SkillID::Warriors_Endurance));

	eskills.push_back(new PvESkill(SkillID::Crippling_Shot));
	eskills.push_back(new PvESkill(SkillID::Greater_Conflagration));
	eskills.push_back(new PvESkill(SkillID::Incendiary_Arrows));
	eskills.push_back(new PvESkill(SkillID::Marksmans_Wager));
	eskills.push_back(new PvESkill(SkillID::Melandrus_Arrows));
	eskills.push_back(new PvESkill(SkillID::Melandrus_Resilience));
	eskills.push_back(new PvESkill(SkillID::Oath_Shot));
	eskills.push_back(new PvESkill(SkillID::Poison_Arrow));
	eskills.push_back(new PvESkill(SkillID::Practiced_Stance));
	eskills.push_back(new PvESkill(SkillID::Punishing_Shot));

	eskills.push_back(new PvESkill(SkillID::Aura_of_the_Lich));
	eskills.push_back(new PvESkill(SkillID::Feast_of_Corruption));
	eskills.push_back(new PvESkill(SkillID::Life_Transfer));
	eskills.push_back(new PvESkill(SkillID::Offering_of_Blood));
	eskills.push_back(new PvESkill(SkillID::Order_of_the_Vampire));
	eskills.push_back(new PvESkill(SkillID::Soul_Leech));
	eskills.push_back(new PvESkill(SkillID::Spiteful_Spirit));
	eskills.push_back(new PvESkill(SkillID::Virulence));
	eskills.push_back(new PvESkill(SkillID::Well_of_Power));
	eskills.push_back(new PvESkill(SkillID::Wither));

	eskills.push_back(new PvESkill(SkillID::Amity));
	eskills.push_back(new PvESkill(SkillID::Aura_of_Faith));
	eskills.push_back(new PvESkill(SkillID::Healing_Hands));
	eskills.push_back(new PvESkill(SkillID::Life_Barrier));
	eskills.push_back(new PvESkill(SkillID::Mark_of_Protection));
	eskills.push_back(new PvESkill(SkillID::Peace_and_Harmony));
	eskills.push_back(new PvESkill(SkillID::Restore_Condition));
	eskills.push_back(new PvESkill(SkillID::Shield_of_Deflection));
	eskills.push_back(new PvESkill(SkillID::Shield_of_Judgment));
	eskills.push_back(new PvESkill(SkillID::Unyielding_Aura));

	eskills.push_back(new PvESkill(SkillID::Fevered_Dreams));
	eskills.push_back(new PvESkill(SkillID::Illusionary_Weaponry));
	eskills.push_back(new PvESkill(SkillID::Ineptitude));
	eskills.push_back(new PvESkill(SkillID::Keystone_Signet));
	eskills.push_back(new PvESkill(SkillID::Mantra_of_Recall));
	eskills.push_back(new PvESkill(SkillID::Migraine));
	eskills.push_back(new PvESkill(SkillID::Panic));
	eskills.push_back(new PvESkill(SkillID::Power_Block));
	eskills.push_back(new PvESkill(SkillID::Signet_of_Midnight));

	eskills.push_back(new PvESkill(SkillID::Ether_Prodigy));
	eskills.push_back(new PvESkill(SkillID::Ether_Renewal));
	eskills.push_back(new PvESkill(SkillID::Glimmering_Mark));
	eskills.push_back(new PvESkill(SkillID::Glyph_of_Energy));
	eskills.push_back(new PvESkill(SkillID::Glyph_of_Renewal));
	eskills.push_back(new PvESkill(SkillID::Mind_Shock));
	eskills.push_back(new PvESkill(SkillID::Mist_Form));
	eskills.push_back(new PvESkill(SkillID::Thunderclap));
	eskills.push_back(new PvESkill(SkillID::Ward_Against_Harm));
	eskills.push_back(new PvESkill(SkillID::Water_Trident));
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
    skills.push_back(new FactionsPvESkill(SkillID::Save_Yourselves_kurzick));
    skills.push_back(new FactionsPvESkill(SkillID::Save_Yourselves_luxon));

    skills.push_back(new FactionsPvESkill(SkillID::Aura_of_Holy_Might_kurzick));
    skills.push_back(new FactionsPvESkill(SkillID::Aura_of_Holy_Might_luxon));

    skills.push_back(new FactionsPvESkill(SkillID::Elemental_Lord_kurzick));
    skills.push_back(new FactionsPvESkill(SkillID::Elemental_Lord_luxon));

    skills.push_back(new FactionsPvESkill(SkillID::Ether_Nightmare_kurzick));
    skills.push_back(new FactionsPvESkill(SkillID::Ether_Nightmare_luxon));

    skills.push_back(new FactionsPvESkill(SkillID::Selfless_Spirit_kurzick));
    skills.push_back(new FactionsPvESkill(SkillID::Selfless_Spirit_luxon));

    skills.push_back(new FactionsPvESkill(SkillID::Shadow_Sanctuary_kurzick));
    skills.push_back(new FactionsPvESkill(SkillID::Shadow_Sanctuary_luxon));

    skills.push_back(new FactionsPvESkill(SkillID::Signet_of_Corruption_kurzick));
    skills.push_back(new FactionsPvESkill(SkillID::Signet_of_Corruption_luxon));

    skills.push_back(new FactionsPvESkill(SkillID::Spear_of_Fury_kurzick));
    skills.push_back(new FactionsPvESkill(SkillID::Spear_of_Fury_luxon));

    skills.push_back(new FactionsPvESkill(SkillID::Summon_Spirits_kurzick));
    skills.push_back(new FactionsPvESkill(SkillID::Summon_Spirits_luxon));

    skills.push_back(new FactionsPvESkill(SkillID::Triple_Shot_kurzick));
    skills.push_back(new FactionsPvESkill(SkillID::Triple_Shot_luxon));

	auto& eskills = elite_skills.at(Campaign::Factions);
	eskills.push_back(new PvESkill(SkillID::Coward));
	eskills.push_back(new PvESkill(SkillID::Auspicious_Parry));
	eskills.push_back(new PvESkill(SkillID::Dragon_Slash));
	eskills.push_back(new PvESkill(SkillID::Enraged_Smash));
	eskills.push_back(new PvESkill(SkillID::Forceful_Blow));
	eskills.push_back(new PvESkill(SkillID::Primal_Rage));
	eskills.push_back(new PvESkill(SkillID::Quivering_Blade));
	eskills.push_back(new PvESkill(SkillID::Shove));
	eskills.push_back(new PvESkill(SkillID::Triple_Chop));
	eskills.push_back(new PvESkill(SkillID::Whirling_Axe));

	eskills.push_back(new PvESkill(SkillID::Archers_Signet));
	eskills.push_back(new PvESkill(SkillID::Broad_Head_Arrow));
	eskills.push_back(new PvESkill(SkillID::Enraged_Lunge));
	eskills.push_back(new PvESkill(SkillID::Equinox));
	eskills.push_back(new PvESkill(SkillID::Famine));
	eskills.push_back(new PvESkill(SkillID::Glass_Arrows));
	eskills.push_back(new PvESkill(SkillID::Heal_as_One));
	eskills.push_back(new PvESkill(SkillID::Lacerate));
	eskills.push_back(new PvESkill(SkillID::Melandrus_Shot));
	eskills.push_back(new PvESkill(SkillID::Trappers_Focus));

	eskills.push_back(new PvESkill(SkillID::Animate_Flesh_Golem));
	eskills.push_back(new PvESkill(SkillID::Cultists_Fervor));
	eskills.push_back(new PvESkill(SkillID::Discord));
	eskills.push_back(new PvESkill(SkillID::Icy_Veins));
	eskills.push_back(new PvESkill(SkillID::Order_of_Apostasy));
	eskills.push_back(new PvESkill(SkillID::Soul_Bind));
	eskills.push_back(new PvESkill(SkillID::Spoil_Victor));
	eskills.push_back(new PvESkill(SkillID::Vampiric_Spirit));
	eskills.push_back(new PvESkill(SkillID::Wail_of_Doom));
	eskills.push_back(new PvESkill(SkillID::Weaken_Knees));

	eskills.push_back(new PvESkill(SkillID::Air_of_Enchantment));
	eskills.push_back(new PvESkill(SkillID::Blessed_Light));
	eskills.push_back(new PvESkill(SkillID::Boon_Signet));
	eskills.push_back(new PvESkill(SkillID::Empathic_Removal));
	eskills.push_back(new PvESkill(SkillID::Healing_Burst));
	eskills.push_back(new PvESkill(SkillID::Healing_Light));
	eskills.push_back(new PvESkill(SkillID::Life_Sheath));
	eskills.push_back(new PvESkill(SkillID::Ray_of_Judgment));
	eskills.push_back(new PvESkill(SkillID::Withdraw_Hexes));
	eskills.push_back(new PvESkill(SkillID::Word_of_Censure));

	eskills.push_back(new PvESkill(SkillID::Arcane_Languor));
	eskills.push_back(new PvESkill(SkillID::Expel_Hexes));
	eskills.push_back(new PvESkill(SkillID::Lyssas_Aura));
	eskills.push_back(new PvESkill(SkillID::Power_Leech));
	eskills.push_back(new PvESkill(SkillID::Psychic_Distraction));
	eskills.push_back(new PvESkill(SkillID::Psychic_Instability));
	eskills.push_back(new PvESkill(SkillID::Recurring_Insecurity));
	eskills.push_back(new PvESkill(SkillID::Shared_Burden));
	eskills.push_back(new PvESkill(SkillID::Shatter_Storm));
	eskills.push_back(new PvESkill(SkillID::Stolen_Speed));

	eskills.push_back(new PvESkill(SkillID::Double_Dragon));
	eskills.push_back(new PvESkill(SkillID::Energy_Boon));
	eskills.push_back(new PvESkill(SkillID::Gust));
	eskills.push_back(new PvESkill(SkillID::Mirror_of_Ice));
	eskills.push_back(new PvESkill(SkillID::Ride_the_Lightning));
	eskills.push_back(new PvESkill(SkillID::Second_Wind));
	eskills.push_back(new PvESkill(SkillID::Shatterstone));
	eskills.push_back(new PvESkill(SkillID::Shockwave));
	eskills.push_back(new PvESkill(SkillID::Star_Burst));
	eskills.push_back(new PvESkill(SkillID::Unsteady_Ground));

	eskills.push_back(new PvESkill(SkillID::Assassins_Promise));
	eskills.push_back(new PvESkill(SkillID::Aura_of_Displacement));
	eskills.push_back(new PvESkill(SkillID::Beguiling_Haze));
	eskills.push_back(new PvESkill(SkillID::Dark_Apostasy));
	eskills.push_back(new PvESkill(SkillID::Flashing_Blades));
	eskills.push_back(new PvESkill(SkillID::Locusts_Fury));
	eskills.push_back(new PvESkill(SkillID::Moebius_Strike));
	eskills.push_back(new PvESkill(SkillID::Palm_Strike));
	eskills.push_back(new PvESkill(SkillID::Seeping_Wound));
	eskills.push_back(new PvESkill(SkillID::Shadow_Form));
	eskills.push_back(new PvESkill(SkillID::Shadow_Shroud));
	eskills.push_back(new PvESkill(SkillID::Shroud_of_Silence));
	eskills.push_back(new PvESkill(SkillID::Siphon_Strength));
	eskills.push_back(new PvESkill(SkillID::Temple_Strike));
	eskills.push_back(new PvESkill(SkillID::Way_of_the_Empty_Palm));

	eskills.push_back(new PvESkill(SkillID::Attuned_Was_Songkai));
	eskills.push_back(new PvESkill(SkillID::Clamor_of_Souls));
	eskills.push_back(new PvESkill(SkillID::Consume_Soul));
	eskills.push_back(new PvESkill(SkillID::Defiant_Was_Xinrae));
	eskills.push_back(new PvESkill(SkillID::Grasping_Was_Kuurong));
	eskills.push_back(new PvESkill(SkillID::Preservation));
	eskills.push_back(new PvESkill(SkillID::Ritual_Lord));
	eskills.push_back(new PvESkill(SkillID::Signet_of_Spirits));
	eskills.push_back(new PvESkill(SkillID::Soul_Twisting));
	eskills.push_back(new PvESkill(SkillID::Spirit_Channeling));
	eskills.push_back(new PvESkill(SkillID::Spirit_Light_Weapon));
	eskills.push_back(new PvESkill(SkillID::Tranquil_Was_Tanasen));
	eskills.push_back(new PvESkill(SkillID::Vengeful_Was_Khanhei));
	eskills.push_back(new PvESkill(SkillID::Wanderlust));
	eskills.push_back(new PvESkill(SkillID::Weapon_of_Quickening));

    auto& h = heros.at(Campaign::Factions);
    h.push_back(new HeroUnlock(Miku));
    h.push_back(new HeroUnlock(ZeiRi));
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
	skills.push_back(new PvESkill(SkillID::Theres_Nothing_to_Fear));
	skills.push_back(new PvESkill(SkillID::Lightbringer_Signet));
	skills.push_back(new PvESkill(SkillID::Lightbringers_Gaze));
	skills.push_back(new PvESkill(SkillID::Critical_Agility));
	skills.push_back(new PvESkill(SkillID::Cry_of_Pain));
	skills.push_back(new PvESkill(SkillID::Eternal_Aura));
	skills.push_back(new PvESkill(SkillID::Intensity));
	skills.push_back(new PvESkill(SkillID::Necrosis));
	skills.push_back(new PvESkill(SkillID::Never_Rampage_Alone));
	skills.push_back(new PvESkill(SkillID::Seed_of_Life));
	skills.push_back(new PvESkill(SkillID::Sunspear_Rebirth_Signet));
	skills.push_back(new PvESkill(SkillID::Vampirism));
	skills.push_back(new PvESkill(SkillID::Whirlwind_Attack));

	auto& eskills = elite_skills.at(Campaign::Nightfall);
	eskills.push_back(new PvESkill(SkillID::Youre_All_Alone));
	eskills.push_back(new PvESkill(SkillID::Charging_Strike));
	eskills.push_back(new PvESkill(SkillID::Crippling_Slash));
	eskills.push_back(new PvESkill(SkillID::Decapitate));
	eskills.push_back(new PvESkill(SkillID::Headbutt));
	eskills.push_back(new PvESkill(SkillID::Magehunter_Strike));
	eskills.push_back(new PvESkill(SkillID::Magehunters_Smash));
	eskills.push_back(new PvESkill(SkillID::Rage_of_the_Ntouka));
	eskills.push_back(new PvESkill(SkillID::Soldiers_Stance));
	eskills.push_back(new PvESkill(SkillID::Steady_Stance));

	eskills.push_back(new PvESkill(SkillID::Burning_Arrow));
	eskills.push_back(new PvESkill(SkillID::Experts_Dexterity));
	eskills.push_back(new PvESkill(SkillID::Infuriating_Heat));
	eskills.push_back(new PvESkill(SkillID::Magebane_Shot));
	eskills.push_back(new PvESkill(SkillID::Prepared_Shot));
	eskills.push_back(new PvESkill(SkillID::Quicksand));
	eskills.push_back(new PvESkill(SkillID::Rampage_as_One));
	eskills.push_back(new PvESkill(SkillID::Scavengers_Focus));
	eskills.push_back(new PvESkill(SkillID::Smoke_Trap));
	eskills.push_back(new PvESkill(SkillID::Strike_as_One));

	eskills.push_back(new PvESkill(SkillID::Contagion));
	eskills.push_back(new PvESkill(SkillID::Corrupt_Enchantment));
	eskills.push_back(new PvESkill(SkillID::Depravity));
	eskills.push_back(new PvESkill(SkillID::Jagged_Bones));
	eskills.push_back(new PvESkill(SkillID::Order_of_Undeath));
	eskills.push_back(new PvESkill(SkillID::Pain_of_Disenchantment));
	eskills.push_back(new PvESkill(SkillID::Ravenous_Gaze));
	eskills.push_back(new PvESkill(SkillID::Reapers_Mark));
	eskills.push_back(new PvESkill(SkillID::Signet_of_Suffering));
	eskills.push_back(new PvESkill(SkillID::Toxic_Chill));

	eskills.push_back(new PvESkill(SkillID::Balthazars_Pendulum));
	eskills.push_back(new PvESkill(SkillID::Defenders_Zeal));
	eskills.push_back(new PvESkill(SkillID::Divert_Hexes));
	eskills.push_back(new PvESkill(SkillID::Glimmer_of_Light));
	eskills.push_back(new PvESkill(SkillID::Healers_Boon));
	eskills.push_back(new PvESkill(SkillID::Healers_Covenant));
	eskills.push_back(new PvESkill(SkillID::Light_of_Deliverance));
	eskills.push_back(new PvESkill(SkillID::Scribes_Insight));
	eskills.push_back(new PvESkill(SkillID::Signet_of_Removal));
	eskills.push_back(new PvESkill(SkillID::Zealous_Benediction));

	eskills.push_back(new PvESkill(SkillID::Air_of_Disenchantment));
	eskills.push_back(new PvESkill(SkillID::Enchanters_Conundrum));
	eskills.push_back(new PvESkill(SkillID::Extend_Conditions));
	eskills.push_back(new PvESkill(SkillID::Hex_Eater_Vortex));
	eskills.push_back(new PvESkill(SkillID::Power_Flux));
	eskills.push_back(new PvESkill(SkillID::Signet_of_Illusions));
	eskills.push_back(new PvESkill(SkillID::Simple_Thievery));
	eskills.push_back(new PvESkill(SkillID::Symbols_of_Inspiration));
	eskills.push_back(new PvESkill(SkillID::Tease));
	eskills.push_back(new PvESkill(SkillID::Visions_of_Regret));

	eskills.push_back(new PvESkill(SkillID::Blinding_Surge));
	eskills.push_back(new PvESkill(SkillID::Ether_Prism));
	eskills.push_back(new PvESkill(SkillID::Icy_Shackles));
	eskills.push_back(new PvESkill(SkillID::Invoke_Lightning));
	eskills.push_back(new PvESkill(SkillID::Master_of_Magic));
	eskills.push_back(new PvESkill(SkillID::Mind_Blast));
	eskills.push_back(new PvESkill(SkillID::Sandstorm));
	eskills.push_back(new PvESkill(SkillID::Savannah_Heat));
	eskills.push_back(new PvESkill(SkillID::Searing_Flames));
	eskills.push_back(new PvESkill(SkillID::Stone_Sheath));

	eskills.push_back(new PvESkill(SkillID::Assault_Enchantments));
	eskills.push_back(new PvESkill(SkillID::Foxs_Promise));
	eskills.push_back(new PvESkill(SkillID::Golden_Skull_Strike));
	eskills.push_back(new PvESkill(SkillID::Hidden_Caltrops));
	eskills.push_back(new PvESkill(SkillID::Mark_of_Insecurity));
	eskills.push_back(new PvESkill(SkillID::Shadow_Meld));
	eskills.push_back(new PvESkill(SkillID::Shadow_Prison));
	eskills.push_back(new PvESkill(SkillID::Shattering_Assault));
	eskills.push_back(new PvESkill(SkillID::Wastrels_Collapse));
	eskills.push_back(new PvESkill(SkillID::Way_of_the_Assassin));

	eskills.push_back(new PvESkill(SkillID::Caretakers_Charge));
	eskills.push_back(new PvESkill(SkillID::Destructive_Was_Glaive));
	eskills.push_back(new PvESkill(SkillID::Offering_of_Spirit));
	eskills.push_back(new PvESkill(SkillID::Reclaim_Essence));
	eskills.push_back(new PvESkill(SkillID::Signet_of_Ghostly_Might));
	eskills.push_back(new PvESkill(SkillID::Spirits_Strength));
	eskills.push_back(new PvESkill(SkillID::Weapon_of_Fury));
	eskills.push_back(new PvESkill(SkillID::Weapon_of_Remedy));
	eskills.push_back(new PvESkill(SkillID::Wielders_Zeal));
	eskills.push_back(new PvESkill(SkillID::Xinraes_Weapon));

	eskills.push_back(new PvESkill(SkillID::Arcane_Zeal));
	eskills.push_back(new PvESkill(SkillID::Avatar_of_Balthazar));
	eskills.push_back(new PvESkill(SkillID::Avatar_of_Dwayna));
	eskills.push_back(new PvESkill(SkillID::Avatar_of_Grenth));
	eskills.push_back(new PvESkill(SkillID::Avatar_of_Lyssa));
	eskills.push_back(new PvESkill(SkillID::Avatar_of_Melandru ));
	eskills.push_back(new PvESkill(SkillID::Ebon_Dust_Aura));
	eskills.push_back(new PvESkill(SkillID::Grenths_Grasp));
	eskills.push_back(new PvESkill(SkillID::Onslaught));
	eskills.push_back(new PvESkill(SkillID::Pious_Renewal));
	eskills.push_back(new PvESkill(SkillID::Reapers_Sweep));
	eskills.push_back(new PvESkill(SkillID::Vow_of_Silence));
	eskills.push_back(new PvESkill(SkillID::Vow_of_Strength));
	eskills.push_back(new PvESkill(SkillID::Wounding_Strike));
	eskills.push_back(new PvESkill(SkillID::Zealous_Vow));

	eskills.push_back(new PvESkill(SkillID::Incoming));
	eskills.push_back(new PvESkill(SkillID::Its_Just_a_Flesh_Wound));
	eskills.push_back(new PvESkill(SkillID::The_Power_Is_Yours));
	eskills.push_back(new PvESkill(SkillID::Angelic_Bond));
	eskills.push_back(new PvESkill(SkillID::Anthem_of_Fury));
	eskills.push_back(new PvESkill(SkillID::Anthem_of_Guidance));
	eskills.push_back(new PvESkill(SkillID::Cautery_Signet));
	eskills.push_back(new PvESkill(SkillID::Crippling_Anthem));
	eskills.push_back(new PvESkill(SkillID::Cruel_Spear));
	eskills.push_back(new PvESkill(SkillID::Defensive_Anthem));
	eskills.push_back(new PvESkill(SkillID::Focused_Anger));
	eskills.push_back(new PvESkill(SkillID::Soldiers_Fury));
	eskills.push_back(new PvESkill(SkillID::Song_of_Purification));
	eskills.push_back(new PvESkill(SkillID::Song_of_Restoration));
	eskills.push_back(new PvESkill(SkillID::Stunning_Strike));

    auto& h = heros.at(Campaign::Nightfall);
    h.push_back(new HeroUnlock(AcolyteJin));
    h.push_back(new HeroUnlock(AcolyteSousuke));
    h.push_back(new HeroUnlock(Dunkoro));
    h.push_back(new HeroUnlock(GeneralMorgahn));
    h.push_back(new HeroUnlock(Goren));
    h.push_back(new HeroUnlock(Koss));
    h.push_back(new HeroUnlock(MargridTheSly));
    h.push_back(new HeroUnlock(MasterOfWhispers));
    h.push_back(new HeroUnlock(Melonni));
    h.push_back(new HeroUnlock(MOX));
    h.push_back(new HeroUnlock(Norgu));
    h.push_back(new HeroUnlock(Olias));
    h.push_back(new HeroUnlock(Razah));
    h.push_back(new HeroUnlock(Tahlkora));
    h.push_back(new HeroUnlock(Zenmai));
    h.push_back(new HeroUnlock(ZhedShadowhoof));
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
	skills.push_back(new PvESkill(SkillID::Air_of_Superiority));
	skills.push_back(new PvESkill(SkillID::Asuran_Scan));
	skills.push_back(new PvESkill(SkillID::Mental_Block));
	skills.push_back(new PvESkill(SkillID::Mindbender));
	skills.push_back(new PvESkill(SkillID::Pain_Inverter));
	skills.push_back(new PvESkill(SkillID::Radiation_Field));
	skills.push_back(new PvESkill(SkillID::Smooth_Criminal));
	skills.push_back(new PvESkill(SkillID::Summon_Ice_Imp));
	skills.push_back(new PvESkill(SkillID::Summon_Mursaat));
	skills.push_back(new PvESkill(SkillID::Summon_Naga_Shaman));
	skills.push_back(new PvESkill(SkillID::Summon_Ruby_Djinn));
	skills.push_back(new PvESkill(SkillID::Technobabble));

	skills.push_back(new PvESkill(SkillID::By_Urals_Hammer));
	skills.push_back(new PvESkill(SkillID::Dont_Trip));
	skills.push_back(new PvESkill(SkillID::Alkars_Alchemical_Acid));
	skills.push_back(new PvESkill(SkillID::Black_Powder_Mine));
	skills.push_back(new PvESkill(SkillID::Brawling_Headbutt ));
	skills.push_back(new PvESkill(SkillID::Breath_of_the_Great_Dwarf));
	skills.push_back(new PvESkill(SkillID::Drunken_Master));
	skills.push_back(new PvESkill(SkillID::Dwarven_Stability));
	skills.push_back(new PvESkill(SkillID::Ear_Bite));
	skills.push_back(new PvESkill(SkillID::Great_Dwarf_Armor));
	skills.push_back(new PvESkill(SkillID::Great_Dwarf_Weapon));
	skills.push_back(new PvESkill(SkillID::Light_of_Deldrimor));
	skills.push_back(new PvESkill(SkillID::Low_Blow));
	skills.push_back(new PvESkill(SkillID::Snow_Storm));

	skills.push_back(new PvESkill(SkillID::Deft_Strike));
	skills.push_back(new PvESkill(SkillID::Ebon_Battle_Standard_of_Courage));
	skills.push_back(new PvESkill(SkillID::Ebon_Battle_Standard_of_Honor));
	skills.push_back(new PvESkill(SkillID::Ebon_Battle_Standard_of_Wisdom));
	skills.push_back(new PvESkill(SkillID::Ebon_Escape));
	skills.push_back(new PvESkill(SkillID::Ebon_Vanguard_Assassin_Support));
	skills.push_back(new PvESkill(SkillID::Ebon_Vanguard_Sniper_Support));
	skills.push_back(new PvESkill(SkillID::Signet_of_Infection));
	skills.push_back(new PvESkill(SkillID::Sneak_Attack));
	skills.push_back(new PvESkill(SkillID::Tryptophan_Signet));
	skills.push_back(new PvESkill(SkillID::Weakness_Trap));
	skills.push_back(new PvESkill(SkillID::Winds));

	skills.push_back(new PvESkill(SkillID::Dodge_This));
	skills.push_back(new PvESkill(SkillID::Finish_Him));
	skills.push_back(new PvESkill(SkillID::I_Am_Unstoppable));
	skills.push_back(new PvESkill(SkillID::I_Am_the_Strongest));
	skills.push_back(new PvESkill(SkillID::You_Are_All_Weaklings));
	skills.push_back(new PvESkill(SkillID::You_Move_Like_a_Dwarf));
	skills.push_back(new PvESkill(SkillID::A_Touch_of_Guile));
	skills.push_back(new PvESkill(SkillID::Club_of_a_Thousand_Bears));
	skills.push_back(new PvESkill(SkillID::Feel_No_Pain));
	skills.push_back(new PvESkill(SkillID::Raven_Blessing));
	skills.push_back(new PvESkill(SkillID::Ursan_Blessing));
	skills.push_back(new PvESkill(SkillID::Volfen_Blessing));

    auto& h = heros.at(Campaign::EyeOfTheNorth);
    h.push_back(new HeroUnlock(Anton));
    h.push_back(new HeroUnlock(Gwen));
    h.push_back(new HeroUnlock(Hayda));
    h.push_back(new HeroUnlock(Jora));
    h.push_back(new HeroUnlock(Kahmu));
    h.push_back(new HeroUnlock(KeiranThackeray));
    h.push_back(new HeroUnlock(Livia));
    h.push_back(new HeroUnlock(Ogden));
    h.push_back(new HeroUnlock(PyreFierceshot));
    h.push_back(new HeroUnlock(Vekk));
    h.push_back(new HeroUnlock(Xandra));

}
void CompletionWindow::Initialize_Dungeons()
{
    LoadTextures(Dungeon::normal_mode_images);
    LoadTextures(Dungeon::hard_mode_images);
    auto& dungeons = missions.at(Campaign::BonusMissionPack);
    dungeons.push_back(new Dungeon(
        MapID::Catacombs_of_Kathandrax_Level_1, QuestID::ZaishenBounty_Ilsundur_Lord_of_Fire));
    dungeons.push_back(new Dungeon(
        MapID::Rragars_Menagerie_Level_1, QuestID::ZaishenBounty_Rragar_Maneater));
    dungeons.push_back(new Dungeon(
        MapID::Cathedral_of_Flames_Level_1, QuestID::ZaishenBounty_Murakai_Lady_of_the_Night));
    dungeons.push_back(new Dungeon(
        MapID::Ooze_Pit_mission));
    dungeons.push_back(new Dungeon(
        MapID::Darkrime_Delves_Level_1));
dungeons.push_back(new Dungeon(
    MapID::Frostmaws_Burrows_Level_1));
dungeons.push_back(new Dungeon(
    MapID::Sepulchre_of_Dragrimmar_Level_1, QuestID::ZaishenBounty_Remnant_of_Antiquities));
dungeons.push_back(new Dungeon(
    MapID::Ravens_Point_Level_1, QuestID::ZaishenBounty_Plague_of_Destruction));
dungeons.push_back(new Dungeon(
    MapID::Vloxen_Excavations_Level_1, QuestID::ZaishenBounty_Zoldark_the_Unholy));
dungeons.push_back(new Dungeon(
    MapID::Bogroot_Growths_Level_1));
dungeons.push_back(new Dungeon(
    MapID::Bloodstone_Caves_Level_1));
dungeons.push_back(new Dungeon(
    MapID::Shards_of_Orr_Level_1, QuestID::ZaishenBounty_Fendi_Nin));
dungeons.push_back(new Dungeon(
    MapID::Oolas_Lab_Level_1, QuestID::ZaishenBounty_TPS_Regulator_Golem));
dungeons.push_back(new Dungeon(
    MapID::Arachnis_Haunt_Level_1, QuestID::ZaishenBounty_Arachni));
dungeons.push_back(new Dungeon(
    MapID::Slavers_Exile_Level_1, {
        QuestID::ZaishenBounty_Forgewight,
        QuestID::ZaishenBounty_Selvetarm,
        QuestID::ZaishenBounty_Justiciar_Thommis,
        QuestID::ZaishenBounty_Rand_Stormweaver,
        QuestID::ZaishenBounty_Duncan_the_Black }));
dungeons.push_back(new Dungeon(
    MapID::Fronis_Irontoes_Lair_mission, {QuestID::ZaishenBounty_Fronis_Irontoe }));
dungeons.push_back(new Dungeon(
    MapID::Secret_Lair_of_the_Snowmen));
dungeons.push_back(new Dungeon(
    MapID::Heart_of_the_Shiverpeaks_Level_1, {QuestID::ZaishenBounty_Magmus }));
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
    for (auto c : minipets)
        delete c;
    minipets.clear();

    for (const auto& camp : character_completion)
        delete camp.second;
    character_completion.clear();
}
void CompletionWindow::Draw(IDirect3DDevice9* device)
{
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
        const auto email = GetAccountEmail();
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 2.f, 8.f });
        bool is_selected = false;
        for (auto& it : character_completion) {
            is_selected = false;
            if (!sel && chosen_player_name == it.first) {
                is_selected = true;
                sel = &it.first;
            }
            if (!is_selected && only_show_account_chars && it.second->account != email) {
                continue; // Different account
            }

			if (it.second->name_str.size() > 0 && ImGui::Selectable(it.second->name_str.c_str(), is_selected)) {
				chosen_player_name = it.first;
				chosen_player_name_s = it.second->name_str;
				CheckProgress(true);
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
    ImGui::SameLine();
    if (ImGui::Checkbox("This Account", &only_show_account_chars)) {
        RefreshAccountCharacters();
    }
    ImGui::ShowHelp("Limits the character dropdown to only show the characters belonging to this account.");
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
    auto draw_missions = [missions_per_row, device](auto& camp_missions, size_t end = 0) {
        if (end == 0) {
            end = camp_missions.size();
        }
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
        ImGui::Columns(static_cast<int>(missions_per_row), "###completion_section_cols", false);
        size_t items_per_col = (size_t)ceil(end / static_cast<float>(missions_per_row));
        size_t col_count = 0;
        for (size_t i = 0; i < end; i++) {
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

    ImGui::Text("Festival Hats");
    ImGui::ShowHelp("To update this list, talk to a Festival Hat Keeper and select \"Please make me a new hat.\"");
    ImGui::SameLine(checkbox_offset);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {0, 0});
    ImGui::Checkbox("Hide collected hats", &hide_collected_hats);
    ImGui::PopStyleVar();

    size_t completed = 0;
    size_t offset = 0;
    size_t to_index = 0;
    const size_t cnt = festival_hats.size();
    char label[128];
    std::vector<Missions::FestivalHat*> filtered;
    // Halloween hats
    completed = 0;
    to_index = wintersday_index;
    filtered.clear();
    for (size_t i = offset; i < to_index && i < cnt; i++) {
        const auto m = festival_hats[i];
        if (m->is_completed) {
            completed++;
            if (hide_collected_hats)
                continue;
        }
        filtered.push_back(m);
    }
    snprintf(label, _countof(label), "Halloween Hats (%d of %d collected) - %.0f%%###halloween_hats", completed, (to_index - offset), ((float)completed / (float)(to_index - offset)) * 100.f);
    if (ImGui::CollapsingHeader(label)) {
        draw_missions(filtered);
    }
    offset = to_index;

    // Wintersday hats
    filtered.clear();
    completed = 0;
    to_index = dragon_festival_index;
    for (size_t i = offset; i < to_index && i < cnt; i++) {
        const auto m = festival_hats[i];
        if (m->is_completed) {
            completed++;
            if (hide_collected_hats)
                continue;
        }
        filtered.push_back(m);
    }
    snprintf(label, _countof(label), "Wintersday Hats (%d of %d collected) - %.0f%%###wintersday_hats", completed, (to_index - offset), ((float)completed / (float)(to_index - offset)) * 100.f);
    if (ImGui::CollapsingHeader(label)) {
        draw_missions(filtered);
    }
    offset = to_index;

    // Dragon festival hats
    filtered.clear();
    completed = 0;
    to_index = festival_hats.size();
    for (size_t i = offset; i < to_index && i < cnt; i++) {
        const auto m = festival_hats[i];
        if (m->is_completed) {
            completed++;
            if (hide_collected_hats)
                continue;
        }
        filtered.push_back(m);
    }
    snprintf(label, _countof(label), "Dragon Festival Hats (%d of %d collected) - %.0f%%###dragon_festival_hats", completed, (to_index - offset), ((float)completed / (float)(to_index - offset)) * 100.f);
    if (ImGui::CollapsingHeader(label)) {
        draw_missions(filtered);
    }
    offset = to_index;

    DrawHallOfMonuments(device);
    ImGui::EndChild();
    ImGui::End();
}
void CompletionWindow::DrawHallOfMonuments(IDirect3DDevice9* device) {
	float single_item_width = Mission::icon_size.x;
	if (show_as_list)
		single_item_width *= 5.f;
	int missions_per_row = (int)std::floor(ImGui::GetContentRegionAvail().x / (ImGui::GetIO().FontGlobalScale * single_item_width + (ImGui::GetStyle().ItemSpacing.x)));
	const float checkbox_offset = ImGui::GetContentRegionAvail().x - 200.f * ImGui::GetIO().FontGlobalScale;
	ImGui::Text("Hall of Monuments");
	ImGui::SameLine(checkbox_offset);
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 0,0 });
	ImGui::Checkbox("Hide unlocked achievements", &hide_unlocked_achievements);
	ImGui::PopStyleVar();
    const auto hom = GetCharacterHom(chosen_player_name);
	// Devotion
	uint32_t completed = 0;
	if (hom && hom->isReady()) {
		for (size_t i = 0; i < _countof(hom->devotion_points); i++) {
			completed += hom->devotion_points[i];
		}
	}
	uint32_t dedicated = 0;
	uint32_t drawn = 0;
	for (auto m : minipets) {
		if (m->is_completed) {
			dedicated++;
			if (hide_unlocked_achievements)
				continue;
		}
		drawn++;
	}

    char label[128];
    snprintf(label, _countof(label), "%s (%d of %d points gained, %d of %d minipets dedicated) - %.0f%%###devotion_points", "Devotion",
        completed, DevotionPoints::TotalAvailable,
        dedicated, minipets.size(),
        (float)dedicated / (float)minipets.size() * 100.f);

	if (ImGui::CollapsingHeader(label)) {
		ImGui::TextDisabled(R"(To update this list, talk to the "Devotion" pedestal in Eye of the North,
then press "Examine the Monument to Devotion.")");
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
		ImGui::Columns(missions_per_row, "###completion_section_cols", false);
		size_t items_per_col = (size_t)ceil(drawn / static_cast<float>(missions_per_row));
		size_t col_count = 0;

        if (!minipets_sorted) {
            bool ready = true;
            for (const auto m : minipets) {
                if (!m->Name()[0]) {
                    ready = false;
                    break;
                }
            }
            if (ready) {
                std::sort(minipets.begin(), minipets.end(), [](MinipetAchievement* a, MinipetAchievement* b) { return strcmp(a->Name(), b->Name()) < 0;  });
                minipets_sorted = true;
            }
        }

		for (const auto m : minipets) {
			if (m->is_completed && hide_unlocked_achievements)
				continue;
			if (!m->Draw(device))
				continue;
			col_count++;
			if (col_count == items_per_col) {
				ImGui::NextColumn();
				col_count = 0;
			}
		}
		ImGui::Columns(1);
		ImGui::PopStyleVar();
	}
	// Valor
	completed = 0;
	if (hom && hom->isReady()) {
		for (size_t i = 0; i < _countof(hom->valor_points); i++) {
			completed += hom->valor_points[i];
		}
	}
	dedicated = 0;
	drawn = 0;
	for (auto m : hom_weapons) {
		if (m->is_completed) {
			dedicated++;
			if (hide_unlocked_achievements)
				continue;
		}
		drawn++;
	}
	snprintf(label, _countof(label), "%s (%d of %d points gained, %d of %d weapons displayed) - %.0f%%###valor_points", "Valor",
		completed, ValorPoints::TotalAvailable,
		dedicated, hom_weapons.size(),
		((float)dedicated / (float)hom_weapons.size()) * 100.f);

	if (ImGui::CollapsingHeader(label)) {

		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
		ImGui::Columns(static_cast<int>(missions_per_row), "###completion_section_cols", false);
		size_t items_per_col = (size_t)ceil(drawn / static_cast<float>(missions_per_row));
		size_t col_count = 0;

        for (auto m : hom_weapons) {
            if (m->is_completed && hide_unlocked_achievements)
                continue;
            if (!m->Draw(device))
                continue;
            col_count++;
            if (col_count == items_per_col) {
                ImGui::NextColumn();
                col_count = 0;
            }
        }
        ImGui::Columns(1);
        ImGui::PopStyleVar();
    }

	// Resilience
	completed = 0;
    if (hom && hom->isReady()) {
        for (size_t i = 0; i < _countof(hom->resilience_points); i++) {
            completed += hom->resilience_points[i];
        }
    }
	dedicated = 0;
	drawn = 0;
	for (auto m : hom_armor) {
		if (m->is_completed) {
			dedicated++;
			if (hide_unlocked_achievements)
				continue;
		}
		drawn++;
	}
	snprintf(label, _countof(label), "%s (%d of %d points gained, %d of %d armor sets displayed) - %.0f%%###resilience_points", "Resilience",
		completed, ResiliencePoints::TotalAvailable,
		dedicated, hom_armor.size(),
		((float)dedicated / (float)hom_armor.size()) * 100.f);

    if (ImGui::CollapsingHeader(label)) {
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
        ImGui::Columns(missions_per_row, "###completion_section_cols", false);
        auto items_per_col = static_cast<size_t>(ceil(drawn / static_cast<float>(missions_per_row)));
        size_t col_count = 0;

		for (auto m : hom_armor) {
			if (m->is_completed && hide_unlocked_achievements)
				continue;
			if (!m->Draw(device))
				continue;
			col_count++;
			if (col_count == items_per_col) {
				ImGui::NextColumn();
				col_count = 0;
			}
		}
		ImGui::Columns(1);
		ImGui::PopStyleVar();
	}

	// Fellowship
	completed = 0;
    if (hom && hom->isReady()) {
        for (size_t i = 0; i < _countof(hom->fellowship_points); i++) {
            completed += hom->fellowship_points[i];
        }
    }
	dedicated = 0;
	drawn = 0;
	for (auto m : hom_companions) {
		if (m->is_completed) {
			dedicated++;
			if (hide_unlocked_achievements)
				continue;
		}
		drawn++;
	}
	snprintf(label, _countof(label), "%s (%d of %d points gained, %d of %d companions displayed) - %.0f%%###fellowship_points", "Fellowship",
		completed, FellowshipPoints::TotalAvailable,
		dedicated, hom_companions.size(),
		((float)dedicated / (float)hom_companions.size()) * 100.f);

	if (ImGui::CollapsingHeader(label)) {
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
		ImGui::Columns(static_cast<int>(missions_per_row), "###completion_section_cols", false);
		size_t items_per_col = (size_t)ceil(drawn / static_cast<float>(missions_per_row));
		size_t col_count = 0;

		for (auto m : hom_companions) {
			if (m->is_completed && hide_unlocked_achievements)
				continue;
			if (!m->Draw(device))
				continue;
			col_count++;
			if (col_count == items_per_col) {
				ImGui::NextColumn();
				col_count = 0;
			}
		}
		ImGui::Columns(1);
		ImGui::PopStyleVar();
	}

	// Honor
	completed = 0;
    if (hom && hom->isReady()) {
        for (size_t i = 0; i < _countof(hom->honor_points); i++) {
            completed += hom->honor_points[i];
        }
    }
	dedicated = 0;
	drawn = 0;
	for (auto m : hom_titles) {
		if (m->is_completed) {
			dedicated++;
			if (hide_unlocked_achievements)
				continue;
		}
		drawn++;
	}
	snprintf(label, _countof(label), "%s (%d of %d points gained, %d of %d titles achieved) - %.0f%%###honor_points", "Honor",
		completed, HonorPoints::TotalAvailable,
		dedicated, hom_titles.size(),
		((float)dedicated / (float)hom_titles.size()) * 100.f);

	if (ImGui::CollapsingHeader(label)) {
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
		ImGui::Columns(static_cast<int>(missions_per_row), "###completion_section_cols", false);
		size_t items_per_col = (size_t)ceil(drawn / static_cast<float>(missions_per_row));
		size_t col_count = 0;

		for (auto m : hom_titles) {
			if (m->is_completed && hide_unlocked_achievements)
				continue;
			if (!m->Draw(device))
				continue;
			col_count++;
			if (col_count == items_per_col) {
				ImGui::NextColumn();
				col_count = 0;
			}
		}
		ImGui::Columns(1);
		ImGui::PopStyleVar();
	}
}
void CompletionWindow::DrawSettingInternal()
{
    ToolboxWindow::DrawSettingInternal();
}

void CompletionWindow::LoadSettings(ToolboxIni* ini)
{
    ToolboxWindow::LoadSettings(ini);
    ToolboxIni* completion_ini = new ToolboxIni(false, false, false);
    completion_ini->LoadFile(Resources::GetPath(completion_ini_filename).c_str());
    std::string ini_str;
    std::wstring name_ws;
    const char* ini_section;

    LOAD_BOOL(show_as_list);
    LOAD_BOOL(hide_unlocked_skills);
    LOAD_BOOL(hide_completed_vanquishes);
    LOAD_BOOL(hide_completed_missions);
    LOAD_BOOL(hide_unlocked_achievements);
    LOAD_BOOL(hide_collected_hats);
    LOAD_BOOL(only_show_account_chars);

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

    ToolboxIni::TNamesDepend entries;
    completion_ini->GetAllSections(entries);
    for (ToolboxIni::Entry& entry : entries) {
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
        read_ini_to_buf(CompletionType::MinipetsUnlocked, "minipets_unlocked");
        read_ini_to_buf(CompletionType::FestivalHats, "festival_hats");

        auto c = GetCharacterCompletion(name_ws.data(),true);
        c->profession = (Profession)completion_ini->GetLongValue(ini_section, "profession", 0);
        c->account = GuiUtils::StringToWString(completion_ini->GetValue(ini_section, "account", ""));
    }
    CheckProgress();
}
CompletionWindow* CompletionWindow::CheckProgress(bool fetch_hom) {
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
    for (auto achievement : festival_hats) {
        achievement->CheckProgress(chosen_player_name);
    }
	for (auto achievement : minipets) {
		achievement->CheckProgress(chosen_player_name);
	}
	for (auto achievement : hom_weapons) {
		achievement->CheckProgress(chosen_player_name);
	}
    for (auto achievement : hom_armor)
    {
        achievement->CheckProgress(chosen_player_name);
    }
	for (auto achievement : hom_companions)
	{
		achievement->CheckProgress(chosen_player_name);
	}
	for (auto achievement : hom_titles)
	{
		achievement->CheckProgress(chosen_player_name);
	}
    if (fetch_hom)
    {
        const auto cc = GetCharacterCompletion(chosen_player_name.c_str(), true);
        FetchHom(&cc->hom_achievements);
	}
	return this;
}
void CompletionWindow::SaveSettings(ToolboxIni* ini)
{
    ToolboxWindow::SaveSettings(ini);
    ToolboxIni* completion_ini = new ToolboxIni(false, false, false);
    std::string ini_str;
    std::string* name;
    CharacterCompletion* char_comp = nullptr;

    SAVE_BOOL(show_as_list);
    SAVE_BOOL(hide_unlocked_skills);
    SAVE_BOOL(hide_completed_vanquishes);
    SAVE_BOOL(hide_completed_missions);
    SAVE_BOOL(hide_unlocked_achievements);
    SAVE_BOOL(hide_collected_hats);
    SAVE_BOOL(only_show_account_chars);

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
        completion_ini->SetValue(name->c_str(), "account", GuiUtils::WStringToString(char_comp->account).c_str());
        write_buf_to_ini("mission", &char_comp->mission, ini_str, name);
        write_buf_to_ini("mission_bonus", &char_comp->mission_bonus, ini_str, name);
        write_buf_to_ini("mission_hm", &char_comp->mission_hm, ini_str, name);
        write_buf_to_ini("mission_bonus_hm", &char_comp->mission_bonus_hm, ini_str, name);
        write_buf_to_ini("skills", &char_comp->skills, ini_str, name);
        write_buf_to_ini("vanquishes", &char_comp->vanquishes, ini_str, name);
        write_buf_to_ini("heros", &char_comp->heroes, ini_str, name);
        write_buf_to_ini("maps_unlocked", &char_comp->maps_unlocked, ini_str, name);
        write_buf_to_ini("minipets_unlocked", &char_comp->minipets_unlocked, ini_str, name);
        write_buf_to_ini("festival_hats", &char_comp->festival_hats, ini_str, name);

        completion_ini->SetValue(name->c_str(), "hom_code", char_comp->hom_code.c_str());
    }
    completion_ini->SaveFile(Resources::GetPath(completion_ini_filename).c_str());
    delete completion_ini;
}

CharacterCompletion* CompletionWindow::GetCharacterCompletion(const wchar_t* character_name, bool create_if_not_found) {
    if (character_completion.contains(character_name)) {
        return character_completion.at(character_name);
    }
    CharacterCompletion* this_character_completion = nullptr;
	if (create_if_not_found) {
        this_character_completion = new CharacterCompletion();
		this_character_completion->name_str = GuiUtils::WStringToString(character_name);
        this_character_completion->hom_achievements.character_name = character_name;
		character_completion[character_name] = this_character_completion;
        FetchHom(&this_character_completion->hom_achievements);
	}
	return this_character_completion;
}


void MinipetAchievement::CheckProgress(const std::wstring& player_name)
{
    is_completed = false;
    auto& cc = character_completion;
    if (!cc.contains(player_name))
        return;
    std::vector<uint32_t>& minipets_unlocked = cc.at(player_name)->minipets_unlocked;
    is_completed = bonus = ArrayBoolAt(minipets_unlocked, encoded_name_index);
}
void WeaponAchievement::CheckProgress(const std::wstring& player_name)
{
	is_completed = false;
    const auto& cc = character_completion;
	if (!cc.contains(player_name))
		return;
	const auto& hom = cc.at(player_name)->hom_achievements;
	if (hom.state != HallOfMonumentsAchievements::State::Done)
		return;
	auto& unlocked = hom.valor_detail;
	is_completed = bonus = unlocked[encoded_name_index] != 0;
}

IDirect3DTexture9* AchieventWithWikiFile::GetMissionImage()
{
	if (!img && !wiki_file_name.empty()) {
		img = Resources::GetGuildWarsWikiImage(wiki_file_name.c_str(),64);
	}
	return img ? *img : nullptr;
}
void ArmorAchievement::CheckProgress(const std::wstring& player_name)
{
	is_completed = false;
    const auto& cc = character_completion;
    if (!cc.contains(player_name))
        return;
    const auto& hom = cc.at(player_name)->hom_achievements;
    if (hom.state != HallOfMonumentsAchievements::State::Done) return;
    const auto& unlocked = hom.resilience_detail;
	is_completed = bonus = unlocked[encoded_name_index] != 0;
}
void CompanionAchievement::CheckProgress(const std::wstring& player_name)
{
	is_completed = false;
    const auto& cc = character_completion;
	if (!cc.contains(player_name))
		return;
	const auto& hom = cc.at(player_name)->hom_achievements;
	if (hom.state != HallOfMonumentsAchievements::State::Done) return;
	const auto& unlocked = hom.fellowship_detail;
	is_completed = bonus = unlocked[encoded_name_index] != 0;
}
void HonorAchievement::CheckProgress(const std::wstring& player_name)
{
	is_completed = false;
    const auto& cc = character_completion;
	if (!cc.contains(player_name))
		return;
    const auto& hom = cc.at(player_name)->hom_achievements;
	if (hom.state != HallOfMonumentsAchievements::State::Done) return;
	const auto& unlocked = hom.honor_detail;
	is_completed = bonus = unlocked[encoded_name_index] != 0;
}

void Missions::FestivalHat::CheckProgress(const std::wstring& player_name)
{
    is_completed = false;
    auto& cc = character_completion;
    if (!cc.contains(player_name))
        return;
    std::vector<uint32_t>& unlocked = cc.at(player_name)->festival_hats;
    is_completed = bonus = ArrayBoolAt(unlocked, encoded_name_index);
}
