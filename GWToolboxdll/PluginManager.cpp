#include "stdafx.h"

#include "PluginManager.h"
#include <Modules/Resources.h>
#include <filesystem>
#include <string>

void PluginManager::RefreshDlls()
{
    // when we refresh, how do we map the modules that were already loaded to the ones on disk?
    // the dll file may have changed
    namespace fs = std::filesystem;

    const fs::path settings_folder = Resources::GetSettingsFolderPath();
    const fs::path module_folder = settings_folder / "plugins";

    if (Resources::EnsureFolderExists(module_folder)) {
        for (auto& p : fs::directory_iterator(module_folder)) {
            fs::path file_path = p.path();
            fs::path ext = file_path.extension();
            if (ext == ".lnk") {
                if (!SUCCEEDED(Resources::ResolveShortcut(file_path, file_path))) continue;
                ext = file_path.extension();
            }
            if (ext == ".dll") {
                if (!LoadDll(file_path)) {
                    Log::Error("Failed to load plugin %s", file_path.filename().string().c_str());
                }
            }
        }
    }
}

void PluginManager::DrawSettingsInternal()
{
    if (ImGui::CollapsingHeader("Plugins")) {
        ImGui::PushID("Plugins");

        for (auto& plugin : plugins) {
            auto& style = ImGui::GetStyle();
            const auto origin_header_col = style.Colors[ImGuiCol_Header];
            style.Colors[ImGuiCol_Header] = {0, 0, 0, 0};
            if (ImGui::CollapsingHeader(plugin.path.filename().string().c_str())) {
                plugin.instance->DrawSettings();
            }
            style.Colors[ImGuiCol_Header] = origin_header_col;
            ImGui::Separator();
        }

        ImGui::Separator();
        if (ImGui::Button("Refresh")) {
            RefreshDlls();
        }

        ImGui::PopID();
    }
}

ToolboxPlugin* PluginManager::LoadDll(const std::filesystem::path& path)
{
    for (auto& plugin : plugins) {
        if (plugin.path == path)
            return plugin.instance;
    }
    const auto dll = LoadLibraryW(path.wstring().c_str());
    if (dll == nullptr) return nullptr;
    using ToolboxPluginInstanceFn = ToolboxPlugin* (*)();
    const auto instance_fn = reinterpret_cast<ToolboxPluginInstanceFn>(GetProcAddress(dll, "ToolboxPluginInstance"));
    if (!instance_fn) return nullptr;
    Plugin p;
    p.instance = instance_fn();
    p.dll = dll;
    p.path = path;
    plugins.push_back(p);
    plugin_instances.push_back(p.instance);
    return p.instance;
}

void PluginManager::UnloadDlls()
{
    for (const auto plugin : plugins) {
        plugin.instance->Terminate();
        const auto success = FreeLibrary(plugin.dll);
        if (!success) {
            std::cout << "Failed to unload plugin " << plugin.path;
        }
    }
    plugins.clear();
    plugin_instances.clear();
}