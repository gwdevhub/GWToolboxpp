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
#include <GWCA/Managers/RenderMgr.h>
#include <GWCA/Managers/StoCMgr.h>

#include <GWCA/Utilities/Hooker.h>
#include <GWCA/Utilities/MemoryPatcher.h>
#include <GWCA/Utilities/Scanner.h>
#include <ImGuiAddons.h>
#include <Logger.h>
#include <Utils/GuiUtils.h>

#include <Defines.h>
#include <Modules/QuestModule.h>
#include <Modules/Resources.h>
#include <Utils/TextUtils.h>
#include "Minimap.h"
#include <Utils/FontLoader.h>
#include <Utils/ToolboxUtils.h>

namespace {
    GW::HookEntry ChatCmd_HookEntry;

    struct Vec2i {
        Vec2i(const int _x, const int _y) : x(_x), y(_y) {}

        Vec2i() = default;
        int x = 0;
        int y = 0;
    };

    enum class MinimapModifierBehaviour : int { Draw, Target, Drag, MoveTo };

    const char* available_modifiers_combo[] = {"None", "Ctrl", "Shift", "Alt", "Disabled"};
    constexpr uint32_t available_modifiers[] = {0, ImGuiMod_Ctrl, ImGuiMod_Shift, ImGuiMod_Alt, 0xffff};

    std::unordered_map<MinimapModifierBehaviour, uint32_t> MinimapModifierBehaviour_Keymap = {{MinimapModifierBehaviour::Draw, 0}, {MinimapModifierBehaviour::Target, 1}, {MinimapModifierBehaviour::Drag, 2}, {MinimapModifierBehaviour::MoveTo, 3}};

    enum FlaggingState : uint32_t { FlagState_All = 0, FlagState_Hero1, FlagState_Hero2, FlagState_Hero3, FlagState_Hero4, FlagState_Hero5, FlagState_Hero6, FlagState_Hero7, FlagState_None };

    bool hide_flagging_controls = false;
    bool hide_compass_when_minimap_draws = false;
    GW::MemoryPatcher hide_flagging_controls_patch;

    GW::UI::Frame* compass_frame = nullptr;
    GW::UI::UIInteractionCallback OnCompassFrame_UICallback_Ret = nullptr, OnCompassFrame_UICallback_Func = nullptr;
    bool compass_position_dirty = true;

    // Flagged when terminating minimap
    bool terminating = false;
    bool cardinal_upright = false;

    bool snap_to_compass = false;

    bool mousedown = false;
    bool camera_currently_reversed = false;

    GW::Vec2f shadowstep_location = {0.f, 0.f};

    Vec2i drag_start;

    // vars for minimap movement
    clock_t last_moved = 0;

    /**
     * Wrap runtime variables in o MinimapRenderContext - expose them as refs for our internal settings
     */
    MinimapRenderContext default_minimap_context{
        .background_color = 0,
        .foreground_color = 0xFF999999,
        .shadow_color = 0xFF120808, 
        .cardinal_color = 0xffffffff, 
        .draw_ranges = true
    };
    GW::Vec2f& translation = default_minimap_context.translation;
    bool& circular_map = default_minimap_context.circular_map;
    Color& color_map = reinterpret_cast<Color&>(default_minimap_context.foreground_color);
    Color& color_mapshadow = reinterpret_cast<Color&>(default_minimap_context.shadow_color);
    Color& color_mapbackground = reinterpret_cast<Color&>(default_minimap_context.background_color);
    float& scale = default_minimap_context.zoom_scale;

    bool loading = false; // only consider some cases but still good
    bool compass_fix_pending = false;
    bool mouse_clickthrough_in_explorable = false;
    bool mouse_clickthrough_in_outpost = false;
    bool flip_on_reverse = false;
    bool rotate_minimap = true;
    bool smooth_rotation = true;
    bool is_observing = false;
    bool hero_flag_controls_show = false;
    bool hero_flag_window_attach = true;
    Color hero_flag_controls_background = 0;
    std::vector<GW::AgentID> player_heroes{};

    using DrawCompassAgentsByType_pt = uint32_t(__fastcall*)(void* ecx, void* edx, uint32_t param_1, uint32_t param_2, uint32_t flags);
    DrawCompassAgentsByType_pt DrawCompassAgentsByType_Func = nullptr;
    DrawCompassAgentsByType_pt DrawCompassAgentsByType_Ret = nullptr;

    bool hide_compass_agents = false;
    bool hide_compass_drawings = false;
    bool hide_compass_quest_marker = false;
    bool render_all_quests = false;

    bool in_interface_settings = false;

    // -------------------------------------------------------------------------
    // Cardinal direction labels (N / S / E / W)
    // -------------------------------------------------------------------------
    Color& cardinal_color = reinterpret_cast<Color&>(default_minimap_context.cardinal_color);
    float cardinal_offset = 0.0f;     // game-unit offset from compass edge; +ve = outward
    float cardinal_font_size = 20.0f; // screen-space font size in pixels

    // Projects a game-world position to an ImGui screen pixel, using the same
    // transform chain that Minimap::Render / RenderSetupProjection applies.
    //
    // View chain (Minimap::Render):
    //   translate(-me->pos) * rotateZ(-rotation + PI/2) * scale(zoom) * translate(pan)
    // Projection (RenderSetupProjection):
    //   ortho: ±5000 game units → ±1 NDC
    //   viewport: NDC → pixels via base_scale and anchor_point
    ImVec2 WorldToScreen(const GW::Vec2f& world_pos, const MinimapRenderContext& ctx, const GW::Vec2f& me_pos)
    {
        GW::Vec2f v = world_pos - me_pos; // translate so player is at origin
        v += ctx.translation;             // apply pan
        v *= ctx.zoom_scale;              // apply zoom

        // Rotate: view uses RotationZ(-rotation + PI/2)
        const float angle = DirectX::XM_PIDIV2 - ctx.rotation;
        const float rx = v.x * std::cos(angle) - v.y * std::sin(angle);
        const float ry = v.x * std::sin(angle) + v.y * std::cos(angle);

        // Orthographic: ±5000 game units → ±1 NDC
        const float nx = rx * (2.0f / 10000.0f);
        const float ny = ry * (2.0f / 10000.0f);

        // NDC → screen pixels (Y flipped: +Y world = up = smaller screen Y)
        const float half = ctx.base_scale * 0.5f;
        return {ctx.anchor_point.x + nx * half, ctx.anchor_point.y - ny * half};
    }


    struct CompassAiControl {
        uint32_t field0_0x0;
        uint32_t field1_0x4;
        uint32_t field2_0x8;
        uint32_t field3_0xc;
        uint32_t field4_0x10;
        uint32_t field5_0x14;
        uint32_t field6_0x18;
    };

    static_assert(sizeof(CompassAiControl) == 0x1c);

    struct CompassContext {
        FlaggingState flagging_state;
        uint32_t field1_0x4;
        uint32_t field2_0x8;
        uint32_t field3_0xc;
        uint32_t field4_0x10;
        uint32_t field5_0x14;
        uint32_t field6_0x18;
        uint32_t frame_id;
        uint32_t field8_0x20;
        uint32_t field9_0x24;
        uint32_t field10_0x28;
        uint32_t field11_0x2c;
        uint32_t field12_0x30;
        uint32_t field13_0x34;
        uint32_t field14_0x38;
        uint32_t field15_0x3c;
        CompassAiControl* ai_controls;
        void* compass_canvas; // size 0x138
        uint32_t field18_0x48;
        uint32_t field19_0x4c;
        uint32_t field20_0x50;
        uint32_t field21_0x54;
        uint32_t field22_0x58;
        uint32_t field23_0x5c;
    };

    static_assert(sizeof(CompassContext) == 0x60);

    GW::HookEntry Generic_HookEntry;

    Minimap& Instance()
    {
        return Minimap::Instance();
    }

    GW::Vec2f InterfaceToWorldPoint(const Vec2i& pos)
    {
        const GW::Agent* me = GW::Agents::GetObservingAgent();
        if (me == nullptr) {
            return {0, 0};
        }

        GW::Vec2f v(static_cast<float>(pos.x), static_cast<float>(pos.y));

        // Translate so the minimap center is the origin, and flip Y
        const auto size = default_minimap_context.size();
        v.x = v.x - (default_minimap_context.top_left.x + size.x * 0.5f);
        v.y = (default_minimap_context.top_left.y + size.y * 0.5f) - v.y;

        // Scale to game units: base_scale (= size.x) spans ±5000
        constexpr float w = 5000.0f;
        v *= 2.0f * w / size.x;

        // translate by camera
        v -= translation;

        // scale by camera
        v /= scale;

        // rotate by current camera rotation
        const float angle = default_minimap_context.rotation - DirectX::XM_PIDIV2;
        const float x1 = v.x * std::cos(angle) - v.y * std::sin(angle);
        const float y1 = v.x * std::sin(angle) + v.y * std::cos(angle);
        v = GW::Vec2f(x1, y1);

        // translate by character position
        v += me->pos;

        return v;
    }

