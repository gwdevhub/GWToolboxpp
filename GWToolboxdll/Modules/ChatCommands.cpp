#include "stdafx.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/Constants/Maps.h>

#include <GWCA/GameContainers/Array.h>
#include <GWCA/GameContainers/GamePos.h>

#include <GWCA/GameEntities/Map.h>
#include <GWCA/GameEntities/NPC.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Guild.h>
#include <GWCA/GameEntities/Skill.h>
#include <GWCA/GameEntities/Player.h>

#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/WorldContext.h>
#include <GWCA/Context/GuildContext.h>

#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/ItemMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/GuildMgr.h>
#include <GWCA/Managers/CameraMgr.h>
#include <GWCA/Managers/MemoryMgr.h>
#include <GWCA/Managers/PlayerMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Managers/FriendListMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/CtoSMgr.h>

#include <GuiUtils.h>
#include <GWToolbox.h>
#include <Keys.h>
#include <Logger.h>

#include <Modules/ChatCommands.h>
#include <Modules/GameSettings.h>
#include <Widgets/PartyDamage.h>
#include <Windows/BuildsWindow.h>
#include <Windows/Hotkeys.h>
#include <Windows/MainWindow.h>
#include <Windows/SettingsWindow.h>


namespace {
    const wchar_t *next_word(const wchar_t *str) {
        while (*str && !isspace(*str))
            str++;
        while (*str && isspace(*str))
            str++;
        return *str ? str : NULL;
    }

    static bool IsMapReady()
    {
        return GW::Map::GetInstanceType() != GW::Constants::InstanceType::Loading && !GW::Map::GetIsObserving()  && GW::MemoryMgr::GetGWWindowHandle() == GetActiveWindow();
    }

    static void TargetNearest(uint32_t model_id = 0, uint32_t type = 0xDB)
    {
        // target nearest agent
        GW::AgentArray agents = GW::Agents::GetAgentArray();
        if (!agents.valid())
            return;

        GW::AgentLiving *me = GW::Agents::GetPlayerAsAgentLiving();
        if (me == nullptr)
            return;

        float distance = GW::Constants::SqrRange::Compass;
        size_t closest = (size_t)-1;

        for (size_t i = 0, size = agents.size(); i < size; ++i) {
            if (agents[i] == nullptr || agents[i] == me)
                continue;
            if (agents[i]->type != type)
                continue;
            if (model_id) {
                if (type == 0x200) {
                    // Target gadget by gadget id
                    GW::AgentGadget *gadget = agents[i]->GetAsAgentGadget();
                    if (!gadget || gadget->gadget_id != model_id)
                        continue;
                } else if (type == 0x400) {
                    // Target item by model id
                    GW::AgentItem *item_agent = agents[i]->GetAsAgentItem();
                    if (!item_agent)
                        continue;
                    GW::Item *item = GW::Items::GetItemById(item_agent->item_id);
                    if (!item || item->model_id != model_id)
                        continue;
                } else {
                    // Target agent by model id
                    GW::AgentLiving *living_agent = agents[i]->GetAsAgentLiving();
                    if (!living_agent || living_agent->player_number != model_id)
                        continue;
                }
            }
            float newDistance =
                GW::GetSquareDistance(me->pos, agents[i]->pos);
            if (newDistance < distance) {
                closest = i;
                distance = newDistance;
            }
        }
        if (closest != (size_t)-1) {
            GW::Agents::ChangeTarget(agents[closest]);
        }
    }
    bool ImInPresearing() { return GW::Map::GetCurrentMapInfo()->region == GW::Region_Presearing; }

    static std::map<std::string, ChatCommands::PendingTransmo> npc_transmos;
} // namespace

void ChatCommands::TransmoAgent(DWORD agent_id, PendingTransmo& transmo)
{
    if (!transmo.npc_id || !agent_id)
        return;
    GW::AgentLiving *a = static_cast<GW::AgentLiving *>(GW::Agents::GetAgentByID(agent_id));
    if (!a || !a->GetIsLivingType())
        return;
    DWORD &npc_id = transmo.npc_id;
    DWORD &scale = transmo.scale;
    GW::NPCArray &npcs = GW::GameContext::instance()->world->npcs;
    if (npc_id == INT_MAX - 1) {
        // Scale only
        npc_id = a->player_number;
        if (a->transmog_npc_id & 0x20000000)
            npc_id = a->transmog_npc_id ^ 0x20000000;
    } else if (npc_id == INT_MAX) {
        // Reset
        npc_id = 0;
        scale = 0x64000000;
    } else if (npc_id >= npcs.size() || !npcs[npc_id].model_file_id) {
        DWORD &npc_model_file_id = transmo.npc_model_file_id;
        DWORD &npc_model_file_data = transmo.npc_model_file_data;
        DWORD &flags = transmo.flags;
        if (!npc_model_file_id)
            return;
        // Need to create the NPC.
        // Those 2 packets (P074 & P075) are used to create a new model, for instance if we want to "use" a tonic.
        // We have to find the data that are in the NPC structure and feed them to those 2 packets.
        GW::NPC npc = {0};
        npc.model_file_id = npc_model_file_id;
        npc.npc_flags = flags;
        npc.primary = 1;
        npc.scale = scale;
        npc.default_level = 0;
        GW::GameThread::Enqueue([npc_id, npc]() {
            GW::Packet::StoC::NpcGeneralStats packet;
            packet.npc_id = npc_id;
            packet.file_id = npc.model_file_id;
            packet.data1 = 0;
            packet.scale = npc.scale;
            packet.data2 = 0;
            packet.flags = npc.npc_flags;
            packet.profession = npc.primary;
            packet.level = npc.default_level;
            packet.name[0] = 0;
            GW::StoC::EmulatePacket(&packet);
        });
        if (npc_model_file_data) {
            GW::GameThread::Enqueue([npc_id, npc_model_file_data]() {
                GW::Packet::StoC::NPCModelFile packet;
                packet.npc_id = npc_id;
                packet.count = 1;
                packet.data[0] = npc_model_file_data;

                GW::StoC::EmulatePacket(&packet);
            });
        }
    }
    GW::GameThread::Enqueue([npc_id, agent_id, scale]() {
        if (npc_id) {
            GW::NPCArray &npcs = GW::GameContext::instance()->world->npcs;
            GW::NPC npc = npcs[npc_id];
            if (!npc.model_file_id)
                return;
        }
        GW::Packet::StoC::AgentScale packet1;
        packet1.header = GW::Packet::StoC::AgentScale::STATIC_HEADER;
        packet1.agent_id = agent_id;
        packet1.scale = scale;
        GW::StoC::EmulatePacket(&packet1);

        GW::Packet::StoC::AgentModel packet2;
        packet2.header = GW::Packet::StoC::AgentModel::STATIC_HEADER;
        packet2.agent_id = agent_id;
        packet2.model_id = npc_id;
        GW::StoC::EmulatePacket(&packet2);
    });
}

bool ChatCommands::GetNPCInfoByName(const std::string name, PendingTransmo& transmo)
{
    for (auto& npc_transmo : npc_transmos) {
        size_t found_len = npc_transmo.first.find(name);
        if (found_len == std::string::npos)
            continue;
        transmo = npc_transmo.second;
        return true;
    }
    return false;
}
bool ChatCommands::GetNPCInfoByName(const std::wstring name, PendingTransmo &transmo)
{
    return GetNPCInfoByName(GuiUtils::WStringToString(name), transmo);
}

