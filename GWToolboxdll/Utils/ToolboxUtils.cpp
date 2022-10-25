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

#include <GWCA/Managers/PlayerMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>

#include <GWCA/Packets/Opcodes.h>
#include <GWCA/Packets/StoC.h>

#include <GWCA/Managers/FriendListMgr.h>
#include <Utils/GuiUtils.h>

#include "ToolboxUtils.h"

namespace {
    bool IsInfused(GW::Item* item) {
        return item && item->info_string && wcschr(item->info_string, 0xAC9);
    }
}

namespace ToolboxUtils {
    bool IsOutpost() {
        return GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost;
    }
    bool IsExplorable() {
        return GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable;
    }

    GW::Player* GetPlayerByName(const wchar_t* _name) {
        if (!_name) return NULL;
        std::wstring name = GuiUtils::SanitizePlayerName(_name);
        GW::PlayerArray* players = GW::PlayerMgr::GetPlayerArray();
        if (!players) return nullptr;
        for (GW::Player& player : *players) {
            if (!player.name) continue;
            if (name == GuiUtils::SanitizePlayerName(player.name))
                return &player;
        }
        return nullptr;
    }
    std::wstring GetPlayerName(uint32_t player_number) {
        GW::Player* player = nullptr;
        if (!player_number) {
            player = GW::PlayerMgr::GetPlayerByID(GW::PlayerMgr::GetPlayerNumber());
            if (!player || !player->name) {
                // Map not loaded; try to get from character context
                auto c = GW::GetCharContext();
                return c ? c->player_name : L"";
            }
        }
        else {
            player = GW::PlayerMgr::GetPlayerByID(player_number);
        }
        return player && player->name ? GuiUtils::SanitizePlayerName(player->name) : L"";
    }
    GW::Array<wchar_t>* GetMessageBuffer() {
        auto* w = GW::GetWorldContext();
        return w && w->message_buff.valid() ? &w->message_buff : nullptr;
    }
    const wchar_t* GetMessageCore() {
        auto* buf = GetMessageBuffer();
        return buf ? buf->begin() : nullptr;
    }
    bool ClearMessageCore() {
        auto* buf = GetMessageBuffer();
        if (!buf)
            return false;
        buf->clear();
        return true;
    }
    const std::wstring GetSenderFromPacket(GW::Packet::StoC::PacketBase* packet) {
        switch (packet->header) {
        case GAME_SMSG_CHAT_MESSAGE_GLOBAL: {
            auto p = (GW::Packet::StoC::MessageGlobal*)packet;
            return p->sender_name;
        } break;
        case GAME_SMSG_CHAT_MESSAGE_LOCAL:
        case GAME_SMSG_TRADE_REQUEST:
            return GetPlayerName(((uint32_t*)packet)[1]);
        }
        return L"";
    }
    GW::Player* GetPlayerByAgentId(uint32_t agent_id, GW::AgentLiving** info_out) {
        const auto agent = static_cast<GW::AgentLiving*>(GW::Agents::GetAgentByID(agent_id));
        if (!(agent && agent->GetIsLivingType() && agent->IsPlayer()))
            return nullptr;
        if (info_out)
            *info_out = agent;
        return GW::PlayerMgr::GetPlayerByID(agent->login_number);

    }

    GW::HeroInfo* GetHeroInfo(uint32_t hero_id) {
        auto w = GW::GetWorldContext();
        if (!(w && w->hero_info.size()))
            return nullptr;
        for (auto& a : w->hero_info) {
            if (a.hero_id == hero_id)
                return &a;
        }
        return nullptr;
    }
    bool IsHenchman(uint32_t agent_id) {
        if (!IsOutpost()) {
            return IsHenchmanInParty(agent_id);
        }
        auto w = GW::GetWorldContext();
        if (!(w && w->henchmen_agent_ids.size()))
            return false;
        for (auto a : w->henchmen_agent_ids) {
            if (a == agent_id)
                return true;
        }
        return false;
    }
    bool IsHero(uint32_t agent_id, GW::HeroInfo** info_out) {
        if (!IsOutpost()) {
            // NB: HeroInfo array is only populated in outposts
            return IsHeroInParty(agent_id);
        }
        auto w = GW::GetWorldContext();
        if (!(w && w->hero_info.size()))
            return false;
        for (auto& a : w->hero_info) {
            if (a.agent_id == agent_id) {
                if (info_out)
                    *info_out = &a;
                return true;
            }
        }
        return false;
    }


