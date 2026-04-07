#pragma once

#include <GWCA/GameEntities/Party.h>

#include <ToolboxWidget.h>

#include "Defines.h"
#include "gwtoolboxdll_export.h"

#include <Utils/GuiUtils.h>

#include <Widgets/Minimap/AgentRenderer.h>
#include <Widgets/Minimap/CustomRenderer.h>
#include <Widgets/Minimap/EffectRenderer.h>
#include <Widgets/Minimap/GameWorldRenderer.h>
#include <Widgets/Minimap/PingsLinesRenderer.h>
#include <Widgets/Minimap/PmapRenderer.h>
#include <Widgets/Minimap/RangeRenderer.h>
#include <Widgets/Minimap/SymbolsRenderer.h>

// Context structure that encapsulates all rendering parameters
struct MinimapRenderContext : RectF {
    // Position and size
    ImVec2 anchor_point; // Center of minimap - this may not necessarily be the center point of the clipping rect

    // Camera/view parameters
    GW::Vec2f translation; // World-space translation (for panning)
    float zoom_scale = 1.f; // Zoom level (scale factor)
    float rotation = 1.5708f; // Map rotation in radians

    float base_scale = 1.f; // The size (in px) of the base scale to use before zooming. Minimap sets this to size.x

    // Visual options
    bool circular_map = false; // Whether to render as circle or square
    bool draw_center_marker = false; // Whether to draw center marker when panned
    D3DCOLOR background_color = D3DCOLOR_ARGB(50, 0, 0, 0); // Background color (or 0 to use renderer's default)
    D3DCOLOR foreground_color = D3DCOLOR_ARGB(0xff, 0xe0, 0xe0, 0xe0); // Foreground color (or 0 to use renderer's default)
    D3DCOLOR shadow_color = 0; // Drop shadow for foreground color
    D3DCOLOR cardinal_color = 0;

    bool draw_ranges = false;

    RECT rect() const
    {
        return {static_cast<LONG>(top_left.x), static_cast<LONG>(top_left.y), static_cast<LONG>(bottom_right.x), static_cast<LONG>(bottom_right.y)};
    }
};

// subclass this structure and register an instance to have plugins draw extra stuff on a minimap
class GWTOOLBOXDLL_EXPORT MinimapRenderer {
public:
    MinimapRenderer() = default;
    virtual ~MinimapRenderer() = default;

    // minimap holds pointers to registered renderers, so move is forbidden
    MinimapRenderer(const MinimapRenderer&) = delete;
    MinimapRenderer(MinimapRenderer&&) = delete;
    MinimapRenderer& operator=(const MinimapRenderer&) = delete;
    MinimapRenderer& operator=(MinimapRenderer&&) = delete;

    // implement this to draw your data on the minimap
    // the view matrix is set up to transform game world positions to minimap appropriately, make sure to restore it if you touch it
    // the world matrix is undefined and can be left in any state
    virtual void RenderMinimap(IDirect3DDevice9* device, const MinimapRenderContext& context) = 0;
};

class Minimap final : public ToolboxWidget {
    Minimap()
    {
        is_resizable = false;
    }

    ~Minimap() override = default;

public:
    Minimap(const Minimap&) = delete;

    static Minimap& Instance()
    {
        static Minimap instance;
        return instance;
    }

    const int ms_before_back = 1000; // time before we snap back to player
    const float acceleration = 0.5f;
    const float max_speed = 15.0f; // game units per frame

    [[nodiscard]] const char* Name() const override { return "Minimap"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_MAP_MARKED_ALT; }

    [[nodiscard]] float Scale() const;

    void DrawHelp() override;
    void Initialize() override;
    void SignalTerminate() override;
    bool CanTerminate() override;
    void Terminate() override;

    // Widget-based rendering (uses internal state)
    void Draw(IDirect3DDevice9* device) override;

    // Static rendering with explicit context (for multiple minimaps)
    static void Render(IDirect3DDevice9* device, const MinimapRenderContext& context);

    // Setup projection matrix for a given context
    static void RenderSetupProjection(IDirect3DDevice9* device, const MinimapRenderContext& context);

    GWTOOLBOXDLL_EXPORT static void RegisterRenderer(MinimapRenderer* renderer);
    GWTOOLBOXDLL_EXPORT static void UnregisterRenderer(MinimapRenderer* renderer);

    bool FlagHeros(LPARAM lParam);
    bool OnMouseDown(UINT Message, WPARAM wParam, LPARAM lParam);
    [[nodiscard]] bool OnMouseDblClick(UINT Message, WPARAM wParam, LPARAM lParam) const;
    bool OnMouseUp(UINT Message, WPARAM wParam, LPARAM lParam);
    bool OnMouseMove(UINT Message, WPARAM wParam, LPARAM lParam);
    bool OnMouseWheel(UINT Message, WPARAM wParam, LPARAM lParam);
    static void CHAT_CMD_FUNC(OnFlagHeroCmd);
    bool WndProc(UINT Message, WPARAM wParam, LPARAM lParam) override;

    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
    void DrawSettingsInternal() override;

    [[nodiscard]] float GetMapRotation() const;
    [[nodiscard]] GW::Vec2f ShadowstepLocation() const;

    // 0 is 'all' flag, 1 to 7 is each hero
    static bool FlagHero(uint32_t idx);

    RangeRenderer range_renderer;
    PmapRenderer pmap_renderer;
    AgentRenderer agent_renderer;
    PingsLinesRenderer pingslines_renderer;
    SymbolsRenderer symbols_renderer;
    CustomRenderer custom_renderer;
    EffectRenderer effect_renderer;
    std::vector<MinimapRenderer*> registered_renderers;

    static bool ShouldMarkersDrawOnMap();
    static bool ShouldDrawAllQuests();

    [[nodiscard]] static bool IsActive();

private:
    [[nodiscard]] bool IsInside(int x, int y) const;
    // returns true if the map is visible, valid, not loading, etc

    static void SelectTarget(GW::Vec2f pos);
    static size_t GetPlayerHeroes(const GW::PartyInfo* party, std::vector<GW::AgentID>& _player_heroes, bool* has_flags = nullptr);

    static void OnUIMessage(GW::HookStatus*, GW::UI::UIMessage /*msgid*/, void* /*wParam*/, void*);
};
