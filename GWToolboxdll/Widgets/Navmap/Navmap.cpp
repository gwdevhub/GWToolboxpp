#include "stdafx.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/Constants/Skills.h>

#include <GWCA/GameContainers/Array.h>
#include <GWCA/GameContainers/GamePos.h>
#include <GWCA/GameEntities/Camera.h>
#include <GWCA/Packets/StoC.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Hero.h>
#include <GWCA/GameEntities/Party.h>
#include <GWCA/GameEntities/Quest.h>
#include <GWCA/GameEntities/Skill.h>

#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/PartyContext.h>
#include <GWCA/Context/WorldContext.h>

#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/CameraMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/EffectMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/PlayerMgr.h>
#include <GWCA/Managers/QuestMgr.h>
#include <GWCA/Managers/StoCMgr.h>

#include <GWCA/Utilities/Hooker.h>
#include <GWCA/Utilities/MemoryPatcher.h>
#include <GWCA/Utilities/Scanner.h>
#include <ImGuiAddons.h>
#include <Logger.h>
#include <Utils/GuiUtils.h>

#include "Navmap.h"
#include <Defines.h>
#include <Modules/Resources.h>

#include <Windows/MainWindow.h>

#include <Timer.h>

namespace {
    DirectX::XMFLOAT2 gwinch_scale;

    //enum FlaggingState : uint32_t {
        //FlagState_All = 0,
        //FlagState_Hero1,
        //FlagState_Hero2,
        //FlagState_Hero3,
        //FlagState_Hero4,
        //FlagState_Hero5,
        //FlagState_Hero6,
        //FlagState_Hero7,
        //FlagState_None
    //};

    enum CaptureMouseClickType : uint32_t {
        CaptureType_None [[maybe_unused]]                = 0,
        CaptureType_SetDest [[maybe_unused]]            = 1
        //CaptureType_SalvageWithUpgrades [[maybe_unused]] = 11,
        //CaptureType_SalvageMaterials [[maybe_unused]]    = 12,
        //CaptureType_Idenfify [[maybe_unused]]            = 13
    };

    /*struct MouseClickCaptureData {
        struct sub1 {
            uint8_t pad0[0x3C];

            struct sub2 {
                uint8_t pad1[0x14];

                struct sub3 {
                    uint8_t pad2[0x24];

                    struct sub4 {
                        uint8_t pad3[0x2C];

                        struct sub5 {
                            uint8_t pad4[0x4];
                            void* flagging_hero;
                        } * sub5;
                    } * sub4;
                } * sub3;
            } * sub2;
        } * sub1;
    }* MouseClickCaptureDataPtr = nullptr;*/

    uint32_t* GameCursorState = nullptr;
    CaptureMouseClickType* CaptureMouseClickTypePtr = nullptr;

    //FlaggingState GetFlaggingState()
    //{
        //if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Explorable) {
            //return FlagState_None;
        //}
        //if (!CaptureMouseClickTypePtr || *CaptureMouseClickTypePtr != CaptureType_FlagHero || !MouseClickCaptureDataPtr || !MouseClickCaptureDataPtr->sub1) {
            //return FlagState_None;
        //}
        //return *MouseClickCaptureDataPtr->sub1->sub2->sub3->sub4->sub5->flagging_hero;
    //}

    //bool compass_fix_pending = false;

    //bool SetFlaggingState(FlaggingState set_state)
    //{
    //    if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Explorable) {
    //        return false;
    //    }
    //    // keep an internal flag for the navmap flagging until StartCaptureMouseClick_Func is working
    //    // navmap_flagging_state = set_state;
    //    if (GetFlaggingState() == set_state) {
    //        return true;
    //    }
    //    if (set_state == FlagState_None) {
    //        set_state = GetFlaggingState();
    //    }
    //    GW::UI::ControlAction key;
    //    switch (set_state) {
    //        case FlagState_Hero1:
    //            key = GW::UI::ControlAction_CommandHero1;
    //            break;
    //        case FlagState_Hero2:
    //            key = GW::UI::ControlAction_CommandHero2;
    //            break;
    //        case FlagState_Hero3:
    //            key = GW::UI::ControlAction_CommandHero3;
    //            break;
    //        case FlagState_Hero4:
    //            key = GW::UI::ControlAction_CommandHero4;
    //            break;
    //        case FlagState_Hero5:
    //            key = GW::UI::ControlAction_CommandHero5;
    //            break;
    //        case FlagState_Hero6:
    //            key = GW::UI::ControlAction_CommandHero6;
    //            break;
    //        case FlagState_Hero7:
    //            key = GW::UI::ControlAction_CommandHero7;
    //            break;
    //        case FlagState_All:
    //            key = GW::UI::ControlAction_CommandParty;
    //            break;
    //        default:
    //            return false;
    //    }
    //    return Keypress(key);
    //}

    // Same as GW::PartyMgr::GetPlayerIsLeader() but has an extra check to ignore disconnected people.
    /*bool GetPlayerIsLeader()
    {
        GW::PartyInfo* party = GW::PartyMgr::GetPartyInfo();
        if (!party || !party->players.size()) {
            return false;
        }
        const uint32_t player_id = GW::PlayerMgr::GetPlayerNumber();
        for (auto& player : party->players) {
            if (!player.connected()) {
                continue;
            }
            return player.login_number == player_id;
        }
        return false;
    }

    GW::PartyInfo* GetPlayerParty()
    {
        const GW::GameContext* gamectx = GW::GetGameContext();
        if (!(gamectx && gamectx->party)) {
            return nullptr;
        }
        return gamectx->party->player_party;
    }

    GW::UI::WindowPosition* compass_frame = nullptr;

    using DrawCompassAgentsByType_pt = uint32_t(__fastcall*)(void* ecx, void* edx, uint32_t param_1, uint32_t param_2, uint32_t flags);
    DrawCompassAgentsByType_pt DrawCompassAgentsByType_Func = nullptr;
    DrawCompassAgentsByType_pt DrawCompassAgentsByType_Ret = nullptr;

    bool hide_compass_agents = false;

    uint32_t __fastcall OnDrawCompassAgentsByType(void* ecx, void* edx, const uint32_t param_1, const uint32_t param_2, const uint32_t flags)
    {
        GW::Hook::EnterHook();
        uint32_t result = 0;
        if (!hide_compass_agents) {
            result = DrawCompassAgentsByType_Ret(ecx, edx, param_1, param_2, flags);
        }
        GW::Hook::LeaveHook();
        return result;
    }*/

    bool hide_compass_quest_marker = false;
    GW::MemoryPatcher show_compass_quest_marker_patch;

    void ToggleCompassQuestMarker(const bool enable)
    {
        if (enable == show_compass_quest_marker_patch.GetIsEnable()) {
            return;
        }
        show_compass_quest_marker_patch.TogglePatch(enable);
        GW::GameThread::Enqueue([] {
            if (const auto quest = GW::QuestMgr::GetActiveQuest()) {
                struct QuestUIMsg {
                    GW::Constants::QuestID quest_id{};
                    GW::Vec3f marker{};
                    uint32_t h0024{};
                    GW::Constants::MapID map_to{};
                    uint32_t log_state{};
                } msg;
                msg.quest_id = quest->quest_id;
                msg.marker = quest->marker;
                msg.h0024 = quest->h0024;
                msg.map_to = quest->map_to;
                msg.log_state = quest->log_state;

                SendUIMessage(GW::UI::UIMessage::kClientActiveQuestChanged, &msg);
            }
        });
    }
}