    GW::Array<GW::PartyInfo*>* GetParties() {
        auto* p = GW::GetPartyContext();
        return p ? &p->parties : nullptr;
    }

    const GW::HenchmanPartyMember* GetHenchmanPartyMember(uint32_t agent_id, GW::PartyInfo** party_out) {
        const auto* parties = GetParties();
        if (!parties) return nullptr;
        for (const auto party : *parties) {
            if (!party) continue;
            for (const auto& p : party->henchmen) {
                if (p.agent_id != agent_id)
                    continue;
                if (party_out)
                    *party_out = party;
                return &p;
            }
        }
        return nullptr;
    }
    bool IsHenchmanInParty(uint32_t agent_id) {
        GW::PartyInfo* party = nullptr;
        return GetHenchmanPartyMember(agent_id, &party) && party == GW::PartyMgr::GetPartyInfo();
    }

    const GW::HeroPartyMember* GetHeroPartyMember(uint32_t agent_id, GW::PartyInfo** party_out) {
        const auto* parties = GetParties();
        if (!parties) return nullptr;
        for (const auto party : *parties) {
            if (!party) continue;
            for (const auto& p : party->heroes) {
                if (p.agent_id != agent_id)
                    continue;
                if (party_out)
                    *party_out = party;
                return &p;
            }
        }
        return nullptr;
    }
    bool IsHeroInParty(uint32_t agent_id) {
        GW::PartyInfo* party = nullptr;
        return GetHeroPartyMember(agent_id, &party) && party == GW::PartyMgr::GetPartyInfo();
    }

    const GW::PlayerPartyMember* GetPlayerPartyMember(uint32_t login_number, GW::PartyInfo** party_out) {
        const auto* parties = GetParties();
        if (!parties) return nullptr;
        for (const auto party : *parties) {
            if (!party) continue;
            for (const auto& p : party->players) {
                if (p.login_number != login_number)
                    continue;
                if (party_out)
                    *party_out = party;
                return &p;
            }
        }
        return nullptr;
    }
    bool IsPlayerInParty(uint32_t login_number) {
        GW::PartyInfo* party = nullptr;
        return GetPlayerPartyMember(login_number, &party) && party == GW::PartyMgr::GetPartyInfo();
    }

    bool IsAgentInParty(uint32_t agent_id) {
        const auto* party = GW::PartyMgr::GetPartyInfo();
        if (!party)
            return false;
        if (IsHenchmanInParty(agent_id) || IsHeroInParty(agent_id))
            return true;
        const auto player = GetPlayerByAgentId(agent_id);
        return player && IsPlayerInParty(player->player_number);
    }

    float GetSkillRange(GW::Constants::SkillID skill_id)
    {
        const auto skill = GW::SkillbarMgr::GetSkillConstantData(skill_id);
        if (!skill)
            return 0.f;
        if (skill->IsTouchRange())
            return GW::Constants::Range::Touch;
        if (skill->IsHalfRange())
            return GW::Constants::Range::Spellcast / 2.f;
        return GW::Constants::Range::Spellcast;
    }

    // Helper function; avoids doing string checks on offline friends.
    GW::Friend* GetFriend(wchar_t* account, wchar_t* playing, GW::FriendType type, GW::FriendStatus status) {
        if (!(account || playing)) return NULL;
        GW::FriendList* fl = GW::FriendListMgr::GetFriendList();
        if (!fl) return NULL;
        uint32_t n_friends = fl->number_of_friend, n_found = 0;
        GW::FriendsListArray& friends = fl->friends;
        for (GW::Friend* it : friends) {
            if (n_found == n_friends) break;
            if (!it) continue;
            if (it->type != type) continue;
            n_found++;
            if (it->status != status) continue;
            if (account && !wcsncmp(it->alias, account, 20))
                return it;
            if (playing && !wcsncmp(it->charname, playing, 20))
                return it;
        }
        return NULL;
    }

