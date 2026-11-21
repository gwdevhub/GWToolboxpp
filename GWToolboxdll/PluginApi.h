#pragma once

// The whole boundary between GWToolboxdll and a plugin dll: plain C structs and __cdecl function pointers, so a
// plugin built with any compiler works with a toolbox built with any other. Bump the version on any layout change.

#include <cstddef>
#include <cstdint>

#include <windows.h>

struct IDirect3DDevice9;
struct ImGuiContext;
struct ImFont;
struct MinimapRenderContext;

constexpr uint32_t GWTOOLBOX_PLUGIN_ABI_VERSION = 1;

extern "C" {
    enum GWToolboxLogLevel : uint32_t {
        GWToolboxLog_Log,
        GWToolboxLog_Info,
        GWToolboxLog_Flash,
        GWToolboxLog_Warning,
        GWToolboxLog_Error,
    };

    using GWToolboxMinimapRenderFn = void(__cdecl*)(void* context, IDirect3DDevice9* device, const MinimapRenderContext* render_context);

    struct GWToolboxHostApi {
        uint32_t abi_version;
        HMODULE toolbox_dll;
        ImGuiContext* imgui_context;
        void* (__cdecl* imgui_alloc)(size_t size, void* user_data);
        void (__cdecl* imgui_free)(void* ptr, void* user_data);
        void* imgui_allocator_user_data;
        void (__cdecl* log)(GWToolboxLogLevel level, const char* message);
        void (__cdecl* log_w)(GWToolboxLogLevel level, const wchar_t* message);
        void (__cdecl* flush_log)();
        void (__cdecl* fatal_assert)(const char* expr, const char* file, unsigned line);
        ImFont* (__cdecl* get_font)();
        void (__cdecl* register_minimap_renderer)(void* context, GWToolboxMinimapRenderFn render);
        void (__cdecl* unregister_minimap_renderer)(void* context);
    };

    struct GWToolboxPluginApi {
        uint32_t abi_version;
        const char* (__cdecl* name)();
        const char* (__cdecl* icon)();
        bool (__cdecl* has_settings)();
        bool* (__cdecl* visible_ptr)();
        bool (__cdecl* show_in_main_menu)();
        bool (__cdecl* show_on_world_map)();
        void (__cdecl* initialize)(const GWToolboxHostApi* host);
        void (__cdecl* signal_terminate)();
        bool (__cdecl* can_terminate)();
        void (__cdecl* terminate)();
        void (__cdecl* update)(float delta_ms);
        void (__cdecl* draw)(IDirect3DDevice9* device);
        bool (__cdecl* wnd_proc)(UINT message, WPARAM wparam, LPARAM lparam);
        void (__cdecl* load_settings)(const wchar_t* folder);
        void (__cdecl* save_settings)(const wchar_t* folder);
        void (__cdecl* draw_settings)();
        bool (__cdecl* draw_tab_button)(bool show_icon, bool show_text, bool center_align_text);
    };

    // The one export every plugin dll provides; returns null when it was built for a different ABI version.
    using GWToolboxPluginGetApiFn = const GWToolboxPluginApi* (__cdecl*)(uint32_t host_abi_version);
}

constexpr const char* GWTOOLBOX_PLUGIN_GET_API_EXPORT = "GWToolboxPlugin_GetApi";
