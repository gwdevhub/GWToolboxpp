#include "stdafx.h"

#include "PluginModule.h"

#include <GWToolbox.h>
#include <GWCA/Managers/ChatMgr.h>

#include <Modules/Resources.h>
#include <filesystem>
#include <string>

namespace {
    wchar_t pluginsfoldername[MAX_PATH]{};

    const char* plugins_enabled_section = "Plugins Enabled";

    struct Plugin {
        Plugin(std::filesystem::path _path) : path(_path) {};
        std::filesystem::path path;
        HMODULE dll = nullptr;
        ToolboxPlugin* instance = nullptr;
        bool initialized = false;
        bool terminating = false;
        bool active = false;
    };

    std::vector<Plugin*> plugins;

    std::vector<Plugin*> loaded_plugins;

    bool UnloadPlugin(Plugin* plugin_ptr) {
        auto& plugin = *plugin_ptr;
        if (!plugin.terminating) {
            if(plugin.instance)
                plugin.instance->SignalTerminate();
            plugin.terminating = true;
        }
        if (plugin.instance && !plugin.instance->CanTerminate())
            return false; // Pending
        
        if (plugin.instance) 
            plugin.instance->Terminate();
        plugin.initialized = false;
        plugin.terminating = false;
        plugin.instance = nullptr;
        if(plugin.dll)
            ASSERT(FreeLibrary(plugin.dll));
        plugin.dll = nullptr;
        std::erase_if(loaded_plugins, [plugin_ptr](auto p) { return p == plugin_ptr; });
        return true; 
    }
    bool LoadPlugin(Plugin* plugin_ptr) {
        auto& plugin = *plugin_ptr;
        if (plugin.instance)
            return true;
        if (!plugin.dll)
            plugin.dll = LoadLibraryW(plugin.path.wstring().c_str());
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
        loaded_plugins.push_back(plugin_ptr);
        return true;
    }
    bool InitializePlugin(Plugin* plugin_ptr) {
        auto& plugin = *plugin_ptr;
        if (plugin.terminating || !plugin.instance)
            return false;
        if (plugin.initialized)
            return true;
        const auto context = ImGui::GetCurrentContext();
        if (!context)
            return false;
        ImGuiAllocFns fns;
        ImGui::GetAllocatorFunctions(&fns.alloc_func, &fns.free_func, &fns.user_data);
        plugin.instance->Initialize(context, fns, GWToolbox::Instance().GetDLLModule());
        plugin.instance->LoadSettings(pluginsfoldername);
        plugin.initialized = true;
        return true;
    }

    void RefreshDlls()
    {
        // when we refresh, how do we map the modules that were already loaded to the ones on disk?
        // the dll file may have changed
        namespace fs = std::filesystem;

        const fs::path plugin_folder = pluginsfoldername;

        if (!Resources::EnsureFolderExists(plugin_folder))
            return;

        for (auto& p : fs::directory_iterator(plugin_folder)) {
            fs::path file_path = p.path();
            fs::path ext = file_path.extension();
            if (ext == ".lnk") {
                if (!SUCCEEDED(Resources::ResolveShortcut(file_path, file_path)))
                    continue;
                ext = file_path.extension();
            }
            if (ext == ".dll") {
                auto found = std::ranges::find_if(plugins, [file_path](const auto plugin) {
                    return plugin->path == file_path;
                    });
                if(found == plugins.end())
                    plugins.push_back(new Plugin(file_path));
            }
        }
    }
    
}

PluginModule::PluginModule()
{
    const auto wpath = Resources::GetPath(L"plugins");
    wcscpy_s(pluginsfoldername, wpath.c_str());
}



