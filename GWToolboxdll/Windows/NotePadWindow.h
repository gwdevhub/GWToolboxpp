#pragma once

#include "ToolboxWindow.h"

#include <Utils/FontLoader.h>

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

    struct Settings {
        float font_size = static_cast<float>(FontLoader::FontSize::text);
    };

    struct TabSettings {
        int id = 0;
        std::string name;
    };

    // Draw user interface. Will be called every frame if the element is visible
    void Draw(IDirect3DDevice9* pDevice) override;

    void Initialize() override;
    void Terminate() override;
    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void SaveSettings(SettingsDoc& doc) override;
    void DrawSettingsInternal() override;
};
