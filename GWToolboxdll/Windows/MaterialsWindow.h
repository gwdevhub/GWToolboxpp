#pragma once

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

    struct Settings {
        bool manage_gold = false;
        bool use_stock = false;
    };

    void Initialize() override;
    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void SaveSettings(SettingsDoc& doc) override;
    void Terminate() override;

    void DrawSettingsInternal() override;

    bool GetIsInProgress() const;

    // Update. Will always be called every frame.
    void Update(float delta) override;

    // Draw user interface. Will be called every frame if the element is visible
    void Draw(IDirect3DDevice9* pDevice) override;


};
