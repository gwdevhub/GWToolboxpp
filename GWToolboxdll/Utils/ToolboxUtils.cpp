#include "stdafx.h"

#include <GWCA/Context/CharContext.h>
#include <GWCA/Context/WorldContext.h>
#include <GWCA/Context/PartyContext.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Party.h>
#include <GWCA/GameEntities/Player.h>
#include <GWCA/GameEntities/Hero.h>
#include <GWCA/GameEntities/Skill.h>
#include <GWCA/GameEntities/Friendslist.h>
#include <GWCA/GameEntities/Item.h>
#include <GWCA/GameEntities/Map.h>

#include <GWCA/Managers/PlayerMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Managers/MemoryMgr.h>

#include <GWCA/Packets/Opcodes.h>
#include <GWCA/Packets/StoC.h>

#include <GWCA/Managers/FriendListMgr.h>

#include "ToolboxUtils.h"
#include <GWCA/Utilities/Scanner.h>
#include <Utils/TextUtils.h>
#include <GWCA/Context/MapContext.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <Timer.h>
#include <GWCA/GameEntities/Frame.h>

namespace {
    typedef UUID* (__cdecl*PortalGetUserId_pt)();
    PortalGetUserId_pt PortalGetUserId_Func = 0;

    GW::Array<GW::AvailableCharacterInfo>* available_chars_ptr = nullptr;

    bool IsInfused(const GW::Item* item)
    {
        return item && item->info_string && wcschr(item->info_string, 0xAC9);
    }

    GW::UI::Frame* GetSelectorFrame()
    {
        return GW::UI::GetFrameByLabel(L"Selector");
    }

}

namespace GW {
    void WaitForFrame(const wchar_t* frame_label, OnGotFrame_Callback callback)
    {
        if (const auto frame = GW::UI::GetFrameByLabel(frame_label)) {
            callback(frame);
        }
        else {
            GW::UI::RegisterCreateUIComponentCallback((GW::HookEntry*)callback, [frame_label, callback](GW::UI::CreateUIComponentPacket* packet) {
                if (packet && packet->component_label && wcscmp(packet->component_label, frame_label) == 0) {
                    const auto frame = GW::UI::GetFrameByLabel(frame_label);
                    ASSERT(frame);
                    callback(frame);
                    GW::GameThread::Enqueue([callback]() {
                        GW::UI::RemoveCreateUIComponentCallback((GW::HookEntry*)callback);
                    });
                }
            }, 0x4000);
        }
    }

    namespace Map {
        bool GetMapWorldMapBounds(GW::AreaInfo* map, ImRect* out)
        {
            if (!map) return false;
            auto bounds = &map->icon_start_x;
            if (*bounds == 0)
                bounds = &map->icon_start_x_dupe;

            // NB: Even though area info holds map bounds as uints, the world map uses signed floats anyway - a cast should be fine here.
            *out = {
                {static_cast<float>(bounds[0]), static_cast<float>(bounds[1])},
                {static_cast<float>(bounds[2]), static_cast<float>(bounds[3])}
            };
            return true;
        }

        void PingCompass(const GW::GamePos& position)
        {
            constexpr float compass_scale = 96.f;
            GW::GameThread::Enqueue([cpy = position]() {
                GW::UI::CompassPoint point({std::lroundf(cpy.x / compass_scale), std::lroundf(cpy.y / compass_scale)});
                GW::UI::UIPacket::kCompassDraw packet = {
                    .player_number = GW::PlayerMgr::GetPlayerNumber(),
                    .session_id = (uint32_t)TIMER_INIT(),
                    .number_of_points = 1,
                    .points = &point
                };
                GW::UI::SendUIMessage(GW::UI::UIMessage::kCompassDraw, &packet);
            });
        }

        GW::Array<GW::MapProp*>* GetMapProps()
        {
            const auto m = GetMapContext();
            const auto p = m ? m->props : nullptr;
            return p ? &p->propArray : nullptr;
        }
    } // namespace Map
    namespace LoginMgr {
        const bool IsCharSelectReady() {
            return GW::UI::GetFrameContext(GetSelectorFrame());
        }

