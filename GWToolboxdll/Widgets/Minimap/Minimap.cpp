#include "stdafx.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/Constants/Skills.h>

#include <GWCA/GameContainers/Array.h>
#include <GWCA/GameContainers/GamePos.h>
#include <GWCA/GameEntities/Camera.h>
#include <GWCA/Packets/StoC.h>

#include <GWCA/GameEntities/Quest.h>
#include <GWCA/GameEntities/Hero.h>
#include <GWCA/GameEntities/Party.h>
#include <GWCA/GameEntities/Skill.h>
#include <GWCA/GameEntities/Agent.h>

#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/PartyContext.h>
#include <GWCA/Context/WorldContext.h>

#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/CameraMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/EffectMgr.h>
#include <GWCA/Managers/PlayerMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>

#include <GWCA/Utilities/Scanner.h>
#include <GWCA/Utilities/Hooker.h>
#include <Utils/GuiUtils.h>
#include <ImGuiAddons.h>
#include <Logger.h>

#include <Widgets/Minimap/Minimap.h>
#include <Modules/Resources.h>
#include <GWCA/Utilities/MemoryPatcher.h>
#include <GWCA/Managers/QuestMgr.h>

namespace {
    DirectX::XMFLOAT2 gwinch_scale;

    enum FlaggingState : uint32_t {
        FlagState_All = 0,
        FlagState_Hero1,
        FlagState_Hero2,
        FlagState_Hero3,
        FlagState_Hero4,
        FlagState_Hero5,
        FlagState_Hero6,
        FlagState_Hero7,
        FlagState_None
    };
    enum CaptureMouseClickType : uint32_t {
        CaptureType_None = 0,
        CaptureType_FlagHero = 1,
        CaptureType_SalvageWithUpgrades = 11,
        CaptureType_SalvageMaterials = 12,
        CaptureType_Idenfify = 13
    };
    struct MouseClickCaptureData {
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
                            FlaggingState* flagging_hero;
                        } *sub5;
                    } *sub4;
                } *sub3;
            } *sub2;
        } *sub1;
    } *MouseClickCaptureDataPtr = nullptr;
    uint32_t* GameCursorState = nullptr;
    CaptureMouseClickType* CaptureMouseClickTypePtr = nullptr;

    // Internal flagging state to workaround not being able to set the in-game cursor state.
    FlaggingState minimap_flagging_state = FlaggingState::FlagState_None;

    FlaggingState GetFlaggingState() {
        if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Explorable)
            return FlaggingState::FlagState_None;
        if (!CaptureMouseClickTypePtr || *CaptureMouseClickTypePtr != CaptureType_FlagHero || !MouseClickCaptureDataPtr || !MouseClickCaptureDataPtr->sub1)
            return FlaggingState::FlagState_None;
        return *MouseClickCaptureDataPtr->sub1->sub2->sub3->sub4->sub5->flagging_hero;
    }
    bool compass_fix_pending = false;
    bool SetFlaggingState(FlaggingState set_state) {
        if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Explorable)
            return false;
        // keep an internal flag for the minimap flagging until StartCaptureMouseClick_Func is working
        // minimap_flagging_state = set_state;
        if (GetFlaggingState() == set_state)
            return true;
        if (set_state == FlaggingState::FlagState_None) {
            set_state = GetFlaggingState();
        }
        GW::UI::ControlAction key = GW::UI::ControlAction_None;
        switch (set_state) {
        case FlaggingState::FlagState_Hero1: key = GW::UI::ControlAction_CommandHero1; break;
        case FlaggingState::FlagState_Hero2: key = GW::UI::ControlAction_CommandHero2; break;
        case FlaggingState::FlagState_Hero3: key = GW::UI::ControlAction_CommandHero3; break;
        case FlaggingState::FlagState_Hero4: key = GW::UI::ControlAction_CommandHero4; break;
        case FlaggingState::FlagState_Hero5: key = GW::UI::ControlAction_CommandHero5; break;
        case FlaggingState::FlagState_Hero6: key = GW::UI::ControlAction_CommandHero6; break;
        case FlaggingState::FlagState_Hero7: key = GW::UI::ControlAction_CommandHero7; break;
        case FlaggingState::FlagState_All: key = GW::UI::ControlAction_CommandParty; break;
        default:
            return false;
        }
        return GW::UI::Keypress(key);
    }
    // Same as GW::PartyMgr::GetPlayerIsLeader() but has an extra check to ignore disconnected people.
    bool GetPlayerIsLeader() {
        GW::PartyInfo* party = GW::PartyMgr::GetPartyInfo();
        if (!party || !party->players.size()) return false;
        const uint32_t player_id = GW::PlayerMgr::GetPlayerNumber();
        for (auto& player : party->players) {
            if (!player.connected())
                continue;
            return player.login_number == player_id;
        }
        return false;
    }

    GW::PartyInfo* GetPlayerParty() {
        const GW::GameContext * gamectx = GW::GetGameContext();
        if (!(gamectx && gamectx->party))
            return nullptr;
        return gamectx->party->player_party;
    }
    GW::UI::WindowPosition* compass_frame = 0;

    typedef uint32_t(__fastcall* DrawCompassAgentsByType_pt)(void* ecx, void* edx, uint32_t param_1, uint32_t param_2, uint32_t flags);
    DrawCompassAgentsByType_pt DrawCompassAgentsByType_Func = 0;
    DrawCompassAgentsByType_pt DrawCompassAgentsByType_Ret = 0;

    bool hide_compass_agents = false;
    uint32_t __fastcall OnDrawCompassAgentsByType(void* ecx, void* edx, uint32_t param_1, uint32_t param_2, uint32_t flags)
    {
        GW::Hook::EnterHook();
        uint32_t result = 0;
        if (!hide_compass_agents) {
            result = DrawCompassAgentsByType_Ret(ecx, edx, param_1, param_2, flags);
        }
        GW::Hook::LeaveHook();
        return result;
    }

    bool hide_compass_quest_marker = false;
    GW::MemoryPatcher show_compass_quest_marker_patch;
    void ToggleCompassQuestMarker(bool enable) {
        if (enable == show_compass_quest_marker_patch.GetIsEnable())
            return;
        show_compass_quest_marker_patch.TogglePatch(enable);
        GW::GameThread::Enqueue([] {
            const auto quest = GW::QuestMgr::GetActiveQuest();
            if (quest) {
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

                GW::UI::SendUIMessage(GW::UI::UIMessage::kClientActiveQuestChanged, &msg);
            }
        });

    }
}
void Minimap::DrawHelp() {
    if (!ImGui::TreeNodeEx("Minimap", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth))
        return;

    ImGui::Bullet(); ImGui::Text("'/marktarget' highlights the current target on your minimap.");
    ImGui::Bullet(); ImGui::Text("'/marktarget clear' removes the current target as a marked target on your minimap.");
    ImGui::Bullet(); ImGui::Text("'/clearmarktarget clear' removes all marked target highlights on your minimap.");
    ImGui::TreePop();
}
void Minimap::Terminate()
{
    ToolboxWidget::Terminate();
    range_renderer.Terminate();
    pmap_renderer.Terminate();
    agent_renderer.Terminate();
    pingslines_renderer.Terminate();
    symbols_renderer.Terminate();
    custom_renderer.Terminate();
    effect_renderer.Terminate();
}

