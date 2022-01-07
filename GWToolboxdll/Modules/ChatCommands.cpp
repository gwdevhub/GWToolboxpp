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
#include <GWCA/GameEntities/Item.h>
#include <GWCA/GameEntities/Quest.h>

#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/WorldContext.h>
#include <GWCA/Context/GuildContext.h>
#include <GWCA/Context/PartyContext.h>

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
#include <Modules/ObserverModule.h>
#include <Modules/GameSettings.h>
#include <Widgets/PartyDamage.h>
#include <Windows/BuildsWindow.h>
#include <Windows/Hotkeys.h>
#include <Windows/MainWindow.h>
#include <Windows/SettingsWindow.h>
#include <Widgets/TimerWidget.h>


#define F_PI 3.14159265358979323846f

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
        return GW::Map::GetInstanceType() != GW::Constants::InstanceType::Loading && !GW::Map::GetIsObserving() && GW::MemoryMgr::GetGWWindowHandle() == GetActiveWindow();
    }
    bool ImInPresearing() { return GW::Map::GetCurrentMapInfo()->region == GW::Region_Presearing; }

    static const float GetAngle(GW::GamePos pos) {
        const float pi = static_cast<float>(M_PI);
        float tan_angle = 0.0f;
        if (pos.x == 0.0f) {
            if (pos.y >= 0.0f) tan_angle = F_PI / 2;
            else tan_angle = (pi / 2) * -1.0f;
        }
        else if (pos.x < 0.0f) {
            if (pos.y >= 0.0f) tan_angle = atan(pos.y / pos.x) + F_PI;
            else tan_angle = atan(pos.y / pos.x) - pi;
        }
        else {
            tan_angle = atan(pos.y / pos.x);
        }
        tan_angle *= 180.0f / pi;
        return tan_angle;
    };

    static void TargetVipers() {
        // target best vipers target (closest)
        const GW::AgentArray agents = GW::Agents::GetAgentArray();
        if (!agents.valid()) return;
        const GW::Agent* me = GW::Agents::GetPlayer();
        if (me == nullptr) return;

        const float wanted_angle = (me->rotation_angle * 180.0f / F_PI);
        const float max_angle_diff = 22.5f; // Acceptable angle for vipers
        float max_distance = GW::Constants::SqrRange::Spellcast;

        size_t closest = static_cast<size_t>(-1);
        for (size_t i = 0, size = agents.size(); i < size; ++i) {
            GW::AgentLiving* agent = static_cast<GW::AgentLiving*>(agents[i]);
            if (agent == nullptr || agent == me || !agent->GetIsLivingType() || agent->GetIsDead())
                continue;
            const float this_distance = GW::GetSquareDistance(me->pos, agents[i]->pos);
            if (this_distance > max_distance)
                continue;
            const float agent_angle = GetAngle(me->pos - agents[i]->pos);
            const float this_angle_diff = abs(wanted_angle - agent_angle);
            if (this_angle_diff > max_angle_diff)
                continue;
            closest = i;
            max_distance = this_distance;
        }
        if (closest != static_cast<size_t>(-1)) {
            GW::Agents::ChangeTarget(agents[closest]);
        }
    }
    static void TargetEE() {
        // target best ebon escape target
        const GW::AgentArray agents = GW::Agents::GetAgentArray();
        if (!agents.valid()) return;
        const GW::Agent* const me = GW::Agents::GetPlayer();
        if (me == nullptr) return;

        const float facing_angle = (me->rotation_angle * 180.0f / F_PI);
        const float wanted_angle = facing_angle > 0.0f ? facing_angle - 180.0f : facing_angle + 180.0f;
        const float max_angle_diff = 22.5f; // Acceptable angle for ebon escape
        const float max_distance = GW::Constants::SqrRange::Spellcast;
        float distance = 0.0f;

        size_t closest = static_cast<size_t>(-1);
        for (size_t i = 0, size = agents.size(); i < size; ++i) {
            const GW::AgentLiving* const agent = static_cast<GW::AgentLiving*>(agents[i]);
            if (agent == nullptr || agent == me
                || !agent->GetIsLivingType() || agent->GetIsDead()
                || agent->allegiance == 0x3)
                continue;
            const float this_distance = GW::GetSquareDistance(me->pos, agents[i]->pos);
            if (this_distance > max_distance || distance > this_distance)
                continue;
            const float agent_angle = GetAngle(me->pos - agents[i]->pos);
            const float this_angle_diff = abs(wanted_angle - agent_angle);
            if (this_angle_diff > max_angle_diff)
                continue;
            closest = i;
            distance = this_distance;
        }
        if (closest != static_cast<size_t>(-1)) {
            GW::Agents::ChangeTarget(agents[closest]);
        }
    }

    static bool IsNearestStr(const wchar_t* str) {
        return wcscmp(str, L"nearest") == 0 || wcscmp(str, L"closest") == 0;
    }

    static std::map<std::string, ChatCommands::PendingTransmo> npc_transmos;
} // namespace

