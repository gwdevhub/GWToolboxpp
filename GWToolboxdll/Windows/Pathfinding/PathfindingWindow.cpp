#include "stdafx.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <list>
#include <map>
#include <mutex>
#include <queue>
#include <set>
#include <tuple>
#include <unordered_set>

#include <GWCA/Constants/Constants.h>
#include <GWCA/Packets/StoC.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/NPC.h>

#include <GWCA/GameEntities/Map.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/MemoryMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <Timer.h>
#include <filesystem>
#include <fstream>
#include <numeric>

#include <EmbeddedResource.h>
#include <GWToolbox.h>
#include <ImGuiAddons.h>
#include <Utils/EncString.h>
#include <Utils/SettingsRegistry.h>
#include <Utils/TextUtils.h>

#include <GWCA/Context/GameplayContext.h>
#include <GWCA/Context/MapContext.h>
#include <Modules/Resources.h>
#include <Modules/ToolboxSettings.h>
#include <Utils/ArenaNetFileParser.h>
#include <Widgets/Minimap/Minimap.h>
#include <Widgets/Minimap/GameWorldRenderer.h>
#include <Windows/Pathfinding/PathfindingWindow.h>
#include <Windows/Pathfinding/Pathing.h>
#include <Windows/Pathfinding/NavMesh.h> // 仅用于调试覆盖层

#include "PathingLog.h"
#include "PathingMapData.h"
#include "PathingMapDataLoader.h"
#include "PortalConnections.h"
#include "maps_constant_data.h"
#include "resource.h"

#include <GWCA/Context/WorldContext.h>
#include <Utils/ToolboxUtils.h>
#include <Widgets/MissionMapWidget.h>
#include <Widgets/WorldMapWidget.h>

// 取消注释 PATHING_VERBOSE 可重新启用此文件中每帧的 Log::Info 调试输出；
// PATH_LOG_ERROR/WARNING 在调试版本中仍会输出到聊天。
// #define PATHING_VERBOSE 1
#ifdef PATHING_VERBOSE
#define PATH_LOG_INFO(...) Log::Info(__VA_ARGS__)
#else
#define PATH_LOG_INFO(...) ((void)0)
// 当日志被编译排除时，仅用于格式化 PATH_LOG_INFO 的局部变量/参数变为未使用。
#pragma warning(disable : 4189 4100)
#endif

namespace {
    struct CachedMapInfo {
        GW::Constants::MapID map_id = GW::Constants::MapID::None;
        Pathing::Vec2f bounds_min{}, bounds_max{}; // 游戏坐标
        std::vector<Pathing::PortalProp> portal_props;
    };

    // 每地图缓存（低 32 位 = file_hash），LRU 限制 — 见 MAX_CACHED_MAPS。
    std::unordered_map<uint64_t, Pathing::MilePath*> mile_paths_by_coords;
    std::unordered_map<uint64_t, CachedMapInfo> cached_map_info;

    // 本次会话中 LoadMapFromDAT 已解析失败的 file_ids — 抑制重复读取和重复日志垃圾。
    std::unordered_set<uint32_t> dat_load_failed_fids;

    // 已警告过无缓存 file_id 的 map_ids — 抑制重复日志；查找本身仍会重试，因为可能在之后解析成功。
    std::unordered_set<uint32_t> warned_no_fid_maps;

    // 序列化整个路径计算：全局路径缓存假设一次只有一个构建（并发构建会读取半插入的 MilePath → 崩溃）。
    // 游戏线程尝试锁定，因此永远不会阻塞；递归是安全的，因为构建辅助函数会重新进入。
    std::recursive_mutex route_mutex;

    // 每个 file_hash 的传送门属性缓存（轻量级，按需加载）。
    std::unordered_map<uint32_t, std::vector<Pathing::PortalProp>> portal_props_cache;

    // ===== 每地图缓存的 LRU 淘汰 =====
    // 驻留外地图上限（当前地图固定在顶部）。内存/重新计算权衡。
    constexpr size_t MAX_CACHED_MAPS = 12;

    std::list<uint64_t> lru_order; // mile_paths_by_coords 的键，front = 最近使用
    std::unordered_map<uint64_t, std::list<uint64_t>::iterator> lru_pos;

    // 保护 lru_* 和 route_jobs_active；仅当没有路径工作线程持有 MilePath 时才允许淘汰。
    std::mutex lru_mutex;
    int route_jobs_active = 0;

    void TouchLru(uint64_t key)
    {
        std::scoped_lock lock(lru_mutex);
        auto it = lru_pos.find(key);
        if (it != lru_pos.end()) lru_order.erase(it->second);
        lru_order.push_front(key);
        lru_pos[key] = lru_order.begin();
    }

    void EnforceCacheLimitIfIdle(); // GetMapFileId 前向声明需在作用域内

    // RAII 守卫：在持有 MilePath* 时延迟淘汰，然后在主线程中修剪缓存（渲染器在那里读取 cached_map_info）。
    struct RouteJobScope {
        RouteJobScope()
        {
            std::scoped_lock lock(lru_mutex);
            ++route_jobs_active;
        }
        ~RouteJobScope()
        {
            {
                std::scoped_lock lock(lru_mutex);
                --route_jobs_active;
            }
            Resources::EnqueueMainTask([] {
                EnforceCacheLimitIfIdle();
            });
        }
        RouteJobScope(const RouteJobScope&) = delete;
        RouteJobScope& operator=(const RouteJobScope&) = delete;
    };

    std::atomic<int> path_calc_in_flight = 0;
    struct PathCalcScope {
        PathCalcScope() { ++path_calc_in_flight; }
        ~PathCalcScope() { --path_calc_in_flight; }
        PathCalcScope(const PathCalcScope&) = delete;
        PathCalcScope& operator=(const PathCalcScope&) = delete;
    };

    bool draw_map_bounds = false;
    bool draw_graph_edges = false;
    bool draw_portals = false;
    constexpr float portal_unreachable_penalty = 100.f;
    GW::GamePos ToCurrentMapCoords(const GW::GamePos& pos, GW::Constants::MapID src_map); // 前向声明（早期）
    Pathing::MilePath* LoadMapFromDAT(GW::Constants::MapID map_id, bool allow_load = true); // 前向声明（早期）
    void BuildMapGraph();                                                                 // 前向声明（早期）

    void ClearEditorHighlightLines();                                                     // 前向声明（早期）
    void UpdatePortalMarkers();                                                           // 前向声明（早期）
    void UpdateBoundsLines();                                                             // 前向声明（早期）
    void EnsureLightweightMapInfo(GW::Constants::MapID map_id, const char* caller = "?"); // 前向声明（早期）
    const CachedMapInfo* GetCachedMapInfo(GW::Constants::MapID map_id);                   // 前向声明（早期）
    uint32_t GetMapFileId(GW::Constants::MapID map_id);                                   // 前向声明（早期）
    bool GamePosToWorldMapForMap(const GW::Vec2f& game_pos, GW::Constants::MapID map_id,
        const Pathing::Vec2f& game_bounds_min, const Pathing::Vec2f& game_bounds_max,
        GW::Vec2f& world_map_pos); // 前向声明（早期）
    bool WorldMapToGamePosForMap(const GW::Vec2f& world_map_pos, GW::Constants::MapID map_id,
        const Pathing::Vec2f& game_bounds_min, const Pathing::Vec2f& game_bounds_max,
        GW::GamePos& game_pos); // 前向声明（早期）

    bool IsOutpostMap(GW::Constants::MapID map_id); // 前向声明

    // 诊断：仅当触及这些 MapID 时才记录日志；编辑此集合以追踪其他意外加载情况。
    inline bool IsInterestingMapForCacheTrace(GW::Constants::MapID mid)
    {
        return mid == (GW::Constants::MapID)114 || mid == (GW::Constants::MapID)153;
    }

    // 缓存共享此 file_hash 的相邻 MapID。跳过前哨站：将它们传播到每个相邻前哨站图标处会绘制杂散的边界矩形
    //（例如 fh 0xce65 → 38, 119）；它们的查找仍通过 GetCachedMapInfo 的回退解析。
    void CacheSharedFileHashMaps(const CachedMapInfo& source_info)
    {
        uint32_t fh = GetMapFileId(source_info.map_id);
        if (!fh) return;
        for (const auto& [file_hash, entries] : constant_maps_info) {
            if ((uint32_t)file_hash != fh) continue;
            for (const auto& entry : entries) {
                auto mid = entry.map_id;
                if (mid == source_info.map_id) continue;
                if (IsOutpostMap(mid)) continue; // 见上方注释
                bool exists = false;
                for (const auto& [h, inf] : cached_map_info) {
                    if (inf.map_id == mid) {
                        exists = true;
                        break;
                    }
                }
                if (exists) continue;
                uint64_t new_hash = static_cast<uint64_t>(fh) | (static_cast<uint64_t>((uint32_t)mid) << 32);
                CachedMapInfo shared;
                shared.map_id = mid;
                shared.bounds_min = source_info.bounds_min;
                shared.bounds_max = source_info.bounds_max;
                shared.portal_props = source_info.portal_props;
                cached_map_info[new_hash] = shared;
                if (IsInterestingMapForCacheTrace(mid)) {
                    PATH_LOG_INFO("[CacheTrace] CacheSharedFileHashMaps 传播 mid=%d source=%d fh=0x%X", (int)mid, (int)source_info.map_id, fh);
                }
            }
        }
    }

    // 释放一个地图的所有缓存分配并将其从 LRU 中移除。
    // 调用者必须持有 lru_mutex。
    void EvictMapByKey(uint64_t mile_key)
    {
        const uint32_t fh = (uint32_t)(mile_key & 0xFFFFFFFF);

        if (auto it = mile_paths_by_coords.find(mile_key); it != mile_paths_by_coords.end()) {
            delete it->second;
            mile_paths_by_coords.erase(it);
        }
        // 多个 cached_map_info 条目可以共享一个 file_hash（相邻地图传播）。
        std::erase_if(cached_map_info, [fh](const auto& kv) {
            return (uint32_t)(kv.first & 0xFFFFFFFF) == fh;
        });
        portal_props_cache.erase(fh);

        if (auto it = lru_pos.find(mile_key); it != lru_pos.end()) {
            lru_order.erase(it->second);
            lru_pos.erase(it);
        }
    }

    // 修剪到 MAX_CACHED_MAPS（LRU 优先，当前地图固定），仅在空闲时执行，因此没有工作线程持有被释放的 MilePath*。
    void EnforceCacheLimitIfIdle()
    {
        std::vector<uint64_t> evicted;
        {
            std::scoped_lock lock(lru_mutex);
            if (route_jobs_active > 0) return;
            // 当前地图键 low32 = MapID，外地图键 low32 = file_hash — 针对两种形式固定。
            const uint32_t cur_fh = GetMapFileId(GW::Map::GetMapID());
            const uint32_t cur_mid = (uint32_t)GW::Map::GetMapID();
            auto is_current = [&](uint64_t key) {
                const uint32_t low = (uint32_t)(key & 0xFFFFFFFF);
                return (cur_fh && low == cur_fh) || low == cur_mid;
            };
            size_t over = lru_order.size() > MAX_CACHED_MAPS ? lru_order.size() - MAX_CACHED_MAPS : 0;
            for (auto it = lru_order.rbegin(); over > 0 && it != lru_order.rend(); ++it) {
                if (is_current(*it)) continue;
                evicted.push_back(*it);
                --over;
            }
            for (uint64_t key : evicted)
                EvictMapByKey(key);
        }
        if (!evicted.empty()) {
            // 被淘汰的地图的边界/传送门线需要刷新（主线程线池）。
            Resources::EnqueueMainTask([] {
                UpdateBoundsLines();
                if (draw_portals) UpdatePortalMarkers();
            });
        }
    }

    // LoadAllMapsAtPosition 稍后在此命名空间中定义

    // 传送门连接编辑器
    Pathing::PortalConnections portal_connections;

    struct EditorEndpoint {
        GW::Constants::MapID map_id = GW::Constants::MapID::None;
        GW::Vec2f pos{};
        int zplane = 0;
        bool set = false;
    };

    // NPC 编辑器字段（每个端点）。根据 agent_kind 存储 AgentLiving（player_number）或 AgentGadget（gadget_id）。
    struct EditorNpcFields {
        uint32_t agent_id = 0;
        uint32_t model_id = 0;
        char name[256] = {};
        uint32_t last_target_id = 0;
        Pathing::AgentKind agent_kind = Pathing::AgentKind::Living;
    };

    // 每个小部件的两步选择状态（地图 → 传送门）
    struct MapSearchState {
        int pending_map_id = 0; // 已选择地图，等待传送门选择
        std::vector<Pathing::PortalProp> pending_portals;
    };
    // =========================================================================
    // 统一编辑器状态 — 所有 From/To 端点数据在一个结构体中，便于快照和交换。
    // =========================================================================
    struct EndpointEditorState {
        EditorEndpoint endpoint;
        int type = 0; // ConnectionType 组合索引
        EditorNpcFields npc;
        char search_buf[128] = {};
        MapSearchState search_state;
    };

    struct ConnectionEditorState {
        EndpointEditorState from;
        EndpointEditorState to;
        char notes[256] = {};
        bool no_draw = false;
        bool one_way = false;
        int selected_connection = -1;
    };

    ConnectionEditorState editor;

    // 向后兼容引用，使现有代码无需大规模重命名。
    auto& editor_from = editor.from.endpoint;
    auto& editor_to = editor.to.endpoint;

    bool pending_connection_lines_update = false;


    // 延迟移除线条，以便在 WorldMapWidget::Draw 期间不会使迭代器失效；在下次 Draw 时排空。
    // 旧指针是安全的：RemoveCustomLines 只会释放仍在活动列表中的指针，永远不会解引用队列。
    std::vector<CustomRenderer::CustomLine*> pending_line_removals;

    void DeferRemoveLines(std::vector<CustomRenderer::CustomLine*>& lines)
    {
        pending_line_removals.insert(pending_line_removals.end(), lines.begin(), lines.end());
        lines.clear();
    }

    void ProcessDeferredRemovals()
    {
        if (pending_line_removals.empty()) return;
        // 单次 O(N) 遍历：每次 RemoveCustomLine 是 O(N)（线性查找 + vector 擦除），
        // 因此逐个移除导航网格覆盖层的数千条线曾经是 O(N²)，是地图加载卡顿的主要来源。
        Minimap::Instance().custom_renderer.RemoveCustomLines(pending_line_removals);
        pending_line_removals.clear();
    }

    std::vector<CustomRenderer::CustomLine*> saved_connection_lines;

    void ClearSavedConnectionLines()
    {
        DeferRemoveLines(saved_connection_lines);
    }

    bool connections_changed = false; // 连接被修改时设置，由 BuildMapGraph 调用者消费

    void UpdateSavedConnectionLines()
    {
        connections_changed = true; // 在下次 BuildMapGraph 调用时使图无效
        ClearSavedConnectionLines();
        ClearEditorHighlightLines();
        if (!draw_portals) return;
        auto cur_map = GW::Map::GetMapID();
        const auto cur_area_cl = GW::Map::GetMapInfo();
        const uint32_t cur_continent_cl = cur_area_cl ? (uint32_t)cur_area_cl->continent : 0;

        for (const auto& conn : portal_connections.GetAll()) {
            // 跳过双向连接的逆向条目以避免重复绘制
            if (!conn.IsOneWay() && conn.from_map > conn.to_map) continue;
            if (conn.no_draw) continue;
            // 跳过其他大陆的连接（campaign=0 始终显示以保持兼容）
            if (conn.campaign && conn.campaign != cur_continent_cl) continue;

            // 跳过缺少世界地图坐标的条目
            if ((conn.from_wm_pos.x == 0.f && conn.from_wm_pos.y == 0.f) || (conn.to_wm_pos.x == 0.f && conn.to_wm_pos.y == 0.f)) continue;

            GW::GamePos p1{}, p2{};
            if (!WorldMapWidget::WorldMapToGamePos(conn.from_wm_pos, p1)) continue;
            if (!WorldMapWidget::WorldMapToGamePos(conn.to_wm_pos, p2)) continue;
            auto* line = Minimap::Instance().custom_renderer.AddCustomLine(p1, p2);
            line->map = cur_map;
            // 按“最显著”端点类型着色
            auto notable_type = (conn.from_type > conn.to_type) ? conn.from_type : conn.to_type;
            switch (notable_type) {
                case Pathing::ConnectionType::Disabled:
                    line->color = 0xFFFF0000;
                    break;
                case Pathing::ConnectionType::NPC:
                    line->color = 0xFF4488FF;
                    break;
                case Pathing::ConnectionType::Dummy:
                    line->color = 0xFFFFAA00;
                    break;
                default:
                    line->color = 0xFF00FF00;
                    break;
            }
            line->draw_on_mission_map = true;
            line->draw_on_minimap = false;
            line->created_by_toolbox = true;
            saved_connection_lines.push_back(line);
        }
    }

    // LoadAndShowMapsAtWorldPos 在匿名命名空间之后定义

    std::vector<CustomRenderer::CustomLine*> editor_highlight_lines;

    void ClearEditorHighlightLines()
    {
        DeferRemoveLines(editor_highlight_lines);
    }



    void UpdateBoundsLines();    // 前向声明
    void UpdatePortalMarkers();  // 前向声明
    void UpdateGraphEdgeLines(); // 前向声明
    struct PortalPair {
        GW::GamePos pos_a, pos_b; // 各自地图中的游戏坐标
        GW::Vec2f wm_mid;         // 世界地图中点
        float pair_dist;          // 两个传送门之间的世界地图距离
    };
    std::vector<PortalPair> FindPortalPairs(GW::Constants::MapID map_a, GW::Constants::MapID map_b); // 前向声明
    const struct CachedMapInfo* GetCachedMapInfo(GW::Constants::MapID map_id);                       // 前向声明
    uint32_t GetMapFileId(GW::Constants::MapID map_id);                                              // 前向声明
    GW::GamePos ToCurrentMapCoords(const GW::GamePos& pos, GW::Constants::MapID src_map);            // 前向声明

