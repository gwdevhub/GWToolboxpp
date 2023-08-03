#pragma once

#include <ToolboxUIElement.h>

class PluginModule final : public ToolboxUIElement {
    PluginModule();
    ~PluginModule() override = default;

public:
    static PluginModule& Instance()
    {
        static PluginModule instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Plugins"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_PUZZLE_PIECE; }

    void Initialize() override;

    void Draw(IDirect3DDevice9*) override;
    void DrawSettingsInternal() override;
    void LoadSettings(ToolboxIni*) override;
    void SaveSettings(ToolboxIni*) override;
    void Update(float) override;
    void SignalTerminate() override;
    void Terminate() override;
    bool CanTerminate() override;

    void ShowVisibleRadio() override
    {
    }

    void DrawSizeAndPositionSettings() override
    {
    }

private:
};
