#include "stdafx.h"

#include <GWCA/Utilities/Scanner.h>
#include <GWCA/Utilities/Hooker.h>

#include <GWCA/Context/MapContext.h>
#include <GWCA/Context/GameplayContext.h>
#include <GWCA/Context/CharContext.h>

#include <GWCA/Constants/Constants.h>

#include <GWCA/GameContainers/Array.h>
#include <GWCA/GameContainers/GamePos.h>

#include <GWCA/GameEntities/Camera.h>
#include <GWCA/GameEntities/Party.h>
#include <GWCA/GameEntities/Quest.h>
#include <GWCA/GameEntities/Skill.h>
#include <GWCA/GameEntities/Item.h>
#include <GWCA/GameEntities/Player.h>
#include <GWCA/GameEntities/Guild.h>
#include <GWCA/GameEntities/NPC.h>
#include <GWCA/GameEntities/Map.h>
#include <GWCA/GameEntities/Title.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Pathing.h>
#include <GWCA/GameEntities/Frame.h>


#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/WorldContext.h>
#include <GWCA/Context/AccountContext.h>

#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/ItemMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/PlayerMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/EffectMgr.h>
#include <GWCA/Managers/GuildMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/CameraMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Managers/QuestMgr.h>
#include <GWCA/Managers/RenderMgr.h>
#include <GWCA/Managers/EventMgr.h>

#include <Widgets/AlcoholWidget.h>
#include <Widgets/Minimap/Minimap.h>
#include <Widgets/PartyDamage.h>
#include <Windows/InfoWindow.h>
#include <Windows/NotepadWindow.h>

#include <Modules/ItemDescriptionHandler.h>
#include <Modules/ResignLogModule.h>
#include <Modules/ToolboxSettings.h>
#include <Modules/DialogModule.h>
#include <Modules/GwDatTextureModule.h>
#include <Modules/HallOfMonumentsModule.h>
#include <Modules/AudioSettings.h>
#include <Modules/Resources.h>
#include <Utils/ToolboxUtils.h>
#include <Utils/ArenaNetFileParser.h>
#include <Utils/TextUtils.h>

#include <Logger.h>
#include <GWToolbox.h>

#include <CircurlarBuffer.h>
#include <Widgets/WorldMapWidget.h>


namespace {

    wchar_t mapfile[8] = {0};
    std::map<std::wstring, HallOfMonumentsAchievements*> target_achievements;
    clock_t send_timer = 0;
    uint32_t last_hovered_item_id = 0;
    uint32_t quoted_item_id = 0;
    GW::Constants::SkillID last_hovered_skill_id = static_cast<GW::Constants::SkillID>(0);

    bool show_widgets = true;
    bool show_open_chest = true;
    bool show_player = true;
    bool show_target = true;
    bool show_map = true;
    bool show_dialog = true;
    bool show_item = true;
    bool show_mobcount = true;
    bool show_quest = true;
    bool show_resignlog = true;

    GW::HookEntry MessageCore_Entry;
    GW::HookEntry InstanceLoadFile_Entry;
    GW::HookEntry OnDialogBody_Entry;
    GW::HookEntry OnDialogButton_Entry;
    GW::HookEntry OnSendDialog_Entry;

    int pending_map_names = 1;
    std::unordered_map<uint32_t,GuiUtils::EncString*> encoded_name_id_to_string;

    bool record_textures = false;
    bool record_ui_messages = false;
    bool record_event_messages = false;
    bool record_enc_strings = false;

    bool EncInfoField(const char* label, const wchar_t* enc_string)
    {
        std::string info_string;
        const size_t size_reqd = enc_string ? wcslen(enc_string) * 7 + 1 : 0;
        info_string.resize(size_reqd, 0); // 7 chars = 0xFFFF plus a space
        size_t offset = 0;
        for (size_t i = 0; enc_string && enc_string[i] && offset < size_reqd - 1; i++) {
            offset += sprintf(&info_string[offset], "0x%X ", enc_string[i]);
        }
        return ImGui::InputTextEx(label, nullptr, info_string.data(), info_string.size(), ImVec2(-160.f * ImGui::GetIO().FontGlobalScale, 0), ImGuiInputTextFlags_ReadOnly);
    }

    bool InfoField(const char* label, const char* fmt, ...)
    {
        char info_string[128];
        va_list vl;
        va_start(vl, fmt);
        vsnprintf(info_string, _countof(info_string), fmt, vl);
        info_string[127] = 0;
        va_end(vl);
        return ImGui::InputTextEx(label, nullptr, info_string, _countof(info_string), ImVec2(-160.f * ImGui::GetIO().FontGlobalScale, 0), ImGuiInputTextFlags_ReadOnly);
    }

    void GetIdsFromFileId(const uint32_t param_1, short* param_2)
    {
        param_2[1] = static_cast<short>((param_1 - 1) / 0xff00) + 0x100;
        *param_2 = static_cast<short>((param_1 - 1) % 0xff00) + 0x100;
    }

