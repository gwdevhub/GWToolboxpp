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

#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/WorldContext.h>

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

void InfoWindow::Initialize() {
    ToolboxWindow::Initialize();

    Resources::Instance().LoadTextureAsync(&button_texture, Resources::GetPath(L"img/icons", L"info.png"), IDB_Icon_Info);
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::MessageCore>(&MessageCore_Entry,OnMessageCore);
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::QuotedItemPrice>(&InstanceLoadFile_Entry,
        [this](GW::HookStatus*, GW::Packet::StoC::QuotedItemPrice* packet) -> void {
            quoted_item_id = packet->itemid;
        });
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::DialogSender>(&OnDialog_Entry, [this](...) {
            ClearAvailableDialogs();
        });
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::DialogButton>(&OnDialog_Entry,
        [this](GW::HookStatus*, GW::Packet::StoC::DialogButton* packet) -> void {
            available_dialogs.push_back(new AvailableDialog(packet->message, packet->dialog_id));
        });
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::InstanceLoadFile>(&InstanceLoadFile_Entry,OnInstanceLoad);

    GW::Chat::CreateCommand(L"resignlog", CmdResignLog);
}

void InfoWindow::CmdResignLog(const wchar_t* cmd, int argc, wchar_t** argv) {
    UNREFERENCED_PARAMETER(cmd);
    UNREFERENCED_PARAMETER(argc);
    UNREFERENCED_PARAMETER(argv);
    if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading) return;
    GW::PartyInfo* info = GW::PartyMgr::GetPartyInfo();
    if (info == nullptr) return;
    GW::PlayerPartyMemberArray partymembers = info->players;
    if (!partymembers.valid()) return;
    GW::PlayerArray players = GW::Agents::GetPlayerArray();
    if (!players.valid()) return;
    auto instance = &Instance();
    size_t index_max = std::min<size_t>(instance->status.size(), partymembers.size());
    for (size_t i = 0; i < index_max; ++i) {
        GW::PlayerPartyMember& partymember = partymembers[i];
        if (partymember.login_number >= players.size()) continue;
        GW::Player& player = players[partymember.login_number];

        wchar_t buffer[256];
        if (instance->status[i] == Connected) {
            instance->PrintResignStatus(buffer, 256, i, player.name);
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
    GW::PlayerPartyMemberArray partymembers = info->players;
    if (!partymembers.valid()) return;
    GW::PlayerArray players = GW::Agents::GetPlayerArray();
    if (!players.valid()) return;

    // Prepare the name
    const int offset = 5;
    wchar_t buf[256];
    {
        int i = 0;
        while (i < 255 && pak->message[i + offset] != 0x1 && pak->message[i + offset] != 0) {
            buf[i] = (pak->message[offset + i]);
            ++i;
        }
        buf[i] = '\0';
    }

    // set the right index in party
    auto instance = &Instance();
    for (unsigned i = 0; i < partymembers.size(); ++i) {
        if (i >= instance->status.size()) continue;
        if (instance->status[i] == Resigned) continue;
        if (partymembers[i].login_number >= players.size()) continue;
        if (GuiUtils::SanitizePlayerName(players[partymembers[i].login_number].name) == buf) {
            instance->status[i] = Resigned;
            Instance().timestamp[i] = GW::Map::GetInstanceTime();
            break;
        }
    }
}

void InfoWindow::DrawItemInfo(GW::Item* item, ForDecode* name) {
    if (!item) return;
    name->init(item->single_item_name);
    static char modelid[32] = "";
    static char slot[12] = "";
    static char encname_buf[32] = "";
    static char encdesc_buf[512] = "";
    strcpy_s(modelid, "-");
    strcpy_s(slot, "-");
    strcpy_s(encname_buf, "-");
    strcpy_s(encdesc_buf, "-");
    if (snprintf(modelid, 32, "%d", item->model_id) < 0)
        return;
    if (item->bag) {
        if (snprintf(slot, 12, "%d/%d", item->bag->index + 1, item->slot + 1) < 0)
            return;
    }

    ImGui::PushItemWidth(-80.0f);
    ImGui::LabelText("Bag/Slot", slot, 12, ImGuiInputTextFlags_ReadOnly);
    ImGui::InputText("ModelID", modelid, 32, ImGuiInputTextFlags_ReadOnly);
    ImGui::InputText("Name", name->str(), 64, ImGuiInputTextFlags_ReadOnly);
    //ImGui::InputText("ItemID", itemid, 32, ImGuiInputTextFlags_ReadOnly);
    ImGui::PopItemWidth();
    if (ImGui::TreeNode("Advanced##item")) {
        ImGui::PushItemWidth(ImGui::GetContentRegionAvailWidth() / 2);
        if (item) {
            ImGui::LabelText("Addr", "%p", item);
            ImGui::LabelText("Id", "%d", item->item_id);
            ImGui::LabelText("Type", "%d", item->type);
            ImGui::LabelText("Interaction", "0x%X", item->interaction);
            ImGui::LabelText("model_file_id", "0x%X", item->model_file_id);
            if (item->name_enc) {
                size_t offset = 0;
                for (size_t i = 0; item->name_enc[i]; i++) {
                    offset += sprintf(encname_buf + offset, "0x%X ", item->name_enc[i]);
                }
            }
            ImGui::InputText("Name Enc", item->name_enc ? encname_buf : "-", 32, ImGuiInputTextFlags_ReadOnly);
            if (item->info_string) {
                size_t offset = 0;
                for (size_t i = 0; item->info_string[i]; i++) {
                    offset += sprintf(encdesc_buf + offset, "0x%X ", item->info_string[i]);
                }
            }
            ImGui::InputText("Desc Enc", item->info_string ? encdesc_buf : "-", 512, ImGuiInputTextFlags_ReadOnly);
            if (item->mod_struct_size) {
                ImGui::Text("Mod Struct (identifier, arg1, arg2)");
            }
            char mod_struct_label[] = "###Mod Struct 1";
            char mod_struct_buf[64];
            for (size_t i = 0; i < item->mod_struct_size; i++) {
                GW::ItemModifier* mod = &item->mod_struct[i];
                mod_struct_label[14] = static_cast<char>(i + 1) + '0';
                sprintf(mod_struct_buf, "0x%X (%d %d %d)", mod->mod, mod->identifier(), mod->arg1(), mod->arg2());
                ImGui::InputText(mod_struct_label, mod_struct_buf, 64, ImGuiInputTextFlags_ReadOnly);
            }
        }
        ImGui::PopItemWidth();
        ImGui::TreePop();
    }
}
void InfoWindow::Draw(IDirect3DDevice9* pDevice) {
    UNREFERENCED_PARAMETER(pDevice);
    if (!visible) return;
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_FirstUseEver, ImVec2(.5f, .5f));
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
            std::sort(widgets.begin(), widgets.end(), [](auto *a, auto *b) { return std::strcmp(a->Name(), b->Name()) < 0; });
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
            static char pos_buf[32];
            static char target_buf[32];
            static GW::Camera* cam;
            if ((cam = GW::CameraMgr::GetCamera()) != nullptr) {
                snprintf(pos_buf, 32, "%.2f, %.2f, %.2f", cam->position.x, cam->position.y, cam->position.z);
                snprintf(target_buf, 32, "%.2f, %.2f, %.2f", cam->look_at_target.x, cam->look_at_target.y, cam->look_at_target.z);
            }
            ImGui::PushItemWidth(-80.0f);
            ImGui::InputText("Position##cam_pos", pos_buf, 32, ImGuiInputTextFlags_ReadOnly);
            ImGui::InputText("Target##cam_target", target_buf, 32, ImGuiInputTextFlags_ReadOnly);
            ImGui::PopItemWidth();
        }
        if (show_player && ImGui::CollapsingHeader("Player")) {
            static char x_buf[32] = "";
            static char y_buf[32] = "";
            static char s_buf[32] = "";
            static char agentid_buf[32] = "";
            static char modelid_buf[32] = "";
            GW::AgentLiving* player = GW::Agents::GetPlayerAsAgentLiving();
            if (player) {
                snprintf(x_buf, 32, "%.2f", player->pos.x);
                snprintf(y_buf, 32, "%.2f", player->pos.y);
                float s = sqrtf(player->move_x * player->move_x + player->move_y * player->move_y);
                snprintf(s_buf, 32, "%.3f", s / 288.0f);
                snprintf(agentid_buf, 32, "%d", player->agent_id);
                snprintf(modelid_buf, 32, "%d", player->player_number);
            }
            ImGui::PushItemWidth(-80.0f);
            ImGui::InputText("X pos##player", x_buf, 32, ImGuiInputTextFlags_ReadOnly);
            ImGui::InputText("Y pos##player", y_buf, 32, ImGuiInputTextFlags_ReadOnly);
            ImGui::InputText("Speed##player", s_buf, 32, ImGuiInputTextFlags_ReadOnly);
            ImGui::InputText("Agent ID##player", agentid_buf, 32, ImGuiInputTextFlags_ReadOnly);
            ImGui::ShowHelp("Agent ID is unique for each agent in the instance,\nIt's generated on spawn and will change in different instances.");
            ImGui::InputText("Player ID##player", modelid_buf, 32, ImGuiInputTextFlags_ReadOnly);
            ImGui::ShowHelp("Player ID is unique for each human player in the instance.");
            ImGui::PopItemWidth();
            if (ImGui::TreeNode("Effects##player")) {
                GW::EffectArray effects = GW::Effects::GetPlayerEffectArray();
                if (effects.valid()) {
                    for (DWORD i = 0; i < effects.size(); ++i) {
                        ImGui::Text("id: %d", effects[i].skill_id);
                        uint32_t time = effects[i].GetTimeRemaining();
                        ImGui::SameLine();
                        ImGui::Text(" duration: %u", time / 1000);
                    }
                }
                ImGui::TreePop();
            }
            if (ImGui::TreeNode("Buffs##player")) {
                GW::BuffArray effects = GW::Effects::GetPlayerBuffArray();
                if (effects.valid()) {
                    for (DWORD i = 0; i < effects.size(); ++i) {
                        ImGui::Text("id: %d", effects[i].skill_id);
                        if (effects[i].target_agent_id) {
                            ImGui::SameLine();
                            ImGui::Text(" target: %d", effects[i].target_agent_id);
                        }
                    }
                }
                ImGui::TreePop();
            }

        }
        if (show_target && ImGui::CollapsingHeader("Target")) {
            static char x_buf[32] = "";
            static char y_buf[32] = "";
            static char s_buf[32] = "";
            static char agentid_buf[32] = "";
            static char modelid_buf[32] = "";
            static char encname_buf[64] = "";
            GW::Agent* target = GW::Agents::GetTarget();
            GW::AgentLiving* target_living = target ? target->GetAsAgentLiving() : nullptr;
            GW::AgentItem* target_item = target ? target->GetAsAgentItem() : nullptr;
            GW::AgentGadget* target_gadget = target ? target->GetAsAgentGadget() : nullptr;
            if (target) {
                snprintf(x_buf, 32, "%.2f", target->pos.x);
                snprintf(y_buf, 32, "%.2f", target->pos.y);
                float s = sqrtf(target->move_x * target->move_x + target->move_y * target->move_y);
                snprintf(s_buf, 32, "%.3f", s / 288.0f);
                
                if (target->GetIsItemType())
                    snprintf(modelid_buf, 32, "%d", target_item ? GW::Items::GetItemById(target_item->item_id)->model_id : 0);
                else
                    snprintf(modelid_buf, 32, "%d", target_living ? target_living->player_number : 0);
                snprintf(agentid_buf, 32, "%d", target->agent_id);
                wchar_t* enc_name = GW::Agents::GetAgentEncName(target);
                if (enc_name) {
                    size_t offset = 0;
                    for (size_t i = 0; enc_name[i]; i++) {
                        offset += sprintf(encname_buf + offset, "0x%X ", enc_name[i]);
                    }
                }
            } else {
                snprintf(x_buf, 32, "-");
                snprintf(y_buf, 32, "-");
                snprintf(s_buf, 32, "-");
                snprintf(agentid_buf, 32, "-");
                snprintf(modelid_buf, 32, "-");
                snprintf(encname_buf, 64, "-");
            }
            ImGui::PushItemWidth(-80.0f);
            ImGui::InputText("X pos##target", x_buf, 32, ImGuiInputTextFlags_ReadOnly);
            ImGui::InputText("Y pos##target", y_buf, 32, ImGuiInputTextFlags_ReadOnly);
            ImGui::InputText("Speed##target", s_buf, 32, ImGuiInputTextFlags_ReadOnly);
            ImGui::InputText("Agent ID##target", agentid_buf, 32, ImGuiInputTextFlags_ReadOnly);
            ImGui::ShowHelp("Agent ID is unique for each agent in the instance,\nIt's generated on spawn and will change in different instances.");
            ImGui::InputText("Model ID##target", modelid_buf, 32, ImGuiInputTextFlags_ReadOnly);
            ImGui::ShowHelp("Model ID is unique for each kind of agent.\nIt is static and shared by the same agents.\nWhen targeting players, this is Player ID instead, unique for each player in the instance.\nFor the purpose of targeting hotkeys and commands, use this value");
            ImGui::InputText("Agent Enc Name##target", encname_buf, 64, ImGuiInputTextFlags_ReadOnly);
            ImGui::PopItemWidth();
            GW::Player* player = nullptr;
            GW::Guild* guild = nullptr;
            GW::NPC* npc = nullptr;
            if (target_living && target_living->IsPlayer()) {
                player = GW::PlayerMgr::GetPlayerByID(target_living->player_number);
                if(target_living->transmog_npc_id & 0x20000000)
                    npc = GW::Agents::GetNPCByID(target_living->transmog_npc_id ^ 0x20000000);
            }
            if (target_living && target_living->IsNPC()) {
                npc = GW::Agents::GetNPCByID(target_living->player_number);
            }
            if (target_living && target_living->tags->guild_id) {
                GW::GuildArray guilds = GW::GuildMgr::GetGuildArray();
                if (guilds.valid() && target_living->tags->guild_id < guilds.size())
                    guild = guilds[target_living->tags->guild_id];
            }
            if (target) {
                if (ImGui::TreeNode("Advanced##target")) {
                    GW::Agent *me = GW::Agents::GetPlayer();
                    
                    ImGui::PushItemWidth(ImGui::GetContentRegionAvailWidth() / 2);
                    ImGui::LabelText("Addr", "%p", target);
                    ImGui::LabelText("Id", "%d", target->agent_id);
                    ImGui::LabelText("Z", "%f", target->z);
                    ImGui::LabelText("Width", "%f", target->width1);
                    ImGui::LabelText("Height", "%f", target->height1);
                    ImGui::LabelText("Rotation", "%f", target->rotation_angle);
                    ImGui::LabelText("NameProperties", "0x%X", target->name_properties);
                    ImGui::LabelText("X", "%f", target->pos.x);
                    ImGui::LabelText("Y", "%f", target->pos.y);
                    if (me != nullptr) {
                        ImGui::LabelText("Distance", "%.0f", GW::GetDistance(me->pos, target->pos));
                    }
                    ImGui::LabelText("Plane", "%d", target->plane);
                    ImGui::LabelText("Type", "0x%X", target->type);
                    if (target_item) {
                        ImGui::LabelText("Owner", "%d", target_item->owner);
                        ImGui::LabelText("ItemId", "%d", target_item->item_id);
                        ImGui::LabelText("ExtraType", "%d", target_item->extra_type);
                    }
                    if (target_gadget) {
                        ImGui::LabelText("Gadget ID", "%d", target_gadget->gadget_id);
                        ImGui::LabelText("ExtraType", "%d", target_gadget->extra_type);
                    }
                    if (target_living) {
                        ImGui::LabelText("AS of Weapon", "%f", target_living->weapon_attack_speed);
                        ImGui::LabelText("AS modifier", "%f", target_living->attack_speed_modifier);
                        ImGui::LabelText("PlayerNumber", "%d", target_living->player_number);
                        ImGui::LabelText("Primary Prof", "%d", target_living->primary);
                        ImGui::LabelText("Secondary Prof", "%d", target_living->secondary);
                        ImGui::LabelText("Level", "%d", target_living->level);
                        ImGui::LabelText("TeamId", "%d", target_living->team_id);
                        ImGui::LabelText("Effects", "0x%X", target_living->effects);
                        ImGui::LabelText("ModelState", "0x%X", target_living->model_state);
                        ImGui::LabelText("typeMap", "0x%X", target_living->type_map);
                        ImGui::LabelText("LoginNumber", "%d", target_living->login_number);
                        ImGui::LabelText("Allegiance", "%d", target_living->allegiance);
                        ImGui::LabelText("WeaponType", "%d", target_living->weapon_type);
                        ImGui::LabelText("Skill", "%d", target_living->skill);
                    }
                    if (npc) {
                        ImGui::LabelText("NPC ID", "%d", target_living->player_number);
                        ImGui::LabelText("NPC ModelFileID", "%d", npc->model_file_id);
                        if(npc->files_count)
                            ImGui::LabelText("NPC ModelFile", "%d", npc->model_files[0]);
                        ImGui::LabelText("NPC Flags", "%d", npc->npc_flags);
                    }
                    ImGui::PopItemWidth();
                    ImGui::TreePop();
                }
                if (player) {
                    if (ImGui::TreeNode("Player Info##target")) {
                        ImGui::PushItemWidth(ImGui::GetContentRegionAvailWidth() / 2);
                        ImGui::LabelText("Addr", "%p", player);
                        ImGui::LabelText("Name", "%s", GuiUtils::WStringToString(player->name).c_str());
                        ImGui::PopItemWidth();
                        ImGui::TreePop();
                    }
                }
                if (guild) {
                    if (ImGui::TreeNode("Guild Info##target")) {
                        ImGui::PushItemWidth(ImGui::GetContentRegionAvailWidth() / 2);
                        ImGui::LabelText("Addr", "%p", guild);
                        ImGui::LabelText("Name", "%s [%s]", GuiUtils::WStringToString(guild->name).c_str(), GuiUtils::WStringToString(guild->tag).c_str());
                        ImGui::LabelText("Faction", "%d (%s)", guild->faction_point, guild->faction ? "Luxon" : "Kurzick");
                        if (ImGui::Button("Go to Guild Hall")) {
                            GW::GuildMgr::TravelGH(guild->key);
                        }
                        ImGui::PopItemWidth();
                        ImGui::TreePop();
                    }
                }
            }
        }
        if (show_map && ImGui::CollapsingHeader("Map")) {
            static char id_buf[32] = "";
            char* type = "";
            static char file_buf[32] = "";
            static char region_buf[32] = "";
            static char district_buf[32] = "";
            snprintf(id_buf, 32, "%d", GW::Map::GetMapID());
            switch (GW::Map::GetInstanceType()) {
            case GW::Constants::InstanceType::Outpost: type = "Outpost\0\0\0"; break;
            case GW::Constants::InstanceType::Explorable: type = "Explorable"; break;
            case GW::Constants::InstanceType::Loading: type = "Loading\0\0\0"; break;
            }
            snprintf(file_buf, 32, "%lu", mapfile);
            snprintf(region_buf, 32, "%d", GW::Map::GetRegion());
            snprintf(district_buf, 32, "%d", GW::Map::GetDistrict());
            ImGui::PushItemWidth(-80.0f);
            ImGui::InputText("Map ID", id_buf, 32, ImGuiInputTextFlags_ReadOnly);
            ImGui::ShowHelp("Map ID is unique for each area");
            ImGui::InputText("Map Region", region_buf, 32, ImGuiInputTextFlags_ReadOnly);
            ImGui::InputText("Map District", district_buf, 32, ImGuiInputTextFlags_ReadOnly);
            ImGui::InputText("Map Type", type, 11, ImGuiInputTextFlags_ReadOnly);
            ImGui::InputText("Map file", file_buf, 32, ImGuiInputTextFlags_ReadOnly);
            ImGui::ShowHelp("Map file is unique for each pathing map (e.g. used by minimap).\nMany different maps use the same map file");
            ImGui::PopItemWidth();
        }
        if (show_dialog && ImGui::CollapsingHeader("Dialog")) {
            static char id_buf[32] = "";
            snprintf(id_buf, 32, "0x%X", GW::Agents::GetLastDialogId());
            ImGui::PushItemWidth(-80.0f);
            ImGui::InputText("Last Dialog", id_buf, 32, ImGuiInputTextFlags_ReadOnly);
            ImGui::PopItemWidth();
            ImGui::Text("Available NPC Dialogs:");
            ImGui::ShowHelp("Talk to an NPC to see available dialogs");
            ImGui::PushItemWidth(140.0f * ImGui::GetIO().FontGlobalScale);
            for (auto dialog : available_dialogs) {
                if (dialog->msg_s.empty() && !dialog->msg_ws.empty())
                    dialog->msg_s = GuiUtils::WStringToString(dialog->msg_ws);
                ImGui::InputText(dialog->msg_s.c_str(), dialog->dialog_buf, 11, ImGuiInputTextFlags_ReadOnly);
            }
            ImGui::PopItemWidth();
        }
        if (show_item && ImGui::CollapsingHeader("Item")) {
            ImGui::Text("First item in inventory");
            static ForDecode item_name;
            DrawItemInfo(GW::Items::GetItemBySlot(GW::Constants::Bag::Backpack, 1),&item_name);
        }
        #ifdef _DEBUG
        if (show_item && ImGui::CollapsingHeader("Quoted Item")) {
            ImGui::Text("Most recently quoted item (buy or sell) from trader");
            static ForDecode quoted_name;
            DrawItemInfo(GW::Items::GetItemById(quoted_item_id),&quoted_name);
        }
        #endif
        if (show_quest && ImGui::CollapsingHeader("Quest")) {
            GW::QuestLog qlog = GW::GameContext::instance()->world->quest_log;
            DWORD qid = GW::GameContext::instance()->world->active_quest_id;
            if (qid && qlog.valid()) {
                for (unsigned int i = 0; i < qlog.size(); ++i) {
                    GW::Quest q = qlog[i];
                    if (q.quest_id == qid) {
                        ImGui::Text("ID: 0x%X", q.quest_id);
                        ImGui::Text("Marker: (%.0f, %.0f)", q.marker.x, q.marker.y);
                        break;
                    }
                }
            }
        }
        if (show_mobcount && ImGui::CollapsingHeader("Enemy count")) {
            const float sqr_soul_range = 1400.0f * 1400.0f;
            int soul_count = 0;
            int cast_count = 0;
            int spirit_count = 0;
            int compass_count = 0;
            GW::AgentArray agents = GW::Agents::GetAgentArray();
            GW::Agent* player = GW::Agents::GetPlayer();
            if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Loading
                && agents.valid()
                && player != nullptr) {

                for (unsigned int i = 0; i < agents.size(); ++i) {
                    if (agents[i] == nullptr) continue; // ignore nothings
                    GW::AgentLiving* agent = agents[i]->GetAsAgentLiving();
                    if (agent == nullptr) continue;
                    if (agent->allegiance != 0x3) continue; // ignore non-hostiles
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
        GW::PlayerPartyMemberArray partymembers = GW::PartyMgr::GetPartyInfo()->players;
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
    GW::PlayerPartyMemberArray partymembers = info->players;
    if (!partymembers.valid()) return;
    GW::PlayerArray players = GW::Agents::GetPlayerArray();
    if (!players.valid()) return;
    for (size_t i = 0; i < partymembers.size(); ++i) {
        GW::PlayerPartyMember& partymember = partymembers[i];
        if (partymember.login_number >= players.size()) continue;
        GW::Player& player = players[partymember.login_number];
        ImGui::PushID(static_cast<int>(i));
        if (ImGui::Button("Send")) {
            // Todo: wording probably needs improvement
            wchar_t buf[256];
            PrintResignStatus(buf, 256, i, player.name);
            GW::Chat::SendChat('#', buf);
        }
        ImGui::SameLine();
        const char* status_str = GetStatusStr(status[i]);
        ImGui::Text("%d. %S - %s", i + 1, player.name, status_str);
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
    ImGui::Checkbox("Show widget toggles", &show_widgets);
    ImGui::Checkbox("Show 'Open Xunlai Chest' button", &show_open_chest);
    ImGui::Checkbox("Show Player", &show_player);
    ImGui::Checkbox("Show Target", &show_target);
    ImGui::Checkbox("Show Map", &show_map);
    ImGui::Checkbox("Show Dialog", &show_dialog);
    ImGui::Checkbox("Show Item", &show_item);
    ImGui::Checkbox("Show Quest", &show_quest);
    ImGui::Checkbox("Show Enemy Count", &show_mobcount);
    ImGui::Checkbox("Show Resign Log", &show_resignlog);
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
