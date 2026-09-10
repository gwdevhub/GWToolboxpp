#pragma once

#include <GWCA/Utilities/Export.h>

#include <cstddef>
#include <cstdint>

// forward declaration, we don't need to include the full directx header here
struct IDirect3DDevice9;

namespace GW {

    struct Module;
    extern Module RenderModule;

    namespace Render {

        // Runtime, not build-time: Gw.exe ships both backends and picks one via Param::GetFlag(10).
        enum class Backend : uint32_t {
            Unknown = 0,
            D3D9,
            GLES3,
        };

        GWCA_API Backend GetBackend();

        // VirtualDeviceRenderer::renderer_mode -- all windowed variants, ordered as the settings dropdown lists them.
        enum class RendererMode : uint32_t {
            Windowed = 0,
            WindowedBorderless = 1,
            WindowedFullscreen = 2,
        };

        // Unknown values pass through rather than being coerced.
        GWCA_API RendererMode GetRendererMode();

        // GetDevice() is the Dx9 device, GetGlesDevice() the GLES3 one; pick with GetBackend(). The rest works on all three.

        // GLES3 device, captured from a hook since ddi_device is an id, not a pointer. Offsets confirmed against a running client.
        using EGLSurface = void*;
        using EGLContext = void*;
        using EGLDisplay = void*;

        // The std140 block the fragment shader reads; the layout is exact, taken from the shader source embedded in the client.
        struct GlFragmentRenderState {
            /* +h0000 */ float texture_factor[4];
            /* +h0010 */ float fog_color[4];
            /* +h0020 */ float sampler_biases[2][4];
            /* +h0040 */ float bump_env_mat[8][4];
            /* +h00C0 */ float discard_settings[4];
        };
        static_assert(sizeof(GlFragmentRenderState) == 0xD0);

        // ArenaNet's own struct wrapping EGL handles, not a COM object -- you cannot call methods on it, only read and write cached state.
        struct GlesDevice {
            /* +h0000 */ uint8_t h0000[0x7D4];
            /* +h07D4 */ uint32_t dev_mode;          // GR_MODE_*
            /* +h07D8 */ uint8_t h07D8[0x28];
            /* +h0800 */ void* dev_window;           // native window
            /* +h0804 */ uint8_t h0804[0x8];
            /* +h080C */ EGLSurface dev_surface;
            /* +h0810 */ uint32_t width;             // live surface size
            /* +h0814 */ uint32_t height;
            /* +h0818 */ EGLContext dev_context;
            /* +h081C */ uint8_t h081C[0x7D4];
            /* +h0FF0 */ GlFragmentRenderState fragment_state;
            /* +h10C0 */ uint32_t uniform_buffer;    // the UBO name
        };
        static_assert(offsetof(GlesDevice, dev_mode)        == 0x07D4);
        static_assert(offsetof(GlesDevice, dev_window)      == 0x0800);
        static_assert(offsetof(GlesDevice, dev_surface)     == 0x080C);
        static_assert(offsetof(GlesDevice, dev_context)     == 0x0818);
        static_assert(offsetof(GlesDevice, fragment_state)  == 0x0FF0);
        static_assert(offsetof(GlesDevice, uniform_buffer)  == 0x10C0);

        // Captured from a hook -- not reachable from the generic device. Null unless GetBackend() is GLES3.
        GWCA_API GlesDevice* GetGlesDevice();

        typedef void(__cdecl* RenderCallback) (IDirect3DDevice9*);

        enum Metric : uint32_t {
            Metric0,
            Metric1,
            Metric2,
            Metric3,
            FullscreenGamma,
            MultiSampling,
            PosX,
            PosY,
            RefreshRate,
            ShaderQuality,
            SizeX,
            SizeY,
            TextureFiltering,
            Metric13,
            Metric14,
            Vsync,
            ScreenBorderless,
            Metric17,
            Metric18,
            Metric19,
            Metric20,
            Metric21,
            MonitorRefreshRate,
            TextureMaxCX,
            TextureMaxCY,
            Metric25,
            Count
        };

        GWCA_API void EnableHooks();

        // this returns the FoV used for rendering
        GWCA_API float GetFieldOfView();

        // Called after GW render each frame on D3D9 (End Scene); call GW::Terminate() or RestoreHooks() from within it.
        GWCA_API void SetRenderCallback(RenderCallback callback);

        GWCA_API RenderCallback GetRenderCallback();

        // Flush GW's deferred GR queue so submitted draws materialise, letting a render hook draw between the world and UI passes.
        GWCA_API void FlushCommandQueue();

        // Can be used to get information like vsync status or monitor refresh rate of the renderer
        GWCA_API uint32_t GetGraphicsRendererValue(Metric metric_id, uint32_t renderer_mode = 0xf);
        GWCA_API bool SetGraphicsRendererValue(Metric metric_id, uint32_t value, uint32_t renderer_mode = 0xf);

        // Returns actual hard frame limit, factoring in vsync, monitor refresh rate and in-game preferences
        GWCA_API uint32_t GetFrameLimit();
        // Set a hard upper limit for frame rate. Actual limit may be lower (but not higher) depending on vsync/in-game preference
        GWCA_API bool SetFrameLimit(uint32_t value);

        // D3D9 device reset. GLES has no lost-device concept; SetGlesResetCallback covers the corresponding event.
        GWCA_API void SetResetCallback(RenderCallback callback);
        GWCA_API RenderCallback GetResetCallback();

        // The same two events typed on the GLES device -- set whichever pair matches GetBackend().

        // Fires each frame on GLES3 at the queue flush, so anything drawn goes out with that frame's commands.
        typedef void(__cdecl* GlesRenderCallback)(GlesDevice* gles_device);

        GWCA_API void SetGlesRenderCallback(GlesRenderCallback callback);
        GWCA_API GlesRenderCallback GetGlesRenderCallback();

        // Fires when the GLES device is created or updated -- a state-change event, not per-frame, and where the pointer is captured.
        GWCA_API void SetGlesResetCallback(GlesRenderCallback callback);
        GWCA_API GlesRenderCallback GetGlesResetCallback();

        // The same frame boundary as the render callbacks, without a device, for callers that only need the tick.
        typedef void(__cdecl* FrameCallback)();

        GWCA_API void SetFrameCallback(FrameCallback callback);
        GWCA_API FrameCallback GetFrameCallback();

        // Needs a callback set and called first, does not update while minimized, and returns -1 until known.
        GWCA_API int GetIsFullscreen();

        GWCA_API bool SetFog(bool enabled);

        GWCA_API HWND GetWindowHandle();

        // Null unless GetBackend() == Backend::D3D9.
        GWCA_API IDirect3DDevice9* GetDevice();

        GWCA_API uint32_t GetViewportWidth();
        GWCA_API uint32_t GetViewportHeight();
    }
}
