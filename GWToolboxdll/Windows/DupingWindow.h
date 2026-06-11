#pragma once

#include <ToolboxWindow.h>

class DupingWindow : public ToolboxWindow {
    DupingWindow() = default;
    ~DupingWindow() override = default;

public:
    static DupingWindow& Instance()
    {
        static DupingWindow instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Duping"; }
    [[nodiscard]] const char* Description() const override { return "Keeps track of soul/water/mind tormentor counts in Ravenheart Gloom"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_COPY; }

    struct Settings {
        float range = 1600.0f;
        bool hide_when_nothing = true;
        bool show_souls_counter = true;
        bool show_waters_counter = true;
        bool show_minds_counter = true;
        float souls_threshhold = 0.6f;
        float waters_threshhold = 0.5f;
        float minds_threshhold = 0.0f;
    };

    void Initialize() override;
    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void SaveSettings(SettingsDoc& doc) override;
    void Terminate() override;

    // Update. Will always be called every frame.
    void Update(float) override { }

    // Draw user interface. Will be called every frame if the element is visible
    void Draw(IDirect3DDevice9* pDevice) override;

    void DrawSettingsInternal() override;
};
