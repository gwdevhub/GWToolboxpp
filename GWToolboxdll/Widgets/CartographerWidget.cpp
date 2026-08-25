#include "stdafx.h"

#include <bit>
#include <fstream>
#include <map>
#include <numeric>

#include <GWCA/Constants/Constants.h>
#include <GWCA/Context/CharContext.h>
#include <GWCA/Context/MapContext.h>
#include <GWCA/Context/WorldContext.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Map.h>
#include <GWCA/GameEntities/Quest.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <ImGuiAddons.h>
#include <Logger.h>
#include <Timer.h>
#include <Modules/QuestModule.h>
#include <Modules/Resources.h>
#include <Utils/EncString.h>
#include <Utils/SettingsRegistry.h>
#include <Utils/ToolboxUtils.h>
#include <Widgets/CartographerWidget.h>
#include <Widgets/CartographyData.h>
#include <Widgets/MissionMapWidget.h>
#include <Widgets/WorldMapWidget.h>
#include <Windows/TravelWindow.h>
#include <Windows/Pathfinding/Pathing.h>
#include <Windows/Pathfinding/PathfindingWindow.h>
#include <Windows/Pathfinding/PathingMapDataLoader.h>

#ifdef _DEBUG
#define CARTO_LOG(...) Log::Log(__VA_ARGS__)
#else
#define CARTO_LOG(...) ((void)0)
#endif

namespace {

    // Gw.exe's fog mesh builder is handed WorldContext::cartographed_areas (+0x5A4) and h05B4
    // (+0x5B4, the grid dims): one bit per 32x32-world-map-unit cell, addressed as below.
    constexpr float kWorldMapUnitsPerCell = 32.f;

    uint32_t RowWords(const uint32_t width)
    {
        return width >> 5;
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

        // Anything without a set bit - off-grid, past the synced array - is unexplored, because
        // that is what the game fogs.
        bool IsExplored(const int cx, const int cy) const
        {
            if (!InGrid(cx, cy)) return false;
            const uint32_t word = static_cast<uint32_t>(cy) * RowWords(width) + (static_cast<uint32_t>(cx) >> 5);
            if (!bits || word >= dword_count) return false;
            return (bits[word] >> (static_cast<uint32_t>(cx) & 31)) & 1;
        }
    };

    bool GetCartoGrid(CartoGrid& out)
    {
        const auto* world = GW::GetWorldContext();
        if (!world) return false;
        out.width = world->h05B4[0];
        out.height = world->h05B4[1];
        out.bits = reinterpret_cast<const uint32_t*>(world->cartographed_areas.m_buffer);
        out.dword_count = world->cartographed_areas.size();
        return out.width && out.height;
    }

    // wm.y is a divide plus an add on top of mid.y, itself a divide plus an add, so it carries a
    // few ulp; 1/64 is a power of two with plenty of headroom and 1/2048 of a tile.
    constexpr float kCellEps = 1.f / 64.f;

    int CreditCellX(const float x)
    {
        return static_cast<int>(floorf((x + kCellEps) / kWorldMapUnitsPerCell));
    }

    int CreditCellY(const float y)
    {
        return static_cast<int>(ceilf((y - kCellEps) / kWorldMapUnitsPerCell)) - 1;
    }

    std::pair<int, int> CreditCellAt(const GW::Vec2f& wm)
    {
        return {CreditCellX(wm.x), CreditCellY(wm.y)};
    }

    GW::Vec2f CreditCellCenterWorldMap(const int cx, const int cy)
    {
        return {cx * kWorldMapUnitsPerCell + 16.f, cy * kWorldMapUnitsPerCell + 16.f};
    }

    std::pair<int, int> FogTileAt(const GW::Vec2f& wm)
    {
        return {
            static_cast<int>(floorf(wm.x / kWorldMapUnitsPerCell)),
            static_cast<int>(ceilf(wm.y / kWorldMapUnitsPerCell)) - 1,
        };
    }

    float Dist2(const GW::Vec2f& a, const GW::Vec2f& b)
    {
        const float dx = a.x - b.x;
        const float dy = a.y - b.y;
        return dx * dx + dy * dy;
    }

    constexpr float kGwinchesPerWorldMapUnit = 96.f;
    constexpr ImU32 kFogPointColor = IM_COL32(64, 220, 255, 255);
    constexpr ImU32 kTargetColor = IM_COL32(255, 190, 64, 255);
    constexpr ImU32 kStandColor = IM_COL32(255, 236, 170, 255);
    constexpr ImU32 kFogColor = IM_COL32(0x50, 0xFF, 0x78, 255);
    constexpr ImU32 kGridColor = IM_COL32(255, 255, 255, 40);
    constexpr ImU32 kGridDotColor = IM_COL32(255, 255, 255, 70);
    constexpr ImU32 kCurrentTileColor = IM_COL32(120, 185, 255, 255);
    constexpr ImU32 kUnexpectedColor = IM_COL32(255, 110, 220, 255);
    constexpr ImU32 kUncoverableColor = IM_COL32(150, 150, 150, 255);
    constexpr ImU32 kGlitchOnlyColor = IM_COL32(255, 205, 55, 255);

    ImU32 WithAlpha(const ImU32 color, const int alpha)
    {
        return (color & 0x00FFFFFF) | (static_cast<ImU32>(alpha) << 24);
    }

    float Pulse()
    {
        const float t = static_cast<float>(TIMER_INIT()) / CLOCKS_PER_SEC;
        return 0.5f + 0.5f * sinf(t * (2.f * IM_PI) / 1.6f);
    }

    // World map coords have north at -y.
    const char* CompassDir(const GW::Vec2f& from, const GW::Vec2f& to)
    {
        static constexpr const char* dirs[] = {"E", "SE", "S", "SW", "W", "NW", "N", "NE"};
        const int idx = static_cast<int>(roundf(atan2f(to.y - from.y, to.x - from.x) / (IM_PI / 4.f)));
        return dirs[(idx + 8) % 8];
    }

    bool show_fog = true;
    bool show_stand_cells = true;
    bool show_grid = false;
    bool using_bec = false;
    bool set_quest_marker = true;

    constexpr int kRevealRadius = 1;
    constexpr int kRevealRadiusBec = 3;

    int RevealRadius()
    {
        return using_bec ? kRevealRadiusBec : kRevealRadius;
    }

    struct StandCell {
        // Reachable from where the player is: where we may send them. Gate-dependent.
        bool reachable = false;
        // Walkable ground exists in this tile at all. A property of the terrain, so it survives a
        // gate change - and it is read over a fog tile's 3x3 block, never at the tile itself.
        bool navmesh = false;
        GW::GamePos pos{}; // somewhere inside the cell you can actually stand
        int reveals = 0;   // still-foggy cells this spot would credit
    };

    struct MapProbe {
        std::map<std::pair<int, int>, StandCell> cells;
        // Slivers a wide-range visit failed to credit. BEC range misses a few tiles that only
        // normal range uncovers, so these stop counting beyond one tile away.
        std::set<std::pair<int, int>> strict;
        std::set<std::pair<int, int>> skipped;
        // Gate state the sweep was taken under; reachability depends on it, so a mismatch means
        // the cached answers no longer describe this instance.
        std::vector<uint32_t> blocked_planes;
        bool complete = false;
    };
    std::map<GW::Constants::MapID, MapProbe> probe_cache;
    // Which tiles are worth probing depends on where that character's fog is, so a different
    // character invalidates the sweep even though the terrain has not moved.
    std::wstring probe_cache_character;

    // Always valid: off the world map it points at an empty probe, so everything downstream reads
    // as "nothing here" without a null check on every access.
    MapProbe no_map_probe;
    MapProbe* probe = &no_map_probe;
    bool map_on_world_map = false;

    constexpr auto kCartographyUpdated = static_cast<GW::UI::UIMessage>(0x10000090);
    GW::HookEntry carto_ui_entry;
    std::vector<uint32_t> carto_snapshot;
    bool carto_dirty = true;
    bool coverage_stale = true;

    void CollectChangedTiles(const CartoGrid& grid, std::vector<std::pair<int, int>>& out)
    {
        const uint32_t row_words = RowWords(grid.width);
        if (!grid.bits || !grid.dword_count || !row_words) return;
        if (carto_snapshot.size() != grid.dword_count) {
            carto_snapshot.assign(grid.bits, grid.bits + grid.dword_count);
            coverage_stale = true; // no basis for a diff, so rebuild wholesale
            return;
        }
        for (uint32_t i = 0; i < grid.dword_count; i++) {
            const uint32_t changed = carto_snapshot[i] ^ grid.bits[i];
            if (!changed) continue;
            carto_snapshot[i] = grid.bits[i];
            const int cy = static_cast<int>(i / row_words);
            const int base_x = static_cast<int>(i % row_words) * 32;
            for (int b = 0; b < 32; b++) {
                if (changed & 1u << b) out.push_back({base_x + b, cy});
            }
        }
    }

    void SelectProbe(const GW::Constants::MapID map_id)
    {
        const auto* ctx = GW::GetCharContext();
        const std::wstring character = ctx ? ctx->player_name : L"";
        if (character != probe_cache_character) {
            probe_cache.clear();
            probe_cache_character = character;
        }
        probe = &probe_cache[map_id];
    }

    void DropProbeIfGatesMoved()
    {
        std::vector<uint32_t> blocked;
        if (!Pathing::CopyBlockedPlanes(blocked) || blocked == probe->blocked_planes) return;
        probe->cells.clear();
        probe->strict.clear();
        // A closed gate is one reason standing somewhere credited nothing, so that verdict expires
        // with the gate too. Manual declines live in `declined_cells` and are untouched.
        probe->skipped.clear();
        probe->blocked_planes = std::move(blocked);
        probe->complete = false;
        coverage_stale = true;
        CARTO_LOG("[cartographer] blocked planes changed; re-probing this map");
    }

    // The map we are standing in, as a tile rectangle. Kept off RebuildFog's cached pair because
    // that one is filled after scoring has already run and reads as the whole world on a revisit.
    GW::Constants::MapID map_rect_id = static_cast<GW::Constants::MapID>(0);
    std::pair<int, int> map_rect_min{}, map_rect_max{};
    ImRect map_rect_bounds;
    bool map_rect_valid = false;

    bool EnsureMapRect()
    {
        const auto map_id = GW::Map::GetMapID();
        if (map_id == map_rect_id) return map_rect_valid;
        map_rect_id = map_id;
        map_rect_valid = false;
        ImRect bounds;
        const auto info = GW::Map::GetMapInfo(map_id);
        if (!(info && GW::Map::GetMapWorldMapBounds(info, &bounds) && bounds.GetWidth() >= 1.f && bounds.GetHeight() >= 1.f)) return false;
        map_rect_min = {CreditCellX(bounds.Min.x), CreditCellY(bounds.Min.y)};
        map_rect_max = {
            static_cast<int>(ceilf(bounds.Max.x / kWorldMapUnitsPerCell)),
            static_cast<int>(ceilf(bounds.Max.y / kWorldMapUnitsPerCell)),
        };
        map_rect_bounds = bounds;
        map_rect_valid = true;
        return true;
    }

    bool InMapBounds(const int cx, const int cy)
    {
        if (!EnsureMapRect()) return true; // no rectangle to clamp against - do not hide everything
        return cx >= map_rect_min.first && cx < map_rect_max.first
            && cy >= map_rect_min.second && cy < map_rect_max.second;
    }

    // Credit stops one square past the standing map's rectangle, not wherever the ring reaches:
    // measured on Shenzun Tunnels ground a row past its south edge, which credits that row, not the next.
    constexpr int kBoundarySlack = 1;

    bool InCreditableBounds(const int cx, const int cy)
    {
        if (!EnsureMapRect()) return true;
        return cx >= map_rect_min.first - kBoundarySlack && cx < map_rect_max.first + kBoundarySlack
            && cy >= map_rect_min.second - kBoundarySlack && cy < map_rect_max.second + kBoundarySlack;
    }

    bool InCreditableBoundsOf(const GW::Constants::MapID map_id, const int cx, const int cy, const int slack = kBoundarySlack)
    {
        const auto info = GW::Map::GetMapInfo(map_id);
        ImRect bounds;
        if (!(info && GW::Map::GetMapWorldMapBounds(info, &bounds))) return false;
        return cx >= CreditCellX(bounds.Min.x) - slack
            && cx < static_cast<int>(ceilf(bounds.Max.x / kWorldMapUnitsPerCell)) + slack
            && cy >= CreditCellY(bounds.Min.y) - slack
            && cy < static_cast<int>(ceilf(bounds.Max.y / kWorldMapUnitsPerCell)) + slack;
    }

    // QuestModule::SetCustomQuestMarker only resolves to a real in-map position while the current
    // map's world-map rectangle contains it; outside it the marker becomes a travel marker instead.
    bool StandRoutable(const GW::Vec2f& wm)
    {
        if (!EnsureMapRect()) return true;
        return map_rect_bounds.Contains({wm.x, wm.y});
    }

    bool CellQualifies(const int fx, const int fy)
    {
        if (!InMapBounds(fx, fy)) return false;
        if (probe->strict.contains({fx, fy})) return false;
        if (!probe->complete) return true;
        for (int ny = -1; ny <= 1; ny++) {
            for (int nx = -1; nx <= 1; nx++) {
                const auto it = probe->cells.find({fx + nx, fy + ny});
                if (it != probe->cells.end() && it->second.navmesh) return true;
            }
        }
        return false;
    }

    bool CellCreditableFrom(const int dx, const int dy, const int fx, const int fy)
    {
        if (abs(dx) <= kRevealRadius && abs(dy) <= kRevealRadius) return InCreditableBounds(fx, fy);
        return CellQualifies(fx, fy);
    }

    std::set<std::pair<int, int>> declined_cells;
    // `was_fog` is what lets a point retire itself: one placed on fog is done when that tile is
    // credited, while one dropped on ground already explored is just a waypoint and never would be.
    struct CustomPoint {
        GW::Vec2f wm{};
        bool was_fog = true;
    };
    std::vector<CustomPoint> custom_points;
    std::string declined_cells_str;
    std::string custom_points_str;

    void SerializeDeclined()
    {
        declined_cells_str.clear();
        for (const auto& [cx, cy] : declined_cells) {
            if (!declined_cells_str.empty()) declined_cells_str += ",";
            declined_cells_str += std::format("{}:{}", cx, cy);
        }
    }

    void ParseDeclined()
    {
        declined_cells.clear();
        std::istringstream is(declined_cells_str);
        std::string tok;
        while (std::getline(is, tok, ',')) {
            int cx, cy;
            if (sscanf_s(tok.c_str(), "%d:%d", &cx, &cy) == 2) declined_cells.insert({cx, cy});
        }
    }

    void SerializePoints()
    {
        custom_points_str.clear();
        for (const auto& p : custom_points) {
            if (!custom_points_str.empty()) custom_points_str += ",";
            custom_points_str += std::format("{:.1f}:{:.1f}:{}", p.wm.x, p.wm.y, p.was_fog ? 1 : 0);
        }
    }

    void ParsePoints()
    {
        custom_points.clear();
        std::istringstream is(custom_points_str);
        std::string tok;
        while (std::getline(is, tok, ',')) {
            float x, y;
            int was_fog = 1; // points written before the flag existed were all placed on fog
            if (sscanf_s(tok.c_str(), "%f:%f:%d", &x, &y, &was_fog) >= 2) custom_points.push_back({{x, y}, was_fog != 0});
        }
    }

    // Fog nothing on this map can reach; excluded from the overlay so it only shows actionable fog.
    int unreachable_fog_cells = 0;

    // Why a foggy square was dropped, so the overlay can say it rather than just omitting the square.
    enum class FogSkip { PastMapBoundary, NoGroundInRange, GlitchOnly, Unreachable, NeverCredits };

    struct UncoverableCell {
        int cx = 0, cy = 0;
        FogSkip why = FogSkip::NoGroundInRange;
    };
    constexpr size_t kUncoverableListMax = 4096;
    std::vector<UncoverableCell> uncoverable_cells;
    // A queued fog point selection had to pass over because nothing reachable credits it.
    bool blocked_point = false;

    bool show_unexpected = false;
    bool show_uncoverable = true;
    // Squares only a gate glitch can credit: off means the overlay answers for normal play.
    bool allow_gate_glitch = false;
    int explored_tiles = 0;
    int coverable_tiles = 0;
    int coverable_tiles_radius = -1;
    int coverable_tiles_continent = -1;
    int unexpected_tiles = 0;
    constexpr size_t kUnexpectedListMax = 4096;
    std::vector<std::pair<int, int>> unexpected_cells;

    // The client's fog texture is this many texels per cartography cell, so the visible fog is
    // four times finer than the 32-unit grid the bits live on.
    constexpr int kFogSubdivisions = 4;

    // Corner alphas are baked on the game thread so the overlay never reads the live bitmap.
    struct FogCell {
        int cx = 0, cy = 0;
        uint8_t corner_alpha[kFogSubdivisions + 1][kFogSubdivisions + 1] = {};
    };
    std::vector<FogCell> fog_cells;
    int map_fog_cells = -1;
    std::pair<int, int> map_cell_min{}, map_cell_max{};
    std::pair<int, int> player_cell{};
    bool player_cell_valid = false;

    bool show_whole_continent = true;

    constexpr int kMaskRadius = 1;

    struct ContinentMask {
        int continent = -1;
        int x0 = 0, y0 = 0, w = 0, h = 0;
        // Which tiles this continent's ground can credit, already clipped per map at bake time,
        // and the undilated ground it came from - the only way to ask whether a claim is about
        // this map or the one next door.
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
    ContinentMask continent_mask;

    bool mask_built_for_glitch = false;

    void BuildContinentMask(const int continent)
    {
        if (continent_mask.continent == continent && mask_built_for_glitch == allow_gate_glitch) return;
        mask_built_for_glitch = allow_gate_glitch;
        continent_mask = {};
        continent_mask.continent = continent;
        const CartographyData::Continent* src = nullptr;
        for (const auto& c : CartographyData::kContinents) {
            if (c.id == continent) { src = &c; break; }
        }
        if (!src) return;
        continent_mask.raw = allow_gate_glitch ? &src->standable_glitched : &src->standable;
        continent_mask.credit = allow_gate_glitch ? &src->creditable_glitched : &src->creditable;
        continent_mask.glitch_only = &src->creditable_glitched;
        // Ground existing at all, which is a different question from ground anything can reach:
        // terrain no gate leads to is real terrain nobody can ever walk on, and a foggy square next
        // to it has to read as uncoverable rather than as empty space between maps.
        continent_mask.raw_any = &src->standable_any;
        continent_mask.any_credit = &src->creditable_any;
        continent_mask.any_raw = &src->standable_any;
        continent_mask.x0 = continent_mask.credit->x0;
        continent_mask.y0 = continent_mask.credit->y0;
        continent_mask.w = continent_mask.credit->width;
        continent_mask.h = continent_mask.credit->height;
    }

