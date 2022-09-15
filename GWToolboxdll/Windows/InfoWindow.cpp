#include "stdafx.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/GameContainers/Array.h>
#include <GWCA/GameContainers/GamePos.h>

#include <GWCA/GameEntities/Camera.h>

#include <GWCA/GameEntities/Party.h>
#include <GWCA/GameEntities/Quest.h>
#include <GWCA/GameEntities/Skill.h>
#include <GWCA/GameEntities/Player.h>
#include <GWCA/GameEntities/Guild.h>
#include <GWCA/GameEntities/NPC.h>
#include <GWCA/GameEntities/Camera.h>
#include <GWCA/GameEntities/Map.h>
#include <GWCA/GameEntities/Title.h>

#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/WorldContext.h>
#include <GWCA/Context/CharContext.h>

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

#include <GWToolbox.h>
#include <Logger.h>

#include <Modules/Resources.h>
#include <Widgets/AlcoholWidget.h>
#include <Widgets/BondsWidget.h>
#include <Widgets/ClockWidget.h>
#include <Widgets/DistanceWidget.h>
#include <Widgets/HealthWidget.h>
#include <Widgets/Minimap/Minimap.h>
#include <Widgets/PartyDamage.h>
#include <Widgets/TimerWidget.h>
#include <Windows/InfoWindow.h>
#include <Windows/NotepadWindow.h>
#include <Windows/StringDecoderWindow.h>

#include <Modules/ToolboxSettings.h>
#include <Modules/DialogModule.h>

namespace {
    uint32_t last_hovered_item_id = 0;
}

void InfoWindow::Terminate() {
    for (const auto& a : target_achievements) {
        delete a.second;
    }
    target_achievements.clear();
}

void InfoWindow::Initialize() {
    ToolboxWindow::Initialize();

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::MessageCore>(&MessageCore_Entry,OnMessageCore);
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::QuotedItemPrice>(&InstanceLoadFile_Entry,
        [this](GW::HookStatus*, GW::Packet::StoC::QuotedItemPrice* packet) -> void {
            quoted_item_id = packet->itemid;
        });
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::InstanceLoadFile>(&InstanceLoadFile_Entry,OnInstanceLoad);
    GW::Chat::CreateCommand(L"resignlog", CmdResignLog);
}
void InfoWindow::CmdResignLog(const wchar_t* cmd, int argc, wchar_t** argv) {
    UNREFERENCED_PARAMETER(cmd);
    UNREFERENCED_PARAMETER(argc);
    UNREFERENCED_PARAMETER(argv);
    if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Explorable) return;
    GW::PartyInfo* info = GW::PartyMgr::GetPartyInfo();
    if (info == nullptr) return;
    GW::PlayerPartyMemberArray& partymembers = info->players;
    if (!partymembers.valid()) return;
    auto instance = &Instance();
    size_t index_max = std::min<size_t>(instance->status.size(), partymembers.size());
    for (size_t i = 0; i < index_max; ++i) {
        GW::PlayerPartyMember& partymember = partymembers[i];
        wchar_t buffer[256];
        if (instance->status[i] == Connected) {
            instance->PrintResignStatus(buffer, 256, i, GW::PlayerMgr::GetPlayerName(partymember.login_number));
            instance->send_queue.push(std::wstring(buffer));
        }
    }
}

void InfoWindow::OnInstanceLoad(GW::HookStatus*, GW::Packet::StoC::InstanceLoadFile* packet) {
    auto instance = &Instance();
    instance->quoted_item_id = 0;
    instance->mapfile = packet->map_fileID;
    for (unsigned i = 0; i < instance->status.size(); ++i) {
        instance->status[i] = NotYetConnected;
        instance->timestamp[i] = 0;
    }
}

void InfoWindow::OnMessageCore(GW::HookStatus*, GW::Packet::StoC::MessageCore* pak) {
    // 0x107 is the "start string" marker
    if (wmemcmp(pak->message, L"\x7BFF\xC9C4\xAEAA\x1B9B\x107", 5) != 0)
        return;

    // get all the data
    GW::PartyInfo* info = GW::PartyMgr::GetPartyInfo();
    if (info == nullptr) return;
    GW::PlayerPartyMemberArray& partymembers = info->players;
    if (!partymembers.valid()) return;

    // Prepare the name
    wchar_t* start = wcschr(pak->message, 0x107) + 1;
    std::wstring buf(start, wcschr(start, 0x1) - start);
    // set the right index in party
    auto instance = &Instance();
    for (size_t i = 0; i < partymembers.size() && i < instance->status.size();i++) {
        if (instance->status[i] == Resigned) continue;
        wchar_t* player_name = GW::PlayerMgr::GetPlayerName(partymembers[i].login_number);
        if (player_name && GuiUtils::SanitizePlayerName(player_name) == buf) {
            instance->status[i] = Resigned;
            Instance().timestamp[i] = GW::Map::GetInstanceTime();
            break;
        }
    }
}