    Pathing::MilePath* LoadMapFromDAT(GW::Constants::MapID map_id, bool allow_load)
    {
        if (IsInterestingMapForCacheTrace(map_id)) {
            PATH_LOG_INFO("[CacheTrace] LoadMapFromDAT(%d) 进入", (int)map_id);
        }
        const uint32_t fid = GetMapFileId(map_id);
        if (!fid) {
            if (allow_load && warned_no_fid_maps.insert((uint32_t)map_id).second) {
                PATH_LOG_ERROR("LoadMapFromDAT: 地图 %d 没有 file_id（先访问该地图以缓存它）", (int)map_id);
            }
            return nullptr;
        }

        // 快速路径：文件已驻留。readFromDat 会路由到游戏的序列化文件子系统，
        // 因此在每次重新计算时重新读取它会阻塞游戏线程；file_id 在会话内唯一标识地图
        //（与 GetMilepathForMap 的行为一致）。
        for (const auto& [hash, mp] : mile_paths_by_coords) {
            if (static_cast<uint32_t>(hash & 0xFFFFFFFF) == fid) {
                TouchLru(hash);
                return mp;
            }
        }

        // ctx 回退条目（下面的边界不匹配）由 map_id + 实时节点数键控，而不是 fid —
        // 探测它，否则驻留路径永远不会看到它，预热循环会永远重新读取 DAT。
        if (map_id == GW::Map::GetMapID()) {
            if (const auto mc = GW::GetMapContext(); mc && mc->path && mc->path->staticData) {
                const auto ctx_hash = static_cast<uint64_t>(map_id) | (static_cast<uint64_t>(mc->path->pathNodes.size()) << 32);
                if (const auto it = mile_paths_by_coords.find(ctx_hash); it != mile_paths_by_coords.end()) {
                    TouchLru(ctx_hash);
                    return it->second;
                }
            }
        }

        // 未驻留：下面的 DAT 读取 + 解析会阻塞调用者。在游戏线程上拒绝。
        if (!allow_load) return nullptr;

        // 本会话已知不可读 — 跳过阻塞的重新读取和重复日志。
        if (dat_load_failed_fids.contains(fid)) return nullptr;

        PATH_LOG_INFO("LoadMapFromDAT: 地图=%d file_id=%u (0x%X)", (int)map_id, fid, fid);

        auto dat_data = Pathing::PathingMapData();
        if (!Pathing::LoadPathingMapDataFromDAT(fid, &dat_data)) {
            dat_load_failed_fids.insert(fid);
            PATH_LOG_ERROR("LoadMapFromDAT: 地图 %d file_id=%u 的 DAT 解析失败", (int)map_id, fid);
            return nullptr;
        }

        // 对于当前地图，将 DAT 与实时内存交叉检查：边界不匹配意味着 file_id 过时，因此使用 MapContext 副本。
        Pathing::PathingMapData ctx_data;
        Pathing::PathingMapData* chosen = &dat_data;
        if (map_id == GW::Map::GetMapID()) {
            if (!Pathing::LoadFromMapContext(GW::GetMapContext(), fid, &ctx_data)) {
                PATH_LOG_ERROR("LoadMapFromDAT: 地图 %d file_id=%u 的上下文解析失败", (int)map_id, fid);
            }
            else if (ctx_data.bounds_max.x != dat_data.bounds_max.x || ctx_data.bounds_max.y != dat_data.bounds_max.y) {
                PATH_LOG_ERROR("LoadMapFromDAT: 当前地图的上下文 bounds_max 与 DAT 传送门不匹配，file_id=%u - 检查 InfoWindow 以更新此地图的地图文件 ID！暂时使用地图上下文数据。", fid);
                chosen = &ctx_data;
            }
        }
        auto& map_data = *chosen;

        auto hash = chosen == &ctx_data ? static_cast<uint64_t>(map_id) : static_cast<uint64_t>(fid);
        hash |= static_cast<uint64_t>(map_data.pathNodeSize) << 32;

        // 缓存地图信息用于边界绘制
        CachedMapInfo info;
        info.map_id = map_id;
        info.bounds_min = map_data.bounds_min;
        info.bounds_max = map_data.bounds_max;
        info.portal_props = map_data.portal_props;
        cached_map_info[hash] = info;
        CacheSharedFileHashMaps(info);

        PATH_LOG_INFO("已加载地图 %d 的 DAT：%d 个传送门属性，边界=(%.0f,%.0f)-(%.0f,%.0f)", (int)map_id, (int)map_data.portal_props.size(), info.bounds_min.x, info.bounds_min.y, info.bounds_max.x, info.bounds_max.y);

        // 收集共享此 file_hash 的所有 MapID，以便包含来自任何一个的传送器
        std::vector<GW::Constants::MapID> all_ids;
        for (const auto& [file_hash, entries] : constant_maps_info) {
            if ((uint32_t)file_hash != fid) continue;
            for (const auto& entry : entries)
                all_ids.push_back(entry.map_id);
        }

        // 轻量级（full_build=false）：仅保留原始地图数据；可视图在首次 AStar 行走时延迟构建。
        auto* m = new Pathing::MilePath(std::move(map_data), map_id, all_ids, false);
        mile_paths_by_coords[hash] = m;
        TouchLru(hash); // 淘汰本身延迟到路径工作结束（RouteJobScope）
        // 将这些线池变异推迟到主线程；LoadMapFromDAT 在工作线程上运行，AddCustomLine 不是线程安全的。
        Resources::EnqueueMainTask([] {
            UpdateBoundsLines();
            if (draw_portals) UpdatePortalMarkers();
        });
        return m;
    }

    Pathing::MilePath* LoadMapFromContext(GW::Constants::MapID map_id, bool allow_load = true)
    {
        const auto mc = GW::GetMapContext();
        if (!(mc && mc->path && mc->path->staticData)) return nullptr;

        // pathNodeSize == pathNodes.size()（LoadFromMapContext 直接设置），因此缓存键可
        // 在不进行完整深拷贝的情况下用于检查驻留状态 — 在支付解析成本之前先检查。
        const auto hash = static_cast<uint64_t>(map_id)
            | (static_cast<uint64_t>(mc->path->pathNodes.size()) << 32);
        if (const auto it = mile_paths_by_coords.find(hash); it != mile_paths_by_coords.end()) {
            TouchLru(hash);
            return it->second;
        }

        // 未驻留：下面的 LoadFromMapContext 是完整的深拷贝 + 指针修复。在游戏线程上拒绝。
        if (!allow_load) return nullptr;

        auto map_data = Pathing::PathingMapData();
        if (!Pathing::LoadFromMapContext(mc, 0, &map_data)) return nullptr;

        CachedMapInfo info;
        info.map_id = map_id;
        info.bounds_min = map_data.bounds_min;
        info.bounds_max = map_data.bounds_max;
        cached_map_info[hash] = info;
        CacheSharedFileHashMaps(info);

        auto* m = new Pathing::MilePath(std::move(map_data), map_id, {map_id}, true);
        mile_paths_by_coords[hash] = m;
        TouchLru(hash);
        Resources::EnqueueMainTask([] {
            UpdateBoundsLines();
            if (draw_portals) UpdatePortalMarkers();
        });
        return m;
    }

