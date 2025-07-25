#include "stdafx.h"

#include <GWCA/Context/PreGameContext.h>
#include <GWCA/Context/CharContext.h>
#include <GWCA/Context/WorldContext.h>
#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/AccountContext.h>

#include <GWCA/GameEntities/Skill.h>
#include <GWCA/GameEntities/Quest.h>
#include <GWCA/GameEntities/Map.h>
#include <GWCA/GameEntities/Hero.h>
#include <GWCA/GameEntities/Item.h>

#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Managers/PlayerMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/ItemMgr.h>
#include <GWCA/Managers/QuestMgr.h>

#include <Modules/Resources.h>
#include <Modules/GwDatTextureModule.h>

#include <Windows/RerollWindow.h>
#include <Windows/CompletionWindow.h>
#include <Windows/CompletionWindow_Constants.h>
#include <Windows/TravelWindow.h>

#include <Color.h>
#include <Modules/DialogModule.h>

#include <Utils/ToolboxUtils.h>
#include <Utils/TextUtils.h>

using namespace GW::Constants;
using namespace Missions;
using namespace CompletionWindow_Constants;

namespace {
    bool ArrayBoolAt(GW::Array<uint32_t>& array, const uint32_t index)
    {
        const uint32_t real_index = index >> 5;
        if (real_index >= array.size()) {
            return false;
        }
        const uint32_t shift = static_cast<byte>(index) & 0x1f;
        const uint32_t flag = 1 << shift;
        return (array[real_index] & flag) != 0;
    }

    bool ArrayBoolAt(const std::vector<uint32_t>& array, const uint32_t index)
    {
        const uint32_t real_index = index / 32;
        if (real_index >= array.size()) {
            return false;
        }
        const uint32_t shift = index % 32;
        const uint32_t flag = 1 << shift;
        return (array[real_index] & flag) != 0;
    }

    void ArrayBoolSet(std::vector<uint32_t>& array, const uint32_t index, const bool is_set = true)
    {
        const uint32_t real_index = index / 32;
        if (real_index >= array.size()) {
            array.resize(real_index + 1, 0);
        }
        const uint32_t shift = index % 32;
        const uint32_t flag = 1u << shift;
        if (is_set) {
            array[real_index] |= flag;
        }
        else {
            array[real_index] ^= flag;
        }
    }

    const wchar_t* GetAccountEmail()
    {
        const auto c = GW::GetCharContext();
        return c && *c->player_email ? c->player_email : nullptr;
    }

    const wchar_t* GetPlayerName()
    {
        const auto c = GW::GetCharContext();
        return c && *c->player_name ? c->player_name : nullptr;
    }

    std::wstring last_player_name;

    void GetOutpostIcons(MapID map_id, IDirect3DTexture9** icons_out[4], uint8_t mission_state, bool is_hard_mode = false)
    {
        memset(icons_out, 0, sizeof(icons_out) * sizeof(*icons_out));

        WorldMapIcon icon_file_ids[4] = {WorldMapIcon::None};
        uint32_t icon_idx = 0;

        auto hardModeMissionIcons = [mission_state](WorldMapIcon icon_file_ids[4], bool has_master = true) {
            uint32_t icon_idx = 0;
            if ((mission_state & ToolboxUtils::MissionState::Primary) != 0)
                icon_file_ids[icon_idx++] = WorldMapIcon::HardMode_CompletePrimary;
            if ((mission_state & ToolboxUtils::MissionState::Expert) != 0)
                icon_file_ids[icon_idx++] = WorldMapIcon::HardMode_CompleteExpert;

            if (has_master && (mission_state & ToolboxUtils::MissionState::Master) != 0)
                icon_file_ids[icon_idx++] = WorldMapIcon::HardMode_CompleteMaster;
            if (!has_master
                && (mission_state & ToolboxUtils::MissionState::Expert) != 0
                && (mission_state & ToolboxUtils::MissionState::Primary) != 0) {
                icon_file_ids[icon_idx++] = WorldMapIcon::HardMode_CompleteAll;
            }
            else if (has_master && (mission_state & ToolboxUtils::MissionState::Master) != 0) {
                icon_file_ids[icon_idx++] = WorldMapIcon::HardMode_CompleteAll;
            }
            else {
                icon_file_ids[icon_idx++] = WorldMapIcon::HardMode;
            }
        };
        const auto area_info = GW::Map::GetMapInfo(map_id);
        if (!area_info) return;

        if (area_info->type == GW::RegionType::ExplorableZone) {
            *icon_file_ids = WorldMapIcon::HardMode;
            if ((mission_state & ToolboxUtils::MissionState::Primary) != 0)
                *icon_file_ids = WorldMapIcon::HardMode_CompleteAll;
        }
        else {
            // TODO: Change these icons out for Hard Mode
            switch (area_info->continent) {
                case GW::Continent::Kryta: {
                    switch (area_info->type) {
                        case GW::RegionType::MissionOutpost:
                        case GW::RegionType::CooperativeMission:
                            if (is_hard_mode) {
                                hardModeMissionIcons(icon_file_ids, false);
                            }
                            else {
                                if ((mission_state & ToolboxUtils::MissionState::Primary) != 0)
                                    icon_file_ids[icon_idx++] = WorldMapIcon::Kryta_CompletePrimary;
                                if ((mission_state & ToolboxUtils::MissionState::Expert) != 0)
                                    icon_file_ids[icon_idx++] = WorldMapIcon::Kryta_CompleteSecondary;
                                icon_file_ids[icon_idx++] = WorldMapIcon::Kryta_Mission;
                            }
                            break;
                        case GW::RegionType::City:
                            icon_file_ids[0] = WorldMapIcon::Kryta_City;
                            break;
                        case GW::RegionType::Outpost:
                            icon_file_ids[0] = WorldMapIcon::Kryta_Outpost;
                            break;
                        case GW::RegionType::Challenge:
                            icon_file_ids[0] = WorldMapIcon::Kryta_Outpost;
                            break;
                        case GW::RegionType::Dungeon:
                            icon_file_ids[0] = is_hard_mode ? WorldMapIcon::EyeOfTheNorth_HardMode_Dungeon : WorldMapIcon::EyeOfTheNorth_Dungeon;
                            break;
                        case GW::RegionType::EotnMission:
                            icon_file_ids[0] = is_hard_mode ? WorldMapIcon::EyeOfTheNorth_HardMode_Mission : WorldMapIcon::EyeOfTheNorth_Mission;
                            break;
                    }
                }
                break;
                case GW::Continent::Cantha: {
                    switch (area_info->type) {
                        case GW::RegionType::MissionOutpost:
                        case GW::RegionType::CooperativeMission:
                            if (is_hard_mode) {
                                hardModeMissionIcons(icon_file_ids);
                            }
                            else {
                                icon_file_ids[icon_idx++] = WorldMapIcon::Cantha_Mission;
                                if ((mission_state & ToolboxUtils::MissionState::Primary) != 0)
                                    icon_file_ids[icon_idx++] = WorldMapIcon::Cantha_CompletePrimary;
                                if ((mission_state & ToolboxUtils::MissionState::Expert) != 0)
                                    icon_file_ids[icon_idx++] = WorldMapIcon::Cantha_CompleteExpert;
                                if ((mission_state & ToolboxUtils::MissionState::Master) != 0)
                                    icon_file_ids[icon_idx++] = WorldMapIcon::Cantha_CompleteMaster;
                            }

                            break;
                        case GW::RegionType::City:
                            icon_file_ids[0] = WorldMapIcon::Cantha_City;
                            break;
                        case GW::RegionType::Outpost:
                        case GW::RegionType::Challenge:
                            icon_file_ids[0] = WorldMapIcon::Cantha_Outpost;
                            break;
                    }
                    break;
                }
                case GW::Continent::Elona: {
                    switch (area_info->type) {
                        case GW::RegionType::MissionOutpost:
                        case GW::RegionType::CooperativeMission:
                            if (is_hard_mode) {
                                hardModeMissionIcons(icon_file_ids);
                            }
                            else {
                                icon_file_ids[icon_idx++] = WorldMapIcon::Elona_Mission;
                                if ((mission_state & ToolboxUtils::MissionState::Primary) != 0)
                                    icon_file_ids[icon_idx++] = WorldMapIcon::Elona_CompletePrimary;
                                if ((mission_state & ToolboxUtils::MissionState::Expert) != 0)
                                    icon_file_ids[icon_idx++] = WorldMapIcon::Elona_CompleteExpert;
                                if ((mission_state & ToolboxUtils::MissionState::Master) != 0)
                                    icon_file_ids[icon_idx++] = WorldMapIcon::Elona_CompleteMaster;
                            }
                            break;
                        case GW::RegionType::City:
                            icon_file_ids[0] = WorldMapIcon::Elona_City;
                            break;
                        case GW::RegionType::Outpost:
                        case GW::RegionType::Challenge:
                            icon_file_ids[0] = WorldMapIcon::Elona_Outpost;
                            break;
                    }
                }
                break;
                case GW::Continent::RealmOfTorment: {
                    switch (area_info->type) {
                        case GW::RegionType::MissionOutpost:
                        case GW::RegionType::CooperativeMission:
                            if (is_hard_mode) {
                                hardModeMissionIcons(icon_file_ids);
                            }
                            else {
                                icon_file_ids[icon_idx++] = WorldMapIcon::RealmOfTorment_Mission;
                                if ((mission_state & ToolboxUtils::MissionState::Primary) != 0)
                                    icon_file_ids[icon_idx++] = WorldMapIcon::Elona_CompletePrimary;
                                if ((mission_state & ToolboxUtils::MissionState::Expert) != 0)
                                    icon_file_ids[icon_idx++] = WorldMapIcon::Elona_CompleteExpert;
                                if ((mission_state & ToolboxUtils::MissionState::Master) != 0)
                                    icon_file_ids[icon_idx++] = WorldMapIcon::Elona_CompleteMaster;
                            }
                            break;
                        case GW::RegionType::City:
                            icon_file_ids[0] = WorldMapIcon::RealmOfTorment_City;
                            break;
                        case GW::RegionType::Outpost:
                        case GW::RegionType::Challenge:
                            icon_file_ids[0] = WorldMapIcon::RealmOfTorment_Outpost;
                            break;
                    }
                }
                break;
            }
        }
        for (size_t i = 0; i < _countof(icon_file_ids) && icon_file_ids[i] != WorldMapIcon::None; i++) {
            icons_out[i] = GwDatTextureModule::LoadTextureFromFileId(std::to_underlying(icon_file_ids[i]));
        }
    }

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

    enum class CompletionType : uint8_t {
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

    std::map<std::wstring, CharacterCompletion*> character_completion;
    GW::HookEntry OnPostUIMessage_Entry;

    std::map<Campaign, std::vector<OutpostUnlock*>> outposts;
    std::map<Campaign, std::vector<Mission*>> missions;
    std::map<Campaign, std::vector<Mission*>> vanquishes;
    std::map<Campaign, std::vector<PvESkill*>> elite_skills;
    std::map<Campaign, std::vector<PvESkill*>> pve_skills;
    std::map<Campaign, std::vector<HeroUnlock*>> heros;
    std::vector<FestivalHat*> festival_hats;
    std::vector<MinipetAchievement*> minipets;
    std::vector<WeaponAchievement*> hom_weapons;
    std::vector<ArmorAchievement*> hom_armor;
    std::vector<CompanionAchievement*> hom_companions;
    std::vector<HonorAchievement*> hom_titles;
    std::map<uint32_t, std::vector<UnlockedPvPItemUpgrade*>> unlocked_pvp_items;
    bool minipets_sorted = false;
    HallOfMonumentsAchievements hom_achievements;
    int hom_achievements_status = 0xf;

    const char* PvPItemUpgradeTypeName(const uint32_t pvp_upgrade_item_id)
    {
        if (const auto& info = GW::Items::GetPvPItemUpgrade(pvp_upgrade_item_id)) {
            const auto found = std::ranges::find_if(item_upgrades_by_file_id, [file_id = info->file_id](auto& check) { return check.file_id == file_id; });
            if (found != item_upgrades_by_file_id.end()) {
                return found->completion_category;
            }
        }
        return "Unknown Upgrades";
    }

    CompletionWindow& Instance()
    {
        return CompletionWindow::Instance();
    }

    HallOfMonumentsAchievements* GetCharacterHom(const std::wstring& player_name)
    {
        const auto cc = CompletionWindow::GetCharacterCompletion(player_name.c_str(), false);
        return cc ? &cc->hom_achievements : nullptr;
    }

