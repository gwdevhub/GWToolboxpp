#pragma once

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <unordered_set>
#include <utility>
#include <vector>

#include <GWCA/Constants/Constants.h>
#include <GWCA/GameContainers/GamePos.h>

#include <Logger.h>

#include <Widgets/CartographyData.h>
#include <Widgets/WorldMapWidget.h>
#include <Windows/Pathfinding/Pathing.h>

// Cartographer internals shared with its debug-only tools (CartographerBake.cpp,
// CartographerProbeLog.cpp). Not an API - the widget owns all of this.
#ifdef _DEBUG
#define CARTO_LOG(...) Log::Log(__VA_ARGS__)
#else
#define CARTO_LOG(...) ((void)0)
#endif

namespace Carto {
    // Gw.exe's fog mesh builder is handed WorldContext::cartographed_areas (+0x5A4) and h05B4
    // (+0x5B4, the grid dims): one bit per 32x32-world-map-unit cell.
    constexpr float kWorldMapUnitsPerCell = 32.f;
    // Standing in a tile credits it plus the ring around it; a Bird's Eye Compass widens that to
    // three rings. Chebyshev throughout, so where inside the tile you stand makes no difference.
    constexpr int kRevealRadius = 1;
    constexpr int kRevealRadiusBec = 3;
    // The bake dilates by one tile regardless of the compass: its standable set is also its navmesh
    // model, so a tile the wide rings reach is already a tile something stands next to.
    constexpr int kMaskRadius = 1;
    // Credit stops one square past the standing map's rectangle, not wherever the ring reaches:
    // measured on Shenzun Tunnels ground a row past its south edge, which credits that row, not the next.
    constexpr int kBoundarySlack = 1;
    // The client's fog texture is this many texels per cell, so visible fog is four times finer
    // than the 32-unit grid the bits live on.
    constexpr int kFogSubdivisions = 4;

    // wm.y is a divide plus an add on top of mid.y, itself a divide plus an add, so it carries a
    // few ulp; 1/64 is a power of two with plenty of headroom and 1/2048 of a tile.
    constexpr float kCellEps = 1.f / 64.f;

    extern bool using_bec;

    inline int RevealRadius() { return using_bec ? kRevealRadiusBec : kRevealRadius; }

    // The fog mesh builder strides rows by (width >> 5) words while the explored-query indexes
    // bits flat as cy * width + cx; width is always a multiple of 32, so either form works.
    inline uint32_t RowWords(const uint32_t width) { return width >> 5; }

    // The tile the client credits from a standing position. Columns own [32c, 32c+32) while rows own
    // (32r, 32r+32]: the grid is anchored in game space and GamePosToWorldMap flips y, which turns a
    // half-open game-space interval into one closed at the other end. Epsilon leans off each axis's
    // closed end so a round-tripped value a few ulp past a boundary still reads as the cell that owns it.
    inline int CreditCellX(const float x) { return static_cast<int>(floorf((x + kCellEps) / kWorldMapUnitsPerCell)); }
    inline int CreditCellY(const float y) { return static_cast<int>(ceilf((y - kCellEps) / kWorldMapUnitsPerCell)) - 1; }

    inline std::pair<int, int> CreditCellAt(const GW::Vec2f& wm) { return {CreditCellX(wm.x), CreditCellY(wm.y)}; }

    inline GW::Vec2f CreditCellCenterWorldMap(const int cx, const int cy)
    {
        return {cx * kWorldMapUnitsPerCell + 16.f, cy * kWorldMapUnitsPerCell + 16.f};
    }

    // Which fog bit is drawn under a point. Credit cells and fog bits are one index space, so this is
    // CreditCellAt without the epsilon only round-tripped positions need.
    inline std::pair<int, int> FogTileAt(const GW::Vec2f& wm)
    {
        return {
            static_cast<int>(floorf(wm.x / kWorldMapUnitsPerCell)),
            static_cast<int>(ceilf(wm.y / kWorldMapUnitsPerCell)) - 1,
        };
    }

    inline float Dist2(const GW::Vec2f& a, const GW::Vec2f& b)
    {
        const float dx = a.x - b.x, dy = a.y - b.y;
        return dx * dx + dy * dy;
    }

    // The reveal rule is a square block, so every neighbourhood walk in the widget is one of these.
    // `fn`/`pred` take (cx, cy, dx, dy).
    template <typename F>
    void ForEachInRing(const int cx, const int cy, const int r, F&& fn)
    {
        for (int dy = -r; dy <= r; dy++) {
            for (int dx = -r; dx <= r; dx++) fn(cx + dx, cy + dy, dx, dy);
        }
    }