    // 返回当前地图的 milepath 指针，若处于无效状态则返回 nullptr。
    // allow_load=false 在游戏线程上是安全的（仅驻留查找）；true 会在 DAT 上阻塞，必须在游戏线程外运行。
    Pathing::MilePath* GetMilepathForCurrentMap(bool allow_load = true)
    {
        // TODO：也许使用其他模块中的 bool GetIsMapReady()？
        if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading || !GW::Map::GetIsMapLoaded()) return nullptr;
        const auto map_id = GW::Map::GetMapID();
        if (map_id == GW::Constants::MapID::None) return nullptr;
        // 优先使用 DAT（与坐标边界共享）；仅在无 file_id 时回退到地图上下文。
        if (GetMapFileId(map_id)) return LoadMapFromDAT(map_id, allow_load);
        return LoadMapFromContext(map_id, allow_load);
    }

    // 后台预热，使游戏线程永不在 DAT 读取上阻塞。ReadyForPathing/覆盖层探测调用
    // GetResidentMilepathOrPrewarm()；如果当前地图尚未驻留，则单个工作线程任务加载它。
    std::atomic<bool> prewarm_in_flight = false;

    void PrewarmCurrentMap()
    {
        bool expected = false;
        if (!prewarm_in_flight.compare_exchange_strong(expected, true)) return; // 一次只允许一个加载
        Resources::EnqueueWorkerTask([] {
            std::scoped_lock route_lock(route_mutex); // 与路径构建相同的序列化；见 route_mutex
            RouteJobScope job_scope;
            GetMilepathForCurrentMap(true); // 阻塞的 DAT 读取在这里发生，在工作线程上
            prewarm_in_flight = false;
        });
    }

    // 游戏线程安全的当前地图 MilePath 访问器：仅当已驻留时返回，否则启动后台加载并返回 nullptr。
    // 绝不读取 DAT 或在调用者线程上解析地图上下文。
    Pathing::MilePath* GetResidentMilepathOrPrewarm()
    {
        if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading || !GW::Map::GetIsMapLoaded()) return nullptr;
        {
            // 尝试锁定，以便从不在游戏线程上阻塞；持锁意味着工作线程正在构建中 → 视为未就绪。
            std::unique_lock route_lock(route_mutex, std::try_to_lock);
            if (route_lock.owns_lock()) {
                if (const auto m = GetMilepathForCurrentMap(false)) return m;
            }
        }
        PrewarmCurrentMap();
        return nullptr;
    }

    std::vector<CustomRenderer::CustomLine*> navmesh_edge_lines; // 仅用于可视化的导航网格多边形边覆盖层
    PathfindingWindow::Settings settings;
    constexpr const char* path_recalc_distance_help = "在重新计算渲染的路径之前你必须移动多远（游戏单位/gwinches）。值越低响应越快，但负载越重；重新计算速率上限约为 30 次/秒。";
    constexpr const char* navmesh_overlay_help = "在你附近的地面上绘制导航网格的多边形边，包含正确的地形高度（桥梁等）。";
    constexpr const char* navmesh_sample_spacing_help = "覆盖层在绘制边时采样地形高度的间隔（游戏单位）。值越低，线条越贴合地面/台阶，但需要构建和绘制的顶点越多。";
    std::vector<CustomRenderer::CustomLine*> bounds_lines;
    std::vector<CustomRenderer::CustomLine*> graph_edge_lines;
    std::vector<CustomRenderer::CustomLine*> portal_marker_lines;
    std::vector<CustomRenderer::CustomLine*> hover_highlight_lines;

    void ClearBoundsLines()
    {
        DeferRemoveLines(bounds_lines);
    }

    void ClearGraphEdgeLines()
    {
        DeferRemoveLines(graph_edge_lines);
    }


    void ClearPortalMarkerLines()
    {
        DeferRemoveLines(portal_marker_lines);
    }

    void ClearHoverHighlightLines()
    {
        DeferRemoveLines(hover_highlight_lines);
    }

    bool IsPortalConnected(GW::Constants::MapID map_id, const GW::Vec2f& pos, float threshold_sq = 500.f * 500.f)
    {
        for (const auto& c : portal_connections.GetAll()) {
            if (c.from_map == map_id) {
                float dx = c.from_pos.x - pos.x, dy = c.from_pos.y - pos.y;
                if (dx * dx + dy * dy < threshold_sq) return true;
            }
            if (c.to_map == map_id) {
                float dx = c.to_pos.x - pos.x, dy = c.to_pos.y - pos.y;
                if (dx * dx + dy * dy < threshold_sq) return true;
            }
        }
        return false;
    }

    void UpdatePortalMarkers()
    {
        ClearPortalMarkerLines();
        if (!draw_portals) return;

        auto cur_map = GW::Map::GetMapID();
        const auto cur_area = GW::Map::GetMapInfo();
        if (!cur_area) return;
        auto cur_continent = cur_area->continent;
        constexpr float sz_default = 600.f;
        constexpr float sz_connected = 750.f;
        constexpr float sz_selected_bump = 350.f;

        auto matches_editor = [](GW::Constants::MapID map_id, const GW::Vec2f& pos, const EditorEndpoint& e) {
            if (!e.set || e.map_id != map_id) return false;
            constexpr float eps = 10.f;
            return fabsf(pos.x - e.pos.x) < eps && fabsf(pos.y - e.pos.y) < eps;
        };

        for (const auto& [hash, info] : cached_map_info) {
            const auto area = GW::Map::GetMapInfo(info.map_id);
            if (area && area->continent != cur_continent) continue;

            for (const auto& pp : info.portal_props) {
                bool connected = IsPortalConnected(info.map_id, {pp.pos.x, pp.pos.y});
                bool selected = matches_editor(info.map_id, {pp.pos.x, pp.pos.y}, editor_from) || matches_editor(info.map_id, {pp.pos.x, pp.pos.y}, editor_to);
                DWORD color = connected ? 0xFF00FF00 : 0xFFFF8000;
                float sz = connected ? sz_connected : sz_default;
                if (selected) sz += sz_selected_bump;

                auto p = ToCurrentMapCoords({pp.pos.x, pp.pos.y, 0}, info.map_id);
                auto add = [&](const GW::GamePos& a, const GW::GamePos& b) {
                    auto* line = Minimap::Instance().custom_renderer.AddCustomLine(a, b);
                    line->map = cur_map;
                    line->color = color;
                    line->draw_on_mission_map = true;
                    line->draw_on_minimap = false;
                    line->created_by_toolbox = true;
                    portal_marker_lines.push_back(line);
                };
                // 菱形形状以获得更好的可见性
                add({p.x - sz, p.y, p.zplane}, {p.x, p.y + sz, p.zplane});
                add({p.x, p.y + sz, p.zplane}, {p.x + sz, p.y, p.zplane});
                add({p.x + sz, p.y, p.zplane}, {p.x, p.y - sz, p.zplane});
                add({p.x, p.y - sz, p.zplane}, {p.x - sz, p.y, p.zplane});
            }
        }
    }


    void UpdateBoundsLines()
    {
        ClearBoundsLines();
        if (!draw_map_bounds) return;

        auto cur_map = GW::Map::GetMapID();

        for (const auto& [hash, info] : cached_map_info) {
            // 源地图游戏坐标中的 4 个角
            GW::GamePos tl = {info.bounds_min.x, info.bounds_min.y, 0};
            GW::GamePos tr = {info.bounds_max.x, info.bounds_min.y, 0};
            GW::GamePos br = {info.bounds_max.x, info.bounds_max.y, 0};
            GW::GamePos bl = {info.bounds_min.x, info.bounds_max.y, 0};

            tl = ToCurrentMapCoords(tl, info.map_id);
            tr = ToCurrentMapCoords(tr, info.map_id);
            br = ToCurrentMapCoords(br, info.map_id);
            bl = ToCurrentMapCoords(bl, info.map_id);

            auto add = [&](const GW::GamePos& a, const GW::GamePos& b) {
                auto* line = Minimap::Instance().custom_renderer.AddCustomLine(a, b);
                line->map = cur_map;
                line->color = 0x8000C8C8; // 青色，半透明
                line->draw_on_mission_map = true;
                line->draw_on_minimap = false;
                line->created_by_toolbox = true;
                bounds_lines.push_back(line);
            };

            add(tl, tr);
            add(tr, br);
            add(br, bl);
            add(bl, tl);
        }
    }

    // UpdateGraphEdgeLines 在地图图声明之后实现

    Pathing::AStar* astar = nullptr;

    volatile bool pending_terminate = false;
    volatile bool pending_worker_task = false;
    std::atomic<bool> pathing_enabled = false; // Initialize() 和 SignalTerminate() 之间为 true；由 QuestModule 轮询

    GW::HookEntry gw_ui_hookentry;

    GW::GamePos path_from;
    GW::GamePos path_to;
    GW::Vec2f path_from_world{};
    GW::Vec2f path_to_world{};
    GW::Constants::MapID path_from_map = GW::Constants::MapID::None;
    GW::Constants::MapID path_to_map = GW::Constants::MapID::None;

    // 每个 file_hash 的存储点（在地图切换时保留，与共享相同 file 的地图共享）
    struct StoredPoints {
        GW::GamePos from{}, to{};
        GW::Vec2f from_world{}, to_world{};
        bool from_set = false, to_set = false;
    };
    std::unordered_map<uint32_t, StoredPoints> points_by_hash; // 由 file_hash 键控

    std::vector<CustomRenderer::CustomLine*> marker_lines;
    std::vector<CustomRenderer::CustomLine*> path_lines;
    std::vector<CustomRenderer::CustomLine*> portal_pair_lines;

    void ClearMarkerLines()
    {
        DeferRemoveLines(marker_lines);
    }

    void ClearPathLines()
    {
        DeferRemoveLines(path_lines);
    }

    void ClearPortalPairLines()
    {
        DeferRemoveLines(portal_pair_lines);
    }


    void AddMarkerCross(const GW::GamePos& pos, DWORD color, GW::Constants::MapID map_id)
    {
        constexpr float sz = 200.f;
        auto add = [&](const GW::GamePos& a, const GW::GamePos& b) {
            auto* line = Minimap::Instance().custom_renderer.AddCustomLine(a, b);
            line->map = map_id;
            line->color = color;
            line->draw_on_mission_map = true;
            line->draw_on_minimap = true;
            line->created_by_toolbox = true;
            marker_lines.push_back(line);
        };
        add({pos.x - sz, pos.y - sz, pos.zplane}, {pos.x + sz, pos.y + sz, pos.zplane});
        add({pos.x - sz, pos.y + sz, pos.zplane}, {pos.x + sz, pos.y - sz, pos.zplane});
    }

    // 通过世界地图将游戏坐标从源地图转换到当前地图坐标
    GW::GamePos ToCurrentMapCoords(const GW::GamePos& pos, GW::Constants::MapID src_map)
    {
        if (src_map == GW::Map::GetMapID() || src_map == GW::Constants::MapID::None) return pos;
        GW::Vec2f world_pos;
        GW::GamePos cur_pos;
        if (WorldMapWidget::GamePosToWorldMap(pos, world_pos, src_map) && WorldMapWidget::WorldMapToGamePos(world_pos, cur_pos)) {
            cur_pos.zplane = pos.zplane;
            return cur_pos;
        }
        return pos;
    }

    void UpdateMarkers(const GW::GamePos& from, const GW::GamePos& to)
    {
        ClearMarkerLines();
        auto cur_map = GW::Map::GetMapID();
        if (from.x != 0.f || from.y != 0.f) {
            auto display_from = ToCurrentMapCoords(from, path_from_map);
            AddMarkerCross(display_from, 0xFF00FF00, cur_map); // 绿色
        }
        if (to.x != 0.f || to.y != 0.f) {
            auto display_to = ToCurrentMapCoords(to, path_to_map);
            AddMarkerCross(display_to, 0xFFFF0000, cur_map); // 红色
        }
    }

    bool IsOutpostMap(GW::Constants::MapID map_id); // forward decl

    // “此处不画线”哨兵，存放在扁平 full_path 数组中；插入到地下（no_draw）地图周围的段之间。
    constexpr float PATH_BREAK_VALUE = FLT_MAX;
    inline bool IsPathBreak(const GW::GamePos& p)
    {
        return p.x == PATH_BREAK_VALUE;
    }
    inline bool IsPathBreak(const GW::Vec2f& p)
    {
        return p.x == PATH_BREAK_VALUE;
    }

    // 隐藏在世界地图上的路径段（地下地图），但保留原生坐标 + 地图标签，以便进入后渲染。
    struct HiddenPathSegment {
        std::vector<GW::GamePos> points;
        GW::Constants::MapID map_id;
    };

    // 如果 map_a 和 map_b 之间有任何手动连接（任意方向，且可探索→前哨站 file_hash 传播）的 no_draw=true。
    bool HasNoDrawConnection(GW::Constants::MapID map_a, GW::Constants::MapID map_b)
    {
        uint32_t fh_a = GetMapFileId(map_a);
        uint32_t fh_b = GetMapFileId(map_b);
        auto maps_match = [](GW::Constants::MapID mid, GW::Constants::MapID target, uint32_t target_fh) {
            if (mid == target) return true;
            if (IsOutpostMap(mid)) return false;
            return target_fh && GetMapFileId(mid) == target_fh;
        };
        for (const auto& conn : portal_connections.GetAll()) {
            if (!conn.no_draw) continue;
            bool fwd = maps_match(conn.from_map, map_a, fh_a) && maps_match(conn.to_map, map_b, fh_b);
            bool rev = maps_match(conn.from_map, map_b, fh_b) && maps_match(conn.to_map, map_a, fh_a);
            if (fwd || rev) return true;
        }
        return false;
    }

    // src_map 游戏坐标 → 世界坐标（公共跨地图空间，无溢出）；
    // 哨兵值透传。
    bool SegmentToWorld(const std::vector<GW::GamePos>& points, GW::Constants::MapID src_map, std::vector<GW::Vec2f>& out)
    {
        out.reserve(out.size() + points.size());
        for (const auto& p : points) {
            if (IsPathBreak(p)) {
                out.push_back({PATH_BREAK_VALUE, PATH_BREAK_VALUE});
                continue;
            }
            GW::Vec2f w;
            if (WorldMapWidget::GamePosToWorldMap(p, w, src_map)) out.push_back(w);
        }
        return true;
    }

    std::vector<GW::GamePos> ConvertPathToCurrentMap(const std::vector<GW::GamePos>& points, GW::Constants::MapID src_map)
    {
        if (src_map == GW::Map::GetMapID()) return points;

        std::vector<GW::GamePos> converted;
        converted.reserve(points.size());
        for (const auto& p : points) {
            if (IsPathBreak(p)) {
                converted.push_back(p);
                continue;
            }
            GW::Vec2f world_pos;
            GW::GamePos cur_pos;
            if (WorldMapWidget::GamePosToWorldMap(p, world_pos, src_map) && WorldMapWidget::WorldMapToGamePos(world_pos, cur_pos)) {
                cur_pos.zplane = p.zplane;
                converted.push_back(cur_pos);
            }
        }
        return converted;
    }

    void DrawPathAsLines(const std::vector<GW::GamePos>& points, GW::Constants::MapID src_map)
    {
        ClearPathLines();
        auto cur_map = GW::Map::GetMapID();
        const auto& draw_points = (src_map != cur_map) ? ConvertPathToCurrentMap(points, src_map) : points;

        for (size_t i = 0; i + 1 < draw_points.size(); i++) {
            // 跳过触及路径中断哨兵的线条 — 用于在多地图路径中隐藏穿过地下地图的段。
            if (IsPathBreak(draw_points[i]) || IsPathBreak(draw_points[i + 1])) continue;
            auto* line = Minimap::Instance().custom_renderer.AddCustomLine(draw_points[i], draw_points[i + 1]);
            line->map = cur_map;
            line->color = 0xFFFFFF00; // 黄色
            line->draw_on_mission_map = true;
            line->draw_on_minimap = true;
            line->created_by_toolbox = true;
            path_lines.push_back(line);
        }
    }

    // 添加原生地图坐标中标记了 map_id 的地下段：在世界地图上隐藏，但一旦玩家进入该地图则在世界中绘制。
    // 在 DrawPathAsLines 之后调用（不清除）— 段共享 path_lines。
    void AddHiddenUndergroundSegmentLines(const std::vector<HiddenPathSegment>& segs)
    {
        for (const auto& seg : segs) {
            for (size_t i = 0; i + 1 < seg.points.size(); i++) {
                auto* line = Minimap::Instance().custom_renderer.AddCustomLine(seg.points[i], seg.points[i + 1]);
                line->map = seg.map_id;   // 原生地下地图，不是 cur_map
                line->color = 0xFFFFFF00; // 黄色
                line->draw_on_mission_map = true;
                line->draw_on_minimap = true;
                line->created_by_toolbox = true;
                path_lines.push_back(line);
            }
        }
    }


    // 如果上次 AStar 计算与我们请求的匹配，则返回 false。
    bool NeedsRecalculating(const GW::GamePos& from, const GW::GamePos& to)
    {
        if (!(astar && astar->m_path.ready() && astar->m_path.points().size())) return true;
        return from != astar->m_path.points().at(0) || to != astar->m_path.points().at(astar->m_path.points().size() - 1);
    }

    Pathing::MilePath* GetMilepathForMap(GW::Constants::MapID map_id)
    {
        // 按 file_hash 查找，以便共享哈希的地图（例如 107/135 使用哈希 0xC77A）
        // 无论哪个 MapID 请求，都可以返回相同的 MilePath。
        uint32_t fh = GetMapFileId(map_id);
        if (!fh) return nullptr;
        for (const auto& [hash, mp] : mile_paths_by_coords) {
            if ((uint32_t)(hash & 0xFFFFFFFF) == fh) {
                TouchLru(hash);
                return mp;
            }
        }
        return nullptr;
    }

    // =========================================================================
    // 地图连接图 — 通过重叠的世界地图边界
    // =========================================================================

    struct MapGraphNode {
        GW::Constants::MapID map_id;
        uint32_t file_hash;
        ImRect wm_bounds; // 世界地图边界
        GW::Continent continent;
    };

    std::vector<MapGraphNode> map_graph_nodes;
    bool map_graph_built = false;

    void BuildMapGraph()
    {
        if (connections_changed) {
            map_graph_built = false;
            connections_changed = false;
        }
        if (map_graph_built) return;
        map_graph_built = true;
        map_graph_nodes.clear();

        // 收集地图，按 file_hash 去重（相同文件 = 相同物理地图）
        std::set<uint32_t> seen_file_hashes;
        for (const auto& [file_hash, entries] : constant_maps_info) {
            if (!file_hash) continue;
            if (seen_file_hashes.contains((uint32_t)file_hash)) continue;

            GW::Constants::MapID chosen = GW::Constants::MapID::None, first_valid = GW::Constants::MapID::None;
            ImRect chosen_bounds, first_bounds;
            for (const auto& entry : entries) {
                const auto area = GW::Map::GetMapInfo(entry.map_id);
                if (!area || area->GetIsPvP() || !area->GetIsOnWorldMap()) continue;
                ImRect bounds;
                if (!GW::Map::GetMapWorldMapBounds(area, &bounds)) continue;
                if (bounds.GetWidth() < 1.f || bounds.GetHeight() < 1.f) continue;
                if (first_valid == GW::Constants::MapID::None) { first_valid = entry.map_id; first_bounds = bounds; }
                if (!IsOutpostMap(entry.map_id)) { chosen = entry.map_id; chosen_bounds = bounds; break; }
            }
            if (chosen == GW::Constants::MapID::None) { chosen = first_valid; chosen_bounds = first_bounds; }
            if (chosen != GW::Constants::MapID::None) {
                const auto area = GW::Map::GetMapInfo(chosen);
                map_graph_nodes.push_back({chosen, (uint32_t)file_hash, chosen_bounds, area ? area->continent : GW::Continent::Kryta});
                seen_file_hashes.insert((uint32_t)file_hash);
            }
        }
        // 添加连接引用的尚未在图中的地图，按精确 MapID 去重（而非 file_hash）— 否则当可探索兄弟被选中时，连接链中的前哨站会被丢弃，Dijkstra 会扩展缺失节点。
        for (const auto& conn : portal_connections.GetAll()) {
            for (auto mid : {conn.from_map, conn.to_map}) {
                if (mid == GW::Constants::MapID::None) continue;
                bool exists = false;
                for (const auto& n : map_graph_nodes) {
                    if (n.map_id == mid) {
                        exists = true;
                        break;
                    }
                }
                if (exists) continue;
                uint32_t fh = GetMapFileId(mid);
                const auto* area = GW::Map::GetMapInfo(mid);
                auto continent = area ? area->continent : GW::Continent::Kryta;
                map_graph_nodes.push_back({mid, fh, ImRect(), continent});
                if (IsInterestingMapForCacheTrace(mid)) {
                    PATH_LOG_INFO("[CacheTrace] BuildMapGraph 添加 mid=%d fh=0x%X（路径=portal_connections，连接 from=%d to=%d）", (int)mid, fh, (int)conn.from_map, (int)conn.to_map);
                }
            }
        }
        PATH_LOG_INFO("地图图：%d 个节点（含连接引用的）", (int)map_graph_nodes.size());
    }

    bool IsOutpostMap(GW::Constants::MapID map_id); // 前向声明

    // 查找相邻地图（重叠边界，相同大陆）
    std::vector<GW::Constants::MapID> GetAdjacentMaps(GW::Constants::MapID map_id)
    {
        BuildMapGraph();
        const MapGraphNode* src = nullptr;
        for (const auto& node : map_graph_nodes) {
            if (node.map_id == map_id) {
                src = &node;
                break;
            }
        }
        if (!src) return {};

        // 连接引用的节点边界为空；跳过它们的分支，否则两个空矩形在原点“重叠”。
        const bool src_empty = src->wm_bounds.GetWidth() < 1.f || src->wm_bounds.GetHeight() < 1.f;

        std::vector<GW::Constants::MapID> result;
        for (const auto& node : map_graph_nodes) {
            if (node.map_id == map_id || node.file_hash == src->file_hash) continue;
            if (node.continent != src->continent) continue;
            if (src_empty) continue;
            if (node.wm_bounds.GetWidth() < 1.f || node.wm_bounds.GetHeight() < 1.f) continue;
            // 检查边界重叠，带容差
            constexpr float tolerance = 5.f; // 世界地图单位
            ImRect expanded = node.wm_bounds;
            expanded.Expand(tolerance);
            if (src->wm_bounds.Overlaps(expanded)) {
                result.push_back(node.map_id);
            }
        }

        // 添加手动连接中的邻居。连接传播规则：可探索地图上的连接应用于所有 file_hash 兄弟；
        // 前哨站上的连接仅局限于该确切前哨站（防止 348/244/218 交叉污染）。
        auto contains = [&](GW::Constants::MapID mid) {
            for (auto m : result)
                if (m == mid) return true;
            return false;
        };
        const uint32_t fh_cur = src->file_hash;
        const bool src_is_outpost = IsOutpostMap(map_id);
        auto matches_current = [&](GW::Constants::MapID conn_map_id) -> bool {
            if (conn_map_id == map_id) return true;
            if (IsOutpostMap(conn_map_id)) return false;
            // 前哨站不能继承其可探索兄弟的连接：那些传送门需要额外的前哨站→可探索跳转，
            // 选择器没有建模，因此将其视为直接连接会产生虚幻的廉价边（例如 fh 0x85A8 57 继承 56→18）。
            if (src_is_outpost) return false;
            return fh_cur && GetMapFileId(conn_map_id) == fh_cur;
        };
        PATH_LOG_INFO("[GetAdj] 地图=%d fh=0x%X 是前哨站=%d", (int)map_id, fh_cur, IsOutpostMap(map_id) ? 1 : 0);
        for (const auto& conn : portal_connections.GetAll()) {
            if (conn.from_type == Pathing::ConnectionType::Disabled || conn.to_type == Pathing::ConnectionType::Disabled) continue;
            bool fwd_match = matches_current(conn.from_map);
            bool rev_match = !conn.IsOneWay() && matches_current(conn.to_map);
            if (fwd_match || rev_match) {
                PATH_LOG_INFO("[GetAdj]   连接 from=%d to=%d ftype=%d ttype=%d 单向=%d fwd=%d rev=%d", (int)conn.from_map, (int)conn.to_map, (int)conn.from_type, (int)conn.to_type, conn.IsOneWay() ? 1 : 0, fwd_match ? 1 : 0, rev_match ? 1 : 0);
            }
            if (fwd_match && conn.to_map != map_id && !contains(conn.to_map)) {
                result.push_back(conn.to_map);
            }
            if (rev_match && conn.from_map != map_id && !contains(conn.from_map)) {
                result.push_back(conn.from_map);
            }
        }

        return result;
    }

    // 点 (px,py) 到轴对齐矩形 [mn,mx] 的外部距离；内部时为 0。
    inline float RectOutsideDistance(float px, float py, const Pathing::Vec2f& mn, const Pathing::Vec2f& mx)
    {
        const float dx = std::max({mn.x - px, 0.f, px - mx.x});
        const float dy = std::max({mn.y - py, 0.f, py - mx.y});
        return std::sqrt(dx * dx + dy * dy);
    }

    // 将目标位置钳制到地图的真实游戏边界内，以便世界→游戏投影落在共享边界外几单位的标记仍以该地图为目标
    //（A* 随后将其解析到最近的可行走节点）。在内部时无操作。
    inline GW::GamePos ClampGoalToMapBounds(const GW::GamePos& g, GW::Constants::MapID map_id)
    {
        Pathing::Vec2f mn, mx;
        if (!Pathing::GetMapGameBoundsFromDAT(GetMapFileId(map_id), mn, mx)) return g;
        return {std::clamp(g.x, mn.x, mx.x), std::clamp(g.y, mn.y, mx.y), g.zplane};
    }

    std::vector<GW::Constants::MapID> RankCandidateMapsForWorldPos(const GW::Vec2f& wm_pos, GW::Constants::MapID prefer_adjacent_to = GW::Constants::MapID::None)
    {
        BuildMapGraph();

        const auto ctx_map = (prefer_adjacent_to != GW::Constants::MapID::None) ? prefer_adjacent_to : GW::Map::GetMapID();
        const auto ctx_area = GW::Map::GetMapInfo(ctx_map);
        const auto ctx_continent = ctx_area ? ctx_area->continent : GW::Continent::Kryta;

        std::vector<GW::Constants::MapID> adjacent;
        if (prefer_adjacent_to != GW::Constants::MapID::None) adjacent = GetAdjacentMaps(prefer_adjacent_to);
        auto is_adjacent = [&](GW::Constants::MapID m) {
            if (m == prefer_adjacent_to) return true;
            for (auto a : adjacent)
                if (a == m) return true;
            return false;
        };

        struct Cand {
            GW::Constants::MapID map;
            bool inside;
            float outside;
            float area;
            bool adjacent;
            bool is_outpost;
        };
        std::vector<Cand> cands;
        for (const auto& node : map_graph_nodes) {
            if (node.continent != ctx_continent) continue;

            ImRect wb = node.wm_bounds;
            if (wb.GetWidth() < 1.f || wb.GetHeight() < 1.f) {
                auto* area = GW::Map::GetMapInfo(node.map_id);
                ImRect b;
                if (!area || !area->GetIsOnWorldMap() || !GW::Map::GetMapWorldMapBounds(area, &b) || b.GetWidth() < 1.f || b.GetHeight() < 1.f) continue;
                wb = b;
            }
            if (!wb.Contains({wm_pos.x, wm_pos.y})) continue;

            GW::GamePos g;
            if (!WorldMapWidget::WorldMapToGamePos(wm_pos, g, node.map_id)) continue;
            // 游戏边界适配仅用于排序候选，从不排除它，因此世界→游戏投影落在可玩边界外几单位的标记（重叠接缝处）
            // 仍然会被尝试，只是排在内部地图之后。
            float outside = 0.f;
            Pathing::Vec2f bmin, bmax;
            if (Pathing::GetMapGameBoundsFromDAT(GetMapFileId(node.map_id), bmin, bmax)) {
                outside = RectOutsideDistance(g.x, g.y, bmin, bmax);
            }

            const float area = wb.GetWidth() * wb.GetHeight();
            cands.push_back({node.map_id, outside == 0.f, outside, area, is_adjacent(node.map_id), IsOutpostMap(node.map_id)});
        }

        std::sort(cands.begin(), cands.end(), [](const Cand& a, const Cand& b) {
            if (a.inside != b.inside) return a.inside;                              // in-bounds maps first
            if (!a.inside && a.outside != b.outside) return a.outside < b.outside;  // then closest to a shared border
            if (a.adjacent != b.adjacent) return a.adjacent;                        // then neighbours of the route's other end
            if (a.is_outpost != b.is_outpost) return !a.is_outpost;
            return a.area < b.area;                                                 // 然后更紧的边界
        });
        std::vector<GW::Constants::MapID> out;
        out.reserve(cands.size());
        for (const auto& c : cands) out.push_back(c.map);
        return out;
    }

    const std::vector<Pathing::PortalProp>& GetPortalPropsForMap(GW::Constants::MapID map_id)
    {
        static const std::vector<Pathing::PortalProp> empty;
        uint32_t fh = GetMapFileId(map_id);
        if (!fh) return empty;

        auto it = portal_props_cache.find(fh);
        if (it != portal_props_cache.end()) return it->second;

        for (const auto& [hash, info] : cached_map_info) {
            if (info.map_id == map_id && !info.portal_props.empty()) {
                portal_props_cache[fh] = info.portal_props;
                return portal_props_cache[fh];
            }
        }

        // 轻量级加载 — 仅传送门属性
        auto& props = portal_props_cache[fh];
        Pathing::LoadPortalPropsFromDAT(fh, props);
        return props;
    }

    // 检查两个地图是否具有传送门属性（边界重叠已是先决条件）
    bool HasPortalConnection(GW::Constants::MapID map_a, GW::Constants::MapID map_b)
    {
        const auto& props_a = GetPortalPropsForMap(map_a);
        const auto& props_b = GetPortalPropsForMap(map_b);
        // 两个地图都需要至少一个传送门属性才能可穿越
        return !props_a.empty() && !props_b.empty();
    }

    // 轻量级地图信息加载 — 仅边界 + 传送门属性，无 MilePath/可视区图
    void EnsureLightweightMapInfo(GW::Constants::MapID map_id, const char* caller)
    {
        if (IsInterestingMapForCacheTrace(map_id)) {
            PATH_LOG_INFO("[CacheTrace] EnsureLightweightMapInfo(%d) caller=%s 已缓存=%d", (int)map_id, caller, GetCachedMapInfo(map_id) ? 1 : 0);
        }
        if (map_id == GW::Map::GetMapID()) return;
        if (GetCachedMapInfo(map_id)) return; // 已有信息

        uint32_t fid = GetMapFileId(map_id);
        if (!fid) return;

        // 仅加载地图信息块以获取边界 + 传送门属性
        ArenaNetFileParser::ArenaNetFile game_asset;
        if (!game_asset.readFromDat(fid, 1)) return;

#pragma pack(push, 1)
        struct MapInfoChunk : ArenaNetFileParser::Chunk {
            uint32_t signature;
            uint8_t version;
            Pathing::Vec2f bounds[2];
        };
#pragma pack(pop)
        const auto map_info_chunk = (MapInfoChunk*)game_asset.FindChunk(ArenaNetFileParser::ChunkType::Map_Info);
        if (!map_info_chunk) return;

        std::vector<Pathing::PortalProp> props;
        Pathing::LoadPortalPropsFromDAT(fid, props);

        // 缓存为轻量级信息（不创建 MilePath）
        auto hash = static_cast<uint64_t>(fid);
        CachedMapInfo info;
        info.map_id = map_id;
        info.bounds_min = map_info_chunk->bounds[0];
        info.bounds_max = map_info_chunk->bounds[1];
        info.portal_props = std::move(props);
        cached_map_info[hash] = std::move(info);
        CacheSharedFileHashMaps(cached_map_info[hash]);
    }

    bool IsOutpostMap(GW::Constants::MapID map_id); // 前向声明

    // True if a (non-disabled) portal connection links these two maps, honouring the connection-spread rule
    // (explorable connections apply to file_hash siblings; outpost connections stay local) and one-way.
    bool HasPortalConnectionBetween(GW::Constants::MapID map_a, GW::Constants::MapID map_b)
    {
        const uint32_t fh_a = GetMapFileId(map_a);
        const uint32_t fh_b = GetMapFileId(map_b);
        auto maps_match = [](GW::Constants::MapID mid, GW::Constants::MapID target, uint32_t target_fh) {
            if (mid == target) return true;
            if (IsOutpostMap(mid)) return false;
            return target_fh && GetMapFileId(mid) == target_fh;
        };
        for (const auto& conn : portal_connections.GetAll()) {
            if (conn.from_type == Pathing::ConnectionType::Disabled || conn.to_type == Pathing::ConnectionType::Disabled) continue;
            const bool fwd = maps_match(conn.from_map, map_a, fh_a) && maps_match(conn.to_map, map_b, fh_b);
            const bool rev = !conn.IsOneWay() && maps_match(conn.from_map, map_b, fh_b) && maps_match(conn.to_map, map_a, fh_a);
            if (fwd || rev) return true;
        }
        return false;
    }

    // 重试的黑名单边（每次 FindPath 调用时清除）
    std::set<uint64_t> blacklisted_edges;

    uint64_t EdgeKey(GW::Constants::MapID a, GW::Constants::MapID b)
    {
        return (uint64_t)(uint32_t)a | ((uint64_t)(uint32_t)b << 32);
    }

    // =====================================================================
    // 传送门连接图 + LazySP 路由器（见 ROUTING_REDESIGN.md）。
    // 拓扑完全基于 portal_connections.json 构建 — 不使用世界地图边界。
    // 节点是按 MapID 键控的传送门端点（从不由 file_hash 合并）；
    // 边是传送门穿越（JSON 连接）和地图内行走（同一物理网格上的两个端点）。
    // 搜索为 LazySP：对乐观欧几里得地图内权重进行 Dijkstra，然后在返回的路径上
    // 加载并细化第一个未评估的地图内边，重复进行。
    // =====================================================================

    // 每次传送门穿越的成本（游戏单位）：适度的每区域偏向，以便路由器更喜欢更少的加载画面，
    // 除非行走绕行节省的距离超过此值。
    constexpr float CROSSING_COST = 2500.f;
    constexpr int MAX_LAZYSP_ITERS = 512;

    bool RunAStarOnMap(GW::Constants::MapID map_id, const GW::GamePos& from, const GW::GamePos& to, std::vector<GW::GamePos>& path_out); // 前向声明

    float WalkDistanceOnMap(GW::Constants::MapID map_id, const GW::GamePos& from, const GW::GamePos& to,
                            bool* out_unreachable = nullptr, bool* out_data_miss = nullptr)
    {
        if (out_unreachable) *out_unreachable = false;
        if (out_data_miss) *out_data_miss = false;
        const float INF = std::numeric_limits<float>::infinity();

        Pathing::MilePath* mp = (map_id == GW::Map::GetMapID()) ? GetMilepathForCurrentMap() : GetMilepathForMap(map_id);
        if (!mp && map_id != GW::Map::GetMapID()) {
            LoadMapFromDAT(map_id);
            mp = GetMilepathForMap(map_id);
        }
        if (!mp) { if (out_data_miss) *out_data_miss = true; return INF; }

        std::vector<GW::GamePos> path;
        const bool ok = RunAStarOnMap(map_id, from, to, path); // 等待构建，进行可视区图 A* + 偏移微调重试
        if (!ok || path.size() < 2) {
            if (ok && path.size() == 1) return 0.f; // src==dst
            if (out_unreachable) *out_unreachable = true;
            return INF;
        }
        float len = 0.f;
        for (size_t i = 0; i + 1 < path.size(); i++) {
            const float dx = path[i + 1].x - path[i].x, dy = path[i + 1].y - path[i].y;
            len += std::sqrt(dx * dx + dy * dy);
        }
        return len;
    }

    uint64_t MeshKeyFor(GW::Constants::MapID map_id)
    {
        const uint32_t fh = GetMapFileId(map_id);
        if (fh) return (uint64_t)fh;
        return (1ull << 40) | (uint32_t)map_id;
    }

    struct PGNode {
        GW::Constants::MapID map_id;
        GW::Vec2f pos;        // 游戏坐标
        GW::Vec2f wm;         // 世界地图坐标
        uint64_t mesh;        // 地图内分组键
        GW::Continent continent;
    };
    struct PGEdge {
        int to;
        bool intra;                 // true = 地图内行走（可细化），false = 传送门穿越
        float est;                  // 乐观权重（地图内为欧几里得，穿越为基准）
        bool evaluated;             // 仅地图内
        float real;                 // 仅地图内，评估后有效
        GW::Constants::MapID owner; // 地图内：运行行走的地图（穿越/自由边为 None）
    };
    struct PortalGraph {
        std::vector<PGNode> nodes;
        std::vector<std::vector<PGEdge>> adj;
        std::unordered_map<uint64_t, std::vector<int>> by_mesh;
        int start_i = -1, goal_i = -1;
    };

    // 为一次查询构建传送门图（包含定位的起点/终点端点）。每次调用重新构建 —
    // 拓扑很小（约 400 条连接），因此缓存不值得起终点簿记开销。
    void BuildPortalGraph(PortalGraph& g, GW::Constants::MapID src, const GW::GamePos* start_pos,
                          GW::Constants::MapID dst, const GW::GamePos* goal_pos,
                          bool same_continent, GW::Continent route_continent)
    {
        std::map<std::tuple<uint32_t, int, int>, int> node_index;
        constexpr float Q = 32.f; // 在 32 游戏单位内去重传送门 → 一个节点
        auto add_node = [&](GW::Constants::MapID mid, const GW::Vec2f& pos, const GW::Vec2f& wm) -> int {
            auto key = std::make_tuple((uint32_t)mid, (int)std::lround(pos.x / Q), (int)std::lround(pos.y / Q));
            if (auto f = node_index.find(key); f != node_index.end()) return f->second;
            const int idx = (int)g.nodes.size();
            const auto* area = GW::Map::GetMapInfo(mid);
            g.nodes.push_back({mid, pos, wm, MeshKeyFor(mid), area ? area->continent : GW::Continent::Kryta});
            g.adj.emplace_back();
            node_index[key] = idx;
            g.by_mesh[g.nodes[idx].mesh].push_back(idx);
            return idx;
        };
        auto add_edge = [&](int a, int b, bool intra, float est, GW::Constants::MapID owner) {
            g.adj[a].push_back({b, intra, est, false, 0.f, owner});
        };

        // 来自 JSON 连接的穿越边（并行边 = 每地图对多个传送门）。
        for (const auto& c : portal_connections.GetAll()) {
            if (c.from_type == Pathing::ConnectionType::Disabled || c.to_type == Pathing::ConnectionType::Disabled) continue;
            if (c.from_map == GW::Constants::MapID::None || c.to_map == GW::Constants::MapID::None) continue;
            // 同大陆剪枝：船/渡轮跳转不能属于同大陆路径。
            if (same_continent) {
                const auto* fa_area = GW::Map::GetMapInfo(c.from_map);
                const auto* fb_area = GW::Map::GetMapInfo(c.to_map);
                if (fa_area && fa_area->continent != route_continent) continue;
                if (fb_area && fb_area->continent != route_continent) continue;
            }
            const int a = add_node(c.from_map, c.from_pos, c.from_wm_pos);
            const int b = add_node(c.to_map, c.to_pos, c.to_wm_pos);
            const float mult = std::min(Pathing::PortalConnections::GetCostMultiplier(c.from_type),
                                        Pathing::PortalConnections::GetCostMultiplier(c.to_type));
            const float cross = CROSSING_COST * mult;
            if (!blacklisted_edges.contains(EdgeKey(c.from_map, c.to_map)))
                add_edge(a, b, false, cross, GW::Constants::MapID::None);
            if (!c.IsOneWay() && !blacklisted_edges.contains(EdgeKey(c.to_map, c.from_map)))
                add_edge(b, a, false, cross, GW::Constants::MapID::None);
        }

        // 起点 / 终点端点。定位端点通过可细化的地图内边加入其网格；空端点（拓扑查询）
        // 通过自由、不可细化的边加入。
        g.start_i = add_node(src, start_pos ? GW::Vec2f{start_pos->x, start_pos->y} : GW::Vec2f{}, {});
        g.goal_i = add_node(dst, goal_pos ? GW::Vec2f{goal_pos->x, goal_pos->y} : GW::Vec2f{}, {});

        // 地图内边：连接共享网格的每对端点。
        for (auto& [mesh, idxs] : g.by_mesh) {
            for (size_t i = 0; i < idxs.size(); i++) {
                for (size_t j = i + 1; j < idxs.size(); j++) {
                    const int ai = idxs[i], bi = idxs[j];
                    const auto& na = g.nodes[ai];
                    const auto& nb = g.nodes[bi];
                    const bool a_positionless = (ai == g.start_i && !start_pos) || (ai == g.goal_i && !goal_pos);
                    const bool b_positionless = (bi == g.start_i && !start_pos) || (bi == g.goal_i && !goal_pos);
                    if (a_positionless || b_positionless) {
                        add_edge(ai, bi, false, 0.f, GW::Constants::MapID::None); // 自由，不可细化
                        add_edge(bi, ai, false, 0.f, GW::Constants::MapID::None);
                        continue;
                    }
                    const float est = GW::GetDistance(na.pos, nb.pos);
                    add_edge(ai, bi, true, est, na.map_id);
                    add_edge(bi, ai, true, est, nb.map_id);
                }
            }
        }
    }

    // 加载边的地图并将其乐观估计替换为真实的导航网格行走距离。
    void RefineIntraEdge(PortalGraph& g, int from, int edge_idx)
    {
        PGEdge& ed = g.adj[from][edge_idx];
        const auto& a = g.nodes[from];
        const auto& b = g.nodes[ed.to];
        bool unreachable = false, data_miss = false;
        float real = WalkDistanceOnMap(ed.owner, {a.pos.x, a.pos.y, 0}, {b.pos.x, b.pos.y, 0}, &unreachable, &data_miss);
        if (std::isinf(real)) {
            // 真正断开 → 带惩罚的有限最后手段（已创作的传送门仍然获胜，但路径可以解析）。
            // 数据质量问题 / 无网格 → 普通欧几里得（不惩罚我们自己的缺失）。
            real = unreachable ? ed.est * portal_unreachable_penalty : ed.est;
        }
        if (real < ed.est) real = ed.est; // 从不低于可接纳下界
        ed.evaluated = true;
        ed.real = real;
        // 镜像到反向边（行走距离在同一网格上对称）以节省一次评估。
        for (auto& re : g.adj[ed.to]) {
            if (re.to == from && re.intra && !re.evaluated) { re.evaluated = true; re.real = real; break; }
        }
    }

    // 在当前边权重上运行 Dijkstra；out_edges = 从 s 到 t 的有序 (node_u, edge_index)。
    bool DijkstraPath(const PortalGraph& g, int s, int t, std::vector<std::pair<int, int>>& out_edges)
    {
        const int n = (int)g.nodes.size();
        const float INF = std::numeric_limits<float>::infinity();
        std::vector<float> dist(n, INF);
        std::vector<int> from(n, -1), fedge(n, -1);
        using QN = std::pair<float, int>; // (dist, node)
        std::priority_queue<QN, std::vector<QN>, std::greater<QN>> pq;
        dist[s] = 0.f;
        pq.push({0.f, s});
        while (!pq.empty()) {
            auto [d, u] = pq.top();
            pq.pop();
            if (d > dist[u]) continue;
            if (u == t) break;
            for (int ei = 0; ei < (int)g.adj[u].size(); ei++) {
                const PGEdge& e = g.adj[u][ei];
                const float w = e.intra ? (e.evaluated ? e.real : e.est) : e.est;
                if (std::isinf(w)) continue;
                const float nd = d + w;
                if (nd < dist[e.to]) {
                    dist[e.to] = nd;
                    from[e.to] = u;
                    fedge[e.to] = ei;
                    pq.push({nd, e.to});
                }
            }
        }
        if (std::isinf(dist[t])) return false;
        std::vector<std::pair<int, int>> rev;
        for (int v = t; v != s; v = from[v]) {
            if (from[v] < 0) return false;
            rev.push_back({from[v], fedge[v]});
        }
        std::ranges::reverse(rev);
        out_edges = std::move(rev);
        return true;
    }

    std::vector<GW::Constants::MapID> FindMapRoute(GW::Constants::MapID src, GW::Constants::MapID dst, const GW::GamePos* start_pos = nullptr, const GW::GamePos* goal_pos = nullptr)
    {
        if (src == dst) return {src, dst};

        const auto* src_area = GW::Map::GetMapInfo(src);
        const auto* dst_area = GW::Map::GetMapInfo(dst);
        const bool same_continent = src_area && dst_area && src_area->continent == dst_area->continent;
        const GW::Continent route_continent = src_area ? src_area->continent : GW::Continent::Kryta;

        PortalGraph g;
        BuildPortalGraph(g, src, start_pos, dst, goal_pos, same_continent, route_continent);
        if (g.start_i < 0 || g.goal_i < 0) return {};

        {
            int goal_indeg = 0;
            for (const auto& al : g.adj)
                for (const auto& e : al)
                    if (e.to == g.goal_i) goal_indeg++;
            PATH_LOG_INFO("[LazySP] 构建 src=%d(网格=0x%llX 出度=%d) dst=%d(网格=0x%llX 入度=%d) 节点=%d",
                (int)src, (unsigned long long)g.nodes[g.start_i].mesh, (int)g.adj[g.start_i].size(),
                (int)dst, (unsigned long long)g.nodes[g.goal_i].mesh, goal_indeg, (int)g.nodes.size());
        }

        for (int iter = 0; iter < MAX_LAZYSP_ITERS && !pending_terminate; iter++) {
            std::vector<std::pair<int, int>> path_edges;
            if (!DijkstraPath(g, g.start_i, g.goal_i, path_edges)) {
                PATH_LOG_INFO("[LazySP] src=%d dst=%d 失败：无路径（迭代 %d）", (int)src, (int)dst, iter);
                return {};
            }
            // 细化路径上第一个未评估的地图内边。
            int refine_from = -1, refine_ei = -1;
            for (const auto& [u, ei] : path_edges) {
                const PGEdge& e = g.adj[u][ei];
                if (e.intra && !e.evaluated) { refine_from = u; refine_ei = ei; break; }
            }
            if (refine_from < 0) {
                // 所有地图内边都是真实的 → 最优。每次传送门穿越发射一个 MapID。
                std::vector<GW::Constants::MapID> route{src};
                for (const auto& [u, ei] : path_edges) {
                    const PGEdge& e = g.adj[u][ei];
                    if (!e.intra) {
                        const auto mid = g.nodes[e.to].map_id;
                        if (route.empty() || route.back() != mid) route.push_back(mid);
                    }
                }
                if (route.empty() || route.back() != dst) route.push_back(dst);
                PATH_LOG_INFO("[LazySP] src=%d dst=%d 成功：%d 个地图（%d 次迭代）", (int)src, (int)dst, (int)route.size(), iter);
                return route;
            }
            RefineIntraEdge(g, refine_from, refine_ei);
        }
        PATH_LOG_INFO("[LazySP] src=%d dst=%d 失败：迭代上限", (int)src, (int)dst);
        return {};
    }

    void UpdateGraphEdgeLines()
    {
        ClearGraphEdgeLines();
        if (!draw_graph_edges) return;
        BuildMapGraph();

        auto cur_map = GW::Map::GetMapID();
        const auto cur_info = GW::Map::GetMapInfo();
        if (!cur_info) return;
        auto cur_continent = cur_info->continent;

        std::set<uint64_t> drawn_edges;
        for (size_t i = 0; i < map_graph_nodes.size(); i++) {
            const auto& node = map_graph_nodes[i];
            if (node.continent != cur_continent) continue;
            ImVec2 ca = node.wm_bounds.GetCenter();

            for (auto neighbor_id : GetAdjacentMaps(node.map_id)) {
                uint64_t edge_key = std::min((uint32_t)node.map_id, (uint32_t)neighbor_id);
                edge_key |= static_cast<uint64_t>(std::max((uint32_t)node.map_id, (uint32_t)neighbor_id)) << 32;
                if (drawn_edges.contains(edge_key)) continue;
                drawn_edges.insert(edge_key);

                for (size_t ni = 0; ni < map_graph_nodes.size(); ni++) {
                    if (map_graph_nodes[ni].map_id != neighbor_id) continue;
                    ImVec2 cb = map_graph_nodes[ni].wm_bounds.GetCenter();

                    GW::GamePos ga, gb;
                    GW::Vec2f wm_a = {ca.x, ca.y}, wm_b = {cb.x, cb.y};
                    if (!WorldMapWidget::WorldMapToGamePos(wm_a, ga)) break;
                    if (!WorldMapWidget::WorldMapToGamePos(wm_b, gb)) break;

                    auto* line = Minimap::Instance().custom_renderer.AddCustomLine(ga, gb);
                    line->map = cur_map;
                    line->color = 0x40FFFF00; // 黄色，透明
                    line->draw_on_mission_map = true;
                    line->draw_on_minimap = false;
                    line->created_by_toolbox = true;
                    graph_edge_lines.push_back(line);
                    break;
                }
            }
        }
    }

    const CachedMapInfo* GetCachedMapInfo(GW::Constants::MapID map_id)
    {
        // 按 MapID 直接匹配
        for (const auto& [hash, info] : cached_map_info) {
            if (info.map_id == map_id) return &info;
        }
        // 回退：按共享 file_hash 匹配（前哨站/可探索对共享相同数据）
        uint32_t fh = GetMapFileId(map_id);
        if (fh) {
            for (const auto& [hash, info] : cached_map_info) {
                if (GetMapFileId(info.map_id) == fh) return &info;
            }
        }
        return nullptr;
    }

    // 判断前哨站/城市类型地图（非可探索）；它们在共享边界内的小足迹意味着继承的传送门可能相距甚远。
    bool IsOutpostMap(GW::Constants::MapID map_id)
    {
        const auto* area = GW::Map::GetMapInfo(map_id);
        if (!area) return false;
        switch (area->type) {
            case GW::RegionType::Outpost:
            case GW::RegionType::MissionOutpost:
            case GW::RegionType::City:
            case GW::RegionType::HeroBattleOutpost:
            case GW::RegionType::CooperativeMission:
            case GW::RegionType::CompetitiveMission:
                return true;
            default:
                return false;
        }
    }

    // 从前哨站图标到传送门的最大世界地图距离，以认为传送门有效（约 24000 游戏单位）
    constexpr float OUTPOST_PORTAL_MAX_WM_DIST = 250.f;

    // 如果 portal_wm 距前哨站图标太远以至于无法到达，则返回 true。
    // 仅当 map_id 是前哨站时过滤。否则返回 false。
    bool IsPortalTooFarFromOutpost(GW::Constants::MapID map_id, const GW::Vec2f& portal_wm)
    {
        if (!IsOutpostMap(map_id)) return false;
        const auto* area = GW::Map::GetMapInfo(map_id);
        if (!area || (!area->x && !area->y)) return false; // 无图标位置
        float dx = portal_wm.x - (float)area->x;
        float dy = portal_wm.y - (float)area->y;
        return (dx * dx + dy * dy) > (OUTPOST_PORTAL_MAX_WM_DIST * OUTPOST_PORTAL_MAX_WM_DIST);
    }


    // 查找两个相邻地图之间的所有传送门对，限制在重叠区域内。
    std::vector<PortalPair> FindPortalPairs(GW::Constants::MapID map_a, GW::Constants::MapID map_b)
    {
        std::vector<PortalPair> pairs;
        const auto* info_a = GetCachedMapInfo(map_a);
        const auto* info_b = GetCachedMapInfo(map_b);
        if (!info_a || !info_b) return pairs;

        // 计算世界地图坐标中的重叠区域
        auto* area_a = GW::Map::GetMapInfo(map_a);
        auto* area_b = GW::Map::GetMapInfo(map_b);
        if (!area_a || !area_b) return pairs;
        ImRect wm_a, wm_b;
        if (!GW::Map::GetMapWorldMapBounds(area_a, &wm_a)) return pairs;
        if (!GW::Map::GetMapWorldMapBounds(area_b, &wm_b)) return pairs;

        constexpr float overlap_margin = 50.f; // 稍微扩大重叠区域
        ImRect overlap(std::max(wm_a.Min.x, wm_b.Min.x) - overlap_margin, std::max(wm_a.Min.y, wm_b.Min.y) - overlap_margin, std::min(wm_a.Max.x, wm_b.Max.x) + overlap_margin, std::min(wm_a.Max.y, wm_b.Max.y) + overlap_margin);

        constexpr float max_pair_dist = 100.f; // 世界地图单位

        for (const auto& pa : info_a->portal_props) {
            GW::Vec2f wm_pa;
            if (!WorldMapWidget::GamePosToWorldMap({pa.pos.x, pa.pos.y, 0}, wm_pa, map_a)) continue;
            if (!overlap.Contains({wm_pa.x, wm_pa.y})) continue;
            if (IsPortalTooFarFromOutpost(map_a, wm_pa)) continue;

            for (const auto& pb : info_b->portal_props) {
                GW::Vec2f wm_pb;
                if (!WorldMapWidget::GamePosToWorldMap({pb.pos.x, pb.pos.y, 0}, wm_pb, map_b)) continue;
                if (!overlap.Contains({wm_pb.x, wm_pb.y})) continue;
                if (IsPortalTooFarFromOutpost(map_b, wm_pb)) continue;

                float dx = wm_pa.x - wm_pb.x, dy = wm_pa.y - wm_pb.y;
                float dist = sqrtf(dx * dx + dy * dy);
                if (dist < max_pair_dist) {
                    pairs.push_back({{pa.pos.x, pa.pos.y, 0}, {pb.pos.x, pb.pos.y, 0}, {(wm_pa.x + wm_pb.x) * 0.5f, (wm_pa.y + wm_pb.y) * 0.5f}, dist});
                }
            }
        }
        return pairs;
    }

    // 通过接近起点/终点来为段选择最佳传送门对。优先级：手动连接 > 自动对 > 边界回退。
    bool FindBestPortalPair(GW::Constants::MapID map_a, GW::Constants::MapID map_b, const GW::Vec2f& hint_wm_pos, GW::GamePos& portal_a_out, GW::GamePos& portal_b_out, const GW::GamePos* start_game = nullptr, const GW::GamePos* goal_game = nullptr)
    {
        // 收集候选对（手动 + 自动），然后在已知起点/终点时按游戏坐标距离评分，否则按世界地图提示评分。
        struct Candidate {
            GW::GamePos pos_a, pos_b;
            GW::Vec2f wm_pos; // 用于提示评分的世界地图位置
            const char* source;
        };
        std::vector<Candidate> candidates;

        // 1) 手动传送门连接
        {
            uint32_t fh_a = GetMapFileId(map_a);
            uint32_t fh_b = GetMapFileId(map_b);
            auto maps_match = [](GW::Constants::MapID mid, GW::Constants::MapID target, uint32_t target_fh) {
                if (mid == target) return true;
                return target_fh && GetMapFileId(mid) == target_fh;
            };
            for (const auto& conn : portal_connections.GetAll()) {
                if (conn.from_type == Pathing::ConnectionType::Disabled || conn.to_type == Pathing::ConnectionType::Disabled) continue;
                bool fwd = (maps_match(conn.from_map, map_a, fh_a) && maps_match(conn.to_map, map_b, fh_b));
                bool rev = !conn.IsOneWay() && (maps_match(conn.from_map, map_b, fh_b) && maps_match(conn.to_map, map_a, fh_a));
                if (!fwd && !rev) continue;
                Candidate c;
                c.pos_a = fwd ? GW::GamePos{conn.from_pos.x, conn.from_pos.y, 0} : GW::GamePos{conn.to_pos.x, conn.to_pos.y, 0};
                c.pos_b = fwd ? GW::GamePos{conn.to_pos.x, conn.to_pos.y, 0} : GW::GamePos{conn.from_pos.x, conn.from_pos.y, 0};
                c.wm_pos = fwd ? conn.from_wm_pos : conn.to_wm_pos;
                c.source = "手动";
                candidates.push_back(c);
            }
        }

        // 2) 自动传送门属性对（仅当未找到手动连接时）
        if (candidates.empty()) {
            auto pairs = FindPortalPairs(map_a, map_b);
            for (const auto& p : pairs) {
                candidates.push_back({p.pos_a, p.pos_b, p.wm_mid, "传送门"});
            }
        }

        if (!candidates.empty()) {
            if (start_game || goal_game) {
                // 按实际 AStar 成本评分；对失败施加严厉惩罚（不可到达的传送门绝不能击败可到达的）。
                constexpr float FAIL_PENALTY = 1e9f;
                float best_score = std::numeric_limits<float>::infinity();
                const Candidate* best = nullptr;
                for (const auto& c : candidates) {
                    float cost_a = 0.f, cost_b = 0.f;
                    const char* method_a = "无";
                    const char* method_b = "无";

                    if (start_game) {
                        auto* mp = (map_a == GW::Map::GetMapID()) ? GetMilepathForCurrentMap() : GetMilepathForMap(map_a);
                        if (mp && mp->ready()) {
                            auto astr = Pathing::AStar(mp);
                            auto res = astr.Search(*start_game, c.pos_a);
                            if (res == Pathing::Error::OK && astr.m_path.ready()) {
                                cost_a = astr.m_path.cost();
                                method_a = "astar";
                            }
                            else {
                                cost_a = FAIL_PENALTY + GW::GetDistance(*start_game, c.pos_a);
                                method_a = "失败";
                            }
                        }
                        else {
                            cost_a = GW::GetDistance(*start_game, c.pos_a);
                            method_a = "直线(无网格)";
                        }
                    }
                    if (goal_game) {
                        auto* mp = (map_b == GW::Map::GetMapID()) ? GetMilepathForCurrentMap() : GetMilepathForMap(map_b);
                        if (mp && mp->ready()) {
                            auto astr = Pathing::AStar(mp);
                            auto res = astr.Search(c.pos_b, *goal_game);
                            if (res == Pathing::Error::OK && astr.m_path.ready()) {
                                cost_b = astr.m_path.cost();
                                method_b = "astar";
                            }
                            else {
                                cost_b = FAIL_PENALTY + GW::GetDistance(c.pos_b, *goal_game);
                                method_b = "失败";
                            }
                        }
                        else {
                            cost_b = GW::GetDistance(c.pos_b, *goal_game);
                            method_b = "直线(无网格)";
                        }
                    }
                    float cost = cost_a + cost_b;
                    PATH_LOG_INFO("  [%s] a=(%.0f,%.0f) b=(%.0f,%.0f) cost_a=%.0f(%s) cost_b=%.0f(%s) 总计=%.0f", c.source, c.pos_a.x, c.pos_a.y, c.pos_b.x, c.pos_b.y, cost_a, method_a, cost_b, method_b, cost);
                    if (cost < best_score) {
                        best_score = cost;
                        best = &c;
                    }
                }
                if (best) {
                    portal_a_out = best->pos_a;
                    portal_b_out = best->pos_b;
                    PATH_LOG_INFO("%s 胜出：地图 %d (%.0f,%.0f) -- 地图 %d (%.0f,%.0f) [%d 个候选，cost=%.0f]", best->source, (int)map_a, portal_a_out.x, portal_a_out.y, (int)map_b, portal_b_out.x, portal_b_out.y, (int)candidates.size(), best_score);
                    return true;
                }
            }
            else {
                // 世界地图提示评分（当没有游戏位置可用时的回退）
                float best_score = std::numeric_limits<float>::infinity();
                const Candidate* best = nullptr;
                for (const auto& c : candidates) {
                    float dx = c.wm_pos.x - hint_wm_pos.x, dy = c.wm_pos.y - hint_wm_pos.y;
                    float score = dx * dx + dy * dy;
                    if (score < best_score) {
                        best_score = score;
                        best = &c;
                    }
                }
                if (best) {
                    portal_a_out = best->pos_a;
                    portal_b_out = best->pos_b;
                    PATH_LOG_INFO(
                        "%s 连接：地图 %d (%.0f,%.0f) -- 地图 %d (%.0f,%.0f) [%d 个候选，wm_score=%.0f]", best->source, (int)map_a, portal_a_out.x, portal_a_out.y, (int)map_b, portal_b_out.x, portal_b_out.y, (int)candidates.size(), best_score
                    );
                    return true;
                }
            }
        }

        // 3) 回退：使用世界地图边界重叠中心作为过渡点
        auto* area_a = GW::Map::GetMapInfo(map_a);
        auto* area_b = GW::Map::GetMapInfo(map_b);
        if (!area_a || !area_b) return false;

        ImRect wm_a, wm_b;
        if (!GW::Map::GetMapWorldMapBounds(area_a, &wm_a)) return false;
        if (!GW::Map::GetMapWorldMapBounds(area_b, &wm_b)) return false;

        // 世界地图坐标中的重叠中心
        ImRect overlap(std::max(wm_a.Min.x, wm_b.Min.x), std::max(wm_a.Min.y, wm_b.Min.y), std::min(wm_a.Max.x, wm_b.Max.x), std::min(wm_a.Max.y, wm_b.Max.y));
        GW::Vec2f wm_center = {overlap.GetCenter().x, overlap.GetCenter().y};

        // 转换到每个地图的游戏坐标（如果未知则回退到当前地图）
        if (!WorldMapWidget::WorldMapToGamePos(wm_center, portal_a_out, map_a)) WorldMapWidget::WorldMapToGamePos(wm_center, portal_a_out);
        if (!WorldMapWidget::WorldMapToGamePos(wm_center, portal_b_out, map_b)) WorldMapWidget::WorldMapToGamePos(wm_center, portal_b_out);

        PATH_LOG_INFO("边界回退：地图 %d (%.0f,%.0f) -- 地图 %d (%.0f,%.0f) wm=(%.0f,%.0f)", (int)map_a, portal_a_out.x, portal_a_out.y, (int)map_b, portal_b_out.x, portal_b_out.y, wm_center.x, wm_center.y);
        return true;
    }

    // 在特定地图上运行 AStar，在两个游戏坐标位置之间
    // 返回该地图坐标系中的路径点
    bool RunAStarOnMap(GW::Constants::MapID map_id, const GW::GamePos& from, const GW::GamePos& to, std::vector<GW::GamePos>& path_out)
    {
        Pathing::MilePath* mp = nullptr;
        if (map_id == GW::Map::GetMapID()) {
            mp = GetMilepathForCurrentMap();
        }
        else {
            mp = GetMilepathForMap(map_id);
        }
        if (!mp) {
            PATH_LOG_ERROR("[AStar] 地图 %d 失败：GetMilepath 返回空", (int)map_id);
            return false;
        }

        while (!mp->ready() && !pending_terminate) {
            Sleep(100);
        }
        if (pending_terminate) {
            PATH_LOG_INFO("[AStar] 地图 %d 中止：pending_terminate", (int)map_id);
            return false;
        }
        if (mp->build_failed()) {
            PATH_LOG_ERROR("[AStar] 地图 %d 失败：MilePath 构建内存不足", (int)map_id);
            return false;
        }

        auto astr = Pathing::AStar(mp);
        auto res = astr.Search(from, to);
        if (res == Pathing::Error::OK && astr.m_path.ready()) {
            path_out = astr.m_path.points();
            PATH_LOG_INFO("[AStar] 地图 %d 成功，cost=%.0f，点=%d", (int)map_id, astr.m_path.cost(), (int)path_out.size());
            return true;
        }

        PATH_LOG_INFO("[AStar] map %d: no path (points not walk-connected)", (int)map_id);
        return false;
    }

    // 多地图寻路：通过传送门对连接运行 AStar 段
    void RecalculateMultiMapPath(const std::vector<GW::Constants::MapID>& route, const GW::GamePos& start, const GW::GamePos& goal)
    {
        ClearPathLines();
        ClearPortalPairLines();
        delete astar;
        astar = nullptr;

        // 为工作线程捕获副本
        auto route_copy = route;
        auto start_copy = start;
        auto goal_copy = goal;
        auto start_wm = path_from_world;
        auto goal_wm = path_to_world;
        auto cur_map = GW::Map::GetMapID();

        struct PortalPairDraw {
            GW::GamePos a, b;
            GW::Constants::MapID map_a, map_b;
        };

        Resources::EnqueueWorkerTask([route_copy, start_copy, goal_copy, start_wm, goal_wm, cur_map] {
            RouteJobScope job_scope; // 在持有 MilePath* 时延迟淘汰
            PathCalcScope calc_scope; // 为世界地图指示器标记路径计算
            std::vector<GW::GamePos> full_path;
            std::vector<HiddenPathSegment> hidden_segments;
            std::vector<PortalPairDraw> portal_draws;

            // 追踪最后使用的传送门的世界地图位置以进行链式传递
            GW::Vec2f last_portal_wm = start_wm;
            GW::GamePos last_seg_end = start_copy; // 最后已知的游戏位置（用于传送门评分）

            for (size_t seg = 0; seg < route_copy.size(); seg++) {
                auto map_id = route_copy[seg];
                GW::GamePos seg_from, seg_to;

                if (seg == 0) {
                    seg_from = start_copy;
                }
                else {
                    // 来自上一地图的入口传送门 — 按距最后位置的距离评分
                    GW::GamePos prev_exit, this_entry;
                    const GW::GamePos* gg = (seg == route_copy.size() - 1) ? &goal_copy : nullptr;
                    if (!FindBestPortalPair(route_copy[seg - 1], map_id, last_portal_wm, prev_exit, this_entry, &last_seg_end, gg)) {
                        PATH_LOG_ERROR("地图 %d 和 %d 之间无传送门对", (int)route_copy[seg - 1], (int)map_id);
                        return;
                    }
                    seg_from = this_entry;
                    portal_draws.push_back({prev_exit, this_entry, route_copy[seg - 1], map_id});
                    WorldMapWidget::GamePosToWorldMap(this_entry, last_portal_wm, map_id);
                }

                if (seg == route_copy.size() - 1) {
                    seg_to = goal_copy;
                }
                else {
                    // 到下一地图的出口传送门 — 按距 seg_from + 到目标的距离评分
                    GW::GamePos this_exit, next_entry;
                    GW::Vec2f hint = last_portal_wm;
                    const GW::GamePos* gg = (seg == route_copy.size() - 2) ? &goal_copy : nullptr;
                    if (!FindBestPortalPair(map_id, route_copy[seg + 1], hint, this_exit, next_entry, &seg_from, gg)) {
                        PATH_LOG_ERROR("地图 %d 和 %d 之间无传送门对", (int)map_id, (int)route_copy[seg + 1]);
                        return;
                    }
                    seg_to = this_exit;
                    WorldMapWidget::GamePosToWorldMap(this_exit, last_portal_wm, map_id);
                }

                PATH_LOG_INFO("段 %d：地图 %d from=(%.0f,%.0f) to=(%.0f,%.0f)", (int)seg, (int)map_id, seg_from.x, seg_from.y, seg_to.x, seg_to.y);

                std::vector<GW::GamePos> seg_path;
                if (!RunAStarOnMap(map_id, seg_from, seg_to, seg_path)) {
                    PATH_LOG_ERROR("AStar 在段 %d（地图 %d）上失败", (int)seg, (int)map_id);
                    return;
                }

                // 为下一次传送门评分追踪最后位置
                last_seg_end = seg_to;

                // 隐藏入口和出口都是 no_draw 的中间段（地下地图）。从不隐藏第一/最后或当前地图。
                bool is_first = (seg == 0);
                bool is_last = (seg + 1 == route_copy.size());
                bool segment_hidden = !is_first && !is_last && map_id != cur_map && HasNoDrawConnection(route_copy[seg - 1], map_id) && HasNoDrawConnection(map_id, route_copy[seg + 1]);

                if (segment_hidden) {
                    // 用于地下渲染的原生坐标段；路径中断使世界地图线跳过间隙。
                    hidden_segments.push_back({seg_path, map_id});
                    if (!full_path.empty() && !IsPathBreak(full_path.back())) {
                        full_path.push_back({PATH_BREAK_VALUE, PATH_BREAK_VALUE, 0});
                    }
                    continue;
                }

                // 段间路径中断：跨地图传送门跳转不是可行走的，会绘制离屏连接线。
                if (!full_path.empty() && !IsPathBreak(full_path.back())) {
                    full_path.push_back({PATH_BREAK_VALUE, PATH_BREAK_VALUE, 0});
                }

                auto converted = ConvertPathToCurrentMap(seg_path, map_id);
                full_path.insert(full_path.end(), converted.begin(), converted.end());
            }

            PATH_LOG_INFO("多地图路径：%d 个总点，跨越 %d 个地图（%d 个地下隐藏段）", (int)full_path.size(), (int)route_copy.size(), (int)hidden_segments.size());
            Resources::EnqueueMainTask([full_path, hidden_segments, cur_map] {
                DrawPathAsLines(full_path, cur_map);
                AddHiddenUndergroundSegmentLines(hidden_segments);
            });
        });
    }

    // 阻塞式纯计算核心（调用者拥有工作线程 + RouteJobScope）：构建路径的世界坐标点（地图间 PATH_BREAK）
    // 和地下隐藏段，在 AStar 失败时使用边黑名单重试。若无路径则返回 false。
    bool BuildCrossMapRoute(GW::Constants::MapID from_map, GW::Constants::MapID to_map, const GW::GamePos& start, const GW::GamePos& goal, const GW::Vec2f& start_wm, std::vector<GW::Vec2f>& out_points, std::vector<HiddenPathSegment>& out_hidden)
    {
        const auto cur_map = GW::Map::GetMapID();
        constexpr int max_retries = 3;
        for (int attempt = 0; attempt <= max_retries; attempt++) {
            auto route = FindMapRoute(from_map, to_map, &start, &goal);
            if (route.empty()) {
                // 对于不可达标记是预期的 — 将其保持在聊天之外（调用者回退）。
                PATH_LOG_INFO("从地图 %d 到地图 %d 未找到地图路径", (int)from_map, (int)to_map);
                blacklisted_edges.clear();
                return false;
            }
            std::string route_str;
            for (auto m : route) {
                route_str += std::to_string((int)m) + " ";
            }
            PATH_LOG_INFO("地图路径（尝试 %d）%d->%d：%s（%d 个地图）", attempt, (int)from_map, (int)to_map, route_str.c_str(), (int)route.size());

            for (auto m : route) {
                if (m != cur_map) LoadMapFromDAT(m);
            }

            // 点累积为世界地图坐标（公共跨地图空间）— 绝不投影到另一个地图的游戏空间。
            std::vector<GW::Vec2f> full_path;
            std::vector<HiddenPathSegment> hidden_segments;
            GW::Vec2f last_portal_wm = start_wm;
            GW::GamePos last_seg_end = start;
            bool failed = false;

            for (size_t seg = 0; seg < route.size(); seg++) {
                auto map_id = route[seg];
                GW::GamePos seg_from, seg_to;

                if (seg == 0) {
                    seg_from = start;
                }
                else {
                    GW::GamePos prev_exit, this_entry;
                    const GW::GamePos* gg = (seg == route.size() - 1) ? &goal : nullptr;
                    if (!FindBestPortalPair(route[seg - 1], map_id, last_portal_wm, prev_exit, this_entry, &last_seg_end, gg)) {
                        PATH_LOG_ERROR("地图 %d 和 %d 之间无传送门对", (int)route[seg - 1], (int)map_id);
                        blacklisted_edges.clear();
                        return false;
                    }
                    seg_from = this_entry;
                    WorldMapWidget::GamePosToWorldMap(this_entry, last_portal_wm, map_id);
                }

                if (seg == route.size() - 1) {
                    seg_to = goal;
                }
                else {
                    GW::GamePos this_exit, next_entry;
                    const GW::GamePos* gg = (seg == route.size() - 2) ? &goal : nullptr;
                    if (!FindBestPortalPair(map_id, route[seg + 1], last_portal_wm, this_exit, next_entry, &seg_from, gg)) {
                        PATH_LOG_ERROR("地图 %d 和 %d 之间无传送门对", (int)map_id, (int)route[seg + 1]);
                        blacklisted_edges.clear();
                        return false;
                    }
                    seg_to = this_exit;
                    WorldMapWidget::GamePosToWorldMap(this_exit, last_portal_wm, map_id);
                }

                PATH_LOG_INFO("段 %d：地图 %d from=(%.0f,%.0f) to=(%.0f,%.0f)", (int)seg, (int)map_id, seg_from.x, seg_from.y, seg_to.x, seg_to.y);

                std::vector<GW::GamePos> seg_path;
                if (!RunAStarOnMap(map_id, seg_from, seg_to, seg_path)) {
                    // 无 DAT 数据的过渡地图（地下/实例）：如果入口和出口连接都存在，则直线行走。
                    bool has_entry = seg == 0 || HasPortalConnectionBetween(route[seg - 1], map_id);
                    bool has_exit = seg + 1 >= route.size() || HasPortalConnectionBetween(map_id, route[seg + 1]);
                    auto map_has_pathing_data = [&](GW::Constants::MapID m) -> bool {
                        Pathing::MilePath* mp = (m == GW::Map::GetMapID()) ? GetMilepathForCurrentMap() : GetMilepathForMap(m);
                        if (!mp || !mp->ready() || mp->build_failed()) return false;
                        const auto* d = mp->GetMapData();
                        return d && d->IsValid();
                    };
                    if (has_entry && has_exit && !map_has_pathing_data(map_id)) {
                        PATH_LOG_INFO("地图 %d 上 AStar 不可用（无寻路数据），使用直线过渡", (int)map_id);
                        seg_path = {seg_from, seg_to};
                    }
                    else {
                        // 黑名单此地图的入口/出口边并重试。解析为图代表 — Dijkstra 在图节点上工作。
                        auto resolve_graph = [](GW::Constants::MapID m) -> GW::Constants::MapID {
                            for (const auto& n : map_graph_nodes) {
                                if (n.map_id == m) return m;
                            }
                            uint32_t fh = GetMapFileId(m);
                            if (fh) {
                                for (const auto& n : map_graph_nodes) {
                                    if (n.file_hash == fh) return n.map_id;
                                }
                            }
                            return m;
                        };
                        auto map_g = resolve_graph(map_id);
                        if (seg > 0) {
                            auto prev_g = resolve_graph(route[seg - 1]);
                            PATH_LOG_INFO("AStar 在地图 %d 上失败，黑名单边 %d->%d（图 %d->%d）", (int)map_id, (int)route[seg - 1], (int)map_id, (int)prev_g, (int)map_g);
                            blacklisted_edges.insert(EdgeKey(prev_g, map_g));
                            blacklisted_edges.insert(EdgeKey(map_g, prev_g));
                        }
                        if (seg + 1 < route.size()) {
                            auto next_g = resolve_graph(route[seg + 1]);
                            blacklisted_edges.insert(EdgeKey(map_g, next_g));
                            blacklisted_edges.insert(EdgeKey(next_g, map_g));
                        }
                        failed = true;
                        break;
                    }
                }

                last_seg_end = seg_to;

                const bool is_first_seg = (seg == 0);
                const bool is_last_seg = (seg + 1 == route.size());
                const auto* seg_area = GW::Map::GetMapInfo(map_id);
                const int seg_owm = (seg_area && seg_area->GetIsOnWorldMap()) ? 1 : 0;
                const bool segment_hidden = !is_first_seg && !is_last_seg && map_id != cur_map && seg_owm == 0;

                if (segment_hidden) {
                    // 记录用于地下地形 / 任务地图渲染的原生坐标段。
                    hidden_segments.push_back({seg_path, map_id});
                    if (!full_path.empty() && !IsPathBreak(full_path.back())) {
                        full_path.push_back({PATH_BREAK_VALUE, PATH_BREAK_VALUE});
                    }
                    continue;
                }

                // 段间路径中断 — 跨地图传送门跳转在任何单一坐标空间中都不是可行走的。
                if (!full_path.empty() && !IsPathBreak(full_path.back())) {
                    full_path.push_back({PATH_BREAK_VALUE, PATH_BREAK_VALUE});
                }

                SegmentToWorld(seg_path, map_id, full_path);
            }

            if (!failed) {
                PATH_LOG_INFO("多地图路径：%d 个总点，跨越 %d 个地图（%d 个地下隐藏段）", (int)full_path.size(), (int)route.size(), (int)hidden_segments.size());
                out_points = std::move(full_path);
                out_hidden = std::move(hidden_segments);
                blacklisted_edges.clear();
                return true;
            }

            if (attempt == max_retries) {
                PATH_LOG_ERROR("从地图 %d 到地图 %d 的所有 %d 次路径尝试均失败", max_retries + 1, (int)from_map, (int)to_map);
            }
        }
        blacklisted_edges.clear();
        return false;
    }

    bool BuildCrossMapRouteResolvingDst(GW::Constants::MapID from_map, const GW::GamePos& start, const GW::Vec2f& start_wm, const GW::Vec2f& to_world, std::vector<GW::Vec2f>& out_points, std::vector<HiddenPathSegment>& out_hidden, GW::Constants::MapID* out_to_map = nullptr)
    {
        std::vector<GW::Constants::MapID> candidates;
        // 同地图偏好：如果目标落在 from_map 的边界内，则优先路由到那里（避免重叠绕行）。
        if (PathfindingWindow::IsWorldPosOnMap(to_world, from_map)) candidates.push_back(from_map);
        for (auto m : RankCandidateMapsForWorldPos(to_world, from_map)) {
            if (std::find(candidates.begin(), candidates.end(), m) == candidates.end()) candidates.push_back(m);
        }
        // 当排名未找到任何结果时，作为最后手段保留旧有的单次解析，以免行为退化。
        if (candidates.empty()) {
            const auto m = WorldMapWidget::GetMapIdForLocation(to_world);
            if (m != GW::Constants::MapID::None) candidates.push_back(m);
        }

        for (const auto to_map : candidates) {
            if (to_map == GW::Constants::MapID::None) continue;
            if (!GetCachedMapInfo(to_map)) LoadMapFromDAT(to_map);
            GW::GamePos goal;
            if (!WorldMapWidget::WorldMapToGamePos(to_world, goal, to_map)) continue;
            // 重叠接缝上的标记可能投影到该地图可玩边界外几单位 — 将其拉回以便 A* 可以寻路。
            goal = ClampGoalToMapBounds(goal, to_map);
            out_points.clear();
            out_hidden.clear();

            if (to_map == from_map) {
                std::vector<GW::GamePos> leg;
                if (RunAStarOnMap(from_map, start, goal, leg) && !leg.empty()) {
                    SegmentToWorld(leg, from_map, out_points);
                    if (out_to_map) *out_to_map = from_map;
                    PATH_LOG_INFO("将 dst wm=(%.0f,%.0f) 解析为 %d 上的直接同地图路径", to_world.x, to_world.y, (int)from_map);
                    return true;
                }
                continue;
            }

            if (BuildCrossMapRoute(from_map, to_map, start, goal, start_wm, out_points, out_hidden) && !out_points.empty()) {
                if (out_to_map) *out_to_map = to_map;
                PATH_LOG_INFO("将模糊 dst wm=(%.0f,%.0f) 解析为地图 %d（共 %d 个候选）", to_world.x, to_world.y, (int)to_map, (int)candidates.size());
                return true;
            }
        }
        return false;
    }

    void RecalculateMultiMapPath_WithRetry(GW::Constants::MapID from_map, GW::Constants::MapID to_map, const GW::GamePos& start, const GW::GamePos& goal, const GW::Vec2f& start_wm, const GW::Vec2f& to_world)
    {
        ClearPathLines();
        ClearPortalPairLines();
        delete astar;
        astar = nullptr;

        auto cur_map = GW::Map::GetMapID();

        Resources::EnqueueWorkerTask([from_map, to_map, start, goal, start_wm, to_world, cur_map] {
            std::lock_guard route_lock(route_mutex); // 一次一个构建；共享缓存 + 梯形 pf 暂存区（节点/prioq）不是并发安全的
            RouteJobScope job_scope; // 在持有 MilePath* 时延迟淘汰
            PathCalcScope calc_scope; // 为世界地图指示器标记路径计算
            std::vector<GW::Vec2f> full_path;
            std::vector<HiddenPathSegment> hidden_segments;
            // 有了目标世界位置，通过尝试排名候选地图来解决重叠边界歧义；否则保持固定的 to_map。
            const bool have_world = (to_world.x != 0.f || to_world.y != 0.f);
            const bool ok = have_world
                ? BuildCrossMapRouteResolvingDst(from_map, start, start_wm, to_world, full_path, hidden_segments)
                : BuildCrossMapRoute(from_map, to_map, start, goal, start_wm, full_path, hidden_segments);
            if (!ok) return;
            Resources::EnqueueMainTask([full_path, cur_map] {
                ClearPathLines();
                const auto* cur_info = GetCachedMapInfo(cur_map);
                auto in_bounds = [&](const GW::GamePos& g) {
                    return cur_info && g.x >= cur_info->bounds_min.x && g.x <= cur_info->bounds_max.x &&
                           g.y >= cur_info->bounds_min.y && g.y <= cur_info->bounds_max.y;
                };
                for (size_t i = 0; i + 1 < full_path.size(); i++) {
                    if (IsPathBreak(full_path[i]) || IsPathBreak(full_path[i + 1])) continue;
                    GW::GamePos g1, g2;
                    const bool on_cur = WorldMapWidget::WorldMapToGamePos(full_path[i], g1, cur_map)
                        && WorldMapWidget::WorldMapToGamePos(full_path[i + 1], g2, cur_map)
                        && in_bounds(g1) && in_bounds(g2);
                    CustomRenderer::CustomLine* line;
                    if (on_cur) {
                        line = Minimap::Instance().custom_renderer.AddCustomLine(g1, g2);
                    } else {
                        line = Minimap::Instance().custom_renderer.AddCustomLine({full_path[i].x, full_path[i].y, 0}, {full_path[i + 1].x, full_path[i + 1].y, 0});
                        line->world_coords = true;
                    }
                    line->map = cur_map;
                    line->color = 0xFFFFFF00;
                    line->draw_on_minimap = true;
                    line->draw_on_mission_map = true;
                    line->created_by_toolbox = true;
                    path_lines.push_back(line);
                }
            });
        });
    }

    void RecalculatePath(const GW::GamePos& from, const GW::GamePos& to)
    {
        if (!NeedsRecalculating(from, to)) return;
        ClearPathLines();
        delete astar;
        astar = nullptr;
        auto target_map = GW::Map::GetMapID();
        if (path_from_map != GW::Constants::MapID::None && path_from_map == path_to_map) target_map = path_from_map;
        const auto map_id = target_map;
        Resources::EnqueueWorkerTask([from, to, map_id] {
            std::lock_guard route_lock(route_mutex); // 一次一个构建；共享缓存 + 梯形 pf 暂存区（节点/prioq）不是并发安全的
            RouteJobScope job_scope; // 在持有 MilePath* 时延迟淘汰
            PathCalcScope calc_scope; // 为世界地图指示器标记路径计算
            Pathing::MilePath* milepath = nullptr;
            if (map_id == GW::Map::GetMapID()) {
                milepath = GetMilepathForCurrentMap();
            }
            else {
                milepath = GetMilepathForMap(map_id);
            }
            if (!milepath) {
                PATH_LOG_ERROR("地图 %d 无 milepath", (int)map_id);
                return;
            }
            if (!milepath->ready()) {
                PATH_LOG_INFO("等待地图 %d 可视区图...", (int)map_id);
                while (!milepath->ready() && !pending_terminate) {
                    Sleep(100);
                }
                if (pending_terminate) return;
                PATH_LOG_INFO("地图 %d 就绪", (int)map_id);
            }
            const auto tmpAstar = new Pathing::AStar(milepath);
            const auto res = tmpAstar->Search(from, to);
            if (res == Pathing::Error::FailedToFinializePath) {
                delete tmpAstar;
                return;
            }
            if (res != Pathing::Error::OK) {
                PATH_LOG_ERROR("寻路失败；Pathing::Error 代码 %d", res);
                delete tmpAstar;
                return;
            }
            if (!tmpAstar->m_path.ready()) {
                PATH_LOG_ERROR("寻路失败；tmpAstar->m_path 未就绪");
                delete tmpAstar;
                return;
            }
            astar = tmpAstar;
            const auto points = astar->m_path.points(); // copy
            Resources::EnqueueMainTask([points, map_id] {
                DrawPathAsLines(points, map_id);
            });
        });
    }

    // MapID → file_hash 查找表，从 maps_constant_data.h 构建
    std::unordered_map<GW::Constants::MapID, uint32_t> map_id_to_file_hash;

    void BuildMapFileHashLookup()
    {
        for (const auto& [file_hash, entries] : constant_maps_info) {
            for (const auto& entry : entries) {
                if (entry.file_hash && !map_id_to_file_hash.contains(entry.map_id)) map_id_to_file_hash[entry.map_id] = (uint32_t)entry.file_hash;
            }
        }
        PATH_LOG_INFO("已构建地图文件哈希查找表：%d 个条目", (int)map_id_to_file_hash.size());
        auto it837 = map_id_to_file_hash.find(GW::Constants::MapID::War_in_Kryta_Talmark_Wilderness);
        PATH_LOG_INFO("  地图 837：%s (0x%X)", it837 != map_id_to_file_hash.end() ? "已找到" : "未找到", it837 != map_id_to_file_hash.end() ? it837->second : 0);
        auto it381 = map_id_to_file_hash.find(GW::Constants::MapID::Yohlon_Haven_outpost);
        PATH_LOG_INFO("  地图 381：%s (0x%X)", it381 != map_id_to_file_hash.end() ? "已找到" : "未找到", it381 != map_id_to_file_hash.end() ? it381->second : 0);
        int found_381 = 0;
        for (const auto& [fh, entries] : constant_maps_info) {
            for (const auto& e : entries) {
                if (e.map_id == GW::Constants::MapID::Yohlon_Haven_outpost) {
                    PATH_LOG_INFO("  constant_maps_info：地图=381 file_hash=0x%X outer_key=0x%X", e.file_hash, fh);
                    found_381++;
                }
            }
        }
        PATH_LOG_INFO("  在 constant_maps_info 中找到 381 %d 次", found_381);
    }

    std::set<uint32_t> file_id_mismatch_warned; // 已警告过的地图

    // 前哨站与可探索地图共享 MapID 但加载不同地图文件的地图（常量表保存可探索文件；
    // 前哨站的 kLoadMapContext file_name 不解码，因此运行时覆盖无法捕获）。
    constexpr std::pair<GW::Constants::MapID, uint32_t> outpost_file_id_overrides[] = {
        {GW::Constants::MapID::Domain_of_Anguish, 0x3452b}, // 前哨站 = 共享痛苦之门房间；可探索 = 0x3584f
    };

    bool IsDualInstanceMap(GW::Constants::MapID map_id)
    {
        return std::ranges::any_of(outpost_file_id_overrides, [map_id](const auto& e) { return e.first == map_id; });
    }

    uint32_t GetMapFileId(GW::Constants::MapID map_id)
    {
        if (map_id == GW::Map::GetMapID() && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost) {
            for (const auto& [mid, outpost_fid] : outpost_file_id_overrides) {
                if (mid == map_id) return outpost_fid;
            }
        }

        // 检查运行时查找表（从 constant_maps_info + StoC 数据包填充）
        auto it = map_id_to_file_hash.find(map_id);
        if (it != map_id_to_file_hash.end()) return it->second;

        // 回退到 AreaInfo
        const auto area_info = GW::Map::GetMapInfo(map_id);
        uint32_t runtime_fid = (area_info && area_info->file_id) ? area_info->file_id : 0;

        // 回退到 constant_maps_info（覆盖自定义/重映射地图 ID）
        uint32_t constant_fid = 0;
        for (const auto& [file_hash, entries] : constant_maps_info) {
            if (!file_hash) continue;
            for (const auto& e : entries) {
                if (e.map_id == map_id) {
                    constant_fid = (uint32_t)file_hash;
                    break;
                }
            }
            if (constant_fid) break;
        }

        // 对每个地图警告一次运行时/常量 file_id 不匹配；跳过地图 0（None 没有 file_id 且被例行查找）。
        if ((uint32_t)map_id != 0 && !file_id_mismatch_warned.contains((uint32_t)map_id)) {
            if (map_id == GW::Constants::MapID::Shing_Jea_Monastery_outpost) {
                int found_count = 0;
                uint32_t found_fh = 0;
                for (const auto& [fh, entries] : constant_maps_info) {
                    for (const auto& e : entries) {
                        if (e.map_id == GW::Constants::MapID::Shing_Jea_Monastery_outpost) {
                            found_count++;
                            found_fh = (uint32_t)fh;
                        }
                    }
                }
                PATH_LOG_INFO(
                    "[FileId] 地图 242 调试：runtime=0x%X constant=0x%X 查找=%s 暴力=%d(0x%X) 总组数=%d", runtime_fid, constant_fid, map_id_to_file_hash.contains(GW::Constants::MapID::Shing_Jea_Monastery_outpost) ? "在表中" : "不在表中",
                    found_count, found_fh, (int)constant_maps_info.size()
                );
            }
            if (runtime_fid && constant_fid && runtime_fid != constant_fid) {
                file_id_mismatch_warned.insert((uint32_t)map_id);
                PATH_LOG_WARNING("[FileId] 地图 %d：runtime=0x%X constant=0x%X 不匹配", (int)map_id, runtime_fid, constant_fid);
            }
            else if (runtime_fid && !constant_fid) {
                file_id_mismatch_warned.insert((uint32_t)map_id);
                PATH_LOG_WARNING("[FileId] 地图 %d：runtime=0x%X 但 maps_constant_data.h 中缺失", (int)map_id, runtime_fid);
            }
            else if (!runtime_fid && !constant_fid) {
                file_id_mismatch_warned.insert((uint32_t)map_id);
                PATH_LOG_WARNING("[FileId] 地图 %d：运行时或常量数据均无 file_id", (int)map_id);
            }
        }

        uint32_t result = runtime_fid ? runtime_fid : constant_fid;
        if (result) map_id_to_file_hash[map_id] = result;
        return result;
    }

    uint32_t map_load_generation = 0;

    void OnUIMessage(GW::HookStatus* status, GW::UI::UIMessage message_id, void* wParam, void*)
    {
        if (status->blocked) return;
        switch (message_id) {
            case GW::UI::UIMessage::kLoadMapContext: {
                // 从数据包缓存 MapID → file_id（发现 maps_constant_data.h 中没有的地图）
                const auto packet = static_cast<GW::UI::UIPacket::kLoadMapContext*>(wParam);
                if (packet->file_name && *packet->file_name) {
                    const uint32_t fid = ArenaNetFileParser::FileHashToFileId(packet->file_name);
                    if (fid) {
                        const auto it = map_id_to_file_hash.find(packet->map_id);
                        if (it == map_id_to_file_hash.end()) {
                            map_id_to_file_hash[packet->map_id] = fid;
                            // 重建图以包含新发现的地图
                            map_graph_built = false;
                            PATH_LOG_INFO("发现地图 %d file_id=0x%X", (int)packet->map_id, fid);
                        }
                        else if (it->second != fid) {
                            // 客户端加载的文件是真实值；不同的缓存 ID 意味着过时的常量数据。
                            PATH_LOG_WARNING("[FileId] 地图 %d：加载的文件 0x%X 覆盖了缓存的 0x%X", (int)packet->map_id, fid, it->second);
                            it->second = fid;
                            map_graph_built = false;
                        }
                    }
                }

                if (IsDualInstanceMap(packet->map_id)) {
                    ++map_load_generation;
                    GameWorldRenderer::ClearNavmeshLines();
                    GameWorldRenderer::SetNavmeshWorldMapLines(GW::Constants::MapID::None, {});
                }

                // 地图切换时清除所有线以避免陈旧渲染
                ClearBoundsLines();
                ClearGraphEdgeLines();
                ClearPortalMarkerLines();
                ClearHoverHighlightLines();
                ClearMarkerLines();
                ClearPathLines();
                ClearPortalPairLines();
                ClearSavedConnectionLines();
                ClearEditorHighlightLines();
                delete astar;
                astar = nullptr;

                PathfindingWindow::ReadyForPathing();
                pending_connection_lines_update = true;
            } break;
        }
    }


} // namespace