        const bool SelectCharacterToPlay(const wchar_t* name, bool play)
        {
            const auto selector = GetSelectorFrame();
            if (!(selector && GW::UI::GetFrameContext(selector))) return false;

            if (!AccountMgr::GetAvailableCharacter(name)) return false;
            GW::UI::UIPacket::kMouseAction action{};
            action.frame_id = selector->frame_id;
            action.child_offset_id = selector->child_offset_id;
            struct button_param {
                const wchar_t* name;
                uint32_t play;
                int h0008 = -1;
            };
            button_param wparam = {name, 0u,-1}; // NB: We'll explicitly play in a bit
            action.wparam = &wparam;
            action.current_state = 0x7;
            if (!GW::UI::SendFrameUIMessage(GW::UI::GetParentFrame(selector), GW::UI::UIMessage::kMouseClick2, &action)) 
                return false;
            return (!play || GW::UI::ButtonClick(GW::UI::GetFrameByLabel(L"Play")));
        }
    }


    namespace PartyMgr {
        GW::PlayerPartyMemberArray* GetPartyPlayers(uint32_t party_id)
        {
            const auto party = GW::PartyMgr::GetPartyInfo(party_id);
            return party ? &party->players : nullptr;
        }

        size_t GetPlayerPartyIndex(uint32_t player_number, uint32_t party_id)
        {
            const auto players = GetPartyPlayers(party_id);
            if (!players) return 0;
            for (size_t i = 0, size = players->size(); i < size; i++) {
                if (players->at(i).login_number == player_number)
                    return i + 1;
            }
            return 0;
        }

        uint32_t GetPartyMemberAgentId(uint32_t party_member_index, uint32_t party_id)
        {
            uint32_t current_idx = (uint32_t)-1;
            const auto party = GW::PartyMgr::GetPartyInfo(party_id);
            if (!party) return 0;
            for (const auto& player_member : party->players) {
                current_idx++;
                if (current_idx == party_member_index) {
                    const auto player = GW::PlayerMgr::GetPlayerByID(player_member.login_number);
                    return player ? player->agent_id : 0;
                }
                for (const auto& hero : party->heroes) {
                    if (hero.owner_player_id != player_member.login_number)
                        continue;
                    current_idx++;
                    if (current_idx == party_member_index) {
                        return hero.agent_id;
                    }
                }
            }
            for (const auto& hench_member : party->henchmen) {
                current_idx++;
                if (current_idx == party_member_index) {
                    return hench_member.agent_id;
                }
            }
            return 0;
        }
    }

    namespace AccountMgr {
        GW::Array<AvailableCharacterInfo>* GetAvailableChars()
        {
            if (!available_chars_ptr) {
                const auto address = GW::Scanner::Find("\x8b\x35\x00\x00\x00\x00\x57\x69\xF8\x84\x00\x00\x00", "xx????xxxxxxx", 0x2);
#ifdef _DEBUG
                ASSERT(address);
#endif
                if (address) {
                    available_chars_ptr = *(GW::Array<AvailableCharacterInfo>**)address;
                }

            }
            return available_chars_ptr;
        }

        const wchar_t* GetCurrentPlayerName()
        {
            const auto c = GetCharContext();
            return c ? c->player_name : nullptr;
        }

        const wchar_t* GetAccountEmail()
        {
            const auto c = GetCharContext();
            return c ? c->player_email : nullptr;
        }

        const UUID* GetPortalAccountUuid()
        {
            if (!PortalGetUserId_Func) {
                HMODULE hPortalDll = GetModuleHandle("GwLoginClient.dll");
                PortalGetUserId_Func = hPortalDll ? (PortalGetUserId_pt)GetProcAddress(hPortalDll, "PortalGetUserId") : nullptr;
            }
            return PortalGetUserId_Func ? PortalGetUserId_Func() : nullptr;
        }

        AvailableCharacterInfo* GetAvailableCharacter(const wchar_t* name)
        {
            const auto characters = name ? GetAvailableChars() : nullptr;
            if (!characters)
                return nullptr;
            for (auto& ac : *characters) {
                if (wcscmp(ac.player_name, name) == 0)
                    return &ac;
            }
            return nullptr;
        }
    }

