#include "stdafx.h"

#ifdef _DEBUG

#include <GWCA/Context/MapContext.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Map.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/MapMgr.h>

#include <Logger.h>
#include <Widgets/CartographerInternal.h>
#include <Widgets/CartographerWidget.h>
#include <Widgets/WorldMapWidget.h>
#include <Windows/Pathfinding/Pathing.h>
#include <Windows/Pathfinding/PathfindingWindow.h>
#include <Windows/Pathfinding/PathingMapDataLoader.h>

// The Cartographer's one diagnostic: everything its verdict for a square rests on, dumped in the
// order the widget consults it. Debug builds only, reached from the map context menu or the test
// harness's `cartoprobe` verb.
namespace Carto {
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
        const auto nav = GetNavGridInfo();
        Log::Log("[carto-bake] (%d,%d) nav_grid origin=(%d,%d) size=%dx%d ground_cells=%d stand_cells=%d built=%d",
                 cx, cy, nav.x0, nav.y0, nav.width, nav.height, nav.ground_count, nav.stand_count, static_cast<int>(nav.built));
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
                    const auto strict = Flood(data, {seed}, {}, true);
                    const auto relaxed = Flood(data, {seed}, {}, false);
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
        if (nav.built && !continent_mask.Empty()) {
            const auto live_at = [&](const int x, const int y) { return NavGroundAt(nav.x0 + x, nav.y0 + y); };
            int best_dx = 0, best_dy = 0, best_hits = -1, in_place = 0;
            for (int oy = -4; oy <= 4; oy++) {
                for (int ox = -4; ox <= 4; ox++) {
                    int hits = 0;
                    for (int y = 0; y < nav.height; y++) {
                        for (int x = 0; x < nav.width; x++) {
                            if (live_at(x, y) && continent_mask.AnyGroundAt(nav.x0 + x + ox, nav.y0 + y + oy)) hits++;
                        }
                    }
                    if (!ox && !oy) in_place = hits;
                    if (hits > best_hits) { best_hits = hits; best_dx = ox; best_dy = oy; }
                }
            }
            Log::Log("[carto-bake] (%d,%d) live vs bake: %d/%d live ground cells baked in place; best offset (%+d,%+d) matches %d%s",
                     cx, cy, in_place, nav.ground_count, best_dx, best_dy, best_hits,
                     best_dx || best_dy ? "  <== ANCHOR DRIFT: the bake and GetMapWorldAnchor disagree" : "");
            // # both, L live only (the bake is missing it), b baked only, . neither.
            for (int y = 0; y < nav.height; y++) {
                std::string row;
                for (int x = 0; x < nav.width; x++) {
                    const bool live = live_at(x, y);
                    const bool baked = continent_mask.AnyGroundAt(nav.x0 + x, nav.y0 + y);
                    row += live && baked ? '#' : live ? 'L' : baked ? 'b' : '.';
                }
                Log::Log("[carto-bake]   y=%3d x=%d %s", nav.y0 + y, nav.x0, row.c_str());
            }
        }
        // Which baked tile the dilated claim came from. Ground behind a travel portal shows as
        // glitch-only, which is the difference the setting turns on.
        ForEachInRing(cx, cy, kMaskRadius, [&](const int bx, const int by, const int dx, const int dy) {
            const bool any = continent_mask.AnyGroundAt(bx, by);
            if (!any && (dx || dy)) return;
            Log::Log("[carto-bake]   (%+d,%+d) cell(%d,%d) stand=%d stand_glitched=%d%s",
                     dx, dy, bx, by, static_cast<int>(continent_mask.RawGet(bx, by)), static_cast<int>(any),
                     any && !continent_mask.RawGet(bx, by) ? "  <== only reachable by gate glitching" : "");
        });
        // The other half, for when the bake defers: every cell the client would credit this tile
        // from, and what this map's navmesh probe made of it.
        ForEachInRing(cx, cy, RevealRadius(), [&](const int nx, const int ny, const int dx, const int dy) {
            const bool creditable = CellCreditableFrom(dx, dy, cx, cy);
            const auto it = probe->cells.find({nx, ny});
            const bool probed = it != probe->cells.end();
            if (!creditable && !probed) return;
            Log::Log("[carto-probe]   (%+d,%+d) cell(%d,%d) creditable=%d probed=%d in_nav_grid=%d navmesh=%d reachable=%d reveals=%d%s",
                     dx, dy, nx, ny, static_cast<int>(creditable), static_cast<int>(probed),
                     static_cast<int>(NavInGrid(nx, ny)),
                     probed ? static_cast<int>(it->second.navmesh) : 0,
                     probed ? static_cast<int>(it->second.reachable) : 0, probed ? it->second.reveals : 0,
                     creditable && probed && it->second.reachable ? "  <== would make it coverable" : "");
        });
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
}

void CartographerWidget::LogProbeAtCell(const int cx, const int cy)
{
    Carto::LogProbe(Carto::CreditCellCenterWorldMap(cx, cy));
}

#endif