    void OnCycleDisplayedMinipetsButton(const GW::UI::DialogButtonInfo* button)
    {
        if (wcsncmp(button->message, L"\x8102\x2B96\xA802\xD212\x380C", 5) != 0) {
            return; // Not "Cycle displayed minipets"
        }
        const wchar_t* dialog_body = DialogModule::GetDialogBody();
        if (!(dialog_body && wcsncmp(dialog_body, L"\x8102\x2B9D\xDE1D\xB19F\x52DD", 5) == 0)) {
            return; // Not devotion dialog "Miniatures on display"
        }
        static constexpr ctll::fixed_string displayed_miniatures = L"\x2\x102\x2([^\x102\x2]+)";
        std::wstring_view subject(dialog_body);
        std::wstring msg;
        const auto cc = character_completion[GetPlayerName()];
        auto& minipets_unlocked = cc->minipets_unlocked;
        minipets_unlocked.clear();
        for (auto m : ctre::search_all<displayed_miniatures>(subject)) {
            std::wstring miniature_encoded_name = m.get<1>().to_string();
            for (size_t i = 0; i < _countof(encoded_minipet_names); i++) {
                if (encoded_minipet_names[i] == miniature_encoded_name) {
                    ArrayBoolSet(minipets_unlocked, i, true);
                    break;
                }
            }
            subject.remove_prefix(std::distance(subject.begin(), m.get<0>().end()));
        }
        static constexpr ctll::fixed_string available_miniatures = L"\x2\x109\x2([^\x109\x2]+)";
        for (auto m : ctre::search_all<available_miniatures>(subject)) {
            std::wstring miniature_encoded_name = m.get<1>().to_string();
            for (size_t i = 0; i < _countof(encoded_minipet_names); i++) {
                if (encoded_minipet_names[i] == miniature_encoded_name) {
                    ArrayBoolSet(minipets_unlocked, i, true);
                    break;
                }
            }
            subject.remove_prefix(std::distance(subject.begin(), m.get<0>().end()));
        }
        Instance().CheckProgress();
    }

    void OnFestivalHatButton(const GW::UI::DialogButtonInfo* button)
    {
        if (wcsncmp(button->message, L"\x8101\x62E2\xAD6D\x82EB\x4C26 ", 5) != 0) {
            return; // Not "Lets talk about something else"
        }
        const wchar_t* dialog_body = DialogModule::GetDialogBody();
        if (!(dialog_body && wcsncmp(dialog_body, L"\x8101\x62E3\x8BAE\xA150\x1329", 5) == 0)) {
            return; // Not "Which festival hat would you like me to make you?"
        }

        const auto& buttons = DialogModule::GetDialogButtons();
        const auto cc = character_completion[GetPlayerName()];
        auto& unlocked = cc->festival_hats;
        for (const auto btn : buttons) {
            for (size_t i = 0; i < _countof(encoded_festival_hat_names); i++) {
                if (wcsstr(btn->message, encoded_festival_hat_names[i])) {
                    ArrayBoolSet(unlocked, i, true);
                    break;
                }
            }
        }
        Instance().CheckProgress();
    }

    // Flag miniature as unlocked for current character when dedicated
    void OnSendDialog(GW::HookStatus*, const GW::UI::UIMessage message_id, void* wparam, void*)
    {
        ASSERT(message_id == GW::UI::UIMessage::kSendDialog);
        if (GW::Map::GetMapID() != MapID::Hall_of_Monuments) {
            return;
        }
        uint32_t dialog_id = (uint32_t)wparam;
        const auto& available_dialogs = DialogModule::GetDialogButtons();
        const auto this_dialog_button = std::ranges::find_if(available_dialogs, [dialog_id](auto d) { return d->dialog_id == dialog_id; });
        if (this_dialog_button == available_dialogs.end()) {
            return;
        }
        const std::wstring_view subject((*this_dialog_button)->message);
        std::wstring msg;
        const auto cc = character_completion[GetPlayerName()];
        auto& minipets_unlocked = cc->minipets_unlocked;
        static constexpr ctll::fixed_string miniature_displayed_regex = L"\x8102\x2B91\xDAA2\xD19F\x32DB\x10A([^\x1]+)";
        if (auto m = ctre::search<miniature_displayed_regex>(subject)) {
            const std::wstring miniature_encoded_name = m.get<1>().to_string();
            for (size_t i = 0; i < _countof(encoded_minipet_names); i++) {
                if (encoded_minipet_names[i] == miniature_encoded_name) {
                    ArrayBoolSet(minipets_unlocked, i, true);
                    Instance().CheckProgress();
                    break;
                }
            }
        }
    }

    // ReSharper disable once CppParameterMayBeConst
    // ReSharper disable once CppParameterMayBeConstPtrOrRef
    void OnHomLoaded(HallOfMonumentsAchievements* result)
    {
        if (result->state != HallOfMonumentsAchievements::State::Done) {
            Log::LogW(L"Failed to load Hall of Monuments achievements for %s", result->character_name.c_str());
            return;
        }
        //Log::InfoW(L"Loaded Hom for %s", result->character_name);
        CompletionWindow::Instance().CheckProgress();
    }

    void FetchHom(HallOfMonumentsAchievements* out = nullptr)
    {
        if (!out) {
            const auto player_name = GetPlayerName();
            const auto cc = CompletionWindow::GetCharacterCompletion(player_name, true);
            if (!cc) {
                return;
            }
            out = &cc->hom_achievements;
        }
        if (!out->isLoading()) {
            HallOfMonumentsModule::AsyncGetAccountAchievements(out->character_name, out, OnHomLoaded);
        }
    }

    bool ParseCompletionBuffer(const CompletionType type, const wchar_t* character_name = nullptr, uint32_t* buffer = nullptr, size_t len = 0)
    {
        bool from_game = false;
        if (!character_name) {
            from_game = true;
            const GW::GameContext* g = GW::GetGameContext();
            if (!g) {
                return false;
            }
            const GW::CharContext* c = g->character;
            if (!c) {
                return false;
            }
            const GW::WorldContext* w = g->world;
            if (!w) {
                return false;
            }
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
                    buffer = static_cast<uint32_t*>(w->unlocked_map.m_buffer);
                    len = w->unlocked_map.m_size;
                    break;
                default: ASSERT("Invalid CompletionType" && false);
            }
        }
        const auto this_character_completion = CompletionWindow::GetCharacterCompletion(character_name, true);
        std::vector<uint32_t>* write_buf = nullptr;
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
                    const GW::HeroInfo* hero_arr = (GW::HeroInfo*)buffer;
                    if (write.size() < len) {
                        write.resize(len, 0);
                    }
                    for (size_t i = 0; i < len; i++) {
                        write[i] = hero_arr[i].hero_id;
                    }
                    return true;
                }
            }
            break;
            case CompletionType::MapsUnlocked:
                write_buf = &this_character_completion->maps_unlocked;
                break;
            case CompletionType::MinipetsUnlocked:
                write_buf = &this_character_completion->minipets_unlocked;
                break;
            case CompletionType::FestivalHats:
                write_buf = &this_character_completion->festival_hats;
                break;
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

    bool only_show_account_chars = true;

    GW::Array<GW::LoginCharacter>* GetAccountChars()
    {
        const auto p = GW::GetPreGameContext();
        return p ? &p->chars : nullptr;
    }

    bool pending_refresh_account_characters = false;

    void RefreshAccountCharacters()
    {
        pending_refresh_account_characters = true;
    }

    // Check login screen; assign missing characters to email account
    bool UpdateRefreshAccountCharacters()
    {
        const auto email = GetAccountEmail();
        if (!email) return false;
        const auto loading = std::ranges::find_if(character_completion, [](const std::pair<std::wstring, CharacterCompletion*>& t) {
            return t.second->hom_achievements.isLoading();
        });
        if (loading != character_completion.end())
            return false;
        const auto chars = GW::AccountMgr::GetAvailableChars();
        if (chars && chars->size()) {
            for (const auto& character : *chars) {
                if (character_completion.contains(character.player_name))
                    continue; // already loaded this character, don't overwrite loaded settings
                const auto cc = CompletionWindow::GetCharacterCompletion(character.player_name, true);
                cc->account = email;
                cc->profession = static_cast<Profession>(character.primary());
                cc->is_pvp = character.is_pvp();
                const auto map_info = GW::Map::GetMapInfo(character.map_id());
                cc->is_pre_searing = map_info && map_info->region == GW::Region::Region_Presearing;
            }
            // Remove any account chars that no longer exist
            auto it = character_completion.begin();
            while (it != character_completion.end()) {
                if (it->second->account == email) {
                    const auto exists = std::ranges::find_if(*chars, [char_name = it->first](const GW::AvailableCharacterInfo& character) {
                        return character.player_name == char_name;
                    });
                    if (exists == chars->end()) {
                        delete it->second;
                        character_completion.erase(it);
                        it = character_completion.begin();
                        continue;
                    }
                }
                it++;
            }
        }
        if (const auto pn = GetPlayerName()) {
            const auto cc = CompletionWindow::GetCharacterCompletion(pn);
            if (cc) {
                cc->account = email;
                const auto map_info = GW::Map::GetMapInfo();
                cc->is_pre_searing = map_info && map_info->region == GW::Region::Region_Presearing;
            }
        }
        return true;
    }

    // Cycle through all available professions - this will trigger the ui message to update the skills unlocked
    void CheckAllSkills()
    {
        if (GW::Map::GetInstanceType() != InstanceType::Outpost)
            return;
        const auto my_id = GW::Agents::GetControlledCharacterId();
        GW::SkillbarMgr::SkillTemplate original_template;
        if (!GW::SkillbarMgr::GetSkillTemplate(my_id, original_template) ||
            original_template.primary == Profession::None ||
            original_template.secondary == Profession::None)
            return;
        for (size_t i = (size_t)Profession::Dervish; i > 0; i--) {
            GW::PlayerMgr::ChangeSecondProfession((Profession)i);
        }
        ASSERT(GW::PlayerMgr::ChangeSecondProfession(original_template.secondary) && "Failed to restore original build/profession");;
        GW::SkillbarMgr::LoadSkillTemplate(my_id, original_template);
    }

    std::wstring& ReplaceString(std::wstring& subject, const std::wstring& search, const std::wstring& replace)
    {
        size_t pos = 0;
        while ((pos = subject.find(search, pos)) != std::string::npos) {
            subject.replace(pos, search.length(), replace);
            pos += replace.length();
        }
        return subject;
    }

    bool IsAreaComplete(const MapID map_id, CompletionCheck check)
    {
        if (map_id == MapID::None)
            return true;
        if (map_id == MapID::Tomb_of_the_Primeval_Kings)
            return true; // Topk special case

        const auto map = GW::Map::GetMapInfo();
        const auto w = GW::GetWorldContext();
        if (!(map && w))
            return false;
        switch (map->type) {
            case GW::RegionType::EliteMission:
                return true;
            case GW::RegionType::ExplorableZone:
                if (map->continent == GW::Continent::BattleIsles)
                    return true; // Fow, Uw
                return !map->GetIsOnWorldMap() || ArrayBoolAt(w->vanquished_areas, static_cast<uint32_t>(map_id));
        }

        if ((check & NormalMode) && !ArrayBoolAt(w->missions_completed, static_cast<uint32_t>(map_id)))
            return false;
        if ((check & HardMode) && !ArrayBoolAt(w->missions_completed_hm, static_cast<uint32_t>(map_id)))
            return false;
        const bool has_bonus = map->campaign != Campaign::EyeOfTheNorth;
        if (has_bonus) {
            if ((check & NormalMode) && !ArrayBoolAt(w->missions_bonus, static_cast<uint32_t>(map_id)))
                return false;
            if ((check & HardMode) && !ArrayBoolAt(w->missions_bonus_hm, static_cast<uint32_t>(map_id)))
                return false;
        }
        return true;
    }

    bool IsAreaComplete(const wchar_t* player_name, const MapID map_id, CompletionCheck check, const GW::AreaInfo* map)
    {
        if (map_id == MapID::None)
            return true;
        if (map_id == MapID::Tomb_of_the_Primeval_Kings)
            return true; // Topk special case
        const auto completion = CompletionWindow::GetCharacterCompletion(player_name, false);
        if (!(map && completion)) return false;

        switch (map->type) {
            case GW::RegionType::EliteMission:
                return true;
            case GW::RegionType::ExplorableZone:
                if (map->continent == GW::Continent::BattleIsles)
                    return true; // Fow, Uw
                return !map->GetIsOnWorldMap() || ArrayBoolAt(completion->vanquishes, static_cast<uint32_t>(map_id));
        }

        if ((check & NormalMode) && !ArrayBoolAt(completion->mission, static_cast<uint32_t>(map_id)))
            return false;
        if ((check & HardMode) && !ArrayBoolAt(completion->mission_hm, static_cast<uint32_t>(map_id)))
            return false;
        const bool has_bonus = map->campaign != Campaign::EyeOfTheNorth;
        if (has_bonus) {
            if ((check & NormalMode) && !ArrayBoolAt(completion->mission_bonus, static_cast<uint32_t>(map_id)))
                return false;
            if ((check & HardMode) && !ArrayBoolAt(completion->mission_bonus_hm, static_cast<uint32_t>(map_id)))
                return false;
        }
        return true;
    }

    void OnMapLoaded()
    {
        if (GW::Map::GetInstanceType() == InstanceType::Loading)
            return;
        const auto current_player_name = GetPlayerName();
        if (!(current_player_name && *current_player_name))
            return;
        if (current_player_name != last_player_name) {
            last_player_name = current_player_name;
            chosen_player_name_s.clear();
            chosen_player_name.clear();
            FetchHom();
        }
        ParseCompletionBuffer(CompletionType::Skills);
        ParseCompletionBuffer(CompletionType::Mission);
        ParseCompletionBuffer(CompletionType::Vanquishes);
        ParseCompletionBuffer(CompletionType::MissionBonus);
        ParseCompletionBuffer(CompletionType::MissionBonusHM);
        ParseCompletionBuffer(CompletionType::MissionHM);
        ParseCompletionBuffer(CompletionType::MapsUnlocked);
        ParseCompletionBuffer(CompletionType::Heroes);
        RefreshAccountCharacters();
        CompletionWindow::Instance().CheckProgress();
    }

    void OnPostUIMessage(GW::HookStatus* status, GW::UI::UIMessage message_id, void* wparam, void* lparam)
    {
        switch (message_id) {
            case GW::UI::UIMessage::kUpdateSkillsAvailable: {
                ParseCompletionBuffer(CompletionType::Skills);
                Instance().CheckProgress();
            }
            break;
            case GW::UI::UIMessage::kVanquishComplete: {
                ParseCompletionBuffer(CompletionType::Vanquishes);
                Instance().CheckProgress();
            }
            break;
            case GW::UI::UIMessage::kDungeonComplete:
            case GW::UI::UIMessage::kMissionComplete: {
                ParseCompletionBuffer(CompletionType::Mission);
                ParseCompletionBuffer(CompletionType::MissionBonus);
                ParseCompletionBuffer(CompletionType::MissionBonusHM);
                ParseCompletionBuffer(CompletionType::MissionHM);
                Instance().CheckProgress();
            }
            break;
            case GW::UI::UIMessage::kMapLoaded: {
                OnMapLoaded();
            }
            break;
            case GW::UI::UIMessage::kDialogButton: {
                const GW::UI::DialogButtonInfo* button = static_cast<GW::UI::DialogButtonInfo*>(wparam);
                OnCycleDisplayedMinipetsButton(button);
                OnFestivalHatButton(button);
            }
            break;
            case GW::UI::UIMessage::kSendDialog: {
                OnSendDialog(status, message_id, wparam, lparam);
            }
            break;
        }
    }
}


