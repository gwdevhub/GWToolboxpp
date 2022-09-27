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

    void Draw();
    void RefreshDlls();
    void UnloadDlls();

    [[nodiscard]] std::vector<ToolboxPlugin*> GetModules() const
    {
        std::vector<ToolboxPlugin*> instances{};
        instances.resize(plugins.size());
        std::ranges::transform(plugins, instances.begin(), [](Plugin p) {
            return p.instance;
        });
        return instances;
    }

private:
    ToolboxPlugin* LoadDLL(const std::filesystem::path& path);

    std::vector<Plugin> plugins{};
};