    namespace MemoryMgr {
        bool GetPersonalDir(std::wstring& out)
        {
            out.resize(512, 0);
            if (!GetPersonalDir(out.capacity(), out.data()))
                return false;
            out.resize(wcslen(out.data()));
            return !out.empty();
        }
    }

    namespace UI {
        void AsyncDecodeStr(const wchar_t* enc_str, std::wstring* out, GW::Constants::Language language_id)
        {
            out->clear();
            AsyncDecodeStr(enc_str, [](void* param, const wchar_t* s) {
                *(std::wstring*)param = s;
            }, out, language_id);
        }
        bool BelongsToFrame(GW::UI::Frame* parent, GW::UI::Frame* child) {
            while (child && parent) {
                if (child == parent) {
                    return true;
                }
                child = GetParentFrame(child);
            }
            return false;
        }
    }

    namespace Agents {
        void AsyncGetAgentName(const Agent* agent, std::wstring& out)
        {
            UI::AsyncDecodeStr(GetAgentEncName(agent), &out);
        }
    }
}

namespace ToolboxUtils {
    bool ArrayBoolAt(const GW::Array<uint32_t>& array, const uint32_t index)
    {
        const uint32_t real_index = index / 32;
        if (real_index >= array.size()) {
            return false;
        }
        const uint32_t shift = index % 32;
        const uint32_t flag = 1 << shift;
        const auto res = (array[real_index] & flag);
        return res != 0;
    }

    uint8_t GetMissionState(GW::Constants::MapID map_id, const GW::Array<uint32_t>& missions_completed, const GW::Array<uint32_t>& missions_bonus)
    {
        const auto area_info = GW::Map::GetMapInfo(map_id);
        if (!area_info)
            return 0;
        switch (area_info->type) {
            case GW::RegionType::CooperativeMission:
            case GW::RegionType::MissionOutpost:
            case GW::RegionType::Dungeon:
                break;
            default:
                return 0;
        }
        uint8_t state_out = 0;
        auto complete = ArrayBoolAt(missions_completed, (uint32_t)map_id);
        auto bonus = ArrayBoolAt(missions_bonus, (uint32_t)map_id);

        auto primary = complete;
        auto expert = bonus;
        auto master = false;

        switch (area_info->campaign) {
            case GW::Constants::Campaign::Factions:
            case GW::Constants::Campaign::Nightfall:
                // Master = bonus, Expert = complete, Primary = any
                master = bonus;
                expert = complete;
                primary = master || expert;
                break;
        }

        if (primary)
            state_out |= MissionState::Primary;
        if (expert)
            state_out |= MissionState::Expert;
        if (master)
            state_out |= MissionState::Master;
        return state_out;
    }

    uint8_t GetMissionState(GW::Constants::MapID map_id, bool is_hard_mode)
    {
        const auto w = GW::GetWorldContext();
        const GW::Array<uint32_t>* missions_completed = &w->missions_completed;
        const GW::Array<uint32_t>* missions_bonus = &w->missions_bonus;
        if (is_hard_mode) {
            missions_completed = &w->missions_completed_hm;
            missions_bonus = &w->missions_bonus_hm;
        }
        return GetMissionState(map_id, *missions_completed, *missions_bonus);
    }

    uint8_t GetMissionState()
    {
        return GetMissionState(GW::Map::GetMapID(), GW::PartyMgr::GetIsPartyInHardMode());
    }

    bool IsOutpost()
    {
        return GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost;
    }

    bool IsExplorable()
    {
        return GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable;
    }

    GW::Player* GetPlayerByName(const wchar_t* _name)
    {
        if (!_name) {
            return nullptr;
        }
        const std::wstring name = TextUtils::SanitizePlayerName(_name);
        GW::PlayerArray* players = GW::PlayerMgr::GetPlayerArray();
        if (!players) {
            return nullptr;
        }
        for (GW::Player& player : *players) {
            if (!player.name) {
                continue;
            }
            if (name == TextUtils::SanitizePlayerName(player.name)) {
                return &player;
            }
        }
        return nullptr;
    }