void ChatCommands::DrawHelp() {
    if (!ImGui::TreeNode("Chat Commands"))
        return;
    ImGui::Text("You can create a 'Send Chat' hotkey to perform any command.");
    ImGui::TextDisabled("Below, <xyz> denotes an argument, use an appropriate value without the quotes.\n"
        "(a|b) denotes a mandatory argument, in this case 'a' or 'b'.\n"
        "[a|b] denotes an optional argument, in this case nothing, 'a' or 'b'.");

    ImGui::Bullet(); ImGui::Text("'/age2' prints the instance time to chat.");
    ImGui::Bullet(); ImGui::Text("'/borderless [on|off]' toggles, enables or disables borderless window.");
    ImGui::Bullet(); ImGui::Text("'/camera (lock|unlock)' to lock or unlock the camera.");
    ImGui::Bullet(); ImGui::Text("'/camera fog (on|off)' sets game fog effect on or off.");
    ImGui::Bullet(); ImGui::Text("'/camera fov <value>' sets the field of view. '/camera fov' resets to default.");
    ImGui::Bullet(); ImGui::Text("'/camera speed <value>' sets the unlocked camera speed.");
    ImGui::Bullet(); ImGui::Text("'/chest' opens xunlai in outposts and locked chests in explorables.");
    ImGui::Bullet(); ImGui::Text("'/damage' or '/dmg' to print party damage to chat.\n"
        "'/damage me' sends your own damage only.\n"
        "'/damage <number>' sends the damage of a party member (e.g. '/damage 3').\n"
        "'/damage reset' resets the damage in party window.");
    ImGui::Bullet(); ImGui::Text("'/dialog <id>' sends a dialog.");
    ImGui::Bullet(); ImGui::Text("'/flag [all|clear|<number>]' to flag a hero in the minimap (same as using the buttons by the minimap).");
    ImGui::Bullet(); ImGui::Text("'/flag [all|<number>] [x] [y]' to flag a hero to coordinates [x],[y].");
    ImGui::Bullet(); ImGui::Text("'/flag <number> clear' to clear flag for a hero.");
    ImGui::Bullet(); ImGui::Text("'/hide <name>' closes the window or widget titled <name>.");
    ImGui::Bullet(); ImGui::Text("'/load [build template|build name] [Hero index]' loads a build. The build name must be between quotes if it contains spaces. First Hero index is 1, last is 7. Leave out for player");
    ImGui::Bullet(); ImGui::Text("'/pcons [on|off]' toggles, enables or disables pcons.");
    ImGui::Bullet(); ImGui::Text("'/show <name>' opens the window or widget titled <name>.");
    ImGui::Bullet(); ImGui::Text("'/target closest' to target the closest agent to you.\n"
        "'/target ee' to target best ebon escape agent.\n"
        "'/target hos' to target best vipers/hos agent.");
    ImGui::Bullet(); ImGui::Text("'/tb <name>' toggles the window or widget titled <name>.");
    ImGui::Bullet(); ImGui::Text("'/tb reset' moves Toolbox and Settings window to the top-left corner.");
    ImGui::Bullet(); ImGui::Text("'/tb quit' or '/tb exit' completely closes toolbox and all its windows.");
    ImGui::Bullet(); ImGui::Text("'/travel <town> [dis]', '/tp <town> [dis]' or '/to <town> [dis]' to travel to a destination. \n"
        "<town> can be any of: doa, kamadan/kama, embark, vlox, gadds, urgoz, deep, gtob, fav1, fav2, fav3.\n"
        "[dis] can be any of: ae, ae1, ee, eg, int, etc");
    ImGui::Bullet(); ImGui::Text("'/useskill <skill>' starts using the skill on recharge. "
        "Use the skill number instead of <skill> (e.g. '/useskill 5'). "
        "It's possible to use more than one skill on recharge. "
        "Use empty '/useskill' or '/useskill stop' to stop all. "
        "Use '/useskill <skill>' to stop the skill.");
    ImGui::Bullet(); ImGui::Text("'/zoom <value>' to change the maximum zoom to the value. "
        "use empty '/zoom' to reset to the default value of 750.");
    const char* transmo_hint = "<npc_name> options: eye, zhu, kuunavang, beetle, polar, celepig, \n"
        "  destroyer, koss, bonedragon, smite, kanaxai, skeletonic, moa";
    ImGui::Bullet(); ImGui::Text("'/transmo <npc_name> [size (6-255)]' to change your appearance into an NPC.\n"
        "'/transmo' to change your appearance into target NPC.\n"
        "'/transmo reset' to reset your appearance.");
    ImGui::ShowHelp(transmo_hint);
    ImGui::Bullet(); ImGui::Text("'/transmotarget <npc_name> [size (6-255)]' to change your target's appearance into an NPC.\n"
        "'/transmotarget reset' to reset your target's appearance.");
    ImGui::ShowHelp(transmo_hint);
    ImGui::Bullet(); ImGui::Text("'/transmoparty <npc_name> [size (6-255)]' to change your party's appearance into an NPC.\n"
        "'/transmoparty' to change your party's appearance into target NPC.\n"
        "'/transmoparty reset' to reset your party's appearance.");
    ImGui::ShowHelp(transmo_hint);
    ImGui::Bullet(); ImGui::Text("'/pingitem <equipped_item>' to ping your equipment in chat.\n"
        "<equipped_item> options: armor, head, chest, legs, boots, gloves, offhand, weapon, weapons, costume");
    ImGui::TreePop();
}

void ChatCommands::DrawSettingInternal() {
    ImGui::Text("'/cam unlock' options");
    ImGui::Indent();
    ImGui::Checkbox("Fix height when moving forward", &forward_fix_z);
    ImGui::InputFloat("Camera speed", &cam_speed, 0.0f, 0.0f, 3);
    ImGui::Unindent();
}

void ChatCommands::LoadSettings(CSimpleIni* ini) {
    forward_fix_z = ini->GetBoolValue(Name(), VAR_NAME(forward_fix_z), forward_fix_z);
    cam_speed = (float)ini->GetDoubleValue(Name(), VAR_NAME(cam_speed), DEFAULT_CAM_SPEED);
}

void ChatCommands::SaveSettings(CSimpleIni* ini) {
    ini->SetBoolValue(Name(), VAR_NAME(forward_fix_z), forward_fix_z);
    ini->SetDoubleValue(Name(), VAR_NAME(cam_speed), cam_speed);
}