    // Squares the client does not credit however you reach them. The undercity stopped counting, so
    // its ground is still in the map files and still dilates into these squares, but standing there
    // will never clear them. Nothing in the DAT records that, so it can only be a list.
    struct DeadTile {
        int continent, cx, cy;
    };

    constexpr DeadTile kNeverCredits[] = {
        {2, 104, 42},
        {2, 105, 42},
    };

    bool TileNeverCredits(const int cx, const int cy)
    {
        return std::ranges::any_of(kNeverCredits, [&](const DeadTile& t) {
            return t.continent == continent_mask.continent && t.cx == cx && t.cy == cy;
        });
    }

    // Whether any map's ground can credit the square. Deliberately says nothing about the loaded
    // navmesh: which map you happen to be in must not change whether a square is worth uncovering.
    bool FogCellCoverable(const int cx, const int cy)
    {
        if (TileNeverCredits(cx, cy)) return false;
        return continent_mask.Empty() || continent_mask.Get(cx, cy);
    }

    // Whether the loaded map has ground that credits the square and can be walked to from here.
    bool ThisMapCanCredit(const int cx, const int cy)
    {
        if (!probe->complete) return true;
        const int r = RevealRadius();
        for (int dy = -r; dy <= r; dy++) {
            for (int dx = -r; dx <= r; dx++) {
                if (!CellCreditableFrom(dx, dy, cx, cy)) continue;
                const auto it = probe->cells.find({cx + dx, cy + dy});
                if (it != probe->cells.end() && it->second.reachable) return true;
            }
        }
        return false;
    }

    // Which of FogCellCoverable's exits dropped the square. Ground within reach but no credit means
    // the bake clipped it: the ground belongs to a map this square is too far outside of.
    FogSkip WhyNotCoverable(const int cx, const int cy)
    {
        if (TileNeverCredits(cx, cy)) return FogSkip::NeverCredits;
        if (continent_mask.NeedsGlitch(cx, cy)) return FogSkip::GlitchOnly;
        const int r = RevealRadius();
        // Ground a player can stand on first: then the square was dropped by the per-map credit clip
        // rather than by the terrain being out of reach, and the two want different answers.
        for (int dy = -r; dy <= r; dy++) {
            for (int dx = -r; dx <= r; dx++) {
                if (continent_mask.RawGet(cx + dx, cy + dy)) return FogSkip::PastMapBoundary;
            }
        }
        for (int dy = -r; dy <= r; dy++) {
            for (int dx = -r; dx <= r; dx++) {
                if (continent_mask.AnyGroundAt(cx + dx, cy + dy)) return FogSkip::Unreachable;
            }
        }
        return FogSkip::NoGroundInRange;
    }

    // Normal range comes straight from the bake, which already clipped each map's dilation to its
    // own rectangle; only the extra Bird's Eye rings still have to be walked over the raw ground.
    // `permissive` asks only whether ground exists, ignoring whether anything can walk to it.
    bool StandableWithin(const int cx, const int cy, const int radius, const bool permissive = false)
    {
        const auto* credit = permissive ? continent_mask.any_credit : continent_mask.credit;
        const auto* ground = permissive ? continent_mask.any_raw : continent_mask.raw;
        if (ContinentMask::Sample(credit, cx, cy)) return true;
        for (int dy = -radius; dy <= radius; dy++) {
            for (int dx = -radius; dx <= radius; dx++) {
                if (std::max(abs(dx), abs(dy)) <= kMaskRadius) continue;
                if (ContinentMask::Sample(ground, cx + dx, cy + dy)) return true;
            }
        }
        return false;
    }

    void RecountExploration(const CartoGrid& grid)
    {
        explored_tiles = 0;
        unexpected_tiles = 0;
        unexpected_cells.clear();
        const uint32_t row_words = RowWords(grid.width);
        if (!grid.bits || !row_words) return;
        const uint32_t words = std::min(grid.dword_count, row_words * grid.height);
        const int radius = RevealRadius();
        const bool have_bake = !continent_mask.Empty();
        for (uint32_t i = 0; i < words; i++) {
            uint32_t word = grid.bits[i];
            if (!word) continue;
            explored_tiles += std::popcount(word);
            if (!have_bake) continue;
            const int cy = static_cast<int>(i / row_words);
            const int base_x = static_cast<int>(i % row_words) * 32;
            while (word) {
                const int cx = base_x + std::countr_zero(word);
                word &= word - 1;
                // Permissive: ground the bake found but cannot walk to still explains the square.
                if (StandableWithin(cx, cy, radius, true)) continue;
                unexpected_tiles++;
                if (unexpected_cells.size() < kUnexpectedListMax) unexpected_cells.push_back({cx, cy});
            }
        }
        if (!have_bake) {
            coverable_tiles = 0;
            coverable_tiles_radius = -1;
            return;
        }
        if (coverable_tiles_radius == radius && coverable_tiles_continent == continent_mask.continent) return;
        coverable_tiles_radius = radius;
        coverable_tiles_continent = continent_mask.continent;
        coverable_tiles = 0;
        const int pad = std::max(0, radius - kMaskRadius);
        for (int cy = continent_mask.y0 - pad; cy < continent_mask.y0 + continent_mask.h + pad; cy++) {
            for (int cx = continent_mask.x0 - pad; cx < continent_mask.x0 + continent_mask.w + pad; cx++) {
                if (StandableWithin(cx, cy, radius)) coverable_tiles++;
            }
        }
    }

    constexpr float kFogMaxAlpha = 135.f;

    float ExploredAtCorner(const CartoGrid& grid, const int cx, const int cy)
    {
        return (static_cast<float>(grid.IsExplored(cx - 1, cy - 1)) + static_cast<float>(grid.IsExplored(cx, cy - 1)) +
                static_cast<float>(grid.IsExplored(cx - 1, cy)) + static_cast<float>(grid.IsExplored(cx, cy))) * 0.25f;
    }

    void BakeFogCell(const CartoGrid& grid, FogCell& out)
    {
        const float tl = ExploredAtCorner(grid, out.cx, out.cy);
        const float tr = ExploredAtCorner(grid, out.cx + 1, out.cy);
        const float bl = ExploredAtCorner(grid, out.cx, out.cy + 1);
        const float br = ExploredAtCorner(grid, out.cx + 1, out.cy + 1);
        for (int j = 0; j <= kFogSubdivisions; j++) {
            const float v = static_cast<float>(j) / kFogSubdivisions;
            for (int i = 0; i <= kFogSubdivisions; i++) {
                const float u = static_cast<float>(i) / kFogSubdivisions;
                const float explored = (tl + (tr - tl) * u) * (1.f - v) + (bl + (br - bl) * u) * v;
                const float quantised = roundf((1.f - explored) * 15.f) / 15.f;
                out.corner_alpha[j][i] = static_cast<uint8_t>(kFogMaxAlpha * quantised);
            }
        }
    }


    // A footing sits anywhere in its cell, so it can be a half-diagonal off the cell centre that
    // the ring search sorts by - which is the slack that search has to allow before it stops early.
    constexpr float kStandOffsetMax = kWorldMapUnitsPerCell * 0.5f * 1.41421356f;

    struct NavCells {
        int x0 = 0, y0 = 0, width = 0, height = 0;
        std::vector<uint8_t> ground; // walkable at all, gate-independent
        std::vector<uint8_t> standable;
        // Credit is per cell, so any reachable spot inside one reveals exactly the same fog as any
        // other - which trapezoid's overlap the footing comes from does not matter.
        std::vector<GW::GamePos> stand;
        std::vector<float> line_x, line_y;
        int ground_count = 0, stand_count = 0;
        GW::Constants::MapID map_id = static_cast<GW::Constants::MapID>(0);
        std::vector<uint32_t> blocked_planes;
        bool built = false;

        bool InGrid(const int cx, const int cy) const
        {
            return cx >= x0 && cy >= y0 && cx < x0 + width && cy < y0 + height;
        }

        size_t Index(const int cx, const int cy) const
        {
            return static_cast<size_t>(cy - y0) * width + (cx - x0);
        }

        void Reset(const int min_x, const int min_y, const int max_x, const int max_y)
        {
            x0 = min_x;
            y0 = min_y;
            width = max_x - min_x + 1;
            height = max_y - min_y + 1;
            ground.assign(static_cast<size_t>(width) * height, 0);
            standable.assign(static_cast<size_t>(width) * height, 0);
            stand.assign(static_cast<size_t>(width) * height, GW::GamePos{});
            line_x.clear();
            line_y.clear();
        }

        bool BuildLines()
        {
            line_x.resize(static_cast<size_t>(width) + 1);
            line_y.resize(static_cast<size_t>(height) + 1);
            GW::GamePos gp{};
            for (int i = 0; i <= width; i++) {
                if (!WorldMapWidget::WorldMapToGamePos({(x0 + i) * kWorldMapUnitsPerCell, 0.f}, gp)) return false;
                line_x[i] = gp.x;
            }
            for (int j = 0; j <= height; j++) {
                if (!WorldMapWidget::WorldMapToGamePos({0.f, (y0 + j) * kWorldMapUnitsPerCell}, gp)) return false;
                line_y[j] = gp.y;
            }
            return true;
        }

        void CellBox(const int cx, const int cy, GW::Vec2f& box_min, GW::Vec2f& box_max) const
        {
            const float x_lo = line_x[cx - x0], x_hi = line_x[cx - x0 + 1];
            const float y_lo = line_y[cy - y0], y_hi = line_y[cy - y0 + 1]; // y flips in the conversion
            box_min = {std::min(x_lo, x_hi), std::min(y_lo, y_hi)};
            box_max = {std::max(x_lo, x_hi), std::max(y_lo, y_hi)};
        }
    };
    NavCells nav_cells;

    bool FootingInCell(const Pathing::TrapezoidRef& ref, const int cx, const int cy, GW::GamePos& out)
    {
        GW::Vec2f box_min{}, box_max{}, footing{};
        nav_cells.CellBox(cx, cy, box_min, box_max);
        if (!Pathing::TrapezoidOverlapsBox(ref.trapezoid, box_min, box_max, footing)) return false;
        out = GW::GamePos{footing.x, footing.y, ref.plane};
        return true;
    }

    bool EnsureNavCells()
    {
        const auto map_id = GW::Map::GetMapID();
        std::vector<uint32_t> blocked;
        // Without a pathing context there is no gate state to key on, and rebuilding against an
        // empty one on every call is how this turned into a per-frame full-map sweep.
        if (!Pathing::CopyBlockedPlanes(blocked)) return false;
        if (nav_cells.built && nav_cells.map_id == map_id && nav_cells.blocked_planes == blocked) return true;
        // Built before the reachability walk can find the player, every trapezoid reads as reachable
        // - and these cells are kept for the life of the map, so that assumption would stick.
        if (!Pathing::IsReachabilityKnown()) return false;

        const auto started = clock();
        const auto trapezoids = Pathing::GetTrapezoidsWithReachability();
        if (trapezoids.empty()) return false;

        // The grid covers the geometry rather than the map's world-map rectangle: credit reaches
        // past that rectangle's edge, and the raw trapezoid extents cost nothing to take.
        float min_x = FLT_MAX, min_y = FLT_MAX, max_x = -FLT_MAX, max_y = -FLT_MAX;
        for (const auto& ref : trapezoids) {
            const auto* t = ref.trapezoid;
            min_x = std::min(min_x, std::min(t->XTL, t->XBL));
            max_x = std::max(max_x, std::max(t->XTR, t->XBR));
            min_y = std::min(min_y, t->YB);
            max_y = std::max(max_y, t->YT);
        }
        GW::Vec2f lo{}, hi{};
        if (!WorldMapWidget::GamePosToWorldMap(GW::GamePos{min_x, min_y}, lo)) return false;
        if (!WorldMapWidget::GamePosToWorldMap(GW::GamePos{max_x, max_y}, hi)) return false;
        const auto [grid_x0, grid_y0] = FogTileAt({std::min(lo.x, hi.x), std::min(lo.y, hi.y)});
        const auto [grid_x1, grid_y1] = FogTileAt({std::max(lo.x, hi.x), std::max(lo.y, hi.y)});

        nav_cells = {};
        nav_cells.map_id = map_id;
        nav_cells.blocked_planes = std::move(blocked);
        nav_cells.Reset(grid_x0, grid_y0, grid_x1, grid_y1);
        if (!nav_cells.BuildLines()) return false;

        for (const auto& ref : trapezoids) {
            const auto* t = ref.trapezoid;
            const bool reachable = ref.reachable;
            GW::Vec2f a{}, b{};
            if (!WorldMapWidget::GamePosToWorldMap(GW::GamePos{std::min(t->XTL, t->XBL), t->YB}, a)) continue;
            if (!WorldMapWidget::GamePosToWorldMap(GW::GamePos{std::max(t->XTR, t->XBR), t->YT}, b)) continue;
            const auto [x0, y0] = FogTileAt({std::min(a.x, b.x), std::min(a.y, b.y)});
            const auto [x1, y1] = FogTileAt({std::max(a.x, b.x), std::max(a.y, b.y)});
            for (int cy = y0; cy <= y1; cy++) {
                for (int cx = x0; cx <= x1; cx++) {
                    if (!nav_cells.InGrid(cx, cy)) continue;
                    const size_t i = nav_cells.Index(cx, cy);
                    if (nav_cells.ground[i] && (!reachable || nav_cells.standable[i])) continue;
                    GW::GamePos footing{};
                    if (!FootingInCell(ref, cx, cy, footing)) continue;
                    if (!nav_cells.ground[i]) {
                        nav_cells.ground[i] = 1;
                        nav_cells.ground_count++;
                    }
                    if (!reachable) continue;
                    nav_cells.standable[i] = 1;
                    nav_cells.stand[i] = footing;
                    nav_cells.stand_count++;
                }
            }
        }
        nav_cells.built = true;
        // The player is by definition standing on the navmesh, so their own cell has to be in here.
        // If it is not, the trapezoid-to-cell mapping is wrong and every verdict built on it is too.
        const auto* self = GW::Agents::GetControlledCharacter();
        GW::Vec2f self_wm{};
        const bool have_self = self && WorldMapWidget::GamePosToWorldMap(self->pos, self_wm);
        const auto self_cell = have_self ? CreditCellAt(self_wm) : std::pair{0, 0};
        const bool self_in_grid = have_self && nav_cells.InGrid(self_cell.first, self_cell.second);
        const size_t self_i = self_in_grid ? nav_cells.Index(self_cell.first, self_cell.second) : 0;
        Log::Log("[cartographer] navmesh cells: %d with ground, %d standable, from %u trapezoids in %dms; grid %dx%d at (%d,%d); "
                 "player cell (%d,%d) ground=%d standable=%d%s\n",
                 nav_cells.ground_count, nav_cells.stand_count, static_cast<unsigned>(trapezoids.size()),
                 static_cast<int>(clock() - started), nav_cells.width, nav_cells.height, nav_cells.x0, nav_cells.y0,
                 self_cell.first, self_cell.second,
                 self_in_grid && nav_cells.ground[self_i], self_in_grid && nav_cells.standable[self_i],
                 self_in_grid && !nav_cells.ground[self_i] ? "  <== MAPPING IS BROKEN" : "");
        Log::FlushFile();
        return true;
    }

    bool ProbeStandCell(const int cx, const int cy, StandCell& out)
    {
        if (!EnsureNavCells()) return false;
        if (!nav_cells.InGrid(cx, cy)) return true; // off the geometry entirely: genuinely nothing there
        const size_t i = nav_cells.Index(cx, cy);
        out.navmesh = nav_cells.ground[i] != 0;
        if (!nav_cells.standable[i]) return true;
        out.pos = nav_cells.stand[i];
        out.reachable = true;
        return true;
    }

    bool warned_stand_off_rect = false;

    bool ResolveStandWorldPos(const GW::Vec2f& fog_wm, const GW::Vec2f& from, GW::Vec2f& out, std::pair<int, int>& out_cell, bool& any_navmesh)
    {
        const auto [fx, fy] = FogTileAt(fog_wm);
        // `covered` is the ring already tried, so the wide pass only visits what the near one did not.
        const auto try_ring = [&](const int covered, const int r) {
            std::vector<std::pair<int, int>> candidates;
            for (int dy = -r; dy <= r; dy++) {
                for (int dx = -r; dx <= r; dx++) {
                    if (std::max(abs(dx), abs(dy)) <= covered) continue;
                    candidates.push_back({fx + dx, fy + dy});
                }
            }
            std::ranges::sort(candidates, [&from](const auto& a, const auto& b) {
                return Dist2(CreditCellCenterWorldMap(a.first, a.second), from) < Dist2(CreditCellCenterWorldMap(b.first, b.second), from);
            });
            float best_d2 = FLT_MAX;
            bool found = false;
            for (const auto& cell : candidates) {
                const float centre_d2 = Dist2(CreditCellCenterWorldMap(cell.first, cell.second), from);
                // A cell's centre only bounds the walk its probed footing actually costs, so ordering
                // by it is not enough to stop at the first hit - but it is enough to stop early.
                if (found && sqrtf(centre_d2) - kStandOffsetMax > sqrtf(best_d2)) break;
                auto it = probe->cells.find(cell);
                if (it == probe->cells.end()) {
                    StandCell sc;
                    if (!ProbeStandCell(cell.first, cell.second, sc)) continue;
                    it = probe->cells.emplace(cell, sc).first;
                    coverage_stale = true; // scored by the next recompute, not by us - we have no grid here
                }
                if (it->second.navmesh) any_navmesh = true;
                GW::Vec2f wm;
                if (!it->second.reachable || !WorldMapWidget::GamePosToWorldMap(it->second.pos, wm)) continue;
                const float d2 = Dist2(wm, from);
                if (found && d2 >= best_d2) continue;
                best_d2 = d2;
                found = true;
                out = wm;
                out_cell = cell;
            }
            return found;
        };
        // Near-range credit is unconditional; BEC range is a guess whose only disproof costs a 15s
        // dwell, so the wide ring is a fallback rather than a competitor.
        if (try_ring(-1, kRevealRadius)) return true;
        return RevealRadius() > kRevealRadius && CellQualifies(fx, fy) && try_ring(kRevealRadius, RevealRadius());
    }