bool PathfindingWindow::IsPathingEnabled()
{
    return pathing_enabled;
}

bool PathfindingWindow::ReadyForPathing()
{
    // 此处绝不加载 DAT：探测驻留缓存，如果缺失则启动后台加载。在加载完成前返回 false，
    // 因此调用者（如任务路径）只需在下一帧重试 — 游戏线程永不阻塞。
    const auto m = GetResidentMilepathOrPrewarm();
    return m && m->ready();
}

bool PathfindingWindow::IsCalculatingPath()
{
    return path_calc_in_flight.load() > 0;
}

void LoadAndShowMapsAtWorldPos(const GW::Vec2f& wm_pos); // 前向声明

// 编辑器 UI 在此分支中已裁剪；寻路 API 不需要窗口。Draw 不渲染任何内容，但必须每帧排空延迟移除队列，
// 否则清除的路径线会留在屏幕上。WndProc 是无操作虚函数表存根。
static void UpdateNavmeshOverlay()
{
    static bool was_on = false;
    static GW::Constants::MapID built_map = GW::Constants::MapID::None;
    static uint32_t built_generation = 0;
    static GW::GamePos last_build_pos{};
    static std::vector<Pathing::NavMesh::DebugEdge> cached_edges; // 每地图提取一次的全地图边
    if (!settings.draw_navmesh_overlay) {
        if (was_on) {
            GameWorldRenderer::ClearNavmeshLines();
            DeferRemoveLines(navmesh_edge_lines); // 丢弃旧的逐线路径留下的任何线
            cached_edges.clear();
            built_map = GW::Constants::MapID::None;
            was_on = false;
        }
        return;
    }
    was_on = true;

    const auto cur_map = GW::Map::GetMapID();
    const auto me = GW::Agents::GetControlledCharacter();
    const bool map_changed = cur_map != built_map || built_generation != map_load_generation;
    float moved2 = 1e30f;
    if (me && !map_changed) { const float dx = me->pos.x - last_build_pos.x, dy = me->pos.y - last_build_pos.y; moved2 = dx * dx + dy * dy; }
    if (!map_changed && moved2 < 600.f * 600.f) return; // 仍在附近，无需重建

    auto* mp = GetResidentMilepathOrPrewarm(); // Draw 在游戏线程上运行 — 绝不能阻塞 DAT 读取
    if (!mp || !mp->ready()) return;
    auto* nav = mp->GetNavMeshForDebug();
    if (!nav || !nav->IsReady()) {
        static Pathing::MilePath* build_requested = nullptr;
        if (build_requested != mp && !mp->build_failed()) {
            build_requested = mp;
            Resources::EnqueueWorkerTask([mp] { mp->EnsureFullBuild(); });
        }
        return;
    }

    auto edge_color = [](const Pathing::NavMesh::DebugEdge& e) -> unsigned int {
        const bool hi = e.a.zplane != 0; // 离开地平面的边使用“高”颜色
        return e.wall ? (hi ? settings.navmesh_wall_color_hi : settings.navmesh_wall_color)
                      : (hi ? settings.navmesh_connection_color_hi : settings.navmesh_connection_color);
    };

    if (map_changed) {
        cached_edges.clear();
        nav->DebugExtractEdges(cached_edges); // 提取整个网格一次；在移动时重新裁剪成本低廉
        // 将完整网格交给 2D 世界地图（平坦且低成本，无需每移动重新裁剪）。
        std::vector<GameWorldRenderer::BatchedLine> full;
        full.reserve(cached_edges.size());
        for (const auto& e : cached_edges) full.push_back({e.a, e.b, edge_color(e)});
        GameWorldRenderer::SetNavmeshWorldMapLines(cur_map, std::move(full));
    }

    // 在游戏世界中：裁剪到玩家周围的共享渲染距离，使批次保持较小（快速悬挂，跟随玩家）
    // — 作为 ONE 双缓冲批处理 VB（一次绘制调用，无闪烁）交给渲染器。
    const float draw_range = GameWorldRenderer::GetRenderMaxDistance();
    const float range2 = draw_range * draw_range;
    std::vector<GameWorldRenderer::BatchedLine> lines;
    lines.reserve(cached_edges.size());
    for (const auto& e : cached_edges) {
        if (me) { const float dx = me->pos.x - e.a.x, dy = me->pos.y - e.a.y; if (dx * dx + dy * dy > range2) continue; }
        lines.push_back({e.a, e.b, edge_color(e)});
    }
    GameWorldRenderer::SetNavmeshLines(cur_map, std::move(lines));
    if (map_changed) Log::Log("[导航网格] 覆盖层已重建：地图=%d gen=%u 边=%d", (int)cur_map, map_load_generation, (int)cached_edges.size());
    built_map = cur_map;
    built_generation = map_load_generation;
    if (me) last_build_pos = me->pos;
}

