#include "stdafx.h"

#include <GWCA/Context/CharContext.h>
#include <GWCA/Context/PartyContext.h>
#include <GWCA/Context/WorldContext.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Attribute.h>
#include <GWCA/GameEntities/Friendslist.h>
#include <GWCA/GameEntities/Hero.h>
#include <GWCA/GameEntities/Item.h>
#include <GWCA/GameEntities/Map.h>
#include <GWCA/GameEntities/Party.h>
#include <GWCA/GameEntities/Player.h>
#include <GWCA/GameEntities/Skill.h>

#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/EffectMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/MemoryMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/PlayerMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>

#include <GWCA/Packets/Opcodes.h>
#include <GWCA/Packets/StoC.h>

#include <GWCA/Managers/FriendListMgr.h>

#include <Constants/EncStrings.h>
#include <GWCA/Context/MapContext.h>
#include <GWCA/GameEntities/Frame.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/ItemMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Utilities/Scanner.h>
#include <Modules/Resources.h>
#include <Timer.h>
#include <Utils/GuiUtils.h>
#include <Utils/TextUtils.h>
#include "ToolboxUtils.h"

namespace {

    GUID* account_uuid = 0;

    GW::Array<GW::AvailableCharacterInfo>* available_chars_ptr = nullptr;

    constexpr uint32_t bogus_area_info_flags = 0x5000000; // e.g. "wrong" Augury Rock is map 119, no NPCs.
    constexpr uint32_t debug_area_info_flag = 0x80000000;

    bool IsInfused(const GW::Item* item)
    {
        return item && item->info_string && wcschr(item->info_string, 0xAC9);
    }

    GW::UI::Frame* GetSelectorFrame()
    {
        return GW::UI::GetFrameByLabel(L"Selector");
    }
    struct CharSelectorChar {
        uint32_t h0000;
        uint32_t h0004;
        uint32_t h0008;
        uint32_t h000C;
        uint32_t h0010;
        uint32_t h0014;
        uint32_t h0018;
        uint32_t h001C;
        wchar_t name[0x14];
        // ...
    };
    struct CharSelectorContext {
        uint32_t vtable;
        uint32_t frame_id;
        GW::Array<CharSelectorChar*> chars;
        // ...
    };

} // namespace

namespace GW {

    namespace Map {
        bool HasMapDisplayInfo(const GW::AreaInfo* map_info)
        {
            return map_info && map_info->thumbnail_id && map_info->name_id && (map_info->x || map_info->y);
        }

        bool IsExcludedMapInfo(const GW::AreaInfo* map_info)
        {
            return map_info && ((map_info->flags & bogus_area_info_flags) == bogus_area_info_flags || (map_info->flags & debug_area_info_flag) != 0);
        }

        bool GetMapWorldMapBounds(GW::AreaInfo* map, ImRect* out)
        {
            if (!map) return false;
            auto bounds = &map->icon_start_x;
            if (*bounds == 0) bounds = &map->icon_start_x_dupe;

            // NB: Even though area info holds map bounds as uints, the world map uses signed floats anyway - a cast should be fine here.
            *out = {{static_cast<float>(bounds[0]), static_cast<float>(bounds[1])}, {static_cast<float>(bounds[2]), static_cast<float>(bounds[3])}};
            return true;
        }

        std::vector<GW::Constants::TitleID> GetTitlesForMap(GW::Constants::MapID map_id)
        {
            using namespace GW::Constants;

            switch (map_id) {
                case MapID::The_Deep:
                    return {TitleID::Luxon};
                case MapID::Urgozs_Warren:
                    return {TitleID::Kurzick};
                // Deldrimor: DepthsOfTyria missions/dungeons
                case MapID::A_Time_for_Heroes:
                case MapID::Central_Transfer_Chamber_outpost:
                case MapID::Destructions_Depths_Level_1:
                case MapID::Destructions_Depths_Level_2:
                case MapID::Destructions_Depths_Level_3:
                case MapID::Ravens_Point_Level_1:
                case MapID::Ravens_Point_Level_2:
                case MapID::Ravens_Point_Level_3:
                case MapID::Glints_Challenge_mission: // CrystalDesert
                // Deldrimor: EotN missions with Destroyers (would otherwise show Norn/Asuran by region)
                case MapID::A_Gate_Too_Far_Level_1:
                case MapID::A_Gate_Too_Far_Level_2:
                case MapID::A_Gate_Too_Far_Level_3:
                case MapID::A_Gate_Too_Far_mission:
                case MapID::The_Elusive_Golemancer_Level_1:
                case MapID::The_Elusive_Golemancer_Level_2:
                case MapID::The_Elusive_Golemancer_Level_3:
                case MapID::The_Elusive_Golemancer_mission:
                case MapID::Genius_Operated_Living_Enchanted_Manifestation_mission:
                    return {TitleID::Deldrimor};
                // Lightbringer: Grand Court of Sebelkeh mission has Margonites (would otherwise show Sunspear by continent)
                case MapID::Grand_Court_of_Sebelkeh:
                    return {TitleID::Lightbringer};
                // Vanguard: DepthsOfTyria dungeons in Charr territory
                case MapID::Cathedral_of_Flames_Level_1:
                case MapID::Cathedral_of_Flames_Level_2:
                case MapID::Cathedral_of_Flames_Level_3:
                case MapID::Rragars_Menagerie_Level_1:
                case MapID::Rragars_Menagerie_Level_2:
                case MapID::Rragars_Menagerie_Level_3:
                    return {TitleID::Vanguard};
                    // @todo: Bear Club for Women/Men (Norn)
            }

            const auto map_info = GW::Map::GetMapInfo(map_id);
            if (!map_info) return {};

            switch (map_info->region) {
                case GW::Region::Region_TarnishedCoast:
                    return {TitleID::Asuran};
                case GW::Region::Region_FarShiverpeaks:
                    return {TitleID::Norn};
                case GW::Region::Region_Ascalon:
                case GW::Region::Region_CharrHomelands:
                    return {TitleID::Vanguard};
                case GW::Region::Region_DomainOfAnguish:
                case GW::Region::Region_Desolation:
                    return {TitleID::Lightbringer};
                case GW::Region::Region_Kurzick:
                case GW::Region::Region_Luxon:
                    return {TitleID::Kurzick, TitleID::Luxon};
            }

            switch (map_info->continent) {
                case GW::Continent::Elona:
                    return {TitleID::Sunspear};
                case GW::Continent::RealmOfTorment:
                    return {TitleID::Lightbringer};
            }

            return {};
        }