void Navmap::DrawHelp()
{
    if (!ImGui::TreeNodeEx("Navmap", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
        return;
    }

    ImGui::Bullet();
    ImGui::Text("'/marktarget' highlights the current target on your navmap.");
    ImGui::Bullet();
    ImGui::Text("'/marktarget clear' removes the current target as a marked target on your navmap.");
    ImGui::Bullet();
    ImGui::Text("'/clearmarktarget clear' removes all marked target highlights on your navmap.");
    ImGui::TreePop();
}

void Navmap::Terminate()
{
    ToolboxWidget::Terminate();
    //range_renderer.Terminate();
    pmap_renderer.Terminate();
    //agent_renderer.Terminate();
    //pingslines_renderer.Terminate();
    //marker_renderer.Terminate();
    //custom_renderer.Terminate();
    //effect_renderer.Terminate();
    //GameWorldRenderer::Terminate();
}

void Navmap::Initialize()
{
    ToolboxWidget::Initialize();

    uintptr_t address = GW::Scanner::Find("\x00\x74\x16\x6A\x27\x68\x80\x00\x00\x00\x6A\x00\x68", "xxxxxxxxxxxxx", -0x51);
    if (address) {
        address = *(uintptr_t*)address;
        //MouseClickCaptureDataPtr = (MouseClickCaptureData*)address;
        GameCursorState = (uint32_t*)(address + 0x4);
        CaptureMouseClickTypePtr = (CaptureMouseClickType*)(address - 0x10);
    }
    Log::Log("[SCAN] CaptureMouseClickTypePtr = %p\n", CaptureMouseClickTypePtr);
    //Log::Log("[SCAN] MouseClickCaptureDataPtr = %p\n", MouseClickCaptureDataPtr);

    //DrawCompassAgentsByType_Func = (DrawCompassAgentsByType_pt)GW::Scanner::Find("\x8b\x46\x08\x8d\x5e\x18\x53", "xxxxxxx", -0xb);
    //GW::HookBase::CreateHook(DrawCompassAgentsByType_Func, OnDrawCompassAgentsByType, (void**)&DrawCompassAgentsByType_Ret);
    //GW::HookBase::EnableHooks(DrawCompassAgentsByType_Func);

    address = GW::Scanner::Find("\xdd\xd8\x6a\x01\x52", "xxxxx");
    if (address) {
        show_compass_quest_marker_patch.SetPatch(address, "\xEB\xEC", 2);
    }
    ToggleCompassQuestMarker(hide_compass_quest_marker);

    /*GW::UI::RegisterKeydownCallback(&CamRev_Entry1, [this](GW::HookStatus*, const uint32_t key) {
        if (key != GW::UI::ControlAction_ReverseCamera) {
            return;
        }
        camera_currently_reversed = true;
    });
    GW::UI::RegisterKeyupCallback(&CamRev_Entry2, [this](GW::HookStatus*, const uint32_t key) {
        if (key != GW::UI::ControlAction_ReverseCamera) {
            return;
        }
        camera_currently_reversed = false;
    });*/

    //GW::StoC::RegisterPacketCallback<GW::Packet::StoC::AgentPinged>(&AgentPinged_Entry, [this](GW::HookStatus*, const GW::Packet::StoC::AgentPinged* pak) -> void {
        //if (visible) {
            //pingslines_renderer.P046Callback(pak);
        //}
    //});
    //GW::StoC::RegisterPacketCallback<GW::Packet::StoC::CompassEvent>(&CompassEvent_Entry, [this](GW::HookStatus*, const GW::Packet::StoC::CompassEvent* pak) -> void {
        //if (visible) {
            //pingslines_renderer.P138Callback(pak);
        //}
    //});
    //GW::StoC::RegisterPacketCallback<GW::Packet::StoC::PlayEffect>(&CompassEvent_Entry, [this](const GW::HookStatus*, GW::Packet::StoC::PlayEffect* pak) -> void {
        //if (visible) {
            //if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable) {
                //effect_renderer.PacketCallback(pak);
            //}
        //}
    //});
    //GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GenericValue>(&GenericValueTarget_Entry, [this](const GW::HookStatus*, const GW::Packet::StoC::GenericValue* pak) -> void {
        //if (visible) {
            //if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable) {
                //effect_renderer.PacketCallback(pak);
            //}
        //}
    //});
    //GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GenericValueTarget>(&GenericValueTarget_Entry, [this](GW::HookStatus*, const GW::Packet::StoC::GenericValueTarget* pak) -> void {
        //if (visible) {
            //pingslines_renderer.P153Callback(pak);
            //if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable) {
                //effect_renderer.PacketCallback(pak);
            //}
        //}
    //});
    constexpr GW::UI::UIMessage hook_messages[] = {
        GW::UI::UIMessage::kMapChange,
        GW::UI::UIMessage::kMapLoaded
        //GW::UI::UIMessage::kChangeTarget,
        //GW::UI::UIMessage::kSkillActivated
    };
    for (const auto message_id : hook_messages) {
        RegisterUIMessageCallback(&UIMsg_Entry, message_id, OnUIMessage);
    }

    last_moved = TIMER_INIT();

    pmap_renderer.Invalidate();

    //GW::Chat::CreateCommand(L"flag", &OnFlagHeroCmd);
}

void Navmap::OnUIMessage(GW::HookStatus*, const GW::UI::UIMessage msgid, void* wParam, void*)
{
    (void)wParam;

    auto& instance = Instance();
    switch (msgid) {
        case GW::UI::UIMessage::kMapLoaded: {
            instance.pmap_renderer.Invalidate();
            instance.loading = false;
            // Compass fix to allow hero flagging controls
            //const GW::UI::WindowPosition* compass_info = GetWindowPosition(GW::UI::WindowID_Compass);
            //if (compass_info && !compass_info->visible()) {
                // Note: Wait for a frame to pass before toggling off again to allow the game to initialise the window.
                //compass_fix_pending = true;
                //SetWindowVisible(GW::UI::WindowID_Compass, true);
            //}
            instance.is_observing = GW::Map::GetIsObserving();
        }
        break;
        /*case GW::UI::UIMessage::kSkillActivated: {
            const struct Payload {
                uint32_t agent_id;
                GW::Constants::SkillID skill_id;
            }* payload = static_cast<Payload*>(wParam);
            if (payload->agent_id == GW::Agents::GetPlayerId()) {
                if (payload->skill_id == GW::Constants::SkillID::Shadow_of_Haste || payload->skill_id == GW::Constants::SkillID::Shadow_Walk) {
                    instance.shadowstep_location = GW::Agents::GetPlayer()->pos;
                }
            }
        }*/
        //break;
        case GW::UI::UIMessage::kMapChange: {
            instance.loading = true;
            //instance.agent_renderer.auto_target_id = 0;
        }
        break;
        /*case GW::UI::UIMessage::kChangeTarget: {
            const GW::UI::ChangeTargetUIMsg* msg = static_cast<GW::UI::ChangeTargetUIMsg*>(wParam);
            instance.agent_renderer.auto_target_id = GW::Agents::GetTargetId() ? 0 : msg->auto_target_id;
        }
        break;*/
        default:
            break;
    }
}

//GW::Vec2f Navmap::ShadowstepLocation() const
//{
//    return shadowstep_location;
//}

//void Navmap::OnFlagHeroCmd(const wchar_t*, const int argc, const LPWSTR* argv)
//{
//    if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Explorable) {
//        return; // Not explorable - "/flag" can be typed in chat to bypass flag hero buttons, so this is needed.
//    }
//    if (argc <= 1) {
//        FlagHero(0); // "/flag"
//        return;
//    }
//    const auto is_flagged = [](auto hero) -> bool {
//        if (hero == 0) {
//            const GW::Vec3f& allflag = GW::GetGameContext()->world->all_flag;
//            if (allflag.x != 0 && allflag.y != 0 && (!std::isinf(allflag.x) || !std::isinf(allflag.y))) {
//                return true;
//            }
//        }
//        else {
//            const GW::HeroFlagArray& flags = GW::GetGameContext()->world->hero_flags;
//            if (!flags.valid() || static_cast<uint32_t>(hero) > flags.size()) {
//                return false;
//            }
//
//            const GW::HeroFlag& flag = flags[hero - 1];
//            if (!std::isinf(flag.flag.x) || !std::isinf(flag.flag.y)) {
//                return true;
//            }
//        }
//        return false;
//    };
//
//    const std::wstring arg1 = GuiUtils::ToLower(argv[1]);
//    float x;
//    float y;
//    unsigned int n_heros = 0; // Count of heros available
//    unsigned int f_hero = 0;  // Hero number to flag
//    if (arg1 == L"all" || arg1 == L"0") {
//        if (argc < 3) {
//            FlagHero(0); // "/flag all" == "/flag"
//            return;
//        }
//        const std::wstring arg2 = GuiUtils::ToLower(argv[2]);
//        if (arg2 == L"clear") {
//            GW::PartyMgr::UnflagAll(); // "/flag 5 clear"
//            return;
//        }
//        if (arg2 == L"toggle") {
//            // "/flag 5 toggle"
//            if (is_flagged(0)) {
//                GW::PartyMgr::UnflagAll();
//            }
//            else {
//                FlagHero(0);
//            }
//            return;
//        }
//        if (argc < 4 || !GuiUtils::ParseFloat(argv[2], &x) || !GuiUtils::ParseFloat(argv[3], &y)) {
//            Log::Error("Please provide command in format /flag all [x] [y]"); // Not enough args or coords not valid float vals.
//            return;
//        }
//        GW::PartyMgr::FlagAll(GW::GamePos(x, y, 0)); // "/flag all -2913.41 3004.78"
//        return;
//    }
//    const auto& heroarray = GW::GetGameContext()->party->player_party->heroes;
//
//    if (heroarray.valid()) {
//        n_heros = heroarray.size();
//    }
//    if (n_heros < 1) {
//        return; // Player has no heroes, so no need to continue.
//    }
//    if (arg1 == L"clear") {
//        for (unsigned int i = 1; i <= n_heros; ++i) {
//            GW::PartyMgr::UnflagHero(i);
//        }
//        GW::PartyMgr::UnflagAll(); // "/flag clear"
//        return;
//    }
//    if (!GuiUtils::ParseUInt(argv[1], &f_hero) || f_hero < 1 || f_hero > n_heros) {
//        Log::Error("Invalid hero number");
//        return; // Invalid hero number
//    }
//    if (argc < 3) {
//        FlagHero(f_hero); // "/flag 5"
//        return;
//    }
//    const std::wstring arg2 = GuiUtils::ToLower(argv[2]);
//    if (arg2 == L"clear") {
//        GW::PartyMgr::UnflagHero(f_hero); // "/flag 5 clear"
//        return;
//    }
//    if (arg2 == L"toggle") {
//        // "/flag 5 toggle"
//        if (is_flagged(f_hero)) {
//            GW::PartyMgr::UnflagHero(f_hero);
//        }
//        else {
//            FlagHero(f_hero);
//        }
//        return;
//    }
//    if (argc < 4 || !GuiUtils::ParseFloat(argv[2], &x) || !GuiUtils::ParseFloat(argv[3], &y)) {
//        Log::Error("Please provide command in format /flag [hero number] [x] [y]"); // Invalid coords
//        return;
//    }
//    GW::PartyMgr::FlagHeroAgent(GW::Agents::GetHeroAgentID(f_hero), GW::GamePos(x, y, 0)); // "/flag 5 -2913.41 3004.78"
//}

void Navmap::DrawSettingsInternal()
{
    constexpr auto navmap_modifier_behavior_combo_str = "Disabled\0Draw\0Target\0Move\0Walk\0\0";

    static ImVec4 red = ImGui::ColorConvertU32ToFloat4(Colors::Red());
    static ImVec4 blue = ImGui::ColorConvertU32ToFloat4(Colors::Blue());
    ImGui::TextColored(blue, "Info: It is recommended to create a new send chat hotkey that will toggle the navmap: tb Navmap toggle");
    ImGui::TextColored(red, "Warning: This is a beta feature and will render over your character, game props, and UI elements.");



    /*if (snap_to_compass) {
        ImGui::NextSpacedElement();
    }*/
    //ImGui::Checkbox("Snap to compass", &snap_to_compass);
    //ImGui::ShowHelp("Resize and position navmap to match in-game compass size and position.");
    //ImGui::Checkbox("Hide GW compass agents", &hide_compass_agents);
    if (ImGui::Checkbox("Hide GW compass quest marker", &hide_compass_quest_marker)) {
        ToggleCompassQuestMarker(hide_compass_quest_marker);
    }
    ImGui::ShowHelp("To disable the toolbox navmap quest marker, set the quest marker color to transparent in the Symbols section below.");

    is_movable = is_resizable = false;
    if (is_resizable) {
        ImVec2 winsize(100.0f, 100.0f);
        if (const auto window = ImGui::FindWindowByName(Name())) {
            winsize = window->Size; //1920
        }
        if (ImGui::DragFloat("Size", &winsize.x, 1.0f, 0.0f, 0.0f, "%.0f")) {
            winsize.y = winsize.x;
            ImGui::SetWindowSize(Name(), winsize);
        }
    }

    ImGui::Text("General");
    static float a = scale;
    if (ImGui::DragFloat("Scale", &a, 0.01f, 0.01f, 10.0f)) {
        scale = a;
    }
    //static float testx_ = testx;
    //static float testy_ = testy;
    //if (ImGui::DragFloat("testx", &testx_, 0.1f, -100000.0f, 100000.0f)) {
        //testx = testx_;
    //}
    //if (ImGui::DragFloat("testy", &testy_, 0.1f, -100000.0f, 100000.0f)) {
        //testy = testy_;
    //}
    ImGui::Text("You can set the color alpha to 0 to disable any navmap feature.");
    // agent_rendered has its own TreeNodes
    //agent_renderer.DrawSettings();
    if (ImGui::TreeNodeEx("Markers", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
        marker_renderer.DrawSettings();
        ImGui::TreePop();
    }
    /*if (ImGui::TreeNodeEx("Pings and drawings", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
        pingslines_renderer.DrawSettings();
        ImGui::TreePop();
    }
    if (ImGui::TreeNodeEx("AoE Effects", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
        NavEffectRenderer::DrawSettings();
        ImGui::TreePop();
    }*/
    //if (ImGui::TreeNodeEx("Symbols", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
        //symbols_renderer.DrawSettings();
        //ImGui::TreePop();
    //}
    if (ImGui::TreeNodeEx("Terrain", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
        pmap_renderer.DrawSettings();
        ImGui::TreePop();
    }
    /*custom_renderer.DrawSettings();
    if (ImGui::TreeNodeEx("Hero flagging", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
        ImGui::Checkbox("Show hero flag controls", &hero_flag_controls_show);
        ImGui::Checkbox("Attach to navmap", &hero_flag_window_attach);
        ImGui::ShowHelp("If disabled, you can move/resize the window with 'Unlock Move All'.");
        Colors::DrawSettingHueWheel("Background", &hero_flag_window_background);
        ImGui::TreePop();
    }*/
    if (ImGui::TreeNodeEx("In-game rendering", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
        game_world_renderer.DrawSettings();
        ImGui::TreePop();
    }
    ImGui::StartSpacedElements(300.f);
    ImGui::NextSpacedElement();
    ////ImGui::Checkbox("Show boss by profession color on navmap", &agent_renderer.boss_colors);
    //ImGui::NextSpacedElement();
    //ImGui::Checkbox("Show hidden NPCs", &agent_renderer.show_hidden_npcs);
    //ImGui::ShowHelp("Show NPCs that aren't usually visible on the navmap\ne.g. minipets, invisible NPCs");
    //ImGui::NextSpacedElement();
    //ImGui::Checkbox("Show symbol for quest NPCs", &agent_renderer.show_quest_npcs_on_navmap);
    //ImGui::ShowHelp("Show a star for NPCs that have quest progress available");

    //ImGui::SliderInt("Agent Border thickness", reinterpret_cast<int*>(&agent_renderer.agent_border_thickness), 0, 50);

    ImGui::Text("Allow mouse click-through in:");
    ImGui::Indent();
    ImGui::StartSpacedElements(200.f);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Explorable areas", &mouse_clickthrough_in_explorable);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Outposts", &mouse_clickthrough_in_outpost);

    ImGui::Unindent();
    ImGui::Text("Hold + Click modifiers");
    ImGui::ShowHelp("Define behaviour of holding keyboard keys and clicking the navmap.\n"
        "Draw: ping and draw on the compass.\n"
        "Target: click to target agent.\n"
        "Move: move the navmap outside of compass range.\n"
        "Walk: start walking character to selected location.\n");
    ImGui::Combo("None", reinterpret_cast<int*>(&key_none_behavior), navmap_modifier_behavior_combo_str);
    ImGui::Combo("Control", reinterpret_cast<int*>(&key_ctrl_behavior), navmap_modifier_behavior_combo_str);
    ImGui::Combo("Shift", reinterpret_cast<int*>(&key_shift_behavior), navmap_modifier_behavior_combo_str);
    ImGui::Combo("Alt", reinterpret_cast<int*>(&key_alt_behavior), navmap_modifier_behavior_combo_str);

    ImGui::StartSpacedElements(256.f);
    ImGui::NextSpacedElement();
    //ImGui::Checkbox("Reduce agent ping spam", &pingslines_renderer.reduce_ping_spam);
    ImGui::ShowHelp("Additional pings on the same agents will increase the duration of the existing ping, rather than create a new one.");
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Map Rotation", &rotate_navmap);
    ImGui::ShowHelp("Map rotation on (e.g. Compass), or off (e.g. Mission Map).");
    ImGui::NextSpacedElement();
    //ImGui::Checkbox("Flip when reversed", &flip_on_reverse);
    //ImGui::ShowHelp("Whether the navmap rotation should flip 180 degrees when you reverse your camera.");
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Map rotation smoothing", &smooth_rotation);
    ImGui::ShowHelp("Navmap rotation speed matches compass rotation speed.");
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Circular", &circular_map);
    ImGui::ShowHelp("Whether the map should be circular like the compass (default) or a square.");
}

void Navmap::LoadSettings(ToolboxIni* ini)
{
    ToolboxWidget::LoadSettings(ini);
    /*Resources::EnsureFileExists(
        Resources::GetPath(L"Markers.ini"),
        "https://raw.githubusercontent.com/HasKha/GWToolboxpp/master/resources/Markers.ini",
        [](const bool success, const std::wstring& error) {
            if (success) {
                Instance().custom_renderer.LoadMarkers();
            }
            else {
                Log::ErrorW(L"Failed to download Markers.ini\n%s", error.c_str());
            }
        });*/
    scale = static_cast<float>(ini->GetDoubleValue(Name(), VAR_NAME(scale), 1.0));
    //LOAD_BOOL(hero_flag_controls_show);
    //LOAD_BOOL(hero_flag_window_attach);
    //hero_flag_window_background = Colors::Load(ini, Name(), "hero_flag_controls_background", hero_flag_window_background);
    LOAD_BOOL(mouse_clickthrough_in_outpost);
    LOAD_BOOL(mouse_clickthrough_in_explorable);
    LOAD_BOOL(rotate_navmap);
    //LOAD_BOOL(flip_on_reverse);
    LOAD_BOOL(smooth_rotation);
    LOAD_BOOL(circular_map);
    //LOAD_BOOL(snap_to_compass);
    //LOAD_BOOL(hide_compass_agents);

    LOAD_BOOL(hide_compass_quest_marker);
    ToggleCompassQuestMarker(hide_compass_quest_marker);

    key_none_behavior = static_cast<NavmapModifierBehaviour>(ini->GetLongValue(Name(), VAR_NAME(key_none_behavior), 1));
    key_ctrl_behavior = static_cast<NavmapModifierBehaviour>(ini->GetLongValue(Name(), VAR_NAME(key_ctrl_behavior), 2));
    key_shift_behavior = static_cast<NavmapModifierBehaviour>(ini->GetLongValue(Name(), VAR_NAME(key_shift_behavior), 3));
    key_alt_behavior = static_cast<NavmapModifierBehaviour>(ini->GetLongValue(Name(), VAR_NAME(key_alt_behavior), 4));

    marker_renderer.LoadSettings(ini, Name());
    pmap_renderer.LoadSettings(ini, Name());
    //agent_renderer.LoadSettings(ini, Name());
    //pingslines_renderer.LoadSettings(ini, Name());
    //symbols_renderer.LoadSettings(ini, Name());
    //custom_renderer.LoadSettings(ini, Name());
    //effect_renderer.LoadSettings(ini, Name());
    game_world_renderer.LoadSettings(ini, Name());
}

void Navmap::SaveSettings(ToolboxIni* ini)
{
    ToolboxWidget::SaveSettings(ini);
    ini->SetDoubleValue(Name(), VAR_NAME(scale), scale);
    //SAVE_BOOL(hero_flag_controls_show);
    //SAVE_BOOL(hero_flag_window_attach);
    //SAVE_COLOR(hero_flag_window_background);
    SAVE_BOOL(mouse_clickthrough_in_outpost);
    SAVE_BOOL(mouse_clickthrough_in_explorable);
    ini->SetLongValue(Name(), VAR_NAME(key_none_behavior), static_cast<long>(key_none_behavior));
    ini->SetLongValue(Name(), VAR_NAME(key_ctrl_behavior), static_cast<long>(key_ctrl_behavior));
    ini->SetLongValue(Name(), VAR_NAME(key_shift_behavior), static_cast<long>(key_shift_behavior));
    ini->SetLongValue(Name(), VAR_NAME(key_alt_behavior), static_cast<long>(key_alt_behavior));

    SAVE_BOOL(rotate_navmap);
    //SAVE_BOOL(flip_on_reverse);
    SAVE_BOOL(smooth_rotation);
    SAVE_BOOL(circular_map);
    //SAVE_BOOL(snap_to_compass);
    //SAVE_BOOL(hide_compass_agents);

    SAVE_BOOL(hide_compass_quest_marker);

    marker_renderer.SaveSettings(ini, Name());
    pmap_renderer.SaveSettings(ini, Name());
    //agent_renderer.SaveSettings(ini, Name());
    //pingslines_renderer.SaveSettings(ini, Name());
    //symbols_renderer.SaveSettings(ini, Name());
    //custom_renderer.SaveSettings(ini, Name());
    //NavEffectRenderer::SaveSettings(ini, Name());
    game_world_renderer.SaveSettings(ini, Name());
}

//size_t Navmap::GetPlayerHeroes(const GW::PartyInfo* party, std::vector<GW::AgentID>& _player_heroes, bool* has_flags)
//{
//    _player_heroes.clear();
//    if (!party) {
//        return 0;
//    }
//    const uint32_t player_id = GW::PlayerMgr::GetPlayerNumber();
//    if (!player_id) {
//        return 0;
//    }
//    const GW::HeroPartyMemberArray& heroes = party->heroes;
//
//    const bool player_is_leader = GetPlayerIsLeader();
//    std::map<uint32_t, const GW::PlayerPartyMember*> party_players_by_id;
//    if (player_is_leader) {
//        for (const GW::PlayerPartyMember& pplayer : party->players) {
//            party_players_by_id.emplace(pplayer.login_number, &pplayer);
//        }
//    }
//
//    _player_heroes.reserve(heroes.size());
//    for (const GW::HeroPartyMember& hero : heroes) {
//        if (hero.owner_player_id == player_id) {
//            _player_heroes.push_back(hero.agent_id);
//        }
//        else if (player_is_leader) {
//            auto found = party_players_by_id.find(hero.owner_player_id);
//            if (found == party_players_by_id.end() || found->second->connected()) {
//                continue;
//            }
//            if (has_flags) {
//                *has_flags = true;
//            }
//        }
//    }
//    if (player_is_leader && has_flags && party->henchmen.size() || _player_heroes.size() && has_flags) {
//        *has_flags = true;
//    }
//    return _player_heroes.size();
//}

float Navmap::GetMapRotation() const
{
    float yaw = 1.5708f;
    if (rotate_navmap) {
        yaw = smooth_rotation ? GW::CameraMgr::GetCamera()->GetCurrentYaw() : GW::CameraMgr::GetYaw();
    }
    //if (camera_currently_reversed && flip_on_reverse) {
        //yaw = DirectX::XM_PI + yaw;
    //}
    return yaw;
}

DirectX::XMFLOAT2 Navmap::GetGwinchScale()
{
    return gwinch_scale;
}

void Navmap::Draw(IDirect3DDevice9*)
{
    if (!IsActive()) {
        return;
    }

    const GW::Agent* me = GW::Agents::GetPlayer();
    if (me == nullptr) {
        return;
    }

    // Check shadowstep location
    /*if (shadowstep_location.x != 0.0f || shadowstep_location.y != 0.0f) {
        GW::EffectArray* effects = GW::Effects::GetPlayerEffects();
        if (!effects) {
            shadowstep_location = GW::Vec2f();
        }
        else {
            bool found = false;
            for (const auto& effect : *effects) {
                found = effect.skill_id == GW::Constants::SkillID::Shadow_of_Haste || effect.skill_id == GW::Constants::SkillID::Shadow_Walk;
                if (found) {
                    break;
                }
            }
            if (!found) {
                shadowstep_location = GW::Vec2f();
            }
        }
    }*/

    // if not center and want to move, move center towards player
    if ((translation.x != 0 || translation.y != 0) && (me->move_x != 0 || me->move_y != 0) && TIMER_DIFF(last_moved) > ms_before_back) {
        const GW::Vec2f v(translation.x, translation.y);
        const auto speed = std::min(static_cast<float>(TIMER_DIFF(last_moved) - ms_before_back) * acceleration, 500.0f);
        GW::Vec2f d = v;
        d = Normalize(d) * speed;
        if (std::abs(d.x) > std::abs(v.x)) {
            translation = GW::Vec2f(0, 0);
        }
        else {
            translation -= d;
        }
    }
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
    ImGui::SetNextWindowSize(ImVec2(500.0f, 500.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(Name(), nullptr, GetWinFlags(ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus))) {
        // window pos are already rounded by imgui, so casting is no big deal
        //if (!snap_to_compass) {
            location.x = static_cast<int>(ImGui::GetWindowPos().x);
            location.y = static_cast<int>(ImGui::GetWindowPos().y);
            size.x = static_cast<int>(ImGui::GetWindowSize().x);
            size.y = static_cast<int>(ImGui::GetWindowSize().y);
        //}
        //else {
        //    // @Cleanup: Don't do this every frame, only when compass is relocated.
        //    //if (!compass_frame) {
        //        //compass_frame = GetWindowPosition(GW::UI::WindowID::WindowID_Compass);
        //    //}
        //    //else {
        //        const float multiplier = GuiUtils::GetGWScaleMultiplier();
        //        float compass_width = compass_frame->width(multiplier);
        //        float compass_height = compass_frame->height(multiplier);
        //        float compass_padding = compass_width * .05f;
        //        location = {static_cast<int>(compass_frame->left(multiplier) + compass_padding), static_cast<int>(compass_frame->top(multiplier) + compass_padding)};
        //        size = {static_cast<int>(compass_width - compass_padding * 2.f), static_cast<int>(compass_height - compass_padding * 2.f)};

        //        if (compass_width == 0 && location.x == 0 && location.y == 0) {
        //            // In "Restore Defaults" state - replace with sane default values
        //            // Default values for a multiplier of 1.0f
        //            constexpr int DEFAULT_WIDTH = 245;
        //            constexpr int DEFAULT_HEIGHT = 269;

        //            compass_width = std::roundf(DEFAULT_WIDTH * multiplier);
        //            compass_height = std::roundf(DEFAULT_HEIGHT * multiplier);
        //            compass_padding = compass_width * .05f;

        //            const auto windowWidth = static_cast<float>(GetPreference(GW::UI::NumberPreference::WindowSizeX));
        //            location.x = static_cast<int>(windowWidth - compass_width + compass_padding);
        //            location.y = static_cast<int>(compass_padding);
        //            size.x = static_cast<int>(compass_width - compass_padding * 2.0f);
        //            size.y = static_cast<int>(compass_height - compass_padding * 2.0f);
        //        }

        //        ImGui::SetWindowPos({static_cast<float>(location.x), static_cast<float>(location.y)});
        //        ImGui::SetWindowSize({static_cast<float>(size.x), static_cast<float>(size.y)});
        //    //}
        //}

        //clipping = {
            //static_cast<LONG>(ImGui::GetWindowPos().x),
            //static_cast<LONG>(ImGui::GetWindowPos().y),
            //static_cast<LONG>(std::ceil(ImGui::GetWindowPos().x + ImGui::GetWindowSize().x)),
            //static_cast<LONG>(std::ceil(ImGui::GetWindowPos().y + ImGui::GetWindowSize().y)),
        //};
    }
    ImGui::End();
    ImGui::PopStyleColor(2);

    //if (hero_flag_controls_show && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable) {
    //    const GW::PartyInfo* playerparty = GetPlayerParty();
    //    bool has_flags = false;
    //    GetPlayerHeroes(playerparty, player_heroes, &has_flags);

    //    if (has_flags) {
    //        if (hero_flag_window_attach) {
    //            ImGui::SetNextWindowPos(ImVec2(static_cast<float>(location.x), static_cast<float>(location.y + size.y)));
    //            ImGui::SetNextWindowSize(ImVec2(static_cast<float>(size.x), 40.0f));
    //        }
    //        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImColor(hero_flag_window_background).Value);
    //        if (ImGui::Begin("Hero Controls", nullptr, GetWinFlags(hero_flag_window_attach ? ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove : 0, false))) {
    //            static const char* flag_txt[] = {"All", "1", "2", "3", "4", "5", "6", "7", "8"};
    //            const unsigned int num_heroflags = player_heroes.size() + 1;
    //            const float w_but = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * static_cast<float>(num_heroflags)) /
    //                                static_cast<float>(num_heroflags + 1);

    //            for (unsigned int i = 0; i < num_heroflags; ++i) {
    //                if (i > 0) {
    //                    ImGui::SameLine();
    //                }
    //                const bool is_flagging = GetFlaggingState() == i;
    //                if (is_flagging) {
    //                    ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonHovered]);
    //                }

    //                if (ImGui::Button(flag_txt[i], ImVec2(w_but, 0))) {
    //                    if (is_flagging) {
    //                        SetFlaggingState(FlagState_None);
    //                    }
    //                    else {
    //                        SetFlaggingState(static_cast<FlaggingState>(i));
    //                    }
    //                    //flagging[i] ^= 1;
    //                }
    //                if (is_flagging) {
    //                    ImGui::PopStyleColor();
    //                }

    //                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
    //                    if (i == 0) {
    //                        GW::PartyMgr::UnflagAll();
    //                    }
    //                    else {
    //                        GW::PartyMgr::FlagHeroAgent(player_heroes[i - 1], GW::GamePos(HUGE_VALF, HUGE_VALF, 0));
    //                    }
    //                }
    //            }
    //            ImGui::SameLine();
    //            if (ImGui::Button("Clear", ImVec2(-1, 0))) {
    //                GW::PartyMgr::UnflagAll();
    //                for (const uint32_t agent_id : player_heroes) {
    //                    GW::PartyMgr::FlagHeroAgent(agent_id, GW::GamePos(HUGE_VALF, HUGE_VALF, 0));
    //                }
    //            }
    //        }
    //        ImGui::End();
    //        ImGui::PopStyleColor();
    //    }
    //}
}

//bool Navmap::ShouldMarkersDrawOnMap()
//{
//    const auto map_has_outpost_and_explorable = [](const GW::Constants::MapID map_id) {
//        switch (map_id) {
//            case GW::Constants::MapID::Domain_of_Anguish:
//                return true;
//            default:
//                return false;
//        }
//    };
//    if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading || (
//            GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost &&
//            map_has_outpost_and_explorable(GW::Map::GetMapID()))) {
//        return false;
//    }
//    return true;
//}

void Navmap::Render(IDirect3DDevice9* device)
{
    //if (compass_fix_pending) {
        // Note: Wait for a frame to pass before toggling off again to allow the game to initialise the window.
        //SetWindowVisible(GW::UI::WindowID_Compass, false);
        //compass_fix_pending = false;
    //}
    auto& instance = Instance();
    if (!instance.IsActive()) {
        return;
    }

    const GW::Agent* me = GW::Agents::GetPlayer();
    if (me == nullptr) {
        return;
    }

    // Backup the DX9 state
    IDirect3DStateBlock9* d3d9_state_block = nullptr;
    if (device->CreateStateBlock(D3DSBT_ALL, &d3d9_state_block) < 0) {
        return;
    }

    // Backup the DX9 transform (DX9 documentation suggests that it is included in the StateBlock but it doesn't appear to)
    D3DMATRIX reset_world;
    D3DMATRIX reset_view;
    D3DMATRIX reset_projection;
    device->GetTransform(D3DTS_WORLD, &reset_world);
    device->GetTransform(D3DTS_VIEW, &reset_view);
    device->GetTransform(D3DTS_PROJECTION, &reset_projection);

    // Setup render state: fixed-pipeline, alpha-blending, no face culling, no depth testing
    device->SetIndices(nullptr);
    device->SetFVF(D3DFVF_CUSTOMVERTEX);
    device->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS, true);
    device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
    device->SetPixelShader(nullptr);
    device->SetVertexShader(nullptr);
    device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    device->SetRenderState(D3DRS_LIGHTING, false);
    device->SetRenderState(D3DRS_ZENABLE, false);
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, true);
    device->SetRenderState(D3DRS_ALPHATESTENABLE, false);
    device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
    device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
    device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
    device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);

    const auto FillRect = [&device](const Color color, const float x, const float y, const float w, const float h) {
        const D3DVertex vertices[6] = {
            {x, y, 0.0f, color},
            {x + w, y, 0.0f, color},
            {x, y + h, 0.0f, color},
            {x + w, y + h, 0.0f, color}
        };
        device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vertices, sizeof(D3DVertex));
    };

    // we MUST draw this for the stencil test, even if alpha is 0
    /*const auto FillCircle = [&device](
        const float x, const float y, const float radius, const Color clr, const unsigned resolution = 192u) {
        const auto res = std::min(resolution, 192u);
        D3DVertex vertices[193];
        for (auto i = 0u; i <= res; i++) {
            const auto angle = static_cast<float>(i) / static_cast<float>(res) * DirectX::XM_2PI;
            vertices[i] = {
                x + radius * cos(angle),
                y + radius * sin(angle),
                0.0f,
                clr
            };
        }
        device->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, res, vertices, sizeof(D3DVertex));
    };*/

    instance.RenderSetupProjection(device);

    const D3DCOLOR background = instance.pmap_renderer.GetBackgroundColor();
    //device->SetScissorRect(&instance.clipping); // always clip to rect as a fallback if the stenciling fails
    device->SetRenderState(D3DRS_SCISSORTESTENABLE, false);
    //if (instance.circular_map) {
    //    device->SetRenderState(D3DRS_STENCILENABLE, true); // enable stencil testing
    //    device->SetRenderState(D3DRS_STENCILMASK, 0xffffffff);
    //    device->SetRenderState(D3DRS_STENCILWRITEMASK, 0xffffffff);

    //    // clear depth and stencil buffer
    //    device->Clear(0, nullptr, D3DCLEAR_STENCIL | D3DCLEAR_ZBUFFER, 0x00000000, 1.0f, 0);

    //    device->SetRenderState(D3DRS_STENCILREF, 1);
    //    device->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_ALWAYS);
    //    device->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_REPLACE); // write ref value into stencil buffer
    //    FillCircle(0, 0, 5000.f, background);                            // draw circle with chosen background color into stencil buffer, fills buffer with 1's
    //    device->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_EQUAL);         // only draw where 1 is in the buffer
    //    device->SetRenderState(D3DRS_STENCILFAIL, D3DSTENCILOP_ZERO);
    //    device->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_REPLACE);
    //}
    //else {
        FillRect(background, -5000.0f, -5000.0f, 10000.f, 10000.f); // fill rect with chosen background color
    //}

    //auto p1 = instance.InterfaceToWorldPoint(Navmap::Vec2i(1920 / 2, 1080 / 2));
    //auto p2 = instance.InterfaceToWorldVector(Navmap::Vec2i(1920 / 2, 1080 / 2));

    auto translate_char = DirectX::XMMatrixTranslation(-me->pos.x, -me->pos.y, 0);

    const auto rotate_char = DirectX::XMMatrixRotationZ(-instance.GetMapRotation() + DirectX::XM_PIDIV2);

    const auto scaleM = DirectX::XMMatrixScaling(instance.scale, instance.scale, 1.0f);
    const auto translationM = DirectX::XMMatrixTranslation(instance.translation.x, instance.translation.y, 0);

    float current_gwinch_scale = static_cast<float>(instance.size.x) / 5000.0f / 2.f * instance.scale;
    if (current_gwinch_scale != gwinch_scale.x) {
        //instance.range_renderer.Invalidate();
        gwinch_scale = {current_gwinch_scale, current_gwinch_scale};
    }

    const auto view = translate_char * rotate_char * scaleM * translationM;
    device->SetTransform(D3DTS_VIEW, reinterpret_cast<const D3DMATRIX*>(&view));

    instance.pmap_renderer.Render(device);

    //instance.custom_renderer.Render(device);

    // move the rings to the char position
    translate_char = DirectX::XMMatrixTranslation(me->pos.x, me->pos.y, 0);
    device->SetTransform(D3DTS_WORLD, reinterpret_cast<const D3DMATRIX*>(&translate_char));
    //instance.range_renderer.Render(device);
    //device->SetTransform(D3DTS_WORLD, &reset_world); //?

    /*if (instance.translation.x != 0 || instance.translation.y != 0) {
        const auto view2 = scaleM;
        device->SetTransform(D3DTS_VIEW, reinterpret_cast<const D3DMATRIX*>(&view2));
        instance.range_renderer.SetDrawCenter(true);
        instance.range_renderer.Render(device);
        instance.range_renderer.SetDrawCenter(false);
        device->SetTransform(D3DTS_VIEW, reinterpret_cast<const D3DMATRIX*>(&view));
    }*/

    instance.marker_renderer.Render(device);

    //device->SetTransform(D3DTS_WORLD, &reset_world); //???? todo
    //instance.agent_renderer.Render(device);

    //instance.effect_renderer.Render(device);

    //instance.pingslines_renderer.Render(device);

    instance.game_world_renderer.Render(device, instance.pmap_renderer.builder.nm, instance.pmap_renderer.builder.nbs);

    /*if (instance.circular_map) {
        device->SetRenderState(D3DRS_STENCILREF, 0);
        device->SetRenderState(D3DRS_STENCILWRITEMASK, 0x00000000);
        device->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_NEVER);
        device->SetRenderState(D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP);
        device->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_KEEP);
        device->SetRenderState(D3DRS_STENCILENABLE, false);
    }*/

    // Restore the DX9 transform
    device->SetTransform(D3DTS_WORLD, &reset_world);
    device->SetTransform(D3DTS_VIEW, &reset_view);
    device->SetTransform(D3DTS_PROJECTION, &reset_projection);

    // Restore the DX9 state
    d3d9_state_block->Apply();
    d3d9_state_block->Release();
}

