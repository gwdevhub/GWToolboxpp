#pragma once

#include <ToolboxModule.h>
#include <GWCA/GameContainers/GamePos.h>

// Debug-only cartography helper: every second, finds the nearest still-foggy cell of the
// account's world-map exploration bitmap (or the nearest user-placed fog point) and places
// the custom quest marker on the closest walkable spot toward it — the regular quest path
// guides the player there. The user can decline suggestions (once or forever) and add their
// own fog points via the world map / mission map context menus.
class CartographerModule : public ToolboxModule {
    CartographerModule() = default;
    ~CartographerModule() override = default;

public:
    static CartographerModule& Instance()
    {
        static CartographerModule instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "地图绘制"; }
    [[nodiscard]] const char* Description() const override
    {
        return "解锁世界地图上最近的未探索（迷雾）区域，并绘制前往该区域最近的路线。";
    }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_MAP_MARKED_ALT; }

    void Initialize() override;
    void SignalTerminate() override;
    void Update(float) override;
    void Draw(IDirect3DDevice9*) override;
    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void DrawSettingsInternal() override;

    static void SetEnabled(bool on);
    static void SkipCurrentTarget(bool forever);
    static void AddCustomPoint(const GW::Vec2f& world_map_pos);
    static void ClearCustomPoints();
    static void ClearDeclined();
    static void GetStatus(char* buf, size_t len);
};