    std::wstring GetPlayerName(const uint32_t player_number)
    {
        const GW::Player* player = nullptr;
        if (!player_number) {
            player = GW::PlayerMgr::GetPlayerByID(GW::PlayerMgr::GetPlayerNumber());
            if (!player || !player->name) {
                // Map not loaded; try to get from character context
                const auto c = GW::GetCharContext();
                return c ? c->player_name : L"";
            }
        }
        else {
            player = GW::PlayerMgr::GetPlayerByID(player_number);
        }
        return player && player->name ? TextUtils::SanitizePlayerName(player->name) : L"";
    }

    GW::Array<wchar_t>* GetMessageBuffer()
    {
        auto* w = GW::GetWorldContext();
        return w && w->message_buff.valid() ? &w->message_buff : nullptr;
    }

    const wchar_t* GetMessageCore()
    {
        auto* buf = GetMessageBuffer();
        return buf ? buf->begin() : nullptr;
    }

    bool ClearMessageCore()
    {
        auto* buf = GetMessageBuffer();
        if (!buf) {
            return false;
        }
        buf->clear();
        return true;
    }

    const std::wstring GetSenderFromPacket(GW::Packet::StoC::PacketBase* packet)
    {
        switch (packet->header) {
            case GAME_SMSG_CHAT_MESSAGE_GLOBAL: {
                const auto p = static_cast<GW::Packet::StoC::MessageGlobal*>(packet);
                return p->sender_name;
            }
            case GAME_SMSG_CHAT_MESSAGE_LOCAL:
            case GAME_SMSG_TRADE_REQUEST:
                return GetPlayerName(((uint32_t*)packet)[1]);
        }
        return L"";
    }

    GW::Player* GetPlayerByAgentId(const uint32_t agent_id, GW::AgentLiving** info_out)
    {
        const auto agent = static_cast<GW::AgentLiving*>(GW::Agents::GetAgentByID(agent_id));
        if (!(agent && agent->GetIsLivingType() && agent->IsPlayer())) {
            return nullptr;
        }
        if (info_out) {
            *info_out = agent;
        }
        return GW::PlayerMgr::GetPlayerByID(agent->login_number);
    }

    bool IsHenchman(const uint32_t agent_id)
    {
        if (!IsOutpost()) {
            return IsHenchmanInParty(agent_id);
        }
        const auto w = GW::GetWorldContext();
        if (!(w && w->henchmen_agent_ids.size())) {
            return false;
        }
        for (const auto a : w->henchmen_agent_ids) {
            if (a == agent_id) {
                return true;
            }
        }
        return false;
    }

    bool IsHero(const uint32_t agent_id, GW::HeroInfo** info_out)
    {
        if (!IsOutpost()) {
            // NB: HeroInfo array is only populated in outposts
            return IsHeroInParty(agent_id);
        }
        const auto w = GW::GetWorldContext();
        if (!(w && w->hero_info.size())) {
            return false;
        }
        for (auto& a : w->hero_info) {
            if (a.agent_id == agent_id) {
                if (info_out) {
                    *info_out = &a;
                }
                return true;
            }
        }
        return false;
    }


    GW::Array<GW::PartyInfo*>* GetParties()
    {
        auto* p = GW::GetPartyContext();
        return p ? &p->parties : nullptr;
    }

    const GW::HenchmanPartyMember* GetHenchmanPartyMember(const uint32_t agent_id, GW::PartyInfo** party_out)
    {
        const auto* parties = GetParties();
        if (!parties) {
            return nullptr;
        }
        for (const auto party : *parties) {
            if (!party) {
                continue;
            }
            for (const auto& p : party->henchmen) {
                if (p.agent_id != agent_id) {
                    continue;
                }
                if (party_out) {
                    *party_out = party;
                }
                return &p;
            }
        }
        return nullptr;
    }

    bool IsHenchmanInParty(const uint32_t agent_id)
    {
        GW::PartyInfo* party = nullptr;
        return GetHenchmanPartyMember(agent_id, &party) && party == GW::PartyMgr::GetPartyInfo();
    }

