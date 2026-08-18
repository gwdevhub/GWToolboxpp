#include "stdafx.h"

#include <fstream>
#include <map>

#include <GWCA/Constants/Constants.h>
#include <GWCA/Context/WorldContext.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Map.h>
#include <GWCA/GameEntities/Quest.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/MapMgr.h>

#include <ImGuiAddons.h>
#include <Logger.h>
#include <Timer.h>
#include <Modules/CartographerModule.h>
#include <Modules/QuestModule.h>
#include <Modules/Resources.h>
#include <Utils/SettingsRegistry.h>
#include <Utils/ToolboxUtils.h>
#include <Widgets/MissionMapWidget.h>
#include <Widgets/WorldMapWidget.h>
#include <Windows/Pathfinding/Pathing.h>
#include <Windows/Pathfinding/PathfindingWindow.h>

namespace {
    bool enabled = true;

    // Fog is one bit per 32x32-world-map-unit cell in WorldContext::cartographed_areas, sized by
    // the two dwords right after it (h05B4). Gw.exe's fog mesh builder is handed exactly those
    // two (+0x5A4 and +0x5B4) and addresses them as below.
    constexpr float kWorldMapUnitsPerCell = 32.f;

    // Rows are word-aligned, and the client's shift truncates - a width that is not a multiple
    // of 32 makes each row's tail columns unaddressable, so this is not a flat bit index.
    uint32_t RowWords(const uint32_t width)
    {
        return width >> 5;
    }

    struct CartoGrid {
        const uint32_t* bits = nullptr;
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t dword_count = 0;

        // Strict bit semantics matching the client's fog mesh builder: anything without a set
        // bit (out of grid, beyond the synced array, empty array) counts as UNexplored — the
        // game draws fog there, so must we. Target eligibility is gated separately.
        bool IsExplored(const int cx, const int cy) const
        {
            if (cx < 0 || cy < 0 || static_cast<uint32_t>(cx) >= width || static_cast<uint32_t>(cy) >= height) return false;
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

    GW::Vec2f CellCenterWorldMap(const int cx, const int cy)
    {
        return {(cx + .5f) * kWorldMapUnitsPerCell, (cy + .5f) * kWorldMapUnitsPerCell};
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

    bool show_cappable_fog = true;
    bool show_stand_cells = true;

    // Credit is granted by cell, not by proximity: standing in a cell credits it and every cell
    // within this many rings (Chebyshev, so a square block). Working theory, matching observed
    // behaviour but unconfirmed, hence a setting - a Bonus Explorer's Cape appears to add two.
    int reveal_radius_cells = 1;

    int RevealRadius()
    {
        return std::clamp(reveal_radius_cells, 1, 3);
    }

    struct StandCell {
        bool walkable = false;
        GW::GamePos pos{}; // somewhere inside the cell you can actually stand
        int reveals = 0;   // still-foggy cappable cells this spot would credit
    };
    std::map<std::pair<int, int>, StandCell> stand_cells;
    bool sweep_complete = false;
    std::set<std::pair<int, int>> skipped_cells;
    std::set<std::pair<int, int>> declined_cells;
    std::vector<GW::Vec2f> custom_points;
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
            custom_points_str += std::format("{:.1f}:{:.1f}", p.x, p.y);
        }
    }

    void ParsePoints()
    {
        custom_points.clear();
        std::istringstream is(custom_points_str);
        std::string tok;
        while (std::getline(is, tok, ',')) {
            float x, y;
            if (sscanf_s(tok.c_str(), "%f:%f", &x, &y) == 2) custom_points.push_back({x, y});
        }
    }

    constexpr float kWmPerTexel = 8.f;

    struct CappableMask {
        int layer = -1;
        int w = 0, h = 0;
        std::vector<uint8_t> bits;
        int tw = 0, th = 0;
        std::vector<uint8_t> texel_frac;
        int cw = 0, ch = 0;
        std::vector<uint8_t> cell_bits;
    };
    std::shared_ptr<const CappableMask> cappable_mask;
    int cappable_mask_pending = -1;

    bool MaskBit(const std::vector<uint8_t>& bits, const size_t bit)
    {
        return bits[bit >> 3] >> (bit & 7) & 1;
    }

    int TexelFrac(const int tx, const int ty)
    {
        const auto m = cappable_mask.get();
        if (!m || tx < 0 || ty < 0 || tx >= m->tw || ty >= m->th) return 0;
        return m->texel_frac[static_cast<size_t>(ty) * m->tw + tx];
    }

    bool CellCappable(const int cx, const int cy)
    {
        const auto m = cappable_mask.get();
        if (!m || cx < 0 || cy < 0 || cx >= m->cw || cy >= m->ch) return false;
        return MaskBit(m->cell_bits, static_cast<size_t>(cy) * m->cw + cx);
    }

    bool MaskCappableAtWm(const GW::Vec2f& wm)
    {
        const auto m = cappable_mask.get();
        const int x = static_cast<int>(floorf(wm.x));
        const int y = static_cast<int>(floorf(wm.y));
        if (!m || x < 0 || y < 0 || x >= m->w || y >= m->h) return false;
        return MaskBit(m->bits, static_cast<size_t>(y) * m->w + x);
    }

    bool MaskUsable()
    {
        return cappable_mask && !cappable_mask->bits.empty();
    }

    // Foggy cells no standable cell can credit; drawn as explored so the overlay only ever shows
    // fog the player can do something about.
    std::set<std::pair<int, int>> uncoverable_cells;
    int map_fog_cells = -1;

    // Everything counts as coverable until the sweep finishes, so the overlay does not blink
    // cells out and back in as probing progresses.
    bool FogCellCoverable(const int cx, const int cy)
    {
        if (!sweep_complete) return true;
        const int r = RevealRadius();
        for (int dy = -r; dy <= r; dy++) {
            for (int dx = -r; dx <= r; dx++) {
                const auto it = stand_cells.find({cx + dx, cy + dy});
                if (it != stand_cells.end() && it->second.walkable) return true;
            }
        }
        return false;
    }

    struct CartoSnapshot {
        std::vector<uint32_t> dwords;
        uint32_t width = 0;
        uint32_t height = 0;

        bool IsExplored(const int cx, const int cy) const
        {
            if (cx < 0 || cy < 0 || static_cast<uint32_t>(cx) >= width || static_cast<uint32_t>(cy) >= height) return false;
            const uint32_t word = static_cast<uint32_t>(cy) * RowWords(width) + (static_cast<uint32_t>(cx) >> 5);
            if (word >= dwords.size()) return false;
            return (dwords[word] >> (static_cast<uint32_t>(cx) & 31)) & 1;
        }