GW::Vec2f Navmap::InterfaceToWorldPoint(const Vec2i pos) const
{
    const GW::Agent* me = GW::Agents::GetPlayer();
    if (me == nullptr) {
        return {0, 0};
    }

    GW::Vec2f v(static_cast<float>(pos.x), static_cast<float>(pos.y));

    // Invert viewport projection
    v.x = v.x - static_cast<float>(location.x);
    v.y = static_cast<float>(location.y) - v.y;

    // go from [0, width][0, height] to [-1, 1][-1, 1]
    v.x = 2.0f * v.x / static_cast<float>(size.x) - 1.0f;
    v.y = 2.0f * v.y / static_cast<float>(size.x) + 1.0f;

    // scale up to [-w, w]
    constexpr float w = 5000.0f;
    v *= w;

    // translate by camera
    v -= translation;

    // scale by camera
    v /= scale;

    // rotate by current camera rotation
    const float angle = Instance().GetMapRotation() - DirectX::XM_PIDIV2;
    const float x1 = v.x * std::cos(angle) - v.y * std::sin(angle);
    const float y1 = v.x * std::sin(angle) + v.y * std::cos(angle);
    v = GW::Vec2f(x1, y1);

    // translate by character position
    v += me->pos;

    return v;
}

GW::Vec2f Navmap::InterfaceToWorldVector(const Vec2i pos) const
{
    GW::Vec2f v(static_cast<float>(pos.x), static_cast<float>(pos.y));

    // Invert y direction
    v.y = -v.y;

    // go from [0, width][0, height] to [-1, 1][-1, 1]
    v.x = 2.0f * v.x / static_cast<float>(size.x);
    v.y = 2.0f * v.y / static_cast<float>(size.x);

    // scale up to [-w, w]
    constexpr float w = 5000.0f;
    v *= w;

    return v;
}

