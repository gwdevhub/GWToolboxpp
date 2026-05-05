#pragma once

#include <Windows.h>
#include <d3d9.h>
#include <GWCA/GameContainers/GamePos.h>
#include <RectF.h>

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