        void MarkExplored(const int cx, const int cy)
        {
            if (cx < 0 || cy < 0 || static_cast<uint32_t>(cx) >= width || static_cast<uint32_t>(cy) >= height) return;
            const uint32_t word = static_cast<uint32_t>(cy) * RowWords(width) + (static_cast<uint32_t>(cx) >> 5);
            if (word < dwords.size()) dwords[word] |= 1u << (static_cast<uint32_t>(cx) & 31);
        }
    };

    struct FogTexState {
        IDirect3DTexture9* tex = nullptr;
        IDirect3DTexture9* retired = nullptr;
        int tw = 0, th = 0;
        int layer = -1;
        uint64_t built_hash = 0;
        bool building = false;
        bool shutdown = false;
    };
    FogTexState fog_tex;

    constexpr uint32_t kCappableFogRgb = 0x50FF78;
    constexpr float kFogTexMaxAlpha = 135.f;

    uint64_t HashCartoBits(const CartoGrid& grid, const int layer)
    {
        uint64_t h = 1469598103934665603ull;
        const auto mix = [&h](const uint64_t v) { h = (h ^ v) * 1099511628211ull; };
        mix(static_cast<uint64_t>(layer));
        mix(grid.width);
        mix(grid.height);
        for (uint32_t i = 0; grid.bits && i < grid.dword_count; i++) mix(grid.bits[i]);
        return h;
    }

    // The client draws one quad per cell with a per-vertex value averaged over the four cells
    // meeting at that vertex, then lets the GPU interpolate; box filter first, interpolate
    // second is what makes this line up with the fog the player actually sees.
    float ExploredAtCorner(const CartoSnapshot& g, const int cx, const int cy)
    {
        return (static_cast<float>(g.IsExplored(cx - 1, cy - 1)) + static_cast<float>(g.IsExplored(cx, cy - 1)) +
                static_cast<float>(g.IsExplored(cx - 1, cy)) + static_cast<float>(g.IsExplored(cx, cy))) * 0.25f;
    }

    float UnexploredCoverageAtWm(const CartoSnapshot& g, const float wmx, const float wmy)
    {
        const float u = wmx / kWorldMapUnitsPerCell;
        const float v = wmy / kWorldMapUnitsPerCell;
        const int cx = static_cast<int>(floorf(u));
        const int cy = static_cast<int>(floorf(v));
        const float fx = u - cx;
        const float fy = v - cy;
        const float tl = ExploredAtCorner(g, cx, cy);
        const float tr = ExploredAtCorner(g, cx + 1, cy);
        const float bl = ExploredAtCorner(g, cx, cy + 1);
        const float br = ExploredAtCorner(g, cx + 1, cy + 1);
        const float explored = (tl + (tr - tl) * fx) * (1.f - fy) + (bl + (br - bl) * fx) * fy;
        return 1.f - explored;
    }

    void BuildFogTexture(const CartoGrid& grid)
    {
        const auto mask = cappable_mask;
        if (!mask || mask->bits.empty() || !mask->tw || !mask->th) return;
        if (fog_tex.building || fog_tex.shutdown) return;
        uint64_t hash = HashCartoBits(grid, mask->layer);
        const std::vector<std::pair<int, int>> ghost_cells(uncoverable_cells.begin(), uncoverable_cells.end());
        for (const auto& [cx, cy] : ghost_cells) {
            hash = (hash ^ (static_cast<uint64_t>(cx) << 20 ^ static_cast<uint64_t>(cy))) * 1099511628211ull;
        }
        if (fog_tex.tex && fog_tex.built_hash == hash && fog_tex.layer == mask->layer) return;
        fog_tex.building = true;
        auto snap = std::make_shared<CartoSnapshot>();
        snap->width = grid.width;
        snap->height = grid.height;
        if (grid.bits && grid.dword_count) snap->dwords.assign(grid.bits, grid.bits + grid.dword_count);
        for (const auto& [cx, cy] : ghost_cells) {
            snap->MarkExplored(cx, cy);
        }
        Resources::EnqueueWorkerTask([mask, snap, hash] {
            const int tw = mask->tw, th = mask->th;
            auto argb = std::make_shared<std::vector<uint32_t>>(static_cast<size_t>(tw) * th);
            for (int ty = 0; ty < th; ty++) {
                for (int tx = 0; tx < tw; tx++) {
                    const uint8_t frac = mask->texel_frac[static_cast<size_t>(ty) * tw + tx];
                    if (!frac) continue;
                    const float unexplored = UnexploredCoverageAtWm(*snap, (tx + 0.5f) * kWmPerTexel, (ty + 0.5f) * kWmPerTexel);
                    if (unexplored <= 0.4f) continue;
                    const float edge = std::min((unexplored - 0.4f) * 5.f, 1.f);
                    const float coast = std::clamp((frac * (1.f / 64.f) - 0.4f) * 5.f, 0.f, 1.f);
                    const uint32_t a = static_cast<uint32_t>(kFogTexMaxAlpha * edge * coast + 0.5f);
                    if (!a) continue;
                    (*argb)[static_cast<size_t>(ty) * tw + tx] = (a << 24) | kCappableFogRgb;
                }
            }
            Resources::EnqueueDxTask([argb, tw, th, hash, layer = mask->layer](IDirect3DDevice9* device) {
                if (fog_tex.shutdown) return;
                if (fog_tex.retired) {
                    fog_tex.retired->Release();
                    fog_tex.retired = nullptr;
                }
                if (fog_tex.tex && (fog_tex.tw != tw || fog_tex.th != th)) {
                    fog_tex.retired = fog_tex.tex;
                    fog_tex.tex = nullptr;
                }
                if (!fog_tex.tex) {
                    if (!device || device->CreateTexture(tw, th, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &fog_tex.tex, nullptr) != D3D_OK || !fog_tex.tex) {
                        fog_tex.tex = nullptr;
                        fog_tex.building = false;
                        Log::Log("[cartographer] fog texture create failed (%dx%d)", tw, th);
                        return;
                    }
                    fog_tex.tw = tw;
                    fog_tex.th = th;
                }
                D3DLOCKED_RECT lr;
                if (fog_tex.tex->LockRect(0, &lr, nullptr, 0) == D3D_OK) {
                    for (int y = 0; y < th; y++) {
                        memcpy(static_cast<uint8_t*>(lr.pBits) + static_cast<size_t>(y) * lr.Pitch, argb->data() + static_cast<size_t>(y) * tw, static_cast<size_t>(tw) * 4);
                    }
                    fog_tex.tex->UnlockRect(0);
                }
                fog_tex.layer = layer;
                fog_tex.built_hash = hash;
                fog_tex.building = false;
            });
        });
    }