//void Navmap::SelectTarget(const GW::Vec2f pos)
//{
//    const auto* agents = GW::Agents::GetAgentArray();
//    if (agents == nullptr) {
//        return;
//    }
//    auto distance = 600.0f * 600.0f;
//    const GW::Agent* closest = nullptr;
//
//    for (const auto* agent : *agents) {
//        if (agent == nullptr) {
//            continue;
//        }
//        const auto* living = agent->GetAsAgentLiving();
//        if (living && living->GetIsDead()) {
//            continue;
//        }
//        if (agent->GetIsItemType()) {
//            continue;
//        }
//        if (agent->GetIsGadgetType() && agent->GetAsAgentGadget()->gadget_id != 8141) {
//            continue; // allow locked chests
//        }
//        if (!GW::Agents::GetIsAgentTargettable(agent)) {
//            continue; // block all useless minis
//        }
//        const float newDistance = GetSquareDistance(pos, agent->pos);
//        if (distance > newDistance) {
//            distance = newDistance;
//            closest = agent;
//        }
//    }
//
//    if (closest != nullptr) {
//        GW::Agents::ChangeTarget(closest);
//    }
//}

bool Navmap::WndProc(const UINT Message, const WPARAM wParam, const LPARAM lParam)
{
    if (!GetKeyState(VK_LBUTTON) && mousedown) {
        // fix left button being released outside of gw window
        mousedown = false;
    }
    if (!IsActive()) {
        return false;
    }
    if (is_observing) {
        return false;
    }
    /*if (mouse_clickthrough_in_explorable && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable) {
        if (!IsKeyDown(NavmapModifierBehaviour::Target) &&
            !IsKeyDown(NavmapModifierBehaviour::Walk) &&
            !IsKeyDown(NavmapModifierBehaviour::Move)) {
            return Message == WM_LBUTTONDOWN && FlagHeros(lParam);
        }
    }
    if (mouse_clickthrough_in_outpost && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost) {
        if (!IsKeyDown(NavmapModifierBehaviour::Target) && !IsKeyDown(NavmapModifierBehaviour::Walk) &&
            !IsKeyDown(NavmapModifierBehaviour::Move)) {
            return false;
        }
    }*/
    switch (Message) {
        case WM_MOUSEMOVE:
            return OnMouseMove(Message, wParam, lParam);
        case WM_LBUTTONDOWN:
            return OnMouseDown(Message, wParam, lParam);
        case WM_MOUSEWHEEL:
            return OnMouseWheel(Message, wParam, lParam);
        case WM_LBUTTONDBLCLK:
            return OnMouseDblClick(Message, wParam, lParam);
        case WM_LBUTTONUP:
            return OnMouseUp(Message, wParam, lParam);
        default:
            return false;
    }
}

