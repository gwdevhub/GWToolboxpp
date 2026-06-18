#include "stdafx.h"

#include <GWCA/Context/GameplayContext.h>
#include <GWCA/Context/WorldContext.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Map.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Utilities/Hooker.h>

#include <ImGuiAddons.h>
#include <Modules/GwDatTextureModule.h>
#include <Modules/QuestModule.h>
#include <Modules/Resources.h>
#include <Widgets/Minimap/Minimap.h>
#include <Widgets/MissionMapWidget.h>
#include <Widgets/WorldMapWidget.h>
#include <Windows/SettingsWindow.h>
#include <Utils/ToolboxUtils.h>
#include <D3DContainers.h>

namespace {
    MissionMapWidget::Settings settings;

    IDirect3DPixelShader9* icon_tint_shader = nullptr;

    std::vector<MissionMapWidget::DrawCallback> draw_callbacks;
    std::vector<MissionMapWidget::ContextMenuCallback> context_menu_callbacks;

    // Pixel-to-game-unit scale — converts pixel thickness to game units
    float cached_px_to_game = 1.f;

    bool render_ready = false;
    D3DMATRIX cached_game_to_screen = {};

    D3DTriangleBuffer minimap_lines;

    const D3DMATRIX IDENTITY_MATRIX = {{1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f}};

    bool GamePosToMissionMapScreenPos(const GW::GamePos& game_map_position, GW::Vec2f& screen_coords);
    void RenderMinimapLayers(IDirect3DDevice9* dx_device, const D3DMATRIX& game_to_screen, const D3DMATRIX& projection);

    struct GameToScreenBasis {
        float ox, oy;
        float ax, ay;
        float bx, by;
        bool valid = false;
        clock_t last_rebuild = TIMER_INIT();

        float cached_basis_zoom = 0.f;
        void Rebuild();

        void Project(float gx, float gy, float& sx, float& sy) const
        {
            sx = ox + gx * ax + gy * bx;
            sy = oy + gx * ay + gy * by;
        }
    } g2s;

    GW::Vec2f mission_map_top_left;
    GW::Vec2f mission_map_bottom_right;
    GW::Vec2f mission_map_scale;
    float mission_map_zoom;

    GW::Vec2f mission_map_center_pos;
    GW::Vec2f mission_map_last_click_location;
    GW::Vec2f current_pan_offset;

    GW::Vec2f mission_map_screen_pos;
    GW::Vec2f world_map_click_pos;

    bool right_clicking = false;

    // Left-click-to-target: a press that doesn't drag (the game uses drag to pan) selects a target
    bool left_clicking = false;
    bool left_click_dragged = false;
    GW::Vec2f left_click_pos;
    constexpr float left_click_drag_threshold_sq = 25.f; // 5px

    GW::UI::Frame* mission_map_frame = nullptr;

    bool IsScreenPosOnMissionMap(const GW::Vec2f& screen_pos)
    {
        if (!(mission_map_frame && mission_map_frame->IsVisible())) return false;
        return (screen_pos.x >= mission_map_top_left.x && screen_pos.x <= mission_map_bottom_right.x && screen_pos.y >= mission_map_top_left.y && screen_pos.y <= mission_map_bottom_right.y);
    }

    GW::Vec2f GetMissionMapScreenCenterPos()
    {
        return mission_map_top_left + (mission_map_bottom_right - mission_map_top_left) / 2;
    }

    bool WorldMapCoordsToMissionMapScreenPos(const GW::Vec2f& world_map_position, GW::Vec2f& screen_coords)
    {
        const auto offset = (world_map_position - current_pan_offset);
        const auto scaled_offset = GW::Vec2f(offset.x * mission_map_scale.x, offset.y * mission_map_scale.y);
        screen_coords = (scaled_offset * mission_map_zoom + mission_map_screen_pos);
        return true;
    }

    GW::Vec2f ScreenPosToMissionMapCoords(const GW::Vec2f screen_position)
    {
        GW::Vec2f unscaled_offset = (screen_position - mission_map_screen_pos) / mission_map_zoom;
        GW::Vec2f offset(unscaled_offset.x / mission_map_scale.x, unscaled_offset.y / mission_map_scale.y);
        return offset + current_pan_offset;
    }