Color Mission::is_daily_bg_color = Colors::ARGB(102, 0, 255, 0);
Color Mission::has_quest_bg_color = Colors::ARGB(102, 0, 150, 0);
ImVec2 Mission::icon_size = {48.0f, 48.0f};

Mission::Mission(const MapID _outpost,
                 const QuestID _zm_quest)
    : outpost(_outpost),
      zm_quest(_zm_quest)
{
    map_to = outpost;
    const GW::AreaInfo* map_info = GW::Map::GetMapInfo(outpost);
    if (map_info) {
        name.reset(map_info->name_id);
    }
};

MapID Mission::GetOutpost() const
{
    return TravelWindow::GetNearestOutpost(map_to);
}

size_t Mission::GetLoadedIcons(IDirect3DTexture9* icons_out[4])
{
    size_t icons_added = 0;
    for (size_t i = 0; i < _countof(icons) && icons_added < 4; i++) {
        if (!icons[i])
            break;
        if (*icons[i]) {
            icons_out[icons_added++] = *icons[i];
        }
    }
    return icons_added;
}

bool Mission::Draw(IDirect3DDevice9*)
{
    const float scale = ImGui::GetIO().FontGlobalScale;

    ImVec2 s(icon_size.x * scale, icon_size.y * scale);
    auto bg = ImVec4(0, 0, 0, 0);
    if (IsDaily()) {
        bg = ImColor(is_daily_bg_color);
    }
    else if (HasQuest()) {
        bg = ImColor(has_quest_bg_color);
    }
    constexpr ImVec4 tint(1, 1, 1, 1);

    const ImVec2 cursor_pos = ImGui::GetCursorPos();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.f, 0.5f));
    ImGui::PushID(this);

    bool clicked = false;
    bool hovered = false;

    IDirect3DTexture9* icons_out[4] = {nullptr};
    size_t icons_len = GetLoadedIcons(icons_out);


    if (show_as_list) {
        s.y /= 2.f;
        if (!map_unlocked) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
        }
        clicked = ImGui::CompositeIconButton(Name(), (ImTextureID*)icons_out, icons_len, {s.x * 5.f, s.y}, 0, {s.x / 2.f, s.y}, icon_uv_offset[0], icon_uv_offset[1]);
        hovered = ImGui::IsItemHovered();
        if (!map_unlocked) {
            ImGui::PopStyleColor();
        }
    }
    else {
        clicked = ImGui::CompositeIconButton("", (ImTextureID*)icons_out, icons_len, s, 0, s, icon_uv_offset[0], icon_uv_offset[1]);
        hovered = ImGui::IsItemHovered();
    }
    if (clicked) {
        OnClick();
    }
    if (hovered) {
        OnHover();
    }
    ImGui::PopID();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();

    if (is_completed && bonus && show_as_list) {
        const ImVec2 cursor_pos2 = ImGui::GetCursorPos();
        ImVec2 icon_size_scaled = {icon_size.x * ImGui::GetIO().FontGlobalScale, icon_size.y * ImGui::GetIO().FontGlobalScale};
        if (show_as_list) {
            icon_size_scaled.x /= 2.f;
            icon_size_scaled.y /= 2.f;
        }

        const ImColor completed_bg = IM_COL32(0, 0x99, 0, 192);
        const ImColor completed_text = IM_COL32(0xE5, 0xFF, 0xCC, 255);
        ImGui::SetCursorPos(cursor_pos);
        const ImVec2 screen_pos = ImGui::GetCursorScreenPos();
        ImGui::RenderFrame(screen_pos, {screen_pos.x + icon_size_scaled.x, screen_pos.y + icon_size_scaled.y}, completed_bg, false, 0.f);

        ImGui::SetCursorPos(cursor_pos);

        const ImVec2 check_size = ImGui::CalcTextSize(ICON_FA_CHECK);
        ImGui::GetWindowDrawList()->AddText({screen_pos.x + (icon_size_scaled.x - check_size.x) / 2, screen_pos.y + (icon_size_scaled.y - check_size.y) / 2},
                                            completed_text, ICON_FA_CHECK);
        ImGui::SetCursorPos(cursor_pos2);
    }
    return true;
}

const char* Mission::Name()
{
    return name.string().c_str();
}