//bool Navmap::FlagHero(uint32_t idx)
//{
//    if (idx > FlagState_None) {
//        return false;
//    }
//    return SetFlaggingState(static_cast<FlaggingState>(idx));
//}
//
//bool Navmap::FlagHeros(const LPARAM lParam)
//{
//    const int x = GET_X_LPARAM(lParam);
//    const int y = GET_Y_LPARAM(lParam);
//    if (!IsInside(x, y)) {
//        return false;
//    }
//    bool has_flagall = false;
//    GetPlayerHeroes(GetPlayerParty(), player_heroes, &has_flagall);
//    const GW::Vec2f worldpos = InterfaceToWorldPoint(Vec2i(x, y));
//
//    switch (const auto flag_state = GetFlaggingState()) {
//        case FlagState_None:
//            return false;
//        case FlagState_All:
//            if (!has_flagall) {
//                return false;
//            }
//            SetFlaggingState(FlagState_None);
//            GW::PartyMgr::FlagAll(GW::GamePos(worldpos));
//            return true;
//        default:
//            if (flag_state > player_heroes.size()) {
//                return false;
//            }
//            SetFlaggingState(FlagState_None);
//            GW::PartyMgr::FlagHeroAgent(player_heroes[flag_state - 1], GW::GamePos(worldpos));
//            return true;
//    }
//}