    void ReleaseFogTexture()
    {
        fog_tex.shutdown = true;
        if (fog_tex.tex) {
            fog_tex.tex->Release();
            fog_tex.tex = nullptr;
        }
        if (fog_tex.retired) {
            fog_tex.retired->Release();
            fog_tex.retired = nullptr;
        }
    }

    void LoadCappableMask(const int layer)
    {
        if (cappable_mask && cappable_mask->layer == layer) return;
        if (cappable_mask_pending == layer) return;
        cappable_mask_pending = layer;
        Resources::EnqueueWorkerTask([layer] {
            const auto mask = std::make_shared<CappableMask>();
            mask->layer = layer;
            const auto path = Resources::GetPath(L"cartography", std::format(L"cappable_L{}.bin", layer));
            std::ifstream in(path, std::ios::binary);
            bool ok = false;
            char magic[4];
            uint32_t hdr[3];
            if (in && in.read(magic, 4) && memcmp(magic, "CCM1", 4) == 0 && in.read(reinterpret_cast<char*>(hdr), sizeof(hdr)) && hdr[0] == static_cast<uint32_t>(layer)) {
                mask->w = static_cast<int>(hdr[1]);
                mask->h = static_cast<int>(hdr[2]);
                if (mask->w > 0 && mask->h > 0 && mask->w <= 32768 && mask->h <= 32768) {
                    const size_t bytes = (static_cast<size_t>(mask->w) * mask->h + 7) / 8;
                    mask->bits.resize(bytes);
                    ok = static_cast<bool>(in.read(reinterpret_cast<char*>(mask->bits.data()), bytes));
                }
            }
            if (ok) {
                const auto px = [&mask](const int x, const int y) -> int {
                    return MaskBit(mask->bits, static_cast<size_t>(y) * mask->w + x);
                };
                mask->tw = mask->w / 8;
                mask->th = mask->h / 8;
                mask->texel_frac.assign(static_cast<size_t>(mask->tw) * mask->th, 0);
                for (int ty = 0; ty < mask->th; ty++) {
                    for (int tx = 0; tx < mask->tw; tx++) {
                        int set = 0;
                        for (int y = 0; y < 8; y++) {
                            for (int x = 0; x < 8; x++) {
                                set += px(tx * 8 + x, ty * 8 + y);
                            }
                        }
                        mask->texel_frac[static_cast<size_t>(ty) * mask->tw + tx] = static_cast<uint8_t>(set);
                    }
                }
                mask->cw = mask->w / 32;
                mask->ch = mask->h / 32;
                mask->cell_bits.assign((static_cast<size_t>(mask->cw) * mask->ch + 7) / 8, 0);
                for (int cy = 0; cy < mask->ch; cy++) {
                    for (int cx = 0; cx < mask->cw; cx++) {
                        bool any = false;
                        for (int y = 0; y < 32 && !any; y++) {
                            for (int x = 0; x < 32 && !any; x++) {
                                any = px(cx * 32 + x, cy * 32 + y) != 0;
                            }
                        }
                        if (any) {
                            const size_t bit = static_cast<size_t>(cy) * mask->cw + cx;
                            mask->cell_bits[bit >> 3] |= 1 << (bit & 7);
                        }
                    }
                }
            }
            else {
                mask->bits.clear();
                mask->w = mask->h = 0;
            }
            GW::GameThread::Enqueue([mask] {
                cappable_mask = mask;
                cappable_mask_pending = -1;
                fog_tex.built_hash = 0;
                Log::Log("[cartographer] cappable mask L%d: %s (%dx%d wm)", mask->layer, mask->bits.empty() ? "missing" : "loaded", mask->w, mask->h);
            });
        });
    }

    // Coastlines and cliffs ignore the cell grid, so the whole cell is sampled rather than its
    // centre: one that is mostly cliff but clips a walkable ledge is still somewhere you can go.
    bool ProbeStandCell(const int cx, const int cy, GW::GamePos& out)
    {
        // Fine enough to catch the shoreline slivers that are often the only footing near fog.
        constexpr int kSamples = 6;
        bool found = false;
        float best_d2 = FLT_MAX;
        const GW::Vec2f centre = CellCenterWorldMap(cx, cy);
        for (int sy = 0; sy < kSamples; sy++) {
            for (int sx = 0; sx < kSamples; sx++) {
                const GW::Vec2f wm{
                    (cx + (sx + 0.5f) / kSamples) * kWorldMapUnitsPerCell,
                    (cy + (sy + 0.5f) / kSamples) * kWorldMapUnitsPerCell,
                };
                GW::GamePos gp{};
                if (!WorldMapWidget::WorldMapToGamePos(wm, gp) || !Pathing::IsPositionWalkable(gp)) continue;
                // Deepest footing wins - standing near a border risks the server crediting the
                // neighbouring cell instead.
                const float d2 = Dist2(wm, centre);
                if (!found || d2 < best_d2) {
                    best_d2 = d2;
                    out = gp;
                    found = true;
                }
            }
        }
        return found;
    }

    // Keeps the sweep to the fog's fringe instead of probing every cell on the map.
    bool CellWorthProbing(const CartoGrid& grid, const int cx, const int cy)
    {
        const int r = RevealRadius();
        for (int dy = -r; dy <= r; dy++) {
            for (int dx = -r; dx <= r; dx++) {
                if (!grid.IsExplored(cx + dx, cy + dy) && CellCappable(cx + dx, cy + dy)) return true;
            }
        }
        return false;
    }

    // Whether a cell is standable never changes within a map, so probe it once and keep it.
    void SweepStandCells(const CartoGrid& grid, GW::AreaInfo* map_info)
    {
        ImRect bounds;
        if (!(map_info && GW::Map::GetMapWorldMapBounds(map_info, &bounds))) return;
        // Bounded to this map because that is where you can stand; the fog credited may still
        // belong to the map next door, which CellWorthProbing allows for.
        const int x0 = static_cast<int>(floorf(bounds.Min.x / kWorldMapUnitsPerCell));
        const int y0 = static_cast<int>(floorf(bounds.Min.y / kWorldMapUnitsPerCell));
        const int x1 = static_cast<int>(ceilf(bounds.Max.x / kWorldMapUnitsPerCell));
        const int y1 = static_cast<int>(ceilf(bounds.Max.y / kWorldMapUnitsPerCell));
        int budget = 6;
        for (int cy = y0; cy < y1 && budget > 0; cy++) {
            for (int cx = x0; cx < x1 && budget > 0; cx++) {
                if (stand_cells.contains({cx, cy})) continue;
                if (!CellWorthProbing(grid, cx, cy)) continue;
                StandCell sc;
                sc.walkable = ProbeStandCell(cx, cy, sc.pos);
                stand_cells[{cx, cy}] = sc;
                budget--;
            }
        }
        sweep_complete = budget > 0;
    }