    // Keeps the sweep to the fog's fringe instead of probing every cell on the map.
    bool CellWorthProbing(const CartoGrid& grid, const int cx, const int cy)
    {
        const int r = RevealRadius();
        for (int dy = -r; dy <= r; dy++) {
            for (int dx = -r; dx <= r; dx++) {
                if (grid.InGrid(cx + dx, cy + dy) && !grid.IsExplored(cx + dx, cy + dy)) return true;
            }
        }
        return false;
    }

    void SweepStandCells(const CartoGrid& grid, GW::AreaInfo* map_info)
    {
        if (probe->complete) return;
        ImRect bounds;
        if (!(map_info && GW::Map::GetMapWorldMapBounds(map_info, &bounds))) return;
        // Only this map's squares are standable; the fog they credit may still be the next map's.
        const int x0 = CreditCellX(bounds.Min.x);
        const int y0 = CreditCellY(bounds.Min.y);
        const int x1 = static_cast<int>(ceilf(bounds.Max.x / kWorldMapUnitsPerCell));
        const int y1 = static_cast<int>(ceilf(bounds.Max.y / kWorldMapUnitsPerCell));
        int budget = 6;
        for (int cy = y0; cy < y1 && budget > 0; cy++) {
            for (int cx = x0; cx < x1 && budget > 0; cx++) {
                if (probe->cells.contains({cx, cy})) continue;
                if (!CellWorthProbing(grid, cx, cy)) continue;
                StandCell sc;
                if (!ProbeStandCell(cx, cy, sc)) return; // navmesh not up; nothing learned, nothing kept
                probe->cells[{cx, cy}] = sc;
                budget--;
            }
        }
        probe->complete = budget > 0;
    }

    void ScoreStandCell(const CartoGrid& grid, const std::pair<int, int>& cell, StandCell& sc)
    {
        sc.reveals = 0;
        if (!sc.reachable) return;
        const int r = RevealRadius();
        for (int dy = -r; dy <= r; dy++) {
            for (int dx = -r; dx <= r; dx++) {
                const int fx = cell.first + dx;
                const int fy = cell.second + dy;
                if (!grid.InGrid(fx, fy) || grid.IsExplored(fx, fy)) continue;
                if (!CellCreditableFrom(dx, dy, fx, fy)) continue;
                sc.reveals++;
            }
        }
    }

    // Rescore only the tiles whose count could have moved: a tile's score counts fog within the
    // reveal radius, so only stands that near a flipped tile are affected.
    void RescoreAround(const CartoGrid& grid, const std::vector<std::pair<int, int>>& changed)
    {
        const int r = RevealRadius();
        for (const auto& [fx, fy] : changed) {
            for (int dy = -r; dy <= r; dy++) {
                for (int dx = -r; dx <= r; dx++) {
                    const auto it = probe->cells.find({fx + dx, fy + dy});
                    if (it != probe->cells.end()) ScoreStandCell(grid, it->first, it->second);
                }
            }
        }
    }

    void RebuildFog(const CartoGrid& grid, GW::AreaInfo* map_info)
    {
        unreachable_fog_cells = 0;
        uncoverable_cells.clear();
        fog_cells.clear();
        map_fog_cells = -1;
        ImRect bounds;
        if (!(map_info && GW::Map::GetMapWorldMapBounds(map_info, &bounds) && bounds.GetWidth() >= 1.f && bounds.GetHeight() >= 1.f)) return;
        int x0 = static_cast<int>(floorf(bounds.Min.x / kWorldMapUnitsPerCell));
        int y0 = static_cast<int>(floorf(bounds.Min.y / kWorldMapUnitsPerCell));
        int x1 = static_cast<int>(ceilf(bounds.Max.x / kWorldMapUnitsPerCell));
        int y1 = static_cast<int>(ceilf(bounds.Max.y / kWorldMapUnitsPerCell));
        map_cell_min = {x0, y0};
        map_cell_max = {x1, y1};
        // With the baked data we can answer for the whole continent, not just the map we are in -
        // which is the point of it: the world map then shows everything still worth walking to.
        if (show_whole_continent && !continent_mask.Empty()) {
            x0 = continent_mask.x0;
            y0 = continent_mask.y0;
            x1 = continent_mask.x0 + continent_mask.w;
            y1 = continent_mask.y0 + continent_mask.h;
        }
        for (int cy = y0; cy < y1; cy++) {
            for (int cx = x0; cx < x1; cx++) {
                if (!grid.InGrid(cx, cy) || grid.IsExplored(cx, cy)) continue;
                if (!FogCellCoverable(cx, cy)) {
                    unreachable_fog_cells++;
                    // Only where ground is actually near: the empty space between maps is most of
                    // the continent grid and was never fog anyone could clear, so greying it says nothing.
                    const auto why = WhyNotCoverable(cx, cy);
                    if (why != FogSkip::NoGroundInRange && uncoverable_cells.size() < kUncoverableListMax) {
                        uncoverable_cells.push_back({cx, cy, why});
                    }
                    continue;
                }
                FogCell f;
                f.cx = cx;
                f.cy = cy;
                BakeFogCell(grid, f);
                fog_cells.push_back(f);
            }
        }
        map_fog_cells = static_cast<int>(fog_cells.size());
    }

    void RecomputeCoverage(const CartoGrid& grid, GW::AreaInfo* map_info)
    {
        for (auto& [cell, sc] : probe->cells) {
            ScoreStandCell(grid, cell, sc);
        }
        RebuildFog(grid, map_info);
    }

    // Flood from one trapezoid under the bake's own rules, optionally relaxed, so a square the bake
    // dropped can be traced back to the connection its walk failed to make.
    std::unordered_set<const GW::PathingTrapezoid*> FloodFrom(const Pathing::PathingMapData& data,
                                                              const GW::PathingTrapezoid* seed,
                                                              const bool honour_no_pathing_flag)
    {
        std::unordered_set<const GW::PathingTrapezoid*> comp;
        if (!seed) return comp;
        std::unordered_map<const GW::PathingTrapezoid*, size_t> plane_of;
        for (size_t p = 0; p < data.planes.size(); p++) {
            const auto& plane = data.planes[p];
            for (uint32_t t = 0; t < plane.trapezoid_count; t++) plane_of[&plane.trapezoids[t]] = p;
        }
        std::vector<const GW::PathingTrapezoid*> queue{seed};
        comp.insert(seed);
        for (size_t head = 0; head < queue.size(); head++) {
            const auto* trap = queue[head];
            const auto expand = [&](const GW::PathingTrapezoid* next) {
                if (next && comp.insert(next).second) queue.push_back(next);
            };
            for (const auto* adj : trap->adjacent) expand(adj);
            const auto it = plane_of.find(trap);
            if (it == plane_of.end() || it->second >= data.planes.size()) continue;
            const auto& plane = data.planes[it->second];
            const auto expand_portal = [&](const uint16_t idx) {
                if (idx >= plane.portal_count) return;
                const auto& portal = plane.portals[idx];
                if (honour_no_pathing_flag && portal.flags & 0x04) return;
                if (!portal.pair) return;
                for (uint32_t i = 0; i < portal.pair->count; i++) expand(portal.pair->trapezoids[i]);
            };
            expand_portal(trap->portal_left);
            expand_portal(trap->portal_right);
        }
        return comp;
    }

    // Defined below; LogProbe reproduces the bake with it.
    std::unordered_set<const GW::PathingTrapezoid*> LargestComponent(const Pathing::PathingMapData& data, bool block_gates = true);
    std::vector<std::unordered_set<const GW::PathingTrapezoid*>> AllComponents(const Pathing::PathingMapData& data, bool block_gates = true);
    void PlayableTrapezoids(const Pathing::PathingMapData& data,
                            std::unordered_set<const GW::PathingTrapezoid*>& gated_out,
                            std::unordered_set<const GW::PathingTrapezoid*>& open_out);

#ifdef _DEBUG
    // Everything FogCellCoverable consults, in the order it consults it. The verdict is map-dependent
    // interpretable next to the map it was taken in.
    void LogProbe(const GW::Vec2f& at)
    {
        CartoGrid g;
        if (!GetCartoGrid(g)) {
            Log::Log("[cartographer] probe: no cartography data");
            Log::FlushFile();
            return;
        }
        // Both indices, because they agreeing is the invariant this widget rests on:
        // one being off from the other means the position conversion has drifted again.
        const auto [cx, cy] = FogTileAt(at);
        const auto [ccx, ccy] = CreditCellAt(at);
        GW::GamePos gp{};
        const bool converted = WorldMapWidget::WorldMapToGamePos(at, gp);
        const auto stand = probe->cells.find({ccx, ccy});
        Log::Log("[cartographer] probe wm(%.2f,%.2f) fog_tile(%d,%d) credit_cell(%d,%d): explored=%d, grid %ux%u (%u words/row), game(%.0f,%.0f), walkable here=%d, reachable here=%d, probed=%d reachable=%d reveals=%d, coverable=%d, radius=%d",
                 at.x, at.y, cx, cy, ccx, ccy, static_cast<int>(g.IsExplored(cx, cy)),
                 g.width, g.height, RowWords(g.width),
                 gp.x, gp.y, converted && Pathing::IsPositionWalkable(gp), converted && Pathing::IsPositionReachable(gp),
                 static_cast<int>(stand != probe->cells.end()),
                 stand != probe->cells.end() ? static_cast<int>(stand->second.reachable) : 0,
                 stand != probe->cells.end() ? stand->second.reveals : 0,
                 static_cast<int>(FogCellCoverable(cx, cy)), RevealRadius());
        if (converted) {
            GW::GamePos snapped = gp;
            GW::Vec2f snapped_wm{};
            const bool found = Pathing::FindClosestPositionOnTrapezoid(snapped) != nullptr;
            const bool back = found && WorldMapWidget::GamePosToWorldMap(snapped, snapped_wm);
            const auto snapped_cell = back ? CreditCellAt(snapped_wm) : std::pair{0, 0};
            Log::Log("[cartographer] probe (%d,%d): nearest ground found=%d game(%.0f,%.0f) wm(%.1f,%.1f) cell(%d,%d) %s dist=%.0f gwinch reachable=%d",
                     cx, cy, found, snapped.x, snapped.y, snapped_wm.x, snapped_wm.y,
                     snapped_cell.first, snapped_cell.second,
                     back && snapped_cell == std::pair{cx, cy} ? "IN THIS CELL" : "in a DIFFERENT cell",
                     sqrtf((snapped.x - gp.x) * (snapped.x - gp.x) + (snapped.y - gp.y) * (snapped.y - gp.y)),
                     found && Pathing::IsPositionReachable(snapped));
        }
        const bool rect = EnsureMapRect();
        Log::Log("[carto-bake] (%d,%d) verdict=%d | credit_mask=%d credit_glitched=%d stand_here=%d stand_glitched_here=%d probe_complete=%d",
                 cx, cy, static_cast<int>(FogCellCoverable(cx, cy)),
                 static_cast<int>(continent_mask.Get(cx, cy)), static_cast<int>(ContinentMask::Sample(continent_mask.glitch_only, cx, cy)),
                 static_cast<int>(continent_mask.RawGet(cx, cy)), static_cast<int>(continent_mask.AnyGroundAt(cx, cy)),
                 static_cast<int>(probe->complete));
        Log::Log("[carto-bake] (%d,%d) map=%d instance=%d rect_valid=%d rect_cells=[%d,%d)-[%d,%d) continent=%d mask_origin=(%d,%d) mask_size=%dx%d",
                 cx, cy, static_cast<int>(GW::Map::GetMapID()), static_cast<int>(GW::Map::GetInstanceType()), static_cast<int>(rect),
                 map_rect_min.first, map_rect_min.second, map_rect_max.first, map_rect_max.second,
                 continent_mask.continent, continent_mask.x0, continent_mask.y0, continent_mask.w, continent_mask.h);
        Log::Log("[carto-bake] (%d,%d) nav_grid origin=(%d,%d) size=%dx%d ground_cells=%d stand_cells=%d built=%d",
                 cx, cy, nav_cells.x0, nav_cells.y0, nav_cells.width, nav_cells.height,
                 nav_cells.ground_count, nav_cells.stand_count, static_cast<int>(nav_cells.built));
        // Reproduce the bake on this map's own DAT, for this one cell. The bake keeps only the
        // largest connected component, so ground the walk cannot reach is dropped from the table
        // even though you are standing on it - the other way a square you occupy reads as unexpected.
        {
            const uint32_t bake_fid = PathfindingWindow::GetMapFileId(GW::Map::GetMapID());
            Pathing::PathingMapData data;
            GW::GamePos ca{}, cb{};
            if (bake_fid && Pathing::LoadPathingMapDataFromDAT(bake_fid, &data)
                && WorldMapWidget::WorldMapToGamePos({cx * kWorldMapUnitsPerCell, cy * kWorldMapUnitsPerCell}, ca)
                && WorldMapWidget::WorldMapToGamePos({(cx + 1) * kWorldMapUnitsPerCell, (cy + 1) * kWorldMapUnitsPerCell}, cb)) {
                std::unordered_set<const GW::PathingTrapezoid*> blocked_comp, open_comp;
                PlayableTrapezoids(data, blocked_comp, open_comp);
                const GW::Vec2f box_min{std::min(ca.x, cb.x), std::min(ca.y, cb.y)};
                const GW::Vec2f box_max{std::max(ca.x, cb.x), std::max(ca.y, cb.y)};
                int overlap = 0, in_blocked = 0, in_open = 0;
                const GW::PathingTrapezoid* seed = nullptr;
                size_t total = 0;
                for (const auto& plane : data.planes) {
                    total += plane.trapezoid_count;
                    for (uint32_t t = 0; t < plane.trapezoid_count; t++) {
                        const auto* trap = &plane.trapezoids[t];
                        GW::Vec2f footing{};
                        if (!Pathing::TrapezoidOverlapsBox(trap, box_min, box_max, footing)) continue;
                        overlap++;
                        if (!seed) seed = trap;
                        if (blocked_comp.contains(trap)) in_blocked++;
                        if (open_comp.contains(trap)) in_open++;
                    }
                }
                if (seed && !in_open) {
                    // Which rule severed it: the "not used for path finding" portal flag, or nothing
                    // reachable at all. Planes are listed because a layered map's upper and lower
                    // levels are separate planes joined only by portals.
                    const auto strict = FloodFrom(data, seed, true);
                    const auto relaxed = FloodFrom(data, seed, false);
                    bool relaxed_reaches_main = false;
                    for (const auto* t : relaxed) {
                        if (open_comp.contains(t)) { relaxed_reaches_main = true; break; }
                    }
                    std::string zplanes;
                    for (size_t pi = 0; pi < data.planes.size(); pi++) {
                        const auto& plane = data.planes[pi];
                        bool mine = false;
                        for (uint32_t t = 0; t < plane.trapezoid_count && !mine; t++) mine = strict.contains(&plane.trapezoids[t]);
                        if (mine) zplanes += std::format("{}{}", zplanes.empty() ? "" : ",", plane.zplane);
                    }
                    Log::Log("[carto-rebake] (%d,%d) your piece: %u trapezoids as the bake walks it, %u with the "
                             "\"not used for path finding\" portal flag ignored; that relaxed walk %s the main component. "
                             "Planes it spans (zplane): %s of %u",
                             cx, cy, static_cast<unsigned>(strict.size()), static_cast<unsigned>(relaxed.size()),
                             relaxed_reaches_main ? "REACHES  <== the 0x04 portal flag is what severs it" : "still misses",
                             zplanes.c_str(), static_cast<unsigned>(data.planes.size()));
                }
                Log::Log("[carto-rebake] (%d,%d) file=0x%X cell box game (%.0f,%.0f)-(%.0f,%.0f): %d trapezoid(s) overlap it, "
                         "%d kept for normal play, %d kept as a playable area; %u/%u of %u trapezoids kept%s",
                         cx, cy, bake_fid, box_min.x, box_min.y, box_max.x, box_max.y, overlap, in_blocked, in_open,
                         static_cast<unsigned>(blocked_comp.size()), static_cast<unsigned>(open_comp.size()),
                         static_cast<unsigned>(total),
                         !overlap ? "  <== NO GEOMETRY HERE: the anchor or the overlap test, not the component filter"
                                  : !in_open ? "  <== NO PLAYABLE AREA CLAIMS IT: the bake dropped ground you are standing on"
                                             : "");
            }
        }
        // Both halves of the anchor the bake re-derives. GetMapWorldAnchor takes the loaded map's
        // bounds from the map context and every other map's from the DAT; bake.py only ever has the
        // DAT. If those two disagree for this map, every tile it bakes is shifted - which is the
        // shape "underworld" maps would take, their geometry sitting somewhere the map info does not
        // describe.
        {
            const auto* map_context = GW::GetMapContext();
            Pathing::Vec2f dat_min{}, dat_max{};
            const uint32_t file_id = PathfindingWindow::GetMapFileId(GW::Map::GetMapID());
            const bool have_dat = Pathing::GetMapGameBoundsFromDAT(file_id, dat_min, dat_max);
            if (map_context) {
                Log::Log("[carto-anchor] map=%d file=0x%X live bounds (%.1f,%.1f)-(%.1f,%.1f) dat bounds %s(%.1f,%.1f)-(%.1f,%.1f)",
                         static_cast<int>(GW::Map::GetMapID()), file_id,
                         map_context->start_pos.x, map_context->start_pos.y, map_context->end_pos.x, map_context->end_pos.y,
                         have_dat ? "" : "UNREADABLE ", dat_min.x, dat_min.y, dat_max.x, dat_max.y);
                // The anchor is built from min.x and max.y only, so those are the two that matter.
                if (have_dat) {
                    Log::Log("[carto-anchor]   anchor delta live-minus-dat = (%.4f, %.4f) world-map units%s",
                             (map_context->start_pos.x - dat_min.x) / -96.f, (map_context->end_pos.y - dat_max.y) / 96.f,
                             map_context->start_pos.x != dat_min.x || map_context->end_pos.y != dat_max.y
                                 ? "  <== the bake anchored this map somewhere else" : "");
                }
            }
        }
        // The whole "unexpected" verdict is the baked table disagreeing with ground that is really
        // there, and the live navmesh grid is the same trapezoids run through the same overlap test.
        // Laying the two over each other separates the two ways that can go wrong: a constant offset
        // means the bake's anchor and GetMapWorldAnchor have drifted apart, disagreement in place
        // means the bake is missing geometry. Every cell marked L is one that can turn purple.
        if (nav_cells.built && !continent_mask.Empty()) {
            const auto live_at = [&](const int x, const int y) {
                return nav_cells.ground[static_cast<size_t>(y) * nav_cells.width + x] != 0;
            };
            int best_dx = 0, best_dy = 0, best_hits = -1, in_place = 0;
            for (int oy = -4; oy <= 4; oy++) {
                for (int ox = -4; ox <= 4; ox++) {
                    int hits = 0;
                    for (int y = 0; y < nav_cells.height; y++) {
                        for (int x = 0; x < nav_cells.width; x++) {
                            if (live_at(x, y) && continent_mask.AnyGroundAt(nav_cells.x0 + x + ox, nav_cells.y0 + y + oy)) hits++;
                        }
                    }
                    if (!ox && !oy) in_place = hits;
                    if (hits > best_hits) { best_hits = hits; best_dx = ox; best_dy = oy; }
                }
            }
            Log::Log("[carto-bake] (%d,%d) live vs bake: %d/%d live ground cells baked in place; best offset (%+d,%+d) matches %d%s",
                     cx, cy, in_place, nav_cells.ground_count, best_dx, best_dy, best_hits,
                     best_dx || best_dy ? "  <== ANCHOR DRIFT: the bake and GetMapWorldAnchor disagree" : "");
            // # both, L live only (the bake is missing it), b baked only, . neither.
            for (int y = 0; y < nav_cells.height; y++) {
                std::string row;
                for (int x = 0; x < nav_cells.width; x++) {
                    const bool live = live_at(x, y);
                    const bool baked = continent_mask.AnyGroundAt(nav_cells.x0 + x, nav_cells.y0 + y);
                    row += live && baked ? '#' : live ? 'L' : baked ? 'b' : '.';
                }
                Log::Log("[carto-bake]   y=%3d x=%d %s", nav_cells.y0 + y, nav_cells.x0, row.c_str());
            }
        }
        // Which baked tile the dilated claim came from. Ground behind a travel portal shows as
        // glitch-only, which is the difference the setting turns on.
        for (int dy = -kMaskRadius; dy <= kMaskRadius; dy++) {
            for (int dx = -kMaskRadius; dx <= kMaskRadius; dx++) {
                const int bx = cx + dx, by = cy + dy;
                const bool any = continent_mask.AnyGroundAt(bx, by);
                if (!any && (dx || dy)) continue;
                Log::Log("[carto-bake]   (%+d,%+d) cell(%d,%d) stand=%d stand_glitched=%d%s",
                         dx, dy, bx, by, static_cast<int>(continent_mask.RawGet(bx, by)), static_cast<int>(any),
                         any && !continent_mask.RawGet(bx, by) ? "  <== only reachable by gate glitching" : "");
            }
        }
        // The other half, for when the bake defers: every cell the client would credit this tile
        // from, and what this map's navmesh probe made of it.
        const int r = RevealRadius();
        for (int dy = -r; dy <= r; dy++) {
            for (int dx = -r; dx <= r; dx++) {
                const bool creditable = CellCreditableFrom(dx, dy, cx, cy);
                const auto it = probe->cells.find({cx + dx, cy + dy});
                const bool probed = it != probe->cells.end();
                if (!creditable && !probed) continue;
                Log::Log("[carto-probe]   (%+d,%+d) cell(%d,%d) creditable=%d probed=%d in_nav_grid=%d navmesh=%d reachable=%d reveals=%d%s",
                         dx, dy, cx + dx, cy + dy, static_cast<int>(creditable), static_cast<int>(probed),
                         static_cast<int>(nav_cells.InGrid(cx + dx, cy + dy)),
                         probed ? static_cast<int>(it->second.navmesh) : 0,
                         probed ? static_cast<int>(it->second.reachable) : 0, probed ? it->second.reveals : 0,
                         creditable && probed && it->second.reachable ? "  <== would make it coverable" : "");
            }
        }
        const auto listed = std::ranges::find_if(uncoverable_cells, [&](const UncoverableCell& u) { return u.cx == cx && u.cy == cy; });
        {
            const auto self = GW::Agents::GetControlledCharacter();
            const auto doorways = Pathing::GetTravelDoorways();
            Log::Log("[carto-doorway] map=%d has %u travel doorways; player game(%.0f,%.0f)",
                     static_cast<int>(GW::Map::GetMapID()), static_cast<unsigned>(doorways.size()),
                     self ? self->pos.x : 0.f, self ? self->pos.y : 0.f);
            GW::GamePos target = gp;
            if (converted && Pathing::FindClosestPositionOnTrapezoid(target)) {
                for (const auto& d : doorways) {
                    const float dx = d.pos.x - target.x, dy = d.pos.y - target.y;
                    const bool blocks = self && Pathing::CrossesTravelPortal({self->pos.x, self->pos.y}, {target.x, target.y});
                    Log::Log("[carto-doorway]   doorway game(%.0f,%.0f) r=%.0f dist_to_ground=%.0f inside=%d segment_player_to_ground_blocked=%d",
                             d.pos.x, d.pos.y, sqrtf(d.radius_sq), sqrtf(dx * dx + dy * dy),
                             static_cast<int>(dx * dx + dy * dy < d.radius_sq), static_cast<int>(blocks));
                }
                Log::Log("[carto-doorway]   ground game(%.0f,%.0f) walkable=%d reachable=%d",
                         target.x, target.y, static_cast<int>(Pathing::IsPositionWalkable(target)),
                         static_cast<int>(Pathing::IsPositionReachable(target)));
            }
        }
        Log::Log("[carto-bake] (%d,%d) this_map_can_credit=%d (drives the red tooltip note)",
                 cx, cy, static_cast<int>(ThisMapCanCredit(cx, cy)));
        Log::Log("[carto-bake] (%d,%d) explored_tiles=%d unexpected_tiles=%d coverable_tiles=%d",
                 cx, cy, explored_tiles, unexpected_tiles, coverable_tiles);
        Log::Log("[carto-bake] (%d,%d) fog_cells=%u uncoverable_cells=%u drawn_grey=%d why=%d",
                 cx, cy, static_cast<unsigned>(fog_cells.size()), static_cast<unsigned>(uncoverable_cells.size()),
                 static_cast<int>(listed != uncoverable_cells.end()),
                 listed != uncoverable_cells.end() ? static_cast<int>(listed->why) : -1);
        Log::FlushFile();
    }
#endif