    bool GamePosToMissionMapScreenPos(const GW::GamePos& game_map_position, GW::Vec2f& screen_coords)
    {
        GW::Vec2f world_map_pos;
        return WorldMapWidget::GamePosToWorldMap(game_map_position, world_map_pos) && WorldMapCoordsToMissionMapScreenPos(world_map_pos, screen_coords);
    }

    // Invert the game->screen basis so a screen click maps back to game coords.
    bool ScreenPosToGamePos(const GW::Vec2f& screen, GW::Vec2f& game)
    {
        const float det = g2s.ax * g2s.by - g2s.bx * g2s.ay;
        if (!g2s.valid || fabsf(det) < 1e-9f) return false;
        const float dx = screen.x - g2s.ox;
        const float dy = screen.y - g2s.oy;
        game = {(g2s.by * dx - g2s.bx * dy) / det, (-g2s.ay * dx + g2s.ax * dy) / det};
        return true;
    }

    void GameToScreenBasis::Rebuild()
    {
        valid = false;

        const auto* mm_ctx = GW::Map::GetMissionMapContext();
        if (!mm_ctx || !mm_ctx->h003c) return;
        const GW::Vec2f mm_pos = mm_ctx->h003c->player_mission_map_pos;

        // Use the controlled character's position when not spectating (works on
        // underground maps where WorldMapToGamePos returns wrong coordinates).
        // When spectating, mm_pos tracks the observed character so we must convert
        // it back to game coords to stay consistent with the origin calculation.
        float px, py;
        const auto* player = GW::Agents::GetControlledCharacter();
        const bool spectating = player && GW::Agents::GetObservingId() != player->agent_id;
        if (spectating) {
            GW::GamePos anchor_pos;
            if (!WorldMapWidget::WorldMapToGamePos(mm_pos, anchor_pos)) return;
            px = anchor_pos.x;
            py = anchor_pos.y;
        } else {
            if (!player) return;
            px = player->pos.x;
            py = player->pos.y;
        }

        if (mission_map_zoom != cached_basis_zoom || !valid) {
            cached_basis_zoom = mission_map_zoom;

            constexpr float STEP = 1000.f;
            GW::Vec2f s00, s10, s01;
            if (!GamePosToMissionMapScreenPos({px, py, 0}, s00) ||
                !GamePosToMissionMapScreenPos({px + STEP, py, 0}, s10) ||
                !GamePosToMissionMapScreenPos({px, py + STEP, 0}, s01)) return;

            ax = (s10.x - s00.x) / STEP;
            ay = (s10.y - s00.y) / STEP;
            bx = (s01.x - s00.x) / STEP;
            by = (s01.y - s00.y) / STEP;
        }

        const GW::Vec2f mm_offset = mm_pos - current_pan_offset;
        const GW::Vec2f mm_scaled = {mm_offset.x * mission_map_scale.x, mm_offset.y * mission_map_scale.y};
        const GW::Vec2f player_screen = {mm_scaled.x * mission_map_zoom + mission_map_screen_pos.x,
                                         mm_scaled.y * mission_map_zoom + mission_map_screen_pos.y};

        ox = player_screen.x - px * ax - py * bx;
        oy = player_screen.y - px * ay - py * by;
        valid = true;
    }

    std::vector<GW::UI::UIMessage> messages_hit;
    GW::UI::UIInteractionCallback OnMissionMap_UICallback_Func = nullptr, OnMissionMap_UICallback_Ret = nullptr;

    void OnMissionMap_UICallback(GW::UI::InteractionMessage* message, void* wparam, void* lparam)
    {
        GW::Hook::EnterHook();
        if (message->message_id == GW::UI::UIMessage::kDestroyFrame) mission_map_frame = nullptr;

        OnMissionMap_UICallback_Ret(message, wparam, lparam);
        if (message->message_id == GW::UI::UIMessage::kInitFrame) mission_map_frame = GW::UI::GetFrameById(message->frame_id);
        GW::Hook::LeaveHook();
    }

