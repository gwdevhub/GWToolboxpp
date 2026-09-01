#include "stdafx.h"

#include <bit>
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
#include <Utils/TextUtils.h>
#include <Utils/ToolboxUtils.h>
#include <Widgets/CartographerInternal.h>
#include <Widgets/CartographerWidget.h>
#include <Widgets/MissionMapWidget.h>
#include <Widgets/WorldMapWidget.h>
#include <Windows/TravelWindow.h>
#include <Windows/Pathfinding/Pathing.h>
#include <Windows/Pathfinding/PathfindingWindow.h>
#include <Windows/Pathfinding/PathingMapDataLoader.h>

namespace Carto {

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

    // 世界坐标中北方为 -y。
    const char* CompassDir(const GW::Vec2f& from, const GW::Vec2f& to)
    {
        static constexpr const char* dirs[] = {"东", "东南", "南", "西南", "西", "西北", "北", "东北"};
        const int idx = static_cast<int>(roundf(atan2f(to.y - from.y, to.x - from.x) / (IM_PI / 4.f)));
        return dirs[(idx + 8) % 8];
    }

    bool show_fog = true;
    bool show_stand_cells = true;
    bool show_grid = false;
    bool using_bec = false;
    bool set_quest_marker = true;

    std::map<GW::Constants::MapID, MapProbe> probe_cache;
    // 不同角色的迷雾使不同的格子值得探索，尽管地形未变。
    std::wstring probe_cache_character;

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
            coverage_stale = true; // 没有差异基准，所以完全重建
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
        // 关闭的门会使某些方格无法获得探索计数，因此门状态改变时这些判定失效。
        probe->skipped.clear();
        probe->blocked_planes = std::move(blocked);
        probe->complete = false;
        coverage_stale = true;
        CARTO_LOG("[cartographer] blocked planes changed; re-probing this map");
    }

    // 与 RebuildFog 的 pair 分开，后者在评分后填充并作为重新访问时的整体数据。
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
        if (!EnsureMapRect()) return true; // 无矩形可约束 – 不要隐藏所有内容
        return cx >= map_rect_min.first && cx < map_rect_max.first
            && cy >= map_rect_min.second && cy < map_rect_max.second;
    }

    bool InCreditableBounds(const int cx, const int cy)
    {
        if (!EnsureMapRect()) return true;
        return cx >= map_rect_min.first - kBoundarySlack && cx < map_rect_max.first + kBoundarySlack
            && cy >= map_rect_min.second - kBoundarySlack && cy < map_rect_max.second + kBoundarySlack;
    }

    bool InCreditableBoundsOf(const GW::Constants::MapID map_id, const int cx, const int cy, const int slack)
    {
        const auto info = GW::Map::GetMapInfo(map_id);
        ImRect bounds;
        if (!(info && GW::Map::GetMapWorldMapBounds(info, &bounds))) return false;
        return cx >= CreditCellX(bounds.Min.x) - slack
            && cx < static_cast<int>(ceilf(bounds.Max.x / kWorldMapUnitsPerCell)) + slack
            && cy >= CreditCellY(bounds.Min.y) - slack
            && cy < static_cast<int>(ceilf(bounds.Max.y / kWorldMapUnitsPerCell)) + slack;
    }

    // 若目标在世界地图矩形之外，SetCustomQuestMarker 会降级为旅行标记。
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
        return AnyInRing(fx, fy, 1, [](const int nx, const int ny, int, int) {
            const auto it = probe->cells.find({nx, ny});
            return it != probe->cells.end() && it->second.navmesh;
        });
    }

    bool CellCreditableFrom(const int dx, const int dy, const int fx, const int fy)
    {
        if (abs(dx) <= kRevealRadius && abs(dy) <= kRevealRadius) return InCreditableBounds(fx, fy);
        return CellQualifies(fx, fy);
    }

    std::set<std::pair<int, int>> declined_cells;
    // 放置在迷雾上的点会在该格子被探索后移除；放在已探索地面上的只是一个路径点。
    struct CustomPoint {
        GW::Vec2f wm{};
        bool was_fog = true;
    };
    std::vector<CustomPoint> custom_points;
    std::string declined_cells_str;
    std::string custom_points_str;

    void SerializeDeclined()
    {
        std::vector<std::string> parts;
        for (const auto& [cx, cy] : declined_cells) parts.push_back(std::format("{}:{}", cx, cy));
        declined_cells_str = TextUtils::Join(parts, ",");
    }

    void ParseDeclined()
    {
        declined_cells.clear();
        for (const auto& tok : TextUtils::Split(declined_cells_str, ",")) {
            int cx, cy;
            if (sscanf_s(tok.c_str(), "%d:%d", &cx, &cy) == 2) declined_cells.insert({cx, cy});
        }
    }

    void SerializePoints()
    {
        std::vector<std::string> parts;
        for (const auto& p : custom_points) parts.push_back(std::format("{:.1f}:{:.1f}:{}", p.wm.x, p.wm.y, p.was_fog ? 1 : 0));
        custom_points_str = TextUtils::Join(parts, ",");
    }

    void ParsePoints()
    {
        custom_points.clear();
        for (const auto& tok : TextUtils::Split(custom_points_str, ",")) {
            float x, y;
            int was_fog = 1; // 在标志存在之前写入的点都是在迷雾上
            if (sscanf_s(tok.c_str(), "%f:%f:%d", &x, &y, &was_fog) >= 2) custom_points.push_back({{x, y}, was_fog != 0});
        }
    }

    int unreachable_fog_cells = 0;
    constexpr size_t kUncoverableListMax = 4096;
    std::vector<UncoverableCell> uncoverable_cells;
    // 因无可达格子能探索此点而被跳过的迷雾点选择。
    bool blocked_point = false;

    bool show_unexpected = false;
    bool show_uncoverable = true;
    // 只有传送门穿行才能探索的方格：关闭表示覆盖层针对正常玩法。
    bool allow_gate_glitch = false;
    int explored_tiles = 0;
    int coverable_tiles = 0;
    int coverable_tiles_radius = -1;
    int coverable_tiles_continent = -1;
    int unexpected_tiles = 0;
    constexpr size_t kUnexpectedListMax = 4096;
    std::vector<std::pair<int, int>> unexpected_cells;
    std::vector<FogCell> fog_cells;
    int map_fog_cells = -1;
    std::pair<int, int> map_cell_min{}, map_cell_max{};
    std::pair<int, int> player_cell{};
    bool player_cell_valid = false;

    bool show_whole_continent = true;
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
        // 地面本身存在，而不是什么可达：其旁边的迷雾应读作无法探索。
        continent_mask.raw_any = &src->standable_any;
        continent_mask.any_credit = &src->creditable_any;
        continent_mask.any_raw = &src->standable_any;
        continent_mask.x0 = continent_mask.credit->x0;
        continent_mask.y0 = continent_mask.credit->y0;
        continent_mask.w = continent_mask.credit->width;
        continent_mask.h = continent_mask.credit->height;
    }

    // 地下城不再计数，因此其地面仍会扩散到这些区域。只有列表才能记录这一点。
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

    // 与加载的导航网格无关：您所在的地图不会改变一个方格的价值。
    bool FogCellCoverable(const int cx, const int cy)
    {
        if (TileNeverCredits(cx, cy)) return false;
        return continent_mask.Empty() || continent_mask.Get(cx, cy);
    }

    bool ThisMapCanCredit(const int cx, const int cy)
    {
        if (!probe->complete) return true;
        return AnyInRing(cx, cy, RevealRadius(), [&](const int nx, const int ny, const int dx, const int dy) {
            if (!CellCreditableFrom(dx, dy, cx, cy)) return false;
            const auto it = probe->cells.find({nx, ny});
            return it != probe->cells.end() && it->second.reachable;
        });
    }

    // 可达地面但未获得计数意味着烘焙数据已针对此方格所在的地图进行了裁剪。
    FogSkip WhyNotCoverable(const int cx, const int cy)
    {
        if (TileNeverCredits(cx, cy)) return FogSkip::NeverCredits;
        if (continent_mask.NeedsGlitch(cx, cy)) return FogSkip::GlitchOnly;
        const int r = RevealRadius();
        // 首先检查可站立地面：这意味着地图裁剪丢弃了它，而非地形不可达。
        if (AnyInRing(cx, cy, r, [](const int nx, const int ny, int, int) { return continent_mask.RawGet(nx, ny); })) return FogSkip::PastMapBoundary;
        if (AnyInRing(cx, cy, r, [](const int nx, const int ny, int, int) { return continent_mask.AnyGroundAt(nx, ny); })) return FogSkip::Unreachable;
        return FogSkip::NoGroundInRange;
    }

    // 烘焙已裁剪了每个地图的扩散，因此只有额外的制图师指南针环会遍历原始地面。
    bool StandableWithin(const int cx, const int cy, const int radius, const bool permissive = false)
    {
        const auto* credit = permissive ? continent_mask.any_credit : continent_mask.credit;
        const auto* ground = permissive ? continent_mask.any_raw : continent_mask.raw;
        if (ContinentMask::Sample(credit, cx, cy)) return true;
        return AnyInRing(cx, cy, radius, [&](const int nx, const int ny, const int dx, const int dy) {
            return std::max(abs(dx), abs(dy)) > kMaskRadius && ContinentMask::Sample(ground, nx, ny);
        });
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
                // 宽容：烘焙找到但无法走到的地面仍可解释该方格。
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


    // 立足点位于其格子的任意位置，因此与搜索排序的中心可能相差半个对角线。
    constexpr float kStandOffsetMax = kWorldMapUnitsPerCell * 0.5f * 1.41421356f;

    struct NavCells {
        int x0 = 0, y0 = 0, width = 0, height = 0;
        std::vector<uint8_t> ground; // 可行走，与门无关
        std::vector<uint8_t> standable;
        // 探索按格子计，因此立足点来自哪个梯形无关紧要。
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
            const float y_lo = line_y[cy - y0], y_hi = line_y[cy - y0 + 1]; // y 在转换中翻转
            box_min = {std::min(x_lo, x_hi), std::min(y_lo, y_hi)};
            box_max = {std::max(x_lo, x_hi), std::max(y_lo, y_hi)};
        }
    };
    NavCells nav_cells;

    NavGridInfo GetNavGridInfo()
    {
        return {nav_cells.x0, nav_cells.y0, nav_cells.width, nav_cells.height,
                nav_cells.ground_count, nav_cells.stand_count, nav_cells.built};
    }

    bool NavInGrid(const int cx, const int cy) { return nav_cells.InGrid(cx, cy); }

    bool NavGroundAt(const int cx, const int cy)
    {
        return nav_cells.InGrid(cx, cy) && nav_cells.ground[nav_cells.Index(cx, cy)] != 0;
    }

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
        // 每次调用都在空门状态上重建，这就是它每帧全地图扫描的原因。
        if (!Pathing::CopyBlockedPlanes(blocked)) return false;
        if (nav_cells.built && nav_cells.map_id == map_id && nav_cells.blocked_planes == blocked) return true;
        // 在行走找到玩家之前，每个梯形都被视为可达，这些格子会被保留。
        if (!Pathing::IsReachabilityKnown()) return false;

        const auto started = clock();
        const auto trapezoids = Pathing::GetTrapezoidsWithReachability();
        if (trapezoids.empty()) return false;

        // 几何范围，而非世界地图矩形：探索能延伸到边缘之外，范围是自由的。
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
        // 玩家站在导航网格上，因此缺失单元格意味着梯形到格子的映射错误。
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
        if (!nav_cells.InGrid(cx, cy)) return true; // 不在几何范围内：确实无地面
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
        // `covered` 是已尝试的环，因此宽范围只访问近环未访问的。
        const auto try_ring = [&](const int covered, const int r) {
            std::vector<std::pair<int, int>> candidates;
            ForEachInRing(fx, fy, r, [&](const int nx, const int ny, const int dx, const int dy) {
                if (std::max(abs(dx), abs(dy)) > covered) candidates.push_back({nx, ny});
            });
            std::ranges::sort(candidates, [&from](const auto& a, const auto& b) {
                return Dist2(CreditCellCenterWorldMap(a.first, a.second), from) < Dist2(CreditCellCenterWorldMap(b.first, b.second), from);
            });
            float best_d2 = FLT_MAX;
            bool found = false;
            for (const auto& cell : candidates) {
                const float centre_d2 = Dist2(CreditCellCenterWorldMap(cell.first, cell.second), from);
                // 中心点仅界定了立足点的行走代价，因此不能在第一击中停止。
                if (found && sqrtf(centre_d2) - kStandOffsetMax > sqrtf(best_d2)) break;
                auto it = probe->cells.find(cell);
                if (it == probe->cells.end()) {
                    StandCell sc;
                    if (!ProbeStandCell(cell.first, cell.second, sc)) continue;
                    it = probe->cells.emplace(cell, sc).first;
                    coverage_stale = true; // 由下一次重新计算评分，而非我们——我们在这里没有网格
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
        // 近距探索是无条件的；制图师指南针范围是一个猜测，唯一的反驳需要15秒停留。
        if (try_ring(-1, kRevealRadius)) return true;
        return RevealRadius() > kRevealRadius && CellQualifies(fx, fy) && try_ring(kRevealRadius, RevealRadius());
    }

    bool CellWorthProbing(const CartoGrid& grid, const int cx, const int cy)
    {
        return AnyInRing(cx, cy, RevealRadius(), [&](const int nx, const int ny, int, int) {
            return grid.InGrid(nx, ny) && !grid.IsExplored(nx, ny);
        });
    }

    void SweepStandCells(const CartoGrid& grid, GW::AreaInfo* map_info)
    {
        if (probe->complete) return;
        ImRect bounds;
        if (!(map_info && GW::Map::GetMapWorldMapBounds(map_info, &bounds))) return;
        // 只有此地图的方格是可站立的；它们探索的迷雾可能仍是下一张地图的。
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
                if (!ProbeStandCell(cx, cy, sc)) return; // 导航网格未就绪；未学到任何东西，不保留
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
        ForEachInRing(cell.first, cell.second, RevealRadius(), [&](const int fx, const int fy, const int dx, const int dy) {
            if (!grid.InGrid(fx, fy) || grid.IsExplored(fx, fy)) return;
            if (CellCreditableFrom(dx, dy, fx, fy)) sc.reveals++;
        });
    }

    // 一个格子的评分会统计探索半径内的迷雾，因此只有接近翻转的立足点才会移动。
    void RescoreAround(const CartoGrid& grid, const std::vector<std::pair<int, int>>& changed)
    {
        for (const auto& [fx, fy] : changed) {
            ForEachInRing(fx, fy, RevealRadius(), [&](const int nx, const int ny, int, int) {
                const auto it = probe->cells.find({nx, ny});
                if (it != probe->cells.end()) ScoreStandCell(grid, it->first, it->second);
            });
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
        // 烘焙数据覆盖整个大陆，因此世界地图显示所有值得前往的地面。
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
                    // 仅在地面附近：地图之间的空间从来不是可清除的迷雾。
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


    // 其他地方保留原始点：此地图矩形之外，标记变为旅行标记。
    enum class GoalKind { None, Elsewhere, Waypoint, Stand };

    struct Target {
        bool valid = false;
        bool custom = false;
        // 对于迷雾目标，这些是要去站立的目标格子，而不是要探索的格子。
        int cx = 0;
        int cy = 0;
        int reveals = 0;
        GW::Vec2f wm{};
        // 携带而非重新推导：到达测试比较格子，重新推导有分歧的风险。
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

    // 玩家被送往的位置。空表示无可达地面能探索该目标。
    const GW::Vec2f* TargetGoal()
    {
        if (!target.valid) return nullptr;
        if (!target.custom) return &target.wm;
        return target.goal == GoalKind::Stand ? &target.stand_wm
            : target.goal == GoalKind::Waypoint || target.goal == GoalKind::Elsewhere ? &target.wm
            : nullptr;
    }

    // 自定义任务标记是共享的，因此绝不要碰已经属于别人的标记。
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
        // 在矩形之外，标记会降级为旅行标记，因此不标记该方格。
        if (target.goal == GoalKind::Stand && !StandRoutable(*goal)) {
            if (!warned_stand_off_rect) {
                warned_stand_off_rect = true;
                CARTO_LOG("[cartographer] stand square at wm(%.0f, %.0f) sits outside this map's world-map rectangle; drawing it without a quest marker", goal->x, goal->y);
            }
            ReleaseQuestMarker();
            return;
        }
        if (marker_placed && Dist2(marker_point, target.wm) < 1.f) {
            // 同一点：仅当标记仍属于我们时遵循它，因此手动清除标记会保持。
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

    // 每次扫描重新解析：扫描会学习新内容，且门移动可能使答案失效。
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
        // 探索按格子计，因此点击在格内的位置不携带信息——两次点击是同一个点。
        const GW::Vec2f wm = CreditCellCenterWorldMap(fx, fy);
        const bool foggy = GetCartoGrid(grid) && !grid.IsExplored(fx, fy);
        std::erase_if(custom_points, [&wm](const CustomPoint& p) { return FogTileAt(p.wm) == FogTileAt(wm); });
        custom_points.push_back({wm, foggy});
        SerializePoints();
        // 立即将目标指向您询问的迷雾点，而非最近点。
        target = {};
        target.valid = true;
        target.custom = true;
        target.wm = wm;
        arrived = false;
        RefreshCustomTargetStand(player_wm_cached);
#ifdef _DEBUG
        // 没有整个近环，选择的方格只是一个没有理由的判定。
        Log::Log("[carto-ring] fog tile (%d, %d) wm(%.0f, %.0f) player wm(%.0f, %.0f) radius=%d\n",
                 fx, fy, wm.x, wm.y, player_wm_cached.x, player_wm_cached.y, RevealRadius());
        ForEachInRing(fx, fy, kRevealRadius, [&](const int nx, const int ny, const int dx, const int dy) {
            const std::pair cell{nx, ny};
            const auto it = probe->cells.find(cell);
            const auto centre = CreditCellCenterWorldMap(nx, ny);
            GW::GamePos gp{};
            const bool converted = WorldMapWidget::WorldMapToGamePos(centre, gp);
            Log::Log("[carto-ring]   (%+d,%+d) cell(%d,%d) centre wm(%.0f,%.0f) game(%.0f,%.0f) probed=%d navmesh=%d reachable=%d live_walkable=%d live_reachable=%d dist=%.0f%s\n",
                     dx, dy, nx, ny, centre.x, centre.y, gp.x, gp.y,
                     it != probe->cells.end(),
                     it != probe->cells.end() && it->second.navmesh,
                     it != probe->cells.end() && it->second.reachable,
                     converted && Pathing::IsPositionWalkable(gp),
                     converted && Pathing::IsPositionReachable(gp),
                     sqrtf(Dist2(centre, player_wm_cached)),
                     target.goal == GoalKind::Stand && cell == std::pair{target.stand_cx, target.stand_cy} ? "  <== CHOSEN" : "");
        });
        Log::FlushFile();
#endif
        CARTO_LOG("[cartographer] custom fog point added at wm(%.0f, %.0f)%s", wm.x, wm.y,
                  target.goal == GoalKind::Stand ? ""
                  : target.goal == GoalKind::Waypoint ? " - already explored, marking the point itself"
                  : target.goal == GoalKind::Elsewhere ? " - no ground on this map is near it; travelling instead"
                  : " - this map has ground near it, but none reachable from here");
    }

    // 传送门在此处的阻挡方式与实时可达性行走相同，因此两者一致。
    using PlaneOf = std::unordered_map<const GW::PathingTrapezoid*, size_t>;

    PlaneOf PlaneIndex(const Pathing::PathingMapData& data)
    {
        PlaneOf plane_of;
        for (size_t p = 0; p < data.planes.size(); p++) {
            const auto& plane = data.planes[p];
            for (uint32_t t = 0; t < plane.trapezoid_count; t++) plane_of[&plane.trapezoids[t]] = p;
        }
        return plane_of;
    }

    std::unordered_set<const GW::PathingTrapezoid*> FloodIndexed(const Pathing::PathingMapData& data, const PlaneOf& plane_of,
                                                                 const std::vector<const GW::PathingTrapezoid*>& seeds,
                                                                 const std::vector<Pathing::TravelDoorway>& gates,
                                                                 const bool honour_no_pathing)
    {
        std::unordered_set<const GW::PathingTrapezoid*> comp;
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
                if ((honour_no_pathing && portal.flags & 0x04) || !portal.pair) return;
                for (uint32_t i = 0; i < portal.pair->count; i++) expand(portal.pair->trapezoids[i]);
            };
            expand_portal(trap->portal_left);
            expand_portal(trap->portal_right);
        }
        return comp;
    }

    std::unordered_set<const GW::PathingTrapezoid*> Flood(const Pathing::PathingMapData& data,
                                                          const std::vector<const GW::PathingTrapezoid*>& seeds,
                                                          const std::vector<Pathing::TravelDoorway>& gates,
                                                          const bool honour_no_pathing)
    {
        return FloodIndexed(data, PlaneIndex(data), seeds, gates, honour_no_pathing);
    }

    // 前哨站与其可探索区域共享同一文件，且您会在它们之间传送，因此最大的组件并非可玩区域。
    std::unordered_set<const GW::PathingTrapezoid*> LargestComponent(const Pathing::PathingMapData& data, const PlaneOf& plane_of,
                                                                     const std::vector<Pathing::TravelDoorway>& gates)
    {
        std::unordered_set<const GW::PathingTrapezoid*> seen, best;
        for (const auto& plane : data.planes) {
            for (uint32_t t = 0; t < plane.trapezoid_count; t++) {
                const auto* root = &plane.trapezoids[t];
                if (seen.contains(root)) continue;
                auto comp = FloodIndexed(data, plane_of, {root}, gates, true);
                seen.insert(comp.begin(), comp.end());
                if (comp.size() > best.size()) best = std::move(comp);
            }
        }
        return best;
    }

    // 以您可以进入的门为种子；门不阻止自身的洪水。与 ffna.py 的 entrance_component 镜像。
    void PlayableTrapezoids(const Pathing::PathingMapData& data,
                            std::unordered_set<const GW::PathingTrapezoid*>& gated_out,
                            std::unordered_set<const GW::PathingTrapezoid*>& open_out)
    {
        const auto plane_of = PlaneIndex(data);
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
            const auto comp = FloodIndexed(data, plane_of, seeds, others, true);
            gated_out.insert(comp.begin(), comp.end());
        }
        const auto largest = LargestComponent(data, plane_of, gates);
        gated_out.insert(largest.begin(), largest.end());
        // 传送门穿行对：行走时仿佛传送门不阻挡你。
        const std::vector<const GW::PathingTrapezoid*> seeds(gated_out.begin(), gated_out.end());
        open_out = FloodIndexed(data, plane_of, seeds, {}, true);
    }

    // GetMapIdForLocation 遍历大陆，且矩形重叠：这标记了方格所属的地图。
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

    // 来自 DAT，因为重叠的矩形经常将无地面的地图命名为所在区域。
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
            // 地图的地面可能位于其矩形之外，因此测试它可能探索的区域，而非探索环。
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
                ForEachTileOfTrapezoid(map_id, trap, [&](const int cx, const int cy) {
                    if (abs(cx - tx) > r || abs(cy - ty) > r) return;
                    any = true;
                    owner.connected = owner.connected || component.contains(&trap);
                    owner.under_tile = owner.under_tile || (cx == tx && cy == ty);
                });
            }
        }
        if (!any) return;
        owner.map_id = map_id;
        owner.file_id = file_id;
        owner.travel_to = TravelWindow::GetNearestOutpost(map_id);
        owner_query.owners.push_back(owner);
    }

    // 每帧处理一个地图文件：从 DAT 解析一个文件对帧来说太慢。
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

    // `why_lines` 仅在该方格无法探索时显示：否则 “walkable: false” 读起来像 bug。
    std::string OwnerTooltip(const OwnerQuery& q, const bool why_lines)
    {
        std::string out;
        for (const auto& owner : q.owners) {
            const auto& name = Resources::GetMapName(owner.map_id)->string();
            const auto& travel = Resources::GetMapName(owner.travel_to)->string();
            if (!out.empty()) out += "\n";
            out += std::format("{}（地图 {}，文件 0x{:X}）", name.empty() ? "未命名地图" : name.c_str(),
                               static_cast<int>(owner.map_id), owner.file_id);
            out += owner.travel_to == GW::Constants::MapID::None || travel.empty() ? "\n没有前哨站可传送至此" : "\n传送至 " + travel;
            if (why_lines) {
                out += std::format("\n可行走：{}\n可探索：{}",
                                   owner.under_tile ? "是" : "否", owner.connected ? "是" : "否");
            }
        }
        return out + std::format("\n未探索的方格（{}，{}）", q.cell.first, q.cell.second);
    }



    void OnCartographyUpdated(GW::HookStatus*, GW::UI::UIMessage, void*, void*)
    {
        carto_dirty = true;
    }

    void BuildStatusText(char* buf, const size_t len)
    {
        static constexpr const char* no_goal = "没有可到达的方格能探索您的迷雾点 - 在地图上右键单击移除它";
        if (!target.valid) {
            const char* idle = blocked_point ? no_goal
                : map_fog_cells == 0 ? "此地图上已无迷雾可探索"
                : !probe->complete ? "正在扫描此地图的迷雾..."
                : "此处已无可到达的迷雾 - 前往别处，或添加迷雾点";
            snprintf(buf, len, "%s", idle);
            return;
        }
        if (arrived) {
            snprintf(buf, len, target.custom
                         ? "已站在您迷雾点所在的方格 - 如果未计探索，请移动一步或用鼠标点走"
                         : "已站在目标方格 - 如果未计探索，请移动一步或用鼠标点走");
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
                snprintf(buf, len, "站在您迷雾点所在的方格，距您 %s %.1fk 单位", CompassDir(player_wm_cached, *goal), dist_k);
                return;
            }
            snprintf(buf, len, "前往您的迷雾点，距您 %s %.1fk 单位%s", CompassDir(player_wm_cached, *goal), dist_k,
                     target.goal == GoalKind::Elsewhere ? "（另一张地图）" : "");
            return;
        }
        snprintf(buf, len, "站在目标方格，距您 %s %.1fk 单位，可探索 %d 个%s%s", CompassDir(player_wm_cached, *goal), dist_k,
                 target.reveals, target.reveals == 1 ? "方格" : "方格",
                 blocked_point ? "（此处的迷雾点无可到达的地面能探索它）" : "");
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
            ImGui::TextColored(ImColor(kTargetColor).Value, ICON_FA_MAP_MARKED_ALT " 制图师");
            char status[224];
            BuildStatusText(status, sizeof(status));
            ImGui::TextDisabled("%s", status);
            if (map_fog_cells > 0) ImGui::TextDisabled("此地图剩余 %d 个未探索方格", map_fog_cells);

            const float near_dist = px_per_wm_unit > 0.f ? 12.f / px_per_wm_unit : 8.f;
            const int point_here = FindCustomPointNear(click_wm, near_dist);
            // 在已建议的位置上再添加迷雾点是无意义的。
            const bool on_suggestion = target.valid && !target.custom
                && CreditCellAt(click_wm) == std::pair{target.cx, target.cy};
            if (target.valid && (on_suggestion || (target.custom && point_here >= 0))) {
                if (ImGui::Button(target.custom ? "移除这个迷雾点" : "跳过此建议", item_size)) {
                    CartographerWidget::SkipCurrentTarget(false);
                    keep_open = false;
                }
                if (!target.custom && ImGui::Button("不再建议此位置", item_size)) {
                    CartographerWidget::SkipCurrentTarget(true);
                    keep_open = false;
                }
            }
            else if (point_here >= 0) {
                if (ImGui::Button("移除迷雾点", item_size)) {
                    CartographerWidget::RemoveCustomPointNear(click_wm, near_dist);
                    keep_open = false;
                }
            }
            else {
                if (ImGui::Button("在此添加迷雾点", item_size)) {
                    CartographerWidget::AddCustomPoint(click_wm);
                    keep_open = false;
                }
            }
            if (custom_points.size() > 1) {
                char label[48];
                snprintf(label, sizeof(label), "清除所有 %u 个迷雾点", static_cast<unsigned>(custom_points.size()));
                if (ImGui::Button(label, item_size)) {
                    CartographerWidget::ClearCustomPoints();
                    keep_open = false;
                }
            }