void ChatCommands::Initialize() {
    ToolboxModule::Initialize();
    const DWORD def_scale = 0x64000000;
    // Available Transmo NPCs
    // @Enhancement: Ability to target an NPC in-game and add it to this list via a GUI
    npc_transmos = { 
        {"charr", {163, def_scale, 0x0004c409, 0, 98820}}, 
        {"reindeer", {5, def_scale, 277573, 277576, 32780}}, 
        {"gwenpre", {244, def_scale, 116377, 116759, 98820}}, 
        {"gwenchan", {245, def_scale, 116377, 283392, 98820}}, 
        {"eye", {0x1f4, def_scale, 0x9d07, 0, 0}},
        {"zhu", {298, def_scale, 170283, 170481, 98820}}, 
        {"kuunavang", {309, def_scale, 157438, 157527, 98820}}, 
        {"beetle", {329, def_scale, 207331, 279211, 98820}},     
        {"polar", {313, def_scale, 277551, 277556, 98820}},    
        {"celepig", {331, def_scale, 279205, 0, 0}},  
        {"mallyx", {315, def_scale, 243812, 0, 98820}},     
        {"bonedragon", {231, def_scale, 16768, 0, 0}},     
        {"destroyer", {312, def_scale, 285891, 285900, 98820}},   
        {"destroyer2", {146, def_scale, 285886, 285890, 32780}},    
        {"koss", {250, def_scale, 243282, 245053, 98820}},     
        {"smite", {346, def_scale, 129664, 0, 98820}}, 
        {"dorian", {8299, def_scale, 86510, 0, 98820}},         
        {"kanaxai", {317, def_scale, 184176, 185319, 98820}},         
        {"skeletonic", {359, def_scale, 52356, 0, 98820}},          
        {"moa", {504, def_scale, 16689, 0, 98820}}
    };

    // you can create commands here in-line with a lambda, but only if they are only 
    // a couple of lines and not used multiple times
    GW::Chat::CreateCommand(L"ff", [](const wchar_t*, int, LPWSTR*) {
        GW::Chat::SendChat('/', "resign");
    });
    GW::Chat::CreateCommand(L"gh", [](const wchar_t*, int, LPWSTR*) {
        GW::Chat::SendChat('/', "tp gh");
    });
    GW::Chat::CreateCommand(L"enter", [](const wchar_t*, int argc, LPWSTR* argv) -> void {
        if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Outpost) return;
        uint32_t item_id;
        std::wstring arg;
        switch (GW::Map::GetMapID()) {
        case GW::Constants::MapID::Temple_of_the_Ages:
        case GW::Constants::MapID::Embark_Beach:
            if (argc < 2)
                return Log::Warning("Use /enter fow or /enter uw to trigger entry");
            arg = GuiUtils::ToLower(argv[1]);
            if (arg == L"fow")
                item_id = 22280;
            else if (arg == L"uw")
                item_id = 3746;
            else
                return Log::Warning("Use /enter fow or /enter uw to trigger entry");
            if (!GW::Items::UseItemByModelId(item_id, 1, 4) && !GW::Items::UseItemByModelId(item_id, 8, 16))
                return Log::Error("Scroll not found!");
            break;
        default:
            return Log::Warning("Use /enter from Temple of the Ages or Embark Beach");
        }
    });
    GW::Chat::CreateCommand(L"age2", ChatCommands::CmdAge2);
    GW::Chat::CreateCommand(L"dialog", ChatCommands::CmdDialog);
    GW::Chat::CreateCommand(L"show", ChatCommands::CmdShow);
    GW::Chat::CreateCommand(L"hide", ChatCommands::CmdHide);
    GW::Chat::CreateCommand(L"tb", ChatCommands::CmdTB);
    GW::Chat::CreateCommand(L"zoom", ChatCommands::CmdZoom);
    GW::Chat::CreateCommand(L"camera", ChatCommands::CmdCamera);
    GW::Chat::CreateCommand(L"cam", ChatCommands::CmdCamera);
    GW::Chat::CreateCommand(L"damage", ChatCommands::CmdDamage);
    GW::Chat::CreateCommand(L"dmg", ChatCommands::CmdDamage);
    GW::Chat::CreateCommand(L"chest", ChatCommands::CmdChest);
    GW::Chat::CreateCommand(L"xunlai", ChatCommands::CmdChest);
    GW::Chat::CreateCommand(L"afk", ChatCommands::CmdAfk);
    GW::Chat::CreateCommand(L"target", ChatCommands::CmdTarget);
    GW::Chat::CreateCommand(L"tgt", ChatCommands::CmdTarget);
    GW::Chat::CreateCommand(L"useskill", ChatCommands::CmdUseSkill);
    GW::Chat::CreateCommand(L"scwiki", ChatCommands::CmdSCWiki);
    GW::Chat::CreateCommand(L"load", ChatCommands::CmdLoad);
    GW::Chat::CreateCommand(L"transmo", ChatCommands::CmdTransmo);
    GW::Chat::CreateCommand(L"transmotarget", ChatCommands::CmdTransmoTarget);
    GW::Chat::CreateCommand(L"transmoparty", ChatCommands::CmdTransmoParty);
    GW::Chat::CreateCommand(L"transmoagent", ChatCommands::CmdTransmoAgent);
    GW::Chat::CreateCommand(L"resize", ChatCommands::CmdResize);
    GW::Chat::CreateCommand(L"settitle", ChatCommands::CmdReapplyTitle);
    GW::Chat::CreateCommand(L"title", ChatCommands::CmdReapplyTitle);
    GW::Chat::CreateCommand(L"pingitem", ChatCommands::CmdPingEquipment);
    GW::Chat::CreateCommand(L"armor", [](const wchar_t*, int, LPWSTR*) -> void {
        GW::Chat::SendChat('/', "pingitem armor");
    });
    GW::Chat::CreateCommand(L"hero", ChatCommands::CmdHeroBehaviour);
}

bool ChatCommands::WndProc(UINT Message, WPARAM wParam, LPARAM lParam) {
    UNREFERENCED_PARAMETER(lParam);
    if (!GW::CameraMgr::GetCameraUnlock()) return false;
    if (GW::Chat::GetIsTyping()) return false;
    if (ImGui::GetIO().WantTextInput) return false;

    switch (Message) {
        case WM_KEYDOWN:
        case WM_KEYUP:
            switch (wParam) {
                case VK_A:
                case VK_D:
                case VK_E:
                case VK_Q:
                case VK_R:
                case VK_S:
                case VK_W:
                case VK_X:
                case VK_Z:
                
                case VK_ESCAPE:
                case VK_UP:
                case VK_DOWN:
                case VK_LEFT:
                case VK_RIGHT:
                    return true;
            }
    }
    return false;
}

