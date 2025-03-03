#pragma once

#include "ToolboxWindow.h"

class NotePadWindow : public ToolboxWindow {
    NotePadWindow() = default;
    ~NotePadWindow() override = default;

public:
    static NotePadWindow& Instance()
    {
        static NotePadWindow instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Notepad"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_CLIPBOARD; }

    // Draw user interface. Will be called every frame if the element is visible
    void Draw(IDirect3DDevice9* pDevice) override;

    void Initialize() override;
    void Terminate() override;
    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
    void DrawSettingsInternal() override;
};