void Minimap::Initialize()
{
    ToolboxWidget::Initialize();

    uintptr_t address = GW::Scanner::Find("\x00\x74\x16\x6A\x27\x68\x80\x00\x00\x00\x6A\x00\x68", "xxxxxxxxxxxxx",-0x51);
    if (address) {
        address = *(uintptr_t*)address;
        MouseClickCaptureDataPtr = (MouseClickCaptureData*)address;
        GameCursorState = (uint32_t*)(address + 0x4);
        CaptureMouseClickTypePtr = (CaptureMouseClickType*)(address - 0x10);
    }
    Log::Log("[SCAN] CaptureMouseClickTypePtr = %p\n", CaptureMouseClickTypePtr);
    Log::Log("[SCAN] MouseClickCaptureDataPtr = %p\n", MouseClickCaptureDataPtr);

    DrawCompassAgentsByType_Func = (DrawCompassAgentsByType_pt)GW::Scanner::Find("\x8b\x46\x08\x8d\x5e\x18\x53", "xxxxxxx", -0xb);
    GW::HookBase::CreateHook(DrawCompassAgentsByType_Func, OnDrawCompassAgentsByType, (void**)&DrawCompassAgentsByType_Ret);
    GW::HookBase::EnableHooks(DrawCompassAgentsByType_Func);

    address = GW::Scanner::Find("\xdd\xd8\x6a\x01\x52","xxxxx");
    if (address)
        show_compass_quest_marker_patch.SetPatch(address, "\xEB\xEC", 2);
    ToggleCompassQuestMarker(hide_compass_quest_marker);


    GW::UI::RegisterKeydownCallback(&AgentPinged_Entry, [this](GW::HookStatus* ,uint32_t key) {
        if (key != GW::UI::ControlAction_ReverseCamera)
            return;
        camera_currently_reversed = true;
        });
    GW::UI::RegisterKeyupCallback(&AgentPinged_Entry, [this](GW::HookStatus*, uint32_t key) {
        if (key != GW::UI::ControlAction_ReverseCamera)
            return;
        camera_currently_reversed = false;
        });

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::AgentPinged>(&AgentPinged_Entry, [this](GW::HookStatus *, GW::Packet::StoC::AgentPinged *pak) -> void {
        if (visible) {
            pingslines_renderer.P046Callback(pak);
        }
    });
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::CompassEvent>(&CompassEvent_Entry, [this](GW::HookStatus *, GW::Packet::StoC::CompassEvent *pak) -> void {
        if (visible) {
            pingslines_renderer.P138Callback(pak);
        }
    });
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::PlayEffect>(&CompassEvent_Entry, [this](GW::HookStatus *status, GW::Packet::StoC::PlayEffect *pak) -> void {
        UNREFERENCED_PARAMETER(status);
        if (visible) {
            if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable)
                effect_renderer.PacketCallback(pak);
        }
    });
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GenericValue>(&GenericValueTarget_Entry, [this](GW::HookStatus *s, GW::Packet::StoC::GenericValue *pak) -> void {
        UNREFERENCED_PARAMETER(s);
        if (visible) {
            if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable)
                effect_renderer.PacketCallback(pak);
        }
    });
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GenericValueTarget>(&GenericValueTarget_Entry, [this](GW::HookStatus *s, GW::Packet::StoC::GenericValueTarget *pak) -> void {
        UNREFERENCED_PARAMETER(s);
        if (visible) {
            pingslines_renderer.P153Callback(pak);
            if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable)
                effect_renderer.PacketCallback(pak);
        }
    });
    constexpr GW::UI::UIMessage hook_messages[] = {
        GW::UI::UIMessage::kMapChange,
        GW::UI::UIMessage::kMapLoaded,
        GW::UI::UIMessage::kChangeTarget,
        GW::UI::UIMessage::kSkillActivated
    };
    for (const auto message_id : hook_messages) {
        GW::UI::RegisterUIMessageCallback(&UIMsg_Entry, message_id, OnUIMessage);
    }

    last_moved = TIMER_INIT();

    pmap_renderer.Invalidate();

    GW::Chat::CreateCommand(L"flag", &OnFlagHeroCmd);
}
void Minimap::OnUIMessage(GW::HookStatus*, GW::UI::UIMessage msgid, void* wParam, void*) {
    auto& instance = Instance();
    switch (msgid) {
    case GW::UI::UIMessage::kMapLoaded: {
        instance.pmap_renderer.Invalidate();
        instance.loading = false;
        // Compass fix to allow hero flagging controls
        const GW::UI::WindowPosition* compass_info = GW::UI::GetWindowPosition(GW::UI::WindowID_Compass);
        if (compass_info && !compass_info->visible()) {
            // Note: Wait for a frame to pass before toggling off again to allow the game to initialise the window.
            compass_fix_pending = true;
            GW::UI::SetWindowVisible(GW::UI::WindowID_Compass, true);
        }
        instance.is_observing = GW::Map::GetIsObserving();
    } break;
    case GW::UI::UIMessage::kSkillActivated: {
        const struct Payload {
            uint32_t agent_id;
            GW::Constants::SkillID skill_id;
        } *payload = static_cast<Payload*>(wParam);
        if (payload->agent_id == GW::Agents::GetPlayerId()) {
            if (payload->skill_id == GW::Constants::SkillID::Shadow_of_Haste || payload->skill_id == GW::Constants::SkillID::Shadow_Walk) {
                instance.shadowstep_location = GW::Agents::GetPlayer()->pos;
            }
        }
    } break;
    case GW::UI::UIMessage::kMapChange: {
        instance.loading = true;
        instance.agent_renderer.auto_target_id = 0;
    } break;
    case GW::UI::UIMessage::kChangeTarget: {
        const GW::UI::ChangeTargetUIMsg* msg = static_cast<GW::UI::ChangeTargetUIMsg*>(wParam);
        instance.agent_renderer.auto_target_id = GW::Agents::GetTargetId() ? 0 : msg->auto_target_id;
    } break;
    }
}

GW::Vec2f Minimap::ShadowstepLocation() const
{
    return shadowstep_location;
}

