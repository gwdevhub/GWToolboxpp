#include "stdafx.h"

#include <fstream>
#include <map>

#include <GWCA/Constants/Constants.h>
#include <GWCA/Context/CharContext.h>
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

    // The fog mesh builder strides rows by (width >> 5) words while the explored-query indexes
    // bits flat as cy * width + cx; the client uses both interchangeably, so width is always a
    // multiple of 32 and either form works.
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

    // The tile the client credits from a standing position. Same grid as the fog bits - both axes
    // span [32c, 32c+32) - now that GamePosToWorldMap no longer reports positions a unit north of
    // where they are; the row skew this used to correct for was that offset, not a property of the
    // client's grid. Epsilon leans off the closed end so a value that arrived through the two
    // conversions and landed a few ulp short of a boundary still reads as the cell that owns it.
    int CreditCellX(const float x)
    {
        return static_cast<int>(floorf((x + kCellEps) / kWorldMapUnitsPerCell));
    }

    int CreditCellY(const float y)
    {
        return static_cast<int>(floorf((y + kCellEps) / kWorldMapUnitsPerCell));
    }

    std::pair<int, int> CreditCellAt(const GW::Vec2f& wm)
    {
        return {CreditCellX(wm.x), CreditCellY(wm.y)};
    }

    GW::Vec2f CreditCellCenterWorldMap(const int cx, const int cy)
    {
        return {cx * kWorldMapUnitsPerCell + 16.f, cy * kWorldMapUnitsPerCell + 16.f};
    }

    // Which fog bit is drawn under a point - the same index CreditCellAt gives, without the epsilon
    // that only round-tripped positions need, so a dx/dy between the two never carries a correction.
    std::pair<int, int> FogTileAt(const GW::Vec2f& wm)
    {
        return {
            static_cast<int>(floorf(wm.x / kWorldMapUnitsPerCell)),
            static_cast<int>(floorf(wm.y / kWorldMapUnitsPerCell)),
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

    // Standing in a tile credits it plus the ring around it; a Bird's Eye Compass widens that to
    // three rings. Chebyshev throughout, on the credit grid above - so where inside the tile you
    // stand makes no difference on either axis.
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

    // Probing a map costs a walkability query per sample per tile, and the answer never changes
    // while you are on that map, so it is swept once and kept for the session. `strict` and
    // `skipped` are learned the same way - by visiting - so they belong with it.
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

    // Gw.exe credits exploration in FUN_00811be0: it writes the (2r+1)^2 tile block around you and
    // then broadcasts 0x10000090 with no payload. So the message says "something changed" and the
    // bitmap itself says what - diffing a snapshot narrows the recompute to the tiles that flipped.
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

    // Gates opening or closing move whole regions in and out of reach, so a sweep taken under the
    // old state is worthless. Comparing the state itself rather than waiting on an event also
    // covers arriving in an instance whose gates already differ from the last visit.
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
        map_rect_min = {
            static_cast<int>(floorf(bounds.Min.x / kWorldMapUnitsPerCell)),
            static_cast<int>(floorf(bounds.Min.y / kWorldMapUnitsPerCell)),
        };
        map_rect_max = {
            static_cast<int>(ceilf(bounds.Max.x / kWorldMapUnitsPerCell)),
            static_cast<int>(ceilf(bounds.Max.y / kWorldMapUnitsPerCell)),
        };
        map_rect_valid = true;
        return true;
    }

    // Two different boundary rules, both measured: the near ring credits a tile up to one tile past
    // the map's edge, while a tile only reachable at Bird's Eye Compass range has to be inside it.
    bool InMapBounds(const int cx, const int cy, const int dilate)
    {
        if (!EnsureMapRect()) return true; // no rectangle to clamp against - do not hide everything
        return cx >= map_rect_min.first - dilate && cx < map_rect_max.first + dilate
            && cy >= map_rect_min.second - dilate && cy < map_rect_max.second + dilate;
    }

    // Measured in game: a tile is reachable beyond the near ring only if walkable ground comes
    // within about one world-map tile of it - the 3x3 block around it, NOT the tile itself, which
    // is what the old reading of Gw.exe's per-tile byte got wrong and why we under-claimed at BEC
    // range. `strict` stays as a runtime backstop while the navmesh test is still the loose one.
    bool CellQualifies(const int fx, const int fy)
    {
        if (!InMapBounds(fx, fy, 0)) return false;
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
        if (abs(dx) <= kRevealRadius && abs(dy) <= kRevealRadius) return InMapBounds(fx, fy, 1);
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

    // The baked standable tiles for the continent we are on, dilated by ONE tile so a lookup answers
    // "could standing somewhere credit this tile" in one test. One tile regardless of the Bird's Eye
    // Compass: the bake's standable set is also its navmesh model, so a tile the wide rings could
    // reach is already a tile something stands next to. Dilating by 3 claimed fog nothing can credit.
    // Built once per continent; the live probe still covers the map we are actually in, which the
    // bake does not have for the handful of maps with no file id.
    constexpr int kMaskRadius = 1;

    struct ContinentMask {
        int continent = -1;
        int x0 = 0, y0 = 0, w = 0, h = 0;
        std::vector<uint8_t> coverable;

        bool Get(const int cx, const int cy) const
        {
            const int lx = cx - x0, ly = cy - y0;
            if (lx < 0 || ly < 0 || lx >= w || ly >= h) return false;
            const size_t bit = static_cast<size_t>(ly) * w + lx;
            return coverable[bit >> 3] >> (bit & 7) & 1;
        }
    };
    ContinentMask continent_mask;

    void BuildContinentMask(const int continent)
    {
        if (continent_mask.continent == continent) return;
        continent_mask = {};
        continent_mask.continent = continent;
        constexpr int radius = kMaskRadius;
        const CartographyData::Continent* src = nullptr;
        for (const auto& c : CartographyData::kContinents) {
            if (c.id == continent) { src = &c; break; }
        }
        if (!src) return;
        // Grow the grid by the radius so tiles credited from the edge are still representable.
        continent_mask.x0 = src->x0 - radius;
        continent_mask.y0 = src->y0 - radius;
        continent_mask.w = src->width + radius * 2;
        continent_mask.h = src->height + radius * 2;
        continent_mask.coverable.assign((static_cast<size_t>(continent_mask.w) * continent_mask.h + 7) / 8, 0);
        for (int y = 0; y < src->height; y++) {
            for (int x = 0; x < src->width; x++) {
                const size_t bit = static_cast<size_t>(y) * src->width + x;
                if (bit / 8 >= static_cast<size_t>(src->byte_count)) continue;
                if (!(src->bits[bit >> 3] >> (bit & 7) & 1)) continue;
                for (int dy = -radius; dy <= radius; dy++) {
                    for (int dx = -radius; dx <= radius; dx++) {
                        const int lx = x + radius + dx, ly = y + radius + dy;
                        if (lx < 0 || ly < 0 || lx >= continent_mask.w || ly >= continent_mask.h) continue;
                        const size_t b = static_cast<size_t>(ly) * continent_mask.w + lx;
                        continent_mask.coverable[b >> 3] |= 1 << (b & 7);
                    }
                }
            }
        }
    }

    // Credit stops one tile past the end of the world: fog that no placed map's world-map rectangle
    // comes within a tile of never uncovers, however close you stand to it. Taken from AreaInfo
    // rather than from the bake so it covers all 350 placed maps, including the 19 the bake has no
    // map file id for.
    struct ContinentWorld {
        int continent = -1;
        int x0 = 0, y0 = 0, w = 0, h = 0;
        std::vector<uint8_t> inside;

        // An empty plane means we found no rectangles for this continent, so clamping would hide
        // everything; answer "in the world" and leave the other tests to decide.
        bool Get(const int cx, const int cy) const
        {
            if (inside.empty()) return true;
            const int lx = cx - x0, ly = cy - y0;
            if (lx < 0 || ly < 0 || lx >= w || ly >= h) return false;
            const size_t bit = static_cast<size_t>(ly) * w + lx;
            return inside[bit >> 3] >> (bit & 7) & 1;
        }
    };
    ContinentWorld continent_world;

    bool InWorld(const int cx, const int cy)
    {
        return continent_world.Get(cx, cy);
    }

    void BuildContinentWorld(const int continent)
    {
        if (continent_world.continent == continent) return;
        continent_world = {};
        continent_world.continent = continent;

        // Already dilated by the one tile the client credits beyond a map's edge.
        std::vector<std::array<int, 4>> rects;
        int x0 = INT_MAX, y0 = INT_MAX, x1 = INT_MIN, y1 = INT_MIN;
        for (size_t i = 1; i < static_cast<size_t>(GW::Constants::MapID::Count); i++) {
            const auto info = GW::Map::GetMapInfo(static_cast<GW::Constants::MapID>(i));
            if (!(info && info->GetIsOnWorldMap() && static_cast<int>(info->continent) == continent)) continue;
            ImRect bounds;
            if (!(GW::Map::GetMapWorldMapBounds(info, &bounds) && bounds.GetWidth() >= 1.f && bounds.GetHeight() >= 1.f)) continue;
            const std::array r{
                static_cast<int>(floorf(bounds.Min.x / kWorldMapUnitsPerCell)) - 1,
                static_cast<int>(floorf(bounds.Min.y / kWorldMapUnitsPerCell)) - 1,
                static_cast<int>(ceilf(bounds.Max.x / kWorldMapUnitsPerCell)),
                static_cast<int>(ceilf(bounds.Max.y / kWorldMapUnitsPerCell)),
            };
            rects.push_back(r);
            x0 = std::min(x0, r[0]);
            y0 = std::min(y0, r[1]);
            x1 = std::max(x1, r[2]);
            y1 = std::max(y1, r[3]);
        }
        if (rects.empty()) return;

        continent_world.x0 = x0;
        continent_world.y0 = y0;
        continent_world.w = x1 - x0 + 1;
        continent_world.h = y1 - y0 + 1;
        continent_world.inside.assign((static_cast<size_t>(continent_world.w) * continent_world.h + 7) / 8, 0);
        for (const auto& r : rects) {
            for (int cy = r[1]; cy <= r[3]; cy++) {
                for (int cx = r[0]; cx <= r[2]; cx++) {
                    const size_t bit = static_cast<size_t>(cy - y0) * continent_world.w + (cx - x0);
                    continent_world.inside[bit >> 3] |= 1 << (bit & 7);
                }
            }
        }
    }

    // Everything counts as coverable until the sweep finishes, so the overlay does not blink
    // cells out and back in as probing progresses.
    bool FogCellCoverable(const int cx, const int cy)
    {
        // The bake knows the whole continent; the live probe knows the map we are standing in,
        // including the few the bake has no file id for. Either is enough. The mask is dilated by
        // one tile, so anything it claims is credited from some map's near ring - which is the ring
        // the one-tile-past-the-edge rule applies to, hence the dilated union here.
        if (continent_mask.Get(cx, cy)) return InWorld(cx, cy);
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

    constexpr float kFogMaxAlpha = 135.f;

    float ExploredAtCorner(const CartoGrid& grid, const int cx, const int cy)
    {
        return (static_cast<float>(grid.IsExplored(cx - 1, cy - 1)) + static_cast<float>(grid.IsExplored(cx, cy - 1)) +
                static_cast<float>(grid.IsExplored(cx - 1, cy)) + static_cast<float>(grid.IsExplored(cx, cy))) * 0.25f;
    }

    // The client averages the four cells meeting at a corner, then bakes that field into a fog
    // texture at kFogSubdivisions texels per cell, 4 bits each - which is why the fog on screen
    // steps at a quarter of a cell and bands rather than ramping smoothly.
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


    // Coastlines ignore the cell grid, so sample the whole cell: one that is mostly cliff but
    // clips a walkable ledge is still somewhere you can go. Answers both halves of the question:
    // where we can send the player (reachable - ground behind a closed gate paths fine but cannot be
    // walked to) and whether the tile carries walkable ground at all, which is what credit turns on.
    void ProbeStandCell(const int cx, const int cy, StandCell& out)
    {
        // Fine enough to catch the shoreline slivers that are often the only footing near fog.
        constexpr int kSamples = 6;
        float best_d2 = FLT_MAX;
        const GW::Vec2f centre = CreditCellCenterWorldMap(cx, cy);
        for (int sy = 0; sy < kSamples; sy++) {
            for (int sx = 0; sx < kSamples; sx++) {
                const GW::Vec2f wm{
                    (cx + (sx + 0.5f) / kSamples) * kWorldMapUnitsPerCell,
                    (cy + (sy + 0.5f) / kSamples) * kWorldMapUnitsPerCell,
                };
                GW::GamePos gp{};
                // Reachability is the expensive half and implies walkability, so the cheap BSP
                // descent goes first and answers `navmesh` on its own.
                if (!WorldMapWidget::WorldMapToGamePos(wm, gp) || !Pathing::IsPositionWalkable(gp)) continue;
                out.navmesh = true;
                if (!Pathing::IsPositionReachable(gp)) continue;
                // Aiming central keeps our own routing error from landing the player in the
                // neighbouring tile.
                const float d2 = Dist2(wm, centre);
                if (!out.reachable || d2 < best_d2) {
                    best_d2 = d2;
                    out.pos = gp;
                    out.reachable = true;
                }
            }
        }
    }

    // A fog point marks fog, and fog is rarely somewhere you can stand. Answers with the closest
    // spot to `from` that is reachable AND sits in a tile the game would credit `fog_wm`'s tile
    // from. False when nothing here can credit it: another map, or a point dropped on ground that
    // is already explored.
    bool ResolveStandWorldPos(const GW::Vec2f& fog_wm, const GW::Vec2f& from, GW::Vec2f& out, std::pair<int, int>& out_cell)
    {
        const auto [fx, fy] = FogTileAt(fog_wm);
        // Nothing on this map reaches past its own edge by more than the near ring.
        if (!InMapBounds(fx, fy, 1)) return false;
        const int r = RevealRadius();
        std::vector<std::pair<int, int>> candidates;
        for (int dy = -r; dy <= r; dy++) {
            for (int dx = -r; dx <= r; dx++) {
                // Credit is decided from the standing tile's side, so the offsets invert here.
                if (CellCreditableFrom(-dx, -dy, fx, fy)) candidates.push_back({fx + dx, fy + dy});
            }
        }
        // Nearest first, so the usual case answers after probing one tile rather than all of them.
        std::ranges::sort(candidates, [&from](const auto& a, const auto& b) {
            return Dist2(CreditCellCenterWorldMap(a.first, a.second), from) < Dist2(CreditCellCenterWorldMap(b.first, b.second), from);
        });
        for (const auto& cell : candidates) {
            auto it = probe->cells.find(cell);
            if (it == probe->cells.end()) {
                StandCell sc;
                ProbeStandCell(cell.first, cell.second, sc);
                it = probe->cells.emplace(cell, sc).first;
                coverage_stale = true; // scored by the next recompute, not by us - we have no grid here
            }
            if (it->second.reachable && WorldMapWidget::GamePosToWorldMap(it->second.pos, out)) {
                out_cell = cell;
                return true;
            }
        }
        return false;
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

    // Whether a cell is standable never changes within a map, so probe it once and keep it. Fog
    // only ever shrinks, so once the sweep has covered the map nothing new becomes worth probing
    // and revisits cost nothing.
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
                ProbeStandCell(cx, cy, sc);
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
        if (show_whole_continent && !continent_mask.coverable.empty()) {
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

    struct Target {
        bool valid = false;
        bool custom = false;
        // For fog targets these are the cell to go and stand in, not the cell being uncovered.
        int cx = 0;
        int cy = 0;
        int reveals = 0;
        GW::Vec2f wm{};
        bool on_map = true;
        // Where a fog point actually sends you: the spot in the tile that credits it. Unset for
        // fog targets, whose `wm` is already the tile to stand in. The cell is carried rather than
        // re-derived, because `stand_wm` is a round trip through two conversions and a value that
        // lands on a row boundary parses back into the neighbouring row.
        GW::Vec2f stand_wm{};
        int stand_cx = 0;
        int stand_cy = 0;
        bool stand_valid = false;
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

    // Where the player is being sent: a fog point's standing spot, or the target itself.
    const GW::Vec2f& TargetGoal()
    {
        return target.stand_valid ? target.stand_wm : target.wm;
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

    // Fog points are the one thing here the player asks for by hand, so they get a quest marker to
    // walk to - on the tile that credits the fog, not on the fog. Suggestions do not: they are a
    // standing offer, and hijacking the quest marker for one is not.
    void SyncQuestMarker()
    {
        if (!set_quest_marker || !target.valid || !target.custom) {
            ReleaseQuestMarker();
            return;
        }
        const GW::Vec2f goal = TargetGoal();
        if (marker_placed && Dist2(marker_point, target.wm) < 1.f) {
            // Same point: follow it only while the marker is still ours, so clearing it by hand sticks.
            if (Dist2(goal, marker_goal) < 1.f || !MarkerStillOurs()) return;
        }
        else {
            ReleaseQuestMarker();
        }
        marker_placed = true;
        marker_point = target.wm;
        marker_goal = goal;
        QuestModule::SetCustomQuestMarker(goal, true);
    }

    void ClearTarget()
    {
        target = {};
        arrived = false;
        arrived_at = 0;
        SyncQuestMarker();
    }

    // Re-resolved every scan rather than kept: the sweep keeps learning what is standable, a gate
    // moving can take the answer away again, and once the tile is credited there is nowhere to send
    // anyone - a point on explored ground is just a waypoint, so it routes to itself.
    void RefreshCustomTargetStand(const GW::Vec2f& from)
    {
        if (!target.valid || !target.custom) return;
        CartoGrid grid;
        const auto [fx, fy] = FogTileAt(target.wm);
        const bool foggy = GetCartoGrid(grid) && !grid.IsExplored(fx, fy);
        std::pair cell{0, 0};
        target.stand_valid = target.on_map && foggy && ResolveStandWorldPos(target.wm, from, target.stand_wm, cell);
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
        map_fog_cells = -1;
        fog_cells.clear();
        unreachable_fog_cells = 0;
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

    void AddCustomPointImpl(const GW::Vec2f& wm)
    {
        CartoGrid grid;
        const auto [fx, fy] = FogTileAt(wm);
        const bool foggy = GetCartoGrid(grid) && !grid.IsExplored(fx, fy);
        custom_points.push_back({wm, foggy});
        SerializePoints();
        // Taking the target over straight away is what makes the marker point at the fog you just
        // asked about, instead of at whichever queued point happens to be nearest.
        target = {};
        target.valid = true;
        target.custom = true;
        target.wm = wm;
        ImRect bounds;
        const auto map_info = GW::Map::GetMapInfo(GW::Map::GetMapID());
        target.on_map = map_info && GW::Map::GetMapWorldMapBounds(map_info, &bounds) && bounds.Contains({wm.x, wm.y});
        arrived = false;
        RefreshCustomTargetStand(player_wm_cached);
        CARTO_LOG("[cartographer] custom fog point added at wm(%.0f, %.0f)%s", wm.x, wm.y,
                  target.stand_valid ? "" : " - no reachable spot credits it from here");
    }

#ifdef _DEBUG
    // Bakes, per continent, which 32x32 tiles have ground you can stand on. Everything it needs is
    // reachable without visiting a map: the file id comes from GetMapFileId, AreaInfo gives the
    // continent and world-map bounds, and the DAT gives the trapezoids. Stores standable rather
    // than discoverable so the reveal radius stays a runtime choice.
    struct ContinentBake {
        std::unordered_set<uint64_t> standable; // (cy << 32) | (uint32)cx
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

    // Trapezoids reachable from the map's largest connected component. Planes are all treated as
    // open - which of them are blocked comes from the server at runtime - so this only drops
    // genuinely disconnected geometry, which is what "pathable but not accessible" means offline.
    std::unordered_set<const GW::PathingTrapezoid*> LargestComponent(const Pathing::PathingMapData& data)
    {
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
        std::unordered_set<const GW::PathingTrapezoid*> best;
        for (const auto* root : all) {
            if (seen.contains(root)) continue;
            std::unordered_set<const GW::PathingTrapezoid*> component;
            std::vector<const GW::PathingTrapezoid*> queue{root};
            component.insert(root);
            seen.insert(root);
            for (size_t head = 0; head < queue.size(); head++) {
                const auto* trap = queue[head];
                const auto expand = [&](const GW::PathingTrapezoid* next) {
                    if (!next || !component.insert(next).second) return;
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
            if (component.size() > best.size()) best = std::move(component);
        }
        return best;
    }

    // Loads through GetMapFileId, which is the known-good path. AreaInfo::file_id is recorded
    // alongside it but not trusted yet: nothing validates that it names a map file - readFromDat's
    // second argument is a stream id, not a type - so a wrong id just yields no pathfinding chunk
    // and looks like a load failure. The counters below are here to settle whether AreaInfo alone
    // would do, since that is the version that survives a game update without a table in the repo.
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
        const auto component = LargestComponent(data);
        auto& out = bake.continents[continent];
        int marked = 0;
        for (const auto& plane : data.planes) {
            for (uint32_t t = 0; t < plane.trapezoid_count; t++) {
                const auto& trap = plane.trapezoids[t];
                if (!component.contains(&trap)) continue;
                // The trapezoid's game-space box, converted through this map's own anchor. A tile
                // is 3072 gwinches, so taking the box rather than the exact quad costs at most a
                // sliver of over-marking on a slanted edge.
                GW::GamePos lo{}, hi{};
                lo.x = std::min(trap.XTL, trap.XBL);
                lo.y = trap.YB;
                hi.x = std::max(trap.XTR, trap.XBR);
                hi.y = trap.YT;
                GW::Vec2f a{}, b{};
                if (!WorldMapWidget::GamePosToWorldMap(lo, a, map_id)) continue;
                if (!WorldMapWidget::GamePosToWorldMap(hi, b, map_id)) continue;
                const int x0 = static_cast<int>(floorf(std::min(a.x, b.x) / kWorldMapUnitsPerCell));
                const int x1 = static_cast<int>(floorf(std::max(a.x, b.x) / kWorldMapUnitsPerCell));
                const int y0 = static_cast<int>(floorf(std::min(a.y, b.y) / kWorldMapUnitsPerCell));
                const int y1 = static_cast<int>(floorf(std::max(a.y, b.y) / kWorldMapUnitsPerCell));
                for (int cy = y0; cy <= y1; cy++) {
                    for (int cx = x0; cx <= x1; cx++) {
                        if (out.standable.insert(TileKey(cx, cy)).second) marked++;
                    }
                }
            }
        }
        out.maps++;
        CARTO_LOG("[carto-bake] map %d (file 0x%X, continent %d): %d planes, +%d tiles",
                  static_cast<int>(map_id), file_id, continent, static_cast<int>(data.planes.size()), marked);
    }

    void WriteBakeFiles()
    {
        for (const auto& [continent, data] : bake.continents) {
            if (data.standable.empty()) continue;
            int x0 = INT_MAX, y0 = INT_MAX, x1 = INT_MIN, y1 = INT_MIN;
            for (const auto key : data.standable) {
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
            for (const auto key : data.standable) {
                const int cx = static_cast<int>(static_cast<uint32_t>(key & 0xffffffff)) - x0;
                const int cy = static_cast<int>(static_cast<uint32_t>(key >> 32)) - y0;
                const size_t bit = static_cast<size_t>(cy) * w + cx;
                bits[bit >> 3] |= 1 << (bit & 7);
            }
            const int32_t header[5] = {continent, x0, y0, w, h};
            const auto path = Resources::GetPath(L"cartography", std::format(L"standable_L{}.bin", continent));
            std::ofstream file(path, std::ios::binary);
            if (!file) {
                CARTO_LOG("[carto-bake] could not write %ls", path.wstring().c_str());
                continue;
            }
            file.write("CSM1", 4);
            file.write(reinterpret_cast<const char*>(header), sizeof(header));
            file.write(reinterpret_cast<const char*>(bits.data()), static_cast<std::streamsize>(bits.size()));
            CARTO_LOG("[carto-bake] continent %d: %d maps, %u tiles, grid %dx%d at (%d,%d), %u bytes",
                      continent, data.maps, static_cast<unsigned>(data.standable.size()), w, h, x0, y0,
                      static_cast<unsigned>(bits.size()));
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
        if (!target.valid) {
            const char* idle = map_fog_cells == 0 ? "nothing left to uncover on this map"
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
        const GW::Vec2f goal = TargetGoal();
        const float dist_k = sqrtf(Dist2(player_wm_cached, goal)) * kGwinchesPerWorldMapUnit / 1000.f;
        if (target.custom) {
            if (target.stand_valid) {
                snprintf(buf, len, "stand in the square %.1fk units %s of you to uncover your fog point", dist_k,
                         CompassDir(player_wm_cached, goal));
                return;
            }
            snprintf(buf, len, "heading to your fog point, %.1fk units %s of you%s", dist_k, CompassDir(player_wm_cached, goal),
                     target.on_map ? "" : " (another map)");
            return;
        }
        snprintf(buf, len, "stand in the square %.1fk units %s of you to uncover %d %s%s", dist_k,
                 CompassDir(player_wm_cached, target.wm), target.reveals, target.reveals == 1 ? "square" : "squares",
                 target.on_map ? "" : " (another map)");
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

    // The fog going away is the whole point of a fog point, so that - not arriving anywhere - is
    // what retires it: credit can land a second or two after the step that earned it, and it lands
    // for every point in range, not just the one being walked to.
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
            char status[160];
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
                GW::GameThread::Enqueue([at] {
                    CartoGrid g;
                    if (!GetCartoGrid(g)) {
                        Log::Log("[cartographer] probe: no cartography data");
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
                    // Which half of FogCellCoverable answered: the bake short-circuits before any
                    // live test, so a wrong verdict there is a wrong bake, not a wrong probe.
                    Log::Log("[cartographer] probe (%d,%d): baked_mask=%d in_world=%d probe_complete=%d",
                             cx, cy, static_cast<int>(continent_mask.Get(cx, cy)), static_cast<int>(InWorld(cx, cy)),
                             static_cast<int>(probe->complete));
                    Log::FlushFile();
                });
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

    // Which map a foggy tile belongs to, for the hover tooltip. GetMapIdForLocation walks every
    // map on the continent, so the answer is kept until the pointer moves to a different tile.
    std::pair<int, int> hover_lookup_cell{INT_MIN, INT_MIN};
    GW::Constants::MapID hover_lookup_map = GW::Constants::MapID::None;

    GW::Constants::MapID MapForTile(const int cx, const int cy)
    {
        if (hover_lookup_cell != std::pair{cx, cy}) {
            hover_lookup_cell = {cx, cy};
            hover_lookup_map = WorldMapWidget::GetMapIdForLocation({(cx + .5f) * kWorldMapUnitsPerCell, (cy + .5f) * kWorldMapUnitsPerCell});
        }
        return hover_lookup_map;
    }

    bool TileOnCurrentMap(const int cx, const int cy)
    {
        return cx >= map_cell_min.first && cx < map_cell_max.first
            && cy >= map_cell_min.second && cy < map_cell_max.second;
    }

    // One quad per fog texel, so ImGui interpolates them as the GPU does when it samples the
    // client's texture.
    void DrawFog(ImDrawList* dl, const ProjectToScreen project, const ImVec2& mouse, std::string& tooltip_out)
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
        // Only worth saying for fog outside the map you are standing in - otherwise the answer is
        // "you are already here".
        if (!hovered || TileOnCurrentMap(hovered->cx, hovered->cy)) return;
        const auto map_id = MapForTile(hovered->cx, hovered->cy);
        if (map_id == GW::Constants::MapID::None) {
            tooltip_out = "Unexplored - no map here";
            return;
        }
        const auto& name = Resources::GetMapName(map_id)->string();
        tooltip_out = name.empty() ? "Unexplored" : "Unexplored - travel to " + name;
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
        if (show_fog) {
            DrawFog(dl, project, mouse, fog_tooltip);
        }
        if (show_grid) {
            DrawGrid(dl, project);
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
        if (target_active && target.custom && target.stand_valid) {
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
        else if (cell_tooltip && !fog_tooltip.empty()) ImGui::SetTooltip("%s", fog_tooltip.c_str());
    }

    void OnWorldMapOverlayDraw(ImDrawList* dl)
    {
        if (!CartographerWidget::GetEnabled() || !map_on_world_map) return;
        DrawMapOverlay(dl, [](const GW::Vec2f& wm, ImVec2& out) { return WorldMapWidget::WorldMapToScreen(wm, out); }, true);
        char status[160];
        BuildStatusText(status, sizeof(status));
        char line[192];
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
    BuildContinentWorld(static_cast<int>(map_info->continent));
    // A completing sweep still needs one last full pass, so the flag is read before the sweep.
    DropProbeIfGatesMoved();
    const bool sweeping = !probe->complete;
    SweepStandCells(grid, map_info);

    std::vector<std::pair<int, int>> changed;
    if (carto_dirty) CollectChangedTiles(grid, changed);
#ifdef _DEBUG
    // What the client actually credited, against what we would have predicted. This is the only
    // measurement of the reveal rule we have: the tile offsets it prints are ground truth for the
    // radius, the extent clamp and any quantisation difference between our grid and the client's.
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
    carto_dirty = false;
    coverage_stale = false;
    PruneUncoveredPoints(grid);

    // Arrival is being inside the square, not near the goal - on a ledge those are a square apart.
    const int player_cx = CreditCellX(player_wm.x);
    const int player_cy = CreditCellY(player_wm.y);
    player_cell = {player_cx, player_cy};
    player_cell_valid = true;
    if (target.valid && target.on_map) {
        if (target.custom) {
            // Fog points retire when their tile is credited (PruneUncoveredPoints); arriving only
            // starts the clock on whether standing here is going to credit anything at all.
            if (!target.stand_valid) {
                // Nothing creditable to walk to - a waypoint, or fog no square here can reach - so
                // getting to the point itself is all there is to finish it off.
                if (Dist2(player_wm, target.wm) < 2.f * 2.f) {
                    CARTO_LOG("[cartographer] reached custom point wm(%.0f, %.0f)", target.wm.x, target.wm.y);
                    RemoveCustomPointAt(target.wm);
                    ClearTarget();
                }
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
                // Standing inside the ring the client credits unconditionally has to credit. That it
                // did not says our tile index disagrees with the client's, which is a fact about our
                // arithmetic and not about this square - so do not write the square off for it.
                Log::Log("[cartographer] stood in cell (%d, %d) for 15s with no credit - game(%.1f, %.1f) wm(%.4f, %.4f) our_cell(%d, %d): index disagreement, not a dead square\n",
                         target.cx, target.cy, player->pos.x, player->pos.y, player_wm.x, player_wm.y, player_cx, player_cy);
            }
            ClearTarget();
        }
    }

    // Same for a fog point, except the tile that has to credit is the one the player picked: if a
    // wide-range visit has not credited it, demote it so the next resolve sends them closer in.
    if (arrived && target.valid && target.custom && target.stand_valid && TIMER_DIFF(arrived_at) > 15000) {
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
    for (const auto& p : custom_points) {
        const float d2 = Dist2(p.wm, player_wm);
        if (d2 < cand_d2) {
            cand = {true, true, 0, 0, 0, p.wm};
            cand_d2 = d2;
        }
    }
    bool on_current_map = true;
    if (cand.valid) {
        ImRect bounds;
        on_current_map = GW::Map::GetMapWorldMapBounds(map_info, &bounds) && bounds.Contains({cand.wm.x, cand.wm.y});
    }
    else {
        // Ranked by cells-credited-per-square-walked: a spot crediting several is worth extra steps.
        float best_value = 0.f;
        for (const auto& [cell, sc] : probe->cells) {
            if (!sc.reachable || sc.reveals <= 0) continue;
            if (probe->skipped.contains(cell) || declined_cells.contains(cell)) continue;
            const auto centre = CreditCellCenterWorldMap(cell.first, cell.second);
            const float d2 = Dist2(centre, player_wm);
            const float dist_cells = sqrtf(d2) / kWorldMapUnitsPerCell;
            const float value = static_cast<float>(sc.reveals) / (dist_cells + 2.f);
            if (value > best_value) {
                best_value = value;
                cand_d2 = d2;
                cand = {true, false, cell.first, cell.second, sc.reveals, centre};
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
            ? std::ranges::any_of(custom_points, [&](const CustomPoint& p) { return Dist2(p.wm, target.wm) < 1.f; })
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

    if (!on_current_map && !cand.custom) return;
    cand.on_map = on_current_map;
    target = cand;
    arrived = false;
    RefreshCustomTargetStand(player_wm);
    if (target.custom) {
        CARTO_LOG("[cartographer] target: custom point wm(%.0f, %.0f)%s, stand wm(%.0f, %.0f) valid=%d", target.wm.x, target.wm.y,
                  on_current_map ? "" : " on another map", target.stand_wm.x, target.stand_wm.y, target.stand_valid);
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
    if (ImGui::Checkbox("Using a Bird's Eye Compass", &using_bec)) {
        GW::GameThread::Enqueue([] {
            // Terrain has not moved, so the probed tiles stay; the radius only widens which tiles
            // are worth probing, so every map's sweep reopens to cover the new fringe. `strict` is
            // a property of the fog tile, not of the radius - it is merely inert at normal range -
            // so it survives the toggle rather than being learned again from scratch.
            for (auto& [map_id, cached] : probe_cache) {
                cached.complete = false;
            }
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
    ImGui::TextDisabled("Foggy squares: %d reachable, %d out of reach", map_fog_cells, unreachable_fog_cells);
    if (continent_mask.coverable.empty()) {
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
        ImGui::TextDisabled("  continent %d: %d maps, %u standable squares", continent, data.maps, static_cast<unsigned>(data.standable.size()));
    }
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
    else if (target.custom) snprintf(target_desc, sizeof(target_desc), "point(%.0f,%.0f)%s", target.wm.x, target.wm.y, target.stand_valid ? "+stand" : "");
    else snprintf(target_desc, sizeof(target_desc), "stand(%d,%d)+%d", target.cx, target.cy, target.reveals);
    snprintf(buf, len, "carto: enabled=%d target=%s arrived=%d radius=%d skipped=%u probed=%u declined=%u points=%u fogcells=%d marker=%d",
             GetEnabled(), target_desc, arrived, RevealRadius(),
             static_cast<unsigned>(probe->skipped.size()), static_cast<unsigned>(probe->cells.size()),
             static_cast<unsigned>(declined_cells.size()), static_cast<unsigned>(custom_points.size()), map_fog_cells, marker_placed);
}