bool Navmap::OnMouseDown(const UINT, const WPARAM, const LPARAM lParam)
{
    if (!IsActive()) {
        return false;
    }
    //if (FlagHeros(lParam)) {
        //return true;
    //}
    const int x = GET_X_LPARAM(lParam);
    const int y = GET_Y_LPARAM(lParam);
    if (!IsInside(x, y)) {
        return false;
    }

    mousedown = true;
    const GW::Vec2f worldpos = InterfaceToWorldPoint(Vec2i(x, y));

    /*if (IsKeyDown(NavmapModifierBehaviour::Target)) {
        SelectTarget(worldpos);
        return true;
    }*/

    /*if (IsKeyDown(NavmapModifierBehaviour::Walk)) {
        GW::Agents::Move(worldpos);
        pingslines_renderer.AddMouseClickPing(worldpos);
        return true;
    }*/

    drag_start.x = x;
    drag_start.y = y;

    if (IsKeyDown(NavmapModifierBehaviour::Move)) {
        return true;
    }

    if (!lock_move) {
        return true;
    }

    //pingslines_renderer.OnMouseDown(worldpos.x, worldpos.y);

    return true;
}

bool Navmap::OnMouseDblClick(const UINT, const WPARAM, const LPARAM lParam) const
{
    if (!IsActive()) {
        return false;
    }

    const int x = GET_X_LPARAM(lParam);
    const int y = GET_Y_LPARAM(lParam);
    if (!IsInside(x, y)) {
        return false;
    }

    /*if (IsKeyDown(NavmapModifierBehaviour::Target)) {
        SelectTarget(InterfaceToWorldPoint(Vec2i(x, y)));
        return true;
    }*/

    return true;
}