    GW::Vec2f InterfaceToWorldVector(const Vec2i& pos)
    {
        GW::Vec2f v(static_cast<float>(pos.x), static_cast<float>(pos.y));

        // Flip Y (no translation needed for vectors)
        v.y = -v.y;

        // Scale to game units: base_scale (= size.x) spans ±5000
        const auto size = default_minimap_context.size();
        constexpr float w = 5000.0f;
        v *= 2.0f * w / size.x;

        return v;
    }

    // Just send the UI message to update frames, bypassing use settings.
    bool SetWindowVisibleTmp(GW::UI::WindowID window_id, bool visible)
    {
        auto position = GW::UI::GetWindowPosition(window_id);
        if (!position) return false;
        auto original_position = *position;
        GW::UI::UIPacket::kUIPositionChanged packet = {window_id, position};
        if (visible) {
            position->state |= 1;
        }
        else {
            position->state &= ~1;
        }
        // Swap position out, send UI message to cascade to frames, then set back to original
        GW::UI::SendUIMessage(GW::UI::UIMessage::kUIPositionChanged, &packet);
        *position = original_position;
        return true;
    }

    bool pending_compass_hide = false;

    GW::UI::Frame* GetCompassFrame();

    bool EnsureCompassIsLoaded()
    {
        if (!GW::Map::GetIsMapLoaded()) return false;
        const auto compass = GetCompassFrame();
        if (!compass) return false;
        const auto context = static_cast<CompassContext*>(GW::UI::GetFrameContext(compass));
        if (!context->compass_canvas) {
            SetWindowVisibleTmp(GW::UI::WindowID_Compass, true);
            pending_compass_hide = true;
        }
        return context->compass_canvas;
    }

    bool ResetWindowPosition(GW::UI::WindowID, GW::UI::Frame*);
    bool RepositionMinimapToCompass();


    // Check whether the compass ought to be hidden or not depending on user settings
    bool OverrideCompassVisibility()
    {
        const auto frame = GetCompassFrame();
        if (!(frame && frame->IsCreated() && !in_interface_settings)) return false;
        const auto context = static_cast<CompassContext*>(GW::UI::GetFrameContext(frame));
        if (!(context && context->compass_canvas)) return false;
        if (pending_compass_hide) {
            SetWindowVisibleTmp(GW::UI::WindowID_Compass, false);
            pending_compass_hide = false;
        }

        if (hide_compass_when_minimap_draws && Minimap::IsActive()) {
            if (!(frame->IsCreated() && frame->IsVisible())) return false;
            return GW::UI::SetFrameVisible(frame, false);
        }
        return ResetWindowPosition(GW::UI::WindowID_Compass, frame);
    }

    // If we've messed around with the window visibility, reset it here.
    bool ResetWindowPosition(GW::UI::WindowID window_id, GW::UI::Frame* frame)
    {
        if (in_interface_settings) return false;
        GW::UI::UIPacket::kUIPositionChanged packet = {window_id, GW::UI::GetWindowPosition(window_id)};
        if (frame && packet.position && frame->IsCreated() && frame->IsVisible() != packet.position->visible()) return GW::UI::SendUIMessage(GW::UI::UIMessage::kUIPositionChanged, &packet);
        return false;
    }

    CompassContext* compass_context = nullptr;

    void __cdecl OnCompassFrame_UICallback(GW::UI::InteractionMessage* message, void* wParam, void* lParam)
    {
        GW::Hook::EnterHook();

        compass_context = message->wParam ? *(CompassContext**)message->wParam : nullptr;
        switch (message->message_id) {
            case GW::UI::UIMessage::kFrameMessage_0x44: {
                if (OverrideCompassVisibility()) {
                    break;
                }
                OnCompassFrame_UICallback_Ret(message, wParam, lParam);
            } break;
            case GW::UI::UIMessage::kResize: {
                // NB: Resize packet creates flagging controls and compass canvas
                OnCompassFrame_UICallback_Ret(message, wParam, lParam);
                OverrideCompassVisibility();
            } break;
            case GW::UI::UIMessage::kFrameMessage_0x4a: // 0x4a need to pass through to allow hotkey flagging
                OnCompassFrame_UICallback_Ret(message, wParam, lParam);
                break;
            case GW::UI::UIMessage::kDestroyFrame:
                OnCompassFrame_UICallback_Ret(message, wParam, lParam);
                compass_context = nullptr;
                compass_frame = nullptr;
                compass_fix_pending = false;
                compass_position_dirty = true;
                break;
            case GW::UI::UIMessage::kFrameMessage_0x13:
            case GW::UI::UIMessage::kRenderFrame_0x30:
            case GW::UI::UIMessage::kFrameVisibilityChanged:
            case GW::UI::UIMessage::kSetLayout:
                OnCompassFrame_UICallback_Ret(message, wParam, lParam);
                compass_position_dirty = true; // Forces a recalculation
                break;
            case GW::UI::UIMessage::kQuestAdded:
            case GW::UI::UIMessage::kClientActiveQuestChanged:
            case GW::UI::UIMessage::kServerActiveQuestChanged:
            case GW::UI::UIMessage::kUnknownQuestRelated:
                if (!hide_compass_quest_marker) {
                    OnCompassFrame_UICallback_Ret(message, wParam, lParam);
                }
                else {
                    const auto prev = message->message_id;
                    message->message_id = GW::UI::UIMessage::kQuestRemoved;
                    OnCompassFrame_UICallback_Ret(message, wParam, lParam);
                    message->message_id = prev;
                }
                break;
            default:
                if (compass_context && hide_flagging_controls) {
                    // Temporarily nullify the pointer to flagging controls for all other message ids
                    const auto prev = compass_context->ai_controls;
                    compass_context->ai_controls = nullptr;
                    OnCompassFrame_UICallback_Ret(message, wParam, lParam);
                    compass_context->ai_controls = prev;
                }
                else {
                    OnCompassFrame_UICallback_Ret(message, wParam, lParam);
                }
                break;
        }

        GW::Hook::LeaveHook();
    }

    GW::UI::Frame* GetCompassFrame()
    {
        if (compass_frame) return compass_frame;
        compass_frame = GW::UI::GetFrameByLabel(L"Compass");
        if (compass_frame) {
            ASSERT(compass_frame->frame_callbacks.size());
            if (!OnCompassFrame_UICallback_Func) {
                OnCompassFrame_UICallback_Func = compass_frame->frame_callbacks[0].callback;
                GW::Hook::CreateHook((void**)&OnCompassFrame_UICallback_Func, OnCompassFrame_UICallback, reinterpret_cast<void**>(&OnCompassFrame_UICallback_Ret));
                GW::Hook::EnableHooks(OnCompassFrame_UICallback_Func);
            }
            compass_position_dirty = true;
        }
        return compass_frame;
    }


    bool IsKeyDown(MinimapModifierBehaviour mmb)
    {
        const auto modkey = available_modifiers[MinimapModifierBehaviour_Keymap[mmb]];
        if (modkey == 0) {
            return !ImGui::IsKeyDown(ImGuiMod_Ctrl) && !ImGui::IsKeyDown(ImGuiMod_Shift) && !ImGui::IsKeyDown(ImGuiMod_Alt) && !ImGui::IsKeyDown(ImGuiMod_Shift);
        }
        if (modkey == 0xffff) return false;
        return ImGui::IsKeyDown(modkey);
    }
    bool IsAnyModifierKeyDown()
    {
        for (auto& it : MinimapModifierBehaviour_Keymap) {
            if (IsKeyDown(it.first)) return true;
        }
        return false;
    }

    FlaggingState GetFlaggingState()
    {
        return compass_context ? compass_context->flagging_state : FlagState_None;
    }

