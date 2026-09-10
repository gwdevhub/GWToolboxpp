#pragma once

#include <functional>
#include <d3d9.h>

namespace GameWorldCompositor {
    using DrawCallback = std::function<void(IDirect3DDevice9* device)>;

    inline constexpr float kZNear = 46.875f;
    inline constexpr float kZFar = 48000.f;

    int RegisterDraw(DrawCallback callback);
    void UnregisterDraw(int token);

    // True once the hook is installed and operational; false if scanning/installing it failed.
    // A module that can also draw on top of the UI should fall back to that when this is false.
    [[nodiscard]] bool IsActive();
    [[nodiscard]] bool HasFailed();

    // Reset the once-per-frame draw guard. Must be called exactly once per rendered frame.
    void BeginFrame();

    bool SetupPipeline(IDirect3DDevice9* device, bool occlude, float max_distance, float fog_factor);

    bool SetWorldViewProj(IDirect3DDevice9* device);
    void SetWorldRenderStates(IDirect3DDevice9* device, bool occlude);
    void SetDistanceFog(IDirect3DDevice9* device, float max_distance, float fog_factor);

    // The shared pipeline objects, valid after a successful SetupPipeline - e.g. to restore the
    // programmable pipeline after a fixed-function pass (such as a stencil punch-out).
    [[nodiscard]] IDirect3DVertexShader9* VertexShader();
    [[nodiscard]] IDirect3DPixelShader9* PixelShader();
    [[nodiscard]] IDirect3DVertexDeclaration9* VertexDeclaration();

#ifdef _DEBUG
    // Log every FrCacheRenderAll invocation's buffer layout for the next few calls (harness diagnostics).
    void RequestBufferDump();
#endif

    // Remove the hook and release shared GPU resources (shutdown).
    void Terminate();
} // namespace GameWorldCompositor