    void RecomputeCoverage(const CartoGrid& grid, GW::AreaInfo* map_info)
    {
        const int r = RevealRadius();
        for (auto& [cell, sc] : stand_cells) {
            sc.reveals = 0;
            if (!sc.walkable) continue;
            for (int dy = -r; dy <= r; dy++) {
                for (int dx = -r; dx <= r; dx++) {
                    const int fx = cell.first + dx;
                    const int fy = cell.second + dy;
                    if (grid.IsExplored(fx, fy) || !CellCappable(fx, fy)) continue;
                    sc.reveals++;
                }
            }
        }

        uncoverable_cells.clear();
        map_fog_cells = -1;
        ImRect bounds;
        if (!(map_info && GW::Map::GetMapWorldMapBounds(map_info, &bounds) && bounds.GetWidth() >= 1.f && bounds.GetHeight() >= 1.f)) return;
        const int x0 = static_cast<int>(floorf(bounds.Min.x / kWorldMapUnitsPerCell));
        const int y0 = static_cast<int>(floorf(bounds.Min.y / kWorldMapUnitsPerCell));
        const int x1 = static_cast<int>(ceilf(bounds.Max.x / kWorldMapUnitsPerCell));
        const int y1 = static_cast<int>(ceilf(bounds.Max.y / kWorldMapUnitsPerCell));
        int count = 0;
        for (int cy = y0; cy < y1; cy++) {
            for (int cx = x0; cx < x1; cx++) {
                if (grid.IsExplored(cx, cy) || !CellCappable(cx, cy)) continue;
                if (FogCellCoverable(cx, cy)) count++;
                else uncoverable_cells.insert({cx, cy});
            }
        }
        map_fog_cells = count;
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
    };

    GW::Constants::MapID state_map_id = static_cast<GW::Constants::MapID>(0);
    GW::Constants::InstanceType state_instance_type = GW::Constants::InstanceType::Loading;
    Target target;
    GW::GamePos goal_game{};
    GW::Vec2f player_wm_cached{};
    clock_t last_scan = 0;
    clock_t arrived_at = 0;
    clock_t map_settled_at = 0;
    clock_t marker_recheck_at = 0;
    bool arrived = false;
    bool self_changing_marker = false;
    bool marker_owned = false;
    GW::Vec2f last_marker_wm{};
    bool warned_no_data = false;
    bool warned_no_fog = false;
    bool warned_pathing_disabled = false;

    void SetMarkerAt(const GW::Vec2f& wm)
    {
        self_changing_marker = true;
        QuestModule::SetCustomQuestMarker(wm, true);
        self_changing_marker = false;
        marker_owned = true;
        last_marker_wm = wm;
    }

    void ClearMarker()
    {
        if (!marker_owned) return;
        marker_owned = false;
        self_changing_marker = true;
        QuestModule::ClearCustomQuestMarker();
        self_changing_marker = false;
    }

    void ClearTarget()
    {
        target = {};
        goal_game = {};
        arrived = false;
        arrived_at = 0;
        ClearMarker();
    }

    void ResetState()
    {
        target = {};
        goal_game = {};
        arrived_at = 0;
        arrived = false;
        warned_no_data = false;
        warned_no_fog = false;
        warned_pathing_disabled = false;
        map_fog_cells = -1;
        marker_recheck_at = 0;
        skipped_cells.clear();
        stand_cells.clear();
        uncoverable_cells.clear();
        sweep_complete = false;
        ClearMarker();
    }

    void RemoveCustomPointAt(const GW::Vec2f& wm)
    {
        std::erase_if(custom_points, [&wm](const GW::Vec2f& p) { return Dist2(p, wm) < 1.f; });
        SerializePoints();
    }

    void SkipTargetImpl(const bool forever)
    {
        if (!target.valid) return;
        if (target.custom) {
            Log::Log("[cartographer] custom point (%.0f, %.0f) removed", target.wm.x, target.wm.y);
            RemoveCustomPointAt(target.wm);
        }
        else if (forever) {
            declined_cells.insert({target.cx, target.cy});
            SerializeDeclined();
            Log::Log("[cartographer] stand cell (%d, %d) declined forever", target.cx, target.cy);
        }
        else {
            skipped_cells.insert({target.cx, target.cy});
            Log::Log("[cartographer] stand cell (%d, %d) declined for this map", target.cx, target.cy);
        }
        ClearTarget();
    }

    void AddCustomPointImpl(const GW::Vec2f& wm)
    {
        custom_points.push_back(wm);
        SerializePoints();
        Log::Log("[cartographer] custom fog point added at wm(%.0f, %.0f)", wm.x, wm.y);
    }

    void OnCustomMarkerChanged()
    {
        if (self_changing_marker) return;
        if (!enabled) {
            marker_owned = false;
            return;
        }
        // Map transitions re-emit or drop the marker; only an in-map change by someone else is a takeover.
        if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading || !GW::Map::GetIsMapLoaded() || !GW::Agents::GetControlledCharacter()) return;
        if (marker_owned) {
            const auto quest = QuestModule::GetCustomQuestMarker();
            GW::Vec2f pos{};
            if (quest && QuestModule::GetCustomQuestMarkerWorldPos(quest->quest_id, pos) && Dist2(pos, last_marker_wm) < 1.f) {
                marker_recheck_at = 0;
                return;
            }
            // The marker gets re-emitted as clear+set around us (map loads, world map interactions);
            // judge ownership once the dust settles instead of yielding on the transient.
            if (!marker_recheck_at) marker_recheck_at = TIMER_INIT();
        }
        // Markers we never owned (e.g. a stale persisted marker re-emitted on map load) are
        // not a takeover — the next scan overwrites them with our target.
    }