void ChatCommands::Update(float delta) {
    static bool keep_forward;

    if (delta == 0.f) return;

    if (GW::CameraMgr::GetCameraUnlock() 
        && !GW::Chat::GetIsTyping() 
        && !ImGui::GetIO().WantTextInput) {

        float forward = 0;
        float vertical = 0;
        float rotate = 0;
        float side = 0;
        if (ImGui::IsKeyDown(VK_W) || ImGui::IsKeyDown(VK_UP) || keep_forward) forward += 1.0f;
        if (ImGui::IsKeyDown(VK_S) || ImGui::IsKeyDown(VK_DOWN)) forward -= 1.0f;
        if (ImGui::IsKeyDown(VK_Q)) side += 1.0f;
        if (ImGui::IsKeyDown(VK_E)) side -= 1.0f;
        if (ImGui::IsKeyDown(VK_Z)) vertical -= 1.0f;
        if (ImGui::IsKeyDown(VK_X)) vertical += 1.0f;
        if (ImGui::IsKeyDown(VK_A) || ImGui::IsKeyDown(VK_LEFT)) rotate += 1.0f;
        if (ImGui::IsKeyDown(VK_D) || ImGui::IsKeyDown(VK_RIGHT)) rotate -= 1.0f;
        if (ImGui::IsKeyDown(VK_R)) keep_forward = true;

        if (ImGui::IsKeyDown(VK_W) || ImGui::IsKeyDown(VK_UP) ||
            ImGui::IsKeyDown(VK_S) || ImGui::IsKeyDown(VK_DOWN) ||
            ImGui::IsKeyDown(VK_ESCAPE))
        {
            keep_forward = false;
        }

        if (GWToolbox::Instance().right_mouse_down && (rotate != 0.f)) {
            side = rotate;
            rotate = 0.f;
        }

        GW::CameraMgr::ForwardMovement(forward * delta * cam_speed, !forward_fix_z);
        GW::CameraMgr::VerticalMovement(vertical * delta * cam_speed);
        GW::CameraMgr::RotateMovement(rotate * delta * ROTATION_SPEED);
        GW::CameraMgr::SideMovement(side * delta * cam_speed);
        GW::CameraMgr::UpdateCameraPos();
    }

    for (uint32_t slot : skills_to_use) {
        if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable 
            && !GW::Map::GetIsObserving()
            && (clock() - skill_timer) / 1000.0f > skill_usage_delay) {
            GW::Skillbar* skillbar = GW::SkillbarMgr::GetPlayerSkillbar();
            if (skillbar && skillbar->IsValid()) {
                const GW::SkillbarSkill& skill = skillbar->skills[slot];
                if (!skill.skill_id)
                    continue;
                const GW::Skill& skilldata = GW::SkillbarMgr::GetSkillConstantData(skill.skill_id);
                if ((skilldata.adrenaline == 0 && skill.GetRecharge() == 0) || (skilldata.adrenaline > 0 && skill.adrenaline_a == skilldata.adrenaline)) {
                    GW::GameThread::Enqueue([slot] {
                        GW::SkillbarMgr::UseSkill(slot, GW::Agents::GetTargetId());
                    });

                    skill_usage_delay = std::max(skilldata.activation + skilldata.aftercast, 0.25f); // a small flat delay of .3s for ping and to avoid spamming in case of bad target
                    skill_timer = clock();
                }
            }
        }
    }
}