    bool SetFlaggingState(FlaggingState set_state)
    {
        if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Explorable) {
            return false;
        }
        // keep an internal flag for the minimap flagging until StartCaptureMouseClick_Func is working
        // minimap_flagging_state = set_state;
        if (GetFlaggingState() == set_state) {
            return true;
        }
        if (set_state == FlagState_None) {
            set_state = GetFlaggingState();
        }
        GW::UI::ControlAction key;
        switch (set_state) {
            case FlagState_Hero1:
                key = GW::UI::ControlAction_CommandHero1;
                break;
            case FlagState_Hero2:
                key = GW::UI::ControlAction_CommandHero2;
                break;
            case FlagState_Hero3:
                key = GW::UI::ControlAction_CommandHero3;
                break;
            case FlagState_Hero4:
                key = GW::UI::ControlAction_CommandHero4;
                break;
            case FlagState_Hero5:
                key = GW::UI::ControlAction_CommandHero5;
                break;
            case FlagState_Hero6:
                key = GW::UI::ControlAction_CommandHero6;
                break;
            case FlagState_Hero7:
                key = GW::UI::ControlAction_CommandHero7;
                break;
            case FlagState_All:
                key = GW::UI::ControlAction_CommandParty;
                break;
            default:
                return false;
        }
        return Keypress(key);
    }

    // Same as GW::PartyMgr::GetPlayerIsLeader() but has an extra check to ignore disconnected people.
    bool GetPlayerIsLeader()
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

    uint32_t __fastcall OnDrawCompassAgentsByType(void* ecx, void* edx, const uint32_t param_1, const uint32_t param_2, const uint32_t flags)
    {
        GW::Hook::EnterHook();
        uint32_t result = 0;
        if (!hide_compass_agents) {
            result = DrawCompassAgentsByType_Ret(ecx, edx, param_1, param_2, flags);
        }
        GW::Hook::LeaveHook();
        return result;
    }

    bool pending_refresh_quest_marker = true;

    bool RefreshQuestMarker()
    {
        const auto frame = GetCompassFrame();
        if (!(frame && frame->IsCreated())) return false;
        if (const auto quest = GW::QuestMgr::GetActiveQuest()) {
            struct QuestUIMsg {
                GW::Constants::QuestID quest_id{};
                GW::GamePos marker{};
                uint32_t h0024{};
                GW::Constants::MapID map_to{};
                uint32_t log_state{};
            } msg;
            msg.quest_id = quest->quest_id;
            msg.marker = quest->marker;
            msg.h0024 = quest->h0024;
            msg.map_to = quest->map_to;
            msg.log_state = quest->log_state;

            GW::GameThread::Enqueue([msg]() mutable {
                SendUIMessage(GW::UI::UIMessage::kClientActiveQuestChanged, &msg);
            });
        }
        return true;
    }

    void PreloadQuestMarkers()
    {
        QuestModule::FetchMissingQuestInfo();
    }


    // Callbacks
    void OnKeydown(GW::HookStatus*, const uint32_t key)
    {
        if (key == GW::UI::ControlAction_ReverseCamera) {
            camera_currently_reversed = true;
        }
    }

    void OnKeyup(GW::HookStatus*, const uint32_t key)
    {
        if (key == GW::UI::ControlAction_ReverseCamera) {
            camera_currently_reversed = false;
        }
    }

    void OnAgentPinged(GW::HookStatus*, const GW::Packet::StoC::AgentPinged* pak)
    {
        if (!Instance().visible) return;
        Instance().pingslines_renderer.P046Callback(pak);
    }

    void OnPlayEffect(const GW::HookStatus*, GW::Packet::StoC::PlayEffect* pak)
    {
        if (!(Instance().visible && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable)) return;
        Instance().effect_renderer.PacketCallback(pak);
    }

    void OnGenericValue(const GW::HookStatus*, const GW::Packet::StoC::GenericValue* pak)
    {
        if (!(Instance().visible && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable)) return;
        Instance().effect_renderer.PacketCallback(pak);
    }

    void OnGenericValueTarget(GW::HookStatus*, const GW::Packet::StoC::GenericValueTarget* pak)
    {
        if (!Instance().visible) return;
        Instance().pingslines_renderer.P153Callback(pak);
        if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable) Instance().effect_renderer.PacketCallback(pak);
    }

    bool RepositionMinimapToCompass()
    {
        if (!snap_to_compass) return false;
        const auto frame = GetCompassFrame();
        if (!frame) return false;
        constexpr float compass_padding = 1.05f;
        auto top_left = frame->position.GetTopLeftOnScreen(frame);
        auto bottom_right = frame->position.GetBottomRightOnScreen(frame);

        const auto height = (bottom_right.y - top_left.y);
        const auto diff = height - (height / compass_padding);
        top_left.y += diff;
        top_left.x += diff;
        bottom_right.x -= diff;
        bottom_right.y = top_left.y + (bottom_right.x - top_left.x);

        default_minimap_context.top_left = top_left;
        default_minimap_context.bottom_right = bottom_right;
        ImGui::SetWindowPos(default_minimap_context.top_left);
        ImGui::SetWindowSize(default_minimap_context.size());
        return true;
    }

    void DrawNSEW(const MinimapRenderContext& context)
    {
        if ((context.cardinal_color & IM_COL32_A_MASK) == 0) return;
        // -------------------------------------------------------------------------
        // Cardinal direction labels (N / S / E / W) — ImGui background draw list
        //
        // Font is chosen via FontLoader::GetFontByPx so we always render at a
        // baked atlas size and avoid sub-pixel scaling blur.
        //
        // Two modes controlled by cardinal_upright:
        //   true  — labels always read upright relative to the screen (default).
        //   false — labels rotate with the minimap; each letter faces outward
        //           from the compass centre so it reads correctly when the map
        //           is rotated.
        // -------------------------------------------------------------------------
        ImDrawList* draw_list = ImGui::GetBackgroundDrawList();

        // Pick the smallest baked font >= the requested size.
        // Render at its native FontSize — no scaling artefacts.
        ImFont* font = FontLoader::GetFontByPx(cardinal_font_size, true);
        const float render_size = font->FontSize;
        const float text_scale = cardinal_font_size / render_size; // always 1.0f; kept for clarity

        const float radius = GW::Constants::Range::Compass + cardinal_offset;

        // Convert GWToolbox ARGB (0xAARRGGBB) → ImGui ABGR (0xAABBGGRR)
        const ImU32 label_col = context.cardinal_color;
        const ImU32 shadow_col = IM_COL32(0, 0, 0, (context.cardinal_color >> 24) & 0xFF);

        // GW world axes: +X = east, +Y = north.
        // label_angle: the angle (screen-space, CW from up) that this label's
        //   top should point toward when in rotated mode.
        //   N faces map-up   → base map angle
        //   E faces map-right → base + PI/2
        //   S faces map-down  → base + PI
        //   W faces map-left  → base + 3*PI/2
        //
        // In upright mode label_angle is unused.
        const float map_angle = context.rotation - DirectX::XM_PIDIV2;

        struct Cardinal {
            const char* text;
            GW::Vec2f offset;
            float label_angle; // per-label rotation for rotated mode
        };
        const Cardinal cardinals[] = {
            {"N", {0.f, radius}, map_angle},
            {"S", {0.f, -radius}, map_angle + DirectX::XM_PI},
            {"E", {radius, 0.f}, map_angle + DirectX::XM_PIDIV2},
            {"W", {-radius, 0.f}, map_angle + DirectX::XM_PI + DirectX::XM_PIDIV2},
        };

        auto cardinal_offset_scale = context.base_scale * 0.5f / GW::Constants::Range::Compass;
        if (cardinal_upright) {
            // ------------------------------------------------------------------
            // Fast path: axis-aligned text via AddText.
            // ------------------------------------------------------------------
            for (const auto& c : cardinals) {
                auto offset = GW::Rotate(c.offset * cardinal_offset_scale, -map_angle);
                const ImVec2 screen = {context.anchor_point.x + offset.x, context.anchor_point.y - offset.y};
                const ImVec2 text_size = font->CalcTextSizeA(render_size, FLT_MAX, 0.f, c.text);
                const ImVec2 draw_pos = {screen.x - text_size.x * 0.5f, screen.y - text_size.y * 0.5f};

                // 1-pixel drop shadow for legibility on any background
                draw_list->AddText(font, render_size, {draw_pos.x + 1.f, draw_pos.y + 1.f}, shadow_col, c.text);
                draw_list->AddText(font, render_size, draw_pos, label_col, c.text);
            }
        }
        else {
            // ------------------------------------------------------------------
            // Rotated path: emit glyph quads manually with a per-label angle
            // so each letter faces outward from its compass point.
            // ------------------------------------------------------------------

            // Emit one glyph string centred on `pivot`, rotated by `angle`.
            // The shadow offset is 1 px in the label's own "down" direction.
            auto AddTextRotated = [&](ImVec2 pivot, ImU32 col, const char* text, float angle) {
                const float ca = std::cos(angle);
                const float sa = std::sin(angle);

                // Rotate local offset (ox, oy) around pivot
                auto rot = [&](float ox, float oy) -> ImVec2 {
                    return {pivot.x + ox * ca - oy * sa, pivot.y + ox * sa + oy * ca};
                };

                const float half_h = font->FontSize * text_scale * 0.5f;

                // Total advance width for horizontal centring
                float total_w = 0.f;
                for (const char* p = text; *p;) {
                    unsigned int cp;
                    p += ImTextCharFromUtf8(&cp, p, nullptr);
                    const ImFontGlyph* g = font->FindGlyph(static_cast<ImWchar>(cp));
                    if (g) total_w += g->AdvanceX * text_scale;
                }

                draw_list->PushTextureID(font->ContainerAtlas->TexID);

                float cursor_x = -total_w * 0.5f;
                for (const char* p = text; *p;) {
                    unsigned int cp;
                    p += ImTextCharFromUtf8(&cp, p, nullptr);
                    const ImFontGlyph* g = font->FindGlyph(static_cast<ImWchar>(cp));
                    if (!g) continue;

                    const float x0 = cursor_x + g->X0 * text_scale;
                    const float y0 = -half_h + g->Y0 * text_scale;
                    const float x1 = cursor_x + g->X1 * text_scale;
                    const float y1 = -half_h + g->Y1 * text_scale;

                    draw_list->PrimReserve(6, 4);
                    draw_list->PrimQuadUV(rot(x0, y0), rot(x1, y0), rot(x1, y1), rot(x0, y1), {g->U0, g->V0}, {g->U1, g->V0}, {g->U1, g->V1}, {g->U0, g->V1}, col);

                    cursor_x += g->AdvanceX * text_scale;
                }

                draw_list->PopTextureID();
            };

            for (const auto& c : cardinals) {
                auto offset = GW::Rotate(c.offset * cardinal_offset_scale, -map_angle);
                const ImVec2 screen = {context.anchor_point.x + offset.x, context.anchor_point.y - offset.y};

                // Shadow: 1 px along the label's own screen-down direction
                const float ca_s = std::cos(c.label_angle);
                const float sa_s = std::sin(c.label_angle);
                const ImVec2 shadow_pivot = {screen.x + sa_s, screen.y - ca_s};

                AddTextRotated(shadow_pivot, shadow_col, c.text, c.label_angle);
                AddTextRotated(screen, label_col, c.text, c.label_angle);
            }
        }
    }

    bool ShouldClickThrough() {
        return ToolboxUtils::IsExplorable() ? mouse_clickthrough_in_explorable : mouse_clickthrough_in_outpost;
    }
} // namespace

