#include "stdafx.h"

#include "PluginModule.h"
#include "../plugins/Base/ToolboxPlugin.h"

#include <GWToolbox.h>
#include <GWCA/Managers/ChatMgr.h>

#include <Modules/Resources.h>
#include <filesystem>
#include <string>

#include "GWCA/Managers/UIMgr.h"

namespace {
    std::wstring pluginsfoldername;

    const char* plugins_enabled_section = "Plugins Enabled";

    std::vector<PluginModule::Plugin*> plugins_available;

    std::vector<PluginModule::Plugin*> plugins_loaded;

    bool UnloadPlugin(PluginModule::Plugin* plugin_ptr)
    {
        auto& plugin = *plugin_ptr;
        if (!plugin.terminating) {
            if (plugin.instance) {
                plugin.instance->SignalTerminate();
            }
            plugin.terminating = true;
        }
        if (plugin.instance && !plugin.instance->CanTerminate()) {
            return false; // Pending
        }

        if (plugin.instance) {
            plugin.instance->Terminate();
        }
        plugin.initialized = false;
        plugin.terminating = false;
        plugin.instance = nullptr;
        if (plugin.dll) {
            ASSERT(FreeLibrary(plugin.dll));
        }
        plugin.dll = nullptr;
        std::erase_if(plugins_loaded, [plugin_ptr](auto p) { return p == plugin_ptr; });
        return true;
    }

    bool LoadPlugin(PluginModule::Plugin* plugin_ptr)
    {
        auto& plugin = *plugin_ptr;
        if (plugin.instance) {
            return true;
        }
        if (!plugin.dll) {
            plugin.dll = LoadLibraryW(plugin.path.wstring().c_str());
        }
        if (!plugin.dll) {
            UnloadPlugin(plugin_ptr);
            Log::Error("Failed to load plugin %s (LoadLibraryW)", plugin.path.filename().string().c_str());
            return false;
        }
        using ToolboxPluginInstanceFn = ToolboxPlugin* (*)();
        const auto instance_fn = reinterpret_cast<ToolboxPluginInstanceFn>(GetProcAddress(plugin.dll, "ToolboxPluginInstance"));
        if (!instance_fn) {
            UnloadPlugin(plugin_ptr);
            Log::Error("Failed to load plugin %s (ToolboxPluginInstance)", plugin.path.filename().string().c_str());
            return false;
        }

        plugin.instance = instance_fn();
        plugins_loaded.push_back(plugin_ptr);
        return true;
    }