void Minimap::OnFlagHeroCmd(const wchar_t *message, int argc, LPWSTR *argv)
{
    UNREFERENCED_PARAMETER(message);

    if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Explorable) {
        return; // Not explorable - "/flag" can be typed in chat to bypass flag hero buttons, so this is needed.
    }
    if (argc <= 1) {
        Instance().FlagHero(0); // "/flag"
        return;
    }
    const auto is_flagged = [](auto hero) -> bool {
        if (hero == 0) {
            const GW::Vec3f &allflag = GW::GetGameContext()->world->all_flag;
            if (allflag.x != 0 && allflag.y != 0 && (!std::isinf(allflag.x) || !std::isinf(allflag.y))) {
                return true;
            }
        } else {
            const GW::HeroFlagArray &flags = GW::GetGameContext()->world->hero_flags;
            if (!flags.valid() || static_cast<uint32_t>(hero) > flags.size())
                return false;

            const GW::HeroFlag &flag = flags[hero - 1];
            if (!std::isinf(flag.flag.x) || !std::isinf(flag.flag.y)) {
                return true;
            }
        }
        return false;
    };

    const std::wstring arg1 = GuiUtils::ToLower(argv[1]);
    float x;
    float y;
    unsigned int n_heros = 0; // Count of heros available
    unsigned int f_hero = 0;  // Hero number to flag
    if (arg1 == L"all" || arg1 == L"0") {
        if (argc < 3) {
            Instance().FlagHero(0); // "/flag all" == "/flag"
            return;
        }
        const std::wstring arg2 = GuiUtils::ToLower(argv[2]);
        if (arg2 == L"clear") {
            GW::PartyMgr::UnflagAll(); // "/flag 5 clear"
            return;
        }
        if (arg2 == L"toggle") { // "/flag 5 toggle"
            if (is_flagged(0))
                GW::PartyMgr::UnflagAll();
            else
                Instance().FlagHero(0);
            return;
        }
        if (argc < 4 || !GuiUtils::ParseFloat(argv[2], &x) || !GuiUtils::ParseFloat(argv[3], &y)) {
            Log::Error("Please provide command in format /flag all [x] [y]"); // Not enough args or coords not valid float vals.
            return;
        }
        GW::PartyMgr::FlagAll(GW::GamePos(x, y, 0)); // "/flag all -2913.41 3004.78"
        return;
    }
    const auto& heroarray = GW::GetGameContext()->party->player_party->heroes;

    if (heroarray.valid())
        n_heros = heroarray.size();
    if (n_heros < 1) {
        return; // Player has no heroes, so no need to continue.
    }
    if (arg1 == L"clear") {
        for (unsigned int i = 1; i <= n_heros; ++i) {
            GW::PartyMgr::UnflagHero(i);
        }
        GW::PartyMgr::UnflagAll(); // "/flag clear"
        return;
    }
    if (!GuiUtils::ParseUInt(argv[1], &f_hero) || f_hero < 1 || f_hero > n_heros) {
        Log::Error("Invalid hero number");
        return; // Invalid hero number
    }
    if (argc < 3) {
        Instance().FlagHero(f_hero); // "/flag 5"
        return;
    }
    const std::wstring arg2 = GuiUtils::ToLower(argv[2]);
    if (arg2 == L"clear") {
        GW::PartyMgr::UnflagHero(f_hero); // "/flag 5 clear"
        return;
    }
    if (arg2 == L"toggle") { // "/flag 5 toggle"
        if (is_flagged(f_hero))
            GW::PartyMgr::UnflagHero(f_hero);
        else
            Instance().FlagHero(f_hero);
        return;
    }
    if (argc < 4 || !GuiUtils::ParseFloat(argv[2], &x) || !GuiUtils::ParseFloat(argv[3], &y)) {
        Log::Error("Please provide command in format /flag [hero number] [x] [y]"); // Invalid coords
        return;
    }
    GW::PartyMgr::FlagHeroAgent(GW::Agents::GetHeroAgentID(f_hero), GW::GamePos(x, y, 0)); // "/flag 5 -2913.41 3004.78"
}