        GW::Constants::TitleID GetTitleForMap(GW::Constants::MapID map_id)
        {
            const auto titles = GetTitlesForMap(map_id);
            return titles.empty() ? GW::Constants::TitleID::None : titles[0];
        }

        void PingCompass(const GW::GamePos& position)
        {
            constexpr float compass_scale = 96.f;
            GW::GameThread::Enqueue([cpy = position]() {
                GW::UI::CompassPoint point({std::lroundf(cpy.x / compass_scale), std::lroundf(cpy.y / compass_scale)});
                GW::UI::UIPacket::kCompassDraw packet = {.player_number = GW::PlayerMgr::GetPlayerNumber(), .session_id = (uint32_t)TIMER_INIT(), .number_of_points = 1, .points = &point};
                GW::UI::SendUIMessage(GW::UI::UIMessage::kCompassDraw, &packet);
            });
        }

        bool IsPreSearing(const GW::Constants::MapID map_id)
        {
            const auto map_info = GW::Map::GetMapInfo(map_id);
            if (!map_info) return false;
            constexpr std::array presearing_dungeon_ids = {
                GW::Constants::MapID::Forsaken_Tunnels_Presearing_Level1,
                GW::Constants::MapID::Forsaken_Tunnels_Presearing_Level2,
                GW::Constants::MapID::Forsaken_Tunnels_Presearing_Level3,
            };
            return map_info->region == GW::Region::Region_Presearing || std::find(presearing_dungeon_ids.begin(), presearing_dungeon_ids.end(), map_id) != presearing_dungeon_ids.end();
        }

        GW::Array<GW::MapProp*>* GetMapProps()
        {
            const auto m = GetMapContext();
            const auto p = m ? m->props : nullptr;
            return p ? &p->propArray : nullptr;
        }

        bool IsFestivalOutpost(const GW::Constants::MapID map_id)
        {
            using namespace GW::Constants;
            switch (map_id) {
                case MapID::Kamadan_Jewel_of_Istan_Halloween_outpost:
                case MapID::Kamadan_Jewel_of_Istan_Wintersday_outpost:
                case MapID::Kamadan_Jewel_of_Istan_Canthan_New_Year_outpost:
                case MapID::Lions_Arch_Halloween_outpost:
                case MapID::Lions_Arch_Wintersday_outpost:
                case MapID::Lions_Arch_Canthan_New_Year_outpost:
                case MapID::Ascalon_City_Wintersday_outpost:
                case MapID::Droknars_Forge_Halloween_outpost:
                case MapID::Droknars_Forge_Wintersday_outpost:
                case MapID::Tomb_of_the_Primeval_Kings_Halloween_outpost:
                case MapID::Shing_Jea_Monastery_Dragon_Festival_outpost:
                case MapID::Shing_Jea_Monastery_Canthan_New_Year_outpost:
                case MapID::Kaineng_Center_Canthan_New_Year_outpost:
                case MapID::Eye_of_the_North_outpost_Wintersday_outpost:
                    return true;
                default:
                    return false;
            }
        }

    } // namespace Map
    namespace SkillbarMgr {
        GW::Attribute* GetPlayerAttribute(GW::Constants::Attribute attribute_id)
        {
            const auto my_id = GW::Agents::GetControlledCharacterId();
            PartyAttributeArray& party_attributes = GW::GetWorldContext()->attributes;
            for (PartyAttribute& agent_attributes : party_attributes) {
                if (agent_attributes.agent_id != my_id) continue;
                return &agent_attributes.attribute[(uint32_t)attribute_id];
            }
            return 0;
        }
    } // namespace SkillbarMgr
    namespace LoginMgr {
        const bool IsCharSelectReady()
        {
            uint32_t ui_state = 10;
            SendUIMessage(GW::UI::UIMessage::kCheckUIState, nullptr, &ui_state);
            const auto frame = GetSelectorFrame();
            if (!(ui_state == 2 && frame && frame->IsVisible())) return false;
            const auto ctx = (CharSelectorContext*)GW::UI::GetFrameContext(frame);
            return ((uintptr_t)ctx > 0xFFFF) && ctx->frame_id == frame->frame_id;
        }