    void BuildStatusText(char* buf, const size_t len)
    {
        if (!PathfindingWindow::IsPathingEnabled()) {
            snprintf(buf, len, "idle, the pathfinding module is disabled");
            return;
        }
        if (!target.valid || marker_recheck_at) {
            const char* idle = map_fog_cells == 0 ? "nothing left to uncover on this map"
                : !sweep_complete ? "scanning this map's fog..."
                : "nothing reachable left here - travel on, or add a fog point";
            snprintf(buf, len, "%s", idle);
            return;
        }
        if (arrived && !target.custom) {
            snprintf(buf, len, "standing in the right square - hold here a moment, or skip this suggestion");
            return;
        }
        const float dist_k = sqrtf(Dist2(player_wm_cached, target.wm)) * kGwinchesPerWorldMapUnit / 1000.f;
        if (target.custom) {
            snprintf(buf, len, "heading to your fog point, %.1fk units %s of you%s", dist_k, CompassDir(player_wm_cached, target.wm),
                     target.on_map ? "" : " (another map - follow the route)");
            return;
        }
        // The square being walked to is the one to stand in, not the one being uncovered - saying
        // so is the whole point of the change, so the status line spells it out.
        snprintf(buf, len, "stand in the square %.1fk units %s of you to uncover %d %s%s", dist_k,
                 CompassDir(player_wm_cached, target.wm), target.reveals, target.reveals == 1 ? "square" : "squares",
                 target.on_map ? "" : " (another map - follow the route)");
    }

    int FindCustomPointNear(const GW::Vec2f& wm, const float max_dist)
    {
        int best = -1;
        float best_d2 = max_dist * max_dist;
        for (size_t i = 0; i < custom_points.size(); i++) {
            const float d2 = Dist2(custom_points[i], wm);
            if (d2 <= best_d2) {
                best_d2 = d2;
                best = static_cast<int>(i);
            }
        }
        return best;
    }

