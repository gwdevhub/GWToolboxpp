#pragma once
#include <GWCA/Runtime.h>

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

        Backend GetBackend();

        // VirtualDeviceRenderer::renderer_mode -- all windowed variants, ordered as the settings dropdown lists them.
        enum class RendererMode : uint32_t {
            Windowed = 0,
            WindowedBorderless = 1,
            WindowedFullscreen = 2,
        };

        // Unknown values pass through rather than being coerced.
        RendererMode GetRendererMode();

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
        GlesDevice* GetGlesDevice();

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

        void EnableHooks();

        // this returns the FoV used for rendering
        float GetFieldOfView();

        // Called after GW render each frame on D3D9 (End Scene); call GW::Terminate() or RestoreHooks() from within it.
        void SetRenderCallback(RenderCallback callback);

        RenderCallback GetRenderCallback();

        // Flush GW's deferred GR queue so submitted draws materialise, letting a render hook draw between the world and UI passes.
        void FlushCommandQueue();

        // Can be used to get information like vsync status or monitor refresh rate of the renderer
        uint32_t GetGraphicsRendererValue(Metric metric_id, uint32_t renderer_mode = 0xf);
        bool SetGraphicsRendererValue(Metric metric_id, uint32_t value, uint32_t renderer_mode = 0xf);

        // Returns actual hard frame limit, factoring in vsync, monitor refresh rate and in-game preferences
        uint32_t GetFrameLimit();
        // Set a hard upper limit for frame rate. Actual limit may be lower (but not higher) depending on vsync/in-game preference
        bool SetFrameLimit(uint32_t value);

        // D3D9 device reset. GLES has no lost-device concept; SetGlesResetCallback covers the corresponding event.
        void SetResetCallback(RenderCallback callback);
        RenderCallback GetResetCallback();

        // The same two events typed on the GLES device -- set whichever pair matches GetBackend().

        // Fires each frame on GLES3 at the queue flush, so anything drawn goes out with that frame's commands.
        typedef void(__cdecl* GlesRenderCallback)(GlesDevice* gles_device);

        void SetGlesRenderCallback(GlesRenderCallback callback);
        GlesRenderCallback GetGlesRenderCallback();

        // Fires when the GLES device is created or updated -- a state-change event, not per-frame, and where the pointer is captured.
        void SetGlesResetCallback(GlesRenderCallback callback);
        GlesRenderCallback GetGlesResetCallback();

        // The same frame boundary as the render callbacks, without a device, for callers that only need the tick.
        typedef void(__cdecl* FrameCallback)();

        void SetFrameCallback(FrameCallback callback);
        FrameCallback GetFrameCallback();

        // Needs a callback set and called first, does not update while minimized, and returns -1 until known.
        int GetIsFullscreen();

        bool SetFog(bool enabled);

        HWND GetWindowHandle();

        // Null unless GetBackend() == Backend::D3D9.
        IDirect3DDevice9* GetDevice();

        uint32_t GetViewportWidth();
        uint32_t GetViewportHeight();
    }
}

namespace GW::Render {
    extern "C" GWCA_API Backend __cdecl GW_Render_GetBackend() noexcept;
    extern "C" GWCA_API RendererMode __cdecl GW_Render_GetRendererMode() noexcept;
    extern "C" GWCA_API GlesDevice* __cdecl GW_Render_GetGlesDevice() noexcept;
    extern "C" GWCA_API void __cdecl GW_Render_EnableHooks() noexcept;
    extern "C" GWCA_API float __cdecl GW_Render_GetFieldOfView() noexcept;
    extern "C" GWCA_API void __cdecl GW_Render_SetRenderCallback(RenderCallback callback) noexcept;
    extern "C" GWCA_API RenderCallback __cdecl GW_Render_GetRenderCallback() noexcept;
    extern "C" GWCA_API void __cdecl GW_Render_FlushCommandQueue() noexcept;
    extern "C" GWCA_API uint32_t __cdecl GW_Render_GetGraphicsRendererValue(Metric metric_id, uint32_t renderer_mode) noexcept;
    extern "C" GWCA_API bool __cdecl GW_Render_SetGraphicsRendererValue(Metric metric_id, uint32_t value, uint32_t renderer_mode) noexcept;
    extern "C" GWCA_API uint32_t __cdecl GW_Render_GetFrameLimit() noexcept;
    extern "C" GWCA_API bool __cdecl GW_Render_SetFrameLimit(uint32_t value) noexcept;
    extern "C" GWCA_API void __cdecl GW_Render_SetResetCallback(RenderCallback callback) noexcept;
    extern "C" GWCA_API RenderCallback __cdecl GW_Render_GetResetCallback() noexcept;
    extern "C" GWCA_API void __cdecl GW_Render_SetGlesRenderCallback(GlesRenderCallback callback) noexcept;
    extern "C" GWCA_API GlesRenderCallback __cdecl GW_Render_GetGlesRenderCallback() noexcept;
    extern "C" GWCA_API void __cdecl GW_Render_SetGlesResetCallback(GlesRenderCallback callback) noexcept;
    extern "C" GWCA_API GlesRenderCallback __cdecl GW_Render_GetGlesResetCallback() noexcept;
    extern "C" GWCA_API void __cdecl GW_Render_SetFrameCallback(FrameCallback callback) noexcept;
    extern "C" GWCA_API FrameCallback __cdecl GW_Render_GetFrameCallback() noexcept;
    extern "C" GWCA_API int __cdecl GW_Render_GetIsFullscreen() noexcept;
    extern "C" GWCA_API bool __cdecl GW_Render_SetFog(bool enabled) noexcept;
    extern "C" GWCA_API HWND __cdecl GW_Render_GetWindowHandle() noexcept;
    extern "C" GWCA_API IDirect3DDevice9* __cdecl GW_Render_GetDevice() noexcept;
    extern "C" GWCA_API uint32_t __cdecl GW_Render_GetViewportWidth() noexcept;
    extern "C" GWCA_API uint32_t __cdecl GW_Render_GetViewportHeight() noexcept;
}