        const bool SelectCharacterToPlay(const wchar_t* name, bool play)
        {
            if (!(name && *name && IsCharSelectReady())) return false;

            const auto selector = GetSelectorFrame();
            const auto ctx = (CharSelectorContext*)GW::UI::GetFrameContext(selector);

            const auto panes = GW::UI::GetChildFrame(selector, 0);

            uint32_t selected_idx = 0;
            GW::UI::SendFrameUIMessage(panes, GW::UI::UIMessage::kFrameMessage_0x4a, 0, (void*)&selected_idx);

            uint32_t target_idx = 0xffff;

            const auto len = ctx->chars.size();

            if (selected_idx >= len) selected_idx = 0;

            bool chosen = false;
            for (size_t i = 0; !chosen && i < len; i++) {
                const auto c = ctx->chars[i];
                if (!(c && wcscmp(c->name, name) == 0)) continue; // Not this character
                target_idx = i;
                break;
            }
            if (target_idx > len) return false;


            auto select_char = [&](size_t idx) {
                GW::UI::UIPacket::kMouseAction action{};
                action.frame_id = selector->frame_id;
                action.child_offset_id = selector->child_offset_id;
                struct button_param {
                    const wchar_t* name;
                    uint32_t play;
                };
                button_param wparam = {ctx->chars[idx]->name, 0u};
                action.wparam = &wparam;
                action.current_state = GW::UI::UIPacket::ActionState::MouseClick;

                if (!GW::UI::SendFrameUIMessage(GW::UI::GetParentFrame(selector), GW::UI::UIMessage::kMouseClick2, &action)) return false;
                GW::UI::SendFrameUIMessage(panes, GW::UI::UIMessage::kFrameMessage_0x4a, 0, (void*)&selected_idx);
                return selected_idx == idx;
            };

            // Navigate to target character by selecting previous/next until we reach it
            while (target_idx < selected_idx) {
                // Need to go backwards - select the previous character
                if (selected_idx == 0) break; // Can't go before first character
                if (!select_char(selected_idx - 1)) return false;
            }

            while (target_idx > selected_idx) {
                // Need to go forwards - select the next character
                if (selected_idx >= ctx->chars.size() - 1) break; // Can't go past last character
                if (!select_char(selected_idx + 1)) return false;
            }
            chosen = selected_idx == target_idx;

            if (!chosen) return false;

            return (!play || GW::UI::ButtonClick(GW::UI::GetFrameByLabel(L"Play")));
        }
    } // namespace LoginMgr


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
                if (players->at(i).login_number == player_number) return i + 1;
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
                    if (hero.owner_player_id != player_member.login_number) continue;
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
        std::vector<uint32_t> GetPartyAgentIds(uint32_t party_id)
        {
            std::vector<uint32_t> out;
            for (size_t i = 0; i < 12; i++) {
                const auto agent_id = GetPartyMemberAgentId(i, party_id);
                if (!agent_id) return out;
                out.push_back(agent_id);
            }
            return out;
        }
        bool IsAgentInParty(uint32_t agent_id, uint32_t party_id)
        {
            const auto party_agents = GetPartyAgentIds(party_id);
            return std::find(party_agents.begin(), party_agents.end(), agent_id) != party_agents.end();
        }
    } // namespace PartyMgr

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
            if (!account_uuid) {
                auto address = GW::Scanner::Find("\x50\x6a\x18\x6a\x02\xff\x15", "xxxxxxx", 0x7);
                if (address && GW::Scanner::IsValidPtr(*(uintptr_t*)address, GW::ScannerSection::Section_DATA)) {
                    address = *(uintptr_t*)address;
                    account_uuid = (GUID*)(address + 0x90);
                }
            }
            return account_uuid;
        }

        GUID GetAccountUuid()
        {
            const auto uuid = GetPortalAccountUuid();
            if (uuid) return *uuid;
            const auto email = GetAccountEmail();
            return email && *email ? TextUtils::ConvertWStringToGuid(email) : GUID{};
        }

        AvailableCharacterInfo* GetAvailableCharacter(const wchar_t* name)
        {
            const auto characters = name ? GetAvailableChars() : nullptr;
            if (!characters) return nullptr;
            for (auto& ac : *characters) {
                if (wcscmp(ac.player_name, name) == 0) return &ac;
            }
            return nullptr;
        }
    } // namespace AccountMgr

    namespace MemoryMgr {
        // Appends an element to a GW-managed array, growing the buffer via MemRealloc if at capacity.
        // Returns a pointer to the newly appended element.
        template <typename T>
        T* AddToGuildWarsArray(GW::BaseArray<T>& arr, const T& element)
        {
            if (arr.m_size >= arr.m_capacity) {
                auto* new_buf = static_cast<T*>(MemRealloc(arr.m_buffer, (arr.m_size + 1) * sizeof(T)));
                GWCA_ASSERT(new_buf);
                arr.m_buffer = new_buf;
                arr.m_capacity++;
            }
            arr.m_buffer[arr.m_size] = element;
            return &arr.m_buffer[arr.m_size++];
        }

        // Removes the element at index from a GW-managed array by shifting remaining elements left.
        // Capacity is unchanged so future AddToGuildWarsArray calls reuse the slack without reallocating.
        template <typename T>
        void RemoveFromGwArray(GW::BaseArray<T>& arr, uint32_t index)
        {
            GWCA_ASSERT(index < arr.m_size);
            const auto remaining = arr.m_size - index - 1;
            if (remaining > 0) memmove(&arr.m_buffer[index], &arr.m_buffer[index + 1], remaining * sizeof(T));
            arr.m_size--;
        }
        bool GetPersonalDir(std::wstring& out)
        {
            out.resize(512, 0);
            if (!GetPersonalDir(out.capacity(), out.data())) return false;
            out.resize(wcslen(out.data()));
            return !out.empty();
        }
        std::filesystem::path GetBuildsDir()
        {
            std::wstring builds_folder;
            GetPersonalDir(builds_folder);
            if (builds_folder.empty()) return L"";
            return std::filesystem::path(builds_folder) / L"Guild Wars" / L"Templates" / L"Skills";
        }
    } // namespace MemoryMgr

    namespace UI {
        void AsyncDecodeStrS(const wchar_t* enc_str, std::string* out, GW::Constants::Language language_id)
        {
            if (!enc_str || !out) {
                return;
            }
            if (language_id == (GW::Constants::Language)0xff) {
                wchar_t decoded[2048] = {};
                GW::UI::AsyncDecodeStr(enc_str, decoded, _countof(decoded));
                *out = TextUtils::WStringToString(decoded);
                return;
            }
            AsyncDecodeStr(
                enc_str,
                [](void* param, const wchar_t* s) {
                    if (!s) {
                        return;
                    }
                    *(std::string*)param = TextUtils::WStringToString(s);
                },
                out, language_id
            );
        }
        void AsyncDecodeStr(const wchar_t* enc_str, std::wstring* out, GW::Constants::Language language_id)
        {
            if (!enc_str || !out) {
                return;
            }
            if (language_id == (GW::Constants::Language)0xff) {
                wchar_t decoded[2048] = {};
                GW::UI::AsyncDecodeStr(enc_str, decoded, _countof(decoded));
                *out = decoded;
                return;
            }
            AsyncDecodeStr(
                enc_str,
                [](void* param, const wchar_t* s) {
                    if (!s) {
                        return;
                    }
                    *(std::wstring*)param = s;
                },
                out, language_id
            );
        }
        bool BelongsToFrame(GW::UI::Frame* parent, GW::UI::Frame* child)
        {
            while (child && parent) {
                if (child == parent) {
                    return true;
                }
                child = GetParentFrame(child);
            }
            return false;
        }
        GW::UI::Frame* GetNthParentFrame(GW::UI::Frame* frame, uint32_t n)
        {
            for (uint32_t i = 0; i < n && frame; i++) {
                frame = GetParentFrame(frame);
            }
            return frame;
        }
        void Screenshot()
        {
            GW::GameThread::Enqueue([] {
                if (!GW::Map::GetIsMapLoaded() || !GW::Agents::GetControlledCharacter()) return;
                const auto frame = GW::UI::GetFrameByLabel(L"Game");
                Keypress(GW::UI::ControlAction_Screenshot, GW::UI::GetChildFrame(frame, 6));
                Keypress(GW::UI::ControlAction_Screenshot, GW::UI::GetParentFrame(frame));
            });
        }
        bool IsLoadingScreenShown()
        {
            return GW::UI::GetFrameByLabel(L"Mission") != nullptr;
        }
    } // namespace UI

    namespace PlayerMgr {
        bool IsMelandrusAccord()
        {
            const auto c = GW::PlayerMgr::GetPlayerByID();
            return (c->reforged_or_dhuums_flags & 0x2) != 0;
        }
        GW::GamePos* GetPlayerPosition()
        {
            const auto player = GW::Agents::GetControlledCharacter();
            return player ? &player->pos : nullptr;
        }
    } // namespace PlayerMgr
    namespace Agents {
        bool IsAgentCarryingBundle(uint32_t agent_id)
        {
            const auto agent = (GW::AgentLiving*)GW::Agents::GetAgentByID(agent_id);
            const auto held_item = agent && agent->GetIsLivingType() ? GW::Items::GetItemById(agent->weapon_item_id) : 0;
            return held_item && held_item->type == GW::Constants::ItemType::Bundle;
        }
        void AsyncGetAgentName(const uint32_t agent_id, std::wstring& out)
        {
            UI::AsyncDecodeStr(GetAgentEncName(agent_id), &out);
        }
        void AsyncGetAgentName(const Agent* agent, std::wstring& out)
        {
            UI::AsyncDecodeStr(GetAgentEncName(agent), &out);
        }
    } // namespace Agents
    namespace Items {
        GW::Constants::Rarity GetRarity(const GW::Item* item)
        {
            if (!item) return GW::Constants::Rarity::Unknown;
            if ((item->interaction & 0x10) != 0) return GW::Constants::Rarity::Green;
            if ((item->interaction & 0x400000) != 0) return GW::Constants::Rarity::Purple;
            if ((item->interaction & 0x20000) != 0) return GW::Constants::Rarity::Gold;
            if (item->single_item_name && item->single_item_name[0] == 0xA3F) return GW::Constants::Rarity::Blue;
            return GW::Constants::Rarity::White;
        }

        ImColor GetRarityColor(const GW::Constants::Rarity rarity)
        {
            switch (rarity) {
                case GW::Constants::Rarity::White:
                    return GW::Chat::TextColor::ColorItemCommon;
                case GW::Constants::Rarity::Blue:
                    return GW::Chat::TextColor::ColorItemEnhance;
                case GW::Constants::Rarity::Purple:
                    return GW::Chat::TextColor::ColorItemUncommon;
                case GW::Constants::Rarity::Gold:
                    return GW::Chat::TextColor::ColorItemRare;
                case GW::Constants::Rarity::Green:
                    return GW::Chat::TextColor::ColorItemUnique;
            }
            return GW::Chat::TextColor::ColorItemDull;
        }

        const char* GetItemTypeName(const GW::Constants::ItemType item_type)
        {
            using GW::Constants::ItemType;
            switch (item_type) {
                case ItemType::Salvage:
                    return "Salvage Item";
                case ItemType::Axe:
                    return "Axe";
                case ItemType::Bag:
                    return "Bag";
                case ItemType::Boots:
                    return "Boots";
                case ItemType::Bow:
                    return "Bow";
                case ItemType::Bundle:
                    return "Bundle";
                case ItemType::Chestpiece:
                    return "Chestpiece";
                case ItemType::Rune_Mod:
                    return "Rune/Upgrade";
                case ItemType::Usable:
                    return "Usable Item";
                case ItemType::Dye:
                    return "Dye";
                case ItemType::Materials_Zcoins:
                    return "Materials";
                case ItemType::Offhand:
                    return "Offhand";
                case ItemType::Gloves:
                    return "Gloves";
                case ItemType::Hammer:
                    return "Hammer";
                case ItemType::Headpiece:
                    return "Headpiece";
                case ItemType::CC_Shards:
                    return "Candy Cane Shards";
                case ItemType::Key:
                    return "Key";
                case ItemType::Leggings:
                    return "Leggings";
                case ItemType::Gold_Coin:
                    return "Gold";
                case ItemType::Quest_Item:
                    return "Quest Item";
                case ItemType::Wand:
                    return "Wand";
                case ItemType::Shield:
                    return "Shield";
                case ItemType::Staff:
                    return "Staff";
                case ItemType::Sword:
                    return "Sword";
                case ItemType::Kit:
                    return "Kit";
                case ItemType::Trophy:
                    return "Trophy";
                case ItemType::Scroll:
                    return "Scroll";
                case ItemType::Daggers:
                    return "Daggers";
                case ItemType::Present:
                    return "Present";
                case ItemType::Minipet:
                    return "Miniature";
                case ItemType::Scythe:
                    return "Scythe";
                case ItemType::Spear:
                    return "Spear";
                case ItemType::Storybook:
                    return "Storybook";
                case ItemType::Costume:
                    return "Costume";
                case ItemType::Costume_Headpiece:
                    return "Costume Headpiece";
                case ItemType::Unknown:
                default:
                    return "Unknown";
            }
        }

        const MaterialInfo* GetMaterialInfo(GW::Constants::MaterialSlot slot)
        {
            using namespace GW::Constants;
            static const std::unordered_map<MaterialSlot, MaterialInfo> material_info = {
                {MaterialSlot::Bone, {GW::EncStrings::Bone, GW::Constants::ItemID::Bone}},
                {MaterialSlot::IronIngot, {GW::EncStrings::IronIngot, GW::Constants::ItemID::IronIngot}},
                {MaterialSlot::TannedHideSquare, {GW::EncStrings::TannedHideSquare, GW::Constants::ItemID::TannedHideSquare}},
                {MaterialSlot::Scale, {GW::EncStrings::Scale, GW::Constants::ItemID::Scale}},
                {MaterialSlot::ChitinFragment, {GW::EncStrings::ChitinFragment, GW::Constants::ItemID::ChitinFragment}},
                {MaterialSlot::BoltofCloth, {GW::EncStrings::BoltofCloth, GW::Constants::ItemID::BoltofCloth}},
                {MaterialSlot::WoodPlank, {GW::EncStrings::WoodPlank, GW::Constants::ItemID::WoodPlank}},
                {MaterialSlot::GraniteSlab, {GW::EncStrings::GraniteSlab, GW::Constants::ItemID::GraniteSlab}},
                {MaterialSlot::PileofGlitteringDust, {GW::EncStrings::PileofGlitteringDust, GW::Constants::ItemID::PileofGlitteringDust}},
                {MaterialSlot::PlantFiber, {GW::EncStrings::PlantFiber, GW::Constants::ItemID::PlantFiber}},
                {MaterialSlot::Feather, {GW::EncStrings::Feather, GW::Constants::ItemID::Feather}},
                {MaterialSlot::FurSquare, {GW::EncStrings::FurSquare, GW::Constants::ItemID::FurSquare}},
                {MaterialSlot::BoltofLinen, {GW::EncStrings::BoltofLinen, GW::Constants::ItemID::BoltofLinen}},
                {MaterialSlot::BoltofDamask, {GW::EncStrings::BoltofDamask, GW::Constants::ItemID::BoltofDamask}},
                {MaterialSlot::BoltofSilk, {GW::EncStrings::BoltofSilk, GW::Constants::ItemID::BoltofSilk}},
                {MaterialSlot::GlobofEctoplasm, {GW::EncStrings::GlobofEctoplasm, GW::Constants::ItemID::GlobofEctoplasm}},
                {MaterialSlot::SteelIngot, {GW::EncStrings::SteelIngot, GW::Constants::ItemID::SteelIngot}},
                {MaterialSlot::DeldrimorSteelIngot, {GW::EncStrings::DeldrimorSteelIngot, GW::Constants::ItemID::DeldrimorSteelIngot}},
                {MaterialSlot::MonstrousClaw, {GW::EncStrings::MonstrousClaw, GW::Constants::ItemID::MonstrousClaw}},
                {MaterialSlot::MonstrousEye, {GW::EncStrings::MonstrousEye, GW::Constants::ItemID::MonstrousEye}},
                {MaterialSlot::MonstrousFang, {GW::EncStrings::MonstrousFang, GW::Constants::ItemID::MonstrousFang}},
                {MaterialSlot::Ruby, {GW::EncStrings::Ruby, GW::Constants::ItemID::Ruby}},
                {MaterialSlot::Sapphire, {GW::EncStrings::Sapphire, GW::Constants::ItemID::Sapphire}},
                {MaterialSlot::Diamond, {GW::EncStrings::Diamond, GW::Constants::ItemID::Diamond}},
                {MaterialSlot::OnyxGemstone, {GW::EncStrings::OnyxGemstone, GW::Constants::ItemID::OnyxGemstone}},
                {MaterialSlot::LumpofCharcoal, {GW::EncStrings::LumpofCharcoal, GW::Constants::ItemID::LumpofCharcoal}},
                {MaterialSlot::ObsidianShard, {GW::EncStrings::ObsidianShard, GW::Constants::ItemID::ObsidianShard}},
                {MaterialSlot::TemperedGlassVial, {GW::EncStrings::TemperedGlassVial, GW::Constants::ItemID::TemperedGlassVial}},
                {MaterialSlot::LeatherSquare, {GW::EncStrings::LeatherSquare, GW::Constants::ItemID::LeatherSquare}},
                {MaterialSlot::ElonianLeatherSquare, {GW::EncStrings::ElonianLeatherSquare, GW::Constants::ItemID::ElonianLeatherSquare}},
                {MaterialSlot::VialofInk, {GW::EncStrings::VialofInk, GW::Constants::ItemID::VialofInk}},
                {MaterialSlot::RollofParchment, {GW::EncStrings::RollofParchment, GW::Constants::ItemID::RollofParchment}},
                {MaterialSlot::RollofVellum, {GW::EncStrings::RollofVellum, GW::Constants::ItemID::RollofVellum}},
                {MaterialSlot::SpiritwoodPlank, {GW::EncStrings::SpiritwoodPlank, GW::Constants::ItemID::SpiritwoodPlank}},
                {MaterialSlot::AmberChunk, {GW::EncStrings::AmberChunk, GW::Constants::ItemID::AmberChunk}},
                {MaterialSlot::JadeiteShard, {GW::EncStrings::JadeiteShard, GW::Constants::ItemID::JadeiteShard}},
            };
            const auto found = material_info.find(slot);
            return found == material_info.end() ? nullptr : &found->second;
        }

        uint32_t GetUses(const GW::Item* item)
        {
            if (!item) return 0;
            const GW::ItemModifier* mod = item->mod_struct;
            for (DWORD i = 0; mod && i < item->mod_struct_size; i++) {
                if (mod->identifier() == 0x2458) {
                    return mod->arg2() * item->quantity;
                }
                mod++;
            }
            return item->quantity;
        }

        uint32_t GetAlcoholPointsPerUse(const GW::Item* item)
        {
            if (!item) return 0;
            switch (item->model_id) {
                case GW::Constants::ItemID::Eggnog:
                case GW::Constants::ItemID::DwarvenAle:
                case GW::Constants::ItemID::HuntersAle:
                case GW::Constants::ItemID::Absinthe:
                case GW::Constants::ItemID::WitchsBrew:
                case GW::Constants::ItemID::Ricewine:
                case GW::Constants::ItemID::ShamrockAle:
                case GW::Constants::ItemID::Cider:
                    return 1;
                case GW::Constants::ItemID::Grog:
                case GW::Constants::ItemID::SpikedEggnog:
                case GW::Constants::ItemID::AgedDwarvenAle:
                case GW::Constants::ItemID::AgedHuntersAle:
                case GW::Constants::ItemID::FlaskOfFirewater:
                case GW::Constants::ItemID::KrytanBrandy:
                case GW::Constants::ItemID::Keg:
                    return 5;
            }
            return 0;
        }

        bool IsAlcohol(const GW::Item* item)
        {
            return GetAlcoholPointsPerUse(item) > 0;
        }

        const char* GetAttributeName(const GW::Constants::AttributeByte attribute)
        {
            const auto attribute_data = GW::SkillbarMgr::GetAttributeConstantData((GW::Constants::Attribute)attribute);

            return attribute_data ? Resources::DecodeStringId(attribute_data->name_id)->string().c_str() : "Unknown";
        }

        const char* GetDamageTypeName(const GW::Constants::DamageType type)
        {
            switch (type) {
                case GW::Constants::DamageType::Blunt:
                    return "Blunt";
                case GW::Constants::DamageType::Piercing:
                    return "Piercing";
                case GW::Constants::DamageType::Slashing:
                    return "Slashing";
                case GW::Constants::DamageType::Cold:
                    return "Cold";
                case GW::Constants::DamageType::Lightning:
                    return "Lightning";
                case GW::Constants::DamageType::Fire:
                    return "Fire";
                case GW::Constants::DamageType::Chaos:
                    return "Chaos";
                case GW::Constants::DamageType::Dark:
                case GW::Constants::DamageType::DarkDupe:
                    return "Dark";
                case GW::Constants::DamageType::Holy:
                    return "Holy";
                case GW::Constants::DamageType::Nature:
                    return "Nature";
                case GW::Constants::DamageType::Sacrifice:
                    return "Sacrifice";
                case GW::Constants::DamageType::Earth:
                    return "Earth";
                case GW::Constants::DamageType::Generic:
                    return "Generic";
                case GW::Constants::DamageType::None:
                    return "None";
                default:
                    return "Unknown";
            }
        }

        const char* GetRarityName(const GW::Constants::Rarity rarity)
        {
            switch (rarity) {
                case GW::Constants::Rarity::White:
                    return "White";
                case GW::Constants::Rarity::Blue:
                    return "Blue";
                case GW::Constants::Rarity::Purple:
                    return "Purple";
                case GW::Constants::Rarity::Gold:
                    return "Gold";
                case GW::Constants::Rarity::Green:
                    return "Green";
            }
            return "Unknown";
        }

    } // namespace Items

    namespace Effects {

        // Effect IDs for custom (synthetic) effects use the high byte 0x0f to avoid collisions.
        // The full ID is 0x0f000000 | skill_id, making it deterministic and recyclable per skill.
        static constexpr uint32_t custom_effect_id_base = 0x0f000000;

        uint32_t AddCustomEffect(const GW::Constants::SkillID skill_id, const float duration_seconds)
        {
            const auto player_effects = GW::Effects::GetPlayerEffectsArray();
            if (!player_effects) return 0;
            auto& arr = player_effects->effects;

            const uint32_t target_id = custom_effect_id_base | static_cast<uint32_t>(skill_id);

            RemoveCustomEffect(target_id);

            GW::Effect new_effect{};
            new_effect.skill_id = skill_id;
            new_effect.effect_id = target_id;
            new_effect.duration = duration_seconds;
            new_effect.timestamp = GW::MemoryMgr::GetSkillTimer();
            new_effect.attribute_level = 0;
            new_effect.agent_id = 0;

            GW::UI::UIPacket::kEffectAdd packet{};
            packet.agent_id = GW::Agents::GetControlledCharacterId();
            packet.effect = GW::MemoryMgr::AddToGuildWarsArray(arr, new_effect);
            GW::UI::SendUIMessage(GW::UI::UIMessage::kEffectAdd, &packet);
            return target_id;
        }

        bool RemoveCustomEffect(const uint32_t effect_id)
        {
            if ((effect_id & 0xff000000) != custom_effect_id_base) return false;
            const auto player_effects = GW::Effects::GetPlayerEffectsArray();
            if (!player_effects) return false;
            auto& arr = player_effects->effects;
            for (uint32_t i = 0; i < arr.m_size; i++) {
                if (arr.m_buffer[i].effect_id != effect_id) continue;
                GW::MemoryMgr::RemoveFromGwArray(arr, i);
                GW::UI::SendUIMessage(GW::UI::UIMessage::kEffectRemove, reinterpret_cast<void*>(static_cast<uintptr_t>(effect_id)));
                return true;
            }
            return false;
        }

    } // namespace Effects

} // namespace GW

