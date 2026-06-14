#pragma once

#include <Windows.h>
#include <d3d9.h>
#include <GWCA/GameContainers/GamePos.h>
#include <RectF.h>

class D3DVertexBuffer;

#ifndef GWTOOLBOXDLL_MINIMAP_EXPORT
#  ifdef GWToolboxdll_EXPORTS
#    define GWTOOLBOXDLL_MINIMAP_EXPORT __declspec(dllexport)
#  else
#    define GWTOOLBOXDLL_MINIMAP_EXPORT __declspec(dllimport)
#  endif
#endif

// Context structure that encapsulates all rendering parameters
struct MinimapRenderContext : RectF {
    // Position and size
    ImVec2 anchor_point = {}; // Center of minimap - this may not necessarily be the center point of the clipping rect

    // Camera/view parameters
    GW::Vec2f translation = {}; // World-space translation (for panning)
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

    // Transform overrides — when non-null, bypass the player-centric view / ortho projection
    // (e.g. drive the pipeline from the mission map's game->screen basis)
    const D3DMATRIX* view_override = nullptr; // game coords -> screen px
    const D3DMATRIX* projection_override = nullptr; // screen px -> clip
    float gwinches_per_pixel_override = 0.f; // 0: derive from base_scale/zoom

    // External line buffer drawn at the custom-lines layer, kept separate from the minimap's
    // CustomRenderer (e.g. the mission map's own draw_on_mission_map lines). Null = none.
    D3DVertexBuffer* lines = nullptr;

    // Per-layer toggles; all default on to preserve the standard minimap output
    bool draw_background = true;
    bool draw_pmap = true;
    bool draw_custom = true;
    bool draw_symbols = true;
    bool draw_agents = true;
    bool draw_effects = true;
    bool draw_pings = true;
    bool draw_cardinals = true;

    RECT rect() const
    {
        return {static_cast<LONG>(top_left.x), static_cast<LONG>(top_left.y), static_cast<LONG>(bottom_right.x), static_cast<LONG>(bottom_right.y)};
    }
};

// subclass this structure and register an instance to have plugins draw extra stuff on a minimap
// Vtable layout (do not change): slot 0 = ~MinimapRenderer, slot 1 = RenderMinimap.
class MinimapRenderer {
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

    GWTOOLBOXDLL_MINIMAP_EXPORT static void RegisterRenderer(MinimapRenderer* renderer);
    GWTOOLBOXDLL_MINIMAP_EXPORT static void UnregisterRenderer(MinimapRenderer* renderer);
};
