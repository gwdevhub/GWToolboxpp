#include "stdafx.h"

#include <string>
#include <filesystem>
#include "PluginManager.h"
#include <Modules/Resources.h>

void PluginManager::RefreshDlls() {
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
                if (!SUCCEEDED(Resources::ResolveShortcut(file_path, file_path)))
                    continue;
                ext = file_path.extension();
            }
            if (ext == ".dll") {
                if (!LoadDLL(file_path)) {
                    Log::Error("Failed to load plugin %s", file_path.filename().string().c_str());
                }
            }
        }
    }
}

void PluginManager::Draw() {
    if (ImGui::CollapsingHeader("Plugins")) {
        ImGui::PushID("Plugins");

        for (auto& plugin : plugins) {
            ImGui::Text("%s", plugin.path.string().c_str());
        }

        ImGui::Separator();
        if (ImGui::Button("Refresh")) {
            RefreshDlls();
        }

        ImGui::PopID();
    }
}

TBModule* PluginManager::LoadDLL(const std::filesystem::path& path) {
    for (auto& plugin : plugins) {
        if (plugin.path == path)
            return plugin.instance;
    }
    HINSTANCE dll = LoadLibraryW(path.wstring().c_str());
    typedef TBModule* (__cdecl* ObjProc)();
    ObjProc objfunc = dll ? (ObjProc)GetProcAddress(dll, "TBModuleInstance") : nullptr;
    if (!objfunc)
        return nullptr;
    Plugin p;
    p.instance = objfunc();
    p.path = path;
    plugins.push_back(p);
    return p.instance;
}

