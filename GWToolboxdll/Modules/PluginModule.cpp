#include "stdafx.h"

#include "PluginModule.h"
#include <MinimapPlugin.h>
#include <PluginApi.h>
#include <Utils/FontLoader.h>

#include <GWToolbox.h>
#include <GWCA/Managers/ChatMgr.h>

#include <Defender.h>
#include <Module.h>
#include <Modules/Resources.h>
#include <filesystem>
#include <string>

#include "GWCA/Managers/UIMgr.h"
#include "Utils/TextUtils.h"

namespace {
    std::wstring pluginsfoldername;

    const char* plugins_enabled_section = "Plugins Enabled";

    std::vector<PluginModule::Plugin*> plugins_available;

    std::vector<PluginModule::Plugin*> plugins_loaded;

    class PluginMinimapRenderer final : public MinimapRenderer {
    public:
        PluginMinimapRenderer(void* _context, const GWToolboxMinimapRenderFn _render)
            : context(_context), render(_render) {}

        void RenderMinimap(IDirect3DDevice9* device, const MinimapRenderContext& render_context) override
        {
            render(context, device, &render_context);
        }

        void* const context;
        const GWToolboxMinimapRenderFn render;
    };
    std::vector<std::unique_ptr<PluginMinimapRenderer>> plugin_minimap_renderers;

