#pragma once

#include <GWCA/Utilities/Hook.h>

#include <ToolboxWindow.h>

class MaterialsWindow : public ToolboxWindow {
    MaterialsWindow() = default;
    ~MaterialsWindow() override = default;

public:
    static MaterialsWindow& Instance()
    {
        static MaterialsWindow instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Materials"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_FEATHER_ALT; }

    void Initialize() override;
    void Terminate() override;

    void DrawSettingsInternal() override;
    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;

    bool GetIsInProgress() const;

    // Update. Will always be called every frame.
    void Update(float delta) override;

    // Draw user interface. Will be called every frame if the element is visible
    void Draw(IDirect3DDevice9* pDevice) override;


};