void Minimap::DrawHelp()
{
    if (!ImGui::TreeNodeEx("Minimap", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
        return;
    }

    ImGui::Bullet();
    ImGui::Text("'/marktarget' highlights the current target on your minimap.");
    ImGui::Bullet();
    ImGui::Text("'/marktarget clear' removes the current target as a marked target on your minimap.");
    ImGui::Bullet();
    ImGui::Text("'/clearmarktarget clear' removes all marked target highlights on your minimap.");
    ImGui::TreePop();
}

void Minimap::SignalTerminate()
{
    terminating = true;
    GW::GameThread::Enqueue([] {
        RefreshQuestMarker();
        ResetWindowPosition(GW::UI::WindowID_Compass, compass_frame);
        terminating = false;
    });
}

void Minimap::Terminate()
{
    GW::UI::RemoveKeydownCallback(&Generic_HookEntry);
    GW::UI::RemoveKeyupCallback(&Generic_HookEntry);
    GW::StoC::RemoveCallbacks(&Generic_HookEntry);
    GW::UI::RemoveUIMessageCallback(&Generic_HookEntry);

    GW::Hook::RemoveHook(DrawCompassAgentsByType_Func);

    GW::Chat::DeleteCommand(&ChatCmd_HookEntry);

    hide_flagging_controls_patch.Reset();

    ToolboxWidget::Terminate();
    range_renderer.Terminate();
    pmap_renderer.Terminate();
    agent_renderer.Terminate();
    pingslines_renderer.Terminate();
    symbols_renderer.Terminate();
    custom_renderer.Terminate();
    effect_renderer.Terminate();
    GameWorldRenderer::Terminate();
}

bool Minimap::CanTerminate()
{
    return !terminating;
}

void Minimap::Initialize()
{
    ToolboxWidget::Initialize();

    uintptr_t address = GW::Scanner::Find("\x8b\x46\x40\x85\xc0\x74\x0c", "xxxxx?x", 0x5);
    if (address) {
        hide_flagging_controls_patch.SetPatch(address, "\xeb", 1);
    }

    DrawCompassAgentsByType_Func = (DrawCompassAgentsByType_pt)GW::Scanner::FunctionFromNearCall(GW::Scanner::Find("\x68\x00\x09\x14\x00\x57\x56\xe8", "xxxxxxxx", 7));

#ifdef _DEBUG
    ASSERT(DrawCompassAgentsByType_Func);
    ASSERT(hide_flagging_controls_patch.IsValid());
#endif

    if (DrawCompassAgentsByType_Func) {
        GW::Hook::CreateHook((void**)&DrawCompassAgentsByType_Func, OnDrawCompassAgentsByType, (void**)&DrawCompassAgentsByType_Ret);
        GW::Hook::EnableHooks(DrawCompassAgentsByType_Func);
    }

    pending_refresh_quest_marker = true;

    GW::UI::RegisterKeydownCallback(&Generic_HookEntry, OnKeydown);
    GW::UI::RegisterKeyupCallback(&Generic_HookEntry, OnKeyup);

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::AgentPinged>(&Generic_HookEntry, OnAgentPinged);
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::PlayEffect>(&Generic_HookEntry, OnPlayEffect);
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GenericValue>(&Generic_HookEntry, OnGenericValue);
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GenericValueTarget>(&Generic_HookEntry, OnGenericValueTarget);
    constexpr std::array hook_messages = {GW::UI::UIMessage::kMapChange,
                                          GW::UI::UIMessage::kMapLoaded,
                                          GW::UI::UIMessage::kChangeTarget,
                                          GW::UI::UIMessage::kSkillActivated,
                                          GW::UI::UIMessage::kCompassDraw,
                                          GW::UI::UIMessage::kEnableUIPositionOverlay,
                                          GW::UI::UIMessage::kDestroyUIPositionOverlay};
    for (const auto message_id : hook_messages) {
        RegisterUIMessageCallback(&Generic_HookEntry, message_id, OnUIMessage, 0x8000);
    }

    if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Loading) {
        PreloadQuestMarkers();
    }

    last_moved = TIMER_INIT();

    pmap_renderer.Invalidate();
    range_renderer.Invalidate();

    GW::Chat::CreateCommand(&ChatCmd_HookEntry, L"flag", &OnFlagHeroCmd);
    GW::GameThread::Enqueue(EnsureCompassIsLoaded);
}

void Minimap::OnUIMessage(GW::HookStatus* status, const GW::UI::UIMessage msgid, void* wParam, void* lParam)
{
    auto& instance = Instance();
    instance.pingslines_renderer.OnUIMessage(status, msgid, wParam, lParam);
    switch (msgid) {
        case GW::UI::UIMessage::kEnableUIPositionOverlay:
        case GW::UI::UIMessage::kDestroyUIPositionOverlay:
            in_interface_settings = (uint32_t)wParam == 1;
            compass_position_dirty = true;
            break;
        case GW::UI::UIMessage::kCompassDraw: {
            ASSERT(wParam);
            if (hide_compass_drawings) status->blocked = true;
        } break;
        case GW::UI::UIMessage::kMapLoaded: {
            in_interface_settings = false;
            EnsureCompassIsLoaded();
            instance.pmap_renderer.Invalidate();
            GameWorldRenderer::TriggerSyncAllMarkers();
            loading = false;
            is_observing = GW::Map::GetIsObserving();
            // Cycle active quests to cache their markers
            PreloadQuestMarkers();
            pending_refresh_quest_marker = true;
        } break;
        case GW::UI::UIMessage::kSkillActivated: {
            const auto packet = static_cast<GW::UI::UIPacket::kAgentSkillPacket*>(wParam);
            ASSERT(packet && packet->skill_id < GW::Constants::SkillID::Count && packet->agent_id);
            if (packet->agent_id == GW::Agents::GetControlledCharacterId()) {
                if (packet->skill_id == GW::Constants::SkillID::Shadow_of_Haste || packet->skill_id == GW::Constants::SkillID::Shadow_Walk) {
                    shadowstep_location = GW::Agents::GetControlledCharacter()->pos;
                }
            }
        } break;
        case GW::UI::UIMessage::kMapChange: {
            loading = true;
            instance.agent_renderer.auto_target_id = 0;
        } break;
        case GW::UI::UIMessage::kChangeTarget: {
            const auto msg = static_cast<GW::UI::UIPacket::kChangeTarget*>(wParam);
            instance.agent_renderer.auto_target_id = GW::Agents::GetTargetId() ? 0 : msg->auto_target_id;
        } break;
        default:
            break;
    }
}

GW::Vec2f Minimap::ShadowstepLocation() const
{
    return shadowstep_location;
}