    template <typename F>
    bool AnyInRing(const int cx, const int cy, const int r, F&& pred)
    {
        for (int dy = -r; dy <= r; dy++) {
            for (int dx = -r; dx <= r; dx++) {
                if (pred(cx + dx, cy + dy, dx, dy)) return true;
            }
        }
        return false;
    }

    struct CartoGrid {
        const uint32_t* bits = nullptr;
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t dword_count = 0;

        bool InGrid(const int cx, const int cy) const
        {
            return cx >= 0 && cy >= 0 && static_cast<uint32_t>(cx) < width && static_cast<uint32_t>(cy) < height;
        }

        // Anything without a set bit - off-grid, past the synced array - is unexplored, because that
        // is what the game fogs.
        bool IsExplored(const int cx, const int cy) const
        {
            if (!InGrid(cx, cy) || !bits) return false;
            const uint32_t word = static_cast<uint32_t>(cy) * RowWords(width) + (static_cast<uint32_t>(cx) >> 5);
            return word < dword_count && (bits[word] >> (static_cast<uint32_t>(cx) & 31) & 1);
        }
    };

    bool GetCartoGrid(CartoGrid& out);

    struct StandCell {
        // Reachable from where the player is: where we may send them. Gate-dependent.
        bool reachable = false;
        // Walkable ground exists in this tile at all. A property of the terrain, so it survives a
        // gate change - and it is read over a fog tile's 3x3 block, never at the tile itself.
        bool navmesh = false;
        GW::GamePos pos{}; // somewhere inside the cell you can actually stand
        int reveals = 0;   // still-foggy cells this spot would credit
    };

    // Probing a map costs a walkability query per cell and the answer never changes while you are on
    // that map, so it is swept once and kept. `strict` and `skipped` are learned the same way.
    struct MapProbe {
        std::map<std::pair<int, int>, StandCell> cells;
        // Slivers a wide-range visit failed to credit; these stop counting beyond one tile away.
        std::set<std::pair<int, int>> strict;
        std::set<std::pair<int, int>> skipped;
        // Reachability depends on gate state, so a mismatch means the cached answers are stale.
        std::vector<uint32_t> blocked_planes;
        bool complete = false;
    };

    // Always valid: off the world map it points at an empty probe.
    extern MapProbe* probe;

    // Why a foggy square was dropped, so the overlay can say it rather than just omitting the square.
    enum class FogSkip { PastMapBoundary, NoGroundInRange, GlitchOnly, Unreachable, NeverCredits };

    struct UncoverableCell {
        int cx = 0, cy = 0;
        FogSkip why = FogSkip::NoGroundInRange;
    };

    // Corner alphas are baked on the game thread so the overlay never reads the live bitmap.
    struct FogCell {
        int cx = 0, cy = 0;
        uint8_t corner_alpha[kFogSubdivisions + 1][kFogSubdivisions + 1] = {};
    };

    extern std::vector<FogCell> fog_cells;
    extern std::vector<UncoverableCell> uncoverable_cells;
    extern int explored_tiles;
    extern int unexpected_tiles;
    extern int coverable_tiles;

    struct ContinentMask {
        int continent = -1;
        int x0 = 0, y0 = 0, w = 0, h = 0;
        // Which tiles this continent's ground can credit, already clipped per map at bake time, and
        // the undilated ground it came from - the only way to ask whether a claim is about this map
        // or the one next door.
        const CartographyData::Mask* credit = nullptr;
        const CartographyData::Mask* raw = nullptr;
        // Always the permissive pair, so a square can be told apart from one nothing can credit.
        const CartographyData::Mask* glitch_only = nullptr;
        const CartographyData::Mask* raw_any = nullptr;
        // Every trapezoid the map files hold, walkable from an entrance or not.
        const CartographyData::Mask* any_credit = nullptr;
        const CartographyData::Mask* any_raw = nullptr;

        static bool Sample(const CartographyData::Mask* m, const int cx, const int cy)
        {
            if (!m) return false;
            const int lx = cx - m->x0, ly = cy - m->y0;
            if (lx < 0 || ly < 0 || lx >= m->width || ly >= m->height) return false;
            const size_t bit = static_cast<size_t>(ly) * m->width + lx;
            if (bit / 8 >= static_cast<size_t>(m->byte_count)) return false;
            return m->bits[bit >> 3] >> (bit & 7) & 1;
        }