void Mission::OnClick()
{
    const MapID travel_to = GetOutpost();
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

void Mission::OnHover()
{
    ImGui::SetTooltip([&] {
        ImGui::TextUnformatted(name.string().c_str());
        const auto chars_without_completed = CompletionWindow::GetCharactersWithoutAreaComplete(outpost);
        if (!chars_without_completed.empty()) {
            ImGui::Separator();
            ImGui::TextUnformatted("Characters who have not completed this area:");
            auto icon_size = ImGui::CalcTextSize(" ");
            icon_size.x = icon_size.y;
            for (auto char_completion : chars_without_completed) {
                ImGui::Image(*Resources::GetProfessionIcon(char_completion->profession), icon_size);
                ImGui::SameLine();
                ImGui::TextUnformatted(char_completion->name_str.c_str());
            }
        }
    });
}

void Mission::CheckProgress(const std::wstring& player_name)
{
    is_completed = bonus = false;
    const auto& completion = character_completion;
    if (!completion.contains(player_name)) {
        return;
    }
    const auto& player_completion = completion.at(player_name);
    const std::vector<uint32_t>* missions_complete = &player_completion->mission;
    const std::vector<uint32_t>* missions_bonus = &player_completion->mission_bonus;
    if (hard_mode) {
        missions_complete = &player_completion->mission_hm;
        missions_bonus = &player_completion->mission_bonus_hm;
    }
    map_unlocked = player_completion->maps_unlocked.empty() || ArrayBoolAt(player_completion->maps_unlocked, std::to_underlying(outpost));
    is_completed = ArrayBoolAt(*missions_complete, std::to_underlying(outpost));
    bonus = ArrayBoolAt(*missions_bonus, std::to_underlying(outpost));

    GW::Array<uint32_t> complete_arr;
    complete_arr.m_buffer = const_cast<uint32_t*>(missions_complete->data());
    complete_arr.m_capacity = complete_arr.m_size = missions_complete->size();

    GW::Array<uint32_t> bonus_arr;
    bonus_arr.m_buffer = const_cast<uint32_t*>(missions_bonus->data());
    bonus_arr.m_capacity = bonus_arr.m_size = missions_bonus->size();

    mission_state = ToolboxUtils::GetMissionState(outpost, complete_arr, bonus_arr);

    GetOutpostIcons(outpost, icons, mission_state, hard_mode);
}

void OutpostUnlock::CheckProgress(const std::wstring& player_name)
{
    const auto& completion = character_completion;
    if (!completion.contains(player_name)) {
        return;
    }
    const auto& player_completion = completion.at(player_name);
    is_completed = bonus = map_unlocked = ArrayBoolAt(player_completion->maps_unlocked, std::to_underlying(outpost));

    GetOutpostIcons(outpost, icons, 0);
}

bool OutpostUnlock::Draw(IDirect3DDevice9* device)
{
    if (!Mission::Draw(device))
        return false;
    return true;
}

void OutpostUnlock::OnHover()
{
    ImGui::SetTooltip([&]() {
        ImGui::TextUnformatted(name.string().c_str());
        const auto chars_without_completed = CompletionWindow::GetCharactersWithoutAreaUnlocked(outpost);
        if (!chars_without_completed.empty()) {
            ImGui::Separator();
            ImGui::TextUnformatted("Characters who have not unlocked this area:");
            auto icon_size = ImGui::CalcTextSize(" ");
            icon_size.x = icon_size.y;
            for (auto char_completion : chars_without_completed) {
                ImGui::Image(*Resources::GetProfessionIcon(char_completion->profession), icon_size);
                ImGui::SameLine();
                ImGui::TextUnformatted(char_completion->name_str.c_str());
            }
        }
    });
}

bool Mission::IsDaily()
{
    return false;
}

bool Mission::HasQuest()
{
    return GW::QuestMgr::GetQuest(zm_quest) != nullptr;
}

bool EotNMission::HasQuest()
{
    for (const auto& zb : zb_quests) {
        if (GW::QuestMgr::GetQuest(zb)) {
            return true;
        }
    }
    return false;
}


void EotNMission::CheckProgress(const std::wstring& player_name)
{
    Mission::CheckProgress(player_name);
    bonus = is_completed;
    // EotN mission icons are sprited - first sprite for incomplete, second for complete
    if (is_completed) {
        icon_uv_offset[0] = {.5f, 0.f};
        icon_uv_offset[1] = {1.f, 1.f};
    }
    else {
        icon_uv_offset[0] = {.0f, 0.f};
        icon_uv_offset[1] = {.5f, 1.f};
    }
}

HeroUnlock::HeroUnlock(HeroID _hero_id)
    : PvESkill(SkillID::No_Skill)
{
    skill_id = static_cast<SkillID>(_hero_id);
}

void HeroUnlock::CheckProgress(const std::wstring& player_name)
{
    is_completed = false;
    const auto& skills = character_completion;
    if (!skills.contains(player_name)) {
        return;
    }
    auto& heroes = skills.at(player_name)->heroes;
    is_completed = bonus = std::ranges::contains(heroes, std::to_underlying(skill_id));
}

const char* HeroUnlock::Name()
{
    return hero_names[std::to_underlying(skill_id)];
}

size_t HeroUnlock::GetLoadedIcons(IDirect3DTexture9* icons_out[4])
{
    if (!icons_loaded) {
        *icons = new IDirect3DTexture9*();
        const auto path = Resources::GetPath(L"img/heros");
        Resources::EnsureFolderExists(path);
        wchar_t local_image[MAX_PATH];
        swprintf(local_image, _countof(local_image), L"%s/hero_%d.jpg", path.c_str(), skill_id);
        char remote_image[128];
        snprintf(remote_image, _countof(remote_image), "https://github.com/gwdevhub/GWToolboxpp/raw/master/resources/heros/hero_%d.jpg", skill_id);
        Resources::LoadTexture(*icons, local_image, remote_image);
        icons_loaded = true;
    }
    return Mission::GetLoadedIcons(icons_out);
}

HeroUnlock::~HeroUnlock()
{
    if (*icons)
        delete*icons;
}

void HeroUnlock::OnClick()
{
    wchar_t buf[128];
    swprintf(buf, 128, L"Game_link:Hero_%d", skill_id);
    GW::GameThread::Enqueue([buf] {
        GuiUtils::OpenWiki(buf);
    });
}

ItemAchievement::ItemAchievement(const size_t _encoded_name_index, const wchar_t* encoded_name)
    : PvESkill(SkillID::No_Skill)
{
    encoded_name_index = _encoded_name_index;
    name.language(Language::English);
    name.reset(encoded_name);
}

const char* ItemAchievement::Name()
{
    return name.string().c_str();
}

size_t ItemAchievement::GetLoadedIcons(IDirect3DTexture9* icons_out[4])
{
    if (!icons_loaded && !name.wstring().empty()) {
        if (name.wstring() == L"Brown Rabbit") {
            *icons = Resources::GetItemImage(L"Brown Rabbit (miniature)");
        }
        else if (name.wstring() == L"Oppressor's Bow") {
            *icons = Resources::GetItemImage(L"Oppressor's Longbow");
        }
        else if (name.wstring() == L"Tormented Bow") {
            *icons = Resources::GetItemImage(L"Tormented Longbow");
        }
        else if (name.wstring() == L"Destroyer Bow") {
            *icons = Resources::GetItemImage(L"Destroyer Longbow");
        }
        else {
            *icons = Resources::GetItemImage(name.wstring());
        }
        icons_loaded = true;
    }
    return Mission::GetLoadedIcons(icons_out);
}

void ItemAchievement::OnClick()
{
    GW::GameThread::Enqueue([url = name.wstring()] {
        GuiUtils::OpenWiki(url);
    });
}

size_t PvESkill::GetLoadedIcons(IDirect3DTexture9* icons_out[4])
{
    if (!icons_loaded) {
        *icons = Resources::GetSkillImage(skill_id);
        icons_loaded = true;
    }
    return Mission::GetLoadedIcons(icons_out);
}

PvESkill::PvESkill(const SkillID _skill_id)
    : Mission(MapID::None),
      skill_id(_skill_id)
{
    if (_skill_id != SkillID::No_Skill) {
        const auto skill = GW::SkillbarMgr::GetSkillConstantData(skill_id);
        if (skill) {
            name.reset(skill->name);
            profession = skill->profession;
        }
    }
}

void PvESkill::OnClick()
{
    const auto wtf = std::format(L"Game_link:Skill_{}", static_cast<std::underlying_type_t<SkillID>>(skill_id));
    // revert this dumb shit once Microsoft fixes the weird bug
    GW::GameThread::Enqueue([url = wtf] {
        GuiUtils::OpenWiki(url);
    });
}

void PvESkill::OnHover()
{
    ImGui::SetTooltip([&]() {
        ImGui::TextUnformatted(name.string().c_str());
        const auto chars_without_completed = CompletionWindow::GetCharactersWithoutSkillUnlocked(skill_id);
        if (!chars_without_completed.empty()) {
            ImGui::Separator();
            ImGui::TextUnformatted("Players without this skill unlocked:");
            auto icon_size = ImGui::CalcTextSize(" ");
            icon_size.x = icon_size.y;
            for (auto char_completion : chars_without_completed) {
                ImGui::Image(*Resources::GetProfessionIcon(char_completion->profession), icon_size);
                ImGui::SameLine();
                ImGui::TextUnformatted(char_completion->name_str.c_str());
            }
        }
    });
}

bool PvESkill::Draw(IDirect3DDevice9* device)
{
    const ImVec2 cursor_pos = ImGui::GetCursorPos();
    if (!Mission::Draw(device)) {
        return false;
    }
    if (is_completed && !show_as_list) {
        const ImVec2 cursor_pos2 = ImGui::GetCursorPos();
        ImVec2 icon_size_scaled = {icon_size.x * ImGui::GetIO().FontGlobalScale, icon_size.y * ImGui::GetIO().FontGlobalScale};
        if (show_as_list) {
            icon_size_scaled.x /= 2.f;
            icon_size_scaled.y /= 2.f;
        }

        const ImColor completed_bg = IM_COL32(0, 0x99, 0, 192);
        const ImColor completed_text = IM_COL32(0xE5, 0xFF, 0xCC, 255);
        ImGui::SetCursorPos(cursor_pos);
        const ImVec2 screen_pos = ImGui::GetCursorScreenPos();
        ImGui::RenderFrame(screen_pos, {screen_pos.x + icon_size_scaled.x, screen_pos.y + icon_size_scaled.y}, completed_bg, false, 0.f);

        ImGui::SetCursorPos(cursor_pos);

        const ImVec2 check_size = ImGui::CalcTextSize(ICON_FA_CHECK);
        ImGui::GetWindowDrawList()->AddText({screen_pos.x + (icon_size_scaled.x - check_size.x) / 2.f,
                                             screen_pos.y + (icon_size_scaled.y - check_size.y) / 2.f},
                                            completed_text, ICON_FA_CHECK);
        ImGui::SetCursorPos(cursor_pos2);
    }
    return true;
}

void PvESkill::CheckProgress(const std::wstring& player_name)
{
    is_completed = false;
    const auto& skills = character_completion;
    if (!skills.contains(player_name)) {
        return;
    }
    const auto& unlocked = skills.at(player_name)->skills;
    is_completed = bonus = ArrayBoolAt(unlocked, std::to_underlying(skill_id));
}

FactionsPvESkill::FactionsPvESkill(const SkillID skill_id)
    : PvESkill(skill_id)
{
    GW::Skill* s = GW::SkillbarMgr::GetSkillConstantData(skill_id);
    if (s) {
        uint32_t faction_id = 0x6C3D;
        if (static_cast<TitleID>(s->title) == TitleID::Luxon) {
            faction_id = 0x6C3E;
        }
        std::wstring buf;
        buf.resize(32, 0);
        GW::UI::UInt32ToEncStr(s->name, buf.data(), buf.size());
        buf.resize(wcslen(buf.data()));
        buf += L"\x2\x108\x107 - \x1\x2";
        buf.resize(wcslen(buf.data()) + 4, 0);
        GW::UI::UInt32ToEncStr(faction_id, buf.data() + buf.size() - 4, 4);
        buf.resize(wcslen(buf.data()) + 1, 0);
        name.reset(buf.c_str());
    }
};

bool FactionsPvESkill::Draw(IDirect3DDevice9* device)
{
    //icon_size.y *= 2.f;
    const bool drawn = PvESkill::Draw(device);
    //icon_size.y /= 2.f;
    return drawn;
}

void Vanquish::CheckProgress(const std::wstring& player_name)
{
    is_completed = false;
    const auto& completion = character_completion;
    if (!completion.contains(player_name)) {
        return;
    }
    const auto& unlocked = completion.at(player_name)->vanquishes;
    is_completed = bonus = ArrayBoolAt(unlocked, std::to_underlying(outpost));
    mission_state = is_completed ? 0x7 : 0x0;

    GetOutpostIcons(outpost, icons, mission_state, true);
}

void CompletionWindow::Initialize()
{
    ToolboxWindow::Initialize();

    *min_size = 780.f;

    //Resources::LoadTexture(&button_texture, Resources::GetPath(L"img/missions", L"MissionIcon.png"), IDB_Missions_MissionIcon);
    outposts = {
        {Campaign::Prophecies, {}},
        {Campaign::Factions, {}},
        {Campaign::Nightfall, {}},
        {Campaign::EyeOfTheNorth, {}}
    };

    missions = {
        {Campaign::Prophecies, {}},
        {Campaign::Factions, {}},
        {Campaign::Nightfall, {}},
        {Campaign::EyeOfTheNorth, {}},
        {Campaign::BonusMissionPack, {}},
    };
    vanquishes = {
        {Campaign::Prophecies, {}},
        {Campaign::Factions, {}},
        {Campaign::Nightfall, {}},
        {Campaign::EyeOfTheNorth, {}}
    };
    elite_skills = {
        {Campaign::Prophecies, {}},
        {Campaign::Factions, {}},
        {Campaign::Nightfall, {}},
        {Campaign::Core, {}},
    };
    pve_skills = {
        {Campaign::Factions, {}},
        {Campaign::Nightfall, {}},
        {Campaign::EyeOfTheNorth, {}},
        {Campaign::Core, {}},
    };
    heros = {
        {Campaign::Factions, {}},
        {Campaign::Nightfall, {}},
        {Campaign::EyeOfTheNorth, {}}
    };
    for (size_t i = 0; i < _countof(encoded_minipet_names); i++) {
        minipets.push_back(new MinipetAchievement(i, encoded_minipet_names[i]));
    }
    for (size_t i = 0; i < _countof(encoded_festival_hat_names); i++) {
        festival_hats.push_back(new FestivalHat(i, encoded_festival_hat_names[i]));
    }
    for (size_t i = 0; i < _countof(encoded_weapon_names); i++) {
        hom_weapons.push_back(new WeaponAchievement(i, encoded_weapon_names[i]));
    }
    /*auto address = GW::Scanner::FindAssertion("\\Code\\Gw\\Const\\constitempvp.cpp", "unlockIndex < ITEM_PVP_UNLOCK_COUNT");
    if (address) {
        unlocked_pvp_item_array_buffer = *(PvPItemInfo**)(address + 0x15);
        unlocked_pvp_item_array_size = *(size_t*)(address - 0xb);
    }
    for (size_t i = 0; i < unlocked_pvp_item_array_size; i++) {
        unlocked_pvp_items.push_back(new UnlockedPvPItem(i));
    }*/

    /*auto address = GW::Scanner::FindAssertion("\\Code\\Gw\\Const\\constitempvp.cpp", "index < ITEM_PVP_ITEM_COUNT");
    if (address) {
        unlocked_pvp_item_array_buffer = *(PvPItemInfo**)(address + 0x15);
        unlocked_pvp_item_array_size = *(size_t*)(address - 0xb);
    }*/

    const auto& unlocked_pvp_item_upgrade_array = GW::Items::GetPvPItemUpgradesArray();

    for (const auto& it : item_upgrades_by_file_id) {
        // GW::Array<something> unlocked_pvp_item_upgrade_array
        for (size_t i = 0; i < unlocked_pvp_item_upgrade_array.size(); i++) {
            const auto& deets = unlocked_pvp_item_upgrade_array[i];
            if (deets.file_id != it.file_id) {
                continue;
            }
            if (deets.is_dev) {
                continue;
            }
            unlocked_pvp_items[(uint32_t)PvPItemUpgradeTypeName(i)].push_back(new UnlockedPvPItemUpgrade(i));
        }
    }

    std::unordered_map<uint32_t, char> dupes;
    for (const auto campaign : outposts | std::views::keys) {
        for (size_t i = 1; i < static_cast<size_t>(MapID::Count); i++) {
            const auto map_id = static_cast<MapID>(i);
            if (map_id == MapID::Titans_Tears)
                continue;
            const auto info = GW::Map::GetMapInfo(map_id);
            if (!info) continue;
            if (!info->GetIsOnWorldMap()) continue;
            if (dupes.contains(info->name_id))
                continue;
            if (info->campaign != campaign) continue;
            if (info->region == GW::Region::Region_Presearing) continue;
            switch (info->type) {
                case GW::RegionType::CooperativeMission:
                case GW::RegionType::MissionOutpost:
                    if (!info->thumbnail_id)
                        break;
                    missions[campaign].push_back(new Mission(static_cast<MapID>(i)));
                case GW::RegionType::City:
                case GW::RegionType::CompetitiveMission:
                case GW::RegionType::Outpost:
                case GW::RegionType::Challenge:
                case GW::RegionType::Arena:
                    switch (map_id) {
                        case MapID::Augury_Rock_outpost:
                        case MapID::Fort_Aspenwood_mission:
                        case MapID::The_Jade_Quarry_mission:
                            continue;
                        default:
                            break;
                    }
                    outposts[campaign].push_back(new OutpostUnlock(static_cast<MapID>(i)));
                    dupes[info->name_id] = 1;
                    break;
                case GW::RegionType::EliteMission:
                    if (map_id == MapID::Domain_of_Anguish) {
                        outposts[campaign].push_back(new OutpostUnlock(static_cast<MapID>(i)));
                    }
                    break;
                case GW::RegionType::ExplorableZone:
                    if (!info->GetIsVanquishableArea())
                        break;
                    if (map_id == MapID::War_in_Kryta_Talmark_Wilderness)
                        continue;
                    vanquishes[campaign].push_back(new Vanquish(static_cast<MapID>(i)));
                    dupes[info->name_id] = 1;
                    break;
            }
        }
    }


    hom_armor.push_back(new ArmorAchievement(hom_armor.size(), L"\x108\x107" "Elite Canthan Armor\x1", "Elementalist_Elite_Canthan_armor_m.jpg"));
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

    ASSERT(hom_armor.size() == static_cast<size_t>(ResilienceDetail::Count));

    hom_companions.push_back(new CompanionAchievement(hom_companions.size(), L"\x108\x107" "Zenmai\x1", "Zenmai_statue.jpg"));
    hom_companions.push_back(new CompanionAchievement(hom_companions.size(), L"\x108\x107" "Norgu\x1", "Norgu_statue.jpg"));
    hom_companions.push_back(new CompanionAchievement(hom_companions.size(), L"\x108\x107" "Goren\x1", "Goren_statue.jpg"));
    hom_companions.push_back(new CompanionAchievement(hom_companions.size(), L"\x108\x107" "Zhed Shadowhoof\x1", "Zhed_Shadowhoof_statue.jpg"));
    hom_companions.push_back(new CompanionAchievement(hom_companions.size(), L"\x108\x107" "General Morgahn\x1", "General_Morgahn_statue.jpg"));
    hom_companions.push_back(new CompanionAchievement(hom_companions.size(), L"\x108\x107" "Margrid The Sly\x1", "Margrid_the_Sly_statue.jpg"));
    hom_companions.push_back(new CompanionAchievement(hom_companions.size(), L"\x108\x107" "Tahlkora\x1", "Tahlkora_statue.jpg"));
    hom_companions.push_back(new CompanionAchievement(hom_companions.size(), L"\x108\x107" "Razah\x1", "Razah_statue.jpg"));
    hom_companions.push_back(new CompanionAchievement(hom_companions.size(), L"\x108\x107" "Master Of Whispers\x1", "Master_of_Whispers_statue.jpg"));
    hom_companions.push_back(new CompanionAchievement(hom_companions.size(), L"\x108\x107" "Koss\x1", "Koss_statue.jpg"));
    hom_companions.push_back(new CompanionAchievement(hom_companions.size(), L"\x108\x107" "Dunkoro\x1", "Dunkoro_statue.jpg"));
    hom_companions.push_back(new CompanionAchievement(hom_companions.size(), L"\x108\x107" "Melonni\x1", "Melonni_statue.jpg"));
    hom_companions.push_back(new CompanionAchievement(hom_companions.size(), L"\x108\x107" "Acolyte Jin\x1", "Acolyte_Jin_statue.jpg"));
    hom_companions.push_back(new CompanionAchievement(hom_companions.size(), L"\x108\x107" "Acolyte Sousuke\x1", "Acolyte_Sousuke_statue.jpg"));
    hom_companions.push_back(new CompanionAchievement(hom_companions.size(), L"\x108\x107" "Vekk\x1", "Vekk_statue.jpg"));
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

    ASSERT(hom_companions.size() == static_cast<size_t>(FellowshipDetail::Count));

    size_t hom_titles_index = 0;
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Champion\x1", "Eternal_Champion.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Commander\x1", "Eternal_Commander.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Skillz\x1", "Eternal_Skillz.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Gladiator\x1", "Eternal_Gladiator.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Hero\x1", "Eternal_Hero.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Lightbringer\x1", "Eternal_Lightbringer.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Bookah\x1", "Eternal_Bookah.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Delver\x1", "Eternal_Delver.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Slayer\x1", "Eternal_Slayer.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Ebon Vanguard Agent\x1", "Eternal_Ebon_Vanguard_Agent.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Defender of Ascalon\x1", "Eternal_Defender_of_Ascalon.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Tyrian Cartographer\x1", "Eternal_Tyrian_Cartographer.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Guardian of Tyria\x1", "Eternal_Guardian_of_Tyria.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Protector of Tyria\x1", "Eternal_Protector_of_Tyria.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Tyrian Skill Hunter\x1", "Eternal_Tyrian_Skill_Hunter.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Tyrian Vanquisher\x1", "Eternal_Tyrian_Vanquisher.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Canthan Cartographer\x1", "Eternal_Canthan_Cartographer.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Guardian of Cantha\x1", "Eternal_Guardian_of_Cantha.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Protector of Cantha\x1", "Eternal_Protector_of_Cantha.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Canthan Skill Hunter\x1", "Eternal_Canthan_Skill_Hunter.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Canthan Vanquisher\x1", "Eternal_Canthan_Vanquisher.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Savior of the Kurzicks\x1", "Eternal_Savior_of_the_Kurzicks.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Savior of the Luxons\x1", "Eternal_Savior_of_the_Luxons.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Elonian Cartographer\x1", "Eternal_Elonian_Cartographer.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Guardian of Elona\x1", "Eternal_Guardian_of_Elona.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Protector of Elona\x1", "Eternal_Protector_of_Elona.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Elonian Skill Hunter\x1", "Eternal_Elonian_Skill_Hunter.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Elonian Vanquisher\x1", "Eternal_Elonian_Vanquisher.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Ale Hound\x1", "Eternal_Ale_Hound.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Party Animal\x1", "Eternal_Party_Animal.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Master of the North\x1", "Eternal_Master_of_the_North.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Legendary Cartographer\x1", "Eternal_Legendary_Cartographer.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Legendary Guardian\x1", "Eternal_Legendary_Guardian.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Legendary Skill Hunter\x1", "Eternal_Legendary_Skill_Hunter.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Legendary Vanquisher\x1", "Eternal_Legendary_Vanquisher.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Fortune\x1", "Eternal_Fortune.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Sweet Tooth\x1", "Eternal_Sweet_Tooth.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Spearmarshal\x1", "Eternal_Spearmarshal.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Survivor\x1", "Eternal_Survivor.jpg"));
    hom_titles_index++; // NB: This is the character based survivor title, which isn't used anymore.
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Treasure Hunter\x1", "Eternal_Treasure_Hunter.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Misfortune\x1", "Eternal_Misfortune.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Source of Wisdom\x1", "Eternal_Source_of_Wisdom.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Hero of Tyria\x1", "Eternal_Hero_of_Tyria.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Hero of Cantha\x1", "Eternal_Hero_of_Cantha.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Hero of Elona\x1", "Eternal_Hero_of_Elona.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Conqueror of Sorrow's Furnace\x1", "Eternal_Conqueror_of_Sorrow's_Furnace.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Conqueror of The Deep\x1", "Eternal_Conqueror_of_the_Deep.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Conqueror of Urgoz's Warren\x1", "Eternal_Conqueror_of_Urgoz's_Warren.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Conqueror of The Fissure of Woe\x1", "Eternal_Conqueror_of_the Fissure of Woe.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Conqueror of The Underworld\x1", "Eternal_Conqueror_of_the Underworld.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Conqueror of The Domain of Anguish\x1", "Eternal_Conqueror_of_the_Domain_of_Anguish.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Zaishen Supporter\x1", "Eternal_Zaishen_Supporter.jpg"));
    hom_titles.push_back(new HonorAchievement(hom_titles_index++, L"\x108\x107" "Eternal Codex Disciple\x1", "Eternal_Codex_Disciple.png"));

    ASSERT(hom_titles_index == static_cast<size_t>(HonorDetail::Count));

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

    constexpr GW::UI::UIMessage message_ids[] = {
        GW::UI::UIMessage::kMapLoaded,
        GW::UI::UIMessage::kSendDialog,
        GW::UI::UIMessage::kDialogButton,
        GW::UI::UIMessage::kMissionComplete,
        GW::UI::UIMessage::kVanquishComplete,
        GW::UI::UIMessage::kDungeonComplete,
        GW::UI::UIMessage::kUpdateSkillsAvailable
    };
    for (const auto message_id : message_ids) {
        RegisterUIMessageCallback(&OnPostUIMessage_Entry, message_id, OnPostUIMessage, 0x8000);
    }
    GW::GameThread::Enqueue(OnMapLoaded);
}

void CompletionWindow::Initialize_Prophecies()
{
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
    eskills.push_back(new PvESkill(SkillID::Avatar_of_Melandru));
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
    auto& eotn_missions = missions.at(Campaign::EyeOfTheNorth);
    // Asura
    eotn_missions.push_back(new EotNMission(MapID::Finding_the_Bloodstone_mission));
    eotn_missions.push_back(new EotNMission(MapID::The_Elusive_Golemancer_mission));
    eotn_missions.push_back(new EotNMission(MapID::Genius_Operated_Living_Enchanted_Manifestation_mission, QuestID::ZaishenMission_G_O_L_E_M));
    // Vanguard
    eotn_missions.push_back(new EotNMission(MapID::Against_the_Charr_mission));
    eotn_missions.push_back(new EotNMission(MapID::Warband_of_brothers_mission));
    eotn_missions.push_back(new EotNMission(MapID::Assault_on_the_Stronghold_mission, QuestID::ZaishenMission_Assault_on_the_Stronghold));
    // Norn
    eotn_missions.push_back(new EotNMission(MapID::Curse_of_the_Nornbear_mission, QuestID::ZaishenMission_Curse_of_the_Nornbear));
    eotn_missions.push_back(new EotNMission(MapID::A_Gate_Too_Far_mission, QuestID::ZaishenMission_A_Gate_Too_Far));
    eotn_missions.push_back(new EotNMission(MapID::Blood_Washes_Blood_mission));
    // Destroyers
    eotn_missions.push_back(new EotNMission(MapID::Destructions_Depths_mission, QuestID::ZaishenMission_Destructions_Depths));
    eotn_missions.push_back(new EotNMission(MapID::A_Time_for_Heroes_mission, QuestID::ZaishenMission_A_Time_for_Heroes));
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
    skills.push_back(new PvESkill(SkillID::Brawling_Headbutt));
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
    auto& dungeons = missions.at(Campaign::BonusMissionPack);
    dungeons.push_back(new EotNMission(
        MapID::Catacombs_of_Kathandrax_Level_1, QuestID::ZaishenBounty_Ilsundur_Lord_of_Fire));
    dungeons.push_back(new EotNMission(
        MapID::Rragars_Menagerie_Level_1, QuestID::ZaishenBounty_Rragar_Maneater));
    dungeons.push_back(new EotNMission(
        MapID::Cathedral_of_Flames_Level_1, QuestID::ZaishenBounty_Murakai_Lady_of_the_Night));
    dungeons.push_back(new EotNMission(
        MapID::Ooze_Pit_mission));
    dungeons.push_back(new EotNMission(
        MapID::Darkrime_Delves_Level_1));
    dungeons.push_back(new EotNMission(
        MapID::Frostmaws_Burrows_Level_1));
    dungeons.push_back(new EotNMission(
        MapID::Sepulchre_of_Dragrimmar_Level_1, QuestID::ZaishenBounty_Remnant_of_Antiquities));
    dungeons.push_back(new EotNMission(
        MapID::Ravens_Point_Level_1, QuestID::ZaishenBounty_Plague_of_Destruction));
    dungeons.push_back(new EotNMission(
        MapID::Vloxen_Excavations_Level_1, QuestID::ZaishenBounty_Zoldark_the_Unholy));
    dungeons.push_back(new EotNMission(
        MapID::Bogroot_Growths_Level_1));
    dungeons.push_back(new EotNMission(
        MapID::Bloodstone_Caves_Level_1));
    dungeons.push_back(new EotNMission(
        MapID::Shards_of_Orr_Level_1, QuestID::ZaishenBounty_Fendi_Nin));
    dungeons.push_back(new EotNMission(
        MapID::Oolas_Lab_Level_1, QuestID::ZaishenBounty_TPS_Regulator_Golem));
    dungeons.push_back(new EotNMission(
        MapID::Arachnis_Haunt_Level_1, QuestID::ZaishenBounty_Arachni));
    dungeons.push_back(new EotNMission(
        MapID::Slavers_Exile_Level_1, {
            QuestID::ZaishenBounty_Forgewight,
            QuestID::ZaishenBounty_Selvetarm,
            QuestID::ZaishenBounty_Justiciar_Thommis,
            QuestID::ZaishenBounty_Rand_Stormweaver,
            QuestID::ZaishenBounty_Duncan_the_Black}));
    dungeons.push_back(new EotNMission(
        MapID::Fronis_Irontoes_Lair_mission, {QuestID::ZaishenBounty_Fronis_Irontoe}));
    dungeons.push_back(new EotNMission(
        MapID::Secret_Lair_of_the_Snowmen));
    dungeons.push_back(new EotNMission(
        MapID::Heart_of_the_Shiverpeaks_Level_1, {QuestID::ZaishenBounty_Magmus}));
}


void CompletionWindow::Terminate()
{
    GW::UI::RemoveUIMessageCallback(&OnPostUIMessage_Entry);
    auto clear_vec = [](auto& vec) {
        for (auto& c : vec) {
            CLEAR_PTR_VEC(c.second);
        }
        vec.clear();
    };

    clear_vec(outposts);
    clear_vec(missions);
    clear_vec(vanquishes);
    clear_vec(pve_skills);
    clear_vec(elite_skills);
    clear_vec(heros);
    clear_vec(unlocked_pvp_items);

    CLEAR_PTR_VEC(festival_hats);
    CLEAR_PTR_VEC(minipets);
    CLEAR_PTR_VEC(hom_weapons);
    CLEAR_PTR_VEC(hom_armor);
    CLEAR_PTR_VEC(hom_companions);
    CLEAR_PTR_VEC(hom_titles);

    for (const auto& camp : character_completion) {
        delete camp.second;
    }
    character_completion.clear();
}

void CompletionWindow::Draw(IDirect3DDevice9* device)
{
    if (!visible) {
        return;
    }

    // TODO Button at the top to go to current daily
    ImGui::SetNextWindowCenter(ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(min_size[0], min_size[1]), ImVec2(max_size[0], max_size[1]));

    if (!ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {
        ImGui::End();
        return;
    }
    constexpr float tabs_per_row = 4.f;
    const ImVec2 tab_btn_size = {ImGui::GetContentRegionAvail().x / tabs_per_row, 0.f};

    const std::wstring* sel = nullptr;
    if (chosen_player_name_s.empty()) {
        chosen_player_name = GetPlayerName();
        chosen_player_name_s = TextUtils::WStringToString(chosen_player_name);
        CheckProgress();
    }

    const float gscale = ImGui::GetIO().FontGlobalScale;
    ImGui::Text("Choose Character");
    ImGui::SameLine();
    ImGui::PushItemWidth(200.f * gscale);
    if (ImGui::BeginCombo("##completion_character_select", chosen_player_name_s.c_str())) // The second parameter is the label previewed before opening the combo.
    {
        const auto email = GetAccountEmail();
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {2.f, 8.f});
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
            if (it.second->is_pvp || it.second->is_pre_searing)
                continue; // Not applicable

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
    if (ImGui::Button("Change") && wcscmp(GetPlayerName(), chosen_player_name.c_str()) != 0) {
        if (!RerollWindow::Instance().Reroll(chosen_player_name.data(), false, false)) {
            Log::Warning("Failed to reroll to character");
        }
    }
#endif
    ImGui::SameLine();
    if (ImGui::Checkbox("This Account", &only_show_account_chars)) {
        RefreshAccountCharacters();
    }
    ImGui::ShowHelp("Limits the character dropdown to only show the characters belonging to this account.");
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 200.f * gscale);
    ImGui::Checkbox("View as list", &show_as_list);
    ImGui::SameLine();
    if (ImGui::Checkbox("Hard mode", &hard_mode)) {
        CheckProgress();
    }
    ImGui::Separator();
    ImGui::BeginChild("completion_scroll");
    float single_item_width = Mission::icon_size.x;
    if (show_as_list) {
        single_item_width *= 5.f;
    }
    int missions_per_row = static_cast<int>(std::floor(ImGui::GetContentRegionAvail().x / (ImGui::GetIO().FontGlobalScale * single_item_width + ImGui::GetStyle().ItemSpacing.x)));
    const float checkbox_offset = ImGui::GetContentRegionAvail().x - 200.f * ImGui::GetIO().FontGlobalScale;
    auto draw_missions = [missions_per_row, device](auto& camp_missions, size_t end = 0) {
        if (end == 0) {
            end = camp_missions.size();
        }
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
        ImGui::Columns(missions_per_row, "###completion_section_cols", false);
        const size_t items_per_col = static_cast<size_t>(ceil(end / static_cast<float>(missions_per_row)));
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
            if (!camp_missions[i]->Name()[0]) {
                can_sort = false;
            }
        }
        if (!can_sort) {
            return false;
        }
        std::ranges::sort(
            camp_missions,
            [](Mission* lhs, Mission* rhs) {
                return strcmp(lhs->Name(), rhs->Name()) < 0;
            });
        return true;
    };
    if (pending_sort) {
        bool sorted = true;
        for (auto it = missions.begin(); sorted && it != missions.end(); ++it) {
            sorted = sort(it->second);
        }
        for (auto it = outposts.begin(); sorted && it != outposts.end(); ++it) {
            sorted = sort(it->second);
        }
        for (auto it = vanquishes.begin(); sorted && it != vanquishes.end(); ++it) {
            sorted = sort(it->second);
        }
        /*for (auto it = pve_skills.begin(); sorted && it != pve_skills.end(); it++) {
            sorted = sort(it->second);
        }
        for (auto it = elite_skills.begin(); sorted && it != elite_skills.end(); it++) {
            sorted = sort(it->second);
        }*/
        for (auto it = heros.begin(); sorted && it != heros.end(); ++it) {
            sorted = sort(it->second);
        }
        if (sorted) {
            pending_sort = false;
        }
    }
    ImGui::Text("Outposts");
    ImGui::SameLine(checkbox_offset);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {0, 0});
    ImGui::Checkbox("Hide unlocked outposts", &hide_completed_missions);
    ImGui::PopStyleVar();
    for (auto& [campaign, unlockable_outposts] : outposts) {
        size_t completed = 0;
        std::vector<Mission*> filtered;
        for (const auto& outpost : unlockable_outposts) {
            if (outpost->is_completed && outpost->bonus) {
                completed++;
                if (hide_completed_missions) {
                    continue;
                }
            }
            filtered.push_back(outpost);
        }
        char label[128];
        snprintf(label, _countof(label), "%s (%d of %d unlocked) - %.0f%%###campaign_outposts_%d",
                 CampaignName(campaign), completed, unlockable_outposts.size(), static_cast<float>(completed) / static_cast<float>(unlockable_outposts.size()) * 100.f, campaign);
        if (ImGui::CollapsingHeader(label)) {
            draw_missions(filtered);
        }
    }
    ImGui::Text("Missions");
    ImGui::SameLine(checkbox_offset);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {0, 0});
    ImGui::Checkbox("Hide completed missions", &hide_completed_missions);
    ImGui::PopStyleVar();
    for (auto& camp : missions) {
        auto& camp_missions = camp.second;
        size_t completed = 0;
        std::vector<Mission*> filtered;
        for (size_t i = 0; i < camp_missions.size(); i++) {
            if (camp_missions[i]->is_completed && camp_missions[i]->bonus) {
                completed++;
                if (hide_completed_missions) {
                    continue;
                }
            }
            filtered.push_back(camp_missions[i]);
        }
        char label[128];
        snprintf(label, _countof(label), "%s (%d of %d completed) - %.0f%%###campaign_missions_%d", CampaignName(camp.first), completed, camp_missions.size(), static_cast<float>(completed) / static_cast<float>(camp_missions.size()) * 100.f, camp.first);
        if (ImGui::CollapsingHeader(label)) {
            draw_missions(filtered);
        }
    }
    ImGui::Text("Vanquishes");
    ImGui::SameLine(checkbox_offset);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {0, 0});
    ImGui::Checkbox("Hide completed vanquishes", &hide_completed_vanquishes);
    ImGui::PopStyleVar();
    for (auto& camp : vanquishes) {
        auto& camp_missions = camp.second;
        if (!camp_missions.size()) {
            continue;
        }
        size_t completed = 0;
        std::vector<Mission*> filtered;
        for (size_t i = 0; i < camp_missions.size(); i++) {
            if (camp_missions[i]->is_completed) {
                completed++;
                if (hide_completed_vanquishes) {
                    continue;
                }
            }
            filtered.push_back(camp_missions[i]);
        }
        char label[128];
        snprintf(label, _countof(label), "%s (%d of %d completed) - %.0f%%###campaign_vanquishes_%d", CampaignName(camp.first), completed, camp_missions.size(), static_cast<float>(completed) / static_cast<float>(camp_missions.size()) * 100.f,
                 camp.first);
        if (ImGui::CollapsingHeader(label)) {
            draw_missions(filtered);
        }
    }

    auto skills_title = [&, checkbox_offset](const char* title) {
        ImGui::PushID(title);
        ImGui::Text(title);
        ImGui::ShowHelp("Guild Wars only shows skills learned for the current primary/secondary profession.\n\n"
            "GWToolbox remembers skills learned for other professions,\nbut is only able to update this info when you switch to that profession.");
        ImGui::SameLine(checkbox_offset - 100.f);
        if (ImGui::Button("Check Now")) {
            GW::GameThread::Enqueue(CheckAllSkills);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Will cycle through your available secondary professions to detect all unlocked skills");
        }
        ImGui::SameLine(checkbox_offset);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {0, 0});
        ImGui::Checkbox("Hide learned skills", &hide_unlocked_skills);
        ImGui::PopStyleVar();
        ImGui::PopID();
    };
    skills_title("Elite Skills");
    for (auto& camp : elite_skills) {
        auto& camp_missions = camp.second;
        size_t completed = 0;
        std::vector<Mission*> filtered;
        for (size_t i = 0; i < camp_missions.size(); i++) {
            if (camp_missions[i]->is_completed) {
                completed++;
                if (hide_unlocked_skills) {
                    continue;
                }
            }
            filtered.push_back(camp_missions[i]);
        }
        char label[128];
        snprintf(label, _countof(label), "%s (%d of %d completed) - %.0f%%###campaign_eskills_%d", CampaignName(camp.first), completed, camp_missions.size(), static_cast<float>(completed) / static_cast<float>(camp_missions.size()) * 100.f, camp.first);
        if (ImGui::CollapsingHeader(label)) {
            draw_missions(filtered);
        }
    }
    skills_title("PvE Skills");
    for (auto& camp : pve_skills) {
        auto& camp_missions = camp.second;
        size_t completed = 0;
        std::vector<Mission*> filtered;
        for (size_t i = 0; i < camp_missions.size(); i++) {
            if (camp_missions[i]->is_completed) {
                completed++;
                if (hide_unlocked_skills) {
                    continue;
                }
            }
            filtered.push_back(camp_missions[i]);
        }
        char label[128];
        snprintf(label, _countof(label), "%s (%d of %d completed) - %.0f%%###campaign_skills_%d", CampaignName(camp.first), completed, camp_missions.size(), static_cast<float>(completed) / static_cast<float>(camp_missions.size()) * 100.f, camp.first);
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
        snprintf(label, _countof(label), "%s (%d of %d completed) - %.0f%%###campaign_heros_%d", CampaignName(camp.first), completed, camp_missions.size(), static_cast<float>(completed) / static_cast<float>(camp_missions.size()) * 100.f, camp.first);
        if (ImGui::CollapsingHeader(label)) {
            draw_missions(camp_missions);
        }
    }
    ImGui::Text("Unlocked Item Upgrades");
    for (auto& camp : unlocked_pvp_items) {
        auto& camp_missions = camp.second;
        size_t completed = 0;
        for (size_t i = 0; i < camp_missions.size(); i++) {
            if (camp_missions[i]->is_completed) {
                completed++;
            }
        }
        char label[128];
        snprintf(label, _countof(label), "%s (%d of %d unlocked) - %.0f%%###unlocked_pvp_items_%d", (const char*)camp.first, completed, camp_missions.size(), static_cast<float>(completed) / static_cast<float>(camp_missions.size()) * 100.f, camp.first);
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
    std::vector<FestivalHat*> filtered;
    // Halloween hats
    completed = 0;
    to_index = wintersday_index;
    filtered.clear();
    for (size_t i = offset; i < to_index && i < cnt; i++) {
        const auto m = festival_hats[i];
        if (m->is_completed) {
            completed++;
            if (hide_collected_hats) {
                continue;
            }
        }
        filtered.push_back(m);
    }
    snprintf(label, _countof(label), "Halloween Hats (%d of %d collected) - %.0f%%###halloween_hats", completed, to_index - offset, static_cast<float>(completed) / static_cast<float>(to_index - offset) * 100.f);
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
            if (hide_collected_hats) {
                continue;
            }
        }
        filtered.push_back(m);
    }
    snprintf(label, _countof(label), "Wintersday Hats (%d of %d collected) - %.0f%%###wintersday_hats", completed, to_index - offset, static_cast<float>(completed) / static_cast<float>(to_index - offset) * 100.f);
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
            if (hide_collected_hats) {
                continue;
            }
        }
        filtered.push_back(m);
    }
    snprintf(label, _countof(label), "Dragon Festival Hats (%d of %d collected) - %.0f%%###dragon_festival_hats", completed, to_index - offset, static_cast<float>(completed) / static_cast<float>(to_index - offset) * 100.f);
    if (ImGui::CollapsingHeader(label)) {
        draw_missions(filtered);
    }
    offset = to_index;

    DrawHallOfMonuments(device);
    ImGui::EndChild();
    ImGui::End();
}

