#pragma once

#include <ToolboxModule.h>
#include <GWCA/GameContainers/GamePos.h>

// Debug-only cartography helper, built on how the client grids the world map: exploration is
// tracked per 32x32 world-map-unit square in the account's exploration bitmap, and standing
// inside a square is believed to credit that square plus the ring of squares around it
// (Chebyshev distance, widened by a Bonus Explorer's Cape).
//
// So rather than routing at the fog itself, the module works out which squares the player can
// stand in on this map, how much still-foggy ground each of them would credit, and puts the
// custom quest marker in the most worthwhile one — the regular quest path guides the player
// there. All UI lives on the maps themselves: the squares worth visiting, the suggested
// square, queued fog points and a status line are drawn on the world map and mission map, and
// the right-click context menu of either map manages the helper (toggle, add/remove fog
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
        return "Works out which 32x32 world-map squares you need to stand in to uncover the fog around you, draws them on the maps and routes you to the best one.";
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