    bool HookMissionMapFrame()
    {
        if (OnMissionMap_UICallback_Func) return true;

        const auto mission_map_context = GW::Map::GetMissionMapContext();
        mission_map_frame = mission_map_context ? GW::UI::GetFrameById(mission_map_context->frame_id) : nullptr;
        if (!(mission_map_frame && mission_map_frame->frame_callbacks[0].callback)) return false;
        OnMissionMap_UICallback_Func = mission_map_frame->frame_callbacks[0].callback;
        GW::Hook::CreateHook((void**)&OnMissionMap_UICallback_Func, OnMissionMap_UICallback, (void**)&OnMissionMap_UICallback_Ret);
        GW::Hook::EnableHooks(OnMissionMap_UICallback_Func);
        return true;
    }
    bool mission_map_ready = false;
    bool InitializeMissionMapParameters()
    {
        const auto gameplay_context = GW::GetGameplayContext();
        const auto mission_map_context = GW::Map::GetMissionMapContext();
        if (!(gameplay_context && mission_map_context && mission_map_frame && mission_map_frame->IsVisible())) return false;

        const auto root = GW::UI::GetRootFrame();
        mission_map_top_left = mission_map_frame->position.GetContentTopLeft(root);
        if (!std::isfinite(mission_map_top_left.x)) 
            return false;
        mission_map_bottom_right = mission_map_frame->position.GetContentBottomRight(root);
        mission_map_scale = mission_map_frame->position.GetViewportScale(root);
        mission_map_zoom = gameplay_context->mission_map_zoom;
        mission_map_center_pos = mission_map_context->player_mission_map_pos;
        mission_map_last_click_location = mission_map_context->last_mouse_location;
        current_pan_offset = mission_map_context->h003c->mission_map_pan_offset;
        mission_map_screen_pos = GetMissionMapScreenCenterPos();
        return true;
    }

    bool MissionMapContextMenu(void*)
    {
        if (!(mission_map_frame && mission_map_frame->IsVisible())) return false;
        const auto c = ImGui::GetCurrentContext();
        auto viewport_offset = c->CurrentViewport->Pos;
        viewport_offset.x *= -1;
        viewport_offset.y *= -1;

        ImGui::Text("%.2f, %.2f", world_map_click_pos.x, world_map_click_pos.y);
#ifdef _DEBUG
        GW::GamePos game_pos;
        if (WorldMapWidget::WorldMapToGamePos(world_map_click_pos, game_pos)) {
            ImGui::Text("%.2f, %.2f", game_pos.x, game_pos.y);
        }
#endif
        if (ImGui::Button("Place Marker")) {
            GW::GameThread::Enqueue([] {
                QuestModule::SetCustomQuestMarker(world_map_click_pos, true);
            });
            return false;
        }
        if (QuestModule::GetCustomQuestMarker()) {
            if (ImGui::Button("Remove Marker")) {
                GW::GameThread::Enqueue([] { QuestModule::ClearCustomQuestMarker(); });
                return false;
            }
        }
        for (const auto& cb : context_menu_callbacks) {
            if (!cb()) return false;
        }

        return true;
    }

    void SubmitVertexBuffers(IDirect3DDevice9* dx_device)
    {
        const D3DStateGuard guard(dx_device);

        dx_device->SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);
        dx_device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
        dx_device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
        dx_device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
        dx_device->SetRenderState(D3DRS_LIGHTING, FALSE);
        dx_device->SetRenderState(D3DRS_ZENABLE, FALSE);

        RECT scissorRect;
        scissorRect.left = static_cast<LONG>(floorf(mission_map_top_left.x));
        scissorRect.top = static_cast<LONG>(floorf(mission_map_top_left.y));
        scissorRect.right = static_cast<LONG>(ceilf(mission_map_bottom_right.x));
        scissorRect.bottom = static_cast<LONG>(ceilf(mission_map_bottom_right.y));
        dx_device->SetScissorRect(&scissorRect);