void CompletionWindow::Update(float)
{
    if (pending_refresh_account_characters) {
        pending_refresh_account_characters = !UpdateRefreshAccountCharacters();
    }
}

void CompletionWindow::DrawHallOfMonuments(IDirect3DDevice9* device)
{
    float single_item_width = Mission::icon_size.x;
    if (show_as_list) {
        single_item_width *= 5.f;
    }
    const int missions_per_row = static_cast<int>(std::floor(ImGui::GetContentRegionAvail().x / (ImGui::GetIO().FontGlobalScale * single_item_width + ImGui::GetStyle().ItemSpacing.x)));
    const float checkbox_offset = ImGui::GetContentRegionAvail().x - 200.f * ImGui::GetIO().FontGlobalScale;
    ImGui::Text("Hall of Monuments");
    ImGui::SameLine(checkbox_offset);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {0, 0});
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
    for (const auto m : minipets) {
        if (m->is_completed) {
            dedicated++;
            if (hide_unlocked_achievements) {
                continue;
            }
        }
        drawn++;
    }

    char label[128];
    snprintf(label, _countof(label), "%s (%d of %d points gained, %d of %d minipets dedicated) - %.0f%%###devotion_points", "Devotion",
             completed, DevotionPoints::TotalAvailable,
             dedicated, minipets.size(),
             static_cast<float>(dedicated) / static_cast<float>(minipets.size()) * 100.f);

    if (ImGui::CollapsingHeader(label)) {
        ImGui::TextDisabled(R"(To update this list, talk to the "Devotion" pedestal in Eye of the North,
then press "Examine the Monument to Devotion.")");
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
        ImGui::Columns(missions_per_row, "###completion_section_cols", false);
        const size_t items_per_col = static_cast<size_t>(ceil(drawn / static_cast<float>(missions_per_row)));
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
                std::sort(minipets.begin(), minipets.end(), [](MinipetAchievement* a, MinipetAchievement* b) { return strcmp(a->Name(), b->Name()) < 0; });
                minipets_sorted = true;
            }
        }

        for (const auto m : minipets) {
            if (m->is_completed && hide_unlocked_achievements) {
                continue;
            }
            if (!m->Draw(device)) {
                continue;
            }
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
    for (const auto m : hom_weapons) {
        if (m->is_completed) {
            dedicated++;
            if (hide_unlocked_achievements) {
                continue;
            }
        }
        drawn++;
    }
    snprintf(label, _countof(label), "%s (%d of %d points gained, %d of %d weapons displayed) - %.0f%%###valor_points", "Valor",
             completed, ValorPoints::TotalAvailable,
             dedicated, hom_weapons.size(),
             static_cast<float>(dedicated) / static_cast<float>(hom_weapons.size()) * 100.f);

    if (ImGui::CollapsingHeader(label)) {
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
        ImGui::Columns(missions_per_row, "###completion_section_cols", false);
        const size_t items_per_col = static_cast<size_t>(ceil(drawn / static_cast<float>(missions_per_row)));
        size_t col_count = 0;

        for (const auto m : hom_weapons) {
            if (m->is_completed && hide_unlocked_achievements) {
                continue;
            }
            if (!m->Draw(device)) {
                continue;
            }
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
    for (const auto m : hom_armor) {
        if (m->is_completed) {
            dedicated++;
            if (hide_unlocked_achievements) {
                continue;
            }
        }
        drawn++;
    }
    snprintf(label, _countof(label), "%s (%d of %d points gained, %d of %d armor sets displayed) - %.0f%%###resilience_points", "Resilience",
             completed, ResiliencePoints::TotalAvailable,
             dedicated, hom_armor.size(),
             static_cast<float>(dedicated) / static_cast<float>(hom_armor.size()) * 100.f);

    if (ImGui::CollapsingHeader(label)) {
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
        ImGui::Columns(missions_per_row, "###completion_section_cols", false);
        const auto items_per_col = static_cast<size_t>(ceil(drawn / static_cast<float>(missions_per_row)));
        size_t col_count = 0;

        for (const auto m : hom_armor) {
            if (m->is_completed && hide_unlocked_achievements) {
                continue;
            }
            if (!m->Draw(device)) {
                continue;
            }
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
    for (const auto m : hom_companions) {
        if (m->is_completed) {
            dedicated++;
            if (hide_unlocked_achievements) {
                continue;
            }
        }
        drawn++;
    }
    snprintf(label, _countof(label), "%s (%d of %d points gained, %d of %d companions displayed) - %.0f%%###fellowship_points", "Fellowship",
             completed, FellowshipPoints::TotalAvailable,
             dedicated, hom_companions.size(),
             static_cast<float>(dedicated) / static_cast<float>(hom_companions.size()) * 100.f);

    if (ImGui::CollapsingHeader(label)) {
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
        ImGui::Columns(missions_per_row, "###completion_section_cols", false);
        const size_t items_per_col = static_cast<size_t>(ceil(drawn / static_cast<float>(missions_per_row)));
        size_t col_count = 0;

        for (const auto m : hom_companions) {
            if (m->is_completed && hide_unlocked_achievements) {
                continue;
            }
            if (!m->Draw(device)) {
                continue;
            }
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
    for (const auto m : hom_titles) {
        if (m->is_completed) {
            dedicated++;
            if (hide_unlocked_achievements) {
                continue;
            }
        }
        drawn++;
    }
    snprintf(label, _countof(label), "%s (%d of %d points gained, %d of %d titles achieved) - %.0f%%###honor_points", "Honor",
             completed, HonorPoints::TotalAvailable,
             dedicated, hom_titles.size(),
             static_cast<float>(dedicated) / static_cast<float>(hom_titles.size()) * 100.f);

    if (ImGui::CollapsingHeader(label)) {
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
        ImGui::Columns(missions_per_row, "###completion_section_cols", false);
        const size_t items_per_col = static_cast<size_t>(ceil(drawn / static_cast<float>(missions_per_row)));
        size_t col_count = 0;

        for (const auto m : hom_titles) {
            if (m->is_completed && hide_unlocked_achievements) {
                continue;
            }
            if (!m->Draw(device)) {
                continue;
            }
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

void CompletionWindow::DrawSettingsInternal()
{
    ToolboxWindow::DrawSettingsInternal();
}

void CompletionWindow::LoadSettings(ToolboxIni* ini)
{
    ToolboxWindow::LoadSettings(ini);
    ToolboxIni completion_ini(false, false, false);
    const auto success = completion_ini.LoadFile(Resources::GetPath(completion_ini_filename).c_str());
    if (success < 0) {
        return Log::Error("Failed to load completion ini");
    }

    LOAD_BOOL(show_as_list);
    LOAD_BOOL(hide_unlocked_skills);
    LOAD_BOOL(hide_completed_vanquishes);
    LOAD_BOOL(hide_completed_missions);
    LOAD_BOOL(hide_unlocked_achievements);
    LOAD_BOOL(hide_collected_hats);
    LOAD_BOOL(only_show_account_chars);

    auto read_ini_to_buf = [&](const CompletionType type, const char* section, const char* ini_section, const std::wstring_view name_ws) {
        char ini_key_buf[64];
        snprintf(ini_key_buf, _countof(ini_key_buf), "%s_length", section);
        const int len = completion_ini.GetLongValue(ini_section, ini_key_buf, 0);
        if (len < 1) {
            return;
        }
        snprintf(ini_key_buf, _countof(ini_key_buf), "%s_values", section);
        const std::string val = completion_ini.GetValue(ini_section, ini_key_buf, "");
        if (val.empty()) {
            return;
        }
        std::vector<uint32_t> completion_buf(len);
        ASSERT(GuiUtils::IniToArray(val, completion_buf.data(), len));
        ParseCompletionBuffer(type, name_ws.data(), completion_buf.data(), completion_buf.size());
    };

    ToolboxIni::TNamesDepend entries;
    completion_ini.GetAllSections(entries);
    for (const ToolboxIni::Entry& entry : entries) {
        const char* ini_section = entry.pItem;
        const auto name_ws = TextUtils::StringToWString(ini_section);

        const auto c = GetCharacterCompletion(name_ws.data(), true);
        c->profession = static_cast<Profession>(completion_ini.GetLongValue(ini_section, "profession", 0));
        c->account = TextUtils::StringToWString(completion_ini.GetValue(ini_section, "account", ""));
        c->is_pvp = completion_ini.GetBoolValue(ini_section, "is_pvp", false);
        c->is_pre_searing = completion_ini.GetBoolValue(ini_section, "is_pre_searing", false);

        read_ini_to_buf(CompletionType::Mission, "mission", ini_section, name_ws);
        read_ini_to_buf(CompletionType::MissionBonus, "mission_bonus", ini_section, name_ws);
        read_ini_to_buf(CompletionType::MissionHM, "mission_hm", ini_section, name_ws);
        read_ini_to_buf(CompletionType::MissionBonusHM, "mission_bonus_hm", ini_section, name_ws);
        read_ini_to_buf(CompletionType::Skills, "skills", ini_section, name_ws);
        read_ini_to_buf(CompletionType::Vanquishes, "vanquishes", ini_section, name_ws);
        read_ini_to_buf(CompletionType::Heroes, "heros", ini_section, name_ws);
        read_ini_to_buf(CompletionType::MapsUnlocked, "maps_unlocked", ini_section, name_ws);
        read_ini_to_buf(CompletionType::MinipetsUnlocked, "minipets_unlocked", ini_section, name_ws);
        read_ini_to_buf(CompletionType::FestivalHats, "festival_hats", ini_section, name_ws);
    }
    RefreshAccountCharacters();
    ParseCompletionBuffer(CompletionType::Mission);
    ParseCompletionBuffer(CompletionType::MissionBonus);
    ParseCompletionBuffer(CompletionType::MissionBonusHM);
    ParseCompletionBuffer(CompletionType::MissionHM);
    ParseCompletionBuffer(CompletionType::Skills);
    ParseCompletionBuffer(CompletionType::Vanquishes);
    ParseCompletionBuffer(CompletionType::Heroes);
    ParseCompletionBuffer(CompletionType::MapsUnlocked);
    CheckProgress();
}

CompletionWindow* CompletionWindow::CheckProgress(const bool fetch_hom)
{
    for (auto& skills : pve_skills | std::views::values) {
        for (const auto& skill : skills) {
            skill->CheckProgress(chosen_player_name);
        }
    }
    for (auto& skills : elite_skills | std::views::values) {
        for (const auto& skill : skills) {
            skill->CheckProgress(chosen_player_name);
        }
    }
    for (auto& skills : outposts | std::views::values) {
        for (const auto& skill : skills) {
            skill->CheckProgress(chosen_player_name);
        }
    }
    for (auto& completed_missions : missions | std::views::values) {
        for (const auto& mission : completed_missions) {
            mission->CheckProgress(chosen_player_name);
        }
    }
    for (auto& completed_missions : vanquishes | std::views::values) {
        for (const auto& mission : completed_missions) {
            mission->CheckProgress(chosen_player_name);
        }
    }
    for (auto& unlocks : heros | std::views::values) {
        for (const auto& unlock : unlocks) {
            unlock->CheckProgress(chosen_player_name);
        }
    }
    for (auto& items : unlocked_pvp_items | std::views::values) {
        for (const auto& item : items) {
            item->CheckProgress(chosen_player_name);
        }
    }
    for (const auto achievement : festival_hats) {
        achievement->CheckProgress(chosen_player_name);
    }
    for (const auto achievement : minipets) {
        achievement->CheckProgress(chosen_player_name);
    }
    for (const auto achievement : hom_weapons) {
        achievement->CheckProgress(chosen_player_name);
    }
    for (const auto achievement : hom_armor) {
        achievement->CheckProgress(chosen_player_name);
    }
    for (const auto achievement : hom_companions) {
        achievement->CheckProgress(chosen_player_name);
    }
    for (const auto achievement : hom_titles) {
        achievement->CheckProgress(chosen_player_name);
    }
    if (fetch_hom) {
        const auto cc = GetCharacterCompletion(chosen_player_name.c_str(), true);
        FetchHom(&cc->hom_achievements);
    }
    return this;
}

void CompletionWindow::SaveSettings(ToolboxIni* ini)
{
    ToolboxWindow::SaveSettings(ini);
    ToolboxIni completion_ini(false, false, false);
    if (character_completion.empty() ||
        (character_completion.size() == 1 && character_completion.contains(L""))) {
        return;
    }

    SAVE_BOOL(show_as_list);
    SAVE_BOOL(hide_unlocked_skills);
    SAVE_BOOL(hide_completed_vanquishes);
    SAVE_BOOL(hide_completed_missions);
    SAVE_BOOL(hide_unlocked_achievements);
    SAVE_BOOL(hide_collected_hats);
    SAVE_BOOL(only_show_account_chars);

    auto write_buf_to_ini = [&completion_ini](const char* section, const std::vector<uint32_t>* read, const std::string_view name) {
        char ini_key_buf[64];
        snprintf(ini_key_buf, _countof(ini_key_buf), "%s_length", section);
        completion_ini.SetLongValue(name.data(), ini_key_buf, read->size());
        std::string ini_str;
        ASSERT(GuiUtils::ArrayToIni(read->data(), read->size(), &ini_str));
        snprintf(ini_key_buf, _countof(ini_key_buf), "%s_values", section);
        completion_ini.SetValue(name.data(), ini_key_buf, ini_str.c_str());
    };

    for (const auto& [entry_name, char_comp] : character_completion) {
        if (entry_name.empty()) {
            continue;
        }
        const std::string& name = char_comp->name_str;
        completion_ini.SetLongValue(name.c_str(), "profession", std::to_underlying(char_comp->profession));
        completion_ini.SetValue(name.c_str(), "account", TextUtils::WStringToString(char_comp->account).c_str());
        completion_ini.SetBoolValue(name.c_str(), "is_pvp", char_comp->is_pvp);
        completion_ini.SetBoolValue(name.c_str(), "is_pre_searing", char_comp->is_pre_searing);

        write_buf_to_ini("mission", &char_comp->mission, name);
        write_buf_to_ini("mission_bonus", &char_comp->mission_bonus, name);
        write_buf_to_ini("mission_hm", &char_comp->mission_hm, name);
        write_buf_to_ini("mission_bonus_hm", &char_comp->mission_bonus_hm, name);
        write_buf_to_ini("skills", &char_comp->skills, name);
        write_buf_to_ini("vanquishes", &char_comp->vanquishes, name);
        write_buf_to_ini("heros", &char_comp->heroes, name);
        write_buf_to_ini("maps_unlocked", &char_comp->maps_unlocked, name);
        write_buf_to_ini("minipets_unlocked", &char_comp->minipets_unlocked, name);
        write_buf_to_ini("festival_hats", &char_comp->festival_hats, name);

        completion_ini.SetValue(name.c_str(), "hom_code", char_comp->hom_code.c_str());
    }
    completion_ini.SaveFile(Resources::GetPath(completion_ini_filename).c_str());
}

CharacterCompletion* CompletionWindow::GetCharacterCompletion(const wchar_t* character_name, const bool create_if_not_found)
{
    if (character_completion.contains(character_name)) {
        return character_completion.at(character_name);
    }
    CharacterCompletion* this_character_completion = nullptr;
    if (create_if_not_found) {
        this_character_completion = new CharacterCompletion();
        this_character_completion->name_str = TextUtils::WStringToString(character_name);
        this_character_completion->hom_achievements.character_name = character_name;
        character_completion[character_name] = this_character_completion;
        FetchHom(&this_character_completion->hom_achievements);
    }
    return this_character_completion;
}

bool CompletionWindow::IsAreaComplete(const MapID map_id, CompletionCheck check)
{
    if (map_id == MapID::None)
        return true;
    if (map_id == MapID::Tomb_of_the_Primeval_Kings)
        return true; // Topk special case

    const auto map = GW::Map::GetMapInfo();
    const auto w = GW::GetWorldContext();
    if (!(map && w))
        return false;
    switch (map->type) {
        case GW::RegionType::EliteMission:
            return true;
        case GW::RegionType::ExplorableZone:
            if (map->continent == GW::Continent::BattleIsles)
                return true; // Fow, Uw
            return !map->GetIsOnWorldMap() || ArrayBoolAt(w->vanquished_areas, static_cast<uint32_t>(map_id));
    }

    if ((check & NormalMode) && !ArrayBoolAt(w->missions_completed, static_cast<uint32_t>(map_id)))
        return false;
    if ((check & HardMode) && !ArrayBoolAt(w->missions_completed_hm, static_cast<uint32_t>(map_id)))
        return false;
    const bool has_bonus = map->campaign != Campaign::EyeOfTheNorth;
    if (has_bonus) {
        if ((check & NormalMode) && !ArrayBoolAt(w->missions_bonus, static_cast<uint32_t>(map_id)))
            return false;
        if ((check & HardMode) && !ArrayBoolAt(w->missions_bonus_hm, static_cast<uint32_t>(map_id)))
            return false;
    }
    return true;
}

bool CompletionWindow::IsAreaComplete(const wchar_t* player_name, const MapID map_id, CompletionCheck check)
{
    return ::IsAreaComplete(player_name, map_id, check, GW::Map::GetMapInfo(map_id));
}

bool CompletionWindow::IsAreaUnlocked(const wchar_t* player_name, const MapID map_id)
{
    const auto completion = GetCharacterCompletion(player_name, false);
    const auto map = completion ? GW::Map::GetMapInfo(map_id) : nullptr;
    if (!(map && completion)) return false;
    return ArrayBoolAt(completion->maps_unlocked, static_cast<uint32_t>(map_id));
}

bool CompletionWindow::IsSkillUnlocked(const wchar_t* player_name, const SkillID skill_id)
{
    const auto completion = GetCharacterCompletion(player_name, false);
    return completion && ArrayBoolAt(completion->skills, static_cast<uint32_t>(skill_id));
}

std::vector<CharacterCompletion*> CompletionWindow::GetCharactersWithoutAreaComplete(MapID map_id, CompletionCheck check)
{
    std::vector<CharacterCompletion*> out;
    if (map_id == MapID::None)
        return out;
    const auto info = GW::Map::GetMapInfo(map_id);
    const auto email = GW::AccountMgr::GetAccountEmail();
    for (auto& it : character_completion) {
        if (it.second->is_pvp || it.second->is_pre_searing)
            continue;
        if (only_show_account_chars && it.second->account != email)
            continue;
        if (!::IsAreaComplete(it.first.c_str(), map_id, check, info))
            out.push_back(it.second);
    }
    std::ranges::sort(out, [](CharacterCompletion* a, CharacterCompletion* b) {
        return a->name_str.compare(b->name_str) < 0;
    });
    return out;
}

std::vector<CharacterCompletion*> CompletionWindow::GetCharactersWithoutAreaUnlocked(MapID map_id)
{
    std::vector<CharacterCompletion*> out;
    const auto email = GW::AccountMgr::GetAccountEmail();
    for (auto& it : character_completion) {
        if (it.second->is_pvp || it.second->is_pre_searing)
            continue;
        if (only_show_account_chars && it.second->account != email)
            continue;
        if (!IsAreaUnlocked(it.first.c_str(), map_id))
            out.push_back(it.second);
    }
    std::ranges::sort(out, [](CharacterCompletion* a, CharacterCompletion* b) {
        return a->name_str.compare(b->name_str) < 0;
    });
    return out;
}

std::vector<CharacterCompletion*> CompletionWindow::GetCharactersWithoutSkillUnlocked(SkillID skill_id)
{
    std::vector<CharacterCompletion*> out;
    const auto email = GW::AccountMgr::GetAccountEmail();
    for (auto& it : character_completion) {
        if (it.second->is_pvp || it.second->is_pre_searing)
            continue;
        if (only_show_account_chars && it.second->account != email)
            continue;
        if (!IsSkillUnlocked(it.first.c_str(), skill_id))
            out.push_back(it.second);
    }
    std::ranges::sort(out, [](CharacterCompletion* a, CharacterCompletion* b) {
        return a->name_str.compare(b->name_str) < 0;
    });
    return out;
}


void MinipetAchievement::CheckProgress(const std::wstring& player_name)
{
    is_completed = false;
    const auto& cc = character_completion;
    if (!cc.contains(player_name)) {
        return;
    }
    const std::vector<uint32_t>& minipets_unlocked = cc.at(player_name)->minipets_unlocked;
    is_completed = bonus = ArrayBoolAt(minipets_unlocked, encoded_name_index);
}

void WeaponAchievement::CheckProgress(const std::wstring& player_name)
{
    is_completed = false;
    const auto& cc = character_completion;
    if (!cc.contains(player_name)) {
        return;
    }
    const auto& hom = cc.at(player_name)->hom_achievements;
    if (hom.state != HallOfMonumentsAchievements::State::Done) {
        return;
    }
    auto& unlocked = hom.valor_detail;
    is_completed = bonus = unlocked[encoded_name_index] != 0;
}

size_t AchieventWithWikiFile::GetLoadedIcons(IDirect3DTexture9* icons_out[4])
{
    if (!icons_loaded && !wiki_file_name.empty()) {
        *icons = Resources::GetGuildWarsWikiImage(wiki_file_name.c_str(), 64);
        icons_loaded = true;
    }
    return Mission::GetLoadedIcons(icons_out);
}

void ArmorAchievement::CheckProgress(const std::wstring& player_name)
{
    is_completed = false;
    const auto& cc = character_completion;
    if (!cc.contains(player_name)) {
        return;
    }
    const auto& hom = cc.at(player_name)->hom_achievements;
    if (hom.state != HallOfMonumentsAchievements::State::Done) {
        return;
    }
    const auto& unlocked = hom.resilience_detail;
    is_completed = bonus = unlocked[encoded_name_index] != 0;
}

void CompanionAchievement::CheckProgress(const std::wstring& player_name)
{
    is_completed = false;
    const auto& cc = character_completion;
    if (!cc.contains(player_name)) {
        return;
    }
    const auto& hom = cc.at(player_name)->hom_achievements;
    if (hom.state != HallOfMonumentsAchievements::State::Done) {
        return;
    }
    const auto& unlocked = hom.fellowship_detail;
    is_completed = bonus = unlocked[encoded_name_index] != 0;
}

void HonorAchievement::CheckProgress(const std::wstring& player_name)
{
    is_completed = false;
    const auto& cc = character_completion;
    if (!cc.contains(player_name)) {
        return;
    }
    const auto& hom = cc.at(player_name)->hom_achievements;
    if (hom.state != HallOfMonumentsAchievements::State::Done) {
        return;
    }
    const auto& unlocked = hom.honor_detail;
    is_completed = bonus = unlocked[encoded_name_index] != 0;
}

void FestivalHat::CheckProgress(const std::wstring& player_name)
{
    is_completed = false;
    const auto& cc = character_completion;
    if (!cc.contains(player_name)) {
        return;
    }
    const std::vector<uint32_t>& unlocked = cc.at(player_name)->festival_hats;
    is_completed = bonus = ArrayBoolAt(unlocked, encoded_name_index);
}

size_t UnlockedPvPItemUpgrade::GetLoadedIcons(IDirect3DTexture9* icons_out[4])
{
    if (!icons_loaded) {
        const auto info = GW::Items::GetPvPItemUpgrade(encoded_name_index);
        if (info) {
            const auto found = std::ranges::find_if(item_upgrades_by_file_id, [file_id = info->file_id](auto& check) { return check.file_id == file_id; });
            if (found != item_upgrades_by_file_id.end()) {
                *icons = Resources::GetGuildWarsWikiImage(found->wiki_filename);
            }
        }
        icons_loaded = true;
    }
    return Mission::GetLoadedIcons(icons_out);
}

void UnlockedPvPItemUpgrade::CheckProgress(const std::wstring&)
{
    const auto acc = GW::GetAccountContext();
    is_completed = false;
    if (acc) {
        is_completed = bonus = ArrayBoolAt(acc->unlocked_pvp_items, encoded_name_index);
    }
}

void UnlockedPvPItemUpgrade::LoadStrings()
{
    const auto info = GW::Items::GetPvPItemUpgrade(encoded_name_index);
    if (!info) {
        return;
    }
    if (name.encoded().empty()) {
        wchar_t* tmp;
        if (GW::Items::GetPvPItemUpgradeEncodedName(encoded_name_index, &tmp)) {
            name.reset(tmp);
            name.wstring();
        }
    }
    if (single_item_name.encoded().empty()) {
        single_item_name.reset(info->name_id);
        single_item_name.wstring();
    }
}

const char* UnlockedPvPItemUpgrade::Name()
{
    LoadStrings();
    return ItemAchievement::Name();
}

void UnlockedPvPItemUpgrade::OnClick()
{
    const auto category = (void*)PvPItemUpgradeTypeName(encoded_name_index);
    const wchar_t* wiki_url = nullptr;
    if (category == completion_category_weapon_upgrades) {
        wiki_url = L"Upgrade component";
    }
    if (category == completion_category_inscriptions) {
        wiki_url = L"Inscription";
    }
    if (category == completion_category_runes_insignias) {
        wiki_url = L"Rune";
    }
    if (wiki_url) {
        GW::GameThread::Enqueue([wiki_url] {
            GuiUtils::OpenWiki(wiki_url);
        });
    }
}