    const GW::HeroPartyMember* GetHeroPartyMember(const uint32_t agent_id, GW::PartyInfo** party_out)
    {
        const auto* parties = GetParties();
        if (!parties) {
            return nullptr;
        }
        for (const auto party : *parties) {
            if (!party) {
                continue;
            }
            for (const auto& p : party->heroes) {
                if (p.agent_id != agent_id) {
                    continue;
                }
                if (party_out) {
                    *party_out = party;
                }
                return &p;
            }
        }
        return nullptr;
    }

    bool IsHeroInParty(const uint32_t agent_id)
    {
        GW::PartyInfo* party = nullptr;
        return GetHeroPartyMember(agent_id, &party) && party == GW::PartyMgr::GetPartyInfo();
    }

    const GW::PlayerPartyMember* GetPlayerPartyMember(const uint32_t login_number, GW::PartyInfo** party_out)
    {
        const auto* parties = GetParties();
        if (!parties) {
            return nullptr;
        }
        for (const auto party : *parties) {
            if (!party) {
                continue;
            }
            for (const auto& p : party->players) {
                if (p.login_number != login_number) {
                    continue;
                }
                if (party_out) {
                    *party_out = party;
                }
                return &p;
            }
        }
        return nullptr;
    }

    bool IsPlayerInParty(const uint32_t login_number)
    {
        const auto* party = GW::PartyMgr::GetPartyInfo();
        if (!party) {
            return false;
        }

        for (const auto& player : party->players) {
            if (player.login_number == login_number) {
                return true;
            }
        }

        return false;
    }

    bool IsAgentInParty(const uint32_t agent_id)
    {
        const auto* party = GW::PartyMgr::GetPartyInfo();
        if (!party) {
            return false;
        }
        if (IsHenchmanInParty(agent_id) || IsHeroInParty(agent_id)) {
            return true;
        }
        const auto player = GetPlayerByAgentId(agent_id);
        return player && IsPlayerInParty(player->player_number);
    }

    float GetSkillRange(const GW::Constants::SkillID skill_id)
    {
        const auto skill = GW::SkillbarMgr::GetSkillConstantData(skill_id);
        if (!skill) {
            return 0.f;
        }
        if (skill->IsTouchRange()) {
            return GW::Constants::Range::Touch;
        }
        if (skill->IsHalfRange()) {
            return GW::Constants::Range::Spellcast / 2.f;
        }
        return GW::Constants::Range::Spellcast;
    }

    // Helper function; avoids doing string checks on offline friends.
    GW::Friend* GetFriend(const wchar_t* account, const wchar_t* playing, const GW::FriendType type, const GW::FriendStatus status)
    {
        if (!(account || playing)) {
            return nullptr;
        }
        GW::FriendList* fl = GW::FriendListMgr::GetFriendList();
        if (!fl) {
            return nullptr;
        }
        uint32_t n_friends = fl->number_of_friend, n_found = 0;
        GW::FriendsListArray& friends = fl->friends;
        for (GW::Friend* it : friends) {
            if (n_found == n_friends) {
                break;
            }
            if (!it) {
                continue;
            }
            if (it->type != type) {
                continue;
            }
            n_found++;
            if (it->status != status) {
                continue;
            }
            if (account && !wcsncmp(it->alias, account, 20)) {
                return it;
            }
            if (playing && !wcsncmp(it->charname, playing, 20)) {
                return it;
            }
        }
        return nullptr;
    }

