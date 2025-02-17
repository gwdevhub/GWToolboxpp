#pragma once

#include <ToolboxWidget.h>

namespace GW {
    struct GamePos;
    struct Vec2f;
    namespace Constants {
        enum class MapID : uint32_t;
    }
}

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

    void SignalTerminate() override;

    void RegisterSettingsContent() override { };

    [[nodiscard]] bool ShowOnWorldMap() const override { return true; }
    [[nodiscard]] const char* Name() const override { return "World Map"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_GLOBE; }

    void LoadSettings(ToolboxIni*) override;
    void SaveSettings(ToolboxIni*) override;
    void Draw(IDirect3DDevice9* pDevice) override;
    void DrawSettingsInternal() override;
    bool WndProc(UINT, WPARAM, LPARAM) override;

    static void ShowAllOutposts(bool show);
    static GW::Constants::MapID GetMapIdForLocation(const GW::Vec2f& world_map_pos, GW::Constants::MapID exclude_map_id = (GW::Constants::MapID)0);
    static bool WorldMapToGamePos(const GW::Vec2f& world_map_pos, GW::GamePos& game_map_pos);
    static bool GamePosToWorldMap(const GW::GamePos& game_map_pos, GW::Vec2f& world_map_pos);
};