    bool InitializePlugin(PluginModule::Plugin* plugin_ptr)
    {
        auto& plugin = *plugin_ptr;
        if (plugin.terminating || !plugin.instance) {
            return false;
        }
        if (plugin.initialized) {
            return true;
        }
        const auto context = ImGui::GetCurrentContext();
        if (!context) {
            return false;
        }
        ImGuiAllocFns fns;
        ImGui::GetAllocatorFunctions(&fns.alloc_func, &fns.free_func, &fns.user_data);
        plugin.instance->Initialize(context, fns, GWToolbox::GetDLLModule());
        plugin.instance->LoadSettings(pluginsfoldername.c_str());
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
                auto found = std::ranges::find_if(plugins_available, [file_path](const auto plugin) {
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

    for (const auto plugin : plugins_available) {
        ImGui::PushID(plugin);
        auto& style = ImGui::GetStyle();
        const auto origin_header_col = style.Colors[ImGuiCol_Header];
        style.Colors[ImGuiCol_Header] = {0, 0, 0, 0};

        static char buf[128];
        const auto has_settings = plugin->initialized && plugin->instance && plugin->instance->HasSettings();
        if (has_settings) {
            sprintf(buf, "      %s", plugin->path.filename().string().c_str());
        }
        else {
            sprintf(buf, "             %s", plugin->path.filename().string().c_str());
        }
        const auto pos = ImGui::GetCursorScreenPos();
        const bool is_showing = has_settings ? ImGui::CollapsingHeader(buf, ImGuiTreeNodeFlags_AllowItemOverlap) : ImGui::CollapsingHeader(buf, ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_AllowItemOverlap);

        if (const auto icon = plugin->initialized ? plugin->instance->Icon() : nullptr) {
            const float text_offset_x = ImGui::GetTextLineHeightWithSpacing() + 4.0f; // TODO: find a proper number
            ImGui::GetWindowDrawList()->AddText(
                ImVec2(pos.x + text_offset_x, pos.y + style.ItemSpacing.y / 2),
                ImColor(style.Colors[ImGuiCol_Text]), icon);
        }

        style.Colors[ImGuiCol_Header] = origin_header_col;

        ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::GetTextLineHeight() - ImGui::GetStyle().FramePadding.x - 128.f);
        snprintf(buf, _countof(buf), "%s###load_unload", plugin->instance ? "Unload" : "Load");
        if (ImGui::Button(buf)) {
            if (!plugin->instance) {
                LoadPlugin(plugin);
            }
            else {
                UnloadPlugin(plugin);
            }
        }
        if (plugin->instance && plugin->instance->GetVisiblePtr()) {
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::GetTextLineHeight() - ImGui::GetStyle().FramePadding.x);
            ImGui::Checkbox("##check", plugin->instance->GetVisiblePtr());
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Visible");
            }
        }

        if (is_showing && InitializePlugin(plugin) && has_settings) {
            plugin->instance->DrawSettings();
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
    return std::ranges::all_of(plugins_loaded, [](const auto plugin) {
        return !plugin->instance || plugin->instance->CanTerminate();
    });
}

bool PluginModule::WndProc(const UINT msg, const WPARAM wParam, const LPARAM lParam)
{
    bool capture = false;
    for (const auto plugin : plugins_loaded) {
        if (!plugin->instance) {
            continue;
        }
        capture |= plugin->instance->WndProc(msg, wParam, lParam);
    }
    return capture;
}

std::vector<ToolboxPlugin*> PluginModule::GetPlugins()
{
    std::vector<ToolboxPlugin*> plugins;
    for (const auto plugin : plugins_loaded) {
        plugins.push_back(plugin->instance);
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
    static bool message_displayed = false;
    if (!plugins_loaded.empty() && !message_displayed) {
        WriteChat(GW::Chat::Channel::CHANNEL_GWCA2,
                  L"<c=#FF0000>Plugins detected, these may be unsafe to use and are not officially supported by GWToolbox++ developers.\n"
                  "Use at your own risk if you trust the author.\n"
                  "Do not report bugs that occur while you play with plugins.</c>",
                  L"GWToolbox++");
        message_displayed = true;
    }
    for (const auto plugin : plugins_loaded) {
        if (!InitializePlugin(plugin)) {
            continue;
        }
        if (GW::UI::GetIsWorldMapShowing() && !plugin->instance->ShowOnWorldMap()) {
            continue;
        }

        if (plugin->instance->GetVisiblePtr() && *plugin->instance->GetVisiblePtr()) {
            plugin->instance->Draw(device);
        }
    }
}

void PluginModule::LoadSettings(ToolboxIni* ini)
{
    std::list<CSimpleIniA::Entry> dlls_to_load;
    std::vector<Plugin*> plugins_loaded_from_ini;
    if (ini->GetAllKeys(plugins_enabled_section, dlls_to_load)) {
        for (const auto& entry : dlls_to_load) {
            std::filesystem::path path = entry.pItem;
            const auto filename = path.filename();
            auto matching_plugins = std::views::filter(plugins_available, [filename](auto plugin) {
                return plugin->path.filename() == filename;
            });
            // Find any matching plugins and load them
            for (auto plugin : matching_plugins) {
                if (!LoadPlugin(plugin)) {
                    continue;
                }
                InitializePlugin(plugin);
                plugins_loaded_from_ini.push_back(plugin);
            }
        }
    }
    // Find any plugins that are currently loaded but not supposed to be
    auto to_unload = std::views::filter(plugins_loaded, [&](auto plugin) {
        return std::ranges::find(plugins_loaded_from_ini, plugin) == plugins_loaded_from_ini.end();
    });
    for (const auto plugin : std::views::reverse(to_unload)) {
        UnloadPlugin(plugin);
    }
}

void PluginModule::SaveSettings(ToolboxIni* ini)
{
    ini->Delete(plugins_enabled_section, nullptr);
    for (const auto plugin : plugins_loaded) {
        plugin->instance->SaveSettings(pluginsfoldername.c_str());
        ini->SetBoolValue(plugins_enabled_section, plugin->path.filename().string().c_str(), true);
    }
}

void PluginModule::Update(const float delta)
{
    for (const auto plugin : plugins_loaded) {
        plugin->instance->Update(delta);
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
    const auto plugins_cpy = plugins_loaded;
    for (const auto p : std::views::reverse(plugins_cpy)) {
        UnloadPlugin(p);
    }
}

void PluginModule::Terminate()
{
    ToolboxUIElement::Terminate();
    SignalTerminate();
    ASSERT(plugins_loaded.empty());
    for (const auto p : plugins_available) {
        delete p;
    }
}
