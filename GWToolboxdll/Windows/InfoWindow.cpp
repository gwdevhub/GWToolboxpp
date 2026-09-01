#include "stdafx.h"

#include <GWCA/Utilities/Hooker.h>
#include <GWCA/Utilities/Scanner.h>

#include <GWCA/Context/CharContext.h>
#include <GWCA/Context/GameplayContext.h>
#include <GWCA/Context/MapContext.h>

#include <GWCA/Constants/Constants.h>

#include <GWCA/GameContainers/Array.h>
#include <GWCA/GameContainers/GamePos.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Camera.h>
#include <GWCA/GameEntities/Frame.h>
#include <GWCA/GameEntities/Guild.h>
#include <GWCA/GameEntities/Item.h>
#include <GWCA/GameEntities/Map.h>
#include <GWCA/GameEntities/NPC.h>
#include <GWCA/GameEntities/Party.h>
#include <GWCA/GameEntities/Pathing.h>
#include <GWCA/GameEntities/Player.h>
#include <GWCA/GameEntities/Quest.h>
#include <GWCA/GameEntities/Skill.h>
#include <GWCA/GameEntities/Title.h>


#include <GWCA/Context/AccountContext.h>
#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/WorldContext.h>

#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/CameraMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/EffectMgr.h>
#include <GWCA/Managers/EventMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/GuildMgr.h>
#include <GWCA/Managers/ItemMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/PlayerMgr.h>
#include <GWCA/Managers/QuestMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Managers/StoCMgr.h>

#include <Defines.h>

#include <Widgets/AlcoholWidget.h>
#include <Widgets/Minimap/Minimap.h>
#include <Widgets/PartyDamage.h>
#include <Windows/InfoWindow.h>
#include <Windows/NotepadWindow.h>

#include <Modules/DialogModule.h>
#include <Modules/GwDatModule.h>
#include <Modules/HallOfMonumentsModule.h>
#include <Modules/ItemDescriptionHandler.h>
#include <Modules/ResignLogModule.h>
#include <Modules/Resources.h>
#include <Modules/ToolboxSettings.h>
#include <Utils/ArenaNetFileParser.h>
#include <Utils/TextUtils.h>
#include <Utils/ToolboxUtils.h>

#include <GWToolbox.h>
#include <Logger.h>

#include <CircurlarBuffer.h>
#include <Widgets/WorldMapWidget.h>


namespace {

    wchar_t mapfile[8] = {0};
    std::map<std::wstring, HallOfMonumentsAchievements*> target_achievements;
    clock_t send_timer = 0;
    uint32_t last_hovered_item_id = 0;
    uint32_t quoted_item_id = 0;
    GW::Constants::SkillID last_hovered_skill_id = static_cast<GW::Constants::SkillID>(0);

    InfoWindow::Settings settings;

    GW::HookEntry MessageCore_Entry;
    GW::HookEntry InstanceLoadFile_Entry;
    GW::HookEntry OnDialogBody_Entry;
    GW::HookEntry OnDialogButton_Entry;
    GW::HookEntry OnSendDialog_Entry;

    int pending_map_names = 1;
    std::unordered_map<uint32_t, std::unique_ptr<GuiUtils::EncString>> encoded_name_id_to_string;

    bool record_textures = false;
    bool record_ui_messages = false;
    bool record_event_messages = false;
    bool record_enc_strings = false;





    bool EncInfoField(const char* label, const wchar_t* enc_string)
    {
        static thread_local std::string info_string;
        const size_t size_reqd = enc_string ? wcslen(enc_string) * 7 + 1 : 0;
        info_string.resize(size_reqd, 0); // 7 chars = 0xFFFF plus a space
        size_t offset = 0;
        for (size_t i = 0; enc_string && enc_string[i] && offset < size_reqd - 1; i++) {
            offset += sprintf(&info_string[offset], "0x%X ", enc_string[i]);
        }
        return ImGui::InputTextEx(label, nullptr, info_string.data(), info_string.size(), ImVec2(-160.f * ImGui::FontScale(), 0), ImGuiInputTextFlags_ReadOnly);
    }

    bool InfoField(const char* label, const char* fmt, ...)
    {
        char info_string[128];
        va_list vl;
        va_start(vl, fmt);
        vsnprintf(info_string, _countof(info_string), fmt, vl);
        info_string[127] = 0;
        va_end(vl);
        return ImGui::InputTextEx(label, nullptr, info_string, _countof(info_string), ImVec2(-160.f * ImGui::FontScale(), 0), ImGuiInputTextFlags_ReadOnly);
    }
    bool FileIdField(const char* label, const uint32_t file_id)
    {
        wchar_t enc_string[8] = {0};
        GW::UI::UInt32ToEncStr(file_id, enc_string, 8);
        EncInfoField(label, enc_string);
        std::string label2 = std::format("{} (文件 ID)", label);
        InfoField(label2.c_str(), "0x%X", file_id);
        return false;
    }
    bool FileIdField(const char* label, const wchar_t* enc_str)
    {
        // Show the raw enc chars, not a re-encoding of the decoded id — when decode fails (0), the raw
        // string is the only clue to what the game actually passed.
        EncInfoField(label, enc_str);
        const std::string label2 = std::format("{} (文件 ID)", label);
        InfoField(label2.c_str(), "0x%X", ArenaNetFileParser::FileHashToFileId(enc_str));
        return false;
    }

    void GetIdsFromFileId(const uint32_t param_1, short* param_2)
    {
        param_2[1] = static_cast<short>((param_1 - 1) / 0xff00) + 0x100;
        *param_2 = static_cast<short>((param_1 - 1) % 0xff00) + 0x100;
    }