    void DrawMapInfo(GW::Constants::MapID map_id) {
        static char info_id[16];
        snprintf(info_id, _countof(info_id), "map_info_%d", map_id);
        ImGui::PushID(info_id);
        auto type = "";
        switch (GW::Map::GetInstanceType()) {
        case GW::Constants::InstanceType::Outpost:
            type = "Outpost\0\0\0";
            break;
        case GW::Constants::InstanceType::Explorable:
            type = "Explorable";
            break;
        case GW::Constants::InstanceType::Loading:
            type = "Loading\0\0\0";
            break;
        }
        InfoField("Map ID", "%d", map_id);
        ImGui::ShowHelp("Map ID is unique for each area");
        InfoField("Map Region", "%d", GW::Map::GetRegion());
        InfoField("Map District", "%d", GW::Map::GetDistrict());
        InfoField("Map Type", type);
        EncInfoField("Map File ID", mapfile);
        ImGui::ShowHelp("Map file is unique for each pathing map (e.g. used by minimap).\nMany different maps use the same map file");
        if (ImGui::TreeNodeEx("Advanced", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
            const GW::AreaInfo* map_info = GW::Map::GetMapInfo(map_id);
            if (map_info) {
                InfoField("Campaign", "%d", map_info->campaign);
                InfoField("Continent", "%d", map_info->continent);
                InfoField("Region", "%d", map_info->region);
                InfoField("Type", "%d", map_info->type);
                InfoField("Mission Complete?", "%d", ToolboxUtils::GetMissionState(GW::Map::GetMapID(), GW::PartyMgr::GetIsPartyInHardMode()));
                InfoField("Flags", "0x%X", map_info->flags);
                InfoField("Thumbnail ID", "%d", map_info->thumbnail_id);
                const auto m = GW::GetMapContext();
                if (m) {
                    InfoField("Map Boundaries", "%.0f, %.0f, %.0f, %.0f, %.0f", m->start_pos.x, m->start_pos.y, m->end_pos.x, m->end_pos.y);
                }
                GW::Vec2f pos = {static_cast<float>(map_info->x), static_cast<float>(map_info->y)};
                InfoField("Map Pos", "%.2f, %.2f", pos.x, pos.y);
                if (!pos.x) {
                    pos.x = static_cast<float>(map_info->icon_start_x + (map_info->icon_end_x - map_info->icon_start_x) / 2);
                    pos.y = static_cast<float>(map_info->icon_start_y + (map_info->icon_end_y - map_info->icon_start_y) / 2);
                }
                if (!pos.x) {
                    pos.x = static_cast<float>(map_info->icon_start_x_dupe + (map_info->icon_end_x_dupe - map_info->icon_start_x_dupe) / 2);
                    pos.y = static_cast<float>(map_info->icon_start_y_dupe + (map_info->icon_end_y_dupe - map_info->icon_start_y_dupe) / 2);
                }
                InfoField("Calculated Pos", "%.2f, %.2f", pos.x, pos.y);
                static wchar_t name_enc[8];
                if (GW::UI::UInt32ToEncStr(map_info->name_id, name_enc, 8)) {
                    EncInfoField("Name Enc", name_enc);
                }
                static wchar_t desc_enc[8];
                if (GW::UI::UInt32ToEncStr(map_info->description_id, desc_enc, 8)) {
                    EncInfoField("Desc Enc", desc_enc);
                }
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
    }

    void DrawSkillInfo(GW::Skill* skill, GuiUtils::EncString* name, const bool force_advanced = false)
    {
        if (!skill) {
            return;
        }
        name->reset(skill->name);
        static char info_id[16];
        snprintf(info_id, _countof(info_id), "skill_info_%d", skill->skill_id);
        ImGui::PushID(info_id);
        InfoField("SkillID", "%d", skill->skill_id);
        InfoField("Name", "%s", name->string().c_str());
        auto draw_advanced = [&, skill] {
            InfoField("Addr", "%p", skill);
            InfoField("Type", "%d", skill->type);
            short file_ids[2];
            GetIdsFromFileId(skill->icon_file_id, file_ids);
            InfoField("FileIds", "%08x %04x %04x", skill->icon_file_id, file_ids[0], file_ids[1]);
            GetIdsFromFileId(skill->icon_file_id_2, file_ids);
            InfoField("FileIds2", "%04x %04x", file_ids[0], file_ids[1]);
            EncInfoField("Name Enc", name->encoded().c_str());
            wchar_t out[8];
            GW::UI::UInt32ToEncStr(skill->description, out, _countof(out));
            EncInfoField("Desc Enc", out);
        };
        if (force_advanced) {
            draw_advanced();
        }
        else if (ImGui::TreeNodeEx("Advanced##skill", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
            draw_advanced();
            ImGui::TreePop();
        }
        ImGui::PopID();
    }

    void DrawGuildInfo(GW::Guild* guild)
    {
        if (!guild) {
            return;
        }
        ImGui::PushID(guild->index);
        if (ImGui::TreeNodeEx("Guild Info", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
            ImGui::PushID("guild_info");
            InfoField("Addr", "0x%p", guild);
            InfoField("Name", "%s [%s]", TextUtils::WStringToString(guild->name).c_str(), TextUtils::WStringToString(guild->tag).c_str());
            InfoField("Faction", "%d (%s)", guild->faction_point, guild->faction ? "Luxon" : "Kurzick");
            ImGui::PopID();
            ImGui::TreePop();
        }
        ImGui::PopID();
    }

    void DrawHomAchievements(const GW::Player* player)
    {
        if (ImGui::TreeNodeEx("Hall of Monuments Info", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
            if (!target_achievements.contains(player->name)) {
                auto* achievements = new HallOfMonumentsAchievements();
                achievements->character_name = player->name;
                target_achievements[achievements->character_name] = achievements;
                HallOfMonumentsModule::AsyncGetAccountAchievements(achievements->character_name, achievements);
            }
            const auto hom_result = target_achievements[player->name];
            if (ImGui::Button("Go to Hom Calculator")) {
                hom_result->OpenInBrowser();
            }
            InfoField("Devotion Points", "%d/%d", hom_result->devotion_points_total, 8);
            InfoField("Fellowship Points", "%d/%d", hom_result->fellowship_points_total, 8);
            InfoField("Honor Points", "%d/%d", hom_result->honor_points_total, 18);
            InfoField("Resilience Points", "%d/%d", hom_result->resilience_points_total, 8);
            InfoField("Valor Points", "%d/%d", hom_result->valor_points_total, 8);
            ImGui::TreePop();
        }
    }

    void DrawItemInfo(GW::Item* item, GuiUtils::EncString* name, const bool force_advanced = false)
    {
        if (!item) {
            return;
        }
        name->reset(item->single_item_name);
        static char slot[8] = "-";
        if (item->bag) {
            snprintf(slot, _countof(slot), "%d/%d", item->bag->index + 1, item->slot + 1);
        }
        InfoField("Bag/Slot", "%s", slot);
        InfoField("ModelID", "%d", item->model_id);
        InfoField("Name", "%s", name->string().c_str());
        ImGui::Image(*Resources::GetItemImage(item), { 48,48 });
        auto draw_advanced = [&, item] {
            InfoField("Addr", "%p", item);
            InfoField("Id", "%d", item->item_id);
            InfoField("Type", "%d", item->type);
            InfoField("Interaction", "0x%X", item->interaction);
            InfoField("model_file_id", "0x%X", item->model_file_id);
            EncInfoField("Name Enc", item->name_enc);
            EncInfoField("Name Enc no mods", ItemDescriptionHandler::GetItemEncNameWithoutMods(item).c_str());
            EncInfoField("Complete Name Enc", item->complete_name_enc);
            EncInfoField("Desc Enc", item->info_string);
            if (item->mod_struct_size) {
                ImGui::Text("Mod Struct (identifier, arg1, arg2)");
                char mod_struct_label[] = "###Mod Struct 1";
                for (size_t i = 0; i < item->mod_struct_size; i++) {
                    const GW::ItemModifier* mod = &item->mod_struct[i];
                    mod_struct_label[14] = static_cast<char>(i + 1) + '0';
                    InfoField(mod_struct_label, "0x%X (%d %d %d)", mod->mod, mod->identifier(), mod->arg1(), mod->arg2());
                }
            }
        };
        if (force_advanced) {
            draw_advanced();
        }
        else if (ImGui::TreeNodeEx("Advanced##item", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
            draw_advanced();
            ImGui::TreePop();
        }
    }

    void DrawAgentInfo(GW::Agent* agent)
    {
        if (!agent) {
            return;
        }
        const GW::AgentLiving* living = agent->GetAsAgentLiving();
        const bool is_player = agent->agent_id == GW::Agents::GetControlledCharacterId();
        const GW::AgentGadget* gadget = agent->GetAsAgentGadget();
        const GW::AgentItem* item = agent->GetAsAgentItem();
        GW::Item* item_actual = item ? GW::Items::GetItemById(item->item_id) : nullptr;
        const GW::Player* player = living && living->IsPlayer() ? GW::PlayerMgr::GetPlayerByID(living->player_number) : nullptr;
        const GW::Agent* me = GW::Agents::GetControlledCharacter();
        uint32_t npc_id = living && living->IsNPC() ? living->player_number : 0;
        if (player && living->transmog_npc_id & 0x20000000) {
            npc_id = living->transmog_npc_id ^ 0x20000000;
        }
        const GW::NPC* npc = npc_id ? GW::Agents::GetNPCByID(npc_id) : nullptr;

        GW::Guild* guild = nullptr;
        if (player && living->tags->guild_id) {
            GW::GuildArray* guilds = GW::GuildMgr::GetGuildArray();
            if (guilds && living->tags->guild_id < guilds->size()) {
                guild = guilds->at(living->tags->guild_id);
            }
        }

        InfoField("Agent ID", "%d", agent->agent_id);
        ImGui::ShowHelp("Agent ID is unique for each agent in the instance,\nIt's generated on spawn and will change in different instances.");
        InfoField("X pos", "%.2f", agent->pos.x);
        InfoField("Y pos", "%.2f", agent->pos.y);
        GW::Vec2f world_map_pos;
        WorldMapWidget::GamePosToWorldMap(agent->pos, world_map_pos);
        InfoField("World Map X pos", "%.2f", world_map_pos.x);
        InfoField("World Map Y pos", "%.2f", world_map_pos.y);
        const float speed = sqrtf(agent->move_x * agent->move_x + agent->move_y * agent->move_y);
        InfoField("Speed (Relative)", "%.2f (%.2f) ", speed, speed > 0.f ? speed / 288.0f : 0.f);
        if (living) {
            InfoField(living->IsPlayer() ? "Player ID" : "Model ID", "%d", living->player_number);
            ImGui::ShowHelp("Model ID is unique for each kind of agent.\n"
                "It is static and shared by the same agents.\n"
                "When targeting players, this is Player ID instead, unique for each player in the instance.\n"
                "For the purpose of targeting hotkeys and commands, use this value");
        }
        if (item && item_actual) {
            if (ImGui::TreeNodeEx("Item Info", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
                static GuiUtils::EncString item_name;
                DrawItemInfo(item_actual, &item_name);
                ImGui::TreePop();
            }
        }
        if (player) {
            if (ImGui::TreeNodeEx("Player Info", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
                ImGui::PushID("player_info");
                InfoField("Addr", "%p", player);
                InfoField("Name", "%s", TextUtils::WStringToString(player->name).c_str());
                if (player->active_title_tier) {
                    const GW::TitleTier& tier = GW::GetGameContext()->world->title_tiers[player->active_title_tier];
                    static GuiUtils::EncString title_enc_string;
                    title_enc_string.reset(tier.tier_name_enc);
                    InfoField("Current Title", "%s", title_enc_string.string().c_str());
                }
                ImGui::PopID();
                ImGui::TreePop();
            }
            DrawHomAchievements(player);
        }
        DrawGuildInfo(guild);
        if (is_player && ImGui::TreeNodeEx("Effects", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
            GW::EffectArray* effects = GW::Effects::GetAgentEffects(agent->agent_id);
            if (effects) {
                for (auto& effect : *effects) {
                    ImGui::Text("id: %d | attrib level: %d | skill: %d | duration: %u", effect.effect_id, effect.attribute_level, effect.skill_id, effect.GetTimeRemaining() / 1000);
                }
            }
            ImGui::TreePop();
        }
        if (is_player && ImGui::TreeNodeEx("Buffs", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
            GW::BuffArray* effects = GW::Effects::GetAgentBuffs(agent->agent_id);
            if (effects) {
                for (const auto& effect : *effects) {
                    ImGui::Text("id: %d", effect.skill_id);
                    if (effect.target_agent_id) {
                        ImGui::SameLine();
                        ImGui::Text(" target: %d", effect.target_agent_id);
                    }
                }
            }
            ImGui::TreePop();
        }
        if (ImGui::TreeNodeEx("Advanced", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
            InfoField("Addr", "%p", agent);
            EncInfoField("Name", GW::Agents::GetAgentEncName(agent));
            InfoField("Plane", "%d", agent->plane);
            InfoField("Type", "0x%X", agent->type);
            InfoField("Width", "%f", agent->width1);
            InfoField("Height", "%f", agent->height1);
            InfoField("Rotation", "%f", agent->rotation_angle);
            InfoField("NameProperties", "0x%X", agent->name_properties);
            InfoField("Distance", "%.2f", me ? GetDistance(me->pos, agent->pos) : 0.f);
            InfoField("Visual effects", "0x%X", agent->visual_effects);
            if (item_actual) {
                InfoField("Owner", "%d", item->owner);
                InfoField("ItemId", "%d", item->item_id);
                InfoField("ExtraType", "%d", item->extra_type);
            }
            if (gadget) {
                InfoField("Gadget ID", "%d", gadget->gadget_id);
                InfoField("ExtraType", "%d", gadget->extra_type);
            }
            if (living) {
                InfoField("AS of Weapon", "%f", living->weapon_attack_speed);
                InfoField("AS modifier", "%f", living->attack_speed_modifier);
                InfoField("Primary Prof", "%d", living->primary);
                InfoField("Secondary Prof", "%d", living->secondary);
                InfoField("Level", "%d", living->level);
                InfoField("TeamId", "%d", living->team_id);
                InfoField("Effects", "0x%X", living->effects);
                InfoField("ModelState", "0x%X", living->model_state);
                InfoField("typeMap", "0x%X", living->type_map);
                InfoField("Allegiance", "0x%X", living->allegiance);
                InfoField("WeaponType", "%d", living->weapon_type);
                InfoField("Skill", "%d", living->skill);
                InfoField("Animation code", "0x%X", living->animation_code);
                InfoField("Animation id", "0x%X", living->animation_id);
                InfoField("Animation type", "0x%X", living->animation_type);
                InfoField("Animation code", "%.3f", living->animation_speed);
            }
            if (npc) {
                ImGui::PushID("npc_info");
                InfoField("Addr", "%p", npc);
                InfoField("NPC ID", "%d", npc_id);
                InfoField("NPC Model File ID", "0x%X", npc->model_file_id);
                InfoField("NPC Skin File ID", "0x%X", npc->skin_file_id);
                InfoField("NPC Adjustment", "0x%X", npc->visual_adjustment);
                InfoField("NPC Appearance", "0x%X", npc->appearance);
                if (npc->files_count) {
                    InfoField("NPC ModelFile", "0x%X", npc->model_files[0]);
                }
                InfoField("NPC Flags", "0x%X", npc->npc_flags);
                EncInfoField("NPC Name", npc->name_enc);
                ImGui::PopID();
            }
            const auto map_agent = GW::Agents::GetMapAgentByID(agent->agent_id);
            if (map_agent) {
                InfoField("Map agent effects", "0x%X", map_agent->effects);
            }
            ImGui::TreePop();
        }
    }

    void DrawResignlog()
    {
        if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading) {
            return;
        }
        const auto players = GW::PartyMgr::GetPartyPlayers();
        if (!players) {
            return;
        }
        std::wstring buf;
        for (auto& partymember : *players) {
            if (!ResignLogModule::PrintResignStatus(partymember.login_number, buf, true))
                continue;
            ImGui::PushID(static_cast<int>(partymember.login_number));
            if (ImGui::Button("Send")) {
                GW::Chat::SendChat('#', buf.c_str());
            }
            ImGui::SameLine();
            ImGui::TextUnformatted(TextUtils::WStringToString(buf).c_str());
            ImGui::PopID();
        }
    }

    bool GetQuestEntryGroupName(const GW::Constants::QuestID quest_id, wchar_t* out, const size_t out_len)
    {
        const auto quest = GW::QuestMgr::GetQuest(quest_id);
        switch (quest->log_state & 0xf0) {
            case 0x20:
                return swprintf(out, out_len, L"\x564") != -1;
            case 0x40:
                return quest->location && swprintf(out, out_len, L"\x8102\x1978\x10A%s\x1", quest->location) != -1;
            case 0:
                return quest->location && swprintf(out, out_len, L"\x565\x10A%s\x1", quest->location) != -1;
            case 0x10:
                // Unknown, maybe current mission quest, but this type of quest isn't in the quest log.
                break;
        }
        return false;
    }


    typedef uint32_t*(__cdecl* CreateTexture_pt)(wchar_t* file_name, uint32_t flags);
    CreateTexture_pt CreateTexture_Func = 0, CreateTexture_Ret = 0;

    typedef void(__cdecl* DoAsyncDecodeStr_pt)(const wchar_t* s, void* cb, void* wParam);
    DoAsyncDecodeStr_pt ValidateAsyncDecodeStr_Func = 0, ValidateAsyncDecodeStr_Ret = 0;

    // Why reinvent the wheel?
    typedef bool(__cdecl* GWCA_SendUIMessage_pt)(GW::UI::UIMessage msgid, void* wParam, void* lParam, bool skip_hooks);
    GWCA_SendUIMessage_pt GWCA_SendUIMessage_Func = 0, GWCA_SendUIMessage_Ret = 0;

    struct UIMessagePacket {
        GW::UI::UIMessage msgid;
        void* wParam;
        void* lParam;
        bool skip_hooks;
    };

    CircularBuffer<UIMessagePacket> ui_message_packets_recorded;

    struct EventPacket {
        GW::EventMgr::EventID event_id;
        void* packet;
        uint32_t packet_size;
    };
    CircularBuffer<EventPacket> event_message_packets_recorded;


    struct RecordedAsyncDecode {
        std::wstring s; 
        void* cb;
        void* wParam;
        std::wstring decoded;
        std::string decoded_str;
    };
    std::unordered_map<std::wstring,RecordedAsyncDecode*> enc_strings_recorded;

    void OnRecordedAsyncDecode_Decoded(void* param, const wchar_t* decoded) {
        auto e = (RecordedAsyncDecode*)param;
        e->decoded = decoded;
        e->decoded_str = TextUtils::WStringToString(e->decoded);
    }

    void __cdecl OnValidateAsyncDecodeStr(const wchar_t* s, void* cb, void* wParam) {
        GW::Hook::EnterHook();
        if (s && wcsncmp(s, L"\x8103\xBB3", 2) != 0 && wcsncmp(s, L"\x55b\x101", 2) != 0 
            && enc_strings_recorded.find(s) == enc_strings_recorded.end()) {
            auto e = new RecordedAsyncDecode();
            e->s = s;
            e->cb = cb;
            e->wParam = wParam;
            enc_strings_recorded[s] = e;
            ValidateAsyncDecodeStr_Ret(e->s.c_str(), OnRecordedAsyncDecode_Decoded, e);
                
        }


        ValidateAsyncDecodeStr_Ret(s, cb, wParam);

        GW::Hook::LeaveHook();
    }

    std::unordered_map<uint32_t, IDirect3DTexture9**> textures_created_by_file_id;
    std::unordered_map<IDirect3DTexture9**, uint32_t> texture_file_ids;
    std::vector<IDirect3DTexture9**> textures_created;

    std::map<uint32_t, IDirect3DTexture9*> dx9_textures_created_by_hash;
    bool record_dx9_textures = false;

    bool OnGWCASendUIMessage(GW::UI::UIMessage msgid, void* wParam, void* lParam, bool skip_hooks) {
        GW::Hook::EnterHook();
        auto res = GWCA_SendUIMessage_Ret(msgid, wParam, lParam, skip_hooks);
        if (record_ui_messages) ui_message_packets_recorded.add({msgid, wParam, lParam, skip_hooks});
        GW::Hook::LeaveHook();
        return res;
    }

    void OnEventMessage(GW::HookStatus*, GW::EventMgr::EventID event_id, void* packet, uint32_t packet_size) {
        if (record_event_messages) 
            event_message_packets_recorded.add({event_id, packet, packet_size});
    }

    typedef HRESULT(WINAPI* CreateDx9Texture_pt)(IDirect3DDevice9*, UINT, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL, IDirect3DTexture9**, HANDLE*);
    CreateDx9Texture_pt CreateDx9Texture_Func = 0, CreateDx9Texture_Ret = 0;

    using TextureRelease_pt = ULONG(WINAPI*)(IDirect3DTexture9*);
    TextureRelease_pt TextureRelease_Func = nullptr;
    TextureRelease_pt TextureRelease_Ret = nullptr;

    IDirect3DTexture9* LastCreatedTexture = nullptr;

        // Hook Release to clean up tracking
    ULONG WINAPI OnD3D9TextureRelease(IDirect3DTexture9* texture)
    {
        GW::Hook::EnterHook();
        ULONG ref = TextureRelease_Ret(texture);
        if (ref == 0) {
            if (texture == LastCreatedTexture) LastCreatedTexture = 0;
            // Remove from hash map if present
            for (auto it = dx9_textures_created_by_hash.begin(); it != dx9_textures_created_by_hash.end();) {
                if (it->second == texture) {
                    it = dx9_textures_created_by_hash.erase(it);
                }
                else {
                    ++it;
                }
            }
        }
        GW::Hook::LeaveHook();

        return ref;
    }

    HRESULT WINAPI OnD3D9CreateTexture(IDirect3DDevice9* device, UINT Width, UINT Height, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DTexture9** ppTexture, HANDLE* pSharedHandle)
    {
        GW::Hook::EnterHook();
        HRESULT result = CreateDx9Texture_Ret(device, Width, Height, Levels, Usage, Format, Pool, ppTexture, pSharedHandle);

        if (LastCreatedTexture != nullptr) {
            const auto hash = Resources::GetTexmodHash(LastCreatedTexture);
            if (!dx9_textures_created_by_hash.contains(hash)) {
                dx9_textures_created_by_hash[hash] = LastCreatedTexture;
            }
            LastCreatedTexture = 0;
        }

        if (SUCCEEDED(result) && *ppTexture) {
            // Hook Release on first texture creation
            if (!TextureRelease_Func) {
                uintptr_t* texture_vtable = *reinterpret_cast<uintptr_t**>(*ppTexture);
                constexpr int RELEASE_INDEX = 2;
                TextureRelease_Func = (TextureRelease_pt)texture_vtable[RELEASE_INDEX];
                GW::Hook::CreateHook((void**)&TextureRelease_Func, OnD3D9TextureRelease, (void**)&TextureRelease_Ret);
                GW::Hook::EnableHooks(TextureRelease_Func);
            }
            LastCreatedTexture = *ppTexture;
        }

        GW::Hook::LeaveHook();
        return result;
    }

    uint32_t FileHashToFileId(wchar_t* param_1) {
        if (!param_1)
            return 0;
        if (((0xff < *param_1) && (0xff < param_1[1])) &&
            ((param_1[2] == 0 || ((0xff < param_1[2] && (param_1[3] == 0)))))) {
            return (*param_1 - 0xff00ff) + (uint32_t)param_1[1] * 0xff00;
        }
        return 0;
    }

    uint32_t* OnCreateTexture(wchar_t* file_name, uint32_t flags) {
        GW::Hook::EnterHook();
        const auto out = CreateTexture_Ret(file_name, flags);
        uint32_t file_id = FileHashToFileId(file_name);
        if (textures_created_by_file_id.find(file_id) == textures_created_by_file_id.end()) {
            const auto f = GwDatTextureModule::LoadTextureFromFileId(file_id);
            textures_created.push_back(f);
            textures_created_by_file_id[file_id] = f;
            texture_file_ids[f] = file_id;
        }
        GW::Hook::LeaveHook();
        return out;
    }

    void HookOnValidateAsyncDecodeStr(bool hook) {
        if (hook && ValidateAsyncDecodeStr_Func)
            return;
        if (hook) {
            ValidateAsyncDecodeStr_Func = (DoAsyncDecodeStr_pt)GW::Scanner::ToFunctionStart(GW::Scanner::FindUseOfString("(codedString[0] & ~WORD_BIT_MORE) >= WORD_VALUE_BASE"));
            if (ValidateAsyncDecodeStr_Func) {
                GW::Hook::CreateHook((void**)&ValidateAsyncDecodeStr_Func, OnValidateAsyncDecodeStr, (void**)&ValidateAsyncDecodeStr_Ret);
                GW::Hook::EnableHooks(ValidateAsyncDecodeStr_Func);
            }

        }
        else if (ValidateAsyncDecodeStr_Func) {
            GW::Hook::RemoveHook(ValidateAsyncDecodeStr_Func);
            while (enc_strings_recorded.begin() != enc_strings_recorded.end()) {
                delete enc_strings_recorded.begin()->second;
                enc_strings_recorded.erase(enc_strings_recorded.begin());
            }
            ValidateAsyncDecodeStr_Func = 0;
        }
    }
    void HookOnCreateTexture(bool hook) {
        if (hook && CreateTexture_Func)
            return;
        if (hook) {
            CreateTexture_Func = (CreateTexture_pt)GW::Scanner::ToFunctionStart(GW::Scanner::FindAssertion("GrTex2d.cpp", "!(flags & GR_TEXTURE_TRANSFER_OWNERSHIP)", 0,0));
            if (CreateTexture_Func) {
                GW::Hook::CreateHook((void**)&CreateTexture_Func, OnCreateTexture, (void**)&CreateTexture_Ret);
                GW::Hook::EnableHooks(CreateTexture_Func);
            }

        }
        else if(CreateTexture_Func) {
            GW::Hook::RemoveHook(CreateTexture_Func);
            CreateTexture_Func = 0;
            textures_created_by_file_id.clear();
            textures_created.clear();
            texture_file_ids.clear();
        }
    }
    void HookOnGWCASendUIMessage(bool hook) {
        if (hook && GWCA_SendUIMessage_Func)
            return;
        if (hook) {
            GWCA_SendUIMessage_Func = (GWCA_SendUIMessage_pt)GW::UI::SendUIMessage;
            if (GWCA_SendUIMessage_Func) {
                GW::Hook::CreateHook((void**)&GWCA_SendUIMessage_Func, OnGWCASendUIMessage, (void**)&GWCA_SendUIMessage_Ret);
                GW::Hook::EnableHooks(GWCA_SendUIMessage_Func);
            }
        }
        else if (GWCA_SendUIMessage_Func) {
            GW::Hook::RemoveHook(GWCA_SendUIMessage_Func);
            ui_message_packets_recorded.clear();
            GWCA_SendUIMessage_Func = 0;
        }
    }
    GW::HookEntry* event_message_hook_entry = nullptr;
    void HookOnGWCASendEventMessage(bool hook)
    {
        if (hook && event_message_hook_entry) return;
        if (hook) {
            event_message_hook_entry = new GW::HookEntry();
            for (size_t i = 0; i < 0xff; i++) {
                if (i == 2 || i == 1) continue;
                GW::EventMgr::RegisterEventCallback(event_message_hook_entry, (GW::EventMgr::EventID)i, OnEventMessage);
            }
        }
        else if (event_message_hook_entry) {
            GW::EventMgr::RemoveEventCallback(event_message_hook_entry);
            delete event_message_hook_entry;
            event_message_hook_entry = 0;
            event_message_packets_recorded.clear();
        }
    }

    void HighlightFrame(GW::UI::Frame* frame) {
        if (!frame) return;
        const auto root = GW::UI::GetRootFrame();
        const auto top_left = frame->position.GetTopLeftOnScreen(root);
        const auto bottom_right = frame->position.GetBottomRightOnScreen(root);
        const auto draw_list = ImGui::GetBackgroundDrawList();
        draw_list->AddRect({ top_left.x, top_left.y }, { bottom_right.x, bottom_right.y }, IM_COL32_WHITE);
    }

    typedef void(__cdecl* SetFpsLimits_pt)(uint32_t target_fps);
    SetFpsLimits_pt SetFpsLimits_Func = 0, SetFpsLimits_Ret = 0;

    uint32_t target_fps = 0;

    void OnSetFpsLimits(uint32_t _target_fps)
    {
        GW::Hook::EnterHook();
        target_fps = _target_fps;
        SetFpsLimits_Ret(_target_fps);
        GW::Hook::LeaveHook();
    }

    void HookOnCreateDx9Texture(bool enable = true)
    {
        if (enable) {
            if (!CreateDx9Texture_Func) {
                IDirect3DDevice9* device = GW::Render::GetDevice();
                if (!device) return;
                // Get vtable pointer
                uintptr_t* vtable = *reinterpret_cast<uintptr_t**>(device);
                // CreateTexture is at index 23 in IDirect3DDevice9 vtable
                constexpr int CREATE_TEXTURE_INDEX = 23;
                CreateDx9Texture_Func = (CreateDx9Texture_pt)vtable[CREATE_TEXTURE_INDEX];
                GW::Hook::CreateHook((void**)&CreateDx9Texture_Func, OnD3D9CreateTexture, (void**)&CreateDx9Texture_Ret);
                GW::Hook::EnableHooks(CreateDx9Texture_Func);
            }

            // Hook texture Release to track when textures are destroyed
            if (!TextureRelease_Func) {
                // We need any texture to get its vtable
                // Create a temporary dummy texture to get the vtable
                IDirect3DTexture9* temp_texture = nullptr;
                IDirect3DDevice9* device = GW::Render::GetDevice();
                if (device && SUCCEEDED(device->CreateTexture(1, 1, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &temp_texture, nullptr))) {
                    uintptr_t* texture_vtable = *reinterpret_cast<uintptr_t**>(temp_texture);
                    // Release is at index 2 in IUnknown (inherited by IDirect3DTexture9)
                    constexpr int RELEASE_INDEX = 2;
                    TextureRelease_Func = (TextureRelease_pt)texture_vtable[RELEASE_INDEX];
                    GW::Hook::CreateHook((void**)&TextureRelease_Func, OnD3D9TextureRelease, (void**)&TextureRelease_Ret);
                    GW::Hook::EnableHooks(TextureRelease_Func);
                    temp_texture->Release(); // Clean up temp texture
                }
            }
            return;
        }
        else {
            if (CreateDx9Texture_Func) {
                GW::Hook::DisableHooks(CreateDx9Texture_Func);
                GW::Hook::RemoveHook(CreateDx9Texture_Func);
                CreateDx9Texture_Func = nullptr;
            }
            if (TextureRelease_Func) {
                GW::Hook::DisableHooks(TextureRelease_Func);
                GW::Hook::RemoveHook(TextureRelease_Func);
                TextureRelease_Func = nullptr;
            }
            dx9_textures_created_by_hash.clear();
            LastCreatedTexture = nullptr;
        }
    }

    bool DownloadStringFiles()
    {
        wchar_t** file_ids = 0;
        ArenaNetFileParser::GameAssetFile asset;
        auto addr = GW::Scanner::FindUseOfString("index < arrsize(s_fileId)", 0x11);
        if (!(addr && GW::Scanner::IsValidPtr(*(uintptr_t*)addr, GW::ScannerSection::Section_DATA))) {
            return false;
        }
        file_ids = *(wchar_t***)addr;
        // wchar_t *[18][99] s_fileId
        Resources::EnsureFolderExists(Resources::GetPath("language_files"));
        for (size_t language_id = 0; language_id < 18; language_id++) {
            wchar_t** language_files = &file_ids[language_id * 99];
            for (size_t file_idx = 0; file_idx < 99; file_idx++) {
                const auto file_name = language_files[file_idx];
                if (!(file_name && *file_name)) 
                    continue;
                if (!asset.readFromDat(file_name)) 
                    return false;
                const auto filename = std::format("language_file_{}_{}.txt", language_id, file_idx);
                const auto write_to = Resources::GetPath("language_files", filename);

                if (std::filesystem::exists(write_to)) {
                    std::filesystem::remove(write_to);
                }
                auto handle = fopen(write_to.string().c_str(), "wb");
                if (!handle) return false;
                fwrite(asset.data.data(), asset.data.size(), 1, handle);
                fclose(handle);
            }
        }
        return true;
    }

    void PostDraw() {
        HookOnCreateDx9Texture(record_dx9_textures);
        HookOnCreateTexture(record_textures);
        HookOnValidateAsyncDecodeStr(record_enc_strings);
        HookOnGWCASendUIMessage(record_ui_messages);
        HookOnGWCASendEventMessage(record_event_messages);
    }
    const uint32_t GetMapPropModelFileId(GW::MapProp* prop)
    {
        if (!(prop && prop->h0034[4]))
            return 0;
        uint32_t* sub_deets = (uint32_t*)prop->h0034[4];
        return ArenaNetFileParser::FileHashToFileId((wchar_t*)sub_deets[1]);
    };
    void DrawDebugInfo() {
        if (!SetFpsLimits_Func) {
            SetFpsLimits_Func = (SetFpsLimits_pt)GW::Scanner::ToFunctionStart(GW::Scanner::Find("\x68\x40\x42\x0f\x00\xe8", "xxxxxx"));
            if (SetFpsLimits_Func) {
                GW::Hook::CreateHook((void**)&SetFpsLimits_Func, OnSetFpsLimits, (void**)&SetFpsLimits_Ret);
                GW::Hook::EnableHooks(SetFpsLimits_Func);
            }
        }
        ImGui::Text("Fps limit: %d", target_fps);
        if (ImGui::CollapsingHeader("Account Features")) {
            const auto& features = GW::GetGameContext()->account->account_unlocked_counts;
            ImGui::PushItemWidth(140.f);
            ImGui::TextUnformatted("ID");
            ImGui::SameLine();
            ImGui::TextUnformatted("Value 1");
            ImGui::SameLine();
            ImGui::TextUnformatted("Value 2");
            for (const auto& feature : features) {
                ImGui::PushID(&feature);
                ImGui::Text("0x%x",feature.id);
                ImGui::SameLine();
                ImGui::Text("%d",feature.unk1);
                ImGui::SameLine();
                ImGui::Text("%d",feature.unk2);
                ImGui::PopID();
            }
        }
        if (ImGui::CollapsingHeader("Quoted Item")) {
            ImGui::Text("Most recently quoted item (buy or sell) from trader");
            static GuiUtils::EncString quoted_name;
            DrawItemInfo(GW::Items::GetItemById(quoted_item_id), &quoted_name);
        }

        
        if (ImGui::CollapsingHeader("UI Message Log")) {
            record_ui_messages = true;
            ImGui::PushID(&ui_message_packets_recorded);
            if (ImGui::SmallButton("Reset")) {
                ui_message_packets_recorded.clear();
            }
            for (const auto packet : ui_message_packets_recorded) {
                ImGui::PushID(&packet);
                ImGui::Text("0x%08x 0x%08x 0x%08x", packet.msgid, packet.wParam, packet.lParam);
                ImGui::PopID();
            }
            ImGui::PopID();
            if (ImGui::IsKeyDown(ImGuiMod_Alt)) {
                ImGui::SetScrollHereY();
            }
        }
        if (ImGui::CollapsingHeader("Event Message Log")) {
            record_event_messages = true;
            ImGui::PushID(&event_message_packets_recorded);
            if (ImGui::SmallButton("Reset")) {
                event_message_packets_recorded.clear();
            }
            for (const auto packet : event_message_packets_recorded) {
                ImGui::PushID(&packet);
                ImGui::Text("0x%08x 0x%08x", packet.event_id, packet.packet);
                ImGui::PopID();
            }
            ImGui::PopID();
            if (ImGui::IsKeyDown(ImGuiMod_Alt)) {
                ImGui::SetScrollHereY();
            }
        }

        

        if (ImGui::CollapsingHeader("Async Str Log")) {
            record_enc_strings = true;
            ImGui::PushID(&enc_strings_recorded);
            if (ImGui::SmallButton("Reset")) {
                while (enc_strings_recorded.begin() != enc_strings_recorded.end()) {
                    delete enc_strings_recorded.begin()->second;
                    enc_strings_recorded.erase(enc_strings_recorded.begin());
                }
            }
            for (const auto packet : enc_strings_recorded) {
                ImGui::PushID(packet.second);
                EncInfoField("Encoded", packet.second->s.c_str());
                InfoField("Decoded", "%s", packet.second->decoded_str.c_str());
                ImGui::PopID();
            }
            ImGui::PopID();
        }
        const auto target = GW::Agents::GetTarget();
        if (target && ImGui::CollapsingHeader("Props within range of target")) {
            float range = GW::Constants::Range::Area;
            const auto props = target ? GW::Map::GetMapProps() : nullptr;
            if (props) {
                ImGui::Indent();
                ImGui::TextUnformatted("Model File ID");
                ImGui::SameLine(128.f);
                ImGui::TextUnformatted("Distance");
                ImGui::SameLine(256.f);
                ImGui::TextUnformatted("Location");
                ImGui::Separator();
                for (const auto prop : *props) {
                    float distance = GW::GetDistance(target->pos, GW::GamePos({ prop->position.x,prop->position.y,0 }));
                    if (distance > range)
                        continue;
                    ImGui::PushID(prop);
                    ImGui::Text("%08X", GetMapPropModelFileId(prop));
                    ImGui::SameLine(128.f);
                    ImGui::Text("%.2f", distance);
                    ImGui::SameLine(256.f);
                    const auto label = std::format("{}, {}", prop->position.x, prop->position.y);
                    if (ImGui::Button(label.c_str())) {
                        GW::Map::PingCompass(GW::GamePos({ prop->position.x, prop->position.y ,0 }));
                    }
                    ImGui::PopID();
                }
                ImGui::Unindent();
            }
        }
        if (ImGui::CollapsingHeader("Loaded Textures by GW File")) {
            record_textures = true;
            ImGui::PushID(&textures_created);
            constexpr ImVec2 scaled_size = { 64.f, 64.f };
            constexpr ImVec4 tint(1, 1, 1, 1);
            const auto normal_bg = ImColor(IM_COL32(0, 0, 0, 0));
            constexpr auto uv0 = ImVec2(0, 0);

            if (ImGui::SmallButton("Reset")) {
                textures_created_by_file_id.clear();
                textures_created.clear();
                texture_file_ids.clear();
            }

            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.f, 0.5f));

            ImGui::StartSpacedElements(scaled_size.x);

            for (const auto texture : textures_created) {
                ImGui::PushID(texture);
                if (!texture || !*texture) {
                    ImGui::PopID();
                    continue;
                }

                const auto uv1 = ImGui::CalculateUvCrop(*texture, scaled_size);
                ImGui::NextSpacedElement();
                const auto clicked = ImGui::ImageButton(*texture, scaled_size, uv0, uv1, -1, normal_bg, tint);
                static wchar_t out[3];
                if (ImGui::IsItemHovered()) {
                    ArenaNetFileParser::FileIdToFileHash(texture_file_ids[texture], out);
                    ImGui::SetTooltip("File ID: 0x%08x\nFile Hash: 0x%04x 0x%04x", texture_file_ids[texture], out[0], out[1]);
                }
                if (clicked) {
                    ImGui::SetContextMenu([texture](void*) {
                        if (ImGui::Button("Download as DDS (naming by gw file id)")) {
                            const auto filename = std::format("{:#010x}.dds", texture_file_ids[texture]);
                            const auto write_to = Resources::GetPath("extracted_textures", filename);
                            Resources::EnsureFolderExists(Resources::GetPath("extracted_textures"));
                            Resources::SaveTextureToFile(*texture, write_to);
                            return false;
                        }
                        if (ImGui::Button("Download as DDS (naming by hash for gMod)")) {
                            const auto hash = Resources::GetTexmodHash(*texture);
                            const auto filename = std::format("GW.EXE_0x{:08X}.dds", hash);
                            const auto write_to = Resources::GetPath("extracted_textures", filename);
                            Resources::EnsureFolderExists(Resources::GetPath("extracted_textures"));
                            Resources::SaveTextureToFile(*texture, write_to);
                            return false;
                        }
                        return true;
                        });
                }
                ImGui::PopID();
            }

            ImGui::PopStyleColor();
            ImGui::PopStyleVar();
            ImGui::PopStyleVar();
            ImGui::PopID();
        }
        static bool game_master_mode = false;
        if (ImGui::Checkbox("Game Master Mode", &game_master_mode)) {
            if (game_master_mode) {
                GW::GetCharContext()->player_flags |= 0x8;
            }
            else {
                GW::GetCharContext()->player_flags ^= 0x8;
            }
        }
        if (ImGui::Button("Open Text Dev Window")) {
            GW::GameThread::Enqueue([] {
                GW::GetCharContext()->player_flags |= 0x8;
                GW::UI::Keypress((GW::UI::ControlAction)0x25);
                GW::GetCharContext()->player_flags ^= 0x8;
                });
        }
        if (ImGui::Button("Open GM Start Menu?")) {
            GW::GameThread::Enqueue([] {
                GW::GetCharContext()->player_flags |= 0x8;
                GW::UI::SendUIMessage((GW::UI::UIMessage)0x1000008a, 0, 0);
                //GW::GetCharContext()->player_flags ^= 0x8;
                });
        }
        // DownloadStringFiles
        if (ImGui::Button("DownloadStringFiles")) {
            Resources::EnqueueWorkerTask([]() {
                Log::Info("Downloading strings...");
                DownloadStringFiles() || (Log::Error("Failed to DownloadStringFiles"), true);
                Log::Info("Done");
            });
        }


        // For debugging changes to flags/arrays etc
        [[maybe_unused]] const GW::GameContext* g = GW::GetGameContext();
        [[maybe_unused]] const GW::GuildContext* gu = g->guild;
        [[maybe_unused]] const GW::CharContext* c = g->character;
        [[maybe_unused]] const GW::WorldContext* w = g->world;
        [[maybe_unused]] const GW::PartyContext* p = g->party;
        [[maybe_unused]] const GW::MapContext* m = g->map;
        [[maybe_unused]] const GW::AccountContext* acc = g->account;
        [[maybe_unused]] const GW::ItemContext* i = g->items;
        [[maybe_unused]] const GW::AgentLiving* me = GW::Agents::GetControlledCharacter();
        [[maybe_unused]] const GW::Player* me_player = me ? GW::PlayerMgr::GetPlayerByID(me->player_number) : nullptr;
        [[maybe_unused]] const GW::Chat::ChatBuffer* log = GW::Chat::GetChatLog();
        [[maybe_unused]] const GW::AreaInfo* ai = GW::Map::GetMapInfo(GW::Map::GetMapID());
        [[maybe_unused]] const auto ac = me_player ? GW::AccountMgr::GetAvailableCharacter(me_player->name) : nullptr;
        [[maybe_unused]] const auto gpc = GW::GetGameplayContext();
        

        [[maybe_unused]] const auto mission_map_context = GW::Map::GetMissionMapContext();
        [[maybe_unused]] const auto mission_map_frame = mission_map_context ? GW::UI::GetFrameById(mission_map_context->frame_id) : nullptr;
        [[maybe_unused]] const auto world_map_context = GW::Map::GetWorldMapContext();

        [[maybe_unused]]  const auto campaign = ac ? ac->campaign() : 0;
        [[maybe_unused]]  const auto level = ac ? ac->level() : 0;
        [[maybe_unused]]  const auto primary = ac ? ac->primary() : 0;
        [[maybe_unused]] const auto secondary = ac ? ac->secondary() : 0;
        [[maybe_unused]] const auto salvage_session = GW::Items::GetSalvageSessionInfo();
#ifdef _DEBUG
        auto frame = GW::UI::GetChildFrame(GW::UI::GetFrameByLabel(L"Vendor"), 0,0,2);
        HighlightFrame(frame); 

#endif
    }

    void OnPostUIMessage(GW::HookStatus*, GW::UI::UIMessage message_id, void* wParam, void*) {
        switch (message_id) {
            case GW::UI::UIMessage::kLoadMapContext: {
                const auto packet = (GW::UI::UIPacket::kLoadMapContext*)wParam;
                wcscpy(mapfile, packet->file_name);
            } break;
        }
    }

}

void InfoWindow::Terminate()
{
    for (const auto& achievement : target_achievements | std::views::values) {
        delete achievement;
    }
    target_achievements.clear();

    HookOnValidateAsyncDecodeStr(false);
    HookOnCreateTexture(false);
    HookOnGWCASendUIMessage(false);
}
void InfoWindow::SignalTerminate() {
    visible = false;
}

void InfoWindow::Initialize()
{
    ToolboxWindow::Initialize();

    ui_message_packets_recorded = CircularBuffer<UIMessagePacket>(512);
    event_message_packets_recorded = CircularBuffer<EventPacket>(512);

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::QuotedItemPrice>(&InstanceLoadFile_Entry,
                                                                        [this](GW::HookStatus*, const GW::Packet::StoC::QuotedItemPrice* packet) -> void {
                                                                            quoted_item_id = packet->itemid;
                                                                        });
    GW::UI::RegisterUIMessageCallback(&InstanceLoadFile_Entry, GW::UI::UIMessage::kLoadMapContext, OnPostUIMessage, 0x8000);
}

void InfoWindow::Draw(IDirect3DDevice9*)
{
    record_dx9_textures = false;
    record_textures = false;
    record_ui_messages = false;
    record_enc_strings = false;
    if (!visible) {
        PostDraw();
        return;
    }
    ImGui::SetNextWindowCenter(ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 0), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {

        if (show_widgets) {
            const auto& widgets = GWToolbox::GetWidgets();

            const unsigned cols = static_cast<unsigned>(ceil(ImGui::GetWindowSize().x / 200.f));
            ImGui::PushID("info_enable_widget_items");
            ImGui::Columns(static_cast<int>(cols), "info_enable_widgets", false);
            const size_t items_per_col = static_cast<size_t>(ceil(static_cast<float>(widgets.size()) / cols));
            size_t col_count = 0u;
            for (const auto widget : widgets) {
                ImGui::Checkbox(widget->Name(), &widget->visible);
                if (++col_count == items_per_col) {
                    ImGui::NextColumn();
                    col_count = 0u;
                }
            }
            ImGui::Columns(1);
            ImGui::PopID();
        }

        if (show_open_chest) {
            if (ImGui::Button("Open Xunlai Chest", ImVec2(-1.0f, 0))) {
                GW::GameThread::Enqueue([] {
                    GW::Items::OpenXunlaiWindow();
                });
            }
        }

        if (ImGui::CollapsingHeader("Camera")) {
            const GW::Camera* cam = GW::CameraMgr::GetCamera();
            if (cam != nullptr) {
                InfoField("Position##cam_pos", "%.2f, %.2f, %.2f", cam->position.x, cam->position.y, cam->position.z);
                InfoField("Target##cam_target", "%.2f, %.2f, %.2f", cam->look_at_target.x, cam->look_at_target.y, cam->look_at_target.z);
                InfoField("Yaw/Pitch##cam_angle", "%.2f, %.2f", cam->GetCurrentYaw(), cam->pitch);
            }
        }
        if (show_player && ImGui::CollapsingHeader("Player")) {
            ImGui::PushID("player_info");
            InfoField("Is Typing?", "%s", GW::Chat::GetIsTyping() ? "Yes" : "No");
            DrawAgentInfo(GW::Agents::GetObservingAgent());
            ImGui::PopID();
        }
        if (show_target && ImGui::CollapsingHeader("Target")) {
            ImGui::PushID("target_info");
            DrawAgentInfo(GW::Agents::GetTarget());
            ImGui::PopID();
        }
        if (show_map && ImGui::CollapsingHeader("Map")) {
            DrawMapInfo(GW::Map::GetMapID());
        }
        if (show_map && ImGui::CollapsingHeader("Lookup Map")) {
            static int map_id = 0;
            ImGui::InputInt("Map ID", &map_id, 1, 1);
            const auto current = GW::Map::GetMapInfo(static_cast<GW::Constants::MapID>(map_id));
            if(current)
                DrawMapInfo(static_cast<GW::Constants::MapID>(map_id));
        }
        if (show_dialog && ImGui::CollapsingHeader("Dialog")) {
            EncInfoField("Dialog Body", DialogModule::GetDialogBody());
            InfoField("Last Dialog", "0x%X", DialogModule::LastDialogId());
            ImGui::Text("Available NPC Dialogs:");
            ImGui::ShowHelp("Talk to an NPC to see available dialogs");
            const auto& messages = DialogModule::GetDialogButtonMessages();
            const auto& buttons = DialogModule::GetDialogButtons();
            char bbuf[48];
            for (size_t i = 0; i < buttons.size(); i++) {
                snprintf(bbuf, _countof(bbuf), "send_dialog_%d", i);
                ImGui::PushID(bbuf);
                if (ImGui::Button("Send")) {
                    uint32_t dialog_id = buttons[i]->dialog_id;
                    GW::GameThread::Enqueue([dialog_id] {
                        DialogModule::SendDialog(dialog_id);
                    });
                }
                ImGui::SameLine();
                InfoField("Icon", "0x%X", buttons[i]->button_icon);
                EncInfoField("Encoded", messages[i]->encoded().c_str());
                InfoField(messages[i]->string().c_str(), "0x%X", buttons[i]->dialog_id);
                ImGui::PopID();
            }
        }
        if (ImGui::CollapsingHeader("Hovered Skill")) {
            static GuiUtils::EncString skill_name;
            const auto current = GW::SkillbarMgr::GetHoveredSkill();
            if (current) {
                last_hovered_skill_id = current->skill_id;
            }
            DrawSkillInfo(GW::SkillbarMgr::GetSkillConstantData(last_hovered_skill_id), &skill_name, true);
        }
        if (ImGui::CollapsingHeader("Lookup Skill")) {
            static GuiUtils::EncString skill_name;
            static int skill_id = 0;
            ImGui::InputInt("Skill ID", &skill_id, 1, 1);
            const auto current = GW::SkillbarMgr::GetSkillConstantData(static_cast<GW::Constants::SkillID>(skill_id));
            if(current)
                DrawSkillInfo(current, &skill_name, true);
        }
        if (show_item && ImGui::CollapsingHeader("Hovered Item")) {
            static GuiUtils::EncString item_name;
            ImGui::PushID("hovered_item");
            const GW::Item* current = GW::Items::GetHoveredItem();
            if (current) {
                last_hovered_item_id = current->item_id;
            }
            DrawItemInfo(GW::Items::GetItemById(last_hovered_item_id), &item_name, true);
            ImGui::PopID();
        }
        if (show_item && ImGui::CollapsingHeader("Item")) {
            ImGui::Text("First item in inventory");
            static GuiUtils::EncString item_name;
            DrawItemInfo(GW::Items::GetItemBySlot(GW::Items::GetBag(GW::Constants::Bag::Backpack), 1), &item_name);
        }
        if (ImGui::CollapsingHeader("Loaded DirectX9 Textures")) {
            record_dx9_textures = true;
            ImGui::PushID(&dx9_textures_created_by_hash);
            constexpr ImVec2 scaled_size = {64.f, 64.f};
            constexpr ImVec4 tint(1, 1, 1, 1);
            const auto normal_bg = ImColor(IM_COL32(0, 0, 0, 0));
            constexpr auto uv0 = ImVec2(0, 0);

            if (ImGui::SmallButton("Reset")) {
                dx9_textures_created_by_hash.clear();
            }

            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.f, 0.5f));

            ImGui::StartSpacedElements(scaled_size.x);

            for (const auto& it : dx9_textures_created_by_hash) {
                const auto texture = it.second;
                const auto hash = it.first;
                if (!texture) continue;
                ImGui::PushID(texture);

                const auto uv1 = ImGui::CalculateUvCrop(texture, scaled_size);
                ImGui::NextSpacedElement();
                const auto clicked = ImGui::ImageButton(texture, scaled_size, uv0, uv1, -1, normal_bg, tint);
                static wchar_t out[3];
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("gMod/uMod/Texmod Hash: 0x%08x (Click to download)", hash);
                }
                if (clicked) {
                    ImGui::SetContextMenu([hash](void*) {
                        ImGui::Text("gMod/uMod/Texmod Hash: 0x%08x", hash);
                        ImGui::Separator();
                        const char* ext = 0;
                        if (ImGui::Button("Download as DDS (for gMod/uMod/Texmod)")) {
                            ext = "dds";
                        }
                        if (ImGui::Button("Download as PNG")) {
                            ext = "png";
                        }
                        if (ext) {
                            const auto filename = std::format("GW.EXE_0x{:08X}.{}", hash, ext);
                            const auto write_to = Resources::GetPath("extracted_textures", filename);
                            Resources::EnsureFolderExists(Resources::GetPath("extracted_textures"));
                            const auto found = dx9_textures_created_by_hash.find(hash);
                            if (found != dx9_textures_created_by_hash.end()) {
                                Resources::SaveTextureToFile(found->second, write_to);
                            }
                            return false;
                        }
                        return true;
                    });
                }
                ImGui::PopID();
            }

            ImGui::PopStyleColor();
            ImGui::PopStyleVar();
            ImGui::PopStyleVar();
            ImGui::PopID();
        }
        if (show_quest && ImGui::CollapsingHeader("Quest")) {
            const GW::Quest* q = GW::QuestMgr::GetActiveQuest();
            if (q) {
                ImGui::Text("ID: 0x%X", q->quest_id);
                ImGui::Text("Marker: (%.0f, %.0f)", q->marker.x, q->marker.y);
                ImGui::Text("State: 0x%08x", q->log_state);
                EncInfoField("Location:", q->location);
                static wchar_t name_buf[128];
                GetQuestEntryGroupName(q->quest_id, name_buf, _countof(name_buf));
                EncInfoField("Quest Entry:", name_buf);
                EncInfoField("Objectives:", q->objectives);
            }
#ifdef _DEBUG
            std::string quests;
            const auto& quest_log = GW::QuestMgr::GetQuestLog();
            std::vector<GW::Quest*> quests_missing_info;
            if (quest_log) {
                for (auto& quest : *quest_log) {
                    if ((quest.log_state & 1) == 0) {
                        quests_missing_info.push_back(&quest);
                    }
                }
            }
            ImGui::Text("Quests missing info: %d", quests_missing_info.size());
            ImGui::SameLine();
            if (ImGui::SmallButton("Request quest info")) {
                for (const auto& quest : quests_missing_info) {
                    GW::QuestMgr::RequestQuestInfo(quest, true);
                }
            }
#endif
        }
        if (show_mobcount && ImGui::CollapsingHeader("Enemy count")) {
            constexpr float sqr_soul_range = 1400.0f * 1400.0f;
            int soul_count = 0;
            int cast_count = 0;
            int spirit_count = 0;
            int compass_count = 0;
            GW::AgentArray* agents = GW::Agents::GetAgentArray();
            const GW::Agent* player = GW::Agents::GetObservingAgent();
            if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Loading
                && agents
                && player != nullptr) {
                for (auto* a : *agents) {
                    const GW::AgentLiving* agent = a ? a->GetAsAgentLiving() : nullptr;
                    if (!(agent && agent->allegiance == GW::Constants::Allegiance::Enemy)) {
                        continue; // ignore non-hostiles
                    }
                    if (agent->GetIsDead()) {
                        continue; // ignore dead
                    }
                    const float sqrd = GetSquareDistance(player->pos, agent->pos);
                    if (agent->player_number == GW::Constants::ModelID::DoA::SoulTormentor
                        || agent->player_number == GW::Constants::ModelID::DoA::VeilSoulTormentor) {
                        if (GW::Map::GetMapID() == GW::Constants::MapID::Domain_of_Anguish
                            && sqrd < sqr_soul_range) {
                            ++soul_count;
                        }
                    }
                    if (sqrd < GW::Constants::SqrRange::Spellcast) {
                        ++cast_count;
                    }
                    if (sqrd < GW::Constants::SqrRange::Spirit) {
                        ++spirit_count;
                    }
                    ++compass_count;
                }
            }

            if (GW::Map::GetMapID() == GW::Constants::MapID::Domain_of_Anguish) {
                ImGui::Text("%d Soul Tormentors", soul_count);
                ImGui::ShowHelp("Only in Domain of Anguish, within 1400 range");
            }
            ImGui::Text("%d foes in casting range", cast_count);
            ImGui::Text("%d foes in spirit range", spirit_count);
            ImGui::Text("%d foes in compass range", compass_count);
        }
        if (show_resignlog 
            && ImGui::CollapsingHeader("Resign Log") 
            && GWToolbox::IsModuleEnabled("Resign Log")) {
            DrawResignlog();
        }
    }
#ifdef _DEBUG
    DrawDebugInfo();
#endif
    ImGui::End();
    PostDraw();
}

void InfoWindow::DrawSettingsInternal()
{
    ImGui::Separator();
    ImGui::StartSpacedElements(250.f);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Show widget toggles", &show_widgets);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Show 'Open Xunlai Chest' button", &show_open_chest);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Show Player", &show_player);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Show Target", &show_target);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Show Map", &show_map);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Show Dialog", &show_dialog);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Show Item", &show_item);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Show Quest", &show_quest);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Show Enemy Count", &show_mobcount);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Show Resign Log", &show_resignlog);
}

void InfoWindow::LoadSettings(ToolboxIni* ini)
{
    ToolboxWindow::LoadSettings(ini);
    LOAD_BOOL(show_widgets);
    LOAD_BOOL(show_open_chest);
    LOAD_BOOL(show_player);
    LOAD_BOOL(show_target);
    LOAD_BOOL(show_map);
    LOAD_BOOL(show_dialog);
    LOAD_BOOL(show_item);
    LOAD_BOOL(show_quest);
    LOAD_BOOL(show_mobcount);
    LOAD_BOOL(show_resignlog);
}

void InfoWindow::SaveSettings(ToolboxIni* ini)
{
    ToolboxWindow::SaveSettings(ini);
    SAVE_BOOL(show_widgets);
    SAVE_BOOL(show_open_chest);
    SAVE_BOOL(show_player);
    SAVE_BOOL(show_target);
    SAVE_BOOL(show_map);
    SAVE_BOOL(show_dialog);
    SAVE_BOOL(show_item);
    SAVE_BOOL(show_quest);
    SAVE_BOOL(show_mobcount);
    SAVE_BOOL(show_resignlog);
}