    bool ContextMenuItems(const GW::Vec2f& click_wm, const float px_per_wm_unit)
    {
        if (!enabled) return true;
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
                && static_cast<int>(floorf(click_wm.x / kWorldMapUnitsPerCell)) == target.cx
                && static_cast<int>(floorf(click_wm.y / kWorldMapUnitsPerCell)) == target.cy;
            if (target.valid && (on_suggestion || (target.custom && point_here >= 0))) {
                if (ImGui::Button(target.custom ? "Remove this fog point" : "Skip this suggestion", item_size)) {
                    CartographerModule::SkipCurrentTarget(false);
                    keep_open = false;
                }
                if (!target.custom && ImGui::Button("Never suggest this spot again", item_size)) {
                    CartographerModule::SkipCurrentTarget(true);
                    keep_open = false;
                }
            }
            else if (point_here >= 0) {
                if (ImGui::Button("Remove fog point", item_size)) {
                    CartographerModule::RemoveCustomPointNear(click_wm, near_dist);
                    keep_open = false;
                }
            }
            else {
                if (ImGui::Button("Add fog point here", item_size)) {
                    CartographerModule::AddCustomPoint(click_wm);
                    keep_open = false;
                }
            }
            if (custom_points.size() > 1) {
                char label[48];
                snprintf(label, sizeof(label), "Clear all %u fog points", static_cast<unsigned>(custom_points.size()));
                if (ImGui::Button(label, item_size)) {
                    CartographerModule::ClearCustomPoints();
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
                    const int cx = static_cast<int>(floorf(at.x / kWorldMapUnitsPerCell));
                    const int cy = static_cast<int>(floorf(at.y / kWorldMapUnitsPerCell));
                    GW::GamePos gp{};
                    const bool converted = WorldMapWidget::WorldMapToGamePos(at, gp);
                    const auto stand = stand_cells.find({cx, cy});
                    Log::Log("[cartographer] probe wm(%.0f,%.0f) cell(%d,%d): explored=%d, cappable px=%d texel=%d cell=%d (mask L%d %s), grid %ux%u (%u words/row), game(%.0f,%.0f), standable here=%d, probed=%d walkable=%d reveals=%d, coverable=%d, radius=%d",
                             at.x, at.y, cx, cy, static_cast<int>(g.IsExplored(cx, cy)),
                             static_cast<int>(MaskCappableAtWm(at)),
                             TexelFrac(static_cast<int>(floorf(at.x / kWmPerTexel)), static_cast<int>(floorf(at.y / kWmPerTexel))),
                             static_cast<int>(CellCappable(cx, cy)),
                             cappable_mask ? cappable_mask->layer : -1, MaskUsable() ? "loaded" : "missing",
                             g.width, g.height, RowWords(g.width),
                             gp.x, gp.y, converted && Pathing::IsPositionWalkable(gp),
                             static_cast<int>(stand != stand_cells.end()),
                             stand != stand_cells.end() ? static_cast<int>(stand->second.walkable) : 0,
                             stand != stand_cells.end() ? stand->second.reveals : 0,
                             static_cast<int>(FogCellCoverable(cx, cy)), RevealRadius());
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
        return ContextMenuItems(MissionMapWidget::GetContextMenuWorldMapPos(), MissionMapWidget::GetPxPerWorldMapUnit());
    }

    bool OnWorldMapContextMenu()
    {
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

    void DrawCappableFog(ImDrawList* dl, const ProjectToScreen project)
    {
        if (!fog_tex.tex) return;
        const auto map_info = GW::Map::GetMapInfo(GW::Map::GetMapID());
        if (!map_info || fog_tex.layer != static_cast<int>(map_info->continent)) return;
        ImVec2 p0, p1;
        if (!project({0.f, 0.f}, p0)) return;
        if (!project({fog_tex.tw * kWmPerTexel, fog_tex.th * kWmPerTexel}, p1)) return;
        dl->AddImage(reinterpret_cast<ImTextureID>(fog_tex.tex), p0, p1);
    }

    bool ProjectCell(const ProjectToScreen project, const int cx, const int cy, ImVec2& min_out, ImVec2& max_out)
    {
        return project({cx * kWorldMapUnitsPerCell, cy * kWorldMapUnitsPerCell}, min_out) &&
            project({(cx + 1) * kWorldMapUnitsPerCell, (cy + 1) * kWorldMapUnitsPerCell}, max_out);
    }

    // Drawn at true 32x32 size, shaded by how much fog the spot would credit.
    void DrawStandCells(ImDrawList* dl, const ProjectToScreen project, const ImVec2& mouse, const char*& tooltip)
    {
        const ImRect clip(dl->GetClipRectMin(), dl->GetClipRectMax());
        for (const auto& [cell, sc] : stand_cells) {
            if (!sc.walkable || sc.reveals <= 0) continue;
            if (declined_cells.contains(cell)) continue;
            // The suggestion is drawn on top with its own pulse — but only while it is actually
            // being shown, so a pending ownership recheck does not blank the square entirely.
            if (target.valid && !marker_recheck_at && !target.custom && target.cx == cell.first && target.cy == cell.second) continue;
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
        if (show_cappable_fog) {
            DrawCappableFog(dl, project);
        }
        if (show_stand_cells) {
            const char* stand_tooltip = nullptr;
            DrawStandCells(dl, project, mouse, stand_tooltip);
            if (cell_tooltip) tooltip = stand_tooltip;
        }
        // While marker ownership is in question (user removed/moved it; yield pending) the
        // target visuals hide immediately so the removal feels instant.
        const bool target_active = target.valid && !marker_recheck_at;
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
        for (const auto& p : custom_points) {
            ImVec2 c;
            if (!project(p, c)) continue;
            const bool is_current = target_active && target.custom && Dist2(target.wm, p) < 1.f;
            DrawFogPointMarker(dl, c, is_current);
            const float mdx = mouse.x - c.x;
            const float mdy = mouse.y - c.y;
            if (mdx * mdx + mdy * mdy < 12.f * 12.f) {
                tooltip = "Cartographer fog point\nRight-click nearby to remove it";
            }
        }
        if (tooltip) ImGui::SetTooltip("%s", tooltip);
    }

    void OnWorldMapOverlayDraw(ImDrawList* dl)
    {
        if (!enabled) return;
        DrawMapOverlay(dl, [](const GW::Vec2f& wm, ImVec2& out) { return WorldMapWidget::WorldMapToScreen(wm, out); }, true);
        char status[160];
        BuildStatusText(status, sizeof(status));
        char line[192];
        snprintf(line, sizeof(line), ICON_FA_MAP_MARKED_ALT " Cartographer: %s", status);
        dl->AddText({16.f, dl->GetClipRectMax().y - 68.f}, ImGui::GetColorU32(ImGuiCol_Text), line);
    }

    void OnMissionMapOverlayDraw(ImDrawList* dl)
    {
        if (!enabled) return;
        DrawMapOverlay(dl, [](const GW::Vec2f& wm, ImVec2& out) { return MissionMapWidget::WorldMapToScreen(wm, out); }, false);
    }
} // namespace

void CartographerModule::Initialize()
{
    ToolboxModule::Initialize();
    SettingsRegistry::RegisterField(this, "enabled", &enabled);
    SettingsRegistry::RegisterField(this, "show_cappable_fog", &show_cappable_fog);
    SettingsRegistry::RegisterField(this, "show_stand_cells", &show_stand_cells);
    SettingsRegistry::RegisterField(this, "reveal_radius_cells", &reveal_radius_cells);
    SettingsRegistry::RegisterField(this, "declined_cells", &declined_cells_str);
    SettingsRegistry::RegisterField(this, "custom_points", &custom_points_str);
    MissionMapWidget::AddContextMenuCallback(&OnMissionMapContextMenu);
    WorldMapWidget::AddContextMenuCallback(&OnWorldMapContextMenu);
    MissionMapWidget::AddOverlayCallback(&OnMissionMapOverlayDraw);
    WorldMapWidget::AddOverlayCallback(&OnWorldMapOverlayDraw);
    QuestModule::AddCustomMarkerChangedCallback(&OnCustomMarkerChanged);
}

void CartographerModule::SignalTerminate()
{
    MissionMapWidget::RemoveContextMenuCallback(&OnMissionMapContextMenu);
    WorldMapWidget::RemoveContextMenuCallback(&OnWorldMapContextMenu);
    MissionMapWidget::RemoveOverlayCallback(&OnMissionMapOverlayDraw);
    WorldMapWidget::RemoveOverlayCallback(&OnWorldMapOverlayDraw);
    QuestModule::RemoveCustomMarkerChangedCallback(&OnCustomMarkerChanged);
    enabled = false;
    ResetState();
    ReleaseFogTexture();
}

void CartographerModule::LoadSettings(SettingsDoc& doc, ToolboxIni* legacy)
{
    ToolboxModule::LoadSettings(doc, legacy);
    reveal_radius_cells = std::clamp(reveal_radius_cells, 1, 3);
    ParseDeclined();
    ParsePoints();
}

void CartographerModule::Update(float)
{
    if (!enabled) {
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
    }
    // Coordinate anchors can be transitional right after a map change; let them settle.
    if (TIMER_DIFF(map_settled_at) < 2000) return;

    if (marker_recheck_at) {
        // Ownership in question: freeze scanning/re-placing until resolved, so a user removing
        // or moving the marker never has it snapped back by a racing target change.
        if (TIMER_DIFF(marker_recheck_at) <= 1500) return;
        marker_recheck_at = 0;
        const auto quest = QuestModule::GetCustomQuestMarker();
        GW::Vec2f pos{};
        const bool still_ours = marker_owned && quest && QuestModule::GetCustomQuestMarkerWorldPos(quest->quest_id, pos) && Dist2(pos, last_marker_wm) < 1.f;
        if (!still_ours) {
            Log::Log("[cartographer] quest marker changed externally (quest=%d wm=%.0f,%.0f vs ours %.0f,%.0f); cartographer disabled",
                     quest ? 1 : 0, pos.x, pos.y, last_marker_wm.x, last_marker_wm.y);
            marker_owned = false;
            enabled = false;
            ResetState();
            return;
        }
    }

    const auto map_info = GW::Map::GetMapInfo(map_id);
    if (!map_info || !map_info->GetIsOnWorldMap()) return;

    const auto player = GW::Agents::GetControlledCharacter();
    if (!player) return;

    if (TIMER_DIFF(last_scan) < 1000) return;
    last_scan = TIMER_INIT();

    if (!PathfindingWindow::IsPathingEnabled()) {
        if (!warned_pathing_disabled) {
            warned_pathing_disabled = true;
            Log::Log("[cartographer] pathfinding module is disabled; cartographer idle");
        }
        return;
    }
    warned_pathing_disabled = false;
    if (!PathfindingWindow::ReadyForPathing()) return;

    CartoGrid grid;
    if (!GetCartoGrid(grid)) {
        if (!warned_no_data) {
            warned_no_data = true;
            Log::Log("[cartographer] no cartography data available");
        }
        return;
    }

    GW::Vec2f player_wm;
    if (!WorldMapWidget::GamePosToWorldMap(player->pos, player_wm)) return;
    player_wm_cached = player_wm;
    const int continent = static_cast<int>(map_info->continent);
    LoadCappableMask(continent);
    if (!(cappable_mask && cappable_mask->layer == continent)) return;
    SweepStandCells(grid, map_info);
    RecomputeCoverage(grid, map_info);
    if (MaskUsable()) {
        BuildFogTexture(grid);
    }

    // Arrival is being inside the square, not near the goal position - on a thin ledge those can
    // be a whole square apart.
    const int player_cx = static_cast<int>(floorf(player_wm.x / kWorldMapUnitsPerCell));
    const int player_cy = static_cast<int>(floorf(player_wm.y / kWorldMapUnitsPerCell));
    if (target.valid && target.on_map) {
        if (target.custom) {
            if (GW::GetSquareDistance(player->pos, goal_game) < 150.f * 150.f) {
                Log::Log("[cartographer] reached custom point wm(%.0f, %.0f)", target.wm.x, target.wm.y);
                RemoveCustomPointAt(target.wm);
                ClearTarget();
            }
        }
        else if (!arrived && player_cx == target.cx && player_cy == target.cy) {
            arrived = true;
            arrived_at = TIMER_INIT();
            Log::Log("[cartographer] standing in cell (%d, %d), which should credit %d cells", target.cx, target.cy, target.reveals);
        }
    }

    // Nothing credited after standing there: the theory does not hold here, so stop suggesting it.
    if (arrived && target.valid && !target.custom && TIMER_DIFF(arrived_at) > 8000) {
        const auto it = stand_cells.find({target.cx, target.cy});
        if (it != stand_cells.end() && it->second.reveals > 0) {
            Log::Log("[cartographer] cell (%d, %d) credited nothing after 8s; skipping it for this map", target.cx, target.cy);
            skipped_cells.insert({target.cx, target.cy});
            ClearTarget();
        }
    }

    Target cand{};
    float cand_d2 = FLT_MAX;
    for (const auto& p : custom_points) {
        const float d2 = Dist2(p, player_wm);
        if (d2 < cand_d2) {
            cand = {true, true, 0, 0, 0, p};
            cand_d2 = d2;
        }
    }
    bool on_current_map = true;
    if (cand.valid) {
        ImRect bounds;
        on_current_map = GW::Map::GetMapWorldMapBounds(map_info, &bounds) && bounds.Contains({cand.wm.x, cand.wm.y});
    }
    else {
        // Ranked by cells-credited-per-square-walked, not distance: a spot crediting several at
        // once is worth a few extra steps.
        float best_value = 0.f;
        for (const auto& [cell, sc] : stand_cells) {
            if (!sc.walkable || sc.reveals <= 0) continue;
            if (skipped_cells.contains(cell) || declined_cells.contains(cell)) continue;
            const auto centre = CellCenterWorldMap(cell.first, cell.second);
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
            Log::Log("[cartographer] no fogged walkable ground left on this map");
        }
        if (target.valid) ClearTarget();
        return;
    }
    warned_no_fog = false;

    bool same = target.valid && target.custom == cand.custom
        && (cand.custom ? Dist2(target.wm, cand.wm) < 1.f : (target.cx == cand.cx && target.cy == cand.cy));
    if (target.valid && !same && !(cand.custom && !target.custom)) {
        // Hysteresis: keep the current target unless it became ineligible or the candidate is meaningfully closer.
        const auto current = stand_cells.find({target.cx, target.cy});
        const bool current_eligible = target.custom
            ? std::ranges::any_of(custom_points, [&](const GW::Vec2f& p) { return Dist2(p, target.wm) < 1.f; })
            : current != stand_cells.end() && current->second.walkable && current->second.reveals > 0
            && !skipped_cells.contains({target.cx, target.cy}) && !declined_cells.contains({target.cx, target.cy});
        if (current_eligible && cand_d2 >= 0.7f * Dist2(target.wm, player_wm)) same = true;
    }
    if (same) {
        // Same square, but the fog around it may have shrunk and the status line quotes it.
        if (target.valid && !target.custom) {
            const auto it = stand_cells.find({target.cx, target.cy});
            if (it != stand_cells.end()) target.reveals = it->second.reveals;
        }
        return;
    }

    GW::GamePos gp{};
    GW::Vec2f marker_wm = cand.wm;
    if (on_current_map) {
        if (cand.custom) {
            if (!WorldMapWidget::WorldMapToGamePos(cand.wm, gp)) return;
            // The marker goes on the closest walkable spot toward the point, not into the void.
            Pathing::FindClosestPositionOnTrapezoid(gp);
        }
        else {
            // Route to the footing the probe found, not the square's centre, which may be cliff.
            const auto it = stand_cells.find({cand.cx, cand.cy});
            if (it == stand_cells.end() || !it->second.walkable) return;
            gp = it->second.pos;
        }
        if (!WorldMapWidget::GamePosToWorldMap(gp, marker_wm)) return;
    }
    // Off-map targets keep the raw position: QuestModule resolves the destination map from it
    // and plots the cross-map route on the world map, same as a manually placed marker.
    cand.on_map = on_current_map;
    target = cand;
    goal_game = gp;
    arrived = false;
    SetMarkerAt(marker_wm);
    if (target.custom) {
        if (on_current_map) Log::Log("[cartographer] target: custom point wm(%.0f, %.0f), marker at game(%.0f, %.0f)", target.wm.x, target.wm.y, gp.x, gp.y);
        else Log::Log("[cartographer] target: custom point wm(%.0f, %.0f) on another map; marker set there for cross-map routing", target.wm.x, target.wm.y);
    }
    else if (on_current_map) {
        Log::Log("[cartographer] stand target: cell (%d, %d) wm(%.0f, %.0f) credits %d cells at radius %d, marker at game(%.0f, %.0f)",
                 target.cx, target.cy, target.wm.x, target.wm.y, target.reveals, RevealRadius(), gp.x, gp.y);
    }
    else {
        Log::Log("[cartographer] stand target: cell (%d, %d) wm(%.0f, %.0f) on another map; marker set there for cross-map routing", target.cx, target.cy, target.wm.x, target.wm.y);
    }
}

void CartographerModule::DrawSettingsInternal()
{
    ImGui::TextDisabled("Debug tool. Exploration is credited by 32x32 world-map square: standing inside a\nsquare credits it and the ring of squares around it. So instead of routing you at the\nfog itself, this works out which squares you could stand in, which of them would\ncredit something still foggy, draws those on the world map and mission map, and puts\nthe custom quest marker in the best one - the regular quest path leads you there.\nRight-click either map to manage the helper.");
    bool on = enabled;
    if (ImGui::Checkbox("Enabled", &on)) {
        GW::GameThread::Enqueue([on] { SetEnabled(on); });
    }
    ImGui::Checkbox("Show cappable fog on the maps", &show_cappable_fog);
    ImGui::ShowHelp("Green: everything still unexplored that can actually be uncovered, straight from the cartography mod's data. Fog no square on this map can credit draws nothing.");
    ImGui::Checkbox("Show squares to stand in", &show_stand_cells);
    ImGui::ShowHelp("Draws every 32x32 square worth walking into, shaded by how many foggy squares standing there would credit. The current suggestion is outlined and pulses.");
    if (MaskUsable()) {
        ImGui::Text("Cappable mask: continent %d, %dx%d world-map units", cappable_mask->layer, cappable_mask->w, cappable_mask->h);
    }
    else {
        ImGui::TextColored(ImVec4(1.f, .6f, .3f, 1.f), "Cappable mask: %s", cappable_mask ? "no data for this continent" : "not loaded yet");
    }
    if (ImGui::SliderInt("Reveal radius (squares)", &reveal_radius_cells, 1, 3)) {
        GW::GameThread::Enqueue([] {
            // The radius decides which cells are worth probing at all, so the sweep restarts.
            stand_cells.clear();
            uncoverable_cells.clear();
            sweep_complete = false;
            fog_tex.built_hash = 0;
        });
    }
    ImGui::ShowHelp("How far exploration credit spreads from the square you stand in, measured in squares (Chebyshev distance, so the credited area is a square block). 1 is a bare character; a Bonus Explorer's Cape is believed to add two more rings, so set 3 when wearing one. Changing this rescans the map.");
    unsigned standable = 0;
    unsigned useful = 0;
    for (const auto& [cell, sc] : stand_cells) {
        if (!sc.walkable) continue;
        standable++;
        if (sc.reveals > 0) useful++;
    }
    ImGui::Text("Squares: %u probed, %u standable, %u worth visiting", static_cast<unsigned>(stand_cells.size()), standable, useful);
    ImGui::Text("Foggy squares: %d coverable here, %u out of reach", map_fog_cells, static_cast<unsigned>(uncoverable_cells.size()));
    ImGui::Text("Declined forever: %u squares", static_cast<unsigned>(declined_cells.size()));
    ImGui::SameLine();
    if (ImGui::SmallButton("Clear##declined")) ClearDeclined();
    ImGui::Text("Custom fog points: %u", static_cast<unsigned>(custom_points.size()));
    ImGui::SameLine();
    if (ImGui::SmallButton("Clear##points")) ClearCustomPoints();
}

void CartographerModule::SetEnabled(const bool on)
{
    if (enabled == on) return;
    enabled = on;
    if (!on) ResetState();
    Log::Log("[cartographer] %s", on ? "enabled" : "disabled");
}

bool CartographerModule::GetEnabled()
{
    return enabled;
}

void CartographerModule::OnUserMarkerAction()
{
    if (!enabled) return;
    enabled = false;
    marker_owned = false;
    GW::GameThread::Enqueue([] {
        ResetState();
        Log::Log("[cartographer] user placed/removed the quest marker; cartographer disabled");
    });
}

bool CartographerModule::GetCurrentTargetWorldPos(GW::Vec2f& out)
{
    if (!target.valid) return false;
    out = target.wm;
    return true;
}





void CartographerModule::SkipCurrentTarget(const bool forever)
{
    GW::GameThread::Enqueue([forever] {
        SkipTargetImpl(forever);
    });
}

void CartographerModule::AddCustomPoint(const GW::Vec2f& world_map_pos)
{
    GW::GameThread::Enqueue([world_map_pos] {
        AddCustomPointImpl(world_map_pos);
    });
}

void CartographerModule::RemoveCustomPointNear(const GW::Vec2f& world_map_pos, const float max_dist_wm)
{
    GW::GameThread::Enqueue([world_map_pos, max_dist_wm] {
        const int idx = FindCustomPointNear(world_map_pos, max_dist_wm);
        if (idx < 0) return;
        const GW::Vec2f p = custom_points[idx];
        const bool was_target = target.valid && target.custom && Dist2(target.wm, p) < 1.f;
        custom_points.erase(custom_points.begin() + idx);
        SerializePoints();
        if (was_target) ClearTarget();
        Log::Log("[cartographer] fog point (%.0f, %.0f) removed", p.x, p.y);
    });
}

void CartographerModule::ClearCustomPoints()
{
    GW::GameThread::Enqueue([] {
        custom_points.clear();
        SerializePoints();
        if (target.valid && target.custom) ClearTarget();
        Log::Log("[cartographer] custom fog points cleared");
    });
}

void CartographerModule::ClearDeclined()
{
    GW::GameThread::Enqueue([] {
        declined_cells.clear();
        skipped_cells.clear();
        stand_cells.clear();
        uncoverable_cells.clear();
        sweep_complete = false;
        SerializeDeclined();
        if (target.valid) ClearTarget();
        Log::Log("[cartographer] declined cells cleared");
    });
}

void CartographerModule::GetStatus(char* buf, const size_t len)
{
    const char* pathing = !PathfindingWindow::IsPathingEnabled() ? "off" : PathfindingWindow::ReadyForPathing() ? "ready" : "prewarming";
    char target_desc[64];
    if (!target.valid) snprintf(target_desc, sizeof(target_desc), "none");
    else if (target.custom) snprintf(target_desc, sizeof(target_desc), "point(%.0f,%.0f)", target.wm.x, target.wm.y);
    else snprintf(target_desc, sizeof(target_desc), "stand(%d,%d)+%d", target.cx, target.cy, target.reveals);
    snprintf(buf, len, "carto: enabled=%d pathing=%s target=%s marker=%d arrived=%d radius=%d skipped=%u probed=%u declined=%u points=%u fogcells=%d mask=L%d/%s fogtex=%s",
             enabled, pathing, target_desc, marker_owned, arrived, RevealRadius(),
             static_cast<unsigned>(skipped_cells.size()), static_cast<unsigned>(stand_cells.size()),
             static_cast<unsigned>(declined_cells.size()), static_cast<unsigned>(custom_points.size()), map_fog_cells,
             cappable_mask ? cappable_mask->layer : -1, MaskUsable() ? "ok" : "none",
             fog_tex.tex ? "ok" : fog_tex.building ? "building" : "none");
}