namespace ToolboxUtils {
    bool FrameRateCheck(clock_t& last_checked, clock_t fps)
    {
        const auto now = TIMER_INIT();
        if (now - last_checked > CLOCKS_PER_SEC / fps) {
            last_checked = now;
            return true;
        }
        return false;
    }
    std::wstring TimeToEncString(clock_t time)
    {
        static constexpr std::pair<clock_t, wchar_t> units[] = {
            {60 * 60 * 24 * 7, 0x768}, {60 * 60 * 24, 0x767}, {3 * 60 * 60, 0x766}, {60, 0x765}, {1, 0x764},
        };

        for (const auto& [divisor, token] : units) {
            if (time < divisor) continue;
            wchar_t buf[4];
            ASSERT(GW::UI::UInt32ToEncStr(time / divisor, buf, _countof(buf)));
            return std::format(L"\x763\x101{}\x10A{}\x1", buf, token);
        }

        return L"\x101";
    }

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

    bool IsOutpost()
    {
        return GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost;
    }

    bool IsExplorable()
    {
        return GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable;
    }

    uint8_t GetMissionState(GW::Constants::MapID map_id, const GW::Array<uint32_t>& missions_completed, const GW::Array<uint32_t>& missions_bonus)
    {
        const auto area_info = GW::Map::GetMapInfo(map_id);
        if (!area_info) return 0;
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

        if (primary) state_out |= MissionState::Primary;
        if (expert) state_out |= MissionState::Expert;
        if (master) state_out |= MissionState::Master;
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

    bool IsHeroUnlocked(GW::Constants::HeroID hero_id)
    {
        const auto w = GW::GetWorldContext();
        if (!(w && w->hero_info.size())) {
            return false;
        }
        for (auto& a : w->hero_info) {
            if (a.hero_id != hero_id) continue;
            switch (hero_id) {
                case GW::Constants::HeroID::Merc1:
                case GW::Constants::HeroID::Merc2:
                case GW::Constants::HeroID::Merc3:
                case GW::Constants::HeroID::Merc4:
                case GW::Constants::HeroID::Merc5:
                case GW::Constants::HeroID::Merc6:
                case GW::Constants::HeroID::Merc7:
                case GW::Constants::HeroID::Merc8:
                    return (a.appearance_bitmap && !wcseq(GW::AccountMgr::GetCurrentPlayerName(), a.name)); // Unlocked, but not assigned.
            }
            return true;
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
        if (!(item && item->info_string && *item->info_string)) return L"";
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
                    auto capture = stacking_match.template get<1>().to_view();
                    swprintf(buffer, _countof(buffer), L"\x2\xAA8\x10A\xA84\x10A%ls\x1\x101\x101\x1", capture.data());
                    original += buffer;
                }

                static constexpr ctll::fixed_string armor_pattern = L"\xA3B\x10A\xA86\x10A\xA44\x1\x101(.)\x1\x2";
                if (auto armor_match = ctre::match<armor_pattern>(item_str)) {
                    auto capture = armor_match.template get<1>().to_view();
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
        original = TextUtils::ctre_regex_replace_with_formatter<L".\x10A\x0AA8\x10A\xAA9\x10A.\x1\x101.\x1\x1">(original, [&buffer](auto& match) -> std::wstring {
            auto found = match.template get<0>().to_view();
            swprintf(buffer, _countof(buffer), L"\x108\x107, q%d \x1\x2%c", found.at(9) - 0x100, found.at(6));
            return buffer;
        });

        // Replace "Requires 9 Scythe Mastery" > "q9 Scythe Mastery"
        original = TextUtils::ctre_regex_replace_with_formatter<L".\x10A\xAA8\x10A\xAA9\x10A\x8101.\x1\x101.\x1\x1">(original, [&buffer](auto& match) -> std::wstring {
            auto found = match.template get<0>().to_view();
            swprintf(buffer, _countof(buffer), L"\x108\x107, q%d \x1\x2\x8101%c", found.at(10) - 0x100, found.at(7));
            return buffer;
        });

        // "vs. Earth damage" > "Earth"
        original = TextUtils::ctre_regex_replace_with_formatter<L"[\xAAC\xAAF]\x10A.\x1">(original, [&buffer](auto& match) -> std::wstring {
            auto found = match.template get<0>().to_view();
            swprintf(buffer, _countof(buffer), L"%c", found.at(2));
            return buffer;
        });

        // Replace "Lengthens ??? duration on foes by 33%" > "??? duration +33%"
        original = TextUtils::ctre_regex_replace_with_formatter<L"\xAA4\x10A.\x1">(original, [&buffer](auto& match) -> std::wstring {
            auto found = match.template get<0>().to_view();
            swprintf(buffer, _countof(buffer), L"%c\x2\x108\x107 +33%%\x1", found.at(2));
            return buffer;
        });

        // Replace "Reduces ??? duration on you by 20%" > "??? duration -20%"
        original = TextUtils::ctre_regex_replace_with_formatter<L"\xAA7\x10A.\x1">(original, [&buffer](auto& match) -> std::wstring {
            auto found = match.template get<0>().to_view();
            swprintf(buffer, _countof(buffer), L"%c\x2\x108\x107 -20%%\x1", found.at(2));
            return buffer;
        });

        // Change " (while Health is above n)" to "^n";
        original = TextUtils::ctre_regex_replace_with_formatter<L"\xAA8\x10A\xABC\x10A\xA52\x1\x101.\x1">(original, [&buffer](auto& match) -> std::wstring {
            auto found = match.template get<0>().to_view();
            swprintf(buffer, _countof(buffer), L"\x108\x107^%d\x1", found.at(7) - 0x100);
            return buffer;
        });

        // Change "Enchantments last 20% longer" to "Ench +20%"
        original = TextUtils::ctre_regex_replace_with_formatter<L"\xAA2\x101.">(original, [&buffer](auto& match) -> std::wstring {
            auto found = match.template get<0>().to_view();
            swprintf(
                buffer, _countof(buffer),
                L"\x108\x107"
                L"Enchantments +%d%%\x1",
                found.at(2) - 0x100
            );
            return buffer;
        });

        // "(Chance: 18%)" > "(18%)"
        original = TextUtils::ctre_regex_replace_with_formatter<L"\xA87\x10A\xA48\x1\x101.">(original, [&buffer](auto& match) -> std::wstring {
            auto found = match.template get<0>().to_view();
            swprintf(buffer, _countof(buffer), L"\x108\x107%d%%\x1", found.at(5) - 0x100);
            return buffer;
        });

        // Change "Halves skill recharge of <attribute> spells" > "HSR <attribute>"
        original = TextUtils::ctre_regex_replace_with_formatter<L"\xA81\x10A\xA58\x1\x10B.\x1">(original, [&buffer](auto& match) -> std::wstring {
            auto found = match.template get<0>().to_view();
            swprintf(
                buffer, _countof(buffer),
                L"\x108\x107"
                L"HSR \x1\x2%c",
                found.at(5)
            );
            return buffer;
        });

        // Change "Inscription: "Blah Blah"" to just "Blah Blah"
        original = TextUtils::ctre_regex_replace_with_formatter<L"\x8101\x5DC5\x10A..\x1">(original, [&buffer](auto& match) -> std::wstring {
            auto found = match.template get<0>().to_view();
            swprintf(buffer, _countof(buffer), L"%c%c", found.at(3), found.at(4));
            return buffer;
        });

        // Change "Halves casting time of <attribute> spells" > "HCT <attribute>"
        original = TextUtils::ctre_regex_replace_with_formatter<L"\xA81\x10A\xA47\x1\x10B.\x1">(original, [&buffer](auto& match) -> std::wstring {
            auto found = match.template get<0>().to_view();
            swprintf(
                buffer, _countof(buffer),
                L"\x108\x107"
                L"HCT \x1\x2%c",
                found.at(5)
            );
            return buffer;
        });

        // Change "Piercing Dmg: 11-22" > "Piercing: 11-22"
        original = TextUtils::ctre_regex_replace_with_formatter<L"\xA89\x10A\xA4E\x1\x10B.\x1\x101.\x102.">(original, [&buffer](auto& match) -> std::wstring {
            auto found = match.template get<0>().to_view();
            swprintf(buffer, _countof(buffer), L"%c\x2\x108\x107: %d-%d\x1", found.at(5), found.at(8) - 0x100, found.at(10) - 0x100);
            return buffer;
        });

        // Change "Life draining -3, Health regeneration -1" > "Vampiric" (add at end of description)
        static constexpr ctll::fixed_string vampiric_pattern = L"\x2\x102\x2.\x10A\xA86\x10A\xA54\x1\x101.\x1\x2\x102\x2.\x10A\xA7E\x10A\xA53\x1\x101.\x1";
        if (ctre::match<vampiric_pattern>(original)) {
            original = TextUtils::ctre_regex_replace<vampiric_pattern, L"">(original);
            original += L"\x2\x102\x2\x108\x107"
                        L"Vampiric\x1";
        }

        // Change "Energy gain on hit 1, Energy regeneration -1" > "Zealous" (add at end of description)
        static constexpr ctll::fixed_string zealous_pattern = L"\x2\x102\x2.\x10A\xA86\x10A\xA50\x1\x101.\x1\x2\x102\x2.\x10A\xA7E\x10A\xA51\x1\x101.\x1";
        if (ctre::match<zealous_pattern>(original)) {
            original = TextUtils::ctre_regex_replace<zealous_pattern, L"">(original);
            original += L"\x2\x102\x2\x108\x107"
                        L"Zealous\x1";
        }

        // Change "Damage" > "Dmg"
        original = TextUtils::str_replace_all(original, L"\xA4C", L"\xA4E");

        // Change Bow "Two-Handed" > ""
        original = TextUtils::str_replace_all(original, L"\x8102\x1227", L"\xA3E");

        // Change "Halves casting time of spells" > "HCT"
        original = TextUtils::str_replace_all(
            original, L"\xA80\x10A\xA47\x1",
            L"\x108\x107"
            L"HCT\x1"
        );

        // Change "Halves skill recharge of spells" > "HSR"
        original = TextUtils::str_replace_all(
            original, L"\xA80\x10A\xA58\x1",
            L"\x108\x107"
            "HSR\x1"
        );

        // Remove (Stacking) and (Non-stacking) rubbish
        original = TextUtils::ctre_regex_replace<L"\x2.\x10A\xAA8\x10A[\xAB1\xAB2]\x1\x1", L"">(original);

        // Replace (while affected by a(n) to just (n)
        original = TextUtils::ctre_regex_replace_with_formatter<L"\x8101\x4D9C\x10A.\x1">(original, [](auto& match) -> std::wstring {
            auto found = match.template get<0>().to_view();
            return std::wstring(1, found.at(3));
        });

        // Replace (while xxx) to just (xxx)
        original = TextUtils::str_replace_all(
            original, L"\xAB4",
            L"\x108\x107"
            L"Attacking\x1"
        );
        original = TextUtils::str_replace_all(
            original, L"\xAB5",
            L"\x108\x107"
            L"Casting\x1"
        );
        original = TextUtils::str_replace_all(
            original, L"\xAB6",
            L"\x108\x107"
            L"Condition\x1"
        );
        original = TextUtils::ctre_regex_replace<
            L"[\xAB7\x4B6]", L"\x108\x107"
                             L"Enchanted\x1">(original);
        original = TextUtils::ctre_regex_replace<
            L"[\xAB8\x4B4]", L"\x108\x107"
                             L"Hexed\x1">(original);
        original = TextUtils::ctre_regex_replace<
            L"[\xAB9\xABA]", L"\x108\x107"
                             L"Stance\x1">(original);

        // Combine Attribute + 3, Attribute + 1 to Attribute +3 +1 (e.g. headpiece)
        original = TextUtils::ctre_regex_replace_with_formatter<L".\x10A\xA84\x10A.\x1\x101.\x1\x2\x102\x2.\x10A\xA84\x10A.\x1\x101.\x1">(original, [&buffer](auto& match) -> std::wstring {
            auto found = match.template get<0>().to_view();
            if (found[4] != found[16]) {
                return std::wstring(found); // Different attributes, return unchanged
            }
            swprintf(buffer, _countof(buffer), L"%c\x10A\xA84\x10A%c\x1\x101%c\x2\xA84\x101%c\x1", found[0], found[4], found[7], found[19]);
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
            L".\x10A\x108\x10A\x8103\xB71\x101\x100\x1\x1", L"\xA85\x10A\xA4E\x1\x101\x114\x2\xAA8\x10A\x108\x107"
                                                            L"Festival\x1\x1">(original);

        // Check for customized item with +20% damage
        static constexpr ctll::fixed_string dmg_plus_20_pattern = L"\x2\x102\x2.\x10A\xA85\x10A[\xA4C\xA4E]\x1\x101\x114\x1";
        if (item->customized && ctre::search<dmg_plus_20_pattern>(original)) {
            // Remove "\nDamage +20%" > "\n"
            original = TextUtils::ctre_regex_replace<dmg_plus_20_pattern, L"">(original);
            // Append "Customized"
            original += L"\x2\x102\x2\x108\x107"
                        L"Customized\x1";
        }

        return original;
    }

    GuiUtils::EncString* GetProfessionName(const GW::Constants::Profession profession)
    {
        const auto idx = static_cast<size_t>(profession);
        const auto str_id = idx < std::size(GW::EncStrings::Profession) ? GW::EncStrings::Profession[idx] : 1u;
        return Resources::DecodeStringId(str_id);
    }

    GuiUtils::EncString* GetProfessionAcronym(const GW::Constants::Profession profession)
    {
        const auto idx = static_cast<size_t>(profession);
        const auto str_id = idx < std::size(GW::EncStrings::ProfessionAcronym) ? GW::EncStrings::ProfessionAcronym[idx] : 1u;
        return Resources::DecodeStringId(str_id);
    }
} // namespace ToolboxUtils