float PathfindingWindow::GetPathRecalcDistance() { return settings.path_recalc_distance; }

bool PathfindingWindow::DebugDumpNavMeshNear(const GW::GamePos& center, float radius)
{
    auto* mp = GetResidentMilepathOrPrewarm();
    if (!mp || !mp->ready()) { Log::Log("[navdump] milepath 未就绪（重试）"); return false; }
    auto* nav = mp->GetNavMeshForDebug();
    if (!nav || !nav->IsReady()) {
        if (!mp->build_failed()) Resources::EnqueueWorkerTask([mp] { mp->EnsureFullBuild(); });
        Log::Log("[navdump] 导航网格正在构建（重试）");
        return false;
    }
    Log::Log("[navdump] 地图=%d", (int)GW::Map::GetMapID());
    nav->DebugDumpNear(center, radius);
    return true;
}

Pathing::NavMesh* PathfindingWindow::GetResidentNavMesh()
{
    auto* mp = GetResidentMilepathOrPrewarm();
    if (!mp || !mp->ready()) return nullptr;
    auto* nav = mp->GetNavMeshForDebug();
    return (nav && nav->IsReady()) ? nav : nullptr;
}

void PathfindingWindow::Draw(IDirect3DDevice9*)
{
    ProcessDeferredRemovals();
    UpdateNavmeshOverlay();
}