    // A plugin that forgets to unregister would leave the minimap calling into its unloaded dll.
    void RemoveMinimapRenderersFrom(const HMODULE dll)
    {
        std::erase_if(plugin_minimap_renderers, [dll](const auto& renderer) {
            HMODULE owner = nullptr;
            GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCWSTR>(renderer->render), &owner);
            if (owner != dll) return false;
            MinimapRenderer::UnregisterRenderer(renderer.get());
            return true;
        });
    }

    void __cdecl HostLog(const GWToolboxLogLevel level, const char* message)
    {
        switch (level) {
            case GWToolboxLog_Info:
                Log::Info("%s", message);
                break;
            case GWToolboxLog_Flash:
                Log::Flash("%s", message);
                break;
            case GWToolboxLog_Warning:
                Log::Warning("%s", message);
                break;
            case GWToolboxLog_Error:
                Log::Error("%s", message);
                break;
            default:
                Log::Log("%s", message);
                break;
        }
    }

    void __cdecl HostLogW(const GWToolboxLogLevel level, const wchar_t* message)
    {
        switch (level) {
            case GWToolboxLog_Info:
                Log::InfoW(L"%ls", message);
                break;
            case GWToolboxLog_Flash:
                Log::FlashW(L"%ls", message);
                break;
            case GWToolboxLog_Warning:
                Log::WarningW(L"%ls", message);
                break;
            case GWToolboxLog_Error:
                Log::ErrorW(L"%ls", message);
                break;
            default:
                Log::LogW(L"%ls", message);
                break;
        }
    }

    GWToolboxHostApi host_api = {
        .abi_version = GWTOOLBOX_PLUGIN_ABI_VERSION,
        .log = HostLog,
        .log_w = HostLogW,
        .flush_log = [] { Log::FlushFile(); },
        .fatal_assert = [](const char* expr, const char* file, const unsigned line) { Log::FatalAssert(expr, file, line); },
        .get_font = [] { return FontLoader::GetFont(); },
        .register_minimap_renderer = [](void* context, const GWToolboxMinimapRenderFn render) {
            const auto& renderer = plugin_minimap_renderers.emplace_back(std::make_unique<PluginMinimapRenderer>(context, render));
            MinimapRenderer::RegisterRenderer(renderer.get());
        },
        .unregister_minimap_renderer = [](void* context) {
            std::erase_if(plugin_minimap_renderers, [context](const auto& renderer) {
                if (renderer->context != context) return false;
                MinimapRenderer::UnregisterRenderer(renderer.get());
                return true;
            });
        },
    };

    bool UnloadPlugin(PluginModule::Plugin* plugin_ptr)
    {
        auto& plugin = *plugin_ptr;
        if (!plugin.terminating) {
            if (plugin.api) {
                plugin.api->signal_terminate();
            }
            plugin.terminating = true;
        }
        if (plugin.api && !plugin.api->can_terminate()) {
            return false; // Pending
        }

        if (plugin.api) {
            plugin.api->terminate();
        }
        if (plugin.dll) {
            RemoveMinimapRenderersFrom(plugin.dll);
        }
        plugin.initialized = false;
        plugin.terminating = false;
        plugin.api = nullptr;
        if (SUCCEEDED(FreeLibrary(plugin.dll))) {
            plugin.dll = nullptr;
        }
        std::erase_if(plugins_loaded, [plugin_ptr](auto p) { return p == plugin_ptr; });
        return true;
    }

    bool LoadPlugin(PluginModule::Plugin* plugin_ptr)
    {
        auto& plugin = *plugin_ptr;
        if (plugin.api) {
            return true;
        }
        if (!plugin.dll) {
            plugin.dll = LoadLibraryW(plugin.path.wstring().c_str());
        }
        if (!plugin.dll) {
            const DWORD err = GetLastError();
            const auto filename = plugin.path.filename();
            UnloadPlugin(plugin_ptr);
            const auto name = TextUtils::PrintFilename(filename.wstring());
            std::wstring detail;
            if ((err == ERROR_VIRUS_INFECTED || err == ERROR_VIRUS_DELETED) && FindRecentDefenderBlock(filename.wstring(), 15, detail))
                Log::ErrorW(L"Failed to load plugin %ls - Windows Defender blocked it: %ls", name.c_str(), detail.c_str());
            else
                Log::ErrorW(L"Failed to load plugin %ls (LoadLibraryW)", name.c_str());
            return false;
        }
        const auto get_api = GetModuleFunctionW<GWToolboxPluginGetApiFn>(plugin.dll, GWTOOLBOX_PLUGIN_GET_API_EXPORT);
        const auto api = get_api ? get_api(GWTOOLBOX_PLUGIN_ABI_VERSION) : nullptr;
        if (!api || api->abi_version != GWTOOLBOX_PLUGIN_ABI_VERSION) {
            UnloadPlugin(plugin_ptr);
            Log::Error("Failed to load plugin %s: it was not built with this version's plugin SDK, so it needs rebuilding", TextUtils::PrintFilename(plugin.path.filename().string()).c_str());
            return false;
        }
        plugin.api = api;
        plugins_loaded.push_back(plugin_ptr);
        return true;
    }

    bool InitializePlugin(PluginModule::Plugin* plugin_ptr)
    {
        auto& plugin = *plugin_ptr;
        if (plugin.terminating || !plugin.api) {
            return false;
        }
        if (plugin.initialized) {
            return true;
        }
        const auto context = ImGui::GetCurrentContext();
        if (!context) {
            return false;
        }
        host_api.toolbox_dll = GWToolbox::GetDLLModule();
        host_api.imgui_context = context;
        ImGui::GetAllocatorFunctions(&host_api.imgui_alloc, &host_api.imgui_free, &host_api.imgui_allocator_user_data);
        plugin.api->initialize(&host_api);
        plugin.api->load_settings(pluginsfoldername.c_str());
        plugin.initialized = true;
        return true;
    }

    void RefreshDlls()
    {
        // when we refresh, how do we map the modules that were already loaded to the ones on disk?
        // the dll file may have changed
        namespace fs = std::filesystem;

        const fs::path plugin_folder = pluginsfoldername;

        if (!Resources::EnsureFolderExists(plugin_folder)) {
            return;
        }

        for (auto& p : fs::directory_iterator(plugin_folder)) {
            fs::path file_path = p.path();
            fs::path ext = file_path.extension();
            if (ext == ".lnk") {
                if (SUCCEEDED(Resources::ResolveShortcut(file_path, file_path))) {
                    ext = file_path.extension();
                }
            }
            if (ext == ".dll") {
                const auto found = std::ranges::find_if(plugins_available, [file_path](const auto plugin) {
                    return plugin->path == file_path;
                });
                if (found == plugins_available.end()) {
                    plugins_available.push_back(new PluginModule::Plugin(file_path));
                }
            }
        }
    }
}