    // Elsewhere keeps the raw point on purpose: the quest marker turns a position outside this
    // map's rectangle into a travel marker, which is how the player reaches another map's fog.
    enum class GoalKind { None, Elsewhere, Waypoint, Stand };

    struct Target {
        bool valid = false;
        bool custom = false;
        // For fog targets these are the cell to go and stand in, not the cell being uncovered.
        int cx = 0;
        int cy = 0;
        int reveals = 0;
        GW::Vec2f wm{};
        // The stand cell is carried rather than re-derived from `stand_wm`: the arrival test
        // compares cells, and re-deriving one is a second chance to disagree with the client.
        GW::Vec2f stand_wm{};
        int stand_cx = 0;
        int stand_cy = 0;
        GoalKind goal = GoalKind::None;
    };

    GW::Constants::MapID state_map_id = static_cast<GW::Constants::MapID>(0);
    GW::Constants::InstanceType state_instance_type = GW::Constants::InstanceType::Loading;
    Target target;
    GW::Vec2f player_wm_cached{};
    clock_t last_scan = 0;
    clock_t arrived_at = 0;
    clock_t map_settled_at = 0;
    bool arrived = false;
    bool warned_no_data = false;
    bool warned_no_fog = false;

    // Where the player is being sent. Null means nothing reachable credits the target.
    const GW::Vec2f* TargetGoal()
    {
        if (!target.valid) return nullptr;
        if (!target.custom) return &target.wm;
        return target.goal == GoalKind::Stand ? &target.stand_wm
            : target.goal == GoalKind::Waypoint || target.goal == GoalKind::Elsewhere ? &target.wm
            : nullptr;
    }

    // The custom quest marker is shared with everything else that sets one, so remember the one we
    // placed and never touch a marker that has since become somebody else's.
    bool marker_placed = false;
    GW::Vec2f marker_point{};
    GW::Vec2f marker_goal{};

    bool MarkerStillOurs()
    {
        const auto* quest = QuestModule::GetCustomQuestMarker();
        GW::Vec2f wm;
        return quest && QuestModule::GetCustomQuestMarkerWorldPos(quest->quest_id, wm) && Dist2(wm, marker_goal) < 1.f;
    }

    void ReleaseQuestMarker()
    {
        const bool ours = marker_placed && MarkerStillOurs();
        marker_placed = false;
        if (ours) QuestModule::ClearCustomQuestMarker();
    }

    void SyncQuestMarker()
    {
        if (!set_quest_marker || !target.valid || !target.custom) {
            ReleaseQuestMarker();
            return;
        }
        const GW::Vec2f* goal = TargetGoal();
        if (!goal) {
            ReleaseQuestMarker();
            return;
        }
        // Outside the rectangle SetCustomQuestMarker degrades to a travel marker, which is right for
        // another map's fog and wrong for a square on this one - so leave that square unmarked.
        if (target.goal == GoalKind::Stand && !StandRoutable(*goal)) {
            if (!warned_stand_off_rect) {
                warned_stand_off_rect = true;
                CARTO_LOG("[cartographer] stand square at wm(%.0f, %.0f) sits outside this map's world-map rectangle; drawing it without a quest marker", goal->x, goal->y);
            }
            ReleaseQuestMarker();
            return;
        }
        if (marker_placed && Dist2(marker_point, target.wm) < 1.f) {
            // Same point: follow it only while the marker is still ours, so clearing it by hand sticks.
            if (Dist2(*goal, marker_goal) < 1.f || !MarkerStillOurs()) return;
        }
        else {
            ReleaseQuestMarker();
        }
        marker_placed = true;
        marker_point = target.wm;
        marker_goal = *goal;
        QuestModule::SetCustomQuestMarker(*goal, true);
    }

    void ClearTarget()
    {
        target = {};
        arrived = false;
        arrived_at = 0;
        SyncQuestMarker();
    }

    // A point on explored ground is a waypoint and routes to itself; fog nothing can credit has
    // nowhere to route at all.
    GoalKind ResolveGoal(const GW::Vec2f& point_wm, const GW::Vec2f& from, GW::Vec2f& goal_wm, std::pair<int, int>& goal_cell)
    {
        CartoGrid grid;
        const auto [fx, fy] = FogTileAt(point_wm);
        if (!GetCartoGrid(grid)) return GoalKind::None;
        if (grid.IsExplored(fx, fy)) return GoalKind::Waypoint;
        bool any_navmesh = false;
        if (ResolveStandWorldPos(point_wm, from, goal_wm, goal_cell, any_navmesh)) return GoalKind::Stand;
        return WorldMapWidget::GetMapIdForLocation(point_wm) == GW::Map::GetMapID() ? GoalKind::None : GoalKind::Elsewhere;
    }

    // Re-resolved every scan rather than kept: the sweep keeps learning what is standable, and a
    // gate moving can take the answer away again.
    void RefreshCustomTargetStand(const GW::Vec2f& from)
    {
        if (!target.valid || !target.custom) return;
        std::pair cell{0, 0};
        target.stand_wm = {};
        target.goal = ResolveGoal(target.wm, from, target.stand_wm, cell);
        target.stand_cx = cell.first;
        target.stand_cy = cell.second;
        SyncQuestMarker();
    }

    void ResetState()
    {
        target = {};
        arrived_at = 0;
        arrived = false;
        warned_no_data = false;
        warned_no_fog = false;
        warned_stand_off_rect = false;
        map_fog_cells = -1;
        fog_cells.clear();
        unreachable_fog_cells = 0;
        uncoverable_cells.clear();
        blocked_point = false;
        map_cell_min = map_cell_max = {};
        player_cell_valid = false;
        map_on_world_map = false;
        carto_snapshot.clear();
        coverage_stale = true;
    }

    void RemoveCustomPointAt(const GW::Vec2f& wm)
    {
        std::erase_if(custom_points, [&wm](const CustomPoint& p) { return Dist2(p.wm, wm) < 1.f; });
        SerializePoints();
    }

    void SkipTargetImpl(const bool forever)
    {
        if (!target.valid) return;
        if (target.custom) {
            CARTO_LOG("[cartographer] custom point (%.0f, %.0f) removed", target.wm.x, target.wm.y);
            RemoveCustomPointAt(target.wm);
        }
        else if (forever) {
            declined_cells.insert({target.cx, target.cy});
            SerializeDeclined();
            CARTO_LOG("[cartographer] stand cell (%d, %d) declined forever", target.cx, target.cy);
        }
        else {
            probe->skipped.insert({target.cx, target.cy});
            CARTO_LOG("[cartographer] stand cell (%d, %d) declined for this map", target.cx, target.cy);
        }
        ClearTarget();
    }

    void AddCustomPointImpl(const GW::Vec2f& raw_wm)
    {
        CartoGrid grid;
        const auto [fx, fy] = FogTileAt(raw_wm);
        // Credit is per tile, so where in it you clicked carries no information: snapping means two
        // clicks in one tile are one point, and every distance downstream is measured tile to tile.
        const GW::Vec2f wm = CreditCellCenterWorldMap(fx, fy);
        const bool foggy = GetCartoGrid(grid) && !grid.IsExplored(fx, fy);
        std::erase_if(custom_points, [&wm](const CustomPoint& p) { return FogTileAt(p.wm) == FogTileAt(wm); });
        custom_points.push_back({wm, foggy});
        SerializePoints();
        // Taking the target over straight away is what makes the marker point at the fog you just
        // asked about, instead of at whichever queued point happens to be nearest.
        target = {};
        target.valid = true;
        target.custom = true;
        target.wm = wm;
        arrived = false;
        RefreshCustomTargetStand(player_wm_cached);
#ifdef _DEBUG
        // One dump of the whole near ring: which square the client would credit from, what the probe
        // thinks of it, and how far it is. Without this the verdict is a single bit with no reason.
        Log::Log("[carto-ring] fog tile (%d, %d) wm(%.0f, %.0f) player wm(%.0f, %.0f) radius=%d\n",
                 fx, fy, wm.x, wm.y, player_wm_cached.x, player_wm_cached.y, RevealRadius());
        for (int dy = -kRevealRadius; dy <= kRevealRadius; dy++) {
            for (int dx = -kRevealRadius; dx <= kRevealRadius; dx++) {
                const std::pair cell{fx + dx, fy + dy};
                const auto it = probe->cells.find(cell);
                const auto centre = CreditCellCenterWorldMap(cell.first, cell.second);
                GW::GamePos gp{};
                const bool converted = WorldMapWidget::WorldMapToGamePos(centre, gp);
                Log::Log("[carto-ring]   (%+d,%+d) cell(%d,%d) centre wm(%.0f,%.0f) game(%.0f,%.0f) probed=%d navmesh=%d reachable=%d live_walkable=%d live_reachable=%d dist=%.0f%s\n",
                         dx, dy, cell.first, cell.second, centre.x, centre.y, gp.x, gp.y,
                         it != probe->cells.end(),
                         it != probe->cells.end() && it->second.navmesh,
                         it != probe->cells.end() && it->second.reachable,
                         converted && Pathing::IsPositionWalkable(gp),
                         converted && Pathing::IsPositionReachable(gp),
                         sqrtf(Dist2(centre, player_wm_cached)),
                         target.goal == GoalKind::Stand && cell == std::pair{target.stand_cx, target.stand_cy} ? "  <== CHOSEN" : "");
            }
        }
        Log::FlushFile();
#endif
        CARTO_LOG("[cartographer] custom fog point added at wm(%.0f, %.0f)%s", wm.x, wm.y,
                  target.goal == GoalKind::Stand ? ""
                  : target.goal == GoalKind::Waypoint ? " - already explored, marking the point itself"
                  : target.goal == GoalKind::Elsewhere ? " - no ground on this map is near it; travelling instead"
                  : " - this map has ground near it, but none reachable from here");
    }

    // Travel portals block here exactly as they block the live reachability walk: the baked table
    // and the overlay drawn from it have to answer the same question the same way.
    // Every walkable island in the file, largest first. A map file routinely holds more than one -
    // an outpost and its explorable share a file and you zone between them rather than walk - so
    // "the largest" is not the same thing as "the playable one".
    std::vector<std::unordered_set<const GW::PathingTrapezoid*>> AllComponents(const Pathing::PathingMapData& data, const bool block_gates)
    {
        const auto gates = block_gates ? Pathing::MakeTravelDoorways(data.portal_props) : std::vector<Pathing::TravelDoorway>{};
        std::unordered_map<const GW::PathingTrapezoid*, size_t> plane_of;
        std::vector<const GW::PathingTrapezoid*> all;
        for (size_t p = 0; p < data.planes.size(); p++) {
            const auto& plane = data.planes[p];
            for (uint32_t t = 0; t < plane.trapezoid_count; t++) {
                plane_of[&plane.trapezoids[t]] = p;
                all.push_back(&plane.trapezoids[t]);
            }
        }

        std::unordered_set<const GW::PathingTrapezoid*> seen;
        std::vector<std::unordered_set<const GW::PathingTrapezoid*>> out;
        for (const auto* root : all) {
            if (seen.contains(root)) continue;
            std::unordered_set<const GW::PathingTrapezoid*> component;
            std::vector<const GW::PathingTrapezoid*> queue{root};
            component.insert(root);
            seen.insert(root);
            for (size_t head = 0; head < queue.size(); head++) {
                const auto* trap = queue[head];
                const GW::Vec2f from = Pathing::TrapezoidCentre(trap);
                const auto expand = [&](const GW::PathingTrapezoid* next) {
                    if (!next || component.contains(next)) return;
                    if (Pathing::CrossesTravelDoorway(gates, from, Pathing::TrapezoidCentre(next))) return;
                    component.insert(next);
                    seen.insert(next);
                    queue.push_back(next);
                };
                for (const auto* adj : trap->adjacent) expand(adj);
                const auto it = plane_of.find(trap);
                if (it == plane_of.end() || it->second >= data.planes.size()) continue;
                const auto& plane = data.planes[it->second];
                const auto expand_portal = [&](const uint16_t idx) {
                    if (idx >= plane.portal_count) return;
                    const auto& portal = plane.portals[idx];
                    if (portal.flags & 0x04) return;
                    const auto* pair = portal.pair;
                    if (!pair) return;
                    for (uint32_t i = 0; i < pair->count; i++) expand(pair->trapezoids[i]);
                };
                expand_portal(trap->portal_left);
                expand_portal(trap->portal_right);
            }
            out.push_back(std::move(component));
        }
        std::ranges::sort(out, [](const auto& a, const auto& b) { return a.size() > b.size(); });
        return out;
    }

