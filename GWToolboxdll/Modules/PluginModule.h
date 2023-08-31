#pragma once

#include <ToolboxUIElement.h>
#include <../plugins/Base/ToolboxPlugin.h>

class PluginModule final : public ToolboxUIElement {
    PluginModule() = default;
    ~PluginModule() override = default;

public:
    struct Plugin {
        Plugin(std::filesystem::path _path)
            : path(std::move(_path)) { }

        std::filesystem::path path;
        HMODULE dll = nullptr;
        ToolboxPlugin* instance = nullptr;
        bool initialized = false;
        bool terminating = false;
        bool visible = false;
    };

    static PluginModule& Instance()
    {
        static PluginModule instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Plugins"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_PUZZLE_PIECE; }

    [[nodiscard]] bool ShowOnWorldMap() const override { return true; }

    void Draw(IDirect3DDevice9*) override;
    void DrawSettingsInternal() override;
    void LoadSettings(ToolboxIni*) override;
    void SaveSettings(ToolboxIni*) override;
    void Update(float) override;
    void Initialize() override;
    void SignalTerminate() override;
    void Terminate() override;
    bool CanTerminate() override;
    bool WndProc(UINT, WPARAM, LPARAM) override;

    static std::vector<ToolboxPlugin*> GetPlugins();

    void ShowVisibleRadio() override { }
    void DrawSizeAndPositionSettings() override { }
};
