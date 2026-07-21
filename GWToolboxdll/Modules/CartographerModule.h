#pragma once

#include <ToolboxModule.h>
#include <GWCA/GameContainers/GamePos.h>

// Debug-only cartography helper: every second, finds the nearest still-foggy cell of the
// account's world-map exploration bitmap (or the nearest user-placed fog point) and places
// the custom quest marker on the closest walkable spot toward it — the regular quest path
// guides the player there. All UI lives on the maps themselves: the suggested cell, queued
// fog points and a status line are drawn on the world map and mission map, and the
// right-click context menu of either map manages the helper (toggle, add/remove fog
// points, skip suggestions once or forever).
class CartographerModule : public ToolboxModule {
    CartographerModule() = default;
    ~CartographerModule() override = default;

public:
    static CartographerModule& Instance()
    {
        static CartographerModule instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Cartographer Helper"; }
    [[nodiscard]] const char* Description() const override
    {
        return "Suggests the closest unexplored (foggy) part of the world map and routes you to the nearest walkable point toward it.";
    }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_MAP_MARKED_ALT; }

    void Initialize() override;
    void SignalTerminate() override;
    void Update(float) override;
    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void DrawSettingsInternal() override;

    static void SetEnabled(bool on);
    static bool GetEnabled();
    // The user explicitly placed/removed the quest marker via toolbox UI — yield instantly.
    static void OnUserMarkerAction();
    // World-map position of the current suggestion (fog cell or custom point); false if none.
    static bool GetCurrentTargetWorldPos(GW::Vec2f& out);
    static void SkipCurrentTarget(bool forever);
    static void AddCustomPoint(const GW::Vec2f& world_map_pos);
    static void RemoveCustomPointNear(const GW::Vec2f& world_map_pos, float max_dist_wm);
    static void ClearCustomPoints();
    static void ClearDeclined();
    static void GetStatus(char* buf, size_t len);
};
