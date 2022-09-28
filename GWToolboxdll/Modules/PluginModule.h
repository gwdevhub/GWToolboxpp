#pragma once

#include <../plugins/Base/ToolboxPlugin.h>
#include <ToolboxUIElement.h>

class PluginModule final : public ToolboxUIElement {
    struct Plugin {
        std::filesystem::path path;
        HMODULE dll = nullptr;
        ToolboxPlugin* instance = nullptr;
        bool initialized = false;
    };

    PluginModule();
    ~PluginModule() = default;

public:
    static PluginModule& Instance()
    {
        static PluginModule instance;
        return instance;
    }

    const char* Name() const override { return "Plugins"; }
    const char8_t* Icon() const override { return ICON_FA_PUZZLE_PIECE; }

    void Initialize() override;

    void Draw(IDirect3DDevice9*) override;
    void DrawSettingInternal() override;
    void LoadSettings(CSimpleIniA*) override;
    void SaveSettings(CSimpleIniA*) override;
    void Update(float) override;
    void SignalTerminate() override;
    void Terminate() override;
    bool CanTerminate() override;

    void ShowVisibleRadio() override {}
    void DrawSizeAndPositionSettings() override {}

    auto GetInitializedPlugins()
    {
        return std::views::filter(plugins, [](const auto& plugin) {
            return plugin.initialized;
        });
    }

private:
    void RefreshDlls();
    bool UnloadDlls();
    ToolboxPlugin* LoadDll(const std::filesystem::path& path);

    std::vector<Plugin> plugins{};
    wchar_t pluginsfoldername[MAX_PATH]{};
};