        D3DVIEWPORT9 vp;
        dx_device->GetViewport(&vp);
        const D3DMATRIX ortho = MakeOrthoProjection(static_cast<float>(vp.Width), static_cast<float>(vp.Height));
        const D3DMATRIX gameToScreen = {{g2s.ax, g2s.ay, 0.f, 0.f, g2s.bx, g2s.by, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, g2s.ox, g2s.oy, 0.f, 1.f}};

        dx_device->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE);
        dx_device->SetTransform(D3DTS_WORLD, &gameToScreen);
        dx_device->SetTransform(D3DTS_VIEW, &IDENTITY_MATRIX);
        dx_device->SetTransform(D3DTS_PROJECTION, &ortho);

        for (const auto& cb : draw_callbacks) {
            cb(dx_device);
        }

        // When the overlay is off (default), draw the lines directly — a single DrawPrimitive with the
        // state already set above. Only the opt-in overlay pays for Minimap::Render (D3DSBT_ALL capture).
        if (settings.draw_minimap) {
            RenderMinimapLayers(dx_device, gameToScreen, ortho);
        }
        else {
            dx_device->SetTransform(D3DTS_WORLD, &gameToScreen);
            minimap_lines.Render(dx_device);
        }
    }

    // Overlay the minimap layers on the mission map; mission-map lines feed ctx.lines (a context layer,
    // separate from the minimap's draw_on_minimap CustomRenderer). Only called when the overlay is on.
    void RenderMinimapLayers(IDirect3DDevice9* dx_device, const D3DMATRIX& game_to_screen, const D3DMATRIX& projection)
    {
        MinimapRenderContext ctx = Minimap::GetRenderContext(); // inherit user colours (symbols)
        ctx.top_left = {mission_map_top_left.x, mission_map_top_left.y};
        ctx.bottom_right = {mission_map_bottom_right.x, mission_map_bottom_right.y};
        ctx.view_override = &game_to_screen;
        ctx.projection_override = &projection;
        ctx.gwinches_per_pixel_override = 1.f / cached_px_to_game;
        ctx.lines = &minimap_lines;

        ctx.translation = {};
        ctx.zoom_scale = 1.f;
        ctx.circular_map = false;
        ctx.draw_center_marker = false;
        ctx.draw_custom = false; // lines come from ctx.lines; minimap markers/polygons are not wanted here

        ctx.draw_background = false;
        ctx.draw_cardinals = false; // mission map is always north-aligned
        ctx.draw_pmap = settings.draw_pmap;
        ctx.draw_symbols = settings.draw_symbols;
        ctx.draw_ranges = settings.draw_ranges;
        ctx.draw_agents = settings.draw_agents;
        ctx.draw_pings = settings.draw_pings;
        ctx.draw_effects = settings.draw_effects;

        // Sub-renderers draw in game coords via the view override, so WORLD must be identity.
        dx_device->SetTransform(D3DTS_WORLD, &IDENTITY_MATRIX);
        Minimap::Render(dx_device, ctx);
    }

    void EnqueueMinimapLines(GW::Constants::MapID map_id)
    {
        const auto& lines = Minimap::Instance().custom_renderer.GetLines();
        minimap_lines.clear();
        minimap_lines.reserve(lines.size());
        if (lines.empty()) return;
        const auto player_pos = GW::PlayerMgr::GetPlayerPosition();
        const float LINE_HALF_THICKNESS = 1.f * cached_px_to_game;
        for (const auto& line : lines) {
            if (!line->visible) continue;
            if (!line->draw_on_mission_map && !(settings.draw_all_minimap_lines && line->draw_on_minimap) && !(settings.draw_all_terrain_lines && line->draw_on_terrain)) continue;
            if (line->map != map_id) continue;
            if (line->from_player_pos && player_pos) {
                minimap_lines.push_back(D3DLine(*player_pos, line->p2, LINE_HALF_THICKNESS, static_cast<DWORD>(line->color)));
            }
            else {
                minimap_lines.push_back(D3DLine(line->p1, line->p2, LINE_HALF_THICKNESS, static_cast<DWORD>(line->color)));
            }
        }
        
    }
} // namespace

void MissionMapWidget::Initialize()
{
    ToolboxWidget::Initialize();
    SettingsRegistry::Register(this, settings);
}