void CHAT_CMD_FUNC(Minimap::OnFlagHeroCmd)
{
    if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Explorable) {
        return; // Not explorable - "/flag" can be typed in chat to bypass flag hero buttons, so this is needed.
    }
    if (argc <= 1) {
        FlagHero(0); // "/flag"
        return;
    }
    const auto is_flagged = [](auto hero) -> bool {
        if (hero == 0) {
            const GW::Vec3f& allflag = GW::GetGameContext()->world->all_flag;
            if (allflag.x != 0 && allflag.y != 0 && (!std::isinf(allflag.x) || !std::isinf(allflag.y))) {
                return true;
            }
        }
        else {
            const GW::HeroFlagArray& flags = GW::GetGameContext()->world->hero_flags;
            if (!flags.valid() || static_cast<uint32_t>(hero) > flags.size()) {
                return false;
            }

            const GW::HeroFlag& flag = flags[hero - 1];
            if (!std::isinf(flag.flag.x) || !std::isinf(flag.flag.y)) {
                return true;
            }
        }
        return false;
    };

    const std::wstring arg1 = TextUtils::ToLower(argv[1]);
    float x;
    float y;
    unsigned int n_heros = 0; // Count of heros available
    unsigned int f_hero = 0;  // Hero number to flag
    if (arg1 == L"all" || arg1 == L"0") {
        if (argc < 3) {
            FlagHero(0); // "/flag all" == "/flag"
            return;
        }
        const std::wstring arg2 = TextUtils::ToLower(argv[2]);
        if (arg2 == L"clear") {
            GW::PartyMgr::UnflagAll(); // "/flag 5 clear"
            return;
        }
        if (arg2 == L"toggle") {
            // "/flag 5 toggle"
            if (is_flagged(0)) {
                GW::PartyMgr::UnflagAll();
            }
            else {
                FlagHero(0);
            }
            return;
        }
        if (argc < 4 || !TextUtils::ParseFloat(argv[2], &x) || !TextUtils::ParseFloat(argv[3], &y)) {
            Log::Error("Please provide command in format /flag all [x] [y]"); // Not enough args or coords not valid float vals.
            return;
        }
        GW::PartyMgr::FlagAll(GW::GamePos(x, y, 0)); // "/flag all -2913.41 3004.78"
        return;
    }
    const auto& heroarray = GW::GetGameContext()->party->player_party->heroes;

    if (heroarray.valid()) {
        n_heros = heroarray.size();
    }
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
    if (!TextUtils::ParseUInt(argv[1], &f_hero) || f_hero < 1 || f_hero > n_heros) {
        Log::Error("Invalid hero number");
        return; // Invalid hero number
    }
    if (argc < 3) {
        FlagHero(f_hero); // "/flag 5"
        return;
    }
    const std::wstring arg2 = TextUtils::ToLower(argv[2]);
    if (arg2 == L"clear") {
        GW::PartyMgr::UnflagHero(f_hero); // "/flag 5 clear"
        return;
    }
    if (arg2 == L"toggle") {
        // "/flag 5 toggle"
        if (is_flagged(f_hero)) {
            GW::PartyMgr::UnflagHero(f_hero);
        }
        else {
            FlagHero(f_hero);
        }
        return;
    }
    if (argc < 4 || !TextUtils::ParseFloat(argv[2], &x) || !TextUtils::ParseFloat(argv[3], &y)) {
        Log::Error("Please provide command in format /flag [hero number] [x] [y]"); // Invalid coords
        return;
    }
    GW::PartyMgr::FlagHero(f_hero, GW::GamePos(x, y, 0)); // "/flag 5 -2913.41 3004.78"
}