    std::unordered_set<const GW::PathingTrapezoid*> LargestComponent(const Pathing::PathingMapData& data, const bool block_gates)
    {
        auto all = AllComponents(data, block_gates);
        return all.empty() ? std::unordered_set<const GW::PathingTrapezoid*>{} : std::move(all.front());
    }

    // Flood from a set of trapezoids, with the given gates blocking. Shared so entrance seeding and
    // the component walk cannot drift apart.
    std::unordered_set<const GW::PathingTrapezoid*> FloodTrapezoids(const Pathing::PathingMapData& data,
                                                                    const std::vector<const GW::PathingTrapezoid*>& seeds,
                                                                    const std::vector<Pathing::TravelDoorway>& gates)
    {
        std::unordered_set<const GW::PathingTrapezoid*> comp;
        if (seeds.empty()) return comp;
        std::unordered_map<const GW::PathingTrapezoid*, size_t> plane_of;
        for (size_t p = 0; p < data.planes.size(); p++) {
            const auto& plane = data.planes[p];
            for (uint32_t t = 0; t < plane.trapezoid_count; t++) plane_of[&plane.trapezoids[t]] = p;
        }
        std::vector<const GW::PathingTrapezoid*> queue;
        for (const auto* seed : seeds) {
            if (seed && comp.insert(seed).second) queue.push_back(seed);
        }
        for (size_t head = 0; head < queue.size(); head++) {
            const auto* trap = queue[head];
            const GW::Vec2f from = Pathing::TrapezoidCentre(trap);
            const auto expand = [&](const GW::PathingTrapezoid* next) {
                if (!next || comp.contains(next)) return;
                if (Pathing::CrossesTravelDoorway(gates, from, Pathing::TrapezoidCentre(next))) return;
                comp.insert(next);
                queue.push_back(next);
            };
            for (const auto* adj : trap->adjacent) expand(adj);
            const auto it = plane_of.find(trap);
            if (it == plane_of.end() || it->second >= data.planes.size()) continue;
            const auto& plane = data.planes[it->second];
            const auto expand_portal = [&](const uint16_t idx) {
                if (idx >= plane.portal_count) return;
                const auto& portal = plane.portals[idx];
                if (portal.flags & 0x04 || !portal.pair) return;
                for (uint32_t i = 0; i < portal.pair->count; i++) expand(portal.pair->trapezoids[i]);
            };
            expand_portal(trap->portal_left);
            expand_portal(trap->portal_right);
        }
        return comp;
    }

    // Which trapezoids of a file a player can actually be standing on.
    //
    // Seeded at the gates you could have zoned in on, not at "the largest island". A map file is not
    // one connected world - it carries every zone placed on that rectangle, and you zone between them
    // rather than walk - so largest-only deletes whole playable areas (every outpost that shares its
    // explorable's file). But every-island-is-playable is the opposite error: it readmits walkable
    // terrain no gate leads to, which nothing can ever reach and which must stay uncoverable.
    //
    // The gate being seeded from does not block its own flood, matching CachedReachableTrapezoids in
    // Pathing.cpp. Unioned with the largest component because a map whose portal props sit off in a
    // side area would otherwise lose everything. Mirrors entrance_component in ffna.py.
    void PlayableTrapezoids(const Pathing::PathingMapData& data,
                            std::unordered_set<const GW::PathingTrapezoid*>& gated_out,
                            std::unordered_set<const GW::PathingTrapezoid*>& open_out)
    {
        const auto gates = Pathing::MakeTravelDoorways(data.portal_props);
        for (size_t i = 0; i < gates.size(); i++) {
            std::vector<const GW::PathingTrapezoid*> seeds;
            for (const auto& plane : data.planes) {
                for (uint32_t t = 0; t < plane.trapezoid_count; t++) {
                    const auto* trap = &plane.trapezoids[t];
                    const auto c = Pathing::TrapezoidCentre(trap);
                    const float dx = c.x - gates[i].pos.x, dy = c.y - gates[i].pos.y;
                    if (dx * dx + dy * dy < gates[i].radius_sq) seeds.push_back(trap);
                }
            }
            if (seeds.empty()) continue;
            std::vector<Pathing::TravelDoorway> others;
            for (size_t j = 0; j < gates.size(); j++) {
                if (j != i) others.push_back(gates[j]);
            }
            const auto comp = FloodTrapezoids(data, seeds, others);
            gated_out.insert(comp.begin(), comp.end());
        }
        const auto largest = LargestComponent(data, true);
        gated_out.insert(largest.begin(), largest.end());
        // The gate-glitch pair: the same ground walked as if a travel portal did not stop you.
        const std::vector<const GW::PathingTrapezoid*> seeds(gated_out.begin(), gated_out.end());
        open_out = FloodTrapezoids(data, seeds, {});
    }

    // GetMapIdForLocation walks every map on the continent, so answers are kept until the
    // continent changes. Rectangles overlap: this labels where a square is, not whose ground is on it.
    std::map<std::pair<int, int>, GW::Constants::MapID> tile_rect_map;

    GW::Constants::MapID MapForTile(const int cx, const int cy)
    {
        const auto [it, added] = tile_rect_map.try_emplace({cx, cy}, GW::Constants::MapID::None);
        if (added) it->second = WorldMapWidget::GetMapIdForLocation({(cx + .5f) * kWorldMapUnitsPerCell, (cy + .5f) * kWorldMapUnitsPerCell});
        return it->second;
    }

    std::string MapRectName(const int cx, const int cy)
    {
        const auto map_id = MapForTile(cx, cy);
        return map_id == GW::Constants::MapID::None ? std::string() : Resources::GetMapName(map_id)->string();
    }

    struct TileOwner {
        GW::Constants::MapID map_id = GW::Constants::MapID::None;
        GW::Constants::MapID travel_to = GW::Constants::MapID::None;
        uint32_t file_id = 0;
        bool connected = false;
        bool under_tile = false;
    };

    // Which map file holds the ground that credits a foggy square, resolved from the DAT because
    // the overlapping world-map rectangles routinely name a map that has no ground there at all.
    struct OwnerQuery {
        std::pair<int, int> cell{INT_MIN, INT_MIN};
        std::vector<GW::Constants::MapID> queue;
        size_t next = 0;
        std::vector<TileOwner> owners;
        int radius = kRevealRadius;
        int out_of_reach_maps = 0;
        bool unreadable = false;
        bool done = false;
    };
    OwnerQuery owner_query;
    std::map<std::pair<int, int>, OwnerQuery> owner_cache;
    int owner_cache_continent = -1;
    std::pair<int, int> owner_wanted{INT_MIN, INT_MIN};
    clock_t owner_wanted_since = 0;

    const OwnerQuery* FinishedOwnerQuery(const int cx, const int cy)
    {
        const auto it = owner_cache.find({cx, cy});
        return it == owner_cache.end() ? nullptr : &it->second;
    }

    void RequestOwnerQuery(const int cx, const int cy)
    {
        if (owner_wanted == std::pair{cx, cy}) return;
        owner_wanted = {cx, cy};
        owner_wanted_since = TIMER_INIT();
    }

    void StartOwnerQuery(const std::pair<int, int>& cell)
    {
        owner_query = {};
        owner_query.cell = cell;
        owner_query.radius = RevealRadius();
        const auto* here = GW::Map::GetMapInfo();
        if (!here) {
            owner_query.done = true;
            return;
        }
        const int r = owner_query.radius;
        std::set<uint32_t> file_ids;
        for (size_t i = 1; i < static_cast<size_t>(GW::Constants::MapID::Count); i++) {
            const auto map_id = static_cast<GW::Constants::MapID>(i);
            const auto info = GW::Map::GetMapInfo(map_id);
            if (!(info && info->GetIsOnWorldMap() && info->continent == here->continent)) continue;
            // A map's ground can sit outside its own rectangle, so the rectangle only bounds what it
            // may credit - test that, not whether it overlaps the reveal ring.
            if (!InCreditableBoundsOf(map_id, cell.first, cell.second)) {
                if (InCreditableBoundsOf(map_id, cell.first, cell.second, r + 1)) owner_query.out_of_reach_maps++;
                continue;
            }
            const uint32_t file_id = PathfindingWindow::GetMapFileId(map_id);
            if (!file_id || !file_ids.insert(file_id).second) continue;
            owner_query.queue.push_back(map_id);
        }
    }

    void ResolveOwnerCandidate(const GW::Constants::MapID map_id)
    {
        const uint32_t file_id = PathfindingWindow::GetMapFileId(map_id);
        Pathing::PathingMapData data;
        if (!Pathing::LoadPathingMapDataFromDAT(file_id, &data)) {
            owner_query.unreadable = true;
            return;
        }
        std::unordered_set<const GW::PathingTrapezoid*> component, unused;
        PlayableTrapezoids(data, component, unused);
        const int r = owner_query.radius;
        const auto [tx, ty] = owner_query.cell;
        TileOwner owner;
        bool any = false;
        for (const auto& plane : data.planes) {
            for (uint32_t t = 0; t < plane.trapezoid_count; t++) {
                const auto& trap = plane.trapezoids[t];
                GW::GamePos lo{}, hi{};
                lo.x = std::min(trap.XTL, trap.XBL);
                lo.y = trap.YB;
                hi.x = std::max(trap.XTR, trap.XBR);
                hi.y = trap.YT;
                GW::Vec2f a{}, b{};
                if (!WorldMapWidget::GamePosToWorldMap(lo, a, map_id)) continue;
                if (!WorldMapWidget::GamePosToWorldMap(hi, b, map_id)) continue;
                const auto [x0, y0] = FogTileAt({std::min(a.x, b.x), std::min(a.y, b.y)});
                const auto [x1, y1] = FogTileAt({std::max(a.x, b.x), std::max(a.y, b.y)});
                for (int cy = std::max(y0, ty - r); cy <= std::min(y1, ty + r); cy++) {
                    for (int cx = std::max(x0, tx - r); cx <= std::min(x1, tx + r); cx++) {
                        GW::GamePos ca{}, cb{};
                        if (!WorldMapWidget::WorldMapToGamePos({cx * kWorldMapUnitsPerCell, cy * kWorldMapUnitsPerCell}, ca, map_id)) continue;
                        if (!WorldMapWidget::WorldMapToGamePos({(cx + 1) * kWorldMapUnitsPerCell, (cy + 1) * kWorldMapUnitsPerCell}, cb, map_id)) continue;
                        GW::Vec2f footing{};
                        if (!Pathing::TrapezoidOverlapsBox(&trap, {std::min(ca.x, cb.x), std::min(ca.y, cb.y)},
                                                           {std::max(ca.x, cb.x), std::max(ca.y, cb.y)}, footing)) {
                            continue;
                        }
                        any = true;
                        owner.connected = owner.connected || component.contains(&trap);
                        owner.under_tile = owner.under_tile || (cx == tx && cy == ty);
                    }
                }
            }
        }
        if (!any) return;
        owner.map_id = map_id;
        owner.file_id = file_id;
        owner.travel_to = TravelWindow::GetNearestOutpost(map_id);
        owner_query.owners.push_back(owner);
    }

    // One map file per tick: parsing one out of the DAT is far too slow to loop over in a frame.
    void StepOwnerQuery()
    {
        if (owner_cache_continent != continent_mask.continent) {
            owner_cache_continent = continent_mask.continent;
            owner_cache.clear();
            owner_query = {};
            tile_rect_map.clear();
        }
        if (owner_wanted.first == INT_MIN) return;
        if (TIMER_DIFF(owner_wanted_since) < 200) return;
        if (owner_cache.contains(owner_wanted)) return;
        if (owner_query.cell != owner_wanted) StartOwnerQuery(owner_wanted);
        if (owner_query.done) return;
        if (owner_query.next < owner_query.queue.size()) {
            ResolveOwnerCandidate(owner_query.queue[owner_query.next++]);
            return;
        }
        owner_query.done = true;
        if (owner_cache.size() >= 512) owner_cache.clear();
        owner_cache[owner_query.cell] = owner_query;
    }

    // `why_lines` only where the square cannot be uncovered: there they say which exit dropped it.
    // On a square you can still clear they answer a question nobody asked, and "walkable: false"
    // over ground you can walk on reads as the overlay being wrong.
    std::string OwnerTooltip(const OwnerQuery& q, const bool why_lines)
    {
        std::string out;
        for (const auto& owner : q.owners) {
            const auto& name = Resources::GetMapName(owner.map_id)->string();
            const auto& travel = Resources::GetMapName(owner.travel_to)->string();
            if (!out.empty()) out += "\n";
            out += std::format("{} (map {}, file 0x{:X})", name.empty() ? "Unnamed map" : name.c_str(),
                               static_cast<int>(owner.map_id), owner.file_id);
            out += owner.travel_to == GW::Constants::MapID::None || travel.empty() ? "\nNo outpost travels there" : "\nTravel to " + travel;
            if (why_lines) {
                out += std::format("\nwalkable: {}\nexplorable: {}",
                                   owner.under_tile ? "true" : "false", owner.connected ? "true" : "false");
            }
        }
        return out + std::format("\nUnexplored square ({}, {})", q.cell.first, q.cell.second);
    }

#ifdef _DEBUG
    struct ContinentBake {
        // (cy << 32) | (uint32)cx, one pair per walk: gates blocking, gates open, and no walk at
        // all. The last is not a reachability claim - it is only "the file has ground here".
        std::unordered_set<uint64_t> standable, creditable;
        std::unordered_set<uint64_t> standable_glitched, creditable_glitched;
        std::unordered_set<uint64_t> standable_any, creditable_any;
        int maps = 0;
    };

    struct BakeState {
        bool running = false;
        size_t next = 0;
        std::vector<GW::Constants::MapID> queue;
        std::map<int, ContinentBake> continents;
        int on_world_map = 0;
        int no_file_id = 0;
        int area_fid_agrees = 0;
        int area_fid_differs = 0;
        int area_fid_missing = 0;
        int load_failed = 0;
        int no_bounds = 0;
        clock_t started = 0;
        std::string summary;
    };
    BakeState bake;

    uint64_t TileKey(const int cx, const int cy)
    {
        return static_cast<uint64_t>(static_cast<uint32_t>(cy)) << 32 | static_cast<uint32_t>(cx);
    }

    void BakeMap(const GW::Constants::MapID map_id, const GW::AreaInfo* info, const int continent)
    {
        const uint32_t file_id = PathfindingWindow::GetMapFileId(map_id);
        const uint32_t area_file_id = info ? info->file_id : 0;
        if (!area_file_id) bake.area_fid_missing++;
        else if (area_file_id == file_id) bake.area_fid_agrees++;
        else bake.area_fid_differs++;
        if (!file_id) {
            bake.no_file_id++;
            return;
        }
        Pathing::PathingMapData data;
        if (!Pathing::LoadPathingMapDataFromDAT(file_id, &data)) {
            bake.load_failed++;
            return;
        }
        std::unordered_set<const GW::PathingTrapezoid*> keep_gated, keep_open;
        PlayableTrapezoids(data, keep_gated, keep_open);
        auto& out = bake.continents[continent];
        std::unordered_set<uint64_t> mine, mine_glitched, mine_any;
        int marked = 0;
        for (const auto& plane : data.planes) {
            for (uint32_t t = 0; t < plane.trapezoid_count; t++) {
                const auto& trap = plane.trapezoids[t];
                GW::GamePos lo{}, hi{};
                lo.x = std::min(trap.XTL, trap.XBL);
                lo.y = trap.YB;
                hi.x = std::max(trap.XTR, trap.XBR);
                hi.y = trap.YT;
                GW::Vec2f a{}, b{};
                if (!WorldMapWidget::GamePosToWorldMap(lo, a, map_id)) continue;
                if (!WorldMapWidget::GamePosToWorldMap(hi, b, map_id)) continue;
                const auto [x0, y0] = FogTileAt({std::min(a.x, b.x), std::min(a.y, b.y)});
                const auto [x1, y1] = FogTileAt({std::max(a.x, b.x), std::max(a.y, b.y)});
                for (int cy = y0; cy <= y1; cy++) {
                    for (int cx = x0; cx <= x1; cx++) {
                        GW::GamePos ca{}, cb{};
                        if (!WorldMapWidget::WorldMapToGamePos({cx * kWorldMapUnitsPerCell, cy * kWorldMapUnitsPerCell}, ca, map_id)) continue;
                        if (!WorldMapWidget::WorldMapToGamePos({(cx + 1) * kWorldMapUnitsPerCell, (cy + 1) * kWorldMapUnitsPerCell}, cb, map_id)) continue;
                        GW::Vec2f footing{};
                        if (!Pathing::TrapezoidOverlapsBox(&trap, {std::min(ca.x, cb.x), std::min(ca.y, cb.y)},
                                                           {std::max(ca.x, cb.x), std::max(ca.y, cb.y)}, footing)) {
                            continue;
                        }
                        const auto key = TileKey(cx, cy);
                        mine_any.insert(key);
                        out.standable_any.insert(key);
                        if (keep_open.contains(&trap)) {
                            mine_glitched.insert(key);
                            out.standable_glitched.insert(key);
                        }
                        if (keep_gated.contains(&trap)) {
                            mine.insert(key);
                            if (out.standable.insert(key).second) marked++;
                        }
                    }
                }
            }
        }
        // Dilated here rather than at runtime because credit stops one square past THIS map's
        // rectangle, and a flat continent bitmap cannot say which map a tile's ground belongs to.
        const auto dilate = [&](const std::unordered_set<uint64_t>& src, std::unordered_set<uint64_t>& dst) {
            for (const auto key : src) {
                const int sx = static_cast<int>(static_cast<uint32_t>(key & 0xffffffff));
                const int sy = static_cast<int>(static_cast<uint32_t>(key >> 32));
                for (int dy = -kMaskRadius; dy <= kMaskRadius; dy++) {
                    for (int dx = -kMaskRadius; dx <= kMaskRadius; dx++) {
                        if (InCreditableBoundsOf(map_id, sx + dx, sy + dy)) dst.insert(TileKey(sx + dx, sy + dy));
                    }
                }
            }
        };
        dilate(mine, out.creditable);
        dilate(mine_glitched, out.creditable_glitched);
        dilate(mine_any, out.creditable_any);
        out.maps++;
        CARTO_LOG("[carto-bake] map %d (file 0x%X, continent %d): %d planes, +%d tiles",
                  static_cast<int>(map_id), file_id, continent, static_cast<int>(data.planes.size()), marked);
    }

