#pragma once

#include "../modules/Base/module_base.h"

class PluginManager {
    struct Plugin {
        std::filesystem::path path;
        TBModule* instance = nullptr;
    };

public:
    PluginManager() = default;
    PluginManager(const PluginManager&) = delete;
    ~PluginManager() = default;

    void Draw();
    void RefreshDlls();
private:
    TBModule* LoadDLL(const std::filesystem::path& path);

    std::vector<Plugin> plugins{};
};