#ifdef _DEBUG
            if (ImGui::Button("记录此处的辅助信息", item_size)) {
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

    // 返回指针是否在方格上，并返回矩形供调用者绘制。
    bool DrawCell(ImDrawList* dl, const ProjectToScreen project, const int cx, const int cy, const ImU32 colour,
                  const int fill_alpha, const int edge_alpha, const float thickness, const ImVec2& mouse,
                  ImRect* rect_out = nullptr)
    {
        ImVec2 cell_min, cell_max;
        if (!ProjectCell(project, cx, cy, cell_min, cell_max)) return false;
        const ImRect rect(cell_min, cell_max);
        if (!ImRect(dl->GetClipRectMin(), dl->GetClipRectMax()).Overlaps(rect)) return false;
        dl->AddRectFilled(cell_min, cell_max, WithAlpha(colour, fill_alpha));
        dl->AddRect(cell_min, cell_max, WithAlpha(colour, edge_alpha), 0.f, 0, thickness);
        if (rect_out) *rect_out = rect;
        return rect.Contains(mouse);
    }

    // 每个迷雾纹素一个四边形，让 ImGui 像 GPU 采样纹理时那样插值。
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
        // 仅在方格自身矩形内：否则“不属于当前加载的地图”是显然的。
        if (InMapBounds(hovered->cx, hovered->cy) && !ThisMapCanCredit(hovered->cx, hovered->cy)) {
            warning_out = "从当前加载的地图无法探索此方格";
        }
        RequestOwnerQuery(hovered->cx, hovered->cy);
        const auto* resolved = FinishedOwnerQuery(hovered->cx, hovered->cy);
        if (!resolved) {
            tooltip_out = std::format("未探索的方格（{}，{}）\n正在读取地图文件以寻找能探索它的地面...", hovered->cx, hovered->cy);
            return;
        }
        if (!resolved->owners.empty()) {
            tooltip_out = OwnerTooltip(*resolved, false);
            return;
        }
        tooltip_out = std::format("未探索的方格（{}，{}）\n没有地图在探索范围内拥有可探索它的地面", hovered->cx, hovered->cy);
        const auto rect_name = MapRectName(hovered->cx, hovered->cy);
        if (!rect_name.empty()) tooltip_out += "\n它所在的世界地图矩形属于 " + rect_name;
        if (resolved->out_of_reach_maps) {
            tooltip_out += std::format("\n{} 张附近地图被跳过：此方格距其边界超过一圈，这是地图能探索的最远距离", resolved->out_of_reach_maps);
        }
        if (resolved->unreadable) tooltip_out += "\n其中一些地图文件不在您的 Gw.dat 中";
    }

    // 每个方格作为一个整体被探索，因此看到边界才能让“站在那”可操作。
    void DrawGrid(ImDrawList* dl, const ProjectToScreen project)
    {
        const auto [x0, y0] = map_cell_min;
        const auto [x1, y1] = map_cell_max;
        if (x1 <= x0 || y1 <= y0) return;
        ImVec2 origin, corner;
        if (!ProjectCell(project, x0, y0, origin, corner)) return;
        const float step_x = corner.x - origin.x;
        const float step_y = corner.y - origin.y;
        if (step_x < 3.f || step_y < 3.f) return; // 更密集会糊成一片，不是网格
        // 两个投影都是仿射的，因此远边由步长而非第二次投影决定。
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

    // 绘制而非省略：空白区域会被读作“已完成”。黄色表示仅传送门穿行。
    void DrawUncoverableCells(ImDrawList* dl, const ProjectToScreen project, const ImVec2& mouse, std::string& tooltip_out)
    {
        for (const auto& [cx, cy, why] : uncoverable_cells) {
            const auto colour = why == FogSkip::GlitchOnly ? kGlitchOnlyColor : kUncoverableColor;
            if (!DrawCell(dl, project, cx, cy, colour, 60, 150, 1.f, mouse)) continue;
            // 地面所属的地图是值得知道的答案，因此同样进行 DAT 查找。
            RequestOwnerQuery(cx, cy);
            const auto* resolved = FinishedOwnerQuery(cx, cy);
            if (!resolved) {
                tooltip_out = std::format("未探索的方格（{}，{}）\n正在读取地图文件以寻找能探索它的地面...", cx, cy);
                continue;
            }
            if (!resolved->owners.empty()) {
                tooltip_out = OwnerTooltip(*resolved, true);
                continue;
            }
            tooltip_out = std::format("未探索的方格（{}，{}）\n没有地图在其 %d 格范围内拥有地面", cx, cy, RevealRadius());
            const auto rect_name = MapRectName(cx, cy);
            if (!rect_name.empty()) tooltip_out += "\n其世界地图矩形属于 " + rect_name;
        }
    }

    // 已探索的方格，但烘焙数据说其范围内无任何可站立地面。
    void DrawUnexpectedCells(ImDrawList* dl, const ProjectToScreen project, const ImVec2& mouse, std::string& tooltip_out)
    {
        for (const auto& [cx, cy] : unexpected_cells) {
            if (!DrawCell(dl, project, cx, cy, kUnexpectedColor, 40, 190, 1.f, mouse)) continue;
            const auto rect_name = MapRectName(cx, cy);
            tooltip_out = std::format("已探索的方格（{}，{}）但在其 %d 格范围内没有烘焙地面", cx, cy, RevealRadius());
            tooltip_out += "\n其世界地图矩形属于 " + rect_name;
        }
    }

    void DrawStandCells(ImDrawList* dl, const ProjectToScreen project, const ImVec2& mouse, const char*& tooltip)
    {
        for (const auto& [cell, sc] : probe->cells) {
            if (!sc.reachable || sc.reveals <= 0) continue;
            if (declined_cells.contains(cell)) continue;
            // 仅在建议绘制在顶部时跳过，否则所有权重新检查会将其清空。
            if (target.valid && !target.custom && target.cx == cell.first && target.cy == cell.second) continue;
            const int strength = std::min(sc.reveals, 9);
            if (DrawCell(dl, project, cell.first, cell.second, kStandColor, 10 + 6 * strength, 60 + 12 * strength, 1.f, mouse)) {
                tooltip = "制图师：站在此方格以探索附近的迷雾";
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
        // 范围以格子为单位，而非点，因此角色标记无法回答此问题。
        if (player_cell_valid) {
            DrawCell(dl, project, player_cell.first, player_cell.second, kCurrentTileColor, 28, 150, 1.f, mouse);
        }
        const bool target_active = target.valid;
        const bool point_stand = target_active && target.custom && target.goal == GoalKind::Stand;
        if (target_active && (!target.custom || point_stand)) {
            const float pulse = Pulse();
            const int tcx = target.custom ? target.stand_cx : target.cx;
            const int tcy = target.custom ? target.stand_cy : target.cy;
            ImRect rect(FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX);
            const bool hovered = DrawCell(dl, project, tcx, tcy, kTargetColor, 16 + static_cast<int>(24.f * pulse), 210, 1.5f + pulse, mouse, &rect);
            // 指向迷雾点的引导线，以明确方格并非迷雾所在。
            ImVec2 point_at;
            if (point_stand && rect.Min.x != FLT_MAX && project(target.wm, point_at)) {
                dl->AddLine(rect.GetCenter(), point_at, WithAlpha(kFogPointColor, 140), 1.f);
            }
            if (cell_tooltip && hovered) {
                tooltip = point_stand ? "制图师：站在此方格以探索您的迷雾点"
                                      : "制图师：站在此方格以探索周围的迷雾\n在地图上右键单击可查看选项";
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
                tooltip = "制图师迷雾点\n在附近右键单击可移除它";
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
        snprintf(line, sizeof(line), ICON_FA_MAP_MARKED_ALT " 制图师：%s", status);
        dl->AddText({16.f, dl->GetClipRectMax().y - 68.f}, ImGui::GetColorU32(ImGuiCol_Text), line);
    }

    void OnMissionMapOverlayDraw(ImDrawList* dl)
    {
        if (!CartographerWidget::GetEnabled() || !map_on_world_map) return;
        DrawMapOverlay(dl, [](const GW::Vec2f& wm, ImVec2& out) { return MissionMapWidget::WorldMapToScreen(wm, out); }, false);
    }
    // 一张表供两者使用，因此设置不会在一处添加而在另一处遗漏。
    struct Option {
        const char* setting;
        const char* label;
        bool* flag;
        void (*on_change)();
        const char* help;
    };

    const Option kOptions[] = {
        {"show_fog", "显示剩余迷雾", &show_fog, nullptr,
         "绿色：所有仍未探索且此地图上某个方格可以探索的迷雾。此地图无法到达的迷雾不会绘制。"},
        {"show_stand_cells", "显示可站立格子", &show_stand_cells, nullptr,
         "绘制每个 32x32 的值得走进的方格，按站立其中可探索的迷雾格数着色。当前建议的方格以轮廓和脉冲高亮。"},
        {"show_whole_continent", "显示整个大陆", &show_whole_continent, [] { coverage_stale = true; },
         "使用游戏地图文件烘焙的数据，绘制此大陆上所有仍值得探索的方格，而不限于您所在的地图。关闭此选项则仅显示当前地图。"},
        {"show_grid", "显示探索网格", &show_grid, nullptr,
         "绘制 32x32 的格子边界。探索按整个格子计数，因此这告诉您实际站在哪个格子里。缩放太远时网格会消失以避免模糊。"},
        {"show_uncoverable", "显示无法探索的方格", &show_uncoverable, [] { coverage_stale = true; },
         "绘制那些此大陆没有任何地面能探索的迷雾方格，并附上原因说明：灰色表示永远无法到达，黄色表示只有传送门穿行可以。不显示它们时，世界地图会显示一块空白，看起来像已探索。"},
        {"allow_gate_glitch", "计入传送门穿行", &allow_gate_glitch,
         [] {
             Pathing::SetGateGlitchAllowed(allow_gate_glitch);
             coverage_stale = true;
         },
         "传送门通常阻止您走到另一边，因此门后的地面不可达，其能探索的方格会显示为黄色。如果您通过暗影步穿过门且它们计入正常探索，则启用此选项。适用于烘焙表和实时覆盖层，两者保持一致。"},
        {"show_unexpected", "显示意外已探索方格", &show_unexpected, nullptr,
         "绘制您已探索但烘焙地图数据表示其探索范围内没有可站立地面（即使仅传送门穿行可达的地面也没有）的方格，因此不应该有任何东西能探索它。要么烘焙缺少该地面，要么它是从烘焙未建模的位置探索的。探索范围遵循下方的制图师指南针设置。"},
        {"using_bec", "使用制图师指南针", &using_bec,
         [] {
             // 半径仅扩大值得探索的格子范围；`strict` 是迷雾格子属性，保留不变。
             for (auto& [map_id, cached] : probe_cache) cached.complete = false;
             owner_cache.clear();
             owner_query = {};
             coverage_stale = true;
         },
         "站在一个格子内会探索它及其周围的 8 个格子（切比雪夫距离，即方形区域——这就是为什么最近的点通常不是正确的位置）。制图师指南针将此范围扩大到每个方向 3 格。站在格子内的具体位置不影响结果。重新扫描地图。"},
        {"set_quest_marker", "为迷雾点设置任务标记", &set_quest_marker, [] { SyncQuestMarker(); },
         "放置迷雾点会在您需要站立的格子上放置一个自定义任务标记，因此常规任务路径会引导您到达那里。到达或移除点后标记会自行清除，手动清除标记会保持清除。建议的方格不会触及标记。"},
    };
} // namespace Carto

using namespace Carto;

void CartographerWidget::Initialize()
{
    ToolboxWidget::Initialize();
    for (const auto& opt : kOptions) SettingsRegistry::RegisterField(this, opt.setting, opt.flag);
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

    // 这里所有坐标都是世界地图坐标，因此不在世界地图上的地图无需计算。
    const auto map_info = GW::Map::GetMapInfo(map_id);
    map_on_world_map = map_info && map_info->GetIsOnWorldMap();
    if (!map_on_world_map) return;

    // 地图变更后坐标锚点可能过渡，让其稳定。
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
    // 完成扫描仍需要最后一次完整遍历，因此在扫描之前读取标志。
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
        // 每个 d 现在必须在 [-r, r] 内；否则表示此类错误再次出现。
        Log::Log("[carto-reveal] map=%d game(%.1f, %.1f) wm(%.4f, %.4f) credit_cell(%d, %d) fog_tile(%d, %d) tile(%d, %d) d(%d, %d) r=%d\n",
                 static_cast<int>(map_id), player->pos.x, player->pos.y, player_wm.x, player_wm.y,
                 our_cx, our_cy, fog_cx, fog_cy, tx, ty, tx - our_cx, ty - our_cy, RevealRadius());
    }
    if (!changed.empty()) Log::FlushFile();
#endif
    if (coverage_stale || sweeping) {
        // 从头重建会取代任何待处理的差异，因此重新基准快照。
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

    // 到达是指站在方格内，而非接近目标——在悬崖上可能相差一格。
    const int player_cx = CreditCellX(player_wm.x);
    const int player_cy = CreditCellY(player_wm.y);
    player_cell = {player_cx, player_cy};
    player_cell_valid = true;
    if (target.valid) {
        if (target.custom) {
            // 迷雾点在其格子被探索后移除；到达只启动计时。
            if (target.goal == GoalKind::Waypoint) {
                // 已探索的地面，因此到达该点本身即完成。
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
            // 离开停止计时：下面的判定是关于站在这里，而非到达这里。
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

    // 探索可能需要移动一步或点击行走，因此给方格足够时间再下结论。
    if (arrived && target.valid && !target.custom && TIMER_DIFF(arrived_at) > 15000) {
        const auto it = probe->cells.find({target.cx, target.cy});
        if (it != probe->cells.end() && it->second.reveals > 0) {
            // 宽范围访问未探索任何东西通常是因为它尝试了只有正常范围才能探索的格子。
            const int r = RevealRadius();
            int demoted = 0;
            ForEachInRing(target.cx, target.cy, r, [&](const int nx, const int ny, const int dx, const int dy) {
                if (abs(dx) <= kRevealRadius && abs(dy) <= kRevealRadius) return;
                if (!grid.InGrid(nx, ny) || grid.IsExplored(nx, ny)) return;
                // 仅归咎于该方格评分时计数的格子：被排除的格子并非本次访问应探索的。
                if (CellCreditableFrom(dx, dy, nx, ny) && probe->strict.insert({nx, ny}).second) demoted++;
            });
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

    // 同理对迷雾点，除了必须探索的是玩家选择的格子。
    if (arrived && target.valid && target.custom && target.goal == GoalKind::Stand && TIMER_DIFF(arrived_at) > 15000) {
        const std::pair cell = FogTileAt(target.wm);
        const int dx = target.stand_cx - cell.first;
        const int dy = target.stand_cy - cell.second;
        // 已在正常范围内仍未探索：没有更近的格子可送他去。
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
    // 没有任何东西能探索的点不能阻塞队列，因此选择会跳过它。
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
        // 按每行走一格可探索的格子数排序：可探索多个格子的点值得多走几步。
        float best_value = 0.f;
        for (const auto& [cell, sc] : probe->cells) {
            if (!sc.reachable || sc.reveals <= 0) continue;
            if (probe->skipped.contains(cell) || declined_cells.contains(cell)) continue;
            // 立足点位置，而非格子中心：在海岸线狭缝上，中心是水。
            GW::Vec2f stand;
            if (!WorldMapWidget::GamePosToWorldMap(sc.pos, stand)) continue;
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
        // 滞后：除非当前目标变得不合格或候选明显更近，否则保留当前目标。
        const auto current = probe->cells.find({target.cx, target.cy});
        const bool current_eligible = target.custom
            ? target.goal != GoalKind::None && std::ranges::any_of(custom_points, [&](const CustomPoint& p) { return Dist2(p.wm, target.wm) < 1.f; })
            : current != probe->cells.end() && current->second.reachable && current->second.reveals > 0
            && !probe->skipped.contains({target.cx, target.cy}) && !declined_cells.contains({target.cx, target.cy});
        if (current_eligible && cand_d2 >= 0.7f * Dist2(target.wm, player_wm)) same = true;
    }
    if (same) {
        // 同一方格，但其周围的迷雾可能缩小了，状态行会引用它。
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
    for (const auto& opt : kOptions) {
        if (ImGui::Checkbox(opt.label, opt.flag) && opt.on_change) {
            GW::GameThread::Enqueue([on_change = opt.on_change] { on_change(); });
        }
        ImGui::ShowHelp(opt.help);
    }
}

void CartographerWidget::Draw(IDirect3DDevice9*)
{
    // 在任务地图上切换，因此可以在游戏过程中打开辅助而无需打开设置。
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
            ImGui::SetTooltip(visible ? "制图师已激活。点击隐藏。" : "制图师已隐藏。点击显示。");
        }
        ImGui::PopStyleColor();
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
}

void CartographerWidget::DrawSettingsInternal()
{
    ImGui::TextDisabled("探索按 32x32 世界地图方格计数：站在方格内的任意位置会探索它及其周围的方格。\n此功能会计算您可以站立哪些方格、其中哪些能探索未探索的迷雾，并在世界地图和任务地图上绘制它们，最值得前往的方格会高亮。前往那里由您自己完成。");
    ImGui::Separator();
    bool on = GetEnabled();
    if (ImGui::Checkbox("启用", &on)) {
        GW::GameThread::Enqueue([on] { SetEnabled(on); });
    }
    ImGui::ShowHelp("也可通过任务地图上的按钮或世界地图自身的制图师复选框切换。");
    DrawWorldMapOptions();

    ImGui::Separator();
    ImGui::TextDisabled("在世界地图或任务地图上右键单击可跳过建议或添加自己的迷雾点。");
    ImGui::Text("永久跳过的方格：%u", static_cast<unsigned>(declined_cells.size()));
    ImGui::SameLine();
    if (ImGui::SmallButton("清除##declined")) ClearDeclined();
    ImGui::Text("自定义迷雾点：%u", static_cast<unsigned>(custom_points.size()));
    ImGui::SameLine();
    if (ImGui::SmallButton("清除##points")) ClearCustomPoints();

#ifdef _DEBUG
    // 在早期返回之前：重新烘焙不应需要打开小部件。
    Carto::DrawBakeSettings();
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
    ImGui::TextDisabled("此地图：已探索 %u 个方格，%u 个可站立，%u 个值得前往", static_cast<unsigned>(probe->cells.size()), standable, useful);
    ImGui::TextDisabled("迷雾方格：%d 个可到达，%d 个无法探索", map_fog_cells, unreachable_fog_cells);
    if (coverable_tiles > 0) {
        ImGui::TextDisabled("此大陆：已探索 %d 个方格，烘焙数据可探索 %d 个（半径 %d），完成度 %.2f%%",
                            explored_tiles, coverable_tiles, RevealRadius(), 100.f * explored_tiles / coverable_tiles);
    }
    else {
        ImGui::TextDisabled("此大陆：已探索 %d 个方格", explored_tiles);
    }
    ImGui::Text("烘焙数据未预期的已探索方格：%d", unexpected_tiles);
    ImGui::ShowHelp("您已探索的方格，但烘焙数据在探索范围内（包括仅传送门穿行可达的地面）没有可站立地面。启用“显示意外已探索方格”可在世界地图上查看它们的位置。");
    if (unexpected_tiles > 0 && ImGui::TreeNodeEx("列出它们##unexpected", ImGuiTreeNodeFlags_NoTreePushOnOpen)) {
        if (static_cast<size_t>(unexpected_tiles) > unexpected_cells.size()) {
            ImGui::TextDisabled("仅显示前 %u 个。", static_cast<unsigned>(unexpected_cells.size()));
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
        ImGui::TextDisabled("此大陆无烘焙数据 - 仅显示当前地图。");
    }
    else {
        ImGui::TextDisabled("烘焙大陆数据：%dx%d 方格，位于 (%d,%d)，半径 %d",
                            continent_mask.w, continent_mask.h, continent_mask.x0, continent_mask.y0, kMaskRadius);
    }
}


#ifdef _DEBUG
void CartographerWidget::SetGateGlitchAllowed(const bool allowed)
{
    allow_gate_glitch = allowed;
    Pathing::SetGateGlitchAllowed(allowed);
    coverage_stale = true;
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
