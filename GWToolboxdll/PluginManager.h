#pragma once

#include "../modules/Base/module_base.h"

class PluginManager {
    struct Plugin {
        std::filesystem::path path;
        HMODULE dll = nullptr;
        ToolboxPlugin* instance = nullptr;
    };

public:
    PluginManager() = default;
    PluginManager(const PluginManager&) = delete;
    ~PluginManager() = default;

    void DrawSettingsInternal();
    void RefreshDlls();
    void UnloadDlls();

    [[nodiscard]] const std::vector<ToolboxPlugin*>& GetPlugins() const
    {
        return plugin_instances;
    }

private:
    ToolboxPlugin* LoadDll(const std::filesystem::path& path);

    std::vector<Plugin> plugins{};
    std::vector<ToolboxPlugin*> plugin_instances{};
};