void PathfindingWindow::DrawSettingsInternal()
{
    ImGui::DragFloat("路径重新计算距离", &settings.path_recalc_distance, 1.f, 1.f, 1000.f, "%.0f");
    ImGui::ShowHelp(path_recalc_distance_help);
    ImGui::Checkbox("导航网格覆盖层", &settings.draw_navmesh_overlay);
    ImGui::ShowHelp(navmesh_overlay_help);
    ImGui::Separator();
    if (settings.draw_navmesh_overlay) {
        auto color_edit = [](const char* label, uint32_t* argb) {
            float c[4] = {((*argb >> 16) & 0xFF) / 255.f, ((*argb >> 8) & 0xFF) / 255.f, (*argb & 0xFF) / 255.f, ((*argb >> 24) & 0xFF) / 255.f};
            if (ImGui::ColorEdit4(label, c, ImGuiColorEditFlags_AlphaBar)) {
                const auto q = [](const float f) { return static_cast<uint32_t>(std::clamp(f * 255.f + 0.5f, 0.f, 255.f)); };
                *argb = (q(c[3]) << 24) | (q(c[0]) << 16) | (q(c[1]) << 8) | q(c[2]);
            }
        };
        if (ImGui::DragFloat("地形采样间距", &settings.navmesh_sample_spacing, 0.5f, 1.f, 100.f, "%.0f 游戏单位")) {
            GameWorldRenderer::SetNavmeshSampleSpacing(settings.navmesh_sample_spacing);
            GameWorldRenderer::RedrapeNavmesh(); // 立即重新悬挂，使更改立即可见
        }
        ImGui::ShowHelp(navmesh_sample_spacing_help);
        color_edit("墙颜色（地面层）", &settings.navmesh_wall_color);
        color_edit("墙颜色（其他平面）", &settings.navmesh_wall_color_hi);
        color_edit("连接颜色（地面层）", &settings.navmesh_connection_color);
        color_edit("连接颜色（其他平面）", &settings.navmesh_connection_color_hi);
    }
}