void PluginModule::DrawSettingsInternal()
{
    ImGui::PushID("Plugins");

    size_t i = 0;
    for (const auto plugin : plugins_available) {
        ImGui::PushID(i++);
        auto& style = ImGui::GetStyle();
        const auto origin_header_col = style.Colors[ImGuiCol_Header];
        style.Colors[ImGuiCol_Header] = {0, 0, 0, 0};

        static char buf[128];
        const auto has_settings = plugin->initialized && plugin->api && plugin->api->has_settings();
        if (has_settings) {
            sprintf(buf, "      %s", plugin->path.filename().string().c_str());
        }
        else {
            sprintf(buf, "             %s", plugin->path.filename().string().c_str());
        }
        const auto pos = ImGui::GetCursorScreenPos();
        const bool is_showing = has_settings ? ImGui::CollapsingHeader(buf, ImGuiTreeNodeFlags_AllowOverlap) : ImGui::CollapsingHeader(buf, ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_AllowOverlap);

        if (const auto icon = plugin->initialized ? plugin->api->icon() : nullptr) {
            const float text_offset_x = ImGui::GetTextLineHeightWithSpacing() + 4.0f; // TODO: find a proper number
            ImGui::GetWindowDrawList()->AddText(
                ImVec2(pos.x + text_offset_x, pos.y + style.ItemSpacing.y / 2),
                ImColor(style.Colors[ImGuiCol_Text]), icon);
        }

        style.Colors[ImGuiCol_Header] = origin_header_col;

        ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::GetTextLineHeight() - ImGui::GetStyle().FramePadding.x - 128.f);
        snprintf(buf, _countof(buf), "%s###load_unload", plugin->api ? "Unload" : "Load");
        if (ImGui::Button(buf)) {
            if (!plugin->api) {
                LoadPlugin(plugin);
            }
            else {
                UnloadPlugin(plugin);
            }
        }
        if (plugin->api && plugin->api->visible_ptr()) {
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::GetTextLineHeight() - ImGui::GetStyle().FramePadding.x);
            ImGui::Checkbox("##check", plugin->api->visible_ptr());
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Visible");
            }
        }

        if (is_showing && InitializePlugin(plugin) && has_settings) {
            plugin->api->draw_settings();
        }
        ImGui::PopID();
        ImGui::Separator();
    }

    if (ImGui::Button("Refresh")) {
        RefreshDlls();
    }

    ImGui::PopID();
}

bool PluginModule::CanTerminate()
{
    return plugins_loaded.empty();
}

bool PluginModule::WndProc(const UINT msg, const WPARAM wParam, const LPARAM lParam)
{
    bool capture = false;
    for (const auto plugin : plugins_loaded) {
        if (!plugin->api) {
            continue;
        }
        capture |= plugin->api->wnd_proc(msg, wParam, lParam);
    }
    return capture;
}

std::vector<const GWToolboxPluginApi*> PluginModule::GetPlugins()
{
    std::vector<const GWToolboxPluginApi*> plugins;
    for (const auto plugin : plugins_loaded) {
        plugins.push_back(plugin->api);
    }
    return plugins;
}

void PluginModule::Initialize()
{
    pluginsfoldername = Resources::GetPath(L"plugins");
    ToolboxUIElement::Initialize();
    RefreshDlls();
}

void PluginModule::Draw(IDirect3DDevice9* device)
{
    for (const auto plugin : plugins_loaded) {
        if (!InitializePlugin(plugin)) {
            continue;
        }
        if (GW::UI::GetIsWorldMapShowing() && !plugin->api->show_on_world_map()) {
            continue;
        }

        if (plugin->api->visible_ptr() && *plugin->api->visible_ptr()) {
            plugin->api->draw(device);
        }
    }
}