void Minimap::DrawSettingInternal()
{
    constexpr const char* minimap_modifier_behavior_combo_str = "Disabled\0Draw\0Target\0Move\0Walk\0\0";

    if (snap_to_compass) {
        ImGui::NextSpacedElement();
    }
    ImGui::Checkbox("Snap to compass", &snap_to_compass);
    ImGui::ShowHelp("Resize and position minimap to match in-game compass size and position.");
    ImGui::Checkbox("Hide GW compass agents", &hide_compass_agents);
    if (ImGui::Checkbox("Hide GW compass quest marker", &hide_compass_quest_marker)) {
        ToggleCompassQuestMarker(hide_compass_quest_marker);
    }
    ImGui::ShowHelp("To disable the toolbox minimap quest marker, set the quest marker color to transparent in the Symbols section below.");

    is_movable = is_resizable = !snap_to_compass;
    if (is_resizable) {
        ImVec2 winsize(100.0f, 100.0f);
        const ImGuiWindow* window = ImGui::FindWindowByName(Name());
        if (window) {
            winsize = window->Size;
        }
        if (ImGui::DragFloat("Size", &winsize.x, 1.0f, 0.0f, 0.0f, "%.0f")) {
            winsize.y = winsize.x;
            ImGui::SetWindowSize(Name(), winsize);
        }
    }

    ImGui::Text("General");
    static float a = scale;
    if (ImGui::DragFloat("Scale", &a, 0.01f, 0.1f, 10.f))
        scale = a;
    ImGui::Text("You can set the color alpha to 0 to disable any minimap feature.");
    // agent_rendered has its own TreeNodes
    agent_renderer.DrawSettings();
    if (ImGui::TreeNodeEx("Ranges", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
        range_renderer.DrawSettings();
        ImGui::TreePop();
    }
    if (ImGui::TreeNodeEx("Pings and drawings", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
        pingslines_renderer.DrawSettings();
        ImGui::TreePop();
    }
    if (ImGui::TreeNodeEx("AoE Effects", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
        effect_renderer.DrawSettings();
        ImGui::TreePop();
    }
    if (ImGui::TreeNodeEx("Symbols", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
        symbols_renderer.DrawSettings();
        ImGui::TreePop();
    }
    if (ImGui::TreeNodeEx("Terrain", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
        pmap_renderer.DrawSettings();
        ImGui::TreePop();
    }
    custom_renderer.DrawSettings();
    if (ImGui::TreeNodeEx("Hero flagging", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
        ImGui::Checkbox("Show hero flag controls", &hero_flag_controls_show);
        ImGui::Checkbox("Attach to minimap", &hero_flag_window_attach);
        ImGui::ShowHelp("If disabled, you can move/resize the window with 'Unlock Move All'.");
        Colors::DrawSettingHueWheel("Background", &hero_flag_window_background);
        ImGui::TreePop();
    }
    ImGui::StartSpacedElements(300.f);
    ImGui::NextSpacedElement();  ImGui::Checkbox("Show boss by profession color on minimap", &agent_renderer.boss_colors);
    ImGui::NextSpacedElement();  ImGui::Checkbox("Show hidden NPCs", &agent_renderer.show_hidden_npcs);
    ImGui::ShowHelp("Show NPCs that aren't usually visible on the minimap\ne.g. minipets, invisible NPCs");
    ImGui::NextSpacedElement();  ImGui::Checkbox("Show symbol for quest NPCs", &agent_renderer.show_quest_npcs_on_minimap);
    ImGui::ShowHelp("Show a star for NPCs that have quest progress available");

    ImGui::SliderInt("Agent Border thickness", reinterpret_cast<int*>(&agent_renderer.agent_border_thickness), 0, 50);

    ImGui::Text("Allow mouse click-through in:");
    ImGui::Indent();
    ImGui::StartSpacedElements(200.f);
    ImGui::NextSpacedElement(); ImGui::Checkbox("Explorable areas", &mouse_clickthrough_in_explorable);
    ImGui::NextSpacedElement(); ImGui::Checkbox("Outposts", &mouse_clickthrough_in_outpost);

    ImGui::Unindent();
    ImGui::Text("Hold + Click modifiers");
    ImGui::ShowHelp("Define behaviour of holding keyboard keys and clicking the minimap.\n"
                    "Draw: ping and draw on the compass.\n"
                    "Target: click to target agent.\n"
                    "Move: move the minimap outside of compass range.\n"
                    "Walk: start walking character to selected location.\n");
    ImGui::Combo("None", reinterpret_cast<int*>(&key_none_behavior), minimap_modifier_behavior_combo_str);
    ImGui::Combo("Control", reinterpret_cast<int*>(&key_ctrl_behavior), minimap_modifier_behavior_combo_str);
    ImGui::Combo("Shift", reinterpret_cast<int*>(&key_shift_behavior), minimap_modifier_behavior_combo_str);
    ImGui::Combo("Alt", reinterpret_cast<int*>(&key_alt_behavior), minimap_modifier_behavior_combo_str);

    ImGui::StartSpacedElements(256.f);
    ImGui::NextSpacedElement(); ImGui::Checkbox("Reduce agent ping spam", &pingslines_renderer.reduce_ping_spam);
    ImGui::ShowHelp("Additional pings on the same agents will increase the duration of the existing ping, rather than create a new one.");
    ImGui::NextSpacedElement(); ImGui::Checkbox("Map Rotation", &rotate_minimap);
    ImGui::ShowHelp("Map rotation on (e.g. Compass), or off (e.g. Mission Map).");
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Flip when reversed", &flip_on_reverse);
    ImGui::ShowHelp("Whether the minimap rotation should flip 180 degrees when you reverse your camera.");
    ImGui::NextSpacedElement(); ImGui::Checkbox("Map rotation smoothing", &smooth_rotation);
    ImGui::ShowHelp("Minimap rotation speed matches compass rotation speed.");
    ImGui::NextSpacedElement(); ImGui::Checkbox("Circular", &circular_map);
    ImGui::ShowHelp("Whether the map should be circular like the compass (default) or a square.");
}

void Minimap::LoadSettings(ToolboxIni *ini)
{
    ToolboxWidget::LoadSettings(ini);
    Resources::EnsureFileExists(Resources::GetPath(L"Markers.ini"),
        "https://raw.githubusercontent.com/HasKha/GWToolboxpp/master/resources/Markers.ini",
        [](bool success, const std::wstring& error) {
            if (success) {
                Minimap::Instance().custom_renderer.LoadMarkers();
            }
            else {
                Log::ErrorW(L"Failed to download Markers.ini\n%s", error.c_str());
            }

        });
    scale = static_cast<float>(ini->GetDoubleValue(Name(), VAR_NAME(scale), 1.0));
    hero_flag_controls_show = ini->GetBoolValue(Name(), VAR_NAME(hero_flag_controls_show), true);
    hero_flag_window_attach = ini->GetBoolValue(Name(), VAR_NAME(hero_flag_window_attach), true);
    hero_flag_window_background = Colors::Load(ini, Name(), "hero_flag_controls_background", hero_flag_window_background);
    mouse_clickthrough_in_outpost = ini->GetBoolValue(Name(), VAR_NAME(mouse_clickthrough_in_outpost), mouse_clickthrough_in_outpost);
    mouse_clickthrough_in_explorable = ini->GetBoolValue(Name(), VAR_NAME(mouse_clickthrough_in_explorable), mouse_clickthrough_in_explorable);
    rotate_minimap = ini->GetBoolValue(Name(), VAR_NAME(rotate_minimap), rotate_minimap);
    flip_on_reverse = ini->GetBoolValue(Name(), VAR_NAME(flip_on_reverse), flip_on_reverse);
    smooth_rotation = ini->GetBoolValue(Name(), VAR_NAME(smooth_rotation), smooth_rotation);
    circular_map = ini->GetBoolValue(Name(), VAR_NAME(circular_map), circular_map);
    snap_to_compass = ini->GetBoolValue(Name(), VAR_NAME(snap_to_compass), snap_to_compass);
    hide_compass_agents = ini->GetBoolValue(Name(), VAR_NAME(hide_compass_agents), hide_compass_agents);

    hide_compass_quest_marker = LOAD_BOOL(hide_compass_quest_marker);
    ToggleCompassQuestMarker(hide_compass_quest_marker);

    key_none_behavior = static_cast<MinimapModifierBehaviour>(ini->GetLongValue(Name(), VAR_NAME(key_none_behavior), 1));
    key_ctrl_behavior = static_cast<MinimapModifierBehaviour>(ini->GetLongValue(Name(), VAR_NAME(key_ctrl_behavior), 2));
    key_shift_behavior = static_cast<MinimapModifierBehaviour>(ini->GetLongValue(Name(), VAR_NAME(key_shift_behavior), 3));
    key_alt_behavior = static_cast<MinimapModifierBehaviour>(ini->GetLongValue(Name(), VAR_NAME(key_alt_behavior), 4));

    range_renderer.LoadSettings(ini, Name());
    pmap_renderer.LoadSettings(ini, Name());
    agent_renderer.LoadSettings(ini, Name());
    pingslines_renderer.LoadSettings(ini, Name());
    symbols_renderer.LoadSettings(ini, Name());
    custom_renderer.LoadSettings(ini, Name());
    effect_renderer.LoadSettings(ini, Name());
}

void Minimap::SaveSettings(ToolboxIni *ini)
{
    ToolboxWidget::SaveSettings(ini);
    ini->SetDoubleValue(Name(), VAR_NAME(scale), scale);
    ini->SetBoolValue(Name(), VAR_NAME(hero_flag_controls_show), hero_flag_controls_show);
    ini->SetBoolValue(Name(), VAR_NAME(hero_flag_window_attach), hero_flag_window_attach);
    Colors::Save(ini, Name(), VAR_NAME(hero_flag_window_background), hero_flag_window_background);
    ini->SetBoolValue(Name(), VAR_NAME(mouse_clickthrough_in_outpost), mouse_clickthrough_in_outpost);
    ini->SetBoolValue(Name(), VAR_NAME(mouse_clickthrough_in_explorable), mouse_clickthrough_in_explorable);
    ini->SetLongValue(Name(), VAR_NAME(key_none_behavior), static_cast<long>(key_none_behavior));
    ini->SetLongValue(Name(), VAR_NAME(key_ctrl_behavior), static_cast<long>(key_ctrl_behavior));
    ini->SetLongValue(Name(), VAR_NAME(key_shift_behavior), static_cast<long>(key_shift_behavior));
    ini->SetLongValue(Name(), VAR_NAME(key_alt_behavior), static_cast<long>(key_alt_behavior));

    ini->SetBoolValue(Name(), VAR_NAME(rotate_minimap), rotate_minimap);
    ini->SetBoolValue(Name(), VAR_NAME(flip_on_reverse), flip_on_reverse);
    ini->SetBoolValue(Name(), VAR_NAME(smooth_rotation), smooth_rotation);
    ini->SetBoolValue(Name(), VAR_NAME(circular_map), circular_map);
    ini->SetBoolValue(Name(), VAR_NAME(snap_to_compass), snap_to_compass);
    ini->SetBoolValue(Name(), VAR_NAME(hide_compass_agents), hide_compass_agents);

    SAVE_BOOL(hide_compass_quest_marker);

    range_renderer.SaveSettings(ini, Name());
    pmap_renderer.SaveSettings(ini, Name());
    agent_renderer.SaveSettings(ini, Name());
    pingslines_renderer.SaveSettings(ini, Name());
    symbols_renderer.SaveSettings(ini, Name());
    custom_renderer.SaveSettings(ini, Name());
    effect_renderer.SaveSettings(ini, Name());
}

size_t Minimap::GetPlayerHeroes(const GW::PartyInfo *party, std::vector<GW::AgentID> &_player_heroes, bool* has_flags)
{
    _player_heroes.clear();
    if (!party)
        return 0;
    const uint32_t player_id = GW::PlayerMgr::GetPlayerNumber();
    if (!player_id)
        return 0;
    const GW::HeroPartyMemberArray& heroes = party->heroes;

    const bool player_is_leader = GetPlayerIsLeader();
    std::map<uint32_t, const GW::PlayerPartyMember*> party_players_by_id;
    if (player_is_leader) {
        for (const GW::PlayerPartyMember& pplayer : party->players) {
            party_players_by_id.emplace(pplayer.login_number, &pplayer);
        }
    }

    _player_heroes.reserve(heroes.size());
    for (const GW::HeroPartyMember &hero : heroes) {
        if (hero.owner_player_id == player_id)
            _player_heroes.push_back(hero.agent_id);
        else if (player_is_leader) {
            auto found = party_players_by_id.find(hero.owner_player_id);
            if (found == party_players_by_id.end() || found->second->connected())
                continue;
            if(has_flags)
                *has_flags = true;
        }
    }
    if (player_is_leader && has_flags && party->henchmen.size())
        *has_flags = true;
    else if (_player_heroes.size() && has_flags)
        *has_flags = true;
    return _player_heroes.size();
}

float Minimap::GetMapRotation() const
{
    float yaw = 1.5708f;
    if (rotate_minimap) {
        yaw = smooth_rotation ? GW::CameraMgr::GetCamera()->GetCurrentYaw() : GW::CameraMgr::GetYaw();
    }
    if (camera_currently_reversed && flip_on_reverse) {
        yaw = DirectX::XM_PI + yaw;
    }
    return yaw;
}

DirectX::XMFLOAT2 Minimap::GetGwinchScale() const
{
    return gwinch_scale;
}

void Minimap::Draw(IDirect3DDevice9 *)
{
    if (!IsActive())
        return;

    const GW::Agent *me = GW::Agents::GetPlayer();
    if (me == nullptr)
        return;

    // Check shadowstep location
    if (shadowstep_location.x != 0.0f || shadowstep_location.y != 0.0f) {
        GW::EffectArray* effects = GW::Effects::GetPlayerEffects();
        if (!effects) {
            shadowstep_location = GW::Vec2f();
        }
        else {
            bool found = false;
            for (const auto& effect : *effects) {
                found = effect.skill_id == GW::Constants::SkillID::Shadow_of_Haste || effect.skill_id == GW::Constants::SkillID::Shadow_Walk;
                if (found) break;
            }
            if (!found) {
                shadowstep_location = GW::Vec2f();
            }
        }
    }

    // if not center and want to move, move center towards player
    if ((translation.x != 0 || translation.y != 0) && (me->move_x != 0 || me->move_y != 0) && TIMER_DIFF(last_moved) > ms_before_back) {
        const GW::Vec2f v(translation.x, translation.y);
        const float speed = std::min((TIMER_DIFF(last_moved) - ms_before_back) * acceleration, 500.0f);
        GW::Vec2f d = v;
        d = GW::Normalize(d) * speed;
        if (std::abs(d.x) > std::abs(v.x)) {
            translation = GW::Vec2f(0, 0);
        } else {
            translation -= d;
        }
    }
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
    ImGui::SetNextWindowSize(ImVec2(500.0f, 500.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(Name(), nullptr, GetWinFlags(ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus))) {
        // window pos are already rounded by imgui, so casting is no big deal
        if (!snap_to_compass) {
            location.x = static_cast<int>(ImGui::GetWindowPos().x);
            location.y = static_cast<int>(ImGui::GetWindowPos().y);
            size.x = static_cast<int>(ImGui::GetWindowSize().x);
            size.y = static_cast<int>(ImGui::GetWindowSize().y);
        }
        else {
            // @Cleanup: Don't do this every frame, only when compass is relocated.
            if (!compass_frame) {
                compass_frame = GW::UI::GetWindowPosition(GW::UI::WindowID::WindowID_Compass);
            }
            else {
                const float multiplier = GuiUtils::GetGWScaleMultiplier();
                const float compass_width = compass_frame->width(multiplier);
                const float compass_padding = compass_width * .05f;
                location = { static_cast<int>(compass_frame->left(multiplier) + compass_padding),static_cast<int>(compass_frame->top(multiplier) + compass_padding) };
                size = { static_cast<int>(compass_width - (compass_padding * 2.f)) , static_cast<int>(compass_frame->height(multiplier) - (compass_padding * 2.f)) };
                ImGui::SetWindowPos({ static_cast<float>(location.x),static_cast<float>(location.y) });
                ImGui::SetWindowSize({ static_cast<float>(size.x),static_cast<float>(size.y) });
            }
        }

        clipping = {
            static_cast<LONG>(ImGui::GetWindowPos().x),
            static_cast<LONG>(ImGui::GetWindowPos().y),
            static_cast<LONG>(std::ceil(ImGui::GetWindowPos().x + ImGui::GetWindowSize().x)),
            static_cast<LONG>(std::ceil(ImGui::GetWindowPos().y + ImGui::GetWindowSize().y)),
        };
    }
    ImGui::End();
    ImGui::PopStyleColor(2);

    if (hero_flag_controls_show && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable) {

        const GW::PartyInfo* playerparty = GetPlayerParty();
        bool has_flags = false;
        GetPlayerHeroes(playerparty, player_heroes, &has_flags);

        if (has_flags) {
            if (hero_flag_window_attach) {
                ImGui::SetNextWindowPos(ImVec2(static_cast<float>(location.x), static_cast<float>(location.y + size.y)));
                ImGui::SetNextWindowSize(ImVec2(static_cast<float>(size.x), 40.0f));
            }
            ImGui::PushStyleColor(ImGuiCol_WindowBg, ImColor(hero_flag_window_background).Value);
            if (ImGui::Begin("Hero Controls", nullptr, GetWinFlags(hero_flag_window_attach ? ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove : 0, false))) {
                static const char *flag_txt[] = {"All", "1", "2", "3", "4", "5", "6", "7", "8"};
                GW::Vec3f allflag = GW::GetGameContext()->world->all_flag;
                const unsigned int num_heroflags = player_heroes.size() + 1;
                const float w_but = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * num_heroflags) /
                    (num_heroflags + 1);

                for (unsigned int i = 0; i < num_heroflags; ++i) {
                    if (i > 0)
                        ImGui::SameLine();
                    const bool is_flagging = GetFlaggingState() == i;
                    if (is_flagging)
                        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonHovered]);

                    if (ImGui::Button(flag_txt[i], ImVec2(w_but, 0))) {
                        if (is_flagging)
                            SetFlaggingState(FlaggingState::FlagState_None);
                        else
                            SetFlaggingState(static_cast<FlaggingState>(i));
                        //flagging[i] ^= 1;
                    }
                    if (is_flagging)
                        ImGui::PopStyleColor();

                    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                        if (i == 0)
                            GW::PartyMgr::UnflagAll();
                        else
                            GW::PartyMgr::FlagHeroAgent(player_heroes[i-1], GW::GamePos(HUGE_VALF, HUGE_VALF, 0));
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("Clear", ImVec2(-1, 0))) {
                    GW::PartyMgr::UnflagAll();
                    for (const uint32_t agent_id : player_heroes) {
                        GW::PartyMgr::FlagHeroAgent(agent_id, GW::GamePos(HUGE_VALF, HUGE_VALF, 0));
                    }
                }
            }
            ImGui::End();
            ImGui::PopStyleColor();
        }
    }
}

void Minimap::Render(IDirect3DDevice9* device) {
    if (compass_fix_pending) {
        // Note: Wait for a frame to pass before toggling off again to allow the game to initialise the window.
        GW::UI::SetWindowVisible(GW::UI::WindowID_Compass, false);
        compass_fix_pending = false;
    }
    auto& instance = Instance();
    if (!instance.IsActive())
        return;

    const GW::Agent* me = GW::Agents::GetPlayer();
    if (me == nullptr) return;

    // Backup the DX9 state
    IDirect3DStateBlock9* d3d9_state_block = nullptr;
    if (device->CreateStateBlock(D3DSBT_ALL, &d3d9_state_block) < 0) return;

    // Backup the DX9 transform (DX9 documentation suggests that it is included in the StateBlock but it doesn't appear to)
    D3DMATRIX reset_world, reset_view, reset_projection;
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
    const auto FillCircle = [&device](
        const float x, const float y, const float radius, const Color clr, const int resolution = 192) {
        const auto res = std::min(resolution, 192);
            D3DVertex vertices[193];
            for (auto i = 0; i <= res; i++) {
                const auto angle = i / static_cast<float>(res) * DirectX::XM_2PI;
                vertices[i] = {
                    x + radius * cos(angle),
                    y + radius * sin(angle),
                    0.0f,
                    clr
                };
            }
            device->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, res, vertices, sizeof(D3DVertex));
    };

    instance.RenderSetupProjection(device);

    const D3DCOLOR background = instance.pmap_renderer.GetBackgroundColor();
    device->SetScissorRect(&instance.clipping); // always clip to rect as a fallback if the stenciling fails
    device->SetRenderState(D3DRS_SCISSORTESTENABLE, true);
    if (instance.circular_map) {
        device->SetRenderState(D3DRS_STENCILENABLE, true); // enable stencil testing
        device->SetRenderState(D3DRS_STENCILMASK, 0xffffffff);
        device->SetRenderState(D3DRS_STENCILWRITEMASK, 0xffffffff);

        // clear depth and stencil buffer
        device->Clear(0, nullptr, D3DCLEAR_STENCIL | D3DCLEAR_ZBUFFER, 0x00000000, 1.0f, 0);

        device->SetRenderState(D3DRS_STENCILREF, 1);
        device->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_ALWAYS);
        device->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_REPLACE); // write ref value into stencil buffer
        FillCircle(0, 0, 5000.f, background); // draw circle with chosen background color into stencil buffer, fills buffer with 1's
        device->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_EQUAL); // only draw where 1 is in the buffer
        device->SetRenderState(D3DRS_STENCILFAIL, D3DSTENCILOP_ZERO);
        device->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_REPLACE);
    }
    else {
        FillRect(background, -5000.0f, -5000.0f, 10000.f, 10000.f); // fill rect with chosen background color
    }

    auto translate_char = DirectX::XMMatrixTranslation(-me->pos.x, -me->pos.y, 0);

    const auto rotate_char = DirectX::XMMatrixRotationZ(-instance.GetMapRotation() + DirectX::XM_PIDIV2);

    const auto scaleM = DirectX::XMMatrixScaling(instance.scale, instance.scale, 1.0f);
    const auto translationM = DirectX::XMMatrixTranslation(instance.translation.x, instance.translation.y, 0);

    float current_gwinch_scale = static_cast<float>(instance.size.x) / 5000.0f / 2.f * instance.scale;
    if (current_gwinch_scale != gwinch_scale.x) {
        instance.range_renderer.Invalidate();
        gwinch_scale = { current_gwinch_scale, current_gwinch_scale };
    }

    const auto view = translate_char * rotate_char * scaleM * translationM;
    device->SetTransform(D3DTS_VIEW, reinterpret_cast<const D3DMATRIX*>(&view));

    instance.pmap_renderer.Render(device);

    instance.custom_renderer.Render(device);

    // move the rings to the char position
    translate_char = DirectX::XMMatrixTranslation(me->pos.x, me->pos.y, 0);
    device->SetTransform(D3DTS_WORLD, reinterpret_cast<const D3DMATRIX*>(&translate_char));
    instance.range_renderer.Render(device);
    device->SetTransform(D3DTS_WORLD, &reset_world);

    if (instance.translation.x != 0 || instance.translation.y != 0) {
        const auto view2 = scaleM;
        device->SetTransform(D3DTS_VIEW, reinterpret_cast<const D3DMATRIX*>(&view2));
        instance.range_renderer.SetDrawCenter(true);
        instance.range_renderer.Render(device);
        instance.range_renderer.SetDrawCenter(false);
        device->SetTransform(D3DTS_VIEW, reinterpret_cast<const D3DMATRIX*>(&view));
    }

    instance.symbols_renderer.Render(device);

    device->SetTransform(D3DTS_WORLD, &reset_world);
    instance.agent_renderer.Render(device);

    instance.effect_renderer.Render(device);

    instance.pingslines_renderer.Render(device);

    if (instance.circular_map) {
        device->SetRenderState(D3DRS_STENCILREF, 0);
        device->SetRenderState(D3DRS_STENCILWRITEMASK, 0x00000000);
        device->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_NEVER);
        device->SetRenderState(D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP);
        device->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_KEEP);
        device->SetRenderState(D3DRS_STENCILENABLE, false);
    }

    // Restore the DX9 transform
    device->SetTransform(D3DTS_WORLD, &reset_world);
    device->SetTransform(D3DTS_VIEW, &reset_view);
    device->SetTransform(D3DTS_PROJECTION, &reset_projection);

    // Restore the DX9 state
    d3d9_state_block->Apply();
    d3d9_state_block->Release();
}

