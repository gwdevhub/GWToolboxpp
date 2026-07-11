#pragma once

#include <functional>
#include <d3d9.h>

// Shared access to Guild Wars' FrCache render pass. GW executes a deferred command buffer
// (the 3D world first, then the in-game UI/HUD). This hooks that pass once and lets modules
// inject draws into the seam between the two, so their geometry composites UNDER the HUD and
// is depth-tested against the scene (occluded by terrain). Only one hook exists process-wide;
// modules register callbacks here instead of each hooking the game themselves.
namespace GameWorldCompositor {
    using DrawCallback = std::function<void(IDirect3DDevice9* device)>;

    // GW's in-world camera clip planes, hardcoded in the engine: far = 48000 at every world
    // projection setup call site in Gw.exe, near = far/1024 (Q readback = 1024/1023). They are
    // not queryable from the device (GW creates a PUREDEVICE and only sets identity FF
    // transforms; the world projection lives in per-shader constants).
    inline constexpr float kZNear = 46.875f;
    inline constexpr float kZFar = 48000.f;

    // Register a per-frame draw, run between the world and HUD passes. The hook is installed
    // on the first registration and removed when the last callback is gone. Returns a token
    // (> 0) to pass to UnregisterDraw, or 0 on failure.
    int RegisterDraw(DrawCallback callback);
    void UnregisterDraw(int token);

    // True once the hook is installed and operational; false if scanning/installing it failed.
    // A module that can also draw on top of the UI should fall back to that when this is false.
    [[nodiscard]] bool IsActive();
    [[nodiscard]] bool HasFailed();

    // Reset the once-per-frame draw guard. Must be called exactly once per rendered frame.
    void BeginFrame();

    // Configure `device` to draw world-space coloured geometry (POSITION + D3DCOLOR vertices
    // through the shared world shaders), depth-tested against the scene when `occlude`, and
    // faded out with distance from the camera focus. Creates the shared shaders on first use.
    // Returns false if the pipeline could not be set (caller should skip drawing). Callers must
    // wrap this in a saved state block so GW's own device state is restored afterwards.
    bool SetupPipeline(IDirect3DDevice9* device, bool occlude, float max_distance, float fog_factor);

    // Building blocks of SetupPipeline, for modules that bring their OWN shaders/vertex format
    // (e.g. textured sprites) but still want the same world transform, occlusion states and
    // distance fade. Bind your shaders/declaration, then call these (inside a saved state block).
    // SetWorldViewProj fills vertex-shader constants c0-c3 (view) and c4-c7 (projection).
    // SetDistanceFog fills pixel-shader constants c0 (camera focus), c1 (max dist), c2 (fog start).
    bool SetWorldViewProj(IDirect3DDevice9* device);
    void SetWorldRenderStates(IDirect3DDevice9* device, bool occlude);
    void SetDistanceFog(IDirect3DDevice9* device, float max_distance, float fog_factor);

    // The shared pipeline objects, valid after a successful SetupPipeline - e.g. to restore the
    // programmable pipeline after a fixed-function pass (such as a stencil punch-out).
    [[nodiscard]] IDirect3DVertexShader9* VertexShader();
    [[nodiscard]] IDirect3DPixelShader9* PixelShader();
    [[nodiscard]] IDirect3DVertexDeclaration9* VertexDeclaration();

    // Remove the hook and release shared GPU resources (shutdown).
    void Terminate();
} // namespace GameWorldCompositor
