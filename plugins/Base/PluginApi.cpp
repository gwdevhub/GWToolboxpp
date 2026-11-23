#include "ToolboxPlugin.h"

#include <cstdarg>
#include <cwchar>
#include <string>

#include <Logger.h>
#include <MinimapPlugin.h>
#include <PluginApi.h>
#include <Utils/FontLoader.h>

// The plugin side of PluginApi.h: the toolbox only ever sees the C table below, so the author's C++ (virtual calls,
// Log::*, MinimapRenderer) stays inside this dll and its compiler.
namespace {
    const GWToolboxHostApi* host = nullptr;

    ToolboxPlugin& Plugin()
    {
        return *ToolboxPluginInstance();
    }

    std::string Format(const char* format, va_list args)
    {
        va_list measure;
        va_copy(measure, args);
        const auto length = vsnprintf(nullptr, 0, format, measure);
        va_end(measure);
        if (length < 0) return {};
        std::string text(static_cast<size_t>(length), '\0');
        vsnprintf(text.data(), text.size() + 1, format, args);
        return text;
    }

    // Only MSVC's vswprintf reports the needed length for a null buffer, so grow until it fits.
    std::wstring Format(const wchar_t* format, va_list args)
    {
        std::wstring text(256, L'\0');
        while (text.size() < (1u << 20)) {
            va_list attempt;
            va_copy(attempt, args);
            const auto written = vswprintf(text.data(), text.size(), format, attempt);
            va_end(attempt);
            if (written >= 0) {
                text.resize(static_cast<size_t>(written));
                return text;
            }
            text.resize(text.size() * 2);
        }
        return {};
    }

    void Send(const GWToolboxLogLevel level, const char* format, va_list args)
    {
        if (host) host->log(level, Format(format, args).c_str());
    }

    void Send(const GWToolboxLogLevel level, const wchar_t* format, va_list args)
    {
        if (host) host->log_w(level, Format(format, args).c_str());
    }

    const GWToolboxPluginApi plugin_api = {
        .abi_version = GWTOOLBOX_PLUGIN_ABI_VERSION,
        .name = [] { return Plugin().Name(); },
        .icon = [] { return Plugin().Icon(); },
        .has_settings = [] { return Plugin().HasSettings(); },
        .visible_ptr = [] { return Plugin().GetVisiblePtr(); },
        .show_in_main_menu = [] { return Plugin().ShowInMainMenu(); },
        .show_on_world_map = [] { return Plugin().ShowOnWorldMap(); },
        .initialize = [](const GWToolboxHostApi* host_api) {
            host = host_api;
            Plugin().Initialize(host_api->imgui_context, {host_api->imgui_alloc, host_api->imgui_free, host_api->imgui_allocator_user_data}, host_api->toolbox_dll);
        },
        .signal_terminate = [] { Plugin().SignalTerminate(); },
        .can_terminate = [] { return Plugin().CanTerminate(); },
        .terminate = [] { Plugin().Terminate(); },
        .update = [](const float delta_ms) { Plugin().Update(delta_ms); },
        .draw = [](IDirect3DDevice9* device) { Plugin().Draw(device); },
        .wnd_proc = [](const UINT message, const WPARAM wparam, const LPARAM lparam) { return Plugin().WndProc(message, wparam, lparam); },
        .load_settings = [](const wchar_t* folder) { Plugin().LoadSettings(folder); },
        .save_settings = [](const wchar_t* folder) { Plugin().SaveSettings(folder); },
        .draw_settings = [] { Plugin().DrawSettings(); },
        .draw_tab_button = [](const bool show_icon, const bool show_text, const bool center_align_text) { return Plugin().DrawTabButton(show_icon, show_text, center_align_text); },
    };
}

extern "C" __declspec(dllexport) const GWToolboxPluginApi* __cdecl GWToolboxPlugin_GetApi(const uint32_t host_abi_version)
{
    return host_abi_version == GWTOOLBOX_PLUGIN_ABI_VERSION ? &plugin_api : nullptr;
}

void Log::Log(const char* msg, ...)
{
    va_list args;
    va_start(args, msg);
    Send(GWToolboxLog_Log, msg, args);
    va_end(args);
}

void Log::LogW(const wchar_t* msg, ...)
{
    va_list args;
    va_start(args, msg);
    Send(GWToolboxLog_Log, msg, args);
    va_end(args);
}

void Log::FlushFile()
{
    if (host) host->flush_log();
}

void Log::Info(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    Send(GWToolboxLog_Info, format, args);
    va_end(args);
}

void Log::InfoW(const wchar_t* format, ...)
{
    va_list args;
    va_start(args, format);
    Send(GWToolboxLog_Info, format, args);
    va_end(args);
}

void Log::Flash(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    Send(GWToolboxLog_Flash, format, args);
    va_end(args);
}

void Log::FlashW(const wchar_t* format, ...)
{
    va_list args;
    va_start(args, format);
    Send(GWToolboxLog_Flash, format, args);
    va_end(args);
}

void Log::Error(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    Send(GWToolboxLog_Error, format, args);
    va_end(args);
}

void Log::ErrorW(const wchar_t* format, ...)
{
    va_list args;
    va_start(args, format);
    Send(GWToolboxLog_Error, format, args);
    va_end(args);
}

void Log::Warning(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    Send(GWToolboxLog_Warning, format, args);
    va_end(args);
}

void Log::WarningW(const wchar_t* format, ...)
{
    va_list args;
    va_start(args, format);
    Send(GWToolboxLog_Warning, format, args);
    va_end(args);
}

void Log::FatalAssert(const char* expr, const char* file, const unsigned line)
{
    if (host) host->fatal_assert(expr, file, line);
}

ImFont* FontLoader::GetFont()
{
    return host ? host->get_font() : nullptr;
}

void MinimapRenderer::RegisterRenderer(MinimapRenderer* renderer)
{
    if (!host) return;
    host->register_minimap_renderer(renderer, [](void* context, IDirect3DDevice9* device, const MinimapRenderContext* render_context) {
        static_cast<MinimapRenderer*>(context)->RenderMinimap(device, *render_context);
    });
}

void MinimapRenderer::UnregisterRenderer(MinimapRenderer* renderer)
{
    if (host) host->unregister_minimap_renderer(renderer);
}