    void WriteTileSet(const std::unordered_set<uint64_t>& tiles, const int continent, const wchar_t* kind, const char* magic)
    {
        if (tiles.empty()) return;
        int x0 = INT_MAX, y0 = INT_MAX, x1 = INT_MIN, y1 = INT_MIN;
        for (const auto key : tiles) {
            const int cx = static_cast<int>(static_cast<uint32_t>(key & 0xffffffff));
            const int cy = static_cast<int>(static_cast<uint32_t>(key >> 32));
            x0 = std::min(x0, cx);
            x1 = std::max(x1, cx);
            y0 = std::min(y0, cy);
            y1 = std::max(y1, cy);
        }
        const int w = x1 - x0 + 1;
        const int h = y1 - y0 + 1;
        std::vector<uint8_t> bits((static_cast<size_t>(w) * h + 7) / 8, 0);
        for (const auto key : tiles) {
            const int cx = static_cast<int>(static_cast<uint32_t>(key & 0xffffffff)) - x0;
            const int cy = static_cast<int>(static_cast<uint32_t>(key >> 32)) - y0;
            const size_t bit = static_cast<size_t>(cy) * w + cx;
            bits[bit >> 3] |= 1 << (bit & 7);
        }
        const int32_t header[5] = {continent, x0, y0, w, h};
        const auto path = Resources::GetPath(L"cartography", std::format(L"{}_L{}.bin", kind, continent));
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        std::ofstream file(path, std::ios::binary);
        if (!file) {
            CARTO_LOG("[carto-bake] could not write %ls", path.wstring().c_str());
            return;
        }
        file.write(magic, 4);
        file.write(reinterpret_cast<const char*>(header), sizeof(header));
        file.write(reinterpret_cast<const char*>(bits.data()), static_cast<std::streamsize>(bits.size()));
        CARTO_LOG("[carto-bake] continent %d %ls: %u tiles, grid %dx%d at (%d,%d), %u bytes",
                  continent, kind, static_cast<unsigned>(tiles.size()), w, h, x0, y0,
                  static_cast<unsigned>(bits.size()));
    }

    void WriteBakeFiles()
    {
        for (const auto& [continent, data] : bake.continents) {
            WriteTileSet(data.standable, continent, L"standable", "CSM1");
            WriteTileSet(data.creditable, continent, L"creditable", "CCM1");
            WriteTileSet(data.standable_glitched, continent, L"standable_glitched", "CSG1");
            WriteTileSet(data.creditable_glitched, continent, L"creditable_glitched", "CCG1");
            WriteTileSet(data.standable_any, continent, L"standable_any", "CSA1");
            WriteTileSet(data.creditable_any, continent, L"creditable_any", "CCA1");
        }
    }

    void StartBake()
    {
        bake = {};
        bake.started = TIMER_INIT();
        for (size_t i = 1; i < static_cast<size_t>(GW::Constants::MapID::Count); i++) {
            const auto map_id = static_cast<GW::Constants::MapID>(i);
            const auto info = GW::Map::GetMapInfo(map_id);
            if (!(info && info->GetIsOnWorldMap())) continue;
            bake.on_world_map++;
            ImRect bounds;
            if (!GW::Map::GetMapWorldMapBounds(info, &bounds)) {
                bake.no_bounds++;
                continue;
            }
            bake.queue.push_back(map_id);
        }
        bake.running = true;
        bake.summary = std::format("{} maps on the world map, queued", bake.queue.size());
    }

    // One map per tick: a DAT parse is far too slow to loop over ~450 of them in a frame.
    void StepBake()
    {
        if (!bake.running) return;
        if (bake.next >= bake.queue.size()) {
            WriteBakeFiles();
            bake.running = false;
            unsigned tiles = 0;
            for (const auto& [continent, data] : bake.continents) tiles += static_cast<unsigned>(data.standable.size());
            bake.summary = std::format("done in {:.1f}s: {} continents, {} tiles, {} maps with no file id, {} failed to load, {} without bounds",
                                       TIMER_DIFF(bake.started) / 1000.f, bake.continents.size(), tiles,
                                       bake.no_file_id, bake.load_failed, bake.no_bounds);
            CARTO_LOG("[carto-bake] %s", bake.summary.c_str());
            return;
        }
        const auto map_id = bake.queue[bake.next++];
        const auto* info = GW::Map::GetMapInfo(map_id);
        if (info) BakeMap(map_id, info, static_cast<int>(info->continent));
        bake.summary = std::format("{}/{} maps...", bake.next, bake.queue.size());
    }
#endif


    void OnCartographyUpdated(GW::HookStatus*, GW::UI::UIMessage, void*, void*)
    {
        carto_dirty = true;
    }

    void BuildStatusText(char* buf, const size_t len)
    {
        static constexpr const char* no_goal = "no reachable square credits your fog point - right-click it on the map to remove it";
        if (!target.valid) {
            const char* idle = blocked_point ? no_goal
                : map_fog_cells == 0 ? "nothing left to uncover on this map"
                : !probe->complete ? "scanning this map's fog..."
                : "nothing reachable left here - travel on, or add a fog point";
            snprintf(buf, len, "%s", idle);
            return;
        }
        if (arrived) {
            snprintf(buf, len, target.custom
                         ? "standing in the square for your fog point - if it does not register, take a step or click-walk"
                         : "standing in the right tile - if it does not register, take a step or click-walk");
            return;
        }
        const GW::Vec2f* goal = TargetGoal();
        if (!goal) {
            snprintf(buf, len, "%s", no_goal);
            return;
        }
        const float dist_k = sqrtf(Dist2(player_wm_cached, *goal)) * kGwinchesPerWorldMapUnit / 1000.f;
        if (target.custom) {
            if (target.goal == GoalKind::Stand) {
                snprintf(buf, len, "stand in the square %.1fk units %s of you to uncover your fog point", dist_k,
                         CompassDir(player_wm_cached, *goal));
                return;
            }
            snprintf(buf, len, "heading to your fog point, %.1fk units %s of you%s", dist_k,
                     CompassDir(player_wm_cached, *goal),
                     target.goal == GoalKind::Elsewhere ? " (another map)" : "");
            return;
        }
        snprintf(buf, len, "stand in the square %.1fk units %s of you to uncover %d %s%s", dist_k,
                 CompassDir(player_wm_cached, target.wm), target.reveals, target.reveals == 1 ? "square" : "squares",
                 blocked_point ? " (a fog point here credits nothing reachable)" : "");
    }

    int FindCustomPointNear(const GW::Vec2f& wm, const float max_dist)
    {
        int best = -1;
        float best_d2 = max_dist * max_dist;
        for (size_t i = 0; i < custom_points.size(); i++) {
            const float d2 = Dist2(custom_points[i].wm, wm);
            if (d2 <= best_d2) {
                best_d2 = d2;
                best = static_cast<int>(i);
            }
        }
        return best;
    }

    void PruneUncoveredPoints(const CartoGrid& grid)
    {
        const size_t before = custom_points.size();
        std::erase_if(custom_points, [&grid](const CustomPoint& p) {
            const auto [fx, fy] = FogTileAt(p.wm);
            return p.was_fog && grid.IsExplored(fx, fy);
        });
        if (custom_points.size() == before) return;
        SerializePoints();
        CARTO_LOG("[cartographer] %u fog point(s) uncovered, removed", static_cast<unsigned>(before - custom_points.size()));
        if (target.valid && target.custom && FindCustomPointNear(target.wm, 1.f) < 0) ClearTarget();
    }

    bool ContextMenuItems(const GW::Vec2f& click_wm, const float px_per_wm_unit)
    {
        if (!CartographerWidget::GetEnabled()) return true;
        bool keep_open = true;
        ImGui::PushID("carto_ctx");
        ImGui::Separator();
        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0, 0));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        const ImVec2 item_size = {250.f * ImGui::FontScale(), 0.f};
        {
            ImGui::TextColored(ImColor(kTargetColor).Value, ICON_FA_MAP_MARKED_ALT " Cartographer");
            char status[224];
            BuildStatusText(status, sizeof(status));
            ImGui::TextDisabled("%s", status);
            if (map_fog_cells > 0) ImGui::TextDisabled("%d squares left to uncover on this map", map_fog_cells);

            const float near_dist = px_per_wm_unit > 0.f ? 12.f / px_per_wm_unit : 8.f;
            const int point_here = FindCustomPointNear(click_wm, near_dist);
            // Clicking the suggestion itself means acting on it — offering to drop a fog point
            // on the very spot already being suggested is nonsense.
            const bool on_suggestion = target.valid && !target.custom
                && CreditCellAt(click_wm) == std::pair{target.cx, target.cy};
            if (target.valid && (on_suggestion || (target.custom && point_here >= 0))) {
                if (ImGui::Button(target.custom ? "Remove this fog point" : "Skip this suggestion", item_size)) {
                    CartographerWidget::SkipCurrentTarget(false);
                    keep_open = false;
                }
                if (!target.custom && ImGui::Button("Never suggest this spot again", item_size)) {
                    CartographerWidget::SkipCurrentTarget(true);
                    keep_open = false;
                }
            }
            else if (point_here >= 0) {
                if (ImGui::Button("Remove fog point", item_size)) {
                    CartographerWidget::RemoveCustomPointNear(click_wm, near_dist);
                    keep_open = false;
                }
            }
            else {
                if (ImGui::Button("Add fog point here", item_size)) {
                    CartographerWidget::AddCustomPoint(click_wm);
                    keep_open = false;
                }
            }
            if (custom_points.size() > 1) {
                char label[48];
                snprintf(label, sizeof(label), "Clear all %u fog points", static_cast<unsigned>(custom_points.size()));
                if (ImGui::Button(label, item_size)) {
                    CartographerWidget::ClearCustomPoints();
                    keep_open = false;
                }
            }
#ifdef _DEBUG
            if (ImGui::Button("Log what the helper sees here", item_size)) {
                const GW::Vec2f at = click_wm;
                GW::GameThread::Enqueue([at] { LogProbe(at); });
                keep_open = false;
            }
#endif
        }
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
        ImGui::PopID();
        return keep_open;
    }

    bool OnMissionMapContextMenu()
    {
        if (!map_on_world_map) return true;
        return ContextMenuItems(MissionMapWidget::GetContextMenuWorldMapPos(), MissionMapWidget::GetPxPerWorldMapUnit());
    }

    bool OnWorldMapContextMenu()
    {
        if (!map_on_world_map) return true;
        return ContextMenuItems(WorldMapWidget::GetContextMenuWorldMapPos(), WorldMapWidget::GetPxPerWorldMapUnit());
    }

    void DrawFogPointMarker(ImDrawList* dl, const ImVec2& c, const bool is_current_target)
    {
        constexpr float r = 6.f;
        const ImVec2 pts[4] = {{c.x, c.y - r}, {c.x + r, c.y}, {c.x, c.y + r}, {c.x - r, c.y}};
        dl->AddConvexPolyFilled(pts, 4, kFogPointColor);
        dl->AddPolyline(pts, 4, IM_COL32(10, 30, 40, 230), ImDrawFlags_Closed, 1.5f);
        if (is_current_target) {
            dl->AddCircle(c, r + 3.f + 3.f * Pulse(), WithAlpha(kFogPointColor, 200), 0, 2.f);
        }
    }

    using ProjectToScreen = bool(*)(const GW::Vec2f&, ImVec2&);

    bool ProjectCell(const ProjectToScreen project, const int cx, const int cy, ImVec2& min_out, ImVec2& max_out)
    {
        return project({cx * kWorldMapUnitsPerCell, cy * kWorldMapUnitsPerCell}, min_out) &&
            project({(cx + 1) * kWorldMapUnitsPerCell, (cy + 1) * kWorldMapUnitsPerCell}, max_out);
    }

    // One quad per fog texel, so ImGui interpolates them as the GPU does when it samples the
    // client's texture.
    void DrawFog(ImDrawList* dl, const ProjectToScreen project, const ImVec2& mouse,
                 std::string& tooltip_out, std::string& warning_out)
    {
        const ImRect clip(dl->GetClipRectMin(), dl->GetClipRectMax());
        const FogCell* hovered = nullptr;
        for (const auto& f : fog_cells) {
            ImVec2 cell_min, cell_max;
            if (!ProjectCell(project, f.cx, f.cy, cell_min, cell_max)) continue;
            if (!clip.Overlaps(ImRect(cell_min, cell_max))) continue;
            if (ImRect(cell_min, cell_max).Contains(mouse)) hovered = &f;
            const float w = (cell_max.x - cell_min.x) / kFogSubdivisions;
            const float h = (cell_max.y - cell_min.y) / kFogSubdivisions;
            for (int j = 0; j < kFogSubdivisions; j++) {
                for (int i = 0; i < kFogSubdivisions; i++) {
                    const ImVec2 t_min{cell_min.x + i * w, cell_min.y + j * h};
                    const ImVec2 t_max{t_min.x + w, t_min.y + h};
                    dl->AddRectFilledMultiColor(t_min, t_max,
                                                WithAlpha(kFogColor, f.corner_alpha[j][i]), WithAlpha(kFogColor, f.corner_alpha[j][i + 1]),
                                                WithAlpha(kFogColor, f.corner_alpha[j + 1][i + 1]), WithAlpha(kFogColor, f.corner_alpha[j + 1][i]));
                }
            }
        }
        if (!hovered) return;
        // Only in the map whose world-map rectangle the square falls in. Anywhere else "not from the
        // currently loaded map" is trivially true of nearly every square and says nothing.
        if (InMapBounds(hovered->cx, hovered->cy) && !ThisMapCanCredit(hovered->cx, hovered->cy)) {
            warning_out = "Not uncoverable from the currently loaded map";
        }
        RequestOwnerQuery(hovered->cx, hovered->cy);
        const auto* resolved = FinishedOwnerQuery(hovered->cx, hovered->cy);
        if (!resolved) {
            tooltip_out = std::format("Unexplored square ({}, {})\nReading the map files to find the ground that credits it...", hovered->cx, hovered->cy);
            return;
        }
        if (!resolved->owners.empty()) {
            tooltip_out = OwnerTooltip(*resolved, false);
            return;
        }
        tooltip_out = std::format("Unexplored square ({}, {})\nNo map that could credit it has ground within reveal range", hovered->cx, hovered->cy);
        const auto rect_name = MapRectName(hovered->cx, hovered->cy);
        if (!rect_name.empty()) tooltip_out += "\nThe world map rectangle it falls in belongs to " + rect_name;
        if (resolved->out_of_reach_maps) {
            tooltip_out += std::format("\n{} nearby map(s) skipped: this square is more than one past their boundary, which is as far as a map can credit", resolved->out_of_reach_maps);
        }
        if (resolved->unreadable) tooltip_out += "\nSome of those map files are not in your Gw.dat yet";
    }

    // The cartography grid itself. Every tile is credited as a unit, so seeing the boundaries is
    // what makes "stand in that tile" actionable.
    void DrawGrid(ImDrawList* dl, const ProjectToScreen project)
    {
        const auto [x0, y0] = map_cell_min;
        const auto [x1, y1] = map_cell_max;
        if (x1 <= x0 || y1 <= y0) return;
        ImVec2 origin, corner;
        if (!ProjectCell(project, x0, y0, origin, corner)) return;
        const float step_x = corner.x - origin.x;
        const float step_y = corner.y - origin.y;
        if (step_x < 3.f || step_y < 3.f) return; // denser than this is a smear, not a grid
        // Both projections are affine, so the far edge follows from the step rather than a second
        // projection that could fail on its own.
        const ImRect clip(dl->GetClipRectMin(), dl->GetClipRectMax());
        const float top = std::max(origin.y, clip.Min.y);
        const float bottom = std::min(origin.y + (y1 - y0) * step_y, clip.Max.y);
        const float left = std::max(origin.x, clip.Min.x);
        const float right = std::min(origin.x + (x1 - x0) * step_x, clip.Max.x);
        for (int cx = x0; cx <= x1; cx++) {
            const float x = origin.x + (cx - x0) * step_x;
            if (x >= clip.Min.x && x <= clip.Max.x) dl->AddLine({x, top}, {x, bottom}, kGridColor);
        }
        for (int cy = y0; cy <= y1; cy++) {
            const float y = origin.y + (cy - y0) * step_y;
            if (y >= clip.Min.y && y <= clip.Max.y) dl->AddLine({left, y}, {right, y}, kGridColor);
        }
        if (step_x < 12.f || step_y < 12.f) return;
        for (int cy = y0; cy < y1; cy++) {
            const float y = origin.y + (cy - y0 + 0.5f) * step_y;
            if (y < clip.Min.y || y > clip.Max.y) continue;
            for (int cx = x0; cx < x1; cx++) {
                const float x = origin.x + (cx - x0 + 0.5f) * step_x;
                if (x >= clip.Min.x && x <= clip.Max.x) dl->AddCircleFilled({x, y}, 1.5f, kGridDotColor);
            }
        }
    }

    // Foggy squares no ground can credit, drawn rather than silently omitted: an empty patch of world
    // map reads as "already done", which is the wrong answer to "is there anything left". Grey for
    // never, yellow for the ones only a gate glitch reaches.
    void DrawUncoverableCells(ImDrawList* dl, const ProjectToScreen project, const ImVec2& mouse, std::string& tooltip_out)
    {
        const ImRect clip(dl->GetClipRectMin(), dl->GetClipRectMax());
        for (const auto& [cx, cy, why] : uncoverable_cells) {
            ImVec2 cell_min, cell_max;
            if (!ProjectCell(project, cx, cy, cell_min, cell_max)) continue;
            if (!clip.Overlaps(ImRect(cell_min, cell_max))) continue;
            const auto colour = why == FogSkip::GlitchOnly ? kGlitchOnlyColor : kUncoverableColor;
            dl->AddRectFilled(cell_min, cell_max, WithAlpha(colour, 60));
            dl->AddRect(cell_min, cell_max, WithAlpha(colour, 150), 0.f, 0, 1.f);
            if (!ImRect(cell_min, cell_max).Contains(mouse)) continue;
            // Which map the ground belongs to is the answer worth having, so run the same DAT
            // lookup the fog tooltip does rather than describing the rule that excluded it.
            RequestOwnerQuery(cx, cy);
            const auto* resolved = FinishedOwnerQuery(cx, cy);
            if (!resolved) {
                tooltip_out = std::format("Unexplored square ({}, {})\nReading the map files to find the ground that credits it...", cx, cy);
                continue;
            }
            if (!resolved->owners.empty()) {
                tooltip_out = OwnerTooltip(*resolved, true);
                continue;
            }
            tooltip_out = std::format("Unexplored square ({}, {})\nNo map has ground within {} square(s) of it", cx, cy, RevealRadius());
            const auto rect_name = MapRectName(cx, cy);
            if (!rect_name.empty()) tooltip_out += "\nIts world map rectangle belongs to " + rect_name;
        }
    }

    // Explored squares the baked table says nothing could have credited: either ground the bake
    // missed, or ground reached in a way the bake does not model.
    void DrawUnexpectedCells(ImDrawList* dl, const ProjectToScreen project, const ImVec2& mouse, std::string& tooltip_out)
    {
        const ImRect clip(dl->GetClipRectMin(), dl->GetClipRectMax());
        for (const auto& [cx, cy] : unexpected_cells) {
            ImVec2 cell_min, cell_max;
            if (!ProjectCell(project, cx, cy, cell_min, cell_max)) continue;
            if (!clip.Overlaps(ImRect(cell_min, cell_max))) continue;
            dl->AddRectFilled(cell_min, cell_max, WithAlpha(kUnexpectedColor, 40));
            dl->AddRect(cell_min, cell_max, WithAlpha(kUnexpectedColor, 190), 0.f, 0, 1.f);
            if (!ImRect(cell_min, cell_max).Contains(mouse)) continue;
            const auto rect_name = MapRectName(cx, cy);
            tooltip_out = std::format("Explored square ({}, {}) with no baked ground within {} squares of it", cx, cy, RevealRadius());
            tooltip_out += "\nThe world map rectangle it falls in belongs to " + rect_name;
        }
    }

    // Drawn at true 32x32 size, shaded by how much fog the spot would credit.
    void DrawStandCells(ImDrawList* dl, const ProjectToScreen project, const ImVec2& mouse, const char*& tooltip)
    {
        const ImRect clip(dl->GetClipRectMin(), dl->GetClipRectMax());
        for (const auto& [cell, sc] : probe->cells) {
            if (!sc.reachable || sc.reveals <= 0) continue;
            if (declined_cells.contains(cell)) continue;
            // Skipped only while the suggestion is actually drawn on top, else a pending ownership
            // recheck blanks the square entirely.
            if (target.valid && !target.custom && target.cx == cell.first && target.cy == cell.second) continue;
            ImVec2 cell_min, cell_max;
            if (!ProjectCell(project, cell.first, cell.second, cell_min, cell_max)) continue;
            if (!clip.Overlaps(ImRect(cell_min, cell_max))) continue;
            const int strength = std::min(sc.reveals, 9);
            dl->AddRectFilled(cell_min, cell_max, WithAlpha(kStandColor, 10 + 6 * strength));
            dl->AddRect(cell_min, cell_max, WithAlpha(kStandColor, 60 + 12 * strength), 0.f, 0, 1.f);
            if (ImRect(cell_min, cell_max).Contains(mouse)) {
                tooltip = "Cartographer: stand here to uncover nearby squares";
            }
        }
    }

    void DrawMapOverlay(ImDrawList* dl, const ProjectToScreen project, const bool cell_tooltip)
    {
        const ImVec2 mouse = ImGui::GetMousePos();
        const char* tooltip = nullptr;
        std::string fog_tooltip;
        std::string fog_warning;
        if (show_fog) {
            DrawFog(dl, project, mouse, fog_tooltip, fog_warning);
        }
        if (show_grid) {
            DrawGrid(dl, project);
        }
        if (show_uncoverable) {
            DrawUncoverableCells(dl, project, mouse, fog_tooltip);
        }
        if (show_unexpected) {
            DrawUnexpectedCells(dl, project, mouse, fog_tooltip);
        }
        if (show_stand_cells) {
            const char* stand_tooltip = nullptr;
            DrawStandCells(dl, project, mouse, stand_tooltip);
            if (cell_tooltip) tooltip = stand_tooltip;
        }
        // Which tile you are standing in is the question the whole thing turns on, and it is not
        // answerable from the character marker alone - the ranges key off the tile, not the dot.
        if (player_cell_valid) {
            ImVec2 cell_min, cell_max;
            if (ProjectCell(project, player_cell.first, player_cell.second, cell_min, cell_max)) {
                dl->AddRectFilled(cell_min, cell_max, WithAlpha(kCurrentTileColor, 28));
                dl->AddRect(cell_min, cell_max, WithAlpha(kCurrentTileColor, 150), 0.f, 0, 1.f);
            }
        }
        const bool target_active = target.valid;
        if (target_active && !target.custom) {
            ImVec2 cell_min, cell_max;
            if (ProjectCell(project, target.cx, target.cy, cell_min, cell_max)) {
                const float pulse = Pulse();
                dl->AddRectFilled(cell_min, cell_max, WithAlpha(kTargetColor, 16 + static_cast<int>(24.f * pulse)));
                dl->AddRect(cell_min, cell_max, WithAlpha(kTargetColor, 210), 0.f, 0, 1.5f + pulse);
                if (cell_tooltip && ImRect(cell_min, cell_max).Contains(mouse)) {
                    tooltip = "Cartographer: stand in this square to uncover the fog around it\nRight-click the map for options";
                }
            }
        }
        // The tile that credits the fog point, drawn with a leader back to the point itself so it
        // is obvious the square is not where the fog is.
        if (target_active && target.custom && target.goal == GoalKind::Stand) {
            const int scx = target.stand_cx;
            const int scy = target.stand_cy;
            ImVec2 cell_min, cell_max, point_at;
            if (ProjectCell(project, scx, scy, cell_min, cell_max)) {
                const float pulse = Pulse();
                dl->AddRectFilled(cell_min, cell_max, WithAlpha(kTargetColor, 16 + static_cast<int>(24.f * pulse)));
                dl->AddRect(cell_min, cell_max, WithAlpha(kTargetColor, 210), 0.f, 0, 1.5f + pulse);
                if (project(target.wm, point_at)) {
                    dl->AddLine({(cell_min.x + cell_max.x) * .5f, (cell_min.y + cell_max.y) * .5f}, point_at, WithAlpha(kFogPointColor, 140), 1.f);
                }
                if (cell_tooltip && ImRect(cell_min, cell_max).Contains(mouse)) {
                    tooltip = "Cartographer: stand in this square to uncover your fog point";
                }
            }
        }
        for (const auto& p : custom_points) {
            ImVec2 c;
            if (!project(p.wm, c)) continue;
            const bool is_current = target_active && target.custom && Dist2(target.wm, p.wm) < 1.f;
            DrawFogPointMarker(dl, c, is_current);
            const float mdx = mouse.x - c.x;
            const float mdy = mouse.y - c.y;
            if (mdx * mdx + mdy * mdy < 12.f * 12.f) {
                tooltip = "Cartographer fog point\nRight-click nearby to remove it";
            }
        }
        if (tooltip) ImGui::SetTooltip("%s", tooltip);
        else if (cell_tooltip && !fog_tooltip.empty()) {
            ImGui::BeginTooltip();
            ImGui::TextUnformatted(fog_tooltip.c_str());
            if (!fog_warning.empty()) ImGui::TextColored(ImVec4(1.f, 0.35f, 0.35f, 1.f), "%s", fog_warning.c_str());
            ImGui::EndTooltip();
        }
    }

    void OnWorldMapOverlayDraw(ImDrawList* dl)
    {
        if (!CartographerWidget::GetEnabled() || !map_on_world_map) return;
        DrawMapOverlay(dl, [](const GW::Vec2f& wm, ImVec2& out) { return WorldMapWidget::WorldMapToScreen(wm, out); }, true);
        char status[224];
        BuildStatusText(status, sizeof(status));
        char line[256];
        snprintf(line, sizeof(line), ICON_FA_MAP_MARKED_ALT " Cartographer: %s", status);
        dl->AddText({16.f, dl->GetClipRectMax().y - 68.f}, ImGui::GetColorU32(ImGuiCol_Text), line);
    }

    void OnMissionMapOverlayDraw(ImDrawList* dl)
    {
        if (!CartographerWidget::GetEnabled() || !map_on_world_map) return;
        DrawMapOverlay(dl, [](const GW::Vec2f& wm, ImVec2& out) { return MissionMapWidget::WorldMapToScreen(wm, out); }, false);
    }
} // namespace