GW::Vec2f Minimap::InterfaceToWorldPoint(Vec2i pos) const
{
    const GW::Agent *me = GW::Agents::GetPlayer();
    if (me == nullptr)
        return {0, 0};

    GW::Vec2f v(static_cast<float>(pos.x), static_cast<float>(pos.y));

    // Invert viewport projection
    v.x = v.x - location.x;
    v.y = location.y - v.y;

    // go from [0, width][0, height] to [-1, 1][-1, 1]
    v.x = (2.0f * v.x / size.x - 1.0f);
    v.y = (2.0f * v.y / size.x + 1.0f);

    // scale up to [-w, w]
    const float w = 5000.0f;
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

GW::Vec2f Minimap::InterfaceToWorldVector(Vec2i pos) const
{
    GW::Vec2f v(static_cast<float>(pos.x), static_cast<float>(pos.y));

    // Invert y direction
    v.y = -v.y;

    // go from [0, width][0, height] to [-1, 1][-1, 1]
    v.x = (2.0f * v.x / size.x);
    v.y = (2.0f * v.y / size.x);

    // scale up to [-w, w]
    const float w = 5000.0f;
    v *= w;

    return v;
}

void Minimap::SelectTarget(const GW::Vec2f pos) const
{
    const auto* agents = GW::Agents::GetAgentArray();
    if (agents == nullptr) return;
    auto distance = 600.0f * 600.0f;
    const GW::Agent* closest = nullptr;

    for (const auto* agent : *agents) {
        if (agent == nullptr)
            continue;
        const auto* living = agent->GetAsAgentLiving();
        if (living && living->GetIsDead())
            continue;
        if (agent->GetIsItemType())
            continue;
        if (agent->GetIsGadgetType() && agent->GetAsAgentGadget()->gadget_id != 8141)
            continue; // allow locked chests
        if (!GW::Agents::GetIsAgentTargettable(agent))
            continue; // block all useless minis
        const float newDistance = GW::GetSquareDistance(pos, agent->pos);
        if (distance > newDistance) {
            distance = newDistance;
            closest = agent;
        }
    }

    if (closest != nullptr) {
        GW::Agents::ChangeTarget(closest);
    }
}

bool Minimap::WndProc(UINT Message, WPARAM wParam, LPARAM lParam)
{
    if (!GetKeyState(VK_LBUTTON) && mousedown) // fix left button being released outside of gw window
        mousedown = false;
    if (!IsActive())
        return false;
    if (is_observing)
        return false;
    if (mouse_clickthrough_in_explorable && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable) {
        if (!IsKeyDown(MinimapModifierBehaviour::Target) &&
            !IsKeyDown(MinimapModifierBehaviour::Walk) &&
            !IsKeyDown(MinimapModifierBehaviour::Move))
            return Message == WM_LBUTTONDOWN && FlagHeros(lParam);
    }
    if (mouse_clickthrough_in_outpost && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost)
        if (!IsKeyDown(MinimapModifierBehaviour::Target) && !IsKeyDown(MinimapModifierBehaviour::Walk) &&
            !IsKeyDown(MinimapModifierBehaviour::Move))
            return false;
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
bool Minimap::FlagHero(uint32_t idx) {
    if (idx > FlaggingState::FlagState_None)
        return false;
    return SetFlaggingState(static_cast<FlaggingState>(idx));
}
bool Minimap::FlagHeros(LPARAM lParam)
{
    const int x = GET_X_LPARAM(lParam);
    const int y = GET_Y_LPARAM(lParam);
    if (!IsInside(x, y))
        return false;
    bool has_flagall = false;
    GetPlayerHeroes(GetPlayerParty(), player_heroes, &has_flagall);
    const GW::Vec2f worldpos = InterfaceToWorldPoint(Vec2i(x, y));

    const FlaggingState flag_state = GetFlaggingState();
    switch (flag_state) {
    case FlaggingState::FlagState_None:
        return false;
    case FlaggingState::FlagState_All:
        if (!has_flagall)
            return false;
        SetFlaggingState(FlaggingState::FlagState_None);
        GW::PartyMgr::FlagAll(GW::GamePos(worldpos));
        return true;
    default:
        if (flag_state > player_heroes.size())
            return false;
        SetFlaggingState(FlaggingState::FlagState_None);
        GW::PartyMgr::FlagHeroAgent(player_heroes[flag_state - 1], GW::GamePos(worldpos));
        return true;
    }
}
bool Minimap::OnMouseDown(UINT Message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(Message);
    UNREFERENCED_PARAMETER(wParam);
    if (!IsActive())
        return false;
    if (FlagHeros(lParam))
        return true;
    const int x = GET_X_LPARAM(lParam);
    const int y = GET_Y_LPARAM(lParam);
    if (!IsInside(x, y))
        return false;

    mousedown = true;
    const GW::Vec2f worldpos = InterfaceToWorldPoint(Vec2i(x, y));

    if (IsKeyDown(MinimapModifierBehaviour::Target)) {
        SelectTarget(worldpos);
        return true;
    }

    if (IsKeyDown(MinimapModifierBehaviour::Walk)) {
        GW::Agents::Move(worldpos);
        pingslines_renderer.AddMouseClickPing(worldpos);
        return true;
    }

    drag_start.x = x;
    drag_start.y = y;

    if (IsKeyDown(MinimapModifierBehaviour::Move))
        return true;

    if (!lock_move)
        return true;

    pingslines_renderer.OnMouseDown(worldpos.x, worldpos.y);

    return true;
}

bool Minimap::OnMouseDblClick(UINT Message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(Message);
    UNREFERENCED_PARAMETER(wParam);
    if (!IsActive())
        return false;

    const int x = GET_X_LPARAM(lParam);
    const int y = GET_Y_LPARAM(lParam);
    if (!IsInside(x, y))
        return false;

    if (IsKeyDown(MinimapModifierBehaviour::Target)) {
        SelectTarget(InterfaceToWorldPoint(Vec2i(x, y)));
        return true;
    }

    return true;
}

bool Minimap::OnMouseUp(UINT Message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(Message);
    UNREFERENCED_PARAMETER(wParam);
    UNREFERENCED_PARAMETER(lParam);
    if (!IsActive())
        return false;

    if (!mousedown)
        return false;

    mousedown = false;

    return pingslines_renderer.OnMouseUp();
}

bool Minimap::OnMouseMove(UINT Message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(Message);
    UNREFERENCED_PARAMETER(wParam);
    UNREFERENCED_PARAMETER(lParam);
    if (!IsActive())
        return false;

    if (!mousedown)
        return false;

    const int x = GET_X_LPARAM(lParam);
    const int y = GET_Y_LPARAM(lParam);
    // if (!IsInside(x, y)) return false;

    if (IsKeyDown(MinimapModifierBehaviour::Target)) {
        SelectTarget(InterfaceToWorldPoint(Vec2i(x, y)));
        return true;
    }

    if (IsKeyDown(MinimapModifierBehaviour::Move)) {
        const Vec2i diff = Vec2i(x - drag_start.x, y - drag_start.y);
        translation += InterfaceToWorldVector(diff);
        drag_start = Vec2i(x, y);
        last_moved = TIMER_INIT();
        return true;
    }

    const GW::Vec2f v = InterfaceToWorldPoint(Vec2i(x, y));
    return pingslines_renderer.OnMouseMove(v.x, v.y);
}

bool Minimap::OnMouseWheel(UINT Message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(Message);
    UNREFERENCED_PARAMETER(lParam);
    if (!IsActive())
        return false;

    // Mouse wheel x and y are in absolute coords, not window coords! (Windows why...)
    const ImVec2 mouse = ImGui::GetMousePos();
    if (!IsInside(static_cast<int>(mouse.x), static_cast<int>(mouse.y)))
        return false;

    const int zDelta = GET_WHEEL_DELTA_WPARAM(wParam);

    if (IsKeyDown(MinimapModifierBehaviour::Move)) {
        const float delta = zDelta > 0 ? 1.024f : 0.9765625f;
        scale *= delta;
        return true;
    }

    return false;
}

bool Minimap::IsInside(int x, int y) const
{
    // if outside square, return false
    if (x < location.x)
        return false;
    if (x > location.x + size.x)
        return false;
    if (y < location.y)
        return false;
    if (y > location.y + size.y)
        return false;

    // if centered, use radar range
    if (translation.x == 0 && translation.y == 0) {
        const GW::Vec2f gamepos = InterfaceToWorldPoint(Vec2i(x, y));
        const GW::Agent *me = GW::Agents::GetPlayer();
        if (!me)
            return false;
        const float sqrdst = GW::GetSquareDistance(me->pos, gamepos);
        return sqrdst < GW::Constants::SqrRange::Compass;
    }
    return true;
}
bool Minimap::IsActive() const
{
    if (snap_to_compass) {
        if (!compass_frame) {
            compass_frame = GW::UI::GetWindowPosition(GW::UI::WindowID::WindowID_Compass);
        }
        if (compass_frame && !compass_frame->visible()) {
            return false;
        }
    }

    return visible
        && !loading
        && GW::Map::GetIsMapLoaded()
        && !GW::UI::GetIsWorldMapShowing()
        && GW::Map::GetInstanceType() != GW::Constants::InstanceType::Loading
        && GW::Agents::GetPlayerId() != 0;
}

void Minimap::RenderSetupProjection(IDirect3DDevice9 *device) const
{
    D3DVIEWPORT9 viewport;
    device->GetViewport(&viewport);

    const float w = 5000.0f * 2;
    // IMPORTANT: we are setting z-near to 0.0f and z-far to 1.0f
    const DirectX::XMMATRIX ortho_matrix(2 / w, 0, 0, 0, 0, 2 / w, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);

    //// note: manually craft the projection to viewport instead of using
    //// SetViewport to allow target regions outside the viewport
    //// e.g. negative x/y for slightly offscreen map
    const float xscale = static_cast<float>(size.x) / viewport.Width;
    const float yscale = static_cast<float>(size.x) / viewport.Height;
    const float xtrans = static_cast<float>(location.x * 2 + size.x) / viewport.Width - 1.0f;
    const float ytrans = -static_cast<float>(location.y * 2 + size.x) / viewport.Height + 1.0f;
    ////IMPORTANT: we are basically setting z-near to 0 and z-far to 1
    const DirectX::XMMATRIX viewport_matrix(xscale, 0, 0, 0, 0, yscale, 0, 0, 0, 0, 1, 0, xtrans, ytrans, 0, 1);

    const auto proj = ortho_matrix * viewport_matrix;

    device->SetTransform(D3DTS_PROJECTION, reinterpret_cast<const D3DMATRIX*>(&proj));
}

bool Minimap::IsKeyDown(const MinimapModifierBehaviour mmb) const
{
    return (key_none_behavior == mmb && !ImGui::IsKeyDown(ImGuiKey_ModCtrl) &&
        !ImGui::IsKeyDown(ImGuiKey_ModShift) &&!ImGui::IsKeyDown(ImGuiKey_ModAlt)) ||
           (key_ctrl_behavior == mmb && ImGui::IsKeyDown(ImGuiKey_ModCtrl)) ||
           (key_shift_behavior == mmb && ImGui::IsKeyDown(ImGuiKey_ModShift)) ||
           (key_alt_behavior == mmb && ImGui::IsKeyDown(ImGuiKey_ModAlt));
}