void PluginModule::LoadSettings(SettingsDoc& doc, ToolboxIni* legacy)
{
    ToolboxUIElement::LoadSettings(doc, legacy);
    std::vector<std::string> enabled_plugins;
    if (!doc.Get(Name(), "enabled_plugins", enabled_plugins) && legacy) {
        TNamesDepend dlls_to_load;
        if (legacy->GetAllKeys(plugins_enabled_section, dlls_to_load)) {
            for (const auto& entry : dlls_to_load) {
                enabled_plugins.push_back(entry.pItem);
            }
        }
    }
    std::vector<Plugin*> plugins_enabled_from_settings;
    for (const auto& entry : enabled_plugins) {
        const auto filename = std::filesystem::path(entry).filename();
        auto matching_plugins = std::views::filter(plugins_available, [filename](auto plugin) {
            return plugin->path.filename() == filename;
        });
        for (const auto plugin : matching_plugins) {
            if (!LoadPlugin(plugin)) {
                continue;
            }
            InitializePlugin(plugin);
            plugins_enabled_from_settings.push_back(plugin);
        }
    }
    // Find any plugins that are currently loaded but not supposed to be
    std::vector<decltype(plugins_loaded)::value_type> to_unload;
    std::ranges::copy_if(plugins_loaded, std::back_inserter(to_unload), [&](auto plugin) {
        return !std::ranges::contains(plugins_enabled_from_settings, plugin);
    });
    for (const auto plugin : std::views::reverse(to_unload)) {
        UnloadPlugin(plugin);
    }
}

void PluginModule::SaveSettings(SettingsDoc& doc)
{
    ToolboxUIElement::SaveSettings(doc);
    std::vector<std::string> enabled_plugins;
    for (const auto plugin : plugins_loaded) {
        plugin->api->save_settings(pluginsfoldername.c_str());
        enabled_plugins.push_back(plugin->path.filename().string());
    }
    doc.Set(Name(), "enabled_plugins", enabled_plugins);
}

void PluginModule::Update(const float delta)
{
    static bool message_displayed = false;
    if (!plugins_loaded.empty() && !message_displayed) {
        GW::Chat::WriteChat(
            GW::Chat::Channel::CHANNEL_GWCA2,
            L"<c=#FFFF00>Plugins detected, these may be unsafe to use and are not officially supported by GWToolbox++ developers.\n"
            "Use at your own risk if you trust the author.\n"
            "Do not report bugs that occur while you play with plugins.</c>", GWTOOLBOX_SENDER, true);
        GW::Chat::WriteChat(
            GW::Chat::Channel::CHANNEL_GWCA2,
            L"<c=#FF0000>Plugins are NOT permitted by ArenaNet.</c>", GWTOOLBOX_SENDER, true);
        GW::Chat::WriteChat(
            GW::Chat::Channel::CHANNEL_WARNING,
            L"Plugins are NOT permitted by ArenaNet.", nullptr, true);
        message_displayed = true;
    }
    for (const auto plugin : plugins_loaded) {
        if (!plugin->initialized)
            continue;
        plugin->api->update(delta);
        if (plugin->terminating) {
            if (UnloadPlugin(plugin)) {
                break; // plugins_loaded vector changed, skip a frame
            }
        }
    }
}

void PluginModule::SignalTerminate()
{
    ToolboxUIElement::SignalTerminate();
    const auto snapshot = plugins_loaded;
    for (const auto plugin : snapshot) {
        UnloadPlugin(plugin);
    }
}

void PluginModule::Terminate()
{
    ASSERT(plugins_loaded.empty());
    for (const auto p : plugins_available) {
        if (p->dll) {
            FreeLibrary(p->dll);
        }
        delete p;
    }
}