void MissionMapWidget::LoadSettings(SettingsDoc& doc, ToolboxIni* legacy)
{
    ToolboxWidget::LoadSettings(doc, legacy);
    doc.GetStruct(Name(), settings);
}

void MissionMapWidget::SaveSettings(SettingsDoc& doc)
{
    ToolboxWidget::SaveSettings(doc);
    doc.SetStruct(Name(), settings);
}

void MissionMapWidget::Draw(IDirect3DDevice9* dx_device)
{
    if (!visible || !mission_map_ready) return;

    SubmitVertexBuffers(dx_device);

    // POC: mission map icons, desaturated. Res shrine icon (maybe others) are wrong colour by default so we desaturate, but this impacts other icons!
    #if 0
    const auto* world_ctx = GW::GetWorldContext();
    if (!world_ctx) return;
    auto* draw_list = ImGui::GetBackgroundDrawList();
    draw_list->PushClipRect({mission_map_top_left.x, mission_map_top_left.y}, {mission_map_bottom_right.x, mission_map_bottom_right.y});
    static constexpr ImU32 kAffiliationColors[] = {
        IM_COL32_WHITE,                   // 0: gray
        IM_COL32(0x20, 0x20, 0xFF, 0xFF), // 1: blue
        IM_COL32(0xFF, 0x20, 0x20, 0xFF), // 2: red
        IM_COL32(0xFF, 0xFF, 0x20, 0xFF), // 3: yellow
        IM_COL32(0x20, 0xFF, 0xFF, 0xFF), // 4: teal
        IM_COL32(0x80, 0x20, 0xFF, 0xFF), // 5: purple
        IM_COL32(0x20, 0xFF, 0x20, 0xFF), // 6: green
        IM_COL32_WHITE,                   // 7: gray (same as 0)
    };
    for (const auto& icon : world_ctx->mission_map_icons) {
        auto** tex = GwDatTextureModule::LoadGreyscaleTextureFromFileId(icon.model_id);
        if (!tex || !*tex) continue;
        GW::Vec2f pos;
        GamePosToMissionMapScreenPos({icon.X, icon.Y}, pos);
        ImVec2 sz = {42.f, 42.f};
        const ImU32 tint = icon.option < std::size(kAffiliationColors) ? kAffiliationColors[icon.option] : IM_COL32_WHITE;
        draw_list->AddImage(*tex, {pos.x - sz.x / 2, pos.y - sz.y / 2}, {pos.x + sz.x / 2, pos.y + sz.y / 2}, {0, 0}, {1, 1}, tint);
    }
    draw_list->PopClipRect();
    #endif
}

void MissionMapWidget::Update(float)
{
    if (!HookMissionMapFrame()) return;
    mission_map_ready = InitializeMissionMapParameters();
    if (!mission_map_ready) return;

    g2s.Rebuild();
    if (!g2s.valid) { render_ready = false; return; }

    cached_px_to_game = 1.0f / sqrtf(g2s.ax * g2s.ax + g2s.ay * g2s.ay);
    cached_game_to_screen = {{g2s.ax, g2s.ay, 0.f, 0.f, g2s.bx, g2s.by, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, g2s.ox, g2s.oy, 0.f, 1.f}};
    render_ready = true;

    // Frame rate check — only throttle minimap line updates
    static clock_t last_check = TIMER_INIT();
    if (!ToolboxUtils::FrameRateCheck(last_check, 30)) return;

    EnqueueMinimapLines(GW::Map::GetMapID());
}

