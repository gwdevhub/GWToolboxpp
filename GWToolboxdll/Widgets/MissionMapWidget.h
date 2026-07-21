#pragma once

#include <ToolboxWidget.h>
#include <d3d9types.h>
#include <GWCA/GameContainers/GamePos.h>

namespace GW::UI { struct Frame; }

class MissionMapWidget : public ToolboxWidget {
    MissionMapWidget()
    {
        visible = true;
        can_show_in_main_window = false;
        has_closebutton = false;
        has_titlebar = false;
        is_resizable = false;
        is_movable = false;
    }
    ~MissionMapWidget() override = default;

public:
    static MissionMapWidget& Instance()
    {
        static MissionMapWidget w;
        return w;
    }
    [[nodiscard]] const char* Name() const override { return "Mission Map"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_GLOBE; }

    struct Settings {
        bool draw_all_terrain_lines = false;
        bool draw_all_minimap_lines = true;
        bool click_to_target = true;
        // Minimap layers overlaid on the mission map via Minimap::Render
        bool draw_minimap = false; // master toggle; off by default to keep current behaviour
        bool draw_ranges = true;
        bool draw_agents = true;
        bool draw_pings = false;
        bool draw_effects = false;
        bool draw_background = false;
        bool draw_pmap = false;
        bool draw_symbols = false;
        // Static walkable/non-walkable terrain overlay, independent of the Vanquish overlay
        bool show_walkable_terrain = false;
        Colors::SettingColor terrain_shade_color = IM_COL32(90, 90, 90, 80);
        Colors::SettingColor terrain_border_color = IM_COL32(220, 220, 220, 100);
    };

    void Initialize() override;
    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void SaveSettings(SettingsDoc& doc) override;
    void Draw(IDirect3DDevice9* pDevice) override;
    void Update(float) override;
    void DrawSettingsInternal() override;
    void Terminate() override;
    bool WndProc(UINT Message, WPARAM, LPARAM lParam) override;

    // Expose mission map rendering context for overlay widgets
    static bool IsRenderReady();
    static const D3DMATRIX& GetGameToScreenMatrix();
    static GW::Vec2f GetTopLeft();
    static GW::Vec2f GetBottomRight();
    static GW::UI::Frame* GetFrame();
    static float GetPxToGame();
    static float GetZoom();

    // Draw callback system — registered callbacks are invoked with D3D render state
    // (scissor, blend, transforms) already configured for mission map rendering
    using DrawCallback = void(*)(IDirect3DDevice9*);
    static void AddDrawCallback(DrawCallback cb);
    static void RemoveDrawCallback(DrawCallback cb);

    // Context menu callback system — registered callbacks contribute items to the
    // mission map right-click context menu. Return false to close the menu.
    using ContextMenuCallback = bool(*)();
    static void AddContextMenuCallback(ContextMenuCallback cb);
    static void RemoveContextMenuCallback(ContextMenuCallback cb);
    // World-map coords of the right-click that opened the context menu.
    static GW::Vec2f GetContextMenuWorldMapPos();

    // ImGui overlay callback system — registered callbacks draw into the background
    // draw list each frame the mission map is visible, clipped to the widget rect.
    using OverlayCallback = void(*)(ImDrawList*);
    static void AddOverlayCallback(OverlayCallback cb);
    static void RemoveOverlayCallback(OverlayCallback cb);
    // Project world-map coords to screen space for the current mission map view.
    static bool WorldMapToScreen(const GW::Vec2f& world_map_pos, ImVec2& out);
    static float GetPxPerWorldMapUnit();
};