void CartographerWidget::Initialize()
{
    ToolboxWidget::Initialize();
    SettingsRegistry::RegisterField(this, "show_fog", &show_fog);
    SettingsRegistry::RegisterField(this, "show_stand_cells", &show_stand_cells);
    SettingsRegistry::RegisterField(this, "show_grid", &show_grid);
    SettingsRegistry::RegisterField(this, "show_unexpected", &show_unexpected);
    SettingsRegistry::RegisterField(this, "show_uncoverable", &show_uncoverable);
    SettingsRegistry::RegisterField(this, "allow_gate_glitch", &allow_gate_glitch);
    SettingsRegistry::RegisterField(this, "show_whole_continent", &show_whole_continent);
    SettingsRegistry::RegisterField(this, "using_bec", &using_bec);
    SettingsRegistry::RegisterField(this, "set_quest_marker", &set_quest_marker);
    SettingsRegistry::RegisterField(this, "declined_cells", &declined_cells_str);
    SettingsRegistry::RegisterField(this, "custom_points", &custom_points_str);
    MissionMapWidget::AddContextMenuCallback(&OnMissionMapContextMenu);
    WorldMapWidget::AddContextMenuCallback(&OnWorldMapContextMenu);
    MissionMapWidget::AddOverlayCallback(&OnMissionMapOverlayDraw);
    WorldMapWidget::AddOverlayCallback(&OnWorldMapOverlayDraw);
    RegisterUIMessageCallback(&carto_ui_entry, kCartographyUpdated, OnCartographyUpdated, 0x4000);
}

void CartographerWidget::SignalTerminate()
{
    MissionMapWidget::RemoveContextMenuCallback(&OnMissionMapContextMenu);
    WorldMapWidget::RemoveContextMenuCallback(&OnWorldMapContextMenu);
    MissionMapWidget::RemoveOverlayCallback(&OnMissionMapOverlayDraw);
    WorldMapWidget::RemoveOverlayCallback(&OnWorldMapOverlayDraw);
    GW::UI::RemoveUIMessageCallback(&carto_ui_entry);
    visible = false;
    ResetState();
}

void CartographerWidget::LoadSettings(SettingsDoc& doc, ToolboxIni* legacy)
{
    ToolboxWidget::LoadSettings(doc, legacy);
    ParseDeclined();
    ParsePoints();
}

void CartographerWidget::Update(float)
{
#ifdef _DEBUG
    StepBake();
#endif
    if (!GetEnabled()) {
        if (target.valid) ResetState();
        return;
    }
    StepOwnerQuery();
    const auto map_id = GW::Map::GetMapID();
    const auto instance_type = GW::Map::GetInstanceType();
    if (instance_type == GW::Constants::InstanceType::Loading || !GW::Map::GetIsMapLoaded()) {
        if (state_instance_type != GW::Constants::InstanceType::Loading) {
            ResetState();
            state_instance_type = GW::Constants::InstanceType::Loading;
        }
        return;
    }
    if (map_id != state_map_id || instance_type != state_instance_type) {
        ResetState();
        state_map_id = map_id;
        state_instance_type = instance_type;
        map_settled_at = TIMER_INIT();
        SelectProbe(map_id);
    }

    // Everything here is expressed in world-map coordinates, so a map that does not appear on the
    // world map has nothing to compute and nothing to draw.
    const auto map_info = GW::Map::GetMapInfo(map_id);
    map_on_world_map = map_info && map_info->GetIsOnWorldMap();
    if (!map_on_world_map) return;

    // Coordinate anchors can be transitional right after a map change; let them settle.
    if (TIMER_DIFF(map_settled_at) < 2000) return;

    const auto player = GW::Agents::GetControlledCharacter();
    if (!player) return;

    if (TIMER_DIFF(last_scan) < 1000) return;
    last_scan = TIMER_INIT();

    CartoGrid grid;
    if (!GetCartoGrid(grid)) {
        if (!warned_no_data) {
            warned_no_data = true;
            CARTO_LOG("[cartographer] no cartography data available");
        }
        return;
    }

    GW::Vec2f player_wm;
    if (!WorldMapWidget::GamePosToWorldMap(player->pos, player_wm)) return;
    player_wm_cached = player_wm;
    BuildContinentMask(static_cast<int>(map_info->continent));
    // A completing sweep still needs one last full pass, so the flag is read before the sweep.
    DropProbeIfGatesMoved();
    const bool sweeping = !probe->complete;
    SweepStandCells(grid, map_info);

    std::vector<std::pair<int, int>> changed;
    if (carto_dirty) CollectChangedTiles(grid, changed);
#ifdef _DEBUG
    for (const auto& [tx, ty] : changed) {
        const int our_cx = CreditCellX(player_wm.x);
        const int our_cy = CreditCellY(player_wm.y);
        const auto [fog_cx, fog_cy] = FogTileAt(player_wm);
        // Every d must now land inside [-r, r]; one that does not is this bug class coming back.
        Log::Log("[carto-reveal] map=%d game(%.1f, %.1f) wm(%.4f, %.4f) credit_cell(%d, %d) fog_tile(%d, %d) tile(%d, %d) d(%d, %d) r=%d\n",
                 static_cast<int>(map_id), player->pos.x, player->pos.y, player_wm.x, player_wm.y,
                 our_cx, our_cy, fog_cx, fog_cy, tx, ty, tx - our_cx, ty - our_cy, RevealRadius());
    }
    if (!changed.empty()) Log::FlushFile();
#endif
    if (coverage_stale || sweeping) {
        // Rebuilding from scratch supersedes any pending diff, so re-baseline the snapshot.
        if (grid.bits && grid.dword_count) carto_snapshot.assign(grid.bits, grid.bits + grid.dword_count);
        RecomputeCoverage(grid, map_info);
    }
    else if (!changed.empty()) {
        RescoreAround(grid, changed);
        RebuildFog(grid, map_info);
    }
    if (coverage_stale || sweeping || !changed.empty()) RecountExploration(grid);
    carto_dirty = false;
    coverage_stale = false;
    PruneUncoveredPoints(grid);

    // Arrival is being inside the square, not near the goal - on a ledge those are a square apart.
    const int player_cx = CreditCellX(player_wm.x);
    const int player_cy = CreditCellY(player_wm.y);
    player_cell = {player_cx, player_cy};
    player_cell_valid = true;
    if (target.valid) {
        if (target.custom) {
            // Fog points retire when their tile is credited (PruneUncoveredPoints); arriving only
            // starts the clock on whether standing here is going to credit anything at all.
            if (target.goal == GoalKind::Waypoint) {
                // Already-explored ground, so getting to the point itself is all there is to finish it off.
                if (Dist2(player_wm, target.wm) < 2.f * 2.f) {
                    CARTO_LOG("[cartographer] reached custom point wm(%.0f, %.0f)", target.wm.x, target.wm.y);
                    RemoveCustomPointAt(target.wm);
                    ClearTarget();
                }
            }
            else if (target.goal != GoalKind::Stand) {
                arrived = false;
                arrived_at = 0;
            }
            // Leaving stops the clock: the verdict below is about standing here, so it must not be
            // reached from somewhere else entirely once the target latches on to the hysteresis.
            else if (player_cell != std::pair{target.stand_cx, target.stand_cy}) {
                arrived = false;
                arrived_at = 0;
            }
            else if (!arrived) {
                arrived = true;
                arrived_at = TIMER_INIT();
                CARTO_LOG("[cartographer] standing in the square for fog point wm(%.0f, %.0f)", target.wm.x, target.wm.y);
            }
        }
        else if (player_cx != target.cx || player_cy != target.cy) {
            arrived = false;
            arrived_at = 0;
        }
        else if (!arrived) {
            arrived = true;
            arrived_at = TIMER_INIT();
            CARTO_LOG("[cartographer] standing in cell (%d, %d), which should credit %d cells", target.cx, target.cy, target.reveals);
        }
    }

    // Credit is not always instant - it can need a step or a click-walk first - so give the square
    // a fair while before concluding anything.
    if (arrived && target.valid && !target.custom && TIMER_DIFF(arrived_at) > 15000) {
        const auto it = probe->cells.find({target.cx, target.cy});
        if (it != probe->cells.end() && it->second.reveals > 0) {
            // A wide visit that credits nothing usually means the tiles it was reaching for are the
            // ones only normal range uncovers; demote those rather than writing the square off.
            const int r = RevealRadius();
            int demoted = 0;
            for (int dy = -r; dy <= r; dy++) {
                for (int dx = -r; dx <= r; dx++) {
                    if (abs(dx) <= kRevealRadius && abs(dy) <= kRevealRadius) continue;
                    const std::pair cell{target.cx + dx, target.cy + dy};
                    if (!grid.InGrid(cell.first, cell.second) || grid.IsExplored(cell.first, cell.second)) continue;
                    // Blame only what the square was scored on: a tile the score already excluded
                    // was never this visit's to credit.
                    if (!CellCreditableFrom(dx, dy, cell.first, cell.second)) continue;
                    if (probe->strict.insert(cell).second) demoted++;
                }
            }
            if (r > kRevealRadius) {
                if (!demoted) probe->skipped.insert({target.cx, target.cy});
                CARTO_LOG("[cartographer] cell (%d, %d) credited nothing; %d tiles demoted to normal range%s",
                          target.cx, target.cy, demoted, demoted ? "" : ", square skipped for this map");
            }
            else {
                Log::Log("[cartographer] stood in cell (%d, %d) for 15s with no credit - game(%.1f, %.1f) wm(%.4f, %.4f) our_cell(%d, %d): index disagreement, not a dead square\n",
                         target.cx, target.cy, player->pos.x, player->pos.y, player_wm.x, player_wm.y, player_cx, player_cy);
            }
            ClearTarget();
        }
    }

    // Same for a fog point, except the tile that has to credit is the one the player picked: if a
    // wide-range visit has not credited it, demote it so the next resolve sends them closer in.
    if (arrived && target.valid && target.custom && target.goal == GoalKind::Stand && TIMER_DIFF(arrived_at) > 15000) {
        const std::pair cell = FogTileAt(target.wm);
        const int dx = target.stand_cx - cell.first;
        const int dy = target.stand_cy - cell.second;
        // Already at normal range and still nothing: there is no closer square to send them to.
        if (abs(dx) > kRevealRadius || abs(dy) > kRevealRadius) {
            arrived = false;
            arrived_at = 0;
            if (probe->strict.insert(cell).second) {
                CARTO_LOG("[cartographer] fog point (%d, %d) not credited from wide range; dropping it to normal range", cell.first, cell.second);
            }
        }
    }

    Target cand{};
    float cand_d2 = FLT_MAX;
    blocked_point = false;
    // A point nothing can credit must not hold the queue hostage - custom points always outrank
    // suggestions - so selection passes over it and the status line names it instead.
    std::vector<size_t> by_distance(custom_points.size());
    std::iota(by_distance.begin(), by_distance.end(), size_t{0});
    std::ranges::sort(by_distance, [&player_wm](const size_t a, const size_t b) {
        return Dist2(custom_points[a].wm, player_wm) < Dist2(custom_points[b].wm, player_wm);
    });
    for (const size_t i : by_distance) {
        const GW::Vec2f& wm = custom_points[i].wm;
        GW::Vec2f goal_wm{};
        std::pair goal_cell{0, 0};
        if (ResolveGoal(wm, player_wm, goal_wm, goal_cell) == GoalKind::None) {
            blocked_point = true;
            continue;
        }

        cand = {true, true, 0, 0, 0, wm};
        cand_d2 = Dist2(wm, player_wm);
        break;
    }
    if (!cand.valid) {
        // Ranked by cells-credited-per-square-walked: a spot crediting several is worth extra steps.
        float best_value = 0.f;
        for (const auto& [cell, sc] : probe->cells) {
            if (!sc.reachable || sc.reveals <= 0) continue;
            if (probe->skipped.contains(cell) || declined_cells.contains(cell)) continue;
            // Send them to the probed footing, not the cell centre: for the coastline slivers
            // ProbeStandCell exists to find, the centre is unwalkable water.
            GW::Vec2f stand;
            if (!WorldMapWidget::GamePosToWorldMap(sc.pos, stand)) continue;
            // Measured to the footing, not the cell centre: the hysteresis below compares this
            // against the incumbent's `wm`, which is a footing too.
            const float d2 = Dist2(stand, player_wm);
            const float dist_cells = sqrtf(d2) / kWorldMapUnitsPerCell;
            const float value = static_cast<float>(sc.reveals) / (dist_cells + 2.f);
            if (value > best_value) {
                best_value = value;
                cand_d2 = d2;
                cand = {true, false, cell.first, cell.second, sc.reveals, stand};
            }
        }
    }
    if (!cand.valid) {
        if (!warned_no_fog) {
            warned_no_fog = true;
            CARTO_LOG("[cartographer] no fogged walkable ground left on this map");
        }
        if (target.valid) ClearTarget();
        return;
    }
    warned_no_fog = false;

    bool same = target.valid && target.custom == cand.custom
        && (cand.custom ? Dist2(target.wm, cand.wm) < 1.f : (target.cx == cand.cx && target.cy == cand.cy));
    if (target.valid && !same && !(cand.custom && !target.custom)) {
        // Hysteresis: keep the current target unless it became ineligible or the candidate is meaningfully closer.
        const auto current = probe->cells.find({target.cx, target.cy});
        const bool current_eligible = target.custom
            ? target.goal != GoalKind::None && std::ranges::any_of(custom_points, [&](const CustomPoint& p) { return Dist2(p.wm, target.wm) < 1.f; })
            : current != probe->cells.end() && current->second.reachable && current->second.reveals > 0
            && !probe->skipped.contains({target.cx, target.cy}) && !declined_cells.contains({target.cx, target.cy});
        if (current_eligible && cand_d2 >= 0.7f * Dist2(target.wm, player_wm)) same = true;
    }
    if (same) {
        // Same square, but the fog around it may have shrunk and the status line quotes it.
        if (target.valid && !target.custom) {
            const auto it = probe->cells.find({target.cx, target.cy});
            if (it != probe->cells.end()) target.reveals = it->second.reveals;
        }
        RefreshCustomTargetStand(player_wm);
        return;
    }

    target = cand;
    arrived = false;
    RefreshCustomTargetStand(player_wm);
    if (target.custom) {
        CARTO_LOG("[cartographer] target: custom point wm(%.0f, %.0f), stand wm(%.0f, %.0f) goal=%s", target.wm.x, target.wm.y,
                  target.stand_wm.x, target.stand_wm.y,
                  target.goal == GoalKind::Stand ? "stand" : target.goal == GoalKind::Waypoint ? "waypoint" : "none");
    }
    else {
        CARTO_LOG("[cartographer] stand target: cell (%d, %d) wm(%.0f, %.0f) credits %d cells at radius %d",
                 target.cx, target.cy, target.wm.x, target.wm.y, target.reveals, RevealRadius());
    }
}