bool Navmap::OnMouseUp(const UINT, const WPARAM, const LPARAM)
{
    if (!IsActive()) {
        return false;
    }

    if (!mousedown) {
        return false;
    }

    mousedown = false;

    //return pingslines_renderer.OnMouseUp();

    return true;
}

bool Navmap::OnMouseMove(const UINT, const WPARAM, const LPARAM lParam)
{
    if (!IsActive()) {
        return false;
    }

    if (!mousedown) {
        return false;
    }

    const int x = GET_X_LPARAM(lParam);
    const int y = GET_Y_LPARAM(lParam);
    // if (!IsInside(x, y)) return false;

    /*if (IsKeyDown(NavmapModifierBehaviour::Target)) {
        SelectTarget(InterfaceToWorldPoint(Vec2i(x, y)));
        return true;
    }*/

    if (IsKeyDown(NavmapModifierBehaviour::Move)) {
        const auto diff = Vec2i(x - drag_start.x, y - drag_start.y);
        translation += InterfaceToWorldVector(diff);
        drag_start = Vec2i(x, y);
        last_moved = TIMER_INIT();
        return true;
    }

    //const GW::Vec2f v = InterfaceToWorldPoint(Vec2i(x, y));
    //return pingslines_renderer.OnMouseMove(v.x, v.y);

    return true;
}