void Minimap::DrawSettingsInternal()
{
    if (snap_to_compass) {
        ImGui::NextSpacedElement();
    }
    if (ImGui::Checkbox("Snap to GW compass", &snap_to_compass)) {
        compass_position_dirty = true;
    }
    ImGui::ShowHelp("Resize and position minimap to match in-game compass size and position.");
    ImGui::Checkbox("Hide GW compass agents", &hide_compass_agents);
    if (ImGui::Checkbox("Hide GW compass quest marker", &hide_compass_quest_marker)) {
        pending_refresh_quest_marker = true;
    }
    ImGui::ShowHelp("To disable the toolbox minimap quest marker, set the quest marker color to transparent in the Symbols section below.");
    ImGui::Checkbox("Draw all quest markers", &render_all_quests);
    ImGui::ShowHelp("Draw quest markers for all quests in your quest log, not just the active quest");

    ImGui::Checkbox("Hide GW compass drawings", &hide_compass_drawings);
    ImGui::ShowHelp("Drawings made by other players will be visible on the minimap, but not the compass");
    if (ImGui::Checkbox("Hide GW compass when minimap is visible", &hide_compass_when_minimap_draws)) {
        GW::GameThread::Enqueue(OverrideCompassVisibility);
    }
    if (ImGui::Checkbox("Hide GW compass flagging controls", &hide_flagging_controls)) {
        hide_flagging_controls_patch.TogglePatch(hide_flagging_controls);
    }
    ImGui::ShowHelp("Takes effect on map change. Doesn't work in PvP as Toolbox is disabled there.");

    is_movable = is_resizable = !snap_to_compass;
    if (is_resizable) {
        ImVec2 winsize(100.0f, 100.0f);
        if (const auto window = ImGui::FindWindowByName(Name())) {
            winsize = window->Size;
        }
        if (ImGui::DragFloat("Size", &winsize.x, 1.0f, 0.0f, 0.0f, "%.0f")) {
            winsize.y = winsize.x;
            ImGui::SetWindowSize(Name(), winsize);
        }
    }

    ImGui::Text("General");
    static float a = scale;
    if (ImGui::DragFloat("Scale", &a, 0.01f, 0.1f, 10.f)) {
        scale = a;
    }
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
        EffectRenderer::DrawSettings();
        ImGui::TreePop();
    }
    if (ImGui::TreeNodeEx("Symbols", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
        symbols_renderer.DrawSettings();
        ImGui::Separator();
        ImGui::Text("Cardinal (N/S/E/W) directions");
        Colors::DrawSettingHueWheel("Color##cardinal", &cardinal_color);
        ImGui::ShowHelp("Colour of the N/S/E/W labels shown at the compass edge.");
        ImGui::SliderFloat("Offset##cardinal", &cardinal_offset, -1000.f, 1000.f, "%.0f gwinches");
        ImGui::ShowHelp("Game-unit offset from the compass edge.\nPositive = outward, negative = inward.");
        ImGui::SliderFloat("Font size##cardinal", &cardinal_font_size, 16.f, 56.f, "%.0f px");
        ImGui::TreePop();
    }
    if (ImGui::TreeNodeEx("Terrain", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
        ImGui::SmallConfirmButton("Restore Defaults", "Are you sure?", [&](bool result, void*) {
            if (result) {
                color_map = 0xFF999999;
                color_mapshadow = 0xFF120808;
                color_mapbackground = 0x00000000;
            }
        });
        Colors::DrawSettingHueWheel("Map", &color_map);
        Colors::DrawSettingHueWheel("Shadow", &color_mapshadow);
        Colors::DrawSettingHueWheel("Background", &color_mapbackground);
        ImGui::TreePop();
    }
    custom_renderer.DrawSettings();
    if (ImGui::TreeNodeEx("Hero flagging", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
        ImGui::Checkbox("Show hero flag controls", &hero_flag_controls_show);
        ImGui::Checkbox("Attach to minimap", &hero_flag_window_attach);
        ImGui::ShowHelp("If disabled, you can move/resize the window with 'Unlock Move All'.");
        Colors::DrawSettingHueWheel("Background", &hero_flag_controls_background);
        ImGui::TreePop();
    }
    if (ImGui::TreeNodeEx("In-game rendering", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
        GameWorldRenderer::DrawSettings();
        ImGui::TreePop();
    }
    ImGui::StartSpacedElements(300.f);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Show boss by profession color on minimap", &agent_renderer.boss_colors);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Show hidden NPCs", &agent_renderer.show_hidden_npcs);
    ImGui::ShowHelp("Show NPCs that aren't usually visible on the minimap\ne.g. minipets, invisible NPCs");
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Show symbol for quest NPCs", &agent_renderer.show_quest_npcs_on_minimap);
    ImGui::ShowHelp("Show a star for NPCs that have quest progress available");

    ImGui::SliderFloat("Agent Border thickness", &agent_renderer.agent_border_thickness, 0.f, 100.f, "%.0f");
    ImGui::SliderFloat("Target Border thickness", &agent_renderer.target_border_thickness, 0.f, 100.f, "%.0f");

    ImGui::Text("Allow mouse click-through in:");
    ImGui::Indent();
    ImGui::StartSpacedElements(200.f);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Explorable areas", &mouse_clickthrough_in_explorable);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Outposts", &mouse_clickthrough_in_outpost);

    ImGui::Unindent();
    ImGui::Text("Hold + Click modifiers");
    ImGui::SameLine();
    ImGui::TextDisabled(" - Define behaviour of holding keyboard keys and clicking the minimap.");
    ImGui::Indent();
    ImGui::PushItemWidth(140.f);
    ImGui::TextUnformatted("Draw: ");
    ImGui::ShowHelp("Ping and draw on the compass.");
    ImGui::SameLine(140.f);
    ImGui::Combo("##Draw_key", reinterpret_cast<int*>(&MinimapModifierBehaviour_Keymap[MinimapModifierBehaviour::Draw]), available_modifiers_combo, _countof(available_modifiers_combo));
    ImGui::TextUnformatted("Target: ");
    ImGui::ShowHelp("Click to target agents.");
    ImGui::SameLine(140.f);
    ImGui::Combo("##Target", reinterpret_cast<int*>(&MinimapModifierBehaviour_Keymap[MinimapModifierBehaviour::Target]), available_modifiers_combo, _countof(available_modifiers_combo));
    ImGui::TextUnformatted("Drag: ");
    ImGui::ShowHelp("Drag the minimap outside of compass range.");
    ImGui::SameLine(140.f);
    ImGui::Combo("##Drag", reinterpret_cast<int*>(&MinimapModifierBehaviour_Keymap[MinimapModifierBehaviour::Drag]), available_modifiers_combo, _countof(available_modifiers_combo));
    ImGui::TextUnformatted("MoveTo: ");
    ImGui::ShowHelp("Start walking character to selected location.");
    ImGui::SameLine(140.f);
    ImGui::Combo("##MoveTo", reinterpret_cast<int*>(&MinimapModifierBehaviour_Keymap[MinimapModifierBehaviour::MoveTo]), available_modifiers_combo, _countof(available_modifiers_combo));
    ImGui::PopItemWidth();
    ImGui::Unindent();

    ImGui::StartSpacedElements(256.f);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Reduce agent ping spam", &pingslines_renderer.reduce_ping_spam);
    ImGui::ShowHelp("Additional pings on the same agents will increase the duration of the existing ping, rather than create a new one.");
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Map Rotation", &rotate_minimap);
    ImGui::ShowHelp("Map rotation on (e.g. Compass), or off (e.g. Mission Map).");
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Flip when reversed", &flip_on_reverse);
    ImGui::ShowHelp("Whether the minimap rotation should flip 180 degrees when you reverse your camera.");
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Map rotation smoothing", &smooth_rotation);
    ImGui::ShowHelp("Minimap rotation speed matches compass rotation speed.");
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Circular", &circular_map);
    ImGui::ShowHelp("Whether the map should be circular like the compass (default) or a square.");
}

void Minimap::LoadSettings(ToolboxIni* ini)
{
    ToolboxWidget::LoadSettings(ini);
    Resources::EnsureFileExists(Resources::GetPath(L"Markers.ini"), "https://raw.githubusercontent.com/gwdevhub/GWToolboxpp/master/resources/Markers.ini", [](const bool success, const std::wstring& error) {
        if (success) {
            Instance().custom_renderer.LoadMarkers();
        }
        else {
            Log::ErrorW(L"Failed to download Markers.ini\n%s", error.c_str());
        }
    });
    scale = static_cast<float>(ini->GetDoubleValue(Name(), VAR_NAME(scale), 1.0));
    LOAD_BOOL(hero_flag_controls_show);
    LOAD_BOOL(hero_flag_window_attach);
    LOAD_COLOR(hero_flag_controls_background);
    LOAD_BOOL(mouse_clickthrough_in_outpost);
    LOAD_BOOL(mouse_clickthrough_in_explorable);
    LOAD_BOOL(rotate_minimap);
    LOAD_BOOL(flip_on_reverse);
    LOAD_BOOL(smooth_rotation);
    LOAD_BOOL(circular_map);
    LOAD_BOOL(snap_to_compass);
    LOAD_BOOL(hide_compass_agents);
    LOAD_BOOL(render_all_quests);
    LOAD_BOOL(hide_compass_quest_marker);
    LOAD_BOOL(hide_compass_drawings);
    LOAD_BOOL(hide_flagging_controls);
    LOAD_BOOL(hide_compass_when_minimap_draws);

    hide_flagging_controls_patch.TogglePatch(hide_flagging_controls);

    MinimapModifierBehaviour_Keymap[MinimapModifierBehaviour::Draw] = ini->GetLongValue(Name(), VAR_NAME(minimap_draw_key), MinimapModifierBehaviour_Keymap[MinimapModifierBehaviour::Draw]);
    MinimapModifierBehaviour_Keymap[MinimapModifierBehaviour::Target] = ini->GetLongValue(Name(), VAR_NAME(minimap_target_key), MinimapModifierBehaviour_Keymap[MinimapModifierBehaviour::Target]);
    MinimapModifierBehaviour_Keymap[MinimapModifierBehaviour::Drag] = ini->GetLongValue(Name(), VAR_NAME(minimap_drag_key), MinimapModifierBehaviour_Keymap[MinimapModifierBehaviour::Drag]);
    MinimapModifierBehaviour_Keymap[MinimapModifierBehaviour::MoveTo] = ini->GetLongValue(Name(), VAR_NAME(minimap_moveto_key), MinimapModifierBehaviour_Keymap[MinimapModifierBehaviour::MoveTo]);

    LOAD_COLOR(color_map);
    LOAD_COLOR(color_mapshadow);
    LOAD_COLOR(color_mapbackground);

    cardinal_color = Colors::Load(ini, Name(), "cardinal_color", 0xFFFFFFFF);
    cardinal_offset = static_cast<float>(ini->GetDoubleValue(Name(), "cardinal_offset", 0.0));
    cardinal_font_size = static_cast<float>(ini->GetDoubleValue(Name(), "cardinal_font_size", 13.0));

    range_renderer.LoadSettings(ini, Name());
    agent_renderer.LoadSettings(ini, Name());
    pingslines_renderer.LoadSettings(ini, Name());
    symbols_renderer.LoadSettings(ini, Name());
    custom_renderer.LoadSettings(ini, Name());
    effect_renderer.LoadSettings(ini, Name());
    GameWorldRenderer::LoadSettings(ini, Name());

    range_renderer.Invalidate();

    pending_refresh_quest_marker = true;
}

void Minimap::SaveSettings(ToolboxIni* ini)
{
    ToolboxWidget::SaveSettings(ini);
    ini->SetDoubleValue(Name(), VAR_NAME(scale), scale);
    SAVE_BOOL(hero_flag_controls_show);
    SAVE_BOOL(hero_flag_window_attach);
    SAVE_COLOR(hero_flag_controls_background);
    SAVE_BOOL(mouse_clickthrough_in_outpost);
    SAVE_BOOL(mouse_clickthrough_in_explorable);

    ini->SetLongValue(Name(), VAR_NAME(minimap_draw_key), MinimapModifierBehaviour_Keymap[MinimapModifierBehaviour::Draw]);
    ini->SetLongValue(Name(), VAR_NAME(minimap_target_key), MinimapModifierBehaviour_Keymap[MinimapModifierBehaviour::Target]);
    ini->SetLongValue(Name(), VAR_NAME(minimap_drag_key), MinimapModifierBehaviour_Keymap[MinimapModifierBehaviour::Drag]);
    ini->SetLongValue(Name(), VAR_NAME(minimap_moveto_key), MinimapModifierBehaviour_Keymap[MinimapModifierBehaviour::MoveTo]);

    SAVE_BOOL(rotate_minimap);
    SAVE_BOOL(flip_on_reverse);
    SAVE_BOOL(smooth_rotation);
    SAVE_BOOL(circular_map);
    SAVE_BOOL(snap_to_compass);
    SAVE_BOOL(hide_compass_agents);
    SAVE_BOOL(hide_compass_quest_marker);
    SAVE_BOOL(hide_compass_drawings);
    SAVE_BOOL(render_all_quests);
    SAVE_BOOL(hide_flagging_controls);
    SAVE_BOOL(hide_compass_when_minimap_draws);

    SAVE_COLOR(color_map);
    SAVE_COLOR(color_mapshadow);
    SAVE_COLOR(color_mapbackground);

    Colors::Save(ini, Name(), "cardinal_color", cardinal_color);
    ini->SetDoubleValue(Name(), "cardinal_offset", static_cast<double>(cardinal_offset));
    ini->SetDoubleValue(Name(), "cardinal_font_size", static_cast<double>(cardinal_font_size));

    range_renderer.SaveSettings(ini, Name());
    agent_renderer.SaveSettings(ini, Name());
    pingslines_renderer.SaveSettings(ini, Name());
    symbols_renderer.SaveSettings(ini, Name());
    custom_renderer.SaveSettings(ini, Name());
    EffectRenderer::SaveSettings(ini, Name());
    GameWorldRenderer::SaveSettings(ini, Name());
}

size_t Minimap::GetPlayerHeroes(const GW::PartyInfo* party, std::vector<GW::AgentID>& _player_heroes, bool* has_flags)
{
    _player_heroes.clear();
    if (!party) {
        return 0;
    }
    const uint32_t player_id = GW::PlayerMgr::GetPlayerNumber();
    if (!player_id) {
        return 0;
    }
    const GW::HeroPartyMemberArray& heroes = party->heroes;

    const bool player_is_leader = GetPlayerIsLeader();
    std::map<uint32_t, const GW::PlayerPartyMember*> party_players_by_id;
    if (player_is_leader) {
        for (const GW::PlayerPartyMember& pplayer : party->players) {
            party_players_by_id.emplace(pplayer.login_number, &pplayer);
        }
    }

    _player_heroes.reserve(heroes.size());
    for (const GW::HeroPartyMember& hero : heroes) {
        if (hero.owner_player_id == player_id) {
            _player_heroes.push_back(hero.agent_id);
        }
        else if (player_is_leader) {
            auto found = party_players_by_id.find(hero.owner_player_id);
            if (found == party_players_by_id.end() || found->second->connected()) {
                continue;
            }
            if (has_flags) {
                *has_flags = true;
            }
        }
    }
    if (player_is_leader && has_flags && party->henchmen.size() || _player_heroes.size() && has_flags) {
        *has_flags = true;
    }
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

void Minimap::Draw(IDirect3DDevice9* device)
{
    if (!IsActive()) {
        return;
    }

    const GW::Agent* me = GW::Agents::GetObservingAgent();
    if (me == nullptr) {
        return;
    }

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
                if (found) {
                    break;
                }
            }
            if (!found) {
                shadowstep_location = GW::Vec2f();
            }
        }
    }

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

    auto win_flags = ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus;
    if (snap_to_compass) {
        win_flags |= ImGuiWindowFlags_NoInputs;
    }
    if (compass_position_dirty) {
        RepositionMinimapToCompass();
        OverrideCompassVisibility();
        compass_position_dirty = false;
    }
    if (ImGui::Begin(Name(), nullptr, GetWinFlags(win_flags, true))) {
        // window pos are already rounded by imgui, so casting is no big deal
        if (!snap_to_compass) {
            default_minimap_context.move_to(ImGui::GetWindowPos());
            default_minimap_context.resize(ImGui::GetWindowSize());
        }
    }
    ImGui::End();
    ImGui::PopStyleColor(2);

    if (pending_refresh_quest_marker && RefreshQuestMarker()) pending_refresh_quest_marker = false;

    const auto sz = default_minimap_context.size();
    default_minimap_context.anchor_point = {default_minimap_context.top_left.x + sz.x * 0.5f, default_minimap_context.top_left.y + sz.y * 0.5f};
    // Scale the minimap relative to the window width
    default_minimap_context.base_scale = sz.x;

    default_minimap_context.rotation = GetMapRotation();

    Render(device, default_minimap_context);

    if (hero_flag_controls_show && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable) {
        const GW::PartyInfo* playerparty = GetPlayerParty();
        bool has_flags = false;
        GetPlayerHeroes(playerparty, player_heroes, &has_flags);

        if (has_flags) {
            if (hero_flag_window_attach) {
                const auto viewport = ImGui::GetMainViewport();
                ImGui::SetNextWindowPos(ImVec2(default_minimap_context.top_left.x + viewport->Pos.x, default_minimap_context.bottom_right.y + viewport->Pos.y));
                ImGui::SetNextWindowSize(ImVec2(default_minimap_context.width(), 40.0f));
            }
            ImGui::PushStyleColor(ImGuiCol_WindowBg, ImColor(hero_flag_controls_background).Value);
            if (ImGui::Begin("Hero Controls", nullptr, GetWinFlags(hero_flag_window_attach ? ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove : 0, false))) {
                static const char* flag_txt[] = {"All", "1", "2", "3", "4", "5", "6", "7", "8"};
                const unsigned int num_heroflags = player_heroes.size() + 1;
                const float w_but = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * static_cast<float>(num_heroflags)) / static_cast<float>(num_heroflags + 1);

                for (unsigned int i = 0; i < num_heroflags; ++i) {
                    if (i > 0) {
                        ImGui::SameLine();
                    }
                    const bool is_flagging = GetFlaggingState() == i;
                    if (is_flagging) {
                        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonHovered]);
                    }

                    if (ImGui::Button(flag_txt[i], ImVec2(w_but, 0))) {
                        if (is_flagging) {
                            SetFlaggingState(FlagState_None);
                        }
                        else {
                            SetFlaggingState(static_cast<FlaggingState>(i));
                        }
                        // flagging[i] ^= 1;
                    }
                    if (is_flagging) {
                        ImGui::PopStyleColor();
                    }

                    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                        if (i == 0) {
                            GW::PartyMgr::UnflagAll();
                        }
                        else {
                            GW::PartyMgr::FlagHero(i, GW::GamePos(HUGE_VALF, HUGE_VALF, 0));
                        }
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("Clear", ImVec2(-1, 0))) {
                    GW::PartyMgr::UnflagAll();
                }
            }
            ImGui::End();
            ImGui::PopStyleColor();
        }
    }
}