void InfoWindow::InfoField(const char* label, const char* fmt, ...) {
    static char info_string[128];
    va_list vl;
    va_start(vl, fmt);
    vsnprintf(info_string, _countof(info_string), fmt, vl);
    va_end(vl);
    ImGui::InputTextEx(label, NULL, info_string, _countof(info_string), ImVec2(-160.f * ImGui::GetIO().FontGlobalScale, 0), ImGuiInputTextFlags_ReadOnly);
}
void InfoWindow::EncInfoField(const char* label, const wchar_t* enc_string) {
    static std::string info_string;
    size_t size_reqd = enc_string ? (wcslen(enc_string) * 7) + 1 : 0;
    info_string.resize(size_reqd,0); // 7 chars = 0xFFFF plus a space
    size_t offset = 0;
    for (size_t i = 0; enc_string && enc_string[i] && offset < size_reqd - 1; i++) {
        offset += sprintf(&info_string[offset], "0x%X ", enc_string[i]);
    }
    ImGui::InputTextEx(label, NULL, info_string.data(), info_string.size(), ImVec2(-160.f * ImGui::GetIO().FontGlobalScale, 0), ImGuiInputTextFlags_ReadOnly);
}

void InfoWindow::DrawSkillInfo(GW::Skill* skill, GuiUtils::EncString* name, bool force_advanced) {
    if (!skill) return;
    name->reset(skill->name);
    static char info_id[16];
    snprintf(info_id, _countof(info_id), "skill_info_%d", skill->skill_id);
    ImGui::PushID(info_id);
    InfoField("SkillID", "%d", skill->skill_id);
    InfoField("Name", "%s", name->string().c_str());
    auto draw_advanced = [&, skill]() {
        InfoField("Addr", "%p", skill);
        InfoField("Type", "%d", skill->type);
        EncInfoField("Name Enc", name->encoded().c_str());
        wchar_t out[8];
        GW::UI::UInt32ToEncStr(skill->description,out,_countof(out));
        EncInfoField("Desc Enc", out);
    };
    if (force_advanced)
        draw_advanced();
    else if (ImGui::TreeNodeEx("Advanced##skill", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
        draw_advanced();
        ImGui::TreePop();
    }
    ImGui::PopID();
}

void InfoWindow::DrawGuildInfo(GW::Guild* guild) {
    if (!guild) return;
    ImGui::PushID(guild->index);
    if (ImGui::TreeNodeEx("Guild Info", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
        ImGui::PushID("guild_info");
        InfoField("Addr", "0x%p", guild);
        InfoField("Name", "%s [%s]", GuiUtils::WStringToString(guild->name).c_str(), GuiUtils::WStringToString(guild->tag).c_str());
        InfoField("Faction", "%d (%s)", guild->faction_point, guild->faction ? "Luxon" : "Kurzick");
        ImGui::PopID();
        ImGui::TreePop();
    }
    ImGui::PopID();
}
void InfoWindow::DrawHomAchievements(const GW::Player* player) {
    if (ImGui::TreeNodeEx("Hall of Monuments Info", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
        if (!target_achievements.contains(player->name)) {
            auto* achievements = new HallOfMonumentsAchievements();
            wcscpy(achievements->character_name, player->name);
            target_achievements[achievements->character_name] = achievements;
            HallOfMonumentsModule::AsyncGetAccountAchievements(achievements->character_name, achievements);
        }
        auto hom_result = target_achievements[player->name];
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
void InfoWindow::DrawItemInfo(GW::Item* item, GuiUtils::EncString* name, bool force_advanced) {
    if (!item) return;
    name->reset(item->single_item_name);
    static char slot[8] = "-";
    if (item->bag) {
        snprintf(slot, _countof(slot), "%d/%d", item->bag->index + 1, item->slot + 1);
    }
    InfoField("Bag/Slot", "%s",slot);
    InfoField("ModelID", "%d", item->model_id);
    InfoField("Name", "%s", name->string().c_str());
    auto draw_advanced = [&,item]() {
        InfoField("Addr", "%p", item);
        InfoField("Id", "%d", item->item_id);
        InfoField("Type", "%d", item->type);
        InfoField("Interaction", "0x%X", item->interaction);
        InfoField("model_file_id", "0x%X", item->model_file_id);
        EncInfoField("Name Enc", item->name_enc);
        EncInfoField("Desc Enc", item->info_string);
        if (item->mod_struct_size) {
            ImGui::Text("Mod Struct (identifier, arg1, arg2)");
            char mod_struct_label[] = "###Mod Struct 1";
            for (size_t i = 0; i < item->mod_struct_size; i++) {
                GW::ItemModifier* mod = &item->mod_struct[i];
                mod_struct_label[14] = static_cast<char>(i + 1) + '0';
                InfoField(mod_struct_label, "0x%X (%d %d %d)", mod->mod, mod->identifier(), mod->arg1(), mod->arg2());
            }
        }
    };
    if (force_advanced)
        draw_advanced();
    else if (ImGui::TreeNodeEx("Advanced##item", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
        draw_advanced();
        ImGui::TreePop();
    }
}
void InfoWindow::DrawAgentInfo(GW::Agent* agent) {
    if (!agent) return;
    UNREFERENCED_PARAMETER(agent);
    const GW::AgentLiving* living = agent->GetAsAgentLiving();
    bool is_player = agent->agent_id == GW::Agents::GetPlayerId();
    const GW::AgentGadget* gadget = agent->GetAsAgentGadget();
    const GW::AgentItem* item = agent->GetAsAgentItem();
    GW::Item* item_actual = item ? GW::Items::GetItemById(item->item_id) : nullptr;
    const GW::Player* player = living && living->IsPlayer() ? GW::PlayerMgr::GetPlayerByID(living->player_number) : nullptr;
    const GW::Agent* me = GW::Agents::GetPlayer();
    uint32_t npc_id = living && living->IsNPC() ? living->player_number : 0;
    if (player && living->transmog_npc_id & 0x20000000)
        npc_id = living->transmog_npc_id ^ 0x20000000;
    const GW::NPC* npc = npc_id ? GW::Agents::GetNPCByID(npc_id) : nullptr;

    GW::Guild* guild = nullptr;
    if (player && living->tags->guild_id) {
        GW::GuildArray* guilds = GW::GuildMgr::GetGuildArray();
        if (guilds && living->tags->guild_id < guilds->size())
            guild = guilds->at((uint32_t)living->tags->guild_id);
    }

    InfoField("Agent ID", "%d", agent->agent_id);
    ImGui::ShowHelp("Agent ID is unique for each agent in the instance,\nIt's generated on spawn and will change in different instances.");
    InfoField("X pos", "%.2f", agent->pos.x);
    InfoField("Y pos", "%.2f", agent->pos.y);
    float speed = sqrtf(agent->move_x * agent->move_x + agent->move_y * agent->move_y);
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
            InfoField("Name", "%s", GuiUtils::WStringToString(player->name).c_str());
            if (player->active_title_tier) {
                GW::TitleTier& tier = GW::GameContext::instance()->world->title_tiers[player->active_title_tier];
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
            for (auto& effect : *effects) {
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
        InfoField("Distance", "%.2f", me ? GW::GetDistance(me->pos, agent->pos) : 0.f);
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
            InfoField("NPC ModelFileID", "0x%X", npc->model_file_id);
            if (npc->files_count)
                InfoField("NPC ModelFile", "0x%X", npc->model_files[0]);
            InfoField("NPC Flags", "0x%X", npc->npc_flags);
            EncInfoField("NPC Name", npc->name_enc);
            InfoField ("NPC Scale", "0x%X", npc->scale);
            ImGui::PopID();
        }
        GW::MapAgent* map_agent = GW::Agents::GetMapAgentByID(agent->agent_id);
        if (map_agent) {
            InfoField("Map agent effects", "0x%X", map_agent->effects);
        }
        ImGui::TreePop();
    }
}
void InfoWindow::Draw(IDirect3DDevice9* pDevice) {
    UNREFERENCED_PARAMETER(pDevice);
    if (!visible) return;
    ImGui::SetNextWindowCenter(ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 0), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {
        if (show_widgets) {
            const std::vector<ToolboxModule *> &optional_modules = ToolboxSettings::Instance().GetOptionalModules();
            std::vector<ToolboxUIElement *> widgets;
            widgets.reserve(optional_modules.size());
            for (ToolboxModule *module : optional_modules) {
                ToolboxUIElement *widget = dynamic_cast<ToolboxUIElement *>(module);
                if (!widget || !widget->IsWidget())
                    continue;
                widgets.push_back(widget);
            }
            std::ranges::sort(widgets, [](auto *a, auto *b) { return std::strcmp(a->Name(), b->Name()) < 0; });
            const unsigned cols = static_cast<unsigned>(ceil(ImGui::GetWindowSize().x / 200.f));
            ImGui::PushID("info_enable_widget_items");
            ImGui::Columns(static_cast<int>(cols), "info_enable_widgets", false);
            const size_t items_per_col = static_cast<size_t>(ceil(static_cast<float>(widgets.size()) / cols));
            size_t col_count = 0u;
            for (ToolboxUIElement* widget : widgets) {
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
                GW::GameThread::Enqueue([]() {
                    GW::Items::OpenXunlaiWindow();
                });
            }
        }

        if (ImGui::CollapsingHeader("Camera")) {
            GW::Camera* cam = GW::CameraMgr::GetCamera();
            if (cam != nullptr) {
                InfoField("Position##cam_pos", "%.2f, %.2f, %.2f", cam->position.x, cam->position.y, cam->position.z);
                InfoField("Target##cam_target", "%.2f, %.2f, %.2f", cam->look_at_target.x, cam->look_at_target.y, cam->look_at_target.z);
                InfoField("Yaw/Pitch##cam_angle", "%.2f, %.2f", cam->GetCurrentYaw(), cam->pitch);
            }
        }
        if (show_player && ImGui::CollapsingHeader("Player")) {
            ImGui::PushID("player_info");
            DrawAgentInfo(GW::Agents::GetPlayer());
            ImGui::PopID();
        }
        if (show_target && ImGui::CollapsingHeader("Target")) {
            ImGui::PushID("target_info");
            DrawAgentInfo(GW::Agents::GetTarget());
            ImGui::PopID();
        }
        if (show_map && ImGui::CollapsingHeader("Map")) {
            ImGui::PushID("map_info");
            const char* type = "";
            switch (GW::Map::GetInstanceType()) {
            case GW::Constants::InstanceType::Outpost: type = "Outpost\0\0\0"; break;
            case GW::Constants::InstanceType::Explorable: type = "Explorable"; break;
            case GW::Constants::InstanceType::Loading: type = "Loading\0\0\0"; break;
            }
            InfoField("Map ID", "%d", GW::Map::GetMapID());
            ImGui::ShowHelp("Map ID is unique for each area");
            InfoField("Map Region", "%d", GW::Map::GetRegion());
            InfoField("Map District", "%d", GW::Map::GetDistrict());
            InfoField("Map Type", type);
            InfoField("Map file", "%lu", mapfile);
            ImGui::ShowHelp("Map file is unique for each pathing map (e.g. used by minimap).\nMany different maps use the same map file");
            if (ImGui::TreeNodeEx("Advanced", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
                const GW::AreaInfo* map_info = GW::Map::GetCurrentMapInfo();
                if (map_info) {
                    InfoField("Campaign", "%d", map_info->campaign);
                    InfoField("Continent", "%d", map_info->continent);
                    InfoField("Region", "%d", map_info->region);
                    InfoField("Type", "%d", map_info->type);
                    InfoField("Instance Info Type", "%d", GW::Map::GetMapTypeInstanceInfo(map_info->type)->request_instance_map_type);
                    InfoField("Flags", "0x%X", map_info->flags);
                    InfoField("Thumbnail ID", "%d", map_info->thumbnail_id);
                    GW::Vec2f pos = { (float)map_info->x,(float)map_info->y };
                    InfoField("Map Pos", "%.2f, %.2f", pos.x, pos.y);
                    if (!pos.x) {
                        pos.x = (float)(map_info->icon_start_x + (map_info->icon_end_x - map_info->icon_start_x) / 2);
                        pos.y = (float)(map_info->icon_start_y + (map_info->icon_end_y - map_info->icon_start_y) / 2);
                    }
                    if (!pos.x) {
                        pos.x = (float)(map_info->icon_start_x_dupe + (map_info->icon_end_x_dupe - map_info->icon_start_x_dupe) / 2);
                        pos.y = (float)(map_info->icon_start_y_dupe + (map_info->icon_end_y_dupe - map_info->icon_start_y_dupe) / 2);
                    }
                    InfoField("Calculated Pos", "%.2f, %.2f", pos.x,pos.y);
                    static wchar_t name_enc[8];
                    if(GW::UI::UInt32ToEncStr(map_info->name_id,name_enc,8))
                        EncInfoField("Name Enc", name_enc);
                }
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
        if (show_dialog && ImGui::CollapsingHeader("Dialog")) {
            EncInfoField("Dialog Body", DialogModule::Instance().GetDialogBody());
            InfoField("Last Dialog", "0x%X", GW::Agents::GetLastDialogId());
            ImGui::Text("Available NPC Dialogs:");
            ImGui::ShowHelp("Talk to an NPC to see available dialogs");
            const auto& messages = DialogModule::Instance().GetDialogButtonMessages();
            const auto& buttons = DialogModule::Instance().GetDialogButtons();
            char bbuf[48];
            for (size_t i = 0; i < buttons.size();i++) {
                snprintf(bbuf, _countof(bbuf), "send_dialog_%d", i);
                ImGui::PushID(bbuf);
                if (ImGui::Button("Send")) {
                    uint32_t dialog_id = buttons[i]->dialog_id;
                    GW::GameThread::Enqueue([dialog_id]() {
                        GW::Agents::SendDialog(dialog_id);
                        });
                }
                ImGui::SameLine();
                InfoField("Icon", "0x%X", buttons[i]->button_icon);
                EncInfoField("Encoded",messages[i]->encoded().c_str());
                InfoField(messages[i]->string().c_str(), "0x%X", buttons[i]->dialog_id);
                ImGui::PopID();
            }
        }
        if (ImGui::CollapsingHeader("Hovered Skill")) {
            static GuiUtils::EncString skill_name;
            DrawSkillInfo(GW::SkillbarMgr::GetHoveredSkill(), &skill_name, true);
        }
        if (show_item && ImGui::CollapsingHeader("Hovered Item")) {
            static GuiUtils::EncString item_name;
            ImGui::PushID("hovered_item");
            GW::Item* current = GW::Items::GetHoveredItem();
            if (current) {
                last_hovered_item_id = current->item_id;
            }
            DrawItemInfo(GW::Items::GetItemById(last_hovered_item_id), &item_name, true);
            ImGui::PopID();
        }
        if (show_item && ImGui::CollapsingHeader("Item")) {
            ImGui::Text("First item in inventory");
            static GuiUtils::EncString item_name;
            DrawItemInfo(GW::Items::GetItemBySlot(GW::Constants::Bag::Backpack, 1),&item_name);
        }
        #ifdef _DEBUG
        if (show_item && ImGui::CollapsingHeader("Quoted Item")) {
            ImGui::Text("Most recently quoted item (buy or sell) from trader");
            static GuiUtils::EncString quoted_name;
            DrawItemInfo(GW::Items::GetItemById(quoted_item_id),&quoted_name);
        }
        #endif
        if (show_quest && ImGui::CollapsingHeader("Quest")) {
            GW::Quest* q = GW::PlayerMgr::GetActiveQuest();
            if (q) {
                ImGui::Text("ID: 0x%X", q->quest_id);
                ImGui::Text("Marker: (%.0f, %.0f)", q->marker.x, q->marker.y);
            }
        }
        if (show_mobcount && ImGui::CollapsingHeader("Enemy count")) {
            const float sqr_soul_range = 1400.0f * 1400.0f;
            int soul_count = 0;
            int cast_count = 0;
            int spirit_count = 0;
            int compass_count = 0;
            GW::AgentArray* agents = GW::Agents::GetAgentArray();
            GW::Agent* player = GW::Agents::GetPlayer();
            if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Loading
                && agents
                && player != nullptr) {

                for (auto* a : *agents) {
                    GW::AgentLiving* agent = a ? a->GetAsAgentLiving() : nullptr;
                    if (!(agent && agent->allegiance == GW::Constants::Allegiance::Enemy)) continue; // ignore non-hostiles
                    if (agent->GetIsDead()) continue; // ignore dead
                    float sqrd = GW::GetSquareDistance(player->pos, agent->pos);
                    if (agent->player_number == GW::Constants::ModelID::DoA::SoulTormentor
                        || agent->player_number == GW::Constants::ModelID::DoA::VeilSoulTormentor) {
                        if (GW::Map::GetMapID() == GW::Constants::MapID::Domain_of_Anguish
                            && sqrd < sqr_soul_range) {
                            ++soul_count;
                        }
                    }
                    if (sqrd < GW::Constants::SqrRange::Spellcast) ++cast_count;
                    if (sqrd < GW::Constants::SqrRange::Spirit) ++spirit_count;
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
        if (show_resignlog && ImGui::CollapsingHeader("Resign Log")) {
            DrawResignlog();
        }
    }
    ImGui::End();
#ifdef _DEBUG
    // For debugging changes to flags/arrays etc
    GW::GameContext* g = GW::GameContext::instance();
    GW::GuildContext* gu = g->guild;
    GW::CharContext* c = g->character;
    GW::WorldContext* w = g->world;
    GW::PartyContext* p = g->party;
    GW::MapContext* m = g->map;
    GW::ItemContext* i = g->items;
    GW::AgentLiving* me = GW::Agents::GetPlayerAsAgentLiving();
    GW::Player* me_player = me ? GW::PlayerMgr::GetPlayerByID(me->player_number) : nullptr;
    GW::Chat::ChatBuffer* log = GW::Chat::GetChatLog();
    GW::AreaInfo* ai = GW::Map::GetMapInfo(GW::Map::GetMapID());
    (g || c || w || p || m || i || me || me_player || log || gu || ai);
#endif
}

void InfoWindow::Update(float delta) {
    UNREFERENCED_PARAMETER(delta);
    if (!send_queue.empty() && TIMER_DIFF(send_timer) > 600) {
        send_timer = TIMER_INIT();
        if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Loading
            && GW::Agents::GetPlayer()) {

            GW::Chat::SendChat('#', send_queue.front().c_str());
            send_queue.pop();
        }
    }

    if (show_resignlog
        && GW::Map::GetInstanceType() != GW::Constants::InstanceType::Loading
        && GW::PartyMgr::GetPartyInfo()) {
        GW::PlayerPartyMemberArray& partymembers = GW::PartyMgr::GetPartyInfo()->players;
        if (partymembers.valid()) {
            if (partymembers.size() != status.size()) {
                status.resize(partymembers.size(), Unknown);
                timestamp.resize(partymembers.size(), 0);
            }
        }
        for (unsigned i = 0; i < partymembers.size(); ++i) {
            GW::PlayerPartyMember& partymember = partymembers[i];
            if (partymember.connected()) {
                if (status[i] == NotYetConnected || status[i] == Unknown) {
                    status[i] = Connected;
                    timestamp[i] = GW::Map::GetInstanceTime();
                }
            } else {
                if (status[i] == Connected || status[i] == Resigned) {
                    status[i] = Left;
                    timestamp[i] = GW::Map::GetInstanceTime();
                }
            }
        }
    }
}

const char* InfoWindow::GetStatusStr(Status status) {
    switch (status) {
    case InfoWindow::Unknown: return "Unknown";
    case InfoWindow::NotYetConnected: return "Not connected";
    case InfoWindow::Connected: return "Connected";
    case InfoWindow::Resigned: return "Resigned";
    case InfoWindow::Left: return "Left";
    default: return "";
    }
}

void InfoWindow::PrintResignStatus(wchar_t *buffer, size_t size, size_t index, const wchar_t *player_name) {
    if (!player_name) return;
    ASSERT(index < status.size());
    Status player_status = status[index];
    const char* status_str = GetStatusStr(player_status);
    _snwprintf(buffer, size, L"%zu. %s - %S", index + 1, player_name,
        (player_status == Connected
            && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable)
        ? "Connected (not resigned)" : status_str);
}

void InfoWindow::DrawResignlog() {
    if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading) return;
    GW::PartyInfo* info = GW::PartyMgr::GetPartyInfo();
    if (info == nullptr) return;
    GW::PlayerPartyMemberArray& partymembers = info->players;
    if (!partymembers.valid()) return;
    for (size_t i = 0; i < partymembers.size(); ++i) {
        GW::PlayerPartyMember& partymember = partymembers[i];
        wchar_t* player_name = GW::PlayerMgr::GetPlayerName(partymember.login_number);
        if (!player_name) continue;
        ImGui::PushID(static_cast<int>(i));
        if (ImGui::Button("Send")) {
            // Todo: wording probably needs improvement
            wchar_t buf[256];
            PrintResignStatus(buf, 256, i, player_name);
            GW::Chat::SendChat('#', buf);
        }
        ImGui::SameLine();
        const char* status_str = GetStatusStr(status[i]);
        ImGui::Text("%d. %S - %s", i + 1, player_name, status_str);
        if (status[i] != Unknown) {
            ImGui::SameLine();
            ImGui::TextDisabled("[%d:%02d:%02d.%03d]",
                (timestamp[i] / (60 * 60 * 1000)),
                (timestamp[i] / (60 * 1000)) % 60,
                (timestamp[i] / (1000)) % 60,
                (timestamp[i]) % 1000);
        }
        ImGui::PopID();
    }
}

void InfoWindow::DrawSettingInternal() {
    ImGui::Separator();
    ImGui::StartSpacedElements(250.f);
    ImGui::NextSpacedElement(); ImGui::Checkbox("Show widget toggles", &show_widgets);
    ImGui::NextSpacedElement(); ImGui::Checkbox("Show 'Open Xunlai Chest' button", &show_open_chest);
    ImGui::NextSpacedElement(); ImGui::Checkbox("Show Player", &show_player);
    ImGui::NextSpacedElement(); ImGui::Checkbox("Show Target", &show_target);
    ImGui::NextSpacedElement(); ImGui::Checkbox("Show Map", &show_map);
    ImGui::NextSpacedElement(); ImGui::Checkbox("Show Dialog", &show_dialog);
    ImGui::NextSpacedElement(); ImGui::Checkbox("Show Item", &show_item);
    ImGui::NextSpacedElement(); ImGui::Checkbox("Show Quest", &show_quest);
    ImGui::NextSpacedElement(); ImGui::Checkbox("Show Enemy Count", &show_mobcount);
    ImGui::NextSpacedElement(); ImGui::Checkbox("Show Resign Log", &show_resignlog);
}

void InfoWindow::LoadSettings(CSimpleIni* ini) {
    ToolboxWindow::LoadSettings(ini);
    show_menubutton = ini->GetBoolValue(Name(), VAR_NAME(show_menubutton), true);

    show_widgets = ini->GetBoolValue(Name(), VAR_NAME(show_widgets), true);
    show_open_chest = ini->GetBoolValue(Name(), VAR_NAME(show_open_chest), true);
    show_player = ini->GetBoolValue(Name(), VAR_NAME(show_player), true);
    show_target = ini->GetBoolValue(Name(), VAR_NAME(show_target), true);
    show_map = ini->GetBoolValue(Name(), VAR_NAME(show_map), true);
    show_dialog = ini->GetBoolValue(Name(), VAR_NAME(show_dialog), true);
    show_item = ini->GetBoolValue(Name(), VAR_NAME(show_item), true);
    show_quest = ini->GetBoolValue(Name(), VAR_NAME(show_quest), true);
    show_mobcount = ini->GetBoolValue(Name(), VAR_NAME(show_enemycount), true);
    show_resignlog = ini->GetBoolValue(Name(), VAR_NAME(show_resignlog), true);
}

void InfoWindow::SaveSettings(CSimpleIni* ini) {
    ToolboxWindow::SaveSettings(ini);
    ini->SetBoolValue(Name(), VAR_NAME(show_widgets), show_widgets);
    ini->SetBoolValue(Name(), VAR_NAME(show_open_chest), show_open_chest);
    ini->SetBoolValue(Name(), VAR_NAME(show_player), show_player);
    ini->SetBoolValue(Name(), VAR_NAME(show_target), show_target);
    ini->SetBoolValue(Name(), VAR_NAME(show_map), show_map);
    ini->SetBoolValue(Name(), VAR_NAME(show_dialog), show_dialog);
    ini->SetBoolValue(Name(), VAR_NAME(show_item), show_item);
    ini->SetBoolValue(Name(), VAR_NAME(show_quest), show_quest);
    ini->SetBoolValue(Name(), VAR_NAME(show_enemycount), show_mobcount);
    ini->SetBoolValue(Name(), VAR_NAME(show_resignlog), show_resignlog);
}