        bool Get(const int cx, const int cy) const { return Sample(credit, cx, cy); }
        bool RawGet(const int cx, const int cy) const { return Sample(raw, cx, cy); }
        bool AnyGroundAt(const int cx, const int cy) const { return Sample(raw_any, cx, cy); }
        bool Empty() const { return !credit; }
        // Creditable only if you Shadow-step through a gate to get there.
        bool NeedsGlitch(const int cx, const int cy) const { return !Get(cx, cy) && Sample(glitch_only, cx, cy); }
    };

    extern ContinentMask continent_mask;

    // The live navmesh grid, kept private to the widget; this is all the probe log needs of it.
    struct NavGridInfo {
        int x0, y0, width, height, ground_count, stand_count;
        bool built;
    };

    NavGridInfo GetNavGridInfo();
    bool NavInGrid(int cx, int cy);
    bool NavGroundAt(int cx, int cy);

    extern std::pair<int, int> map_rect_min, map_rect_max;

    bool EnsureMapRect();
    bool InMapBounds(int cx, int cy);
    bool InCreditableBoundsOf(GW::Constants::MapID map_id, int cx, int cy, int slack = kBoundarySlack);
    bool CellCreditableFrom(int dx, int dy, int fx, int fy);
    bool FogCellCoverable(int cx, int cy);
    bool ThisMapCanCredit(int cx, int cy);

    // Flood the map file's trapezoids from `seeds`. `gates` block the walk; `honour_no_pathing`
    // respects the 0x04 portal flag.
    std::unordered_set<const GW::PathingTrapezoid*> Flood(const Pathing::PathingMapData& data,
                                                          const std::vector<const GW::PathingTrapezoid*>& seeds,
                                                          const std::vector<Pathing::TravelDoorway>& gates,
                                                          bool honour_no_pathing = true);

    // Which trapezoids of a file a player can actually be standing on, and the same ground walked as
    // if travel portals did not stop you.
    void PlayableTrapezoids(const Pathing::PathingMapData& data,
                            std::unordered_set<const GW::PathingTrapezoid*>& gated_out,
                            std::unordered_set<const GW::PathingTrapezoid*>& open_out);

    // Every tile `trap` overlaps, converted through `map_id`'s own world-map anchor rather than the
    // loaded map's - the bake and the tile-owner lookup both answer for maps they are not standing in.
    // `fn` takes (cx, cy).
    template <typename F>
    void ForEachTileOfTrapezoid(const GW::Constants::MapID map_id, const GW::PathingTrapezoid& trap, F&& fn)
    {
        const GW::GamePos lo{std::min(trap.XTL, trap.XBL), trap.YB};
        const GW::GamePos hi{std::max(trap.XTR, trap.XBR), trap.YT};
        GW::Vec2f a{}, b{};
        if (!WorldMapWidget::GamePosToWorldMap(lo, a, map_id)) return;
        if (!WorldMapWidget::GamePosToWorldMap(hi, b, map_id)) return;
        const auto [x0, y0] = FogTileAt({std::min(a.x, b.x), std::min(a.y, b.y)});
        const auto [x1, y1] = FogTileAt({std::max(a.x, b.x), std::max(a.y, b.y)});
        for (int cy = y0; cy <= y1; cy++) {
            for (int cx = x0; cx <= x1; cx++) {
                GW::GamePos ca{}, cb{};
                if (!WorldMapWidget::WorldMapToGamePos({cx * kWorldMapUnitsPerCell, cy * kWorldMapUnitsPerCell}, ca, map_id)) continue;
                if (!WorldMapWidget::WorldMapToGamePos({(cx + 1) * kWorldMapUnitsPerCell, (cy + 1) * kWorldMapUnitsPerCell}, cb, map_id)) continue;
                GW::Vec2f footing{};
                if (Pathing::TrapezoidOverlapsBox(&trap, {std::min(ca.x, cb.x), std::min(ca.y, cb.y)},
                                                  {std::max(ca.x, cb.x), std::max(ca.y, cb.y)}, footing)) {
                    fn(cx, cy);
                }
            }
        }
    }

#ifdef _DEBUG
    void LogProbe(const GW::Vec2f& at);
    void StartBake();
    void StepBake();
    bool BakeRunning();
    void DrawBakeSettings();
#endif
}