#if !defined(GWCA_BUILD_EXPORTS)
namespace GW::Render {
    inline Backend GetBackend()
    {
        return GWCA_CALL_C(GW_Render_GetBackend);
    }

    inline RendererMode GetRendererMode()
    {
        return GWCA_CALL_C(GW_Render_GetRendererMode);
    }

    inline GlesDevice* GetGlesDevice()
    {
        return GWCA_CALL_C(GW_Render_GetGlesDevice);
    }

    inline void EnableHooks()
    {
        return GWCA_CALL_C(GW_Render_EnableHooks);
    }

    inline float GetFieldOfView()
    {
        return GWCA_CALL_C(GW_Render_GetFieldOfView);
    }

    inline void SetRenderCallback(RenderCallback callback)
    {
        return GWCA_CALL_C(GW_Render_SetRenderCallback, callback);
    }

    inline RenderCallback GetRenderCallback()
    {
        return GWCA_CALL_C(GW_Render_GetRenderCallback);
    }

    inline void FlushCommandQueue()
    {
        return GWCA_CALL_C(GW_Render_FlushCommandQueue);
    }

    inline uint32_t GetGraphicsRendererValue(Metric metric_id, uint32_t renderer_mode)
    {
        return GWCA_CALL_C(GW_Render_GetGraphicsRendererValue, metric_id, renderer_mode);
    }

    inline bool SetGraphicsRendererValue(Metric metric_id, uint32_t value, uint32_t renderer_mode)
    {
        return GWCA_CALL_C(GW_Render_SetGraphicsRendererValue, metric_id, value, renderer_mode);
    }

    inline uint32_t GetFrameLimit()
    {
        return GWCA_CALL_C(GW_Render_GetFrameLimit);
    }

    inline bool SetFrameLimit(uint32_t value)
    {
        return GWCA_CALL_C(GW_Render_SetFrameLimit, value);
    }

    inline void SetResetCallback(RenderCallback callback)
    {
        return GWCA_CALL_C(GW_Render_SetResetCallback, callback);
    }

    inline RenderCallback GetResetCallback()
    {
        return GWCA_CALL_C(GW_Render_GetResetCallback);
    }

    inline void SetGlesRenderCallback(GlesRenderCallback callback)
    {
        return GWCA_CALL_C(GW_Render_SetGlesRenderCallback, callback);
    }

    inline GlesRenderCallback GetGlesRenderCallback()
    {
        return GWCA_CALL_C(GW_Render_GetGlesRenderCallback);
    }

    inline void SetGlesResetCallback(GlesRenderCallback callback)
    {
        return GWCA_CALL_C(GW_Render_SetGlesResetCallback, callback);
    }

    inline GlesRenderCallback GetGlesResetCallback()
    {
        return GWCA_CALL_C(GW_Render_GetGlesResetCallback);
    }

    inline void SetFrameCallback(FrameCallback callback)
    {
        return GWCA_CALL_C(GW_Render_SetFrameCallback, callback);
    }

    inline FrameCallback GetFrameCallback()
    {
        return GWCA_CALL_C(GW_Render_GetFrameCallback);
    }

    inline int GetIsFullscreen()
    {
        return GWCA_CALL_C(GW_Render_GetIsFullscreen);
    }

    inline bool SetFog(bool enabled)
    {
        return GWCA_CALL_C(GW_Render_SetFog, enabled);
    }

    inline HWND GetWindowHandle()
    {
        return GWCA_CALL_C(GW_Render_GetWindowHandle);
    }

    inline IDirect3DDevice9* GetDevice()
    {
        return GWCA_CALL_C(GW_Render_GetDevice);
    }

    inline uint32_t GetViewportWidth()
    {
        return GWCA_CALL_C(GW_Render_GetViewportWidth);
    }

    inline uint32_t GetViewportHeight()
    {
        return GWCA_CALL_C(GW_Render_GetViewportHeight);
    }
}
#endif