    void DrawMapInfo(GW::Constants::MapID map_id)
    {
        static char info_id[16];
        snprintf(info_id, _countof(info_id), "map_info_%d", map_id);
        ImGui::PushID(info_id);
        auto type = "";
        switch (GW::Map::GetInstanceType()) {
            case GW::Constants::InstanceType::Outpost:
                type = "前哨站";
                break;
            case GW::Constants::InstanceType::Explorable:
                type = "探索区域";
                break;
            case GW::Constants::InstanceType::Loading:
                type = "加载中";
                break;
        }
        InfoField("地图 ID", "%d", map_id);
        ImGui::ShowHelp("每个区域的地图 ID 是唯一的");
        InfoField("地图区域", "%d", GW::Map::GetRegion());
        InfoField("地图分区", "%d", GW::Map::GetDistrict());
        InfoField("地图类型", type);
        FileIdField("地图文件 ID", mapfile);
        ImGui::ShowHelp("地图文件对于每个路径地图（例如小地图使用）是唯一的。\n许多不同的地图使用同一个地图文件");
        const GW::AreaInfo* map_info = GW::Map::GetMapInfo(map_id);
        if (map_info) {
            static wchar_t name_enc[8];
            if (GW::UI::UInt32ToEncStr(map_info->name_id, name_enc, 8)) {
                FileIdField("名称加密", name_enc);
                InfoField("名称", "%s", Resources::DecodeStringId(map_info->name_id)->string().c_str());
            }

            static wchar_t desc_enc[8];
            if (GW::UI::UInt32ToEncStr(map_info->description_id, desc_enc, 8)) {
                FileIdField("描述加密", desc_enc);
                InfoField("描述", "%s", Resources::DecodeStringId(map_info->description_id)->string().c_str());
            }
        }

        if (ImGui::TreeNodeEx("高级##map_advanced", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
            if (map_info) {
                InfoField("战役", "%d", map_info->campaign);
                InfoField("大陆", "%d", map_info->continent);
                InfoField("区域", "%d", map_info->region);
                InfoField("类型", "%d", map_info->type);
                InfoField("任务完成?", "%d", ToolboxUtils::GetMissionState(map_id, GW::PartyMgr::GetIsPartyInHardMode()));
                InfoField("标记", "0x%X", map_info->flags);
                InfoField("缩略图 ID", "%d", map_info->thumbnail_id);
                const auto m = GW::GetMapContext();
                if (m) {
                    InfoField("地图边界", "%.0f, %.0f, %.0f, %.0f, %.0f", m->start_pos.x, m->start_pos.y, m->end_pos.x, m->end_pos.y);
                }
                GW::Vec2f pos = {static_cast<float>(map_info->x), static_cast<float>(map_info->y)};
                InfoField("地图坐标", "%.2f, %.2f", pos.x, pos.y);
                if (!pos.x) {
                    pos.x = static_cast<float>(map_info->icon_start_x + (map_info->icon_end_x - map_info->icon_start_x) / 2);
                    pos.y = static_cast<float>(map_info->icon_start_y + (map_info->icon_end_y - map_info->icon_start_y) / 2);
                }
                if (!pos.x) {
                    pos.x = static_cast<float>(map_info->icon_start_x_dupe + (map_info->icon_end_x_dupe - map_info->icon_start_x_dupe) / 2);
                    pos.y = static_cast<float>(map_info->icon_start_y_dupe + (map_info->icon_end_y_dupe - map_info->icon_start_y_dupe) / 2);
                }
                InfoField("计算坐标", "%.2f, %.2f", pos.x, pos.y);
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
        InfoField("技能 ID", "%d", skill->skill_id);
        InfoField("名称", "%s", name->string().c_str());
        auto draw_advanced = [&, skill] {
            InfoField("地址", "%p", skill);
            InfoField("类型", "%d", skill->type);
            InfoField("标记", "%08x", skill->special);
            short file_ids[2];
            GetIdsFromFileId(skill->icon_file_id, file_ids);
            InfoField("文件 ID", "%08x %04x %04x", skill->icon_file_id, file_ids[0], file_ids[1]);
            GetIdsFromFileId(skill->icon_file_id_2, file_ids);
            InfoField("文件 ID 2", "%04x %04x", file_ids[0], file_ids[1]);
            EncInfoField("名称加密", name->encoded().c_str());
            wchar_t out[8];
            GW::UI::UInt32ToEncStr(skill->description, out, _countof(out));
            EncInfoField("描述加密", out);
        };
        if (force_advanced) {
            draw_advanced();
        }
        else if (ImGui::TreeNodeEx("高级##skill", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
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
        if (ImGui::TreeNodeEx("公会信息", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
            ImGui::PushID("guild_info");
            InfoField("地址", "0x%p", guild);
            InfoField("名称", "%s [%s]", TextUtils::WStringToString(guild->name).c_str(), TextUtils::WStringToString(guild->tag).c_str());
            InfoField("势力值", "%d (%s)", guild->faction_point, guild->faction ? "勒克森" : "库兹柯");
            ImGui::PopID();
            ImGui::TreePop();
        }
        ImGui::PopID();
    }

    void DrawHomAchievements(const GW::Player* player)
    {
        if (ImGui::TreeNodeEx("纪念堂信息", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
            if (!target_achievements.contains(player->name)) {
                auto* achievements = new HallOfMonumentsAchievements();
                achievements->character_name = player->name;
                target_achievements[achievements->character_name] = achievements;
                HallOfMonumentsModule::AsyncGetAccountAchievements(achievements->character_name, achievements);
            }
            const auto hom_result = target_achievements[player->name];
            if (ImGui::Button("前往纪念堂计算器")) {
                hom_result->OpenInBrowser();
            }
            InfoField("虔诚点数", "%d/%d", hom_result->devotion_points_total, 8);
            InfoField("伙伴点数", "%d/%d", hom_result->fellowship_points_total, 8);
            InfoField("荣誉点数", "%d/%d", hom_result->honor_points_total, 18);
            InfoField("坚韧点数", "%d/%d", hom_result->resilience_points_total, 8);
            InfoField("勇气点数", "%d/%d", hom_result->valor_points_total, 8);
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
        InfoField("背包/槽位", "%s", slot);
        InfoField("模型 ID", "%d", item->model_id);
        InfoField("名称", "%s", name->string().c_str());
        ImGui::Image(*Resources::GetItemImage(item), {48, 48});
        auto draw_advanced = [&, item] {
            InfoField("地址", "%p", item);
            InfoField("ID", "%d", item->item_id);
            InfoField("类型", "%d", item->type);
            InfoField("交互", "0x%X", item->interaction);
            InfoField("model_file_id", "0x%X", item->model_file_id);
            EncInfoField("名称加密", item->name_enc);
            EncInfoField("名称加密（无修饰）", ItemDescriptionHandler::GetItemEncNameWithoutMods(item).c_str());
            EncInfoField("完整名称加密", item->complete_name_enc);
            EncInfoField("描述加密", item->info_string);
            if (item->mod_struct_size) {
                ImGui::Text("修饰结构（标识符, 参数1, 参数2）");
                char mod_struct_label[] = "###修饰结构 1";
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
        else if (ImGui::TreeNodeEx("高级##item", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
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
        ImGui::ShowHelp("Agent ID 在实例中对每个 agent 是唯一的，\n它会在生成时产生，并在不同实例中改变。");
        InfoField("X 坐标", "%.2f", agent->pos.x);
        InfoField("Y 坐标", "%.2f", agent->pos.y);
        GW::Vec2f world_map_pos;
        WorldMapWidget::GamePosToWorldMap(agent->pos, world_map_pos);
        InfoField("世界地图 X 坐标", "%.2f", world_map_pos.x);
        InfoField("世界地图 Y 坐标", "%.2f", world_map_pos.y);
        const float speed = sqrtf(agent->move_x * agent->move_x + agent->move_y * agent->move_y);
        InfoField("速度（相对）", "%.2f (%.2f) ", speed, speed > 0.f ? speed / 288.0f : 0.f);
        if (living) {
            InfoField(living->IsPlayer() ? "玩家 ID" : "模型 ID", "%d", living->player_number);
            ImGui::ShowHelp(
                "模型 ID 对每种 agent 是唯一的。\n"
                "它是静态的，相同的 agent 共享此 ID。\n"
                "当目标为玩家时，此值为玩家 ID，在实例中对每个玩家唯一。\n"
                "对于目标热键和命令，请使用此值"
            );
        }
        if (item && item_actual) {
            if (ImGui::TreeNodeEx("物品信息", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
                static GuiUtils::EncString item_name;
                DrawItemInfo(item_actual, &item_name);
                ImGui::TreePop();
            }
        }
        if (player) {
            if (ImGui::TreeNodeEx("玩家信息", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
                ImGui::PushID("player_info");
                InfoField("地址", "%p", player);
                InfoField("名称", "%s", TextUtils::WStringToString(player->name).c_str());
                if (player->active_title_tier) {
                    const GW::TitleTier& tier = GW::GetGameContext()->world->title_tiers[player->active_title_tier];
                    static GuiUtils::EncString title_enc_string;
                    title_enc_string.reset(tier.tier_name_enc);
                    InfoField("当前称号", "%s", title_enc_string.string().c_str());
                }
                ImGui::PopID();
                ImGui::TreePop();
            }
            DrawHomAchievements(player);
        }
        DrawGuildInfo(guild);
        if (is_player && ImGui::TreeNodeEx("效果", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
            GW::EffectArray* effects = GW::Effects::GetAgentEffects(agent->agent_id);
            if (effects) {
                for (auto& effect : *effects) {
                    ImGui::Text("id: %d | attrib level: %d | skill: %d | duration: %u", effect.effect_id, effect.attribute_level, effect.skill_id, effect.GetTimeRemaining() / 1000);
                }
            }
            ImGui::TreePop();
        }
        if (is_player && ImGui::TreeNodeEx("增益", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
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
        if (ImGui::TreeNodeEx("高级", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
            InfoField("地址", "%p", agent);
            EncInfoField("名称", GW::Agents::GetAgentEncName(agent));
            InfoField("平面", "%d", agent->plane);
            InfoField("类型", "0x%X", agent->type);
            InfoField("宽度", "%f", agent->width1);
            InfoField("高度", "%f", agent->height1);
            InfoField("旋转", "%f", agent->rotation_angle);
            InfoField("名称属性", "0x%X", agent->name_properties);
            InfoField("距离", "%.2f", me ? GetDistance(me->pos, agent->pos) : 0.f);
            InfoField("视觉效果", "0x%X", agent->visual_effects);
            if (item_actual) {
                InfoField("拥有者", "%d", item->owner);
                InfoField("ItemId", "%d", item->item_id);
                InfoField("ExtraType", "%d", item->extra_type);
            }
            if (gadget) {
                InfoField("Gadget ID", "%d", gadget->gadget_id);
                InfoField("ExtraType", "%d", gadget->extra_type);
            }
            if (living) {
                InfoField("武器攻击速度", "%f", living->weapon_attack_speed);
                InfoField("攻击速度修正", "%f", living->attack_speed_modifier);
                InfoField("主职", "%d", living->primary);
                InfoField("副职", "%d", living->secondary);
                InfoField("等级", "%d", living->level);
                InfoField("队伍 ID", "%d", living->team_id);
                InfoField("效果", "0x%X", living->effects);
                InfoField("模型状态", "0x%X", living->model_state);
                InfoField("typeMap", "0x%X", living->type_map);
                InfoField("阵营", "0x%X", living->allegiance);
                InfoField("武器类型", "%d", living->weapon_type);
                InfoField("技能", "%d", living->skill);
                InfoField("动画代码", "0x%X", living->animation_code);
                InfoField("动画 ID", "0x%X", living->animation_id);
                InfoField("动画类型", "0x%X", living->animation_type);
                InfoField("动画速度", "%.3f", living->animation_speed);
            }
            if (npc) {
                ImGui::PushID("npc_info");
                InfoField("地址", "%p", npc);
                InfoField("NPC ID", "%d", npc_id);
                InfoField("NPC 模型文件 ID", "0x%X", npc->model_file_id);
                InfoField("NPC 皮肤文件 ID", "0x%X", npc->skin_file_id);
                InfoField("NPC 调整", "0x%X", npc->visual_adjustment);
                InfoField("NPC 外观", "0x%X", npc->appearance);
                if (npc->files_count) {
                    InfoField("NPC 模型文件", "0x%X", npc->model_files[0]);
                }
                InfoField("NPC 标记", "0x%X", npc->npc_flags);
                EncInfoField("NPC 名称", npc->name_enc);
                ImGui::PopID();
            }
            const auto map_agent = GW::Agents::GetMapAgentByID(agent->agent_id);
            if (map_agent) {
                InfoField("地图 agent 效果", "0x%X", map_agent->effects);
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
            if (!ResignLogModule::PrintResignStatus(partymember.login_number, buf, true)) continue;
            ImGui::PushID(static_cast<int>(partymember.login_number));
            if (ImGui::Button("发送")) {
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
    std::unordered_map<std::wstring, RecordedAsyncDecode*> enc_strings_recorded;

    void OnRecordedAsyncDecode_Decoded(void* param, const wchar_t* decoded)
    {
        auto e = (RecordedAsyncDecode*)param;
        e->decoded = decoded;
        e->decoded_str = TextUtils::WStringToString(e->decoded);
    }

    void __cdecl OnValidateAsyncDecodeStr(const wchar_t* s, void* cb, void* wParam)
    {
        GW::Hook::EnterHook();
        if (s && wcsncmp(s, L"\x8103\xBB3", 2) != 0 && wcsncmp(s, L"\x55b\x101", 2) != 0 && enc_strings_recorded.find(s) == enc_strings_recorded.end()) {
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

    bool OnGWCASendUIMessage(GW::UI::UIMessage msgid, void* wParam, void* lParam, bool skip_hooks)
    {
        GW::Hook::EnterHook();
        auto res = GWCA_SendUIMessage_Ret(msgid, wParam, lParam, skip_hooks);
        if (record_ui_messages) ui_message_packets_recorded.add({msgid, wParam, lParam, skip_hooks});
        GW::Hook::LeaveHook();
        return res;
    }

    void OnEventMessage(GW::HookStatus*, GW::EventMgr::EventID event_id, void* packet, uint32_t packet_size)
    {
        if (record_event_messages) event_message_packets_recorded.add({event_id, packet, packet_size});
    }

    uint32_t FileHashToFileId(wchar_t* param_1)
    {
        if (!param_1) return 0;
        if (((0xff < *param_1) && (0xff < param_1[1])) && ((param_1[2] == 0 || ((0xff < param_1[2] && (param_1[3] == 0)))))) {
            return (*param_1 - 0xff00ff) + (uint32_t)param_1[1] * 0xff00;
        }
        return 0;
    }

    uint32_t* OnCreateTexture(wchar_t* file_name, uint32_t flags)
    {
        GW::Hook::EnterHook();
        const auto out = CreateTexture_Ret(file_name, flags);
        uint32_t file_id = FileHashToFileId(file_name);
        if (textures_created_by_file_id.find(file_id) == textures_created_by_file_id.end()) {
            const auto f = GwDatModule::LoadTextureFromFileId(file_id);
            textures_created.push_back(f);
            textures_created_by_file_id[file_id] = f;
            texture_file_ids[f] = file_id;
        }
        GW::Hook::LeaveHook();
        return out;
    }

    void HookOnValidateAsyncDecodeStr(bool hook)
    {
        if (hook && ValidateAsyncDecodeStr_Func) return;
        if (hook) {
            ValidateAsyncDecodeStr_Func = (DoAsyncDecodeStr_pt)GW::Scanner::ToFunctionStart(GW::Scanner::FindUseOfString("(codedString[0] & ~WORD_BIT_MORE) >= WORD_VALUE_BASE"));
            DEBUG_ASSERT(ValidateAsyncDecodeStr_Func);
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
    void HookOnCreateTexture(bool hook)
    {
        if (hook && CreateTexture_Func) return;
        if (hook) {
            CreateTexture_Func = (CreateTexture_pt)GW::Scanner::ToFunctionStart(GW::Scanner::FindAssertion("GrTex2d.cpp", "!(flags & GR_TEXTURE_TRANSFER_OWNERSHIP)", 0, 0));
            DEBUG_ASSERT(CreateTexture_Func);
            if (CreateTexture_Func) {
                GW::Hook::CreateHook((void**)&CreateTexture_Func, OnCreateTexture, (void**)&CreateTexture_Ret);
                GW::Hook::EnableHooks(CreateTexture_Func);
            }
        }
        else if (CreateTexture_Func) {
            GW::Hook::RemoveHook(CreateTexture_Func);
            CreateTexture_Func = 0;
            textures_created_by_file_id.clear();
            textures_created.clear();
            texture_file_ids.clear();
        }
    }
    void HookOnGWCASendUIMessage(bool hook)
    {
        if (hook && GWCA_SendUIMessage_Func) return;
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

    void HighlightFrame(GW::UI::Frame* frame)
    {
        if (!frame) return;
        const auto root = GW::UI::GetRootFrame();
        const auto top_left = frame->position.GetTopLeftOnScreen(root);
        const auto bottom_right = frame->position.GetBottomRightOnScreen(root);
        const auto draw_list = ImGui::GetBackgroundDrawList();
        draw_list->AddRect({top_left.x, top_left.y}, {bottom_right.x, bottom_right.y}, IM_COL32_WHITE);
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

    bool DownloadStringFiles()
    {
        wchar_t** file_ids = 0;
        ArenaNetFileParser::GameAssetFile asset;
        auto addr = GW::Scanner::FindUseOfString("index < arrsize(s_fileId)", 0x11);
        DEBUG_ASSERT(addr);
        if (!(addr && GW::Scanner::IsValidPtr(*(uintptr_t*)addr, GW::ScannerSection::Section_DATA))) {
            return false;
        }
        file_ids = *(wchar_t***)addr;
        // wchar_t *[18][99] s_fileId
        for (size_t language_id = 0; language_id < 18; language_id++) {
            wchar_t** language_files = &file_ids[language_id * 99];
            for (size_t file_idx = 0; file_idx < 99; file_idx++) {
                const auto file_name = language_files[file_idx];
                if (!(file_name && *file_name)) continue;
                if (!asset.readFromDat(file_name)) return false;
                const auto filename = std::format("language_file_{}_{}.txt", language_id, file_idx);
                const auto write_to = Resources::GetPath("language_files", filename);

                if (!Resources::WriteFile(write_to, std::string(reinterpret_cast<const char*>(asset.data.data()), asset.data.size()))) {
                    Log::Warning("写入磁盘失败 DownloadStringFiles");
                    return false;
                }
            }
        }
        return true;
    }

    void PostDraw()
    {
        HookOnCreateTexture(record_textures);
        HookOnValidateAsyncDecodeStr(record_enc_strings);
        HookOnGWCASendUIMessage(record_ui_messages);
        HookOnGWCASendEventMessage(record_event_messages);
    }
    const uint32_t GetMapPropModelFileId(GW::MapProp* prop)
    {
        if (!(prop && prop->model_info)) return 0;
        return ArenaNetFileParser::FileHashToFileId(prop->model_info->model_file_name);
    };
    void DrawDebugInfo()
    {
        if (!SetFpsLimits_Func) {
            SetFpsLimits_Func = (SetFpsLimits_pt)GW::Scanner::ToFunctionStart(GW::Scanner::Find("\x68\x40\x42\x0f\x00\xe8", "xxxxxx"));
            DEBUG_ASSERT(SetFpsLimits_Func);
            if (SetFpsLimits_Func) {
                GW::Hook::CreateHook((void**)&SetFpsLimits_Func, OnSetFpsLimits, (void**)&SetFpsLimits_Ret);
                GW::Hook::EnableHooks(SetFpsLimits_Func);
            }
        }
        ImGui::Text("FPS 限制: %d", target_fps);
        if (ImGui::CollapsingHeader("账号特性")) {
            const auto& features = GW::GetGameContext()->account->account_unlocked_counts;
            ImGui::PushItemWidth(140.f);
            ImGui::TextUnformatted("ID");
            ImGui::SameLine();
            ImGui::TextUnformatted("值 1");
            ImGui::SameLine();
            ImGui::TextUnformatted("值 2");
            for (const auto& feature : features) {
                ImGui::PushID(feature.id);
                ImGui::Text("0x%x", feature.id);
                ImGui::SameLine();
                ImGui::Text("%d", feature.unk1);
                ImGui::SameLine();
                ImGui::Text("%d", feature.unk2);
                ImGui::PopID();
            }
        }
        if (ImGui::CollapsingHeader("引用的物品")) {
            ImGui::Text("最近引用的物品（从交易者购买或出售）");
            static GuiUtils::EncString quoted_name;
            DrawItemInfo(GW::Items::GetItemById(quoted_item_id), &quoted_name);
        }


        if (ImGui::CollapsingHeader("UI 消息日志")) {
            record_ui_messages = true;
            ImGui::PushID("ui_message_packets_recorded");
            if (ImGui::SmallButton("重置")) {
                ui_message_packets_recorded.clear();
            }
            for (const auto packet : ui_message_packets_recorded) {
                ImGui::Text("0x%08x 0x%08x 0x%08x", packet.msgid, packet.wParam, packet.lParam);
            }
            ImGui::PopID();
            if (ImGui::IsKeyDown(ImGuiMod_Alt)) {
                ImGui::SetScrollHereY();
            }
        }
        if (ImGui::CollapsingHeader("事件消息日志")) {
            record_event_messages = true;
            if (ImGui::SmallButton("重置")) {
                event_message_packets_recorded.clear();
            }
            for (const auto packet : event_message_packets_recorded) {
                ImGui::Text("0x%08x 0x%08x", packet.event_id, packet.packet);
            }
            if (ImGui::IsKeyDown(ImGuiMod_Alt)) {
                ImGui::SetScrollHereY();
            }
        }



        if (ImGui::CollapsingHeader("异步字符串日志")) {
            record_enc_strings = true;
            if (ImGui::SmallButton("重置")) {
                while (enc_strings_recorded.begin() != enc_strings_recorded.end()) {
                    delete enc_strings_recorded.begin()->second;
                    enc_strings_recorded.erase(enc_strings_recorded.begin());
                }
            }
            size_t i = 0;
            for (const auto& [key, value] : enc_strings_recorded) {
                ImGui::PushID(i++);
                EncInfoField("加密", value->s.c_str());
                InfoField("解码", "%s", value->decoded_str.c_str());
                ImGui::PopID();
            }
        }
        const auto target = GW::Agents::GetTarget();
        if (target && ImGui::CollapsingHeader("目标范围内的道具")) {
            float range = GW::Constants::Range::Area;
            const auto props = target ? GW::Map::GetMapProps() : nullptr;
            if (props) {
                ImGui::Indent();
                ImGui::TextUnformatted("模型文件 ID");
                ImGui::SameLine(128.f);
                ImGui::TextUnformatted("距离");
                ImGui::SameLine(256.f);
                ImGui::TextUnformatted("位置");
                ImGui::Separator();
                for (size_t i = 0, cnt = props->size(); i < cnt; i++) {
                    const auto& prop = (*props)[i];
                    float distance = GW::GetDistance(target->pos, GW::GamePos({prop->position.x, prop->position.y, 0}));
                    if (distance > range) continue;
                    ImGui::PushID(i);
                    ImGui::Text("%08X", GetMapPropModelFileId(prop));
                    ImGui::SameLine(128.f);
                    ImGui::Text("%.2f", distance);
                    ImGui::SameLine(256.f);
                    const auto label = std::format("{}, {}", prop->position.x, prop->position.y);
                    if (ImGui::Button(label.c_str())) {
                        GW::Map::PingCompass(GW::GamePos({prop->position.x, prop->position.y, 0}));
                    }
                    ImGui::PopID();
                }
                ImGui::Unindent();
            }
        }
        if (ImGui::CollapsingHeader("按 GW 文件加载的纹理")) {
            record_textures = true;
            constexpr ImVec2 scaled_size = {64.f, 64.f};
            constexpr ImVec4 tint(1, 1, 1, 1);
            const auto normal_bg = ImColor(IM_COL32(0, 0, 0, 0));
            constexpr auto uv0 = ImVec2(0, 0);

            if (ImGui::SmallButton("重置")) {
                textures_created_by_file_id.clear();
                textures_created.clear();
                texture_file_ids.clear();
            }

            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.f, 0.5f));

            ImGui::StartSpacedElements(scaled_size.x);

            size_t i = 0;
            for (const auto texture : textures_created) {
                ImGui::PushID(i++);
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
                    ImGui::SetTooltip("文件 ID: 0x%08x\n文件哈希: 0x%04x 0x%04x", texture_file_ids[texture], out[0], out[1]);
                }
                if (clicked) {
                    ImGui::SetContextMenu([texture](void*) {
                        if (ImGui::Button("下载为 DDS（按 GW 文件 ID 命名）")) {
                            const auto filename = std::format("{:#010x}.dds", texture_file_ids[texture]);
                            const auto write_to = Resources::GetPath("extracted_textures", filename);
                            Resources::EnsureFolderExists(Resources::GetPath("extracted_textures"));
                            Resources::SaveTextureToFile(*texture, write_to);
                            return false;
                        }
                        if (ImGui::Button("下载为 DDS（按 gMod 哈希命名）")) {
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
        }
        static bool game_master_mode = false;
        if (ImGui::Checkbox("游戏大师模式", &game_master_mode)) {
            if (game_master_mode) {
                GW::GetCharContext()->player_flags |= 0x8;
            }
            else {
                GW::GetCharContext()->player_flags ^= 0x8;
            }
        }
        if (ImGui::Button("打开文本开发窗口")) {
            GW::GameThread::Enqueue([] {
                GW::GetCharContext()->player_flags |= 0x8;
                GW::UI::UIPacket::kKeyAction packet;
                packet.gw_key = (GW::UI::ControlAction)0x25;
                packet.state_flags = 0x6; // Ctrl and shift
                GW::UI::SendFrameUIMessage(GW::UI::GetChildFrame(GW::UI::GetFrameByLabel(L"Game"), 6), GW::UI::UIMessage::kKeyDown, &packet);
                GW::GetCharContext()->player_flags ^= 0x8;
            });
        }
        if (ImGui::Button("打开 GM 开始菜单？")) {
            GW::GameThread::Enqueue([] {
                GW::GetCharContext()->player_flags |= 0x8;
                GW::UI::SendUIMessage((GW::UI::UIMessage)0x1000008a, 0, 0);
                // GW::GetCharContext()->player_flags ^= 0x8;
            });
        }
        if (ImGui::Button("DownloadStringFiles")) {
            Resources::EnqueueWorkerTask([]() {
                Log::Info("下载字符串中...");
                DownloadStringFiles() || (Log::Error("下载字符串文件失败"), true);
                Log::Info("完成");
            });
        }


        // For debugging changes to flags/arrays etc
        [[maybe_unused]] const GW::GameContext* g = GW::GetGameContext();
        if (!g) return;
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

        [[maybe_unused]] const auto campaign = ac ? ac->campaign() : (GW::Constants::Campaign)0;
        [[maybe_unused]] const auto level = ac ? ac->level() : 0;
        [[maybe_unused]] const auto primary = ac ? ac->primary() : (GW::Constants::Profession)0;
        [[maybe_unused]] const auto secondary = ac ? ac->secondary() : (GW::Constants::Profession)0;
        [[maybe_unused]] const auto salvage_session = GW::Items::GetSalvageSessionInfo();
#ifdef _DEBUG
        if (ImGui::CollapsingHeader("框架查找器")) {
            static char frame_label[64] = "";
            static int child_offsets[3] = {0, 0, 0};
            static int depth = 0;

            ImGui::InputText("框架标签", frame_label, sizeof(frame_label));
            ImGui::ShowHelp("传递给 GW::UI::GetFrameByLabel，例如 \"Vendor\", \"Game\", \"Compass\"。");
            ImGui::SliderInt("子级深度", &depth, 0, 3);
            for (int j = 0; j < depth; j++) {
                char lbl[24];
                snprintf(lbl, sizeof(lbl), "子级偏移 %d", j);
                ImGui::InputInt(lbl, &child_offsets[j]);
                if (child_offsets[j] < 0) child_offsets[j] = 0;
            }

            GW::UI::Frame* frame = nullptr;
            if (frame_label[0]) {
                frame = GW::UI::GetFrameByLabel(TextUtils::StringToWString(frame_label).c_str());
                for (int j = 0; j < depth && frame; j++)
                    frame = GW::UI::GetChildFrame(frame, static_cast<uint32_t>(child_offsets[j]));
            }

            if (frame) {
                InfoField("框架地址", "%p", static_cast<void*>(frame));
                InfoField("框架 ID", "%u", frame->frame_id);
                InfoField("子级偏移 ID", "%u", frame->child_offset_id);
                HighlightFrame(frame); // draws a box around the frame's on-screen bounds
            }
            else {
                ImGui::TextDisabled("%s", frame_label[0] ? "该标签+偏移没有找到框架。" : "输入框架标签。");
            }
        }
#endif
    }

    void OnPostUIMessage(GW::HookStatus*, GW::UI::UIMessage message_id, void* wParam, void*)
    {
        switch (message_id) {
            case GW::UI::UIMessage::kLoadMapContext: {
                const auto packet = static_cast<GW::UI::UIPacket::kLoadMapContext*>(wParam);
                if (packet->file_name) {
                    wcsncpy(mapfile, packet->file_name, _countof(mapfile) - 1);
                    mapfile[_countof(mapfile) - 1] = 0;
                }
                else {
                    mapfile[0] = 0;
                }
            } break;
        }
    }

} // namespace

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
void InfoWindow::SignalTerminate()
{
    visible = false;
}

void InfoWindow::Initialize()
{
    ToolboxWindow::Initialize();
    SettingsRegistry::Register(this, settings);

    ui_message_packets_recorded = CircularBuffer<UIMessagePacket>(512);
    event_message_packets_recorded = CircularBuffer<EventPacket>(512);

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::QuotedItemPrice>(&InstanceLoadFile_Entry, [this](GW::HookStatus*, const GW::Packet::StoC::QuotedItemPrice* packet) -> void {
        quoted_item_id = packet->itemid;
    });
    RegisterUIMessageCallback(&InstanceLoadFile_Entry, GW::UI::UIMessage::kLoadMapContext, OnPostUIMessage, 0x8000);
}

void InfoWindow::LoadSettings(SettingsDoc& doc, ToolboxIni* legacy)
{
    ToolboxWindow::LoadSettings(doc, legacy);
    doc.GetStruct(Name(), settings);
}

void InfoWindow::SaveSettings(SettingsDoc& doc)
{
    ToolboxWindow::SaveSettings(doc);
    doc.SetStruct(Name(), settings);
}

void InfoWindow::Draw(IDirect3DDevice9*)
{
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
        if (settings.show_widgets) {
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

        if (settings.show_open_chest) {
            if (ImGui::Button("打开快捷仓库", ImVec2(-1.0f, 0))) {
                GW::GameThread::Enqueue([] {
                    GW::Items::OpenXunlaiWindow();
                });
            }
        }

        if (ImGui::CollapsingHeader("摄像机")) {
            const GW::Camera* cam = GW::CameraMgr::GetCamera();
            if (cam != nullptr) {
                InfoField("位置##cam_pos", "%.2f, %.2f, %.2f", cam->position.x, cam->position.y, cam->position.z);
                InfoField("目标##cam_target", "%.2f, %.2f, %.2f", cam->look_at_target.x, cam->look_at_target.y, cam->look_at_target.z);
                InfoField("偏航/俯仰##cam_angle", "%.2f, %.2f", cam->GetCurrentYaw(), cam->pitch);
            }
        }
        if (settings.show_player && ImGui::CollapsingHeader("玩家")) {
            ImGui::PushID("player_info");
            InfoField("正在输入?", "%s", GW::Chat::GetIsTyping() ? "是" : "否");
            DrawAgentInfo(GW::Agents::GetObservingAgent());
            ImGui::PopID();
        }
        if (settings.show_target && ImGui::CollapsingHeader("目标")) {
            ImGui::PushID("target_info");
            DrawAgentInfo(GW::Agents::GetTarget());
            ImGui::PopID();
        }
        if (settings.show_map && ImGui::CollapsingHeader("地图")) {
            DrawMapInfo(GW::Map::GetMapID());
        }
        if (settings.show_map && ImGui::CollapsingHeader("查找地图")) {
            static int map_id = 0;
            ImGui::InputInt("地图 ID", &map_id, 1, 1);
            const auto current = GW::Map::GetMapInfo(static_cast<GW::Constants::MapID>(map_id));
            if (current) DrawMapInfo(static_cast<GW::Constants::MapID>(map_id));
        }
        if (settings.show_dialog && ImGui::CollapsingHeader("对话框")) {
            EncInfoField("对话框内容", DialogModule::GetDialogBody());
            InfoField("最后对话框", "0x%X", DialogModule::LastDialogId());
            ImGui::Text("可用的 NPC 对话框：");
            ImGui::ShowHelp("与 NPC 交谈可查看可用对话框");
            const auto& messages = DialogModule::GetDialogButtonMessages();
            const auto& buttons = DialogModule::GetDialogButtons();
            char bbuf[48];
            for (size_t i = 0; i < buttons.size(); i++) {
                snprintf(bbuf, _countof(bbuf), "send_dialog_%d", i);
                ImGui::PushID(bbuf);
                if (ImGui::Button("发送")) {
                    uint32_t dialog_id = buttons[i]->dialog_id;
                    GW::GameThread::Enqueue([dialog_id] {
                        DialogModule::SendDialog(dialog_id);
                    });
                }
                ImGui::SameLine();
                InfoField("图标", "0x%X", buttons[i]->button_icon);
                EncInfoField("加密", messages[i]->encoded().c_str());
                InfoField(messages[i]->string().c_str(), "0x%X", buttons[i]->dialog_id);
                ImGui::PopID();
            }
        }
        if (ImGui::CollapsingHeader("悬停技能")) {
            static GuiUtils::EncString skill_name;
            const auto current = GW::SkillbarMgr::GetHoveredSkill();
            if (current) {
                last_hovered_skill_id = current->skill_id;
            }
            DrawSkillInfo(GW::SkillbarMgr::GetSkillConstantData(last_hovered_skill_id), &skill_name, true);
        }
        if (ImGui::CollapsingHeader("查找技能")) {
            static GuiUtils::EncString skill_name;
            static int skill_id = 0;
            ImGui::InputInt("技能 ID", &skill_id, 1, 1);
            const auto current = GW::SkillbarMgr::GetSkillConstantData(static_cast<GW::Constants::SkillID>(skill_id));
            if (current) DrawSkillInfo(current, &skill_name, true);
        }
        if (settings.show_item && ImGui::CollapsingHeader("悬停物品")) {
            static GuiUtils::EncString item_name;
            ImGui::PushID("hovered_item");
            const GW::Item* current = GW::Items::GetHoveredItem();
            if (current) {
                last_hovered_item_id = current->item_id;
            }
            DrawItemInfo(GW::Items::GetItemById(last_hovered_item_id), &item_name, true);
            ImGui::PopID();
        }
        if (settings.show_item && ImGui::CollapsingHeader("物品")) {
            ImGui::Text("背包中的第一个物品");
            static GuiUtils::EncString item_name;
            DrawItemInfo(GW::Items::GetItemBySlot(GW::Items::GetBag(GW::Constants::Bag::Backpack), 1), &item_name);
        }
        if (settings.show_quest && ImGui::CollapsingHeader("任务")) {
            const GW::Quest* q = GW::QuestMgr::GetActiveQuest();
            if (q) {
                ImGui::Text("ID: 0x%X", q->quest_id);
                ImGui::Text("标记: (%.0f, %.0f)", q->marker.x, q->marker.y);
                ImGui::Text("状态: 0x%08x", q->log_state);
                EncInfoField("地点:", q->location);
                static wchar_t name_buf[128];
                GetQuestEntryGroupName(q->quest_id, name_buf, _countof(name_buf));
                EncInfoField("任务条目:", name_buf);
                EncInfoField("目标:", q->objectives);
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
            ImGui::Text("缺少信息的任务: %d", quests_missing_info.size());
            ImGui::SameLine();
            if (ImGui::SmallButton("请求任务信息")) {
                for (const auto& quest : quests_missing_info) {
                    GW::QuestMgr::RequestQuestInfo(quest, true);
                }
            }
#endif
        }
        if (settings.show_mobcount && ImGui::CollapsingHeader("敌人数量")) {
            constexpr float sqr_soul_range = 1400.0f * 1400.0f;
            int soul_count = 0;
            int cast_count = 0;
            int spirit_count = 0;
            int compass_count = 0;
            GW::AgentArray* agents = GW::Agents::GetAgentArray();
            const GW::Agent* player = GW::Agents::GetObservingAgent();
            if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Loading && agents && player != nullptr) {
                for (auto* a : *agents) {
                    const GW::AgentLiving* agent = a ? a->GetAsAgentLiving() : nullptr;
                    if (!(agent && agent->allegiance == GW::Constants::Allegiance::Enemy)) {
                        continue; // ignore non-hostiles
                    }
                    if (agent->GetIsDead()) {
                        continue; // ignore dead
                    }
                    const float sqrd = GetSquareDistance(player->pos, agent->pos);
                    if (agent->player_number == GW::Constants::ModelID::DoA::SoulTormentor || agent->player_number == GW::Constants::ModelID::DoA::VeilSoulTormentor) {
                        if (GW::Map::GetMapID() == GW::Constants::MapID::Domain_of_Anguish && sqrd < sqr_soul_range) {
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
                ImGui::Text("%d 个灵魂折磨者", soul_count);
                ImGui::ShowHelp("仅在痛苦领域，1400 范围内");
            }
            ImGui::Text("%d 个敌人在施法范围内", cast_count);
            ImGui::Text("%d 个敌人在灵范围", spirit_count);
            ImGui::Text("%d 个敌人在罗盘范围内", compass_count);
        }
        if (settings.show_resignlog && ImGui::CollapsingHeader("退出记录") && GWToolbox::IsModuleEnabled("Resign Log")) {
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
    ImGui::Checkbox("显示小部件开关", &settings.show_widgets);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("显示 '打开快捷仓库' 按钮", &settings.show_open_chest);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("显示玩家", &settings.show_player);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("显示目标", &settings.show_target);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("显示地图", &settings.show_map);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("显示对话框", &settings.show_dialog);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("显示物品", &settings.show_item);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("显示任务", &settings.show_quest);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("显示敌人数量", &settings.show_mobcount);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("显示退出记录", &settings.show_resignlog);
}