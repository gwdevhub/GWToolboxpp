#pragma once

#include <ToolboxWidget.h>

class WorldMapWidget : public ToolboxWidget {
    WorldMapWidget() = default;
    ~WorldMapWidget() = default;

public:
    static WorldMapWidget& Instance() {
        static WorldMapWidget w;
        return w;
    }

    void Initialize() override;

    virtual void RegisterSettingsContent() override {};

    bool ShowOnWorldMap() const { return true; }

    const char* Name() const override { return "World Map"; }

    const char* Icon() const override { return ICON_FA_GLOBE;  }

    void Draw(IDirect3DDevice9* pDevice) override;

    void ShowAllOutposts(bool show);

    void DrawSettingInternal() override;

    bool WndProc(UINT, WPARAM, LPARAM);

private:
    void InitializeMapsUnlockedArrays();
};