void MissionMapWidget::DrawSettingsInternal()
{
    ImGui::Checkbox("Draw all terrain lines", &settings.draw_all_terrain_lines);
    ImGui::Checkbox("Draw all minimap lines", &settings.draw_all_minimap_lines);

    ImGui::Separator();
    ImGui::Checkbox("Show minimap on mission map", &settings.draw_minimap);
    ImGui::BeginDisabled(!settings.draw_minimap);
    ImGui::TextDisabled("Minimap layers drawn on the mission map");
    ImGui::Checkbox("Range rings", &settings.draw_ranges);
    ImGui::Checkbox("Agents", &settings.draw_agents);
    ImGui::Checkbox("Pings & drawings", &settings.draw_pings);
    ImGui::Checkbox("Effects", &settings.draw_effects);
    ImGui::Checkbox("Background", &settings.draw_background);
    ImGui::Checkbox("Pathing map (terrain)", &settings.draw_pmap);
    ImGui::Checkbox("Symbols", &settings.draw_symbols);
    ImGui::Checkbox("Click to target", &settings.click_to_target);
    ImGui::ShowHelp("Left-click (without dragging) on the mission map to target the nearest agent");
    ImGui::EndDisabled();

    if (ImGui::Button("Customise minimap (colours, agents, ranges)...")) {
        SettingsWindow::Instance().NavigateToSection(Minimap::Instance().SettingsName());
    }
}

bool MissionMapWidget::WndProc(const UINT Message, WPARAM, LPARAM lParam)
{
    const GW::Vec2f cursor_pos = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};

    switch (Message) {
        case WM_GW_RBUTTONCLICK: {
            if (!IsScreenPosOnMissionMap(cursor_pos)) break;
            if (!mission_map_frame) break;
            if (GW::UI::GetCurrentTooltip()) break;
            world_map_click_pos = ScreenPosToMissionMapCoords(cursor_pos);
            ImGui::SetContextMenu(MissionMapContextMenu);
        } break;

        // A left click that doesn't drag selects a target; drags are left to the game (it pans the map), so never capture.
        case WM_LBUTTONDOWN: {
            left_clicking = settings.draw_minimap && settings.click_to_target && IsScreenPosOnMissionMap(cursor_pos) && !GW::UI::GetCurrentTooltip();
            left_click_dragged = false;
            left_click_pos = cursor_pos;
        } break;
        case WM_MOUSEMOVE: {
            const float dx = cursor_pos.x - left_click_pos.x, dy = cursor_pos.y - left_click_pos.y;
            if (left_clicking && dx * dx + dy * dy > left_click_drag_threshold_sq) left_click_dragged = true;
        } break;
        case WM_LBUTTONUP: {
            if (left_clicking && !left_click_dragged && IsScreenPosOnMissionMap(cursor_pos)) {
                GW::Vec2f game_pos;
                if (ScreenPosToGamePos(left_click_pos, game_pos)) Minimap::SelectTarget(game_pos);
            }
            left_clicking = false;
        } break;
    }
    return false;
}

void MissionMapWidget::Terminate()
{
    ToolboxWidget::Terminate();
    if (OnMissionMap_UICallback_Func) GW::Hook::RemoveHook(OnMissionMap_UICallback_Func);
    OnMissionMap_UICallback_Func = nullptr;

    if (icon_tint_shader) {
        icon_tint_shader->Release();
        icon_tint_shader = nullptr;
    }

    minimap_lines.Terminate();
    render_ready = false;
}

bool MissionMapWidget::IsRenderReady() { return render_ready && GW::Map::GetIsMapLoaded() && mission_map_frame && mission_map_frame->IsVisible(); }
const D3DMATRIX& MissionMapWidget::GetGameToScreenMatrix() { return cached_game_to_screen; }
GW::Vec2f MissionMapWidget::GetTopLeft() { return mission_map_top_left; }
GW::Vec2f MissionMapWidget::GetBottomRight() { return mission_map_bottom_right; }
GW::UI::Frame* MissionMapWidget::GetFrame() { return mission_map_frame; }
float MissionMapWidget::GetPxToGame() { return cached_px_to_game; }
float MissionMapWidget::GetZoom() { return mission_map_zoom; }

void MissionMapWidget::AddDrawCallback(DrawCallback cb) { draw_callbacks.push_back(cb); }
void MissionMapWidget::RemoveDrawCallback(DrawCallback cb) { std::erase(draw_callbacks, cb); }
void MissionMapWidget::AddContextMenuCallback(ContextMenuCallback cb) { context_menu_callbacks.push_back(cb); }
void MissionMapWidget::RemoveContextMenuCallback(ContextMenuCallback cb) { std::erase(context_menu_callbacks, cb); }
