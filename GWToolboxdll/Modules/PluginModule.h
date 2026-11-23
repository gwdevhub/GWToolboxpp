#pragma once

#include <ToolboxUIElement.h>
#include <PluginApi.h>

class PluginModule final : public ToolboxUIElement {
    PluginModule() = default;
    ~PluginModule() override = default;

public:
    struct Plugin {
        Plugin(std::filesystem::path _path)
            : path(std::move(_path)) { }

        std::filesystem::path path;
        HMODULE dll = nullptr;
        const GWToolboxPluginApi* api = nullptr;
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
    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void SaveSettings(SettingsDoc& doc) override;
    void Update(float) override;
    void Initialize() override;
    void SignalTerminate() override;
    void Terminate() override;
    bool CanTerminate() override;
    bool WndProc(UINT, WPARAM, LPARAM) override;

    static std::vector<const GWToolboxPluginApi*> GetPlugins();

    void ShowVisibleRadio() override { }
    void DrawSizeAndPositionSettings() override { }
};