bool ChatCommands::ReadTemplateFile(std::wstring path, char *buff, size_t buffSize) {
    HANDLE fileHandle = CreateFileW(path.c_str(), GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (fileHandle == INVALID_HANDLE_VALUE) {
        // We don't print that error, because we can /load [template]
        // Log::Error("Failed openning file '%S'", path.c_str());
        return false;
    }

    DWORD fileSize = GetFileSize(fileHandle, NULL);
    if (fileSize >= buffSize) {
        Log::Error("Buffer size too small, file size is %d", fileSize);
        CloseHandle(fileHandle);
        return false;
    }

    DWORD bytesReaded; // @Remark, necessary !!!!! failed on some Windows 7.
    if (ReadFile(fileHandle, buff, fileSize, &bytesReaded, NULL) == FALSE) {
        Log::Error("ReadFile failed ! (%u)", GetLastError());
        CloseHandle(fileHandle);
        return false;
    }

    buff[fileSize] = 0;
    CloseHandle(fileHandle);
    return true;
}

void ChatCommands::CmdAge2(const wchar_t* , int, LPWSTR* ) {
    DWORD second = GW::Map::GetInstanceTime() / 1000;
    Log::Info("%02u:%02u:%02u", (second / 3600), (second / 60) % 60, second % 60);
}

void ChatCommands::CmdDialog(const wchar_t *, int argc, LPWSTR *argv) {
    if (!IsMapReady())
        return;
    if (argc <= 1) {
        Log::Error("Please provide an integer or hex argument");
    } else {
        uint32_t id=0;
        if (GuiUtils::ParseUInt(argv[1], &id)) {
            GW::Agents::SendDialog(id);
            Log::Info("Sent Dialog 0x%X", id);
        } else {
            Log::Error("Invalid argument '%ls', please use an integer or hex value", argv[0]);
        }
    }
}

void ChatCommands::CmdChest(const wchar_t *, int, LPWSTR *) {
    if (!IsMapReady())
        return;
    switch (GW::Map::GetInstanceType()) {
    case GW::Constants::InstanceType::Outpost:
        GW::Items::OpenXunlaiWindow();
        break;
    case GW::Constants::InstanceType::Explorable: {
        GW::Agent* target = GW::Agents::GetTarget();
        if (target && target->type == 0x200) {
            GW::Agents::GoSignpost(target);
            GW::Items::OpenLockedChest();
        }
    }
        break;
    default:
        break;
    }
}

void ChatCommands::CmdTB(const wchar_t *message, int argc, LPWSTR *argv) {
    if (argc <= 1) {
        MainWindow::Instance().visible ^= 1;
    } else {
        std::wstring arg = GuiUtils::ToLower(argv[1]);
        if (arg == L"age") {
            CmdAge2(message, 0, argv);
        } else if (arg == L"hide") {
            MainWindow::Instance().visible = false;
        } else if (arg == L"show") {
            MainWindow::Instance().visible = true;
        } else if (arg == L"reset") {
            ImGui::SetWindowPos(MainWindow::Instance().Name(), ImVec2(50.0f, 50.0f));
            ImGui::SetWindowPos(SettingsWindow::Instance().Name(), ImVec2(50.0f, 50.0f));
            MainWindow::Instance().visible = false;
            SettingsWindow::Instance().visible = true;
        } else if (arg == L"settings") {
            SettingsWindow::Instance().visible = true;
        } else if (arg == L"mini" || arg == L"minimize") {
            ImGui::SetWindowCollapsed(MainWindow::Instance().Name(), true);
        } else if (arg == L"maxi" || arg == L"maximize") {
            ImGui::SetWindowCollapsed(MainWindow::Instance().Name(), false);
        } else if (arg == L"close" || arg == L"quit" || arg == L"exit") {
            GWToolbox::Instance().StartSelfDestruct();
        } else if (arg == L"ignore") {

        } else {
            auto windows = MatchingWindows(message, argc, argv);
            for (ToolboxUIElement* window : windows) {
                window->visible ^= 1;
            }
        }
    }
}

std::vector<ToolboxUIElement*> ChatCommands::MatchingWindows(const wchar_t *message, int argc, LPWSTR *) {
    std::vector<ToolboxUIElement*> ret;
    if (argc <= 1) {
        ret.push_back(&MainWindow::Instance());
    } else {
        const wchar_t *tail = next_word(message);
        std::wstring arg = GuiUtils::ToLower(tail);
        if (arg == L"all") {
            for (ToolboxUIElement* window : GWToolbox::Instance().GetUIElements()) {
                ret.push_back(window);
            }
        } else if(arg.size()) {
            std::string name = GuiUtils::WStringToString(arg);
            for (ToolboxUIElement* window : GWToolbox::Instance().GetUIElements()) {
                if (GuiUtils::ToLower(window->Name()).find(name) == 0){
                    ret.push_back(window);
                }
            }
        }
    }
    return ret;
}

void ChatCommands::CmdShow(const wchar_t *message, int argc, LPWSTR *argv) {
    std::vector<ToolboxUIElement *> windows = MatchingWindows(message, argc, argv);
    if (windows.empty()) {
        if (argc == 2 && !wcscmp(argv[1], L"settings")) {
            SettingsWindow::Instance().visible = true;
        } else {
            Log::Error("Cannot find window '%ls'", argc > 1 ? argv[1] : L"");
        }
    } else {
        for (ToolboxUIElement* window : windows) {
            window->visible = true;
        }
    }
}

void ChatCommands::CmdHide(const wchar_t *message, int argc, LPWSTR *argv) {
    std::vector<ToolboxUIElement *> windows = MatchingWindows(message, argc, argv);
    if (windows.empty()) {
        Log::Error("Cannot find window '%ls'", argc > 1 ? argv[1] : L"");
    } else {
        for (ToolboxUIElement* window : windows) {
            window->visible = false;
        }
    }
}

void ChatCommands::CmdZoom(const wchar_t *message, int argc, LPWSTR *argv) {
    UNREFERENCED_PARAMETER(message);
    if (argc <= 1) {
        GW::CameraMgr::SetMaxDist();
    } else {
        int distance;
        if (GuiUtils::ParseInt(argv[1], &distance)) {
            if (distance > 0) {
                GW::CameraMgr::SetMaxDist(static_cast<float>(distance));
            } else {
                Log::Error("Invalid argument '%ls', please use a positive integer value", argv[1]);
            }
        } else {
            Log::Error("Invalid argument '%ls', please use an integer value", argv[1]);
        }
    }
}

void ChatCommands::CmdCamera(const wchar_t *message, int argc, LPWSTR *argv) {
    UNREFERENCED_PARAMETER(message);
    if (argc == 1) {
        GW::CameraMgr::UnlockCam(false);
    } else {
        std::wstring arg1 = GuiUtils::ToLower(argv[1]);
        if (arg1 == L"lock") {
            GW::CameraMgr::UnlockCam(false);
        } else if (arg1 == L"unlock") {
            GW::CameraMgr::UnlockCam(true);
            Log::Info("Use Q/E, A/D, W/S, X/Z, R and arrows for camera movement");
        } else if (arg1 == L"fog") {
            if (argc == 3) {
                std::wstring arg2 = GuiUtils::ToLower(argv[2]);
                if (arg2 == L"on") {
                    GW::CameraMgr::SetFog(true);
                } else if (arg2 == L"off") {
                    GW::CameraMgr::SetFog(false);
                }
            }
        } else if (arg1 == L"speed") {
            if (argc < 3) {
                Instance().cam_speed = Instance().DEFAULT_CAM_SPEED;
            } else {
                std::wstring arg2 = GuiUtils::ToLower(argv[2]);
                if (arg2 == L"default") {
                    Instance().cam_speed = Instance().DEFAULT_CAM_SPEED;
                } else {
                    float speed = 0.0f;
                    if (!GuiUtils::ParseFloat(arg2.c_str(), &speed)) {
                        Log::Error(
                            "Invalid argument '%ls', please use a float value",
                            argv[2]);
                        return;
                    }
                    Instance().cam_speed = speed;
                    Log::Info("Camera speed is now %f", speed);
                }
            }
        } else {
            Log::Error("Invalid argument.");
        }
    }
}

void ChatCommands::CmdDamage(const wchar_t *message, int argc, LPWSTR *argv) {
    UNREFERENCED_PARAMETER(message);
    if (argc <= 1) {
        PartyDamage::Instance().WritePartyDamage();
    } else {
        std::wstring arg1 = GuiUtils::ToLower(argv[1]);
        if (arg1 == L"print" || arg1 == L"report") {
            PartyDamage::Instance().WritePartyDamage();
        } else if (arg1 == L"me") {
            PartyDamage::Instance().WriteOwnDamage();
        } else if (arg1 == L"reset") {
            PartyDamage::Instance().ResetDamage();
        } else {
            int idx;
            if (GuiUtils::ParseInt(argv[1], &idx)) {
                PartyDamage::Instance().WriteDamageOf(static_cast<uint32_t>(idx) - 1);
            }
        }
    }
}

void ChatCommands::CmdAfk(const wchar_t *message, int argc, LPWSTR *argv) {
    UNREFERENCED_PARAMETER(argv);
    GW::FriendListMgr::SetFriendListStatus(GW::Constants::OnlineStatus::AWAY);
    GameSettings& settings = GameSettings::Instance();
    if (argc > 1) {
        const wchar_t *afk_msg = next_word(message);
        settings.SetAfkMessage(afk_msg);
    } else {
        settings.afk_message.clear();
    }
}

void ChatCommands::CmdTarget(const wchar_t *message, int argc, LPWSTR *argv) {
    if (argc < 2)
        return Log::Error("Missing argument for /target");
    static auto CalcAngle = [](GW::GamePos pos) {
        const float pi = 3.14159f;
        float tan_angle = 0.0f;
        if (pos.x == 0.0f) {
            if (pos.y >= 0.0f) tan_angle = pi / 2;
            else tan_angle = (pi / 2) * -1.0f;
        }
        else if (pos.x < 0.0f) {
            if (pos.y >= 0.0f) tan_angle = atan(pos.y / pos.x) + pi;
            else tan_angle = atan(pos.y / pos.x) - pi;
        }
        else {
            tan_angle = atan(pos.y / pos.x);
        }
        tan_angle *= 180.0f / pi;
        return tan_angle;
    };
    uint32_t target_id = 0;
    const float pi = 3.14159f;
    std::wstring arg1 = GuiUtils::ToLower(argv[1]);
    if (arg1 == L"ee") {
        // target best ebon escape target
        GW::AgentArray agents = GW::Agents::GetAgentArray();
        if (!agents.valid()) return;
        GW::Agent* me = GW::Agents::GetPlayer();
        if (me == nullptr) return;
        
        const float facing_angle = (me->rotation_angle * 180.0f / pi);
        const float wanted_angle = facing_angle > 0.0f ? facing_angle - 180.0f : facing_angle + 180.0f;
        const float max_angle_diff = 22.5f; // Acceptable angle for ebon escape
        const float max_distance = GW::Constants::SqrRange::Spellcast;
        float distance = 0.0f;

        size_t closest = (size_t)-1;
        for (size_t i = 0, size = agents.size(); i < size; ++i) {
            GW::AgentLiving* agent = (GW::AgentLiving * )agents[i];
            if (agent == nullptr || agent == me 
                || !agent->GetIsLivingType() || agent->GetIsDead() 
                || agent->allegiance == 0x3)
                continue;
            float this_distance = GW::GetSquareDistance(me->pos, agents[i]->pos);
            if (this_distance > max_distance || distance > this_distance)
                continue;
            float agent_angle = CalcAngle(me->pos - agents[i]->pos);
            float this_angle_diff = abs(wanted_angle - agent_angle);
            if (this_angle_diff > max_angle_diff)
                continue;
            closest = i;
            distance = this_distance;
        }
        if (closest != (size_t)-1) {
            GW::Agents::ChangeTarget(agents[closest]);
        }
    }
    if (arg1 == L"vipers" || arg1 == L"hos") {
        // target best vipers target (closest)
        GW::AgentArray agents = GW::Agents::GetAgentArray();
        if (!agents.valid()) return;
        GW::Agent* me = GW::Agents::GetPlayer();
        if (me == nullptr) return;

        const float wanted_angle = (me->rotation_angle * 180.0f / pi);
        const float max_angle_diff = 22.5f; // Acceptable angle for vipers
        float max_distance = GW::Constants::SqrRange::Spellcast;

        size_t closest = (size_t)-1;
        for (size_t i = 0, size = agents.size(); i < size; ++i) {
            GW::AgentLiving* agent = static_cast<GW::AgentLiving*>(agents[i]);
            if (agent == nullptr || agent == me || !agent->GetIsLivingType() || agent->GetIsDead())
                continue;
            float this_distance = GW::GetSquareDistance(me->pos, agents[i]->pos);
            if (this_distance > max_distance)
                continue;
            float agent_angle = CalcAngle(me->pos - agents[i]->pos);
            float this_angle_diff = abs(wanted_angle - agent_angle);
            if (this_angle_diff > max_angle_diff)
                continue;
            closest = i;
            max_distance = this_distance;
        }
        if (closest != (size_t)-1) {
            GW::Agents::ChangeTarget(agents[closest]);
        }
    }
    else if (arg1 == L"closest" || arg1 == L"nearest") {
        TargetNearest(0, 0xDB);
    } else if (arg1 == L"getid") {
        GW::AgentLiving* target = GW::Agents::GetTargetAsAgentLiving();
        if (target == nullptr) {
            Log::Error("No target selected!");
        } else {
            Log::Info("Target model id (PlayerNumber) is %d", target->player_number);
        }
    } else if (arg1 == L"getpos") {
        GW::AgentLiving* target = GW::Agents::GetTargetAsAgentLiving();
        if (target == nullptr) {
            Log::Error("No target selected!");
        } else {
            Log::Info("Target coordinates are (%f, %f)", target->pos.x, target->pos.y);
        }
    } else if (arg1 == L"item" && argc > 2 &&
               GuiUtils::ParseUInt(argv[2], &target_id)) {
        TargetNearest(target_id, 0x400);
    } else if (arg1 == L"gadget" && argc > 2 &&
               GuiUtils::ParseUInt(argv[2], &target_id)) {
        TargetNearest(target_id, 0x200);
    } else if (argc == 2 &&
               GuiUtils::ParseUInt(argv[1], &target_id)) {
        TargetNearest(target_id, 0xDB);
    } else {
        const wchar_t *name = next_word(message);
        GW::Player *target = GW::PlayerMgr::GetPlayerByName(name);
        if (target != NULL) {
            GW::Agent *agent = GW::Agents::GetAgentByID(target->agent_id);
            if (agent) {
                GW::Agents::ChangeTarget(agent);
            }
        }
    }
}

void ChatCommands::AddSkillToUse(uint32_t skill) {
    if (skill <= 0 || skill > 8) return;
    auto i = std::find(skills_to_use.begin(), skills_to_use.end(), skill - 1);
    if (i == skills_to_use.end()) {
        skills_to_use.push_front(skill - 1);
    }
}

void ChatCommands::CmdUseSkill(const wchar_t *, int argc, LPWSTR *argv) {
    if (!IsMapReady())
        return;
    Instance().skills_to_use.clear();
    if (argc < 2)
        return;
    std::wstring arg1 = GuiUtils::ToLower(argv[1]);
    if (arg1 == L"stop" || arg1 == L"off" || arg1 == L"0")
        return; // do nothing, already cleared skills_to_use
    uint32_t num = 0;
    for (int i = argc - 1; i > 0; --i) {
        if (!GuiUtils::ParseUInt(argv[i], &num) || num < 1) {
            Log::Error("Invalid argument '%ls', please use an integer value", argv[i]);
            continue;
        }
        // note: num can be one or more skills
        while (num > 10) {
            Instance().AddSkillToUse(num % 10);
            num = num / 10;
        }
        Instance().AddSkillToUse(num);
    }
}

void ChatCommands::CmdSCWiki(const wchar_t *message, int argc, LPWSTR *argv) {
    UNREFERENCED_PARAMETER(message);
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (argc == 1) {
        ShellExecuteW(NULL, L"open", L"http://wiki.fbgmguild.com/Main_Page", NULL, NULL, SW_SHOWNORMAL);
    } else {
        // the buffer is large enough, because you can type only 120 characters at once in the chat.
        wchar_t link[256] = L"http://wiki.fbgmguild.com/index.php?search=";
        int i;
        for (i = 1; i < argc - 1; i++) {
            wcscat_s(link, argv[i]);
            wcscat_s(link, L"+");
        }
        wcscat_s(link, argv[i]);
        ShellExecuteW(NULL, L"open", link, NULL, NULL, SW_SHOWNORMAL);
    }
}

void ChatCommands::CmdLoad(const wchar_t *message, int argc, LPWSTR *argv) {
    UNREFERENCED_PARAMETER(message);
    // We will & should move that to GWCA.
    static int(__cdecl *GetPersonalDir)(size_t size, wchar_t *dir) = 0;
    *(uintptr_t*)&GetPersonalDir = GW::MemoryMgr::GetPersonalDirPtr;
    if (argc == 1) {
        // We could open the build template window, but any interaction with it would make gw crash.
        // int32_t param[2] = { 0, 2 };
        // SendUIMessage(0x100001B4, param, NULL);
        return;
    }

    LPWSTR arg1 = argv[1];
    wchar_t dir[512];
    GetPersonalDir(512, dir); // @Fix, GetPersonalDir failed on Windows7, return path without slashes
    wcscat_s(dir, L"/GUILD WARS/Templates/Skills/");
    wcscat_s(dir, arg1);
    wcscat_s(dir, L".txt");

    char temp[64];
    if (!ReadTemplateFile(dir, temp, 64)) {
        // If it failed, we will interpret the input as the code models.
        size_t len = wcslen(arg1);
        if (len >= 64) return;
        for (size_t i = 0; i < len; i++)
            temp[i] = (char)arg1[i];
        temp[len] = 0;
    }
    if (argc == 2)
        GW::SkillbarMgr::LoadSkillTemplate(temp);   
    else if (argc == 3) {
        int ihero_number;
        if (GuiUtils::ParseInt(argv[2], &ihero_number)) {
            // @Robustness:
            // Check that the number is actually valid or make sure LoadSkillTemplate is safe
            if (0 < ihero_number && ihero_number <= 8) {
                GW::SkillbarMgr::LoadSkillTemplate(temp, static_cast<uint32_t>(ihero_number));
            }
        }
    }
}
void ChatCommands::CmdPingEquipment(const wchar_t* message, int argc, LPWSTR* argv) {
    UNREFERENCED_PARAMETER(message);
    if (!IsMapReady())
        return;
    if (argc < 2) {
        Log::Error("Missing argument for /pingitem");
        return;
    }
    std::wstring arg1 = GuiUtils::ToLower(argv[1]);
    if (arg1 == L"weapon")
        GameSettings::PingItem(GW::Items::GetItemBySlot(GW::Constants::Bag::Equipped_Items, 1), 3);
    else if (arg1 == L"offhand" || arg1 == L"shield")
        GameSettings::PingItem(GW::Items::GetItemBySlot(GW::Constants::Bag::Equipped_Items, 2), 3);
    else if (arg1 == L"chest")      
        GameSettings::PingItem(GW::Items::GetItemBySlot(GW::Constants::Bag::Equipped_Items, 3), 2);
    else if (arg1 == L"legs")       
        GameSettings::PingItem(GW::Items::GetItemBySlot(GW::Constants::Bag::Equipped_Items, 4), 2);
    else if (arg1 == L"head")
        GameSettings::PingItem(GW::Items::GetItemBySlot(GW::Constants::Bag::Equipped_Items, 5), 2);
    else if (arg1 == L"boots" || arg1 == L"feet")       
        GameSettings::PingItem(GW::Items::GetItemBySlot(GW::Constants::Bag::Equipped_Items, 6), 2);
    else if (arg1 == L"gloves" || arg1 == L"hands")     
        GameSettings::PingItem(GW::Items::GetItemBySlot(GW::Constants::Bag::Equipped_Items, 7), 2);
    else if (arg1 == L"weapons") {
        GameSettings::PingItem(GW::Items::GetItemBySlot(GW::Constants::Bag::Equipped_Items, 1),3);
        GameSettings::PingItem(GW::Items::GetItemBySlot(GW::Constants::Bag::Equipped_Items, 2), 3);
    }
    else if (arg1 == L"armor") {
        GameSettings::PingItem(GW::Items::GetItemBySlot(GW::Constants::Bag::Equipped_Items, 5), 2);
        GameSettings::PingItem(GW::Items::GetItemBySlot(GW::Constants::Bag::Equipped_Items, 3), 2);
        GameSettings::PingItem(GW::Items::GetItemBySlot(GW::Constants::Bag::Equipped_Items, 7), 2);
        GameSettings::PingItem(GW::Items::GetItemBySlot(GW::Constants::Bag::Equipped_Items, 4), 2);
        GameSettings::PingItem(GW::Items::GetItemBySlot(GW::Constants::Bag::Equipped_Items, 6), 2);
    }
    else if (arg1 == L"costume") {
        GameSettings::PingItem(GW::Items::GetItemBySlot(GW::Constants::Bag::Equipped_Items, 8), 1);
        GameSettings::PingItem(GW::Items::GetItemBySlot(GW::Constants::Bag::Equipped_Items, 9), 1);
    }
    else {
        Log::Error("Unrecognised /pingitem %ls", argv[1]);
    }
}

void ChatCommands::CmdTransmoParty(const wchar_t*, int argc, LPWSTR* argv) {
    GW::PartyInfo* pInfo = GW::PartyMgr::GetPartyInfo();
    if (!pInfo) return;
    PendingTransmo transmo;

    if (argc > 1) {
        int iscale;
        if (wcsncmp(argv[1], L"reset", 5) == 0) {
            transmo.npc_id = INT_MAX;
        }
        else if (GuiUtils::ParseInt(argv[1], &iscale)) {
            if (!ParseScale(iscale, transmo))
                return;
        } else if (!GetNPCInfoByName(argv[1], transmo)) {
            Log::Error("Unknown transmo '%ls'", argv[1]);
            return;
        }
        if (argc > 2 && GuiUtils::ParseInt(argv[2], &iscale)) {
            if (!ParseScale(iscale, transmo))
                return;
        }
    }
    else {
        if (!GetTargetTransmoInfo(transmo))
            return;
    }
    for (GW::HeroPartyMember& p : pInfo->heroes) {
        TransmoAgent(p.agent_id, transmo);
    }
    for (GW::HenchmanPartyMember& p : pInfo->henchmen) {
        TransmoAgent(p.agent_id, transmo);
    }
    for (GW::PlayerPartyMember& p : pInfo->players) {
        GW::Player* player = GW::PlayerMgr::GetPlayerByID(p.login_number);
        if (!player) continue;
        TransmoAgent(player->agent_id, transmo);
    }
}

bool ChatCommands::ParseScale(int scale, PendingTransmo& transmo) {
    if (scale < 6 || scale > 255) {
        Log::Error("scale must be between [6, 255]");
        return false;
    }
    transmo.scale = static_cast<DWORD>(scale) << 24;
    if (!transmo.npc_id)
        transmo.npc_id = INT_MAX - 1;
    return true;
}

void ChatCommands::CmdTransmoTarget(const wchar_t*, int argc, LPWSTR* argv) {
    
    GW::AgentLiving* target = GW::Agents::GetTargetAsAgentLiving();
    if (argc < 2) {
        Log::Error("Missing /transmotarget argument");
        return;
    }
    if (!target) {
        Log::Error("Invalid /transmotarget target");
        return;
    }
    PendingTransmo transmo;
    int iscale;
    if (wcsncmp(argv[1], L"reset", 5) == 0) {
        transmo.npc_id = INT_MAX;
    }
    else if (GuiUtils::ParseInt(argv[1], &iscale)) {
        if (!ParseScale(iscale, transmo))
            return;
    } else if (!GetNPCInfoByName(argv[1], transmo)) {
        Log::Error("Unknown transmo '%ls'", argv[1]);
        return;
    }
    if (argc > 2 && GuiUtils::ParseInt(argv[2], &iscale)) {
        if (!ParseScale(iscale, transmo))
            return;
    }
    TransmoAgent(target->agent_id, transmo);
}

void ChatCommands::CmdTransmo(const wchar_t *, int argc, LPWSTR *argv) {
    PendingTransmo transmo;
    
    if (argc > 1) {
        int iscale;
        if (wcsncmp(argv[1], L"reset", 5) == 0) {
            transmo.npc_id = INT_MAX;
        }
        else if(GuiUtils::ParseInt(argv[1], &iscale)) {
            if (!ParseScale(iscale, transmo))
                return;
        } else if (!GetNPCInfoByName(argv[1], transmo)) {
            Log::Error("unknown transmo '%ls'", argv[1]);
            return;
        }
        if (argc > 2 && GuiUtils::ParseInt(argv[2], &iscale)) {
            if (!ParseScale(iscale, transmo))
                return;
        }
    }
    else {
        if (!GetTargetTransmoInfo(transmo))
            return;
    }
    TransmoAgent(GW::Agents::GetPlayerId(), transmo);
}
bool ChatCommands::GetTargetTransmoInfo(PendingTransmo &transmo)
{
    GW::AgentLiving *target = GW::Agents::GetTargetAsAgentLiving();
    if (!target)
        return false;
    transmo.npc_id = target->player_number;
    if (target->transmog_npc_id & 0x20000000)
        transmo.npc_id = target->transmog_npc_id ^ 0x20000000;
    else if (target->IsPlayer())
        return false;
    return true;
}

void ChatCommands::CmdTransmoAgent(const wchar_t* , int argc, LPWSTR* argv) {
    if(argc < 3)
        return Log::Error("Missing /transmoagent argument");
    int iagent_id = 0;
    if(!GuiUtils::ParseInt(argv[1], &iagent_id) || iagent_id < 0)
        return Log::Error("Invalid /transmoagent agent_id");
    PendingTransmo transmo;
    uint32_t agent_id = static_cast<uint32_t>(iagent_id);
    int iscale;
    if (wcsncmp(argv[2], L"reset", 5) == 0) {
        transmo.npc_id = INT_MAX;
    }
    else if (GuiUtils::ParseInt(argv[2], &iscale)) {
        if (!ParseScale(iscale, transmo))
            return;
    } else if (!GetNPCInfoByName(argv[2], transmo)) {
        Log::Error("unknown transmo '%s'", argv[1]);
        return;
    }
    if (argc > 4 && GuiUtils::ParseInt(argv[3], &iscale)) {
        if (!ParseScale(iscale, transmo))
            return;
    }
    TransmoAgent(agent_id, transmo);
}

void ChatCommands::CmdResize(const wchar_t *message, int argc, LPWSTR *argv) {
    UNREFERENCED_PARAMETER(message);
    if (argc != 3) {
        Log::Error("The syntax is /resize width height");
        return;
    }
    int width, height;
    if (!(GuiUtils::ParseInt(argv[1], &width) && GuiUtils::ParseInt(argv[2], &height))) {
        Log::Error("The syntax is /resize width height");
        return;
    }
    HWND hwnd = GW::MemoryMgr::GetGWWindowHandle();
    RECT rect;
    GetWindowRect(hwnd, &rect);
    MoveWindow(hwnd, rect.left, rect.top, width, height, TRUE);
}

void ChatCommands::CmdReapplyTitle(const wchar_t* message, int argc, LPWSTR* argv) {
    UNREFERENCED_PARAMETER(message);
    UNREFERENCED_PARAMETER(argc);
    UNREFERENCED_PARAMETER(argv);
    if (!IsMapReady())
        return;
    GW::PlayerMgr::RemoveActiveTitle();

    switch (GW::Map::GetMapID()) {
    case GW::Constants::MapID::Rata_Sum_outpost:
    case GW::Constants::MapID::Tarnished_Haven_outpost:
    case GW::Constants::MapID::Vloxs_Falls:
    case GW::Constants::MapID::Gadds_Encampment_outpost:
    case GW::Constants::MapID::Umbral_Grotto_outpost:
    case GW::Constants::MapID::Magus_Stones:
    case GW::Constants::MapID::Riven_Earth:
    case GW::Constants::MapID::Arbor_Bay:
    case GW::Constants::MapID::Alcazia_Tangle:
    case GW::Constants::MapID::Sparkfly_Swamp:
    case GW::Constants::MapID::Verdant_Cascades:
        GW::PlayerMgr::SetActiveTitle(GW::Constants::TitleID::Asuran);
        break;
    case GW::Constants::MapID::Glints_Challenge_mission:
    case GW::Constants::MapID::Central_Transfer_Chamber_outpost:
    case GW::Constants::MapID::Genius_Operated_Living_Enchanted_Manifestation:
    case GW::Constants::MapID::A_Gate_Too_Far_Level_1:
    case GW::Constants::MapID::A_Gate_Too_Far_Level_2:
    case GW::Constants::MapID::A_Gate_Too_Far_Level_3:
    case GW::Constants::MapID::Destructions_Depths_Level_1:
    case GW::Constants::MapID::Destructions_Depths_Level_2:
    case GW::Constants::MapID::Destructions_Depths_Level_3:
    case GW::Constants::MapID::A_Time_for_Heroes:
    case GW::Constants::MapID::Ravens_Point_Level_1:
    case GW::Constants::MapID::Ravens_Point_Level_2:
    case GW::Constants::MapID::Ravens_Point_Level_3:
        GW::PlayerMgr::SetActiveTitle(GW::Constants::TitleID::Deldrimor);
        break;
    case GW::Constants::MapID::Boreal_Station_outpost:
    case GW::Constants::MapID::Eye_of_the_North_outpost:
    case GW::Constants::MapID::Gunnars_Hold_outpost:
    case GW::Constants::MapID::Sifhalla_outpost:
    case GW::Constants::MapID::Olafstead_outpost:
    case GW::Constants::MapID::Ice_Cliff_Chasms:
    case GW::Constants::MapID::Norrhart_Domains:
    case GW::Constants::MapID::Drakkar_Lake:
    case GW::Constants::MapID::Jaga_Moraine:
    case GW::Constants::MapID::Bjora_Marches:
    case GW::Constants::MapID::Varajar_Fells:
    case GW::Constants::MapID::Attack_of_the_Nornbear:
    case GW::Constants::MapID::Curse_of_the_Nornbear:
    case GW::Constants::MapID::Blood_Washes_Blood:
    case GW::Constants::MapID::Mano_a_Norn_o:
    case GW::Constants::MapID::Service_In_Defense_of_the_Eye:
    case GW::Constants::MapID::Cold_as_Ice:
    case GW::Constants::MapID::The_Norn_Fighting_Tournament:
        // @todo: case MapID for Bear Club for Women/Men
        GW::PlayerMgr::SetActiveTitle(GW::Constants::TitleID::Norn);
        break;
    case GW::Constants::MapID::Doomlore_Shrine_outpost:
    case GW::Constants::MapID::Longeyes_Ledge_outpost:
    case GW::Constants::MapID::Grothmar_Wardowns:
    case GW::Constants::MapID::Dalada_Uplands:
    case GW::Constants::MapID::Sacnoth_Valley:
    case GW::Constants::MapID::Against_the_Charr:
    case GW::Constants::MapID::Warband_of_Brothers_Level_1:
    case GW::Constants::MapID::Warband_of_Brothers_Level_2:
    case GW::Constants::MapID::Warband_of_Brothers_Level_3:
    case GW::Constants::MapID::Assault_on_the_Stronghold:
    case GW::Constants::MapID::Cathedral_of_Flames_Level_1:
    case GW::Constants::MapID::Cathedral_of_Flames_Level_2:
    case GW::Constants::MapID::Cathedral_of_Flames_Level_3:
    case GW::Constants::MapID::Rragars_Menagerie_Level_1:
    case GW::Constants::MapID::Rragars_Menagerie_Level_2:
    case GW::Constants::MapID::Rragars_Menagerie_Level_3:
    case GW::Constants::MapID::Warband_Training:
    case GW::Constants::MapID::Ascalon_City_outpost:
    case GW::Constants::MapID::The_Great_Northern_Wall:
    case GW::Constants::MapID::Fort_Ranik:
    case GW::Constants::MapID::Ruins_of_Surmia:
    case GW::Constants::MapID::Nolani_Academy:
    case GW::Constants::MapID::Frontier_Gate_outpost:
    case GW::Constants::MapID::Grendich_Courthouse_outpost:
    case GW::Constants::MapID::Sardelac_Sanitarium_outpost:
    case GW::Constants::MapID::Piken_Square_outpost:
    case GW::Constants::MapID::Old_Ascalon:
    case GW::Constants::MapID::Regent_Valley:
    case GW::Constants::MapID::The_Breach:
    case GW::Constants::MapID::Diessa_Lowlands:
    case GW::Constants::MapID::Flame_Temple_Corridor:
    case GW::Constants::MapID::Dragons_Gullet:
        GW::PlayerMgr::SetActiveTitle(GW::Constants::TitleID::Vanguard);
        break;
    default:
        GW::PlayerMgr::SetActiveTitle(GW::Constants::TitleID::Lightbringer);
    }
}
void ChatCommands::CmdHeroBehaviour(const wchar_t*, int argc, LPWSTR* argv)
{
    if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading)
        return;
    // Argument validation
    if (argc < 2)
        return Log::Error("Invalid argument for /hero. It can be one of: avoid | guard | attack");
    // set behavior based on command message
    int behaviour = 1; // guard by default
    std::wstring arg1 = GuiUtils::ToLower(argv[1]);
    if (arg1 == L"avoid") {
        behaviour = 2; // avoid combat
    } else if (arg1 == L"guard") {
        behaviour = 1; // guard
    } else if (arg1 == L"attack") {
        behaviour = 0; // attack
    } else {
        return Log::Error("Invalid argument for /hero. It can be one of: avoid | guard | attack");
    }

    const GW::PartyInfo *party_info = GW::PartyMgr::GetPartyInfo();
    if (!party_info)
        return Log::Error("Could not retrieve party info");
    const GW::HeroPartyMemberArray& party_heros = party_info->heroes;
    if (!party_heros.valid())
        return Log::Error("Party heroes validation failed");
    const GW::AgentLiving* me = GW::Agents::GetPlayerAsAgentLiving();
    if (!me)
        return Log::Error("Failed to get player");

    //execute in explorable area or outpost
    for (const GW::HeroPartyMember& hero : party_heros) {
        if (hero.owner_player_id == me->login_number) {
            GW::CtoS::SendPacket(0xC, GAME_CMSG_HERO_BEHAVIOR, hero.agent_id, behaviour);
        }
    }
}
