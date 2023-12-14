#pragma once

#include <ToolboxWidget.h>

class WorldMapWidget : public ToolboxWidget {
    WorldMapWidget() = default;
    ~WorldMapWidget() override = default;

public:
    static WorldMapWidget& Instance()
    {
        static WorldMapWidget w;
        return w;
    }

    void Initialize() override;

    void Terminate() override;

    void RegisterSettingsContent() override { };

    [[nodiscard]] bool ShowOnWorldMap() const override { return true; }

    [[nodiscard]] const char* Name() const override { return "World Map"; }

    [[nodiscard]] const char* Icon() const override { return ICON_FA_GLOBE; }

    void Draw(IDirect3DDevice9* pDevice) override;

    static void ShowAllOutposts(bool show);

    void DrawSettingsInternal() override;

    bool WndProc(UINT, WPARAM, LPARAM) override;
};