void CartographerWidget::DrawWorldMapOptions()
{
    ImGui::Checkbox("Show remaining fog", &show_fog);
    ImGui::ShowHelp("Green: everything still unexplored that some square on this map can credit. Fog nothing here can reach draws nothing.");
    ImGui::Checkbox("Show squares to stand in", &show_stand_cells);
    ImGui::ShowHelp("Draws every 32x32 square worth walking into, shaded by how many foggy squares standing there would credit. The current suggestion is outlined and pulses.");
    if (ImGui::Checkbox("Show the whole continent", &show_whole_continent)) {
        GW::GameThread::Enqueue([] { coverage_stale = true; });
    }
    ImGui::ShowHelp("Draws every square still worth uncovering anywhere on this continent, not just the map you are in, using data baked from the game's own map files. Turn off to show only the current map.");
    ImGui::Checkbox("Show the cartography grid", &show_grid);
    ImGui::ShowHelp("Draws the 32x32 tile boundaries. Exploration is credited a whole tile at a time, so this is what tells you which tile you are actually standing in. Hidden when zoomed out far enough that the lines would smear together.");
    if (ImGui::Checkbox("Show squares that cannot be uncovered", &show_uncoverable)) {
        GW::GameThread::Enqueue([] { coverage_stale = true; });
    }
    ImGui::ShowHelp("Draws foggy squares that no ground on this continent can credit, with a note on why: grey where"
                    "\nnothing can ever reach them, yellow where only a gate glitch can. Without them the world map shows"
                    "\na blank patch where fog you can never clear used to be, which reads as already explored.");
    if (ImGui::Checkbox("Count gate glitching", &allow_gate_glitch)) {
        GW::GameThread::Enqueue([] {
            Pathing::SetGateGlitchAllowed(allow_gate_glitch);
            coverage_stale = true;
        });
    }
    ImGui::ShowHelp("Travel portals normally stop you walking past them, so ground behind one is out of reach and the"
                    "\nsquares it would credit are drawn yellow. Turn this on if you Shadow-step through gates and they"
                    "\ncount as ordinary fog instead. Applies to the baked table and the live overlay alike, so the two"
                    "\nkeep agreeing.");
    ImGui::Checkbox("Show unexpected explored squares", &show_unexpected);
    ImGui::ShowHelp("Draws every square you have already uncovered that the baked map data says has no standable ground within reveal range - not even ground only a gate glitch reaches - so nothing should have been able to credit it. Either the bake is missing that ground, or it was uncovered from somewhere the bake does not model. The reveal range follows the Bird's Eye Compass setting below.");
    if (ImGui::Checkbox("Using a Bird's Eye Compass", &using_bec)) {
        GW::GameThread::Enqueue([] {
            for (auto& [map_id, cached] : probe_cache) {
                cached.complete = false;
            }
            owner_cache.clear();
            owner_query = {};
            coverage_stale = true;
        });
    }
    ImGui::ShowHelp("Standing in a tile credits it and the 8 tiles around it (Chebyshev distance, so a square block - not a circle, which is why the nearest-looking spot often is not the right one). A Bird's Eye Compass widens that to 3 tiles in each direction. Where inside the tile you stand makes no difference. Rescans the map.");
    if (ImGui::Checkbox("Set a quest marker to fog points", &set_quest_marker)) {
        GW::GameThread::Enqueue([] { SyncQuestMarker(); });
    }
    ImGui::ShowHelp("Placing a fog point puts a custom quest marker on the square you need to stand in to uncover it, so the usual quest path walks you there. It clears itself once the point is reached or removed, and clearing the marker by hand leaves it cleared. Suggested squares never touch the marker.");
}

void CartographerWidget::Draw(IDirect3DDevice9*)
{
    // Toggle on the mission map, so the helper can be turned on mid-run without opening settings
    // or the world map. Sits beside the vanquish overlay's button rather than under it.
    if (!MissionMapWidget::IsRenderReady()) return;
    const auto top_left = MissionMapWidget::GetTopLeft();
    const auto bottom_right = MissionMapWidget::GetBottomRight();

    constexpr float padding = 4.f;
    const float button_size = ImGui::GetTextLineHeight() + padding * 2;
    ImGui::SetNextWindowPos({top_left.x + padding + button_size + padding, bottom_right.y - button_size - padding});
    ImGui::SetNextWindowSize({0, 0});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {2, 2});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, {0, 0});
    if (ImGui::Begin("##carto_toggle", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(visible ? ImGuiCol_Text : ImGuiCol_TextDisabled));
        if (ImGui::Button(ICON_FA_MAP_MARKED_ALT "##carto_toggler")) {
            const bool on = !visible;
            GW::GameThread::Enqueue([on] { SetEnabled(on); });
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(visible ? "Cartographer active. Click to hide." : "Cartographer hidden. Click to show.");
        }
        ImGui::PopStyleColor();
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
}

void CartographerWidget::DrawSettingsInternal()
{
    ImGui::TextDisabled("Exploration is credited by 32x32 world-map square: standing anywhere inside a square\ncredits it and the ring of squares around it. This works out which squares you could\nstand in, which of them would credit something still foggy, and draws those on the\nworld map and mission map with the most worthwhile one highlighted. Getting there is\nup to you.");
    ImGui::Separator();
    bool on = GetEnabled();
    if (ImGui::Checkbox("Enabled", &on)) {
        GW::GameThread::Enqueue([on] { SetEnabled(on); });
    }
    ImGui::ShowHelp("Also togglable from the button on the mission map, and from the world map's own Cartographer checkbox.");
    DrawWorldMapOptions();

    ImGui::Separator();
    ImGui::TextDisabled("Right-click the world map or mission map to skip a suggestion or queue your own fog points.");
    ImGui::Text("Declined forever: %u squares", static_cast<unsigned>(declined_cells.size()));
    ImGui::SameLine();
    if (ImGui::SmallButton("Clear##declined")) ClearDeclined();
    ImGui::Text("Custom fog points: %u", static_cast<unsigned>(custom_points.size()));
    ImGui::SameLine();
    if (ImGui::SmallButton("Clear##points")) ClearCustomPoints();

#ifdef _DEBUG
    // Before the early-out below: re-baking is a maintenance job, not something you should have to
    // turn the widget on to reach.
    DrawBakeSettings();
#endif

    if (!GetEnabled()) return;
    unsigned standable = 0;
    unsigned useful = 0;
    for (const auto& [cell, sc] : probe->cells) {
        if (!sc.reachable) continue;
        standable++;
        if (sc.reveals > 0) useful++;
    }
    ImGui::Separator();
    ImGui::TextDisabled("This map: %u squares probed, %u standable, %u worth visiting", static_cast<unsigned>(probe->cells.size()), standable, useful);
    ImGui::TextDisabled("Foggy squares: %d reachable, %d that nothing can credit", map_fog_cells, unreachable_fog_cells);
    if (coverable_tiles > 0) {
        ImGui::TextDisabled("This continent: %d squares explored of %d the baked data can credit at radius %d (%.2f%%)",
                            explored_tiles, coverable_tiles, RevealRadius(), 100.f * explored_tiles / coverable_tiles);
    }
    else {
        ImGui::TextDisabled("This continent: %d squares explored", explored_tiles);
    }
    ImGui::Text("Explored squares the baked data did not expect: %d", unexpected_tiles);
    ImGui::ShowHelp("Squares you have uncovered that have no baked standable ground within reveal range, counting ground only a gate glitch reaches. Turn on \"Show unexpected explored squares\" to see where they are on the world map.");
    if (unexpected_tiles > 0 && ImGui::TreeNodeEx("List them##unexpected", ImGuiTreeNodeFlags_NoTreePushOnOpen)) {
        if (static_cast<size_t>(unexpected_tiles) > unexpected_cells.size()) {
            ImGui::TextDisabled("Showing the first %u.", static_cast<unsigned>(unexpected_cells.size()));
        }
        ImGui::BeginChild("##unexpected_list", {0.f, 160.f * ImGui::FontScale()}, true);
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(unexpected_cells.size()));
        while (clipper.Step()) {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                const auto [cx, cy] = unexpected_cells[i];
                ImGui::Text("(%d, %d)  %s", cx, cy, MapRectName(cx, cy).c_str());
            }
        }
        ImGui::EndChild();
    }
    if (continent_mask.Empty()) {
        ImGui::TextDisabled("No baked data for this continent - showing this map only.");
    }
    else {
        ImGui::TextDisabled("Baked continent data: %dx%d squares at (%d,%d), radius %d",
                            continent_mask.w, continent_mask.h, continent_mask.x0, continent_mask.y0, kMaskRadius);
    }
}


#ifdef _DEBUG
void CartographerWidget::DrawBakeSettings()
{
    ImGui::Separator();
    ImGui::Text("Continent bake (debug)");
    ImGui::TextDisabled("Reads every world-map map's pathing data out of the DAT and records which\n32x32 squares have standable ground, per continent. One map per frame, so it\nruns for a while; results go to Settings/cartography/standable_L<n>.bin.");
    ImGui::BeginDisabled(bake.running);
    if (ImGui::Button("Bake standable squares for all continents")) {
        GW::GameThread::Enqueue([] { StartBake(); });
    }
    ImGui::EndDisabled();
    if (bake.running) {
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            GW::GameThread::Enqueue([] {
                bake.running = false;
                bake.summary = "cancelled";
            });
        }
        ImGui::ProgressBar(bake.queue.empty() ? 0.f : static_cast<float>(bake.next) / bake.queue.size());
    }
    if (!bake.summary.empty()) ImGui::TextWrapped("%s", bake.summary.c_str());
    if (bake.on_world_map) {
        ImGui::TextDisabled("%d maps on the world map; %d with no file id, %d failed to load, %d had no bounds",
                            bake.on_world_map, bake.no_file_id, bake.load_failed, bake.no_bounds);
        ImGui::TextDisabled("AreaInfo::file_id vs GetMapFileId: %d agree, %d differ, %d absent",
                            bake.area_fid_agrees, bake.area_fid_differs, bake.area_fid_missing);
    }
    for (const auto& [continent, data] : bake.continents) {
        ImGui::TextDisabled("  continent %d: %d maps, %u standable, %u creditable squares", continent, data.maps,
                            static_cast<unsigned>(data.standable.size()), static_cast<unsigned>(data.creditable.size()));
    }
}
#endif


#ifdef _DEBUG
void CartographerWidget::LogProbeAtCell(const int cx, const int cy)
{
    LogProbe(CreditCellCenterWorldMap(cx, cy));
}
#endif

#ifdef _DEBUG
void CartographerWidget::SetGateGlitchAllowed(const bool allowed)
{
    allow_gate_glitch = allowed;
    Pathing::SetGateGlitchAllowed(allowed);
    coverage_stale = true;
}

void CartographerWidget::StartContinentBake()
{
    StartBake();
}

bool CartographerWidget::ContinentBakeRunning()
{
    return bake.running;
}
#endif

void CartographerWidget::SetEnabled(const bool on)
{
    auto& self = Instance();
    if (self.visible == on) return;
    self.visible = on;
    if (!on) {
        ReleaseQuestMarker();
        ResetState();
    }
    CARTO_LOG("[cartographer] %s", on ? "enabled" : "disabled");
}

bool CartographerWidget::GetEnabled()
{
    return Instance().visible;
}

bool CartographerWidget::GetCurrentTargetWorldPos(GW::Vec2f& out)
{
    if (!target.valid) return false;
    out = target.wm;
    return true;
}





void CartographerWidget::SkipCurrentTarget(const bool forever)
{
    GW::GameThread::Enqueue([forever] {
        SkipTargetImpl(forever);
    });
}

void CartographerWidget::AddCustomPoint(const GW::Vec2f& world_map_pos)
{
    GW::GameThread::Enqueue([world_map_pos] {
        AddCustomPointImpl(world_map_pos);
    });
}

void CartographerWidget::RemoveCustomPointNear(const GW::Vec2f& world_map_pos, const float max_dist_wm)
{
    GW::GameThread::Enqueue([world_map_pos, max_dist_wm] {
        const int idx = FindCustomPointNear(world_map_pos, max_dist_wm);
        if (idx < 0) return;
        blocked_point = false;
        const GW::Vec2f p = custom_points[idx].wm;
        const bool was_target = target.valid && target.custom && Dist2(target.wm, p) < 1.f;
        custom_points.erase(custom_points.begin() + idx);
        SerializePoints();
        if (was_target) ClearTarget();
        CARTO_LOG("[cartographer] fog point (%.0f, %.0f) removed", p.x, p.y);
    });
}

void CartographerWidget::ClearCustomPoints()
{
    GW::GameThread::Enqueue([] {
        custom_points.clear();
        blocked_point = false;
        SerializePoints();
        if (target.valid && target.custom) ClearTarget();
        CARTO_LOG("[cartographer] custom fog points cleared");
    });
}

void CartographerWidget::ClearDeclined()
{
    GW::GameThread::Enqueue([] {
        declined_cells.clear();
        for (auto& [map_id, cached] : probe_cache) {
            cached.skipped.clear();
            cached.strict.clear();
        }
        coverage_stale = true;
        SerializeDeclined();
        if (target.valid) ClearTarget();
        CARTO_LOG("[cartographer] declined cells cleared");
    });
}

void CartographerWidget::GetStatus(char* buf, const size_t len)
{
    char target_desc[64];
    if (!target.valid) snprintf(target_desc, sizeof(target_desc), "none");
    else if (target.custom) snprintf(target_desc, sizeof(target_desc), "point(%.0f,%.0f)%s", target.wm.x, target.wm.y,
                                     target.goal == GoalKind::Stand ? "+stand" : target.goal == GoalKind::Waypoint ? "+waypoint" : "+nogoal");
    else snprintf(target_desc, sizeof(target_desc), "stand(%d,%d)+%d", target.cx, target.cy, target.reveals);
    snprintf(buf, len, "carto: enabled=%d target=%s arrived=%d radius=%d skipped=%u probed=%u declined=%u points=%u fogcells=%d marker=%d",
             GetEnabled(), target_desc, arrived, RevealRadius(),
             static_cast<unsigned>(probe->skipped.size()), static_cast<unsigned>(probe->cells.size()),
             static_cast<unsigned>(declined_cells.size()), static_cast<unsigned>(custom_points.size()), map_fog_cells, marker_placed);
}