void PathfindingWindow::LoadSettings(SettingsDoc& doc, ToolboxIni* legacy)
{
    ToolboxModule::LoadSettings(doc, legacy);
    doc.GetStruct(Name(), settings);
    GameWorldRenderer::SetNavmeshSampleSpacing(settings.navmesh_sample_spacing);
}

void PathfindingWindow::SaveSettings(SettingsDoc& doc)
{
    ToolboxModule::SaveSettings(doc);
    doc.SetStruct(Name(), settings);
}

bool PathfindingWindow::WndProc(UINT, WPARAM, LPARAM)
{
    return false;
}

void PathfindingWindow::SignalTerminate()
{
    ToolboxModule::SignalTerminate();
    pathing_enabled = false;
    pending_terminate = true;
    GW::UI::RemoveUIMessageCallback(&gw_ui_hookentry);
    ClearBoundsLines();
    ClearGraphEdgeLines();
    ClearPortalMarkerLines();
    ClearHoverHighlightLines();
    ClearMarkerLines();
    ClearPathLines();
    ClearPortalPairLines();
    ClearSavedConnectionLines();
    ClearEditorHighlightLines();
    // 向所有 milepath 发送停止信号，但先不删除（Terminate 会删除）
    for (const auto mile_path : mile_paths_by_coords | std::views::values) {
        mile_path->stopProcessing();
    }
}

bool PathfindingWindow::CanTerminate()
{
    if (!pending_terminate && pending_worker_task) return false;
    for (const auto m : mile_paths_by_coords) {
        if (m.second->isProcessing()) return false;
    }
    return true;
}