bool Navmap::OnMouseWheel(const UINT, const WPARAM wParam, const LPARAM)
{
    if (!IsActive()) {
        return false;
    }

    // Mouse wheel x and y are in absolute coords, not window coords! (Windows why...)
    const ImVec2 mouse = ImGui::GetMousePos();
    if (!IsInside(static_cast<int>(mouse.x), static_cast<int>(mouse.y))) {
        return false;
    }

    const int zDelta = GET_WHEEL_DELTA_WPARAM(wParam);

    if (IsKeyDown(NavmapModifierBehaviour::Move)) {
        const float delta = zDelta > 0 ? 1.024f : 0.9765625f;
        scale *= delta;
        return true;
    }

    return false;
}

bool Navmap::IsInside(const int x, const int y) const
{
    // if outside square, return false
    if (x < location.x) {
        return false;
    }
    if (x > location.x + size.x) {
        return false;
    }
    if (y < location.y) {
        return false;
    }
    if (y > location.y + size.y) {
        return false;
    }

    // if centered, use radar range
    if (translation.x == 0 && translation.y == 0) {
        const GW::Vec2f gamepos = InterfaceToWorldPoint(Vec2i(x, y));
        const GW::Agent* me = GW::Agents::GetPlayer();
        if (!me) {
            return false;
        }
        const float sqrdst = GetSquareDistance(me->pos, gamepos);
        return sqrdst < GW::Constants::SqrRange::Compass;
    }
    return true;
}

bool Navmap::IsActive() const
{
    /*if (snap_to_compass) {
        if (!compass_frame) {
            compass_frame = GetWindowPosition(GW::UI::WindowID::WindowID_Compass);
        }
        if (compass_frame && !compass_frame->visible()) {
            return false;
        }
    }*/

    return visible
           && !loading
           && GW::Map::GetIsMapLoaded()
           && !GW::UI::GetIsWorldMapShowing()
           && GW::Map::GetInstanceType() != GW::Constants::InstanceType::Loading
           && GW::Agents::GetPlayerId() != 0;
}

void Navmap::RenderSetupProjection(IDirect3DDevice9* device) const
{
    D3DVIEWPORT9 viewport;
    device->GetViewport(&viewport);

    constexpr float w = 5000.0f * 2;
    // IMPORTANT: we are setting z-near to 0.0f and z-far to 1.0f
    const DirectX::XMMATRIX ortho_matrix(2 / w, 0, 0, 0, 0, 2 / w, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);

    //// note: manually craft the projection to viewport instead of using
    //// SetViewport to allow target regions outside the viewport
    //// e.g. negative x/y for slightly offscreen map
    const auto width_f = static_cast<float>(viewport.Width);
    const auto height_f = static_cast<float>(viewport.Height);
    const float xscale = static_cast<float>(size.x) / width_f;
    const float yscale = static_cast<float>(size.x) / height_f;
    const float xtrans = static_cast<float>(location.x * 2 + size.x) / width_f - 1.0f;
    const float ytrans = -static_cast<float>(location.y * 2 + size.x) / height_f + 1.0f;
    ////IMPORTANT: we are basically setting z-near to 0 and z-far to 1
    const DirectX::XMMATRIX viewport_matrix(xscale, 0, 0, 0, 0, yscale, 0, 0, 0, 0, 1, 0, xtrans, ytrans, 0, 1);

    const auto proj = ortho_matrix * viewport_matrix;

    device->SetTransform(D3DTS_PROJECTION, reinterpret_cast<const D3DMATRIX*>(&proj));
}

bool Navmap::IsKeyDown(const NavmapModifierBehaviour mmb) const
{
    return (key_none_behavior == mmb && !ImGui::IsKeyDown(ImGuiKey_ModCtrl) &&
            !ImGui::IsKeyDown(ImGuiKey_ModShift) && !ImGui::IsKeyDown(ImGuiKey_ModAlt)) ||
           (key_ctrl_behavior == mmb && ImGui::IsKeyDown(ImGuiKey_ModCtrl)) ||
           (key_shift_behavior == mmb && ImGui::IsKeyDown(ImGuiKey_ModShift)) ||
           (key_alt_behavior == mmb && ImGui::IsKeyDown(ImGuiKey_ModAlt));
}

void Navmap::DrawSizeAndPositionSettings()
{
    //visible = false;
    lock_move = true;
    lock_size = true;
    //show_menubutton = false;
    show_closebutton = false;
    can_show_in_main_window = true;
    has_closebutton = false;
    is_resizable = false;
    is_movable = false;

    auto& io = ImGui::GetIO();
    //float wx = io.DisplaySize.x;
    float wh = io.DisplaySize.y;
    float yoffset = -(wh / 2.0f) + center_y_offset;

    //scale 0.05

    ImVec2 pos_(0, yoffset);
    ImVec2 size_(io.DisplaySize.x, io.DisplaySize.y);
    if (const auto window = ImGui::FindWindowByName(Name())) {
        //pos = window->Pos;
        //size = window->Size;

        ImGui::SetWindowPos(Name(), pos_);
        ImGui::SetWindowSize(Name(), size_);
    }
    if (is_movable || is_resizable) {
        char buf[128];
        sprintf(buf, "You need to show the %s for this control to work", TypeName());
        if (is_movable) {
            if (ImGui::DragFloat2("Position", reinterpret_cast<float*>(&pos_), 1.0f, 0.0f, 0.0f, "%.0f")) {
                ImGui::SetWindowPos(Name(), pos_);
            }
            ImGui::ShowHelp(buf);
        }
        if (is_resizable) {
            if (ImGui::DragFloat2("Size", reinterpret_cast<float*>(&size_), 1.0f, 0.0f, 0.0f, "%.0f")) {
                ImGui::SetWindowSize(Name(), size_);
            }
            ImGui::ShowHelp(buf);
        }
    }
    auto count = 0;
    if (is_movable) {
        if (++count % 2 == 0) {
            ImGui::SameLine();
        }
        ImGui::Checkbox("Lock Position", &lock_move);
    }
    if (is_resizable) {
        if (++count % 2 == 0) {
            ImGui::SameLine();
        }
        ImGui::Checkbox("Lock Size", &lock_size);
    }
    if (has_closebutton) {
        if (++count % 2 == 0) {
            ImGui::SameLine();
        }
        ImGui::Checkbox("Show close button", &show_closebutton);
    }
    if (can_show_in_main_window) {
        if (++count % 2 == 0) {
            ImGui::SameLine();
        }
        if (ImGui::Checkbox("Show in main window", &show_menubutton)) {
            MainWindow::Instance().pending_refresh_buttons = true;
        }
    }
}

extern volatile bool navmesh_building;

void Navmap::Update(float delta)
{
    if(navmesh_building || !pmap_renderer.builder.nm || !pmap_renderer.builder.nm->geom || !pmap_renderer.builder.nm->sample)return;

    pmap_renderer.builder.nm->Update(delta);
}