void ChatCommands::TransmoAgent(DWORD agent_id, PendingTransmo& transmo)
{
    if (!transmo.npc_id || !agent_id)
        return;
    const GW::AgentLiving* const a = static_cast<GW::AgentLiving*>(GW::Agents::GetAgentByID(agent_id));
    if (!a || !a->GetIsLivingType())
        return;
    DWORD &npc_id = transmo.npc_id;
    DWORD &scale = transmo.scale;
    const GW::NPCArray &npcs = GW::GameContext::instance()->world->npcs;
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
        const DWORD& npc_model_file_id = transmo.npc_model_file_id;
        const DWORD& npc_model_file_data = transmo.npc_model_file_data;
        const DWORD& flags = transmo.flags;
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
            const GW::NPCArray& npcs = GW::GameContext::instance()->world->npcs;
            const GW::NPC npc = npcs[npc_id];
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
    for (const auto& npc_transmo : npc_transmos) {
        const size_t found_len = npc_transmo.first.find(name);
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
    if (!ImGui::TreeNodeEx("Chat Commands", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth))
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
    ImGui::Bullet(); ImGui::Text("'/observer:reset' resets observer mode data.");
    ImGui::Bullet(); ImGui::Text("'/dialog <id>' sends a dialog.");
    ImGui::Bullet(); ImGui::Text("'/flag [all|clear|<number>]' to flag a hero in the minimap (same as using the buttons by the minimap).");
    ImGui::Bullet(); ImGui::Text("'/flag [all|<number>] [x] [y]' to flag a hero to coordinates [x],[y].");
    ImGui::Bullet(); ImGui::Text("'/flag <number> clear' to clear flag for a hero.");
    
    ImGui::Bullet(); ImGui::Text("'/load [build template|build name] [Hero index]' loads a build. The build name must be between quotes if it contains spaces. First Hero index is 1, last is 7. Leave out for player");
    ImGui::Bullet(); ImGui::Text("'/pcons [on|off]' toggles, enables or disables pcons.");
    const char* toggle_hint = "<name> options: helm, costume, costume_head, cape, <window_or_widget_name>";
    ImGui::Bullet(); ImGui::Text("'/show <name>' opens the window, in-game feature or widget titled <name>.");
    ImGui::ShowHelp(toggle_hint);
    ImGui::Bullet(); ImGui::Text("'/hide <name>' closes the window, in-game feature or widget titled <name>.");
    ImGui::ShowHelp(toggle_hint);
    ImGui::Bullet(); ImGui::Text("'/toggle <name> [on|off|toggle]' toggles the window, in-game feature or widget titled <name>.");
    ImGui::ShowHelp(toggle_hint);
    ImGui::Bullet(); ImGui::Text("'/target closest' to target the closest agent to you.\n"
        "'/target ee' to target best ebon escape agent.\n"
        "'/target hos' to target best vipers/hos agent.\n"
        "'/target [name|model_id] [index]' target nearest NPC by name or model_id. \n\tIf index is specified, it will target index-th by ID.\n"
        "'/target player [name|player_number]' target nearest player by name or player number.\n"
        "'/target gadget [name|gadget_id]' target nearest interactive object by name or gadget_id.\n"
        "'/target priority [partymember]' to target priority target of party member.");
    ImGui::Bullet(); ImGui::Text("'/tb <name>' toggles the window or widget titled <name>.");
    ImGui::Bullet(); ImGui::Text("'/tb reset' moves Toolbox and Settings window to the top-left corner.");
    ImGui::Bullet(); ImGui::Text("'/tb quit' or '/tb exit' completely closes toolbox and all its windows.");
    ImGui::Bullet(); ImGui::Text("'/travel <town> [dis]', '/tp <town> [dis]' or '/to <town> [dis]' to travel to a destination. \n"
        "<town> can be any of: doa, kamadan/kama, embark, vlox, gadds, urgoz, deep, gtob, fav1, fav2, fav3.\n"
        "[dis] can be any of: ae, ae1, ee, eg, int, etc");
    ImGui::Bullet(); ImGui::Text("'/useskill <skill>' starts using the skill on recharge. "
        "Use the skill number instead of <skill> (e.g. '/useskill 5'). "
        "Use empty '/useskill' or '/useskill stop' to stop all. "
        "Use '/useskill <skill>' to stop the skill.");
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
    ImGui::InputFloat("Camera speed", &cam_speed);
    ImGui::Unindent();
}

void ChatCommands::LoadSettings(CSimpleIni* ini) {
    forward_fix_z = ini->GetBoolValue(Name(), VAR_NAME(forward_fix_z), forward_fix_z);
    cam_speed = static_cast<float>(ini->GetDoubleValue(Name(), VAR_NAME(cam_speed), DEFAULT_CAM_SPEED));
}

void ChatCommands::SaveSettings(CSimpleIni* ini) {
    ini->SetBoolValue(Name(), VAR_NAME(forward_fix_z), forward_fix_z);
    ini->SetDoubleValue(Name(), VAR_NAME(cam_speed), cam_speed);
}

void ChatCommands::CmdPingQuest(const wchar_t* , int , LPWSTR* ) {
    Instance().quest_ping.Init();
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
    GW::Chat::CreateCommand(L"enter", ChatCommands::CmdEnterMission);
    GW::Chat::CreateCommand(L"age2", ChatCommands::CmdAge2);
    GW::Chat::CreateCommand(L"dialog", ChatCommands::CmdDialog);
    GW::Chat::CreateCommand(L"show", ChatCommands::CmdShow);
    GW::Chat::CreateCommand(L"hide", ChatCommands::CmdHide);
    GW::Chat::CreateCommand(L"toggle", ChatCommands::CmdToggle);
    GW::Chat::CreateCommand(L"tb", ChatCommands::CmdTB);
    GW::Chat::CreateCommand(L"zoom", ChatCommands::CmdZoom);
    GW::Chat::CreateCommand(L"camera", ChatCommands::CmdCamera);
    GW::Chat::CreateCommand(L"cam", ChatCommands::CmdCamera);
    GW::Chat::CreateCommand(L"damage", ChatCommands::CmdDamage);
    GW::Chat::CreateCommand(L"dmg", ChatCommands::CmdDamage);
    GW::Chat::CreateCommand(L"observer:reset", ChatCommands::CmdObserverReset);

    GW::Chat::CreateCommand(L"chest", ChatCommands::CmdChest);
    GW::Chat::CreateCommand(L"xunlai", ChatCommands::CmdChest);
    GW::Chat::CreateCommand(L"afk", ChatCommands::CmdAfk);
    GW::Chat::CreateCommand(L"target", ChatCommands::CmdTarget);
    GW::Chat::CreateCommand(L"tgt", ChatCommands::CmdTarget);
    GW::Chat::CreateCommand(L"useskill", ChatCommands::CmdUseSkill);
    GW::Chat::CreateCommand(L"scwiki", ChatCommands::CmdSCWiki);
    GW::Chat::CreateCommand(L"load", ChatCommands::CmdLoad);
    GW::Chat::CreateCommand(L"ping", ChatCommands::CmdPing);
    GW::Chat::CreateCommand(L"quest", ChatCommands::CmdPingQuest);
    GW::Chat::CreateCommand(L"transmo", ChatCommands::CmdTransmo);
    GW::Chat::CreateCommand(L"transmotarget", ChatCommands::CmdTransmoTarget);
    GW::Chat::CreateCommand(L"transmoparty", ChatCommands::CmdTransmoParty);
    GW::Chat::CreateCommand(L"transmoagent", ChatCommands::CmdTransmoAgent);
    GW::Chat::CreateCommand(L"resize", ChatCommands::CmdResize);
    GW::Chat::CreateCommand(L"settitle", ChatCommands::CmdReapplyTitle);
    GW::Chat::CreateCommand(L"title", ChatCommands::CmdReapplyTitle);
    GW::Chat::CreateCommand(L"pingitem", ChatCommands::CmdPingEquipment);
    GW::Chat::CreateCommand(L"tick", [](const wchar_t*, int, LPWSTR*) -> void {
        GW::PartyMgr::Tick(!GW::PartyMgr::GetIsPlayerTicked());
        });
    GW::Chat::CreateCommand(L"armor", [](const wchar_t*, int, LPWSTR*) -> void {
        GW::Chat::SendChat('/', "pingitem armor");
    });
    GW::Chat::CreateCommand(L"hero", ChatCommands::CmdHeroBehaviour);
    GW::Chat::CreateCommand(L"morale", ChatCommands::CmdMorale);
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
    static bool keep_forward; // No init. should it be false as default

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
    skill_to_use.Update();
    npc_to_find.Update();
    quest_ping.Update();

}
void ChatCommands::QuestPing::Init() {
    auto w = GW::GameContext::instance()->world;
    if (!w->active_quest_id)
        return;
    for (auto& quest : w->quest_log) {
        if (quest.quest_id != w->active_quest_id)
            continue;
        name.reset(quest.name);
        objectives.reset(quest.objectives);
        return;
    }

}
void ChatCommands::QuestPing::Update() {
    if (!name.wstring().empty()) {
        wchar_t print_buf[128];
        swprintf(print_buf, _countof(print_buf), L"Current Quest: %s", name.wstring().c_str());
        GW::Chat::SendChat('#', print_buf);
    }
    if (!objectives.wstring().empty()) {
        // Find current objective using regex
        std::wregex current_obj_regex(L"\\{s\\}([^\\{]+)");
        std::wsmatch m;
        if (std::regex_search(objectives.wstring(), m, current_obj_regex)) {
            wchar_t print_buf[128];
            swprintf(print_buf, _countof(print_buf), L" - %s", m[1].str().c_str());
            GW::Chat::SendChat('#', print_buf);
        }
        objectives.reset(nullptr);
    }
    if (!name.wstring().empty()) {
        GW::Chat::SendChat('#', GuiUtils::WikiUrl(name.wstring().c_str()).c_str());
        name.reset(nullptr);
    }
}
void ChatCommands::SearchAgent::Init(const wchar_t* _search, TargetType type) {
    started = 0;
    npc_names.clear();
    if (!_search || !_search[0])
        return;
    search = GuiUtils::ToLower(_search);
    npc_names.clear();
    started = TIMER_INIT();
    GW::AgentArray agents = GW::Agents::GetAgentArray();
    for (const GW::Agent* agent : agents) {
        if (!agent) continue;
        switch (type) {
        case Item:
            if (!agent->GetIsItemType())
                continue;
            break;
        case Gadget:
            if (!agent->GetIsGadgetType())
                continue;
            break;
        case Player:
            if (!agent->GetIsLivingType() || !agent->GetAsAgentLiving()->IsPlayer())
                continue;
            break;
        case Npc: {
            const GW::AgentLiving* agent_living = agent->GetAsAgentLiving();
            if (!agent_living || !agent_living->IsNPC() || !agent_living->GetIsAlive())
                continue;
        } break;
        case Living: {
                const GW::AgentLiving* agent_living = agent->GetAsAgentLiving();
                if (!agent_living || !agent_living->GetIsAlive())
                    continue;
        } break;
        default:
            continue;
        }
        const wchar_t* enc_name = GW::Agents::GetAgentEncName(agent);
        if (!enc_name || !enc_name[0])
            continue;
        npc_names.push_back({ agent->agent_id, new GuiUtils::EncString(enc_name) });
    }
}
void ChatCommands::SearchAgent::Update() {
    if (!started) return;
    if (TIMER_DIFF(started) > 3000) {
        Log::Error("Timeout getting NPC names");
        Init(nullptr);
        return;
    }
    for (auto& enc_name : npc_names) {
        if (enc_name.second->wstring().empty())
            return; // Not all decoded yet
    }
    // Do search
    size_t found = std::wstring::npos;
    float distance = GW::Constants::SqrRange::Compass;
    size_t closest = 0;
    GW::Agent* me = GW::Agents::GetPlayer();
    if (!me)
        return;
    for (auto& enc_name : npc_names) {
        found = GuiUtils::ToLower(enc_name.second->wstring()).find(search.c_str());
        if (found == std::wstring::npos)
            continue;
        GW::Agent* agent = GW::Agents::GetAgentByID(enc_name.first);
        if (!agent)
            continue;
        float dist = GW::GetDistance(me->pos, agent->pos);
        if (dist < distance) {
            closest = agent->agent_id;
            distance = dist;
        }
    }
    if(closest) GW::Agents::ChangeTarget(closest);
    Init(nullptr);
}

void ChatCommands::SkillToUse::Update() {
    if (!slot)
        return;
    if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Explorable || GW::Map::GetIsObserving()) {
        slot = 0;
        return;
    }
    if ((clock() - skill_timer) / 1000.0f < skill_usage_delay)
        return;
    const GW::Skillbar* const skillbar = GW::SkillbarMgr::GetPlayerSkillbar();
    if (!skillbar || !skillbar->IsValid()) {
        slot = 0;
        return;
    }
    uint32_t lslot = slot - 1;
    const GW::SkillbarSkill& skill = skillbar->skills[lslot];
    if (!skill.skill_id
        || skill.skill_id == (uint32_t)GW::Constants::SkillID::Mystic_Healing
        || skill.skill_id == (uint32_t)GW::Constants::SkillID::Cautery_Signet) {
        slot = 0;
        return;
    }
    const GW::Skill& skilldata = GW::SkillbarMgr::GetSkillConstantData(skill.skill_id);
    if ((skilldata.adrenaline == 0 && skill.GetRecharge() == 0) || (skilldata.adrenaline > 0 && skill.adrenaline_a == skilldata.adrenaline)) {
        GW::SkillbarMgr::UseSkill(lslot, GW::Agents::GetTargetId());
        skill_usage_delay = std::max(skilldata.activation + skilldata.aftercast, 0.25f); // a small flat delay of .3s for ping and to avoid spamming in case of bad target
        skill_timer = clock();
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

void ChatCommands::CmdEnterMission(const wchar_t*, int argc, LPWSTR* argv) {
    const char* const error_use_from_outpost = "Use '/enter' to start a mission or elite area from an outpost";
    const char* const error_fow_uw_syntax = "Use '/enter fow' or '/enter uw' to trigger entry";
    const char* const error_no_scrolls = "Unable to enter elite area; no scroll found";
    const char* const error_not_leading = "Unable to enter mission; you're not party leader";
    
    if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Outpost) 
        return Log::Error(error_use_from_outpost);

    switch (GW::Map::GetMapID()) {
    case GW::Constants::MapID::Temple_of_the_Ages:
    case GW::Constants::MapID::Embark_Beach: {
        if (argc < 2)
            return Log::Error(error_fow_uw_syntax);
        uint32_t item_id;
        const std::wstring arg1 = GuiUtils::ToLower(argv[1]);
        if (arg1 == L"fow")
            item_id = 22280;
        else if (arg1 == L"uw")
            item_id = 3746;
        else
            return Log::Error(error_fow_uw_syntax);
        if (!GW::Items::UseItemByModelId(item_id, 1, 4) && !GW::Items::UseItemByModelId(item_id, 8, 16))
            return Log::Error(error_no_scrolls);
    }
        break;
    default:
        const GW::AreaInfo* const map_info = GW::Map::GetCurrentMapInfo();
        if (!map_info || !map_info->GetHasEnterButton())
            return Log::Error(error_use_from_outpost);
        if (!GW::PartyMgr::GetPlayerIsLeader())
            return Log::Error(error_not_leading);
        const GW::PartyContext* const p = GW::GameContext::instance()->party;
        if (p && (p->flag & 0x8) != 0) {
            GW::CtoS::SendPacket(4, GAME_CMSG_PARTY_CANCEL_ENTER_CHALLENGE);
        }
        else {
            GW::Map::EnterChallenge();
        }
    }
}

void ChatCommands::CmdMorale(const wchar_t*, int , LPWSTR* ) {
    if (GW::GameContext::instance()->world->morale == 100)
        GW::Chat::SendChat('#', L"I have no Morale Boost or Death Penalty!");
    else
        GW::CtoS::SendPacket(0xC, GAME_CMSG_TARGET_CALL, 0x7, GW::Agents::GetPlayerId());
}
void ChatCommands::CmdAge2(const wchar_t* , int, LPWSTR* ) {
    TimerWidget::Instance().PrintTimer();
}

void ChatCommands::CmdDialog(const wchar_t *, int argc, LPWSTR *argv) {
    if (!IsMapReady())
        return;
    if (argc <= 1) {
        Log::Error("Please provide an integer or hex argument");
    } else {
        uint32_t id = 0;
        if (GuiUtils::ParseUInt(argv[1], &id)) {
            GW::Agents::SendDialog(id);
            Log::Info("Sent Dialog 0x%X", id);
        } else {
            Log::Error("Invalid argument '%ls', please use an integer or hex value", argv[0]);
        }
    }
}

void ChatCommands::CmdChest(const wchar_t *, int, LPWSTR * argv) {
    if (!IsMapReady())
        return;
    if (wcscmp(argv[0], L"chest") == 0 
        && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable) {
        const GW::Agent* const target = GW::Agents::GetTarget();
        if (target && target->type == 0x200) {
            GW::Agents::GoSignpost(target);
            GW::Items::OpenLockedChest();
        }
        return;
    }
    GW::Items::OpenXunlaiWindow();
}

void ChatCommands::CmdTB(const wchar_t *message, int argc, LPWSTR *argv) {
    if (!ImGui::GetCurrentContext())
        return; // Don't process window manips until ImGui is ready
    if (argc < 2) { // e.g. /tb
        MainWindow::Instance().visible ^= 1;
        return;
    }
    
    if (argc < 3) {
        const std::wstring arg = GuiUtils::ToLower(argv[1]);
        if (arg == L"hide") { // e.g. /tb hide
            MainWindow::Instance().visible = false;
        }
        else if (arg == L"show") { // e.g. /tb show
            MainWindow::Instance().visible = true;
        }
        else if (arg == L"save") { // e.g. /tb save
            GWToolbox::Instance().SaveSettings();
        }
        else if (arg == L"load") { // e.g. /tb load
            GWToolbox::Instance().OpenSettingsFile();
            GWToolbox::Instance().LoadModuleSettings();
        }
        else if (arg == L"reset") { // e.g. /tb reset
            ImGui::SetWindowPos(MainWindow::Instance().Name(), ImVec2(50.0f, 50.0f));
            ImGui::SetWindowPos(SettingsWindow::Instance().Name(), ImVec2(50.0f, 50.0f));
            MainWindow::Instance().visible = false;
            SettingsWindow::Instance().visible = true;
        }
        else if (arg == L"mini" || arg == L"minimize" || arg == L"collapse") { // e.g. /tb mini
            ImGui::SetWindowCollapsed(MainWindow::Instance().Name(), true);
        }
        else if (arg == L"maxi" || arg == L"maximize") { // e.g. /tb maxi
            ImGui::SetWindowCollapsed(MainWindow::Instance().Name(), false);
        }
        else if (arg == L"close" || arg == L"quit" || arg == L"exit") { // e.g. /tb close
            GWToolbox::Instance().StartSelfDestruct();
        }
        else { // e.g. /tb travel
            std::vector<ToolboxUIElement*> windows = MatchingWindows(message, argc, argv);
            for (ToolboxUIElement* window : windows) {
                window->visible ^= 1;
            }
        }
        return;
    }
    std::vector<ToolboxUIElement*> windows = MatchingWindows(message, argc, argv);
    const std::wstring arg = GuiUtils::ToLower(argv[2]);
    if (arg == L"hide") { // e.g. /tb travel hide
        for (auto const& window : windows)
            window->visible = false;
    }
    else if (arg == L"show") { // e.g. /tb travel show
        for (auto const& window : windows)
            window->visible = true;
    }
    else if (arg == L"mini" || arg == L"minimize" || arg == L"collapse") { // e.g. /tb travel mini
        for (auto const& window : windows)
            ImGui::SetWindowCollapsed(window->Name(), true);
    }
    else if (arg == L"maxi" || arg == L"maximize") { // e.g. /tb travel maxi
        for (auto const& window : windows)
            ImGui::SetWindowCollapsed(window->Name(), false);
    }
    else { // Invalid argument
        constexpr size_t buffer_size = 255;
        char buffer[buffer_size];
        snprintf(buffer, buffer_size, "Syntax: /%S %S [hide|show|mini|maxi]", argv[0], argv[1]);
        Log::Error(buffer);
    }
}

GW::UI::WindowID ChatCommands::MatchingGWWindow(const wchar_t*, int argc, LPWSTR* argv) {
    const std::map<GW::UI::WindowID, const wchar_t*> gw_windows = {
        {GW::UI::WindowID_Compass,L"compass"},
        {GW::UI::WindowID_HealthBar,L"healthbar"},
        {GW::UI::WindowID_EnergyBar,L"energybar"},
        {GW::UI::WindowID_ExperienceBar,L"experiencebar"},
        {GW::UI::WindowID_Chat,L"chat"}
    };
    if (argc < 2)
        return GW::UI::WindowID_Count;
    const std::wstring arg = GuiUtils::ToLower(argv[1]);
    if (arg.size() && arg != L"all") {
        for (auto it : gw_windows) {
            if (wcscmp(it.second, arg.c_str()) == 0)
                return it.first;
        }
    }
    return GW::UI::WindowID_Count;
};

std::vector<ToolboxUIElement*> ChatCommands::MatchingWindows(const wchar_t *, int argc, LPWSTR *argv) {
    std::vector<ToolboxUIElement*> ret;
    if (argc <= 1) {
        ret.push_back(&MainWindow::Instance());
    } else {
        const std::wstring arg = GuiUtils::ToLower(argv[1]);
        if (arg == L"all") {
            for (ToolboxUIElement* window : GWToolbox::Instance().GetUIElements()) {
                ret.push_back(window);
            }
        } else if(arg.size()) {
            const std::string name = GuiUtils::WStringToString(arg);
            for (ToolboxUIElement* window : GWToolbox::Instance().GetUIElements()) {
                if (GuiUtils::ToLower(window->Name()).find(name) == 0){
                    ret.push_back(window);
                }
            }
        }
    }
    return ret;
}

void ChatCommands::CmdShow(const wchar_t *message, int , LPWSTR *) {
    std::wstring cmd = L"toggle ";
    cmd.append(GetRemainingArgsWstr(message, 1));
    cmd.append(L" on");
    GW::Chat::SendChat('/', cmd.c_str());
}
void ChatCommands::CmdHide(const wchar_t* message, int, LPWSTR*) {
    std::wstring cmd = L"toggle ";
    cmd.append(GetRemainingArgsWstr(message, 1));
    cmd.append(L" off");
    GW::Chat::SendChat('/', cmd.c_str());
}
void ChatCommands::CmdToggle(const wchar_t* message, int argc, LPWSTR* argv) {
    if (argc < 2) {
        Log::ErrorW(L"Invalid syntax: %s", message);
        return;
    }
    std::wstring last_arg = GuiUtils::ToLower(argv[argc - 1]);
    bool ignore_last_arg = false;
    enum ActionType : uint8_t {
        Toggle,
        On,
        Off
    } action = Toggle;
    if (last_arg == L"on" || last_arg == L"1" || last_arg == L"show") {
        action = On;
        ignore_last_arg = true;
    }
    else if (last_arg == L"off" || last_arg == L"0" || last_arg == L"hide") {
        action = Off;
        ignore_last_arg = true;
    }
    std::wstring second_arg = GuiUtils::ToLower(argv[1]);

    GW::Constants::EquipmentStatus (*statusGetter)() = nullptr;
    void (*statusSetter)(GW::Constants::EquipmentStatus) = nullptr;
    if (second_arg == L"cape") {
        statusGetter = &GW::Items::GetCapeStatus;
        statusSetter = &GW::Items::SetCapeStatus;
    }
    else if (second_arg == L"head" || second_arg == L"helm") {
        statusGetter = &GW::Items::GetHelmStatus;
        statusSetter = &GW::Items::SetHelmStatus;
    }
    else if (second_arg == L"costume_head") {
        statusGetter = &GW::Items::GetCostumeHeadpieceStatus;
        statusSetter = &GW::Items::SetCostumeHeadpieceStatus;
    }
    else if (second_arg == L"costume") {
        statusGetter = &GW::Items::GetCostumeBodyStatus;
        statusSetter = &GW::Items::SetCostumeBodyStatus;
    }
    if (statusSetter) {
        // Toggling visibility of equipment
        switch (action) {
        case On:
            return statusSetter(GW::Constants::EquipmentStatus::AlwaysShow);
        case Off:
            return statusSetter(GW::Constants::EquipmentStatus::AlwaysHide);
        default:
            GW::Constants::EquipmentStatus current = statusGetter();
            if (current == GW::Constants::EquipmentStatus::AlwaysShow)
                return statusSetter(GW::Constants::EquipmentStatus::AlwaysHide);
            return statusSetter(GW::Constants::EquipmentStatus::AlwaysShow);
        }
    }
    std::vector<ToolboxUIElement*> windows = MatchingWindows(message, ignore_last_arg ? argc - 1 : argc, argv);
    /*if (windows.empty()) {
        Log::Error("Cannot find window or command '%ls'", argc > 1 ? argv[1] : L"");
        return;
    }*/
    for (ToolboxUIElement* window : windows) {
        switch (action) {
        case On:
            window->visible = true;
            break;
        case Off:
            window->visible = false;
            break;
        default:
            window->visible = !window->visible;
            break;
        }
    }
    GW::UI::WindowID gw_window = MatchingGWWindow(message, ignore_last_arg ? argc - 1 : argc, argv);
    if (gw_window < GW::UI::WindowID_Count) {
        bool set = true;
        switch (action) {
        case Off:
            set = false;
            break;
        case Toggle:
            set = !GW::UI::GetWindowPosition(gw_window)->visible();
            break;
        }
        GW::UI::SetWindowVisible(gw_window, set);
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
        const std::wstring arg1 = GuiUtils::ToLower(argv[1]);
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
                const std::wstring arg2 = GuiUtils::ToLower(argv[2]);
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


void ChatCommands::CmdObserverReset(const wchar_t* message, int argc, LPWSTR* argv) {
    UNREFERENCED_PARAMETER(message);
    UNREFERENCED_PARAMETER(argc);
    UNREFERENCED_PARAMETER(argv);
    ObserverModule::Instance().Reset();
}


void ChatCommands::CmdDamage(const wchar_t *message, int argc, LPWSTR *argv) {
    UNREFERENCED_PARAMETER(message);
    if (argc <= 1) {
        PartyDamage::Instance().WritePartyDamage();
    } else {
        const std::wstring arg1 = GuiUtils::ToLower(argv[1]);
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

const wchar_t* ChatCommands::GetRemainingArgsWstr(const wchar_t* message, int argc_start) {
    const wchar_t* out = message;
    for (int i = 0; i < argc_start && out; i++) {
        out = wcschr(out, ' ');
        if (out) out++;
    }
    return out;
};
void ChatCommands::CmdTarget(const wchar_t *message, int argc, LPWSTR *argv) {
    if (argc < 2)
        return Log::ErrorW(L"Missing argument for /%s", argv[0]);

    const std::wstring arg1 = GuiUtils::ToLower(argv[1]);
    if (arg1 == L"ee")
        return TargetEE();
    if (arg1 == L"vipers" || arg1 == L"hos")
        return TargetVipers();
    if (IsNearestStr(arg1.c_str()))
        return TargetNearest(L"0", Living);
    if (arg1 == L"getid") {
        const GW::AgentLiving* const target = GW::Agents::GetTargetAsAgentLiving();
        if (target == nullptr) {
            Log::Error("No target selected!");
        } else {
            Log::Info("Target model id (PlayerNumber) is %d", target->player_number);
        }
        return;
    } 
    if (arg1 == L"getpos") {
        const GW::AgentLiving* const target = GW::Agents::GetTargetAsAgentLiving();
        if (target == nullptr) {
            Log::Error("No target selected!");
        } else {
            Log::Info("Target coordinates are (%f, %f)", target->pos.x, target->pos.y);
        }
        return;
    }
    if (arg1 == L"item") {
        if (argc < 3)
            return Log::ErrorW(L"Syntax: /%s item [model_id|name]",argv[0]);
        return TargetNearest(GetRemainingArgsWstr(message, 2),Item);
    }
    if (arg1 == L"priority") {
        const GW::PartyInfo* party = GW::PartyMgr::GetPartyInfo();
        if (!party || !party->players.valid()) return;

        uint32_t calledTargetId = 0;

        if (argc == 2) {
            const GW::Agent* me = GW::Agents::GetPlayer();
            if (me == nullptr) return;
            const GW::AgentLiving* meLiving = me->GetAsAgentLiving();
            if (!meLiving) return;
            for (size_t i = 0; i < party->players.size(); ++i) {
                if (party->players[i].login_number != meLiving->login_number) continue;
                calledTargetId = party->players[i].calledTargetId;
                break;
            }
        } else {
            if (!party->heroes.valid()) return;
            uint32_t partyMemberNumber = 0;
            uint32_t partySize = party->players.size() + party->heroes.size();

            if (!GuiUtils::ParseUInt(argv[2], &partyMemberNumber) || partyMemberNumber <= 0 ||
                partyMemberNumber > partySize) {
                Log::Error("Invalid argument '%ls', please use an integer value of 1 to %u", argv[2], partySize);
                return;
            }

            uint32_t count = 0;
            for (const GW::PlayerPartyMember& player : party->players) {
                count++;
                if (partyMemberNumber == count) {
                    calledTargetId = player.calledTargetId;
                    break;
                }
                for (const GW::HeroPartyMember& hero : party->heroes) {
                    if (hero.owner_player_id == player.login_number) {
                        count++;
                    }
                }
                if (count > partyMemberNumber) {
                    return;
                }
            }
        }
        if (calledTargetId == 0) return;
        GW::Agent* agent = GW::Agents::GetAgentByID(calledTargetId);
        if (!agent) return;
        GW::Agents::ChangeTarget(agent);
    }
    if (arg1 == L"npc") {
        if (argc < 3)
            return Log::ErrorW(L"Syntax: /%s npc [npc_id|name]", argv[0]);
        return TargetNearest(GetRemainingArgsWstr(message, 2), Npc);
    }
    if (arg1 == L"gadget") {
        if (argc < 3)
            return Log::ErrorW(L"Syntax: /%s gadget [gadget_id|name]", argv[0]);
        return TargetNearest(GetRemainingArgsWstr(message, 2), Gadget);
    }
    if (arg1 == L"player") {
        if (argc < 3)
            return Log::ErrorW(L"Syntax: /%s player [player_number|name]", argv[0]);
        return TargetNearest(GetRemainingArgsWstr(message, 2), Player);
    }
    return TargetNearest(GetRemainingArgsWstr(message, 1), Npc);
}

void ChatCommands::CmdUseSkill(const wchar_t *, int argc, LPWSTR *argv) {
    if (!IsMapReady())
        return;
    SkillToUse& skill_to_use = Instance().skill_to_use;
    skill_to_use.slot = 0;
    if (argc < 2)
        return;
    const std::wstring arg1 = GuiUtils::ToLower(argv[1]);
    if (arg1 == L"stop" || arg1 == L"off" || arg1 == L"0")
        return; // do nothing, already cleared skills_to_use
    uint32_t num = 0;
    if (!GuiUtils::ParseUInt(argv[1], &num) || num < 1 || num > 8) {
        Log::Error("Invalid argument '%ls', please use an integer value of 1 to 8", argv[1]);
        return;
    }
    skill_to_use.slot = num;
    skill_to_use.skill_usage_delay = .0f;
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

void ChatCommands::CmdLoad(const wchar_t* message, int argc, LPWSTR* argv)
{
    UNREFERENCED_PARAMETER(message);
    // We will & should move that to GWCA.
    static int(__cdecl * GetPersonalDir)(size_t size, wchar_t * dir) = 0;
    *(uintptr_t*)&GetPersonalDir = GW::MemoryMgr::GetPersonalDirPtr;
    if (argc == 1) {
        return;
    }

    constexpr size_t dir_size = 512;
    constexpr size_t temp_size = 64;

    const LPWSTR arg1 = argv[1];
    wchar_t dir[dir_size];
    GetPersonalDir(dir_size, dir); // @Fix, GetPersonalDir failed on Windows7, return path without slashes
    wcscat_s(dir, L"/GUILD WARS/Templates/Skills/");
    wcscat_s(dir, arg1);
    wcscat_s(dir, L".txt");

    char temp[temp_size];
    if (!ReadTemplateFile(dir, temp, temp_size)) {
        // If it failed, we will interpret the input as the code models.
        const size_t len = wcslen(arg1);
        if (len >= temp_size) return;
        for (size_t i = 0; i < len; i++)
            temp[i] = static_cast<char>(arg1[i]);
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

void ChatCommands::CmdPing(const wchar_t* message, int argc, LPWSTR* argv) {
    UNREFERENCED_PARAMETER(message);
    // We will & should move that to GWCA.
    static int(__cdecl * GetPersonalDir)(size_t size, wchar_t * dir) = 0;
    *(uintptr_t*)&GetPersonalDir = GW::MemoryMgr::GetPersonalDirPtr;
    if (argc < 2) {
        return;
    }

    constexpr size_t dir_size = 512;
    constexpr size_t temp_size = 64;
    constexpr size_t template_size = 256;

    for (int arg_idx = 1; arg_idx < argc; arg_idx++) {
        const LPWSTR arg = argv[arg_idx];
        wchar_t dir[dir_size];
        GetPersonalDir(dir_size, dir); // @Fix, GetPersonalDir failed on Windows7, return path without slashes
        wcscat_s(dir, L"/GUILD WARS/Templates/Skills/");
        wcscat_s(dir, arg);
        wcscat_s(dir, L".txt");

        char temp[temp_size];
        if (!ReadTemplateFile(dir, temp, temp_size)) {
            // If it failed, we will interpret the input as the code models.
            const size_t len = wcslen(arg);
            if (len >= temp_size) continue;
            for (size_t i = 0; i < len; i++)
                temp[i] = static_cast<char>(arg[i]);
            temp[len] = 0;
        }

        // If template file does not exist, skip
        GW::SkillbarMgr::SkillTemplate skill_template;
        if (!GW::SkillbarMgr::DecodeSkillTemplate(&skill_template, temp)) {
            continue;
        }

        char template_code[template_size];
        snprintf(template_code, template_size, "[%S;%s]", arg, temp);
        GW::Chat::SendChat('#', template_code);
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
    const std::wstring arg1 = GuiUtils::ToLower(argv[1]);
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
        const GW::Player* const player = GW::PlayerMgr::GetPlayerByID(p.login_number);
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
    
    const GW::AgentLiving* const target = GW::Agents::GetTargetAsAgentLiving();
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
    const GW::AgentLiving* const target = GW::Agents::GetTargetAsAgentLiving();
    if (!target)
        return false;
    transmo.npc_id = target->player_number;
    if (target->transmog_npc_id & 0x20000000)
        transmo.npc_id = target->transmog_npc_id ^ 0x20000000;
    else if (target->IsPlayer())
        return false;
    return true;
}

void ChatCommands::TargetNearest(const wchar_t* model_id_or_name, TargetType type)
{
    uint32_t model_id = 0;
    uint32_t index = 0; // 0=nearest. 1=first by id, 2=second by id, etc.

    // Searching by name; offload this to decode agent names first.
    if (GuiUtils::ParseUInt(model_id_or_name, &model_id)) {
        // check if there's an index component
        if (const wchar_t* rest = GetRemainingArgsWstr(model_id_or_name, 1)) {
            GuiUtils::ParseUInt(rest, &index);
        }

    } else {
        if (!IsNearestStr(model_id_or_name)) {
            Instance().npc_to_find.Init(model_id_or_name, type);
            return;
        }
    }

    // target nearest agent
    const GW::AgentArray agents = GW::Agents::GetAgentArray();
    if (!agents.valid())
        return;

    const GW::AgentLiving* const me = GW::Agents::GetPlayerAsAgentLiving();
    if (me == nullptr)
        return;

    float distance = GW::Constants::SqrRange::Compass;
    size_t closest = 0;
    size_t count = 0;

    for (const GW::Agent * agent : agents) {
        if (!agent || agent == me)
            continue;
        switch (type) {
            case Gadget: {
                // Target gadget by gadget id
                const GW::AgentGadget* const gadget = agent->GetAsAgentGadget();
                if (!gadget || (model_id && gadget->gadget_id != model_id))
                    continue;
            } break;
            case Item: {
                // Target item by model id
                const GW::AgentItem* const item_agent = agent->GetAsAgentItem();
                if (!item_agent)
                    continue;
                const GW::Item* const item = GW::Items::GetItemById(item_agent->item_id);
                if (!item || (model_id && item->model_id != model_id))
                    continue;
            } break;
            case Npc: {
                // Target npc by model id
                const GW::AgentLiving* const living_agent = agent->GetAsAgentLiving();
                if (!living_agent || !living_agent->IsNPC() || !living_agent->GetIsAlive() || (model_id && living_agent->player_number != model_id))
                    continue;
            } break;
            case Player: {
                // Target player by player number
                const GW::AgentLiving* const living_agent = agent->GetAsAgentLiving();
                if (!living_agent || !living_agent->IsPlayer() || (model_id && living_agent->player_number != model_id))
                    continue;
            } break;
            case Living: {
                // Target any living agent by model id
                const GW::AgentLiving* const living_agent = agent->GetAsAgentLiving();
                if (!living_agent || !living_agent->GetIsAlive() || (model_id && living_agent->player_number != model_id))
                    continue;
            } break;
            default:
                continue;
        }
        if (index == 0) { // target closest
            const float newDistance = GW::GetSquareDistance(me->pos, agent->pos);
            if (newDistance < distance) {
                closest = agent->agent_id;
                distance = newDistance;
            }
        } else { // target based on id
            ++count;
            if (count == index) {
                closest = agent->agent_id;
                break;
            }
        }
    }
    if(closest) GW::Agents::ChangeTarget(closest);
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
    case GW::Constants::MapID::The_Sulfurous_Wastes:
    case GW::Constants::MapID::Jokos_Domain:
    case GW::Constants::MapID::The_Alkali_Pan:
    case GW::Constants::MapID::Crystal_Overlook:
    case GW::Constants::MapID::The_Ruptured_Heart:
    case GW::Constants::MapID::Poisoned_Outcrops:
    case GW::Constants::MapID::The_Shattered_Ravines:
    case GW::Constants::MapID::Gate_of_Desolation:
    case GW::Constants::MapID::Jennurs_Horde:
    case GW::Constants::MapID::Nundu_Bay:
    case GW::Constants::MapID::Grand_Court_of_Sebelkeh:
    case GW::Constants::MapID::Dzagonur_Bastion:
    case GW::Constants::MapID::Ruins_of_Morah:
    case GW::Constants::MapID::The_Mouth_of_Torment_outpost:
    case GW::Constants::MapID::Lair_of_the_Forgotten_outpost:
    case GW::Constants::MapID::Bone_Palace_outpost:
    case GW::Constants::MapID::Basalt_Grotto_outpost:
    case GW::Constants::MapID::Gate_of_Torment_outpost:
    case GW::Constants::MapID::Gate_of_Fear_outpost:
    case GW::Constants::MapID::Gate_of_Secrets_outpost:
    case GW::Constants::MapID::Gate_of_the_Nightfallen_Lands_outpost:
    case GW::Constants::MapID::The_Shadow_Nexus:
    case GW::Constants::MapID::Gate_of_Pain:
    case GW::Constants::MapID::Gate_of_Madness:
    case GW::Constants::MapID::Abaddons_Gate:
    case GW::Constants::MapID::Depths_of_Madness:
    case GW::Constants::MapID::Domain_of_Anguish:
    case GW::Constants::MapID::Domain_of_Fear:
    case GW::Constants::MapID::Domain_of_Pain:
    case GW::Constants::MapID::Domain_of_Secrets:
    case GW::Constants::MapID::Heart_of_Abaddon:
    case GW::Constants::MapID::Nightfallen_Garden:
    case GW::Constants::MapID::Nightfallen_Jahai:
    case GW::Constants::MapID::Throne_of_Secrets:
        GW::PlayerMgr::SetActiveTitle(GW::Constants::TitleID::Lightbringer);
        break;
    default: return;
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
    const std::wstring arg1 = GuiUtils::ToLower(argv[1]);
    if (arg1 == L"avoid") {
        behaviour = 2; // avoid combat
    } else if (arg1 == L"guard") {
        behaviour = 1; // guard
    } else if (arg1 == L"attack") {
        behaviour = 0; // attack
    } else {
        return Log::Error("Invalid argument for /hero. It can be one of: avoid | guard | attack");
    }

    const GW::PartyInfo* const party_info = GW::PartyMgr::GetPartyInfo();
    if (!party_info)
        return Log::Error("Could not retrieve party info");
    const GW::HeroPartyMemberArray& party_heros = party_info->heroes;
    if (!party_heros.valid())
        return Log::Error("Party heroes validation failed");
    const GW::AgentLiving* const me = GW::Agents::GetPlayerAsAgentLiving();
    if (!me)
        return Log::Error("Failed to get player");

    //execute in explorable area or outpost
    for (const GW::HeroPartyMember& hero : party_heros) {
        if (hero.owner_player_id == me->login_number) {
            GW::CtoS::SendPacket(0xC, GAME_CMSG_HERO_BEHAVIOR, hero.agent_id, behaviour);
        }
    }
}