clock_t PathfindingWindow::CalculatePath(const GW::GamePos& from, const GW::GamePos& to, CalculatedCallback callback, void* args)
{
    if (pending_terminate) return 0;

    if (!ReadyForPathing()) return 0;

    pending_worker_task = true;

    Resources::EnqueueWorkerTask([from, to, callback, args] {
        RouteJobScope job_scope; // 在持有 MilePath* 时延迟淘汰
        // 始终精确调用一次回调；静默失败会使调用者的“计算中”标志卡住，路径冻结。
        auto fire_empty = [callback, args] {
            Resources::EnqueueMainTask([callback, args] {
                std::vector<GW::GamePos> empty_vec = {};
                callback(empty_vec, args);
            });
        };

        if (pending_terminate) {
            pending_worker_task = false;
            return;
        }

        const auto milepath = GetMilepathForCurrentMap();
        if (!milepath || !milepath->ready()) {
            if (milepath && milepath->build_failed()) {
                PATH_LOG_ERROR("寻路失败；MilePath 构建内存不足，可视区图不可用");
            }
            fire_empty();
            pending_worker_task = false;
            return;
        }

        auto astr = Pathing::AStar(milepath);
        const auto res = astr.Search(from, to);
        if (res == Pathing::Error::FailedToFinializePath) {
            // 路径被阻挡；这是有效结果。
            fire_empty();
            pending_worker_task = false;
            return;
        }
        if (res != Pathing::Error::OK) {
            PATH_LOG_ERROR("寻路失败；Pathing::Error 代码 %d", res);
            fire_empty();
            pending_worker_task = false;
            return;
        }
        if (!astr.m_path.ready()) {
            Log::Log("寻路失败；astar.m_path 未就绪");
            fire_empty();
            pending_worker_task = false;
            return;
        }

        const auto& points = astr.m_path.points();
        auto waypoints = new std::vector<GW::GamePos>();
        waypoints->reserve(points.size());
        for (const auto& p : points) {
            waypoints->emplace_back(p);
        }

        Resources::EnqueueMainTask([waypoints, callback, args] {
            callback(*waypoints, args);
            delete waypoints;
        });
        pending_worker_task = false;
    });
    return TIMER_INIT();
}

void PathfindingWindow::Terminate()
{
    ToolboxModule::Terminate();
    // 工作线程已停止/加入，因此每次 `delete mp` 纯粹是分配器工作；由于调试分配器在串行下较慢，并行化删除。
    if (!mile_paths_by_coords.empty()) {
        std::vector<std::thread> deletes;
        deletes.reserve(mile_paths_by_coords.size());
        for (const auto& [hash, mp] : mile_paths_by_coords) {
            deletes.emplace_back([mp] {
                delete mp;
            });
        }
        for (auto& t : deletes)
            t.join();
    }
    mile_paths_by_coords.clear();
    {
        std::scoped_lock lock(lru_mutex);
        lru_order.clear();
        lru_pos.clear();
        route_jobs_active = 0;
    }
    path_calc_in_flight = 0; // 强制清除指示器（镜像上面的 route_jobs_active）
    cached_map_info.clear();
    portal_props_cache.clear();
    map_graph_nodes.clear();
    map_graph_built = false;
    points_by_hash.clear();
    map_id_to_file_hash.clear();
    delete astar;
    astar = nullptr;
}

uint32_t PathfindingWindow::GetMapFileId(GW::Constants::MapID map_id)
{
    return ::GetMapFileId(map_id);
}

void PathfindingWindow::SetFrom(const GW::GamePos& pos)
{
    path_from = pos;
    path_from_map = GW::Map::GetMapID();
    WorldMapWidget::GamePosToWorldMap(pos, path_from_world);
    uint32_t fh = GetMapFileId(path_from_map);
    if (fh) {
        auto& sp = points_by_hash[fh];
        sp.from = pos;
        sp.from_world = path_from_world;
        sp.from_set = true;
    }
    UpdateMarkers(path_from, path_to);
}

void PathfindingWindow::SetTo(const GW::GamePos& pos)
{
    path_to = pos;
    path_to_map = GW::Map::GetMapID();
    WorldMapWidget::GamePosToWorldMap(pos, path_to_world);
    uint32_t fh = GetMapFileId(path_to_map);
    if (fh) {
        auto& sp = points_by_hash[fh];
        sp.to = pos;
        sp.to_world = path_to_world;
        sp.to_set = true;
    }
    UpdateMarkers(path_from, path_to);
}

bool PathfindingWindow::GetNextPortalToward(GW::Constants::MapID from_map, const GW::GamePos& from_pos, GW::Constants::MapID to_map, const GW::Vec2f& goal_world_pos, GW::GamePos& out_portal_pos)
{
    if (from_map == GW::Constants::MapID::None || to_map == GW::Constants::MapID::None) return false;
    if (from_map == to_map) return false;
    const uint32_t fh_from = GetMapFileId(from_map);
    const uint32_t fh_to = GetMapFileId(to_map);
    if (fh_from && fh_from == fh_to &&
        !portal_connections.HasConnection(from_map, to_map) &&
        !portal_connections.HasConnection(to_map, from_map)) {
        return false;
    }

    // 解析目标世界位置 → 目标地图中的游戏位置（需要时强制加载 DAT）。
    if (!GetCachedMapInfo(to_map)) LoadMapFromDAT(to_map);
    GW::GamePos goal_game{};
    const bool have_goal = WorldMapWidget::WorldMapToGamePos(goal_world_pos, goal_game, to_map);

    auto route = FindMapRoute(from_map, to_map, &from_pos, have_goal ? &goal_game : nullptr);
    if (route.size() < 2) return false;

    const auto next = route[1];

    GW::Vec2f hint_wm{};
    WorldMapWidget::GamePosToWorldMap(from_pos, hint_wm, from_map);

    GW::GamePos exit_portal{}, next_entry{};
    const GW::GamePos* gg = (route.size() == 2 && have_goal) ? &goal_game : nullptr;
    if (!FindBestPortalPair(from_map, next, hint_wm, exit_portal, next_entry, &from_pos, gg)) return false;
    out_portal_pos = exit_portal;
    return true;
}

namespace {
    // 阻塞式。两个世界地图位置之间的完整跨地图路径到 `out`（世界坐标，地图间 PATH_BREAK）。失败时返回 false。
    bool ComputeRoute(const GW::Vec2f& from_world, const GW::Vec2f& to_world, std::vector<GW::Vec2f>& out)
    {
        out.clear();
        const auto from_map = GW::Map::GetMapID();
        if (from_map == GW::Constants::MapID::None) return false;

        GW::GamePos start;
        if (!WorldMapWidget::WorldMapToGamePos(from_world, start, from_map)) return false;

        // 通过按优先级顺序尝试排名候选地图来解析（可能模糊的）目标，直到一个连接成功。
        // 同地图偏好存在于解析器内部，因此当前边界内的目标仍然首先被尝试。
        std::vector<HiddenPathSegment> hidden;
        return BuildCrossMapRouteResolvingDst(from_map, start, from_world, to_world, out, hidden);
    }

    // 阻塞式。跨 `map_id`（其游戏坐标）的 A*，段以世界坐标输出。无共享状态，因此调用者可保留其路径。失败时返回 false。
    bool ComputeSegment(GW::Constants::MapID map_id, const GW::GamePos& from, const GW::GamePos& to, std::vector<GW::Vec2f>& out, std::vector<GW::GamePos>* out_game = nullptr)
    {
        out.clear();
        if (out_game) out_game->clear();
        if ((uint32_t)map_id == 0) map_id = GW::Map::GetMapID();
        if (map_id == GW::Constants::MapID::None) return false;
        std::vector<GW::GamePos> leg;
        if (!RunAStarOnMap(map_id, from, to, leg) || leg.empty()) return false;
        // 在 SegmentToWorld 将其展平为无平面的世界 Vec2f 之前，返回原始段（游戏坐标，带 zplane，在 map_id 的空间中）。
        if (out_game) *out_game = leg;
        SegmentToWorld(leg, map_id, out); // leg 游戏坐标 → 世界坐标
        return true;
    }
} // namespace

// 检测点击是否靠近先前位置（在世界地图约 5 单位内）
static bool IsNearby(const GW::Vec2f& a, const GW::Vec2f& b)
{
    float dx = a.x - b.x, dy = a.y - b.y;
    return (dx * dx + dy * dy) < 25.f;
}

// 解析世界地图点击的地图 ID，在重叠地图间循环
static GW::Constants::MapID ResolveMapForClick(const GW::Vec2f& world_map_pos, const GW::Vec2f& prev_world_pos, GW::Constants::MapID prev_map)
{
    auto map_id = WorldMapWidget::GetMapIdForLocation(world_map_pos);
    // 如果点击位置与上次相近，循环到下一个重叠地图
    if (map_id != GW::Constants::MapID::None && prev_map != GW::Constants::MapID::None && map_id == prev_map && IsNearby(world_map_pos, prev_world_pos)) {
        auto next = WorldMapWidget::GetMapIdForLocation(world_map_pos, prev_map);
        if (next != GW::Constants::MapID::None) map_id = next;
    }
    return (map_id != GW::Constants::MapID::None) ? map_id : GW::Map::GetMapID();
}

// 加载世界地图边界包含此位置的所有地图
void LoadAllMapsAtPosition(const GW::Vec2f& world_map_pos)
{
    RouteJobScope job_scope; // 仅在整批加载完成后修剪缓存
    BuildMapGraph();
    const auto cur_area = GW::Map::GetMapInfo();
    const auto cur_continent = cur_area ? cur_area->continent : GW::Continent::Kryta;
    const auto cur_map = GW::Map::GetMapID();

    // 加载边界包含点击的地图，以及它们的相邻/重叠地图
    std::set<uint32_t> to_load;
    for (const auto& node : map_graph_nodes) {
        if (node.continent != cur_continent) continue;
        if (node.wm_bounds.Contains({world_map_pos.x, world_map_pos.y})) {
            to_load.insert((uint32_t)node.map_id);
            if (IsInterestingMapForCacheTrace(node.map_id)) {
                PATH_LOG_INFO(
                    "[CacheTrace] LoadAllMapsAtPosition click=(%.0f,%.0f) bounds=(%.0f,%.0f)-(%.0f,%.0f) 添加直接 mid=%d", world_map_pos.x, world_map_pos.y, node.wm_bounds.Min.x, node.wm_bounds.Min.y, node.wm_bounds.Max.x, node.wm_bounds.Max.y,
                    (int)node.map_id
                );
            }
            for (auto adj : GetAdjacentMaps(node.map_id)) {
                to_load.insert((uint32_t)adj);
                if (IsInterestingMapForCacheTrace(adj)) {
                    PATH_LOG_INFO("[CacheTrace] LoadAllMapsAtPosition click=(%.0f,%.0f) 添加 adj mid=%d via parent=%d", world_map_pos.x, world_map_pos.y, (int)adj, (int)node.map_id);
                }
            }
        }
    }
    for (auto mid : to_load) {
        if (static_cast<GW::Constants::MapID>(mid) != cur_map) {
            LoadMapFromDAT(static_cast<GW::Constants::MapID>(mid));
        }
    }
}

void LoadAndShowMapsAtWorldPos(const GW::Vec2f& wm_pos)
{
    LoadAllMapsAtPosition(wm_pos);
    UpdateBoundsLines();
    if (draw_portals) UpdatePortalMarkers();
}

void PathfindingWindow::SetFromWorldMap(const GW::Vec2f& world_map_pos)
{
    path_from_map = ResolveMapForClick(world_map_pos, path_from_world, path_from_map);
    path_from_world = world_map_pos;

    LoadAllMapsAtPosition(world_map_pos);

    GW::GamePos game_pos;
    if (!WorldMapWidget::WorldMapToGamePos(world_map_pos, game_pos, path_from_map)) return;
    path_from = game_pos;

    uint32_t fh = GetMapFileId(path_from_map);
    if (fh) {
        auto& sp = points_by_hash[fh];
        sp.from = game_pos;
        sp.from_world = world_map_pos;
        sp.from_set = true;
    }

    PATH_LOG_INFO("SetFrom：wm=(%.1f,%.1f) game=(%.1f,%.1f) 地图=%d hash=0x%X cur=%d", world_map_pos.x, world_map_pos.y, game_pos.x, game_pos.y, (int)path_from_map, fh, (int)GW::Map::GetMapID());
    UpdateMarkers(path_from, path_to);
}

void PathfindingWindow::SetToWorldMap(const GW::Vec2f& world_map_pos)
{
    path_to_map = ResolveMapForClick(world_map_pos, path_to_world, path_to_map);
    path_to_world = world_map_pos;

    LoadAllMapsAtPosition(world_map_pos);

    GW::GamePos game_pos;
    if (!WorldMapWidget::WorldMapToGamePos(world_map_pos, game_pos, path_to_map)) return;
    path_to = game_pos;

    uint32_t fh = GetMapFileId(path_to_map);
    if (fh) {
        auto& sp = points_by_hash[fh];
        sp.to = game_pos;
        sp.to_world = world_map_pos;
        sp.to_set = true;
    }

    PATH_LOG_INFO("SetTo：wm=(%.1f,%.1f) game=(%.1f,%.1f) 地图=%d hash=0x%X cur=%d", world_map_pos.x, world_map_pos.y, game_pos.x, game_pos.y, (int)path_to_map, fh, (int)GW::Map::GetMapID());
    UpdateMarkers(path_from, path_to);
}

void PathfindingWindow::FindPath()
{
    UpdateMarkers(path_from, path_to);

    const bool have_to_world = (path_to_world.x != 0.f || path_to_world.y != 0.f);
    if (path_from_map != GW::Constants::MapID::None && have_to_world) {
        // 快速路径：如果目标落在当前地图边界内，首先尝试直接单地图 A*（以游戏坐标绘制）。
        const auto* from_info = GetCachedMapInfo(path_from_map);
        if (from_info) {
            GW::GamePos to_on_from_map;
            if (WorldMapWidget::WorldMapToGamePos(path_to_world, to_on_from_map, path_from_map)) {
                bool in_game_bounds = to_on_from_map.x >= from_info->bounds_min.x && to_on_from_map.x <= from_info->bounds_max.x && to_on_from_map.y >= from_info->bounds_min.y && to_on_from_map.y <= from_info->bounds_max.y;
                if (in_game_bounds) {
                    std::vector<GW::GamePos> direct_path;
                    if (RunAStarOnMap(path_from_map, path_from, to_on_from_map, direct_path) && !direct_path.empty()) {
                        DrawPathAsLines(direct_path, path_from_map);
                        return;
                    }
                    // 直接路径失败（例如仅共享文件 ID 的隔离区域）— 继续执行多地图路由。
                }
            }
        }

        // 在 AStar 段失败时带重试的多地图寻路
        blacklisted_edges.clear();
        RecalculateMultiMapPath_WithRetry(path_from_map, path_to_map, path_from, path_to, path_from_world, path_to_world);
    }
    else {
        RecalculatePath(path_from, path_to);
    }
}

void PathfindingWindow::ShowRouteToWorldMap(const GW::GamePos& from, const GW::Vec2f& goal_world_pos)
{
    SetFrom(from);
    SetToWorldMap(goal_world_pos);
    FindPath();
}

void PathfindingWindow::ClearWorldMapRoute()
{
    ClearPathLines();
    ClearPortalPairLines();
}

bool PathfindingWindow::CalculateRoute(const GW::Vec2f& from_world, const GW::Vec2f& to_world, std::vector<GW::Vec2f>* out)
{
    if (!out) return false;
    std::scoped_lock route_lock(route_mutex); // 一次一个构建；见 route_mutex
    RouteJobScope job_scope;                 // 在持有 MilePath* 时延迟淘汰
    PathCalcScope calc_scope;                // 为此流程也标记计算，以便窗口的进度条显示
    return ComputeRoute(from_world, to_world, *out);
}

bool PathfindingWindow::RecalculateSegment(GW::Constants::MapID map_id, const GW::GamePos& from, const GW::GamePos& to, std::vector<GW::Vec2f>* out, std::vector<GW::GamePos>* out_game)
{
    if (!out) return false;
    std::scoped_lock route_lock(route_mutex); // 一次一个构建；见 route_mutex
    RouteJobScope job_scope;                 // 在持有 MilePath* 时延迟淘汰
    return ComputeSegment(map_id, from, to, *out, out_game);
}

bool PathfindingWindow::IsWorldPosOnMap(const GW::Vec2f& world_pos, GW::Constants::MapID map_id)
{
    if ((uint32_t)map_id == 0) map_id = GW::Map::GetMapID();
    if (map_id == GW::Constants::MapID::None) return false;
    GW::GamePos g;
    if (!WorldMapWidget::WorldMapToGamePos(world_pos, g, map_id)) return false;
    Pathing::Vec2f bmin, bmax;
    if (!Pathing::GetMapGameBoundsFromDAT(GetMapFileId(map_id), bmin, bmax)) return false;
    return g.x >= bmin.x && g.x <= bmax.x && g.y >= bmin.y && g.y <= bmax.y;
}

bool PathfindingWindow::IsRouteBreak(const GW::Vec2f& p)
{
    return IsPathBreak(p);
}

void PathfindingWindow::Initialize()
{
    ToolboxModule::Initialize();
    SettingsRegistry::Register(this, settings);
    SettingsRegistry::Describe(this, "draw_navmesh_overlay", "导航网格覆盖层", navmesh_overlay_help);
    SettingsRegistry::Describe(this, "path_recalc_distance", "路径重新计算距离", path_recalc_distance_help);
    SettingsRegistry::Describe(this, "navmesh_sample_spacing", "地形采样间距", navmesh_sample_spacing_help);
    SettingsRegistry::Describe(this, "navmesh_wall_color", "墙颜色（地面层）");
    SettingsRegistry::Describe(this, "navmesh_wall_color_hi", "墙颜色（其他平面）");
    SettingsRegistry::Describe(this, "navmesh_connection_color", "连接颜色（地面层）");
    SettingsRegistry::Describe(this, "navmesh_connection_color_hi", "连接颜色（其他平面）");
    pending_terminate = false; // 模块现在是可选的；先前禁用会设置此项，会中止所有路由
    pathing_enabled = true;
    BuildMapFileHashLookup();
    RegisterUIMessageCallback(&gw_ui_hookentry, GW::UI::UIMessage::kLoadMapContext, OnUIMessage, 0x4000);

    // 从嵌入为 RCDATA 资源的 JSON 加载传送门连接，因此运行时无需将松散文件放在 DLL 旁边。
    const EmbeddedResource portal_json(IDR_PORTAL_CONNECTIONS_JSON, RT_RCDATA, GWToolbox::GetDLLModule());
    if (portal_json.data() && portal_json.size()) {
        portal_connections.LoadFromMemory(
            static_cast<const char*>(portal_json.data()), portal_json.size(), "<嵌入式资源>");
    }
    else {
        PATH_LOG_ERROR("加载嵌入式传送门连接资源失败");
    }
    pending_connection_lines_update = true;
}
