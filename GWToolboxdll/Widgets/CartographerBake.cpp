#include "stdafx.h"

#ifdef _DEBUG

#include <filesystem>
#include <fstream>

#include <GWCA/Constants/Constants.h>
#include <GWCA/GameEntities/Map.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/MapMgr.h>

#include <ImGuiAddons.h>
#include <Logger.h>
#include <Timer.h>
#include <Modules/Resources.h>
#include <Widgets/CartographerInternal.h>
#include <Widgets/CartographerWidget.h>
#include <Widgets/WorldMapWidget.h>
#include <Windows/Pathfinding/Pathing.h>
#include <Windows/Pathfinding/PathfindingWindow.h>
#include <Windows/Pathfinding/PathingMapDataLoader.h>

// Bakes, per continent, which 32x32 tiles have standable ground and which tiles that ground can
// credit, straight out of the DAT. Writes the same .bin files tools/bake_cartography/make_header.py
// turns into CartographyData.h; it exists in-game because it shares GamePosToWorldMap with the
// runtime, and an offline copy of that formula is what once let the shipped table drift a row north.
// Debug builds only.
namespace Carto {
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

    int TileX(const uint64_t key) { return static_cast<int>(static_cast<uint32_t>(key & 0xffffffff)); }
    int TileY(const uint64_t key) { return static_cast<int>(static_cast<uint32_t>(key >> 32)); }

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
                ForEachTileOfTrapezoid(map_id, trap, [&](const int cx, const int cy) {
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
                });
            }
        }
        // Dilated here rather than at runtime because credit stops one square past THIS map's
        // rectangle, and a flat continent bitmap cannot say which map a tile's ground belongs to.
        const auto dilate = [&](const std::unordered_set<uint64_t>& src, std::unordered_set<uint64_t>& dst) {
            for (const auto key : src) {
                ForEachInRing(TileX(key), TileY(key), kMaskRadius, [&](const int cx, const int cy, int, int) {
                    if (InCreditableBoundsOf(map_id, cx, cy)) dst.insert(TileKey(cx, cy));
                });
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
            x0 = std::min(x0, TileX(key));
            x1 = std::max(x1, TileX(key));
            y0 = std::min(y0, TileY(key));
            y1 = std::max(y1, TileY(key));
        }
        const int w = x1 - x0 + 1;
        const int h = y1 - y0 + 1;
        std::vector<uint8_t> bits((static_cast<size_t>(w) * h + 7) / 8, 0);
        for (const auto key : tiles) {
            const size_t bit = static_cast<size_t>(TileY(key) - y0) * w + (TileX(key) - x0);
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
    bool BakeRunning() { return bake.running; }

    void DrawBakeSettings()
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
}

void CartographerWidget::StartContinentBake()
{
    Carto::StartBake();
}

bool CartographerWidget::ContinentBakeRunning()
{
    return Carto::BakeRunning();
}

#endif