    std::wstring ShorthandItemDescription(GW::Item* item)
    {
        std::wstring original(item->info_string);
        wchar_t buffer[128];

        // For armor items, include full item name and a few description bits.
        switch (item->type) {
            case GW::Constants::ItemType::Headpiece:
            case GW::Constants::ItemType::Boots:
            case GW::Constants::ItemType::Chestpiece:
            case GW::Constants::ItemType::Gloves:
            case GW::Constants::ItemType::Leggings: {
                original = item->complete_name_enc;
                const std::wstring_view item_str(item->info_string);

                static constexpr ctll::fixed_string stacking_pattern = L"\x2.\x10A\xA84\x10A(.{1,2})\x1\x101\x101\x1\x2\xA3E\x10A\xAA8\x10A\xAB1\x1\x1";
                if (const auto stacking_match = ctre::match<stacking_pattern>(item_str)) {
                    auto capture = stacking_match.get<1>().to_view();
                    swprintf(buffer, _countof(buffer), L"\x2\xAA8\x10A\xA84\x10A%ls\x1\x101\x101\x1", capture.data());
                    original += buffer;
                }

                static constexpr ctll::fixed_string armor_pattern = L"\xA3B\x10A\xA86\x10A\xA44\x1\x101(.)\x1\x2";
                if (auto armor_match = ctre::match<armor_pattern>(item_str)) {
                    auto capture = armor_match.get<1>().to_view();
                    swprintf(buffer, _countof(buffer), L"\x2\x102\x2\xA86\x10A\xA44\x1\x101%ls", capture.data());
                    original += buffer;
                }

                if (IsInfused(item)) {
                    original += L"\x2\x102\x2\xAC9";
                }
                return original;
            }
            default:
                break;
        }

        // Replace "Requires 9 Divine Favor" > "q9 Divine Favor"
        original = TextUtils::ctre_regex_replace_with_formatter<L".\x10A\x0AA8\x10A\xAA9\x10A.\x1\x101.\x1\x1">(
            original,
            [&buffer](auto& match) -> std::wstring {
                auto found = match.get<0>().to_view();
                swprintf(buffer, _countof(buffer), L"\x108\x107, q%d \x1\x2%c", found.at(9) - 0x100, found.at(6));
                return buffer;
            });

        // Replace "Requires 9 Scythe Mastery" > "q9 Scythe Mastery"
        original = TextUtils::ctre_regex_replace_with_formatter<L".\x10A\xAA8\x10A\xAA9\x10A\x8101.\x1\x101.\x1\x1">(
            original,
            [&buffer](auto& match) -> std::wstring {
                auto found = match.get<0>().to_view();
                swprintf(buffer, _countof(buffer), L"\x108\x107, q%d \x1\x2\x8101%c", found.at(10) - 0x100, found.at(7));
                return buffer;
            });

        // "vs. Earth damage" > "Earth"
        original = TextUtils::ctre_regex_replace_with_formatter<L"[\xAAC\xAAF]\x10A.\x1">(
            original,
            [&buffer](auto& match) -> std::wstring {
                auto found = match.get<0>().to_view();
                swprintf(buffer, _countof(buffer), L"%c", found.at(2));
                return buffer;
            });

        // Replace "Lengthens ??? duration on foes by 33%" > "??? duration +33%"
        original = TextUtils::ctre_regex_replace_with_formatter<L"\xAA4\x10A.\x1">(
            original,
            [&buffer](auto& match) -> std::wstring {
                auto found = match.get<0>().to_view();
                swprintf(buffer, _countof(buffer), L"%c\x2\x108\x107 +33%%\x1", found.at(2));
                return buffer;
            });

        // Replace "Reduces ??? duration on you by 20%" > "??? duration -20%"
        original = TextUtils::ctre_regex_replace_with_formatter<L"\xAA7\x10A.\x1">(
            original,
            [&buffer](auto& match) -> std::wstring {
                auto found = match.get<0>().to_view();
                swprintf(buffer, _countof(buffer), L"%c\x2\x108\x107 -20%%\x1", found.at(2));
                return buffer;
            });

        // Change " (while Health is above n)" to "^n";
        original = TextUtils::ctre_regex_replace_with_formatter<L"\xAA8\x10A\xABC\x10A\xA52\x1\x101.\x1">(
            original,
            [&buffer](auto& match) -> std::wstring {
                auto found = match.get<0>().to_view();
                swprintf(buffer, _countof(buffer), L"\x108\x107^%d\x1", found.at(7) - 0x100);
                return buffer;
            });

        // Change "Enchantments last 20% longer" to "Ench +20%"
        original = TextUtils::ctre_regex_replace_with_formatter<L"\xAA2\x101.">(
            original,
            [&buffer](auto& match) -> std::wstring {
                auto found = match.get<0>().to_view();
                swprintf(buffer, _countof(buffer), L"\x108\x107" L"Enchantments +%d%%\x1", found.at(2) - 0x100);
                return buffer;
            });

        // "(Chance: 18%)" > "(18%)"
        original = TextUtils::ctre_regex_replace_with_formatter<L"\xA87\x10A\xA48\x1\x101.">(
            original,
            [&buffer](auto& match) -> std::wstring {
                auto found = match.get<0>().to_view();
                swprintf(buffer, _countof(buffer), L"\x108\x107%d%%\x1", found.at(5) - 0x100);
                return buffer;
            });

        // Change "Halves skill recharge of <attribute> spells" > "HSR <attribute>"
        original = TextUtils::ctre_regex_replace_with_formatter<L"\xA81\x10A\xA58\x1\x10B.\x1">(
            original,
            [&buffer](auto& match) -> std::wstring {
                auto found = match.get<0>().to_view();
                swprintf(buffer, _countof(buffer), L"\x108\x107" L"HSR \x1\x2%c", found.at(5));
                return buffer;
            });

        // Change "Inscription: "Blah Blah"" to just "Blah Blah"
        original = TextUtils::ctre_regex_replace_with_formatter<L"\x8101\x5DC5\x10A..\x1">(
            original,
            [&buffer](auto& match) -> std::wstring {
                auto found = match.get<0>().to_view();
                swprintf(buffer, _countof(buffer), L"%c%c", found.at(3), found.at(4));
                return buffer;
            });

        // Change "Halves casting time of <attribute> spells" > "HCT <attribute>"
        original = TextUtils::ctre_regex_replace_with_formatter<L"\xA81\x10A\xA47\x1\x10B.\x1">(
            original,
            [&buffer](auto& match) -> std::wstring {
                auto found = match.get<0>().to_view();
                swprintf(buffer, _countof(buffer), L"\x108\x107" L"HCT \x1\x2%c", found.at(5));
                return buffer;
            });

        // Change "Piercing Dmg: 11-22" > "Piercing: 11-22"
        original = TextUtils::ctre_regex_replace_with_formatter<L"\xA89\x10A\xA4E\x1\x10B.\x1\x101.\x102.">(
            original,
            [&buffer](auto& match) -> std::wstring {
                auto found = match.get<0>().to_view();
                swprintf(buffer, _countof(buffer), L"%c\x2\x108\x107: %d-%d\x1", found.at(5), found.at(8) - 0x100, found.at(10) - 0x100);
                return buffer;
            });

        // Change "Life draining -3, Health regeneration -1" > "Vampiric" (add at end of description)
        static constexpr ctll::fixed_string vampiric_pattern = L"\x2\x102\x2.\x10A\xA86\x10A\xA54\x1\x101.\x1\x2\x102\x2.\x10A\xA7E\x10A\xA53\x1\x101.\x1";
        if (ctre::match<vampiric_pattern>(original)) {
            original = TextUtils::ctre_regex_replace<vampiric_pattern, L"">(original);
            original += L"\x2\x102\x2\x108\x107" L"Vampiric\x1";
        }

        // Change "Energy gain on hit 1, Energy regeneration -1" > "Zealous" (add at end of description)
        static constexpr ctll::fixed_string zealous_pattern = L"\x2\x102\x2.\x10A\xA86\x10A\xA50\x1\x101.\x1\x2\x102\x2.\x10A\xA7E\x10A\xA51\x1\x101.\x1";
        if (ctre::match<zealous_pattern>(original)) {
            original = TextUtils::ctre_regex_replace<zealous_pattern, L"">(original);
            original += L"\x2\x102\x2\x108\x107" L"Zealous\x1";
        }

        // Change "Damage" > "Dmg"
        original = TextUtils::str_replace_all(original, L"\xA4C", L"\xA4E");

        // Change Bow "Two-Handed" > ""
        original = TextUtils::str_replace_all(original, L"\x8102\x1227", L"\xA3E");

        // Change "Halves casting time of spells" > "HCT"
        original = TextUtils::str_replace_all(original, L"\xA80\x10A\xA47\x1", L"\x108\x107" L"HCT\x1");

        // Change "Halves skill recharge of spells" > "HSR"
        original = TextUtils::str_replace_all(original, L"\xA80\x10A\xA58\x1", L"\x108\x107" "HSR\x1");

        // Remove (Stacking) and (Non-stacking) rubbish
        original = TextUtils::ctre_regex_replace<L"\x2.\x10A\xAA8\x10A[\xAB1\xAB2]\x1\x1", L"">(original);

        // Replace (while affected by a(n) to just (n)
        original = TextUtils::ctre_regex_replace_with_formatter<L"\x8101\x4D9C\x10A.\x1">(
            original,
            [](auto& match) -> std::wstring {
                auto found = match.get<0>().to_view();
                return std::wstring(1, found.at(3));
            });

        // Replace (while xxx) to just (xxx)
        original = TextUtils::str_replace_all(original, L"\xAB4", L"\x108\x107" L"Attacking\x1");
        original = TextUtils::str_replace_all(original, L"\xAB5", L"\x108\x107" L"Casting\x1");
        original = TextUtils::str_replace_all(original, L"\xAB6", L"\x108\x107" L"Condition\x1");
        original = TextUtils::ctre_regex_replace<L"[\xAB7\x4B6]", L"\x108\x107" L"Enchanted\x1">(original);
        original = TextUtils::ctre_regex_replace<L"[\xAB8\x4B4]", L"\x108\x107" L"Hexed\x1">(original);
        original = TextUtils::ctre_regex_replace<L"[\xAB9\xABA]", L"\x108\x107" L"Stance\x1">(original);

        // Combine Attribute + 3, Attribute + 1 to Attribute +3 +1 (e.g. headpiece)
        original = TextUtils::ctre_regex_replace_with_formatter<L".\x10A\xA84\x10A.\x1\x101.\x1\x2\x102\x2.\x10A\xA84\x10A.\x1\x101.\x1">(
            original,
            [&buffer](auto& match) -> std::wstring {
                auto found = match.get<0>().to_view();
                if (found[4] != found[16]) {
                    return std::wstring(found); // Different attributes, return unchanged
                }
                swprintf(buffer, _countof(buffer), L"%c\x10A\xA84\x10A%c\x1\x101%c\x2\xA84\x101%c\x1",
                         found[0], found[4], found[7], found[19]);
                return buffer;
            });

        // Remove "Value: 122 gold"
        original = TextUtils::ctre_regex_replace<L"\x2\x102\x2\xA3E\x10A\xA8A\x10A\xA59\x1\x10B.\x101.(\x102.)?\x1\x1", L"">(original);

        // Remove other "greyed" generic terms e.g. "Two-Handed", "Unidentified"
        original = TextUtils::ctre_regex_replace<L"\x2\x102\x2\xA3E\x10A.\x1", L"">(original);

        // Remove "Necromancer Munne sometimes gives these to me in trade" etc
        original = TextUtils::ctre_regex_replace<L"\x2\x102\x2.\x10A\x8102.\x1", L"">(original);

        // Remove "Inscription: None"
        original = TextUtils::ctre_regex_replace<L"\x2\x102\x2.\x10A\x8101\x5A1F\x1", L"">(original);

        // Remove "Crafted in tribute to an enduring legend." etc
        original = TextUtils::ctre_regex_replace<L"\x2\x102\x2.\x10A\x8103.\x1", L"">(original);

        // Remove "20% Additional damage during festival events" > "Dmg +20% (Festival)"
        original = TextUtils::ctre_regex_replace<
            L".\x10A\x108\x10A\x8103\xB71\x101\x100\x1\x1",
            L"\xA85\x10A\xA4E\x1\x101\x114\x2\xAA8\x10A\x108\x107" L"Festival\x1\x1"
        >(original);

        // Check for customized item with +20% damage
        static constexpr ctll::fixed_string dmg_plus_20_pattern = L"\x2\x102\x2.\x10A\xA85\x10A[\xA4C\xA4E]\x1\x101\x114\x1";
        if (item->customized && ctre::search<dmg_plus_20_pattern>(original)) {
            // Remove "\nDamage +20%" > "\n"
            original = TextUtils::ctre_regex_replace<dmg_plus_20_pattern, L"">(original);
            // Append "Customized"
            original += L"\x2\x102\x2\x108\x107" L"Customized\x1";
        }

        return original;
    }
}