void PluginModule::DrawSettingInternal()
{
    ImGui::PushID("Plugins");

    for (auto plugin : plugins) {
        ImGui::PushID(plugin);
        auto& style = ImGui::GetStyle();
        const auto origin_header_col = style.Colors[ImGuiCol_Header];
        style.Colors[ImGuiCol_Header] = {0, 0, 0, 0};

        static char buf[128];
        sprintf(buf, "      %s", plugin->path.filename().string().c_str());
        const auto pos = ImGui::GetCursorScreenPos();
        const bool is_showing = ImGui::CollapsingHeader(buf, ImGuiTreeNodeFlags_AllowItemOverlap);

        const auto icon = plugin->initialized ? plugin->instance->Icon() : nullptr;
        if (icon) {
            const float text_offset_x = ImGui::GetTextLineHeightWithSpacing() + 4.0f; // TODO: find a proper number
            ImGui::GetWindowDrawList()->AddText(
                ImVec2(pos.x + text_offset_x, pos.y + style.ItemSpacing.y / 2),
                ImColor(style.Colors[ImGuiCol_Text]), icon);
        }

        style.Colors[ImGuiCol_Header] = origin_header_col;

        ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::GetTextLineHeight() - ImGui::GetStyle().FramePadding.x - 128.f);
        snprintf(buf, _countof(buf), "%s###load_unload", plugin->instance ? "Unload" : "Load");
        if (ImGui::Button(buf)) {
            if(!plugin->instance)
                LoadPlugin(plugin);
            else {
                UnloadPlugin(plugin);  
            }
        }
        if (plugin->instance) {
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::GetTextLineHeight() - ImGui::GetStyle().FramePadding.x);
            ImGui::Checkbox("##check", &plugin->active);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Active");
        }


        if (is_showing && plugin->active && InitializePlugin(plugin)) {
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
    return std::ranges::all_of(loaded_plugins, [](const auto plugin) {
        return !plugin->instance || plugin->instance->CanTerminate();
    });
}

void PluginModule::Initialize()
{
    ToolboxUIElement::Initialize();
    RefreshDlls();
}

void PluginModule::Draw(IDirect3DDevice9* device)
{
    static bool message_displayed = false;
    if (!loaded_plugins.empty() && !message_displayed) {
        GW::Chat::WriteChat(GW::Chat::Channel::CHANNEL_GWCA2,
            L"<c=#FF0000>Plugins detected, these may be unsafe to use and are not officially supported by GWToolbox++ developers.\n"
            "Use at your own risk if you trust the author.\n"
            "Do not report bugs that occur while you play with plugins.</c>",
            L"GWToolbox++");
        message_displayed = true;
    }
    for (auto plugin : loaded_plugins) {
        if (!InitializePlugin(plugin))
            continue;
        if (!plugin->active)
            continue;

        plugin->instance->Draw(device);
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
            bool is_active = ini->GetBoolValue(plugins_enabled_section, entry.pItem, false);
            auto matching_plugins = std::views::filter(plugins, [filename](auto plugin) {
                return plugin->path.filename() == filename;
                });
            // Find any matching plugins and load them
            for (auto plugin : matching_plugins) {
                if (!LoadPlugin(plugin))
                    continue;
                plugin->active = is_active;
                InitializePlugin(plugin);
                plugins_loaded_from_ini.push_back(plugin);
            }
        }
    }
    // Find any plugins that are currently loaded but not supposed to be
    auto to_unload = std::views::filter(loaded_plugins, [&](auto plugin) {
        return std::ranges::find(plugins_loaded_from_ini, plugin) == plugins_loaded_from_ini.end();
        });
    for (const auto plugin : std::views::reverse(to_unload)) {
        UnloadPlugin(plugin);
    }
}

void PluginModule::SaveSettings(ToolboxIni* ini)
{
    ini->Delete(plugins_enabled_section,NULL);
    for (const auto plugin : loaded_plugins) {
        plugin->instance->SaveSettings(pluginsfoldername);
        ini->SetBoolValue(plugins_enabled_section, plugin->path.filename().string().c_str(), plugin->active);
    }
}

void PluginModule::Update(float delta)
{
    for (const auto plugin : loaded_plugins) {
        plugin->instance->Update(delta);
        if (plugin->terminating) {
            if (UnloadPlugin(plugin))
                break; // loaded_plugins vector changed, skip a frame
        } 
    }
}

void PluginModule::SignalTerminate()
{
    ToolboxUIElement::SignalTerminate();
    auto plugins_cpy = loaded_plugins;
    for (auto p : std::views::reverse(plugins_cpy)) {
        UnloadPlugin(p);
    }
}

void PluginModule::Terminate() { 
    ToolboxUIElement::Terminate();
    SignalTerminate();
    ASSERT(loaded_plugins.empty());
    for (auto p : plugins)
        delete p;
}