void Minimap::RegisterRenderer(MinimapRenderer* renderer)
{
    auto& inst = Instance();
    ASSERT(std::ranges::find(inst.registered_renderers, renderer) == inst.registered_renderers.end());
    inst.registered_renderers.push_back(renderer);
}

void Minimap::UnregisterRenderer(MinimapRenderer* renderer)
{
    auto& inst = Instance();
    auto it = std::ranges::find(inst.registered_renderers, renderer);
    if (it != inst.registered_renderers.end()) {
        inst.registered_renderers.erase(it);
    }
}

bool Minimap::ShouldMarkersDrawOnMap()
{
    const auto map_has_outpost_and_explorable = [](const GW::Constants::MapID map_id) {
        switch (map_id) {
            case GW::Constants::MapID::Domain_of_Anguish:
                return true;
            default:
                return false;
        }
    };
    if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading || (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost && map_has_outpost_and_explorable(GW::Map::GetMapID()))) {
        return false;
    }
    return true;
}

bool Minimap::ShouldDrawAllQuests()
{
    // NB: Drawing all quest markers is unstable; there are a bunch of times when the quest marker is stale and we don't know about it. Disable unless debug.
    return render_all_quests;
}

void Minimap::Render(IDirect3DDevice9* device, const MinimapRenderContext& context)
{
    const GW::Agent* me = GW::Agents::GetObservingAgent();
    if (me == nullptr) {
        return;
    }

    // Backup the DX9 state
    IDirect3DStateBlock9* d3d9_state_block = nullptr;
    if (device->CreateStateBlock(D3DSBT_ALL, &d3d9_state_block) < 0) {
        return;
    }

    // Backup the DX9 transform
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
        const D3DVertex vertices[6] = {{x, y, 0.0f, color}, {x + w, y, 0.0f, color}, {x, y + h, 0.0f, color}, {x + w, y + h, 0.0f, color}};
        device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vertices, sizeof(D3DVertex));
    };

    const auto FillCircle = [&device](const float x, const float y, const float radius, const Color clr, const unsigned resolution = 192u) {
        const auto res = std::min(resolution, 192u);
        D3DVertex vertices[193];
        for (auto i = 0u; i <= res; i++) {
            const auto angle = static_cast<float>(i) / static_cast<float>(res) * DirectX::XM_2PI;
            vertices[i] = {x + radius * cos(angle), y + radius * sin(angle), 0.0f, clr};
        }
        device->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, res, vertices, sizeof(D3DVertex));
    };

    // Use context instead of RenderSetupProjection()
    RenderSetupProjection(device, context);

    // Use context background color (or from pmap_renderer if 0)
    auto& instance = Instance();
    // Use context clipping rect instead of global
    const auto rect = context.rect();
    device->SetScissorRect(&rect);
    device->SetRenderState(D3DRS_SCISSORTESTENABLE, true);

    // Use context.circular_map instead of global
    if (context.circular_map) {
        device->SetRenderState(D3DRS_STENCILENABLE, true);
        device->SetRenderState(D3DRS_STENCILMASK, 0xffffffff);
        device->SetRenderState(D3DRS_STENCILWRITEMASK, 0xffffffff);

        device->Clear(0, nullptr, D3DCLEAR_STENCIL | D3DCLEAR_ZBUFFER, 0x00000000, 1.0f, 0);

        device->SetRenderState(D3DRS_STENCILREF, 1);
        device->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_ALWAYS);
        device->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_REPLACE);
        FillCircle(0, 0, 5000.f, context.background_color);
        device->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_EQUAL);
        device->SetRenderState(D3DRS_STENCILFAIL, D3DSTENCILOP_ZERO);
        device->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_REPLACE);
    }
    else {
        FillRect(context.background_color, -5000.0f, -5000.0f, 10000.f, 10000.f);
    }



    auto translate_char = DirectX::XMMatrixTranslation(-me->pos.x, -me->pos.y, 0);

    // Use context.rotation instead of GetMapRotation()
    const auto rotate_char = DirectX::XMMatrixRotationZ(-context.rotation + DirectX::XM_PIDIV2);

    // Use context.zoom_scale instead of global scale
    const auto scaleM = DirectX::XMMatrixScaling(context.zoom_scale, context.zoom_scale, 1.0f);

    // Use context.translation instead of global translation
    const auto translationM = DirectX::XMMatrixTranslation(context.translation.x, context.translation.y, 0);

    const auto view = translate_char * rotate_char * scaleM * translationM;
    device->SetTransform(D3DTS_VIEW, reinterpret_cast<const D3DMATRIX*>(&view));

    instance.pmap_renderer.Render(device, context);
    instance.custom_renderer.Render(device);



    // Move the rings to the char position
    if (context.draw_ranges) {
        translate_char = DirectX::XMMatrixTranslation(me->pos.x, me->pos.y, 0);
        device->SetTransform(D3DTS_WORLD, reinterpret_cast<const D3DMATRIX*>(&translate_char));
        const float gwinches_per_pixel = context.base_scale / 5000.0f / 2.f * context.zoom_scale;
        instance.range_renderer.Render(device, gwinches_per_pixel);
        device->SetTransform(D3DTS_WORLD, &reset_world);
    }


    // Use context.draw_center_marker or check context.translation
    if (context.draw_center_marker || context.translation.x) {
        const auto view2 = scaleM;
        device->SetTransform(D3DTS_VIEW, reinterpret_cast<const D3DMATRIX*>(&view2));
        instance.range_renderer.SetDrawCenter(true);
        instance.range_renderer.Render(device);
        instance.range_renderer.SetDrawCenter(false);
        device->SetTransform(D3DTS_VIEW, reinterpret_cast<const D3DMATRIX*>(&view));
    }

    instance.symbols_renderer.Render(device, context.zoom_scale);
    device->SetTransform(D3DTS_WORLD, &reset_world);
    instance.agent_renderer.Render(device);
    instance.effect_renderer.Render(device);
    instance.pingslines_renderer.Render(device);
    for (auto renderer : instance.registered_renderers)
        renderer->RenderMinimap(device, context);
    
    DrawNSEW(context);

    if (context.circular_map) {
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

void Minimap::SelectTarget(const GW::Vec2f pos)
{
    const auto* agents = GW::Agents::GetAgentArray();
    if (agents == nullptr) {
        return;
    }
    auto distance = 600.0f * 600.0f;
    const GW::Agent* closest = nullptr;

    for (const auto* agent : *agents) {
        const auto agent_is_locked_chest = agent && agent->GetIsGadgetType() && agent->GetAsAgentGadget()->gadget_id == GW::Constants::ModelID::LockedChest;
        if (!agent_is_locked_chest && !GW::Agents::GetAgentMatchesFlags(agent,GW::TargetFilter::AnyLiving)) continue;
        const float new_distance = GetSquareDistance(pos, agent->pos);
        if (distance > new_distance) {
            distance = new_distance;
            closest = agent;
        }
    }

    if (closest != nullptr) {
        GW::Agents::ChangeTarget(closest);
    }
}

bool Minimap::WndProc(const UINT Message, const WPARAM wParam, const LPARAM lParam)
{
    if (!IsActive()) {
        return false;
    }
    if (is_observing) {
        return false;
    }
    if (ShouldClickThrough() && !IsAnyModifierKeyDown()) {
        // Even through clickthrough might be disabled, we still allow flags to be placed on the minimap.
        return Message == WM_LBUTTONDOWN && FlagHeros(lParam);
    }
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

bool Minimap::FlagHero(uint32_t idx)
{
    if (idx > FlagState_None) {
        return false;
    }
    return SetFlaggingState(static_cast<FlaggingState>(idx));
}

bool Minimap::FlagHeros(const LPARAM lParam)
{
    if (!ToolboxUtils::IsExplorable()) return false;
    const int x = GET_X_LPARAM(lParam);
    const int y = GET_Y_LPARAM(lParam);
    if (!IsInside(x, y)) {
        return false;
    }
    bool has_flagall = false;
    GetPlayerHeroes(GetPlayerParty(), player_heroes, &has_flagall);
    const GW::Vec2f worldpos = InterfaceToWorldPoint(Vec2i(x, y));

    switch (const auto flag_state = GetFlaggingState()) {
        case FlagState_None:
            return false;
        case FlagState_All:
            if (!has_flagall) {
                return false;
            }
            SetFlaggingState(FlagState_None);
            return GW::PartyMgr::FlagAll(GW::GamePos(worldpos));
        default:
            if (flag_state > player_heroes.size()) {
                return false;
            }
            SetFlaggingState(FlagState_None);
            return GW::PartyMgr::FlagHero(flag_state, GW::GamePos(worldpos));
    }
}

bool Minimap::OnMouseDown(const UINT, const WPARAM, const LPARAM lParam)
{
    if (!IsActive()) {
        return false;
    }
    if (FlagHeros(lParam)) {
        return true;
    }
    const int x = GET_X_LPARAM(lParam);
    const int y = GET_Y_LPARAM(lParam);
    if (!IsInside(x, y)) {
        return false;
    }

    mousedown = true;
    const GW::Vec2f worldpos = InterfaceToWorldPoint(Vec2i(x, y));

    if (IsKeyDown(MinimapModifierBehaviour::Target)) {
        SelectTarget(worldpos);
        return true;
    }

    if (IsKeyDown(MinimapModifierBehaviour::MoveTo)) {
        GW::Agents::Move(worldpos);
        pingslines_renderer.AddMouseClickPing(worldpos);
        return true;
    }

    drag_start.x = x;
    drag_start.y = y;

    if (IsKeyDown(MinimapModifierBehaviour::Drag)) {
        return true;
    }

    if (!lock_move && !snap_to_compass) {
        return true;
    }

    pingslines_renderer.OnMouseDown(worldpos.x, worldpos.y);

    return true;
}

bool Minimap::OnMouseDblClick(const UINT, const WPARAM, const LPARAM lParam) const
{
    if (!IsActive()) {
        return false;
    }

    const int x = GET_X_LPARAM(lParam);
    const int y = GET_Y_LPARAM(lParam);
    if (!IsInside(x, y)) {
        return false;
    }

    if (IsKeyDown(MinimapModifierBehaviour::Target)) {
        SelectTarget(InterfaceToWorldPoint(Vec2i(x, y)));
        return true;
    }

    return true;
}

bool Minimap::OnMouseUp(const UINT, const WPARAM, const LPARAM)
{
    if (!IsActive()) {
        return false;
    }

    if (!mousedown) {
        return false;
    }

    mousedown = false;

    return pingslines_renderer.OnMouseUp();
}

bool Minimap::OnMouseMove(const UINT, const WPARAM, const LPARAM lParam)
{
    if (!IsActive()) {
        return false;
    }

    if (!GetKeyState(VK_LBUTTON)) {
        mousedown = false;
    }

    if (!mousedown) {
        return false;
    }

    const int x = GET_X_LPARAM(lParam);
    const int y = GET_Y_LPARAM(lParam);
    if (!IsInside(x, y)) {
        mousedown = false;
        return false;
    }

    if (IsKeyDown(MinimapModifierBehaviour::Target)) {
        SelectTarget(InterfaceToWorldPoint(Vec2i(x, y)));
        return true;
    }

    if (IsKeyDown(MinimapModifierBehaviour::Drag)) {
        const auto diff = Vec2i(x - drag_start.x, y - drag_start.y);
        translation += InterfaceToWorldVector(diff);
        drag_start = Vec2i(x, y);
        last_moved = TIMER_INIT();
        return true;
    }

    const GW::Vec2f v = InterfaceToWorldPoint(Vec2i(x, y));
    return pingslines_renderer.OnMouseMove(v.x, v.y);
}

bool Minimap::OnMouseWheel(const UINT, const WPARAM wParam, const LPARAM)
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

    if (IsKeyDown(MinimapModifierBehaviour::Drag)) {
        const float delta = zDelta > 0 ? 1.024f : 0.9765625f;
        scale *= delta;
        return true;
    }

    return false;
}

bool Minimap::IsInside(const int x, const int y) const
{
    // if outside square, return false


    // if centered, use radar range
    if (translation.x == 0 && translation.y == 0) {
        const GW::Vec2f gamepos = InterfaceToWorldPoint(Vec2i(x, y));
        const GW::Agent* me = GW::Agents::GetObservingAgent();
        if (!me) {
            return false;
        }
        const float sqrdst = GetSquareDistance(me->pos, gamepos);
        return sqrdst < GW::Constants::SqrRange::Compass;
    }
    return true;
}

bool Minimap::IsActive()
{
    return Instance().visible && !terminating && !loading && GW::Map::GetIsMapLoaded() && !GW::UI::GetIsWorldMapShowing() && GW::Map::GetInstanceType() != GW::Constants::InstanceType::Loading && GW::Agents::GetObservingId() != 0;
}


void Minimap::RenderSetupProjection(IDirect3DDevice9* device, const MinimapRenderContext& context)
{
    D3DVIEWPORT9 viewport;
    device->GetViewport(&viewport);

    constexpr float w = 5000.0f * 2;
    const DirectX::XMMATRIX ortho_matrix(2 / w, 0, 0, 0, 0, 2 / w, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);

    const auto width_f = static_cast<float>(viewport.Width);
    const auto height_f = static_cast<float>(viewport.Height);
    const float xscale = context.base_scale / width_f;
    const float yscale = context.base_scale / height_f;

    // anchor_point is where world (0,0) maps to on screen.
    // After the view transform, the player IS at world (0,0),
    // so this controls where the player appears on screen.
    const float xtrans = (context.anchor_point.x * 2.0f) / width_f - 1.0f;
    const float ytrans = -(context.anchor_point.y * 2.0f) / height_f + 1.0f;

    const DirectX::XMMATRIX viewport_matrix(xscale, 0, 0, 0, 0, yscale, 0, 0, 0, 0, 1, 0, xtrans, ytrans, 0, 1);

    const auto proj = ortho_matrix * viewport_matrix;

    device->SetTransform(D3DTS_PROJECTION, reinterpret_cast<const D3DMATRIX*>(&proj));
}