    std::wstring ShorthandItemDescription(GW::Item* item) {
        std::wstring original(item->info_string);
        std::wsmatch m;
        wchar_t buffer[128];

        // For armor items, include full item name and a few description bits.
        switch (static_cast<GW::Constants::ItemType>(item->type)) {
        case GW::Constants::ItemType::Headpiece:
        case GW::Constants::ItemType::Boots:
        case GW::Constants::ItemType::Chestpiece:
        case GW::Constants::ItemType::Gloves:
        case GW::Constants::ItemType::Leggings: {
            original = item->complete_name_enc;
            const std::wstring item_str(item->info_string);
            const std::wregex stacking_att(L"\x2.\x10A\xA84\x10A(.{1,2})\x1\x101\x101\x1\x2\xA3E\x10A\xAA8\x10A\xAB1\x1\x1");
            if (std::regex_search(item_str, m, stacking_att)) {
                swprintf(buffer, _countof(buffer), L"\x2\xAA8\x10A\xA84\x10A%s\x1\x101\x101\x1", m[1].str().c_str());
                original += buffer;
            }
            const std::wregex armor_rating(L"\xA3B\x10A\xA86\x10A\xA44\x1\x101(.)\x1\x2");
            if (std::regex_search(item_str, m, armor_rating)) {
                swprintf(buffer, _countof(buffer), L"\x2\x102\x2\xA86\x10A\xA44\x1\x101%s", m[1].str().c_str());
                original += buffer;
            }
            if (IsInfused(item))
                original += L"\x2\x102\x2\xAC9";
            return original;
        }
        default:
            break;
        }

        // Replace "Requires 9 Divine Favor" > "q9 Divine Favor"
        const std::wregex regexp_req(L".\x10A\x0AA8\x10A\xAA9\x10A.\x1\x101.\x1\x1");
        while (std::regex_search(original, m, regexp_req)) {
            for (auto& match : m) {
                const std::wstring found = match.str();
                swprintf(buffer, _countof(buffer), L"\x108\x107, q%d \x1\x2%c", found.at(9) - 0x100, found.at(6));
                original = std::regex_replace(original, std::wregex(found), buffer);
            }
        }
        // Replace "Requires 9 Scythe Mastery" > "q9 Scythe Mastery"
        const std::wregex regexp_req2(L".\x10A\xAA8\x10A\xAA9\x10A\x8101.\x1\x101.\x1\x1");
        while (std::regex_search(original, m, regexp_req2)) {
            for (auto& match : m) {
                const std::wstring found = match.str();
                swprintf(buffer, _countof(buffer), L"\x108\x107, q%d \x1\x2\x8101%c", found.at(10) - 0x100, found.at(7));
                original = std::regex_replace(original, std::wregex(found), buffer);
            }
        }

        // "vs. Earth damage" > "Earth"
        // "vs. Demons" > "Demons"
        const std::wregex vs_damage(L"[\xAAC\xAAF]\x10A.\x1");
        while (std::regex_search(original, m, vs_damage)) {
            for (auto& match : m) {
                const std::wstring found = match.str();
                swprintf(buffer, _countof(buffer), L"%c", found.at(2));
                original = std::regex_replace(original, std::wregex(found), buffer);
            }
        }

        // Replace "Lengthens ??? duration on foes by 33%" > "??? duration +33%"
        const std::wregex regexp_lengthens_duration(L"\xAA4\x10A.\x1");
        while (std::regex_search(original, m, regexp_lengthens_duration)) {
            for (auto& match : m) {
                const std::wstring found = match.str();
                swprintf(buffer, _countof(buffer), L"%c\x2\x108\x107 +33%%\x1", found.at(2));
                original = std::regex_replace(original, std::wregex(found), buffer);
            }
        }

        // Replace "Reduces ??? duration on you by 20%" > "??? duration -20%"
        const std::wregex regexp_reduces_duration(L"\xAA7\x10A.\x1");
        while (std::regex_search(original, m, regexp_reduces_duration)) {
            for (auto& match : m) {
                const std::wstring found = match.str();
                swprintf(buffer, _countof(buffer), L"%c\x2\x108\x107 -20%%\x1", found.at(2));
                original = std::regex_replace(original, std::wregex(found), buffer);
            }
        }

        // Change "Damage 15% (while Health is above 50%)" to "Damage +15^50"
        //std::wregex damage_15_over_50(L".\x010A\xA85\x010A\xA4C\x1\x101.\x1\x2" L".\x010A\xAA8\x010A\xABC\x10A\xA52\x1\x101.\x1\x1");
        // Change " (while Health is above n)" to "^n";
        const std::wregex n_over_n(L"\xAA8\x10A\xABC\x10A\xA52\x1\x101.\x1");
        while (std::regex_search(original, m, n_over_n)) {
            for (auto& match : m) {
                std::wstring found = match.str();
                swprintf(buffer, _countof(buffer), L"\x108\x107^%d\x1", found.at(7) - 0x100);
                original = std::regex_replace(original, std::wregex(found), buffer);
            }
        }

        // Change "Enchantments last 20% longer" to "Ench +20%"
        const std::wregex enchantments(L"\xAA2\x101.");
        while (std::regex_search(original, m, enchantments)) {
            for (auto& match : m) {
                std::wstring found = match.str();
                swprintf(buffer, _countof(buffer), L"\x108\x107" L"Enchantments +%d%%\x1", found.at(2) - 0x100);
                original = std::regex_replace(original, std::wregex(found), buffer);
            }
        }

        // "(Chance: 18%)" > "(18%)"
        const std::wregex chance_regex(L"\xA87\x10A\xA48\x1\x101.");
        while (std::regex_search(original, m, chance_regex)) {
            for (auto& match : m) {
                std::wstring found = match.str();
                swprintf(buffer, _countof(buffer), L"\x108\x107%d%%\x1", found.at(5) - 0x100);
                original = std::regex_replace(original, std::wregex(found), buffer);
            }
        }
        // Change "Halves skill recharge of <attribute> spells" > "HSR <attribute>"
        const std::wregex hsr_attribute(L"\xA81\x10A\xA58\x1\x10B.\x1");
        while (std::regex_search(original, m, hsr_attribute)) {
            for (auto& match : m) {
                std::wstring found = match.str();
                swprintf(buffer, _countof(buffer), L"\x108\x107" L"HSR \x1\x2%c", found.at(5));
                original = std::regex_replace(original, std::wregex(found), buffer);
            }
        }
        // Change "Inscription: "Blah Blah"" to just "Blah Blah"
        const std::wregex inscription(L"\x8101\x5DC5\x10A..\x1");
        while (std::regex_search(original, m, inscription)) {
            for (auto& match : m) {
                std::wstring found = match.str();
                swprintf(buffer, _countof(buffer), L"%c%c", found.at(3), found.at(4));
                original = std::regex_replace(original, std::wregex(found), buffer);
            }
        }

        // Change "Halves casting time of <attribute> spells" > "HCT <attribute>"
        const std::wregex hct_attribute(L"\xA81\x10A\xA47\x1\x10B.\x1");
        while (std::regex_search(original, m, hct_attribute)) {
            for (auto& match : m) {
                std::wstring found = match.str();
                swprintf(buffer, _countof(buffer), L"\x108\x107" L"HCT \x1\x2%c", found.at(5));
                original = std::regex_replace(original, std::wregex(found), buffer);
            }
        }

        // Change "Piercing Dmg: 11-22" > "Piercing: 11-22"
        const std::wregex weapon_dmg(L"\xA89\x10A\xA4E\x1\x10B.\x1\x101.\x102.");
        while (std::regex_search(original, m, weapon_dmg)) {
            for (auto& match : m) {
                std::wstring found = match.str();
                swprintf(buffer, _countof(buffer), L"%c\x2\x108\x107: %d-%d\x1", found.at(5),found.at(8) - 0x100, found.at(10) - 0x100);
                original = std::regex_replace(original, std::wregex(found), buffer);
            }
        }

        // Change "Life draining -3, Health regeneration -1" > "Vampiric" (add at end of description)
        const std::wregex vampiric(L"\x2\x102\x2.\x10A\xA86\x10A\xA54\x1\x101.\x1" L"\x2\x102\x2.\x10A\xA7E\x10A\xA53\x1\x101.\x1");
        if (std::regex_search(original, vampiric)) {
            original = std::regex_replace(original, vampiric, L"");
            original += L"\x2\x102\x2\x108\x107" L"Vampiric\x1";
        }
        // Change "Energy gain on hit 1, Energy regeneration -1" > "Zealous" (add at end of description)
        const std::wregex zealous(L"\x2\x102\x2.\x10A\xA86\x10A\xA50\x1\x101.\x1" L"\x2\x102\x2.\x10A\xA7E\x10A\xA51\x1\x101.\x1");
        if (std::regex_search(original, zealous)) {
            original = std::regex_replace(original, zealous, L"");
            original += L"\x2\x102\x2\x108\x107" L"Zealous\x1";
        }

        // Change "Damage" > "Dmg"
        original = std::regex_replace(original, std::wregex(L"\xA4C"), L"\xA4E");

        // Change Bow "Two-Handed" > ""
        original = std::regex_replace(original, std::wregex(L"\x8102\x1227"), L"\xA3E");

        // Change "Halves casting time of spells" > "HCT"
        original = std::regex_replace(original, std::wregex(L"\xA80\x10A\xA47\x1"), L"\x108\x107" L"HCT\x1");

        // Change "Halves skill recharge of spells" > "HSR"
        const std::wregex half_skill_recharge(L"\xA80\x10A\xA58\x1");
        original = std::regex_replace(original, half_skill_recharge, L"\x108\x107" L"HSR\x1");

        // Remove (Stacking) and (Non-stacking) rubbish
        const std::wregex stacking_non_stacking(L"\x2.\x10A\xAA8\x10A[\xAB1\xAB2]\x1\x1");
        original = std::regex_replace(original, stacking_non_stacking, L"");

        // Replace (while affected by a(n) to just (n)
        const std::wregex while_affected_by(L"\x8101\x4D9C\x10A.\x1");
        while (std::regex_search(original, m, while_affected_by)) {
            for (auto& match : m) {
                const std::wstring found = match.str();
                wmemset(buffer, 0, 2);
                buffer[0] = found.at(3);
                original = std::regex_replace(original, std::wregex(found), buffer);
            }
        }
        // Replace (while xxx) to just (xxx)
        original = std::regex_replace(original, std::wregex(L"\xAB4"), L"\x108\x107" L"Attacking\x1");
        original = std::regex_replace(original, std::wregex(L"\xAB5"), L"\x108\x107" L"Casting\x1");
        original = std::regex_replace(original, std::wregex(L"\xAB6"), L"\x108\x107" L"Condition\x1");
        original = std::regex_replace(original, std::wregex(L"[\xAB7\x4B6]"), L"\x108\x107" L"Enchanted\x1");
        original = std::regex_replace(original, std::wregex(L"[\xAB8\x4B4]"), L"\x108\x107" L"Hexed\x1");
        original = std::regex_replace(original, std::wregex(L"[\xAB9\xABA]"), L"\x108\x107" L"Stance\x1");

        // Combine Attribute + 3, Attribute + 1 to Attribute +3 +1 (e.g. headpiece)
        const std::wregex attribute_stacks(L".\x10A\xA84\x10A.\x1\x101.\x1\x2\x102\x2.\x10A\xA84\x10A.\x1\x101.\x1");
        if (std::regex_search(original, m, attribute_stacks)) {
            for (auto& match : m) {
                const std::wstring found = match.str();
                if (found[4] != found[16]) continue; // Different attributes.
                swprintf(buffer, _countof(buffer), L"%c\x10A\xA84\x10A%c\x1\x101%c\x2\xA84\x101%c\x1",
                    found[0], found[4],found[7], found[19]);
                original = std::regex_replace(original, std::wregex(found), buffer);
            }
        }
        // Remove "Value: 122 gold"
        original = std::regex_replace(original, std::wregex(L"\x2\x102\x2\xA3E\x10A\xA8A\x10A\xA59\x1\x10B.\x101.(\x102.)?\x1\x1"), L"");

        // Remove other "greyed" generic terms e.g. "Two-Handed", "Unidentified"
        original = std::regex_replace(original, std::wregex(L"\x2\x102\x2\xA3E\x10A.\x1"), L"");

        // Remove "Necromancer Munne sometimes gives these to me in trade" etc
        original = std::regex_replace(original, std::wregex(L"\x2\x102\x2.\x10A\x8102.\x1"), L"");

        // Remove "Inscription: None"
        original = std::regex_replace(original, std::wregex(L"\x2\x102\x2.\x10A\x8101\x5A1F\x1"), L"");

        // Remove "Crafted in tribute to an enduring legend." etc
        original = std::regex_replace(original, std::wregex(L"\x2\x102\x2.\x10A\x8103.\x1"), L"");

        // Remove "20% Additional damage during festival events" > "Dmg +20% (Festival)"
        original = std::regex_replace(original, std::wregex(L".\x10A\x108\x10A\x8103\xB71\x101\x100\x1\x1"), L"\xA85\x10A\xA4E\x1\x101\x114\x2\xAA8\x10A\x108\x107" L"Festival\x1\x1");

        const std::wregex dmg_plus_20(L"\x2\x102\x2.\x10A\xA85\x10A[\xA4C\xA4E]\x1\x101\x114\x1");
        if (item->customized && std::regex_search(original, dmg_plus_20)) {
            // Remove "\nDamage +20%" > "\n"
            original = std::regex_replace(original, dmg_plus_20, L"");
            // Append "Customized"
            original += L"\x2\x102\x2\x108\x107" L"Customized\x1";
        }

        return std::move(original);
    }
}
