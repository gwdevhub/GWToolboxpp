#include "stdafx.h"

#include <GWCA/GameContainers/GamePos.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Pathing.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/MapMgr.h>

#include <Color.h>
#include <ImGuiAddons.h>
#include <Logger.h>
#include <Modules/GwDatModule.h>
#include <Modules/RiverModule.h>
#include <Utils/GameWorldCompositor.h>
#include <Utils/SettingsDoc.h>
#include <Utils/SettingsRegistry.h>
#include <Utils/TerrainDrape.h>
#include <Widgets/Minimap/GameWorldRenderer.h>

// 由 CMake (fxc) 从共享着色器目录中的 .hlsl 生成。
#include "Widgets/Minimap/Shaders/river_ps.h"
#include "Widgets/Minimap/Shaders/river_vs.h"

// 外部链接（非匿名命名空间），以便 glaze 在（反）序列化每地图河流列表时将这些反映为元素。
// 普通聚合体，无需 glz::meta 即可进行反射。
namespace lava_river_module {
    struct RiverPoint {
        float x = 0.f;
        float y = 0.f;
    };
    struct LavaRiver {
        std::vector<RiverPoint> points;
        float width = 200.f;
    };
} // namespace lava_river_module

namespace {
    using namespace lava_river_module;

    constexpr float kSampleSpacing = 50.f;            // resample a river centerline every ~50 gu (follow slopes)
    constexpr float kAltUnknown = TerrainDrape::kNoAltitude; // sentinel: no terrain data at this (x,y)
    constexpr size_t kMaxVertices = 1500000;          // safety cap on total surface geometry per map (raise cell_size if hit)

    float render_max_distance = 5000.f;
    float fog_factor = 1.0f;     // 距离衰减强度，传递给着色器
    float glow = 0.4f;           // 自发光亮度增强（0 = 原始纹理）
    float flow_speed = 0.15f;    // 纹理每秒钟滚动的图块数（在漂流的流动方向上）
    float flow_dir_change = 0.06f; // 流动方向每秒钟漂移的弧度
    float tile_length = 256.f;   // 每个纹理重复的世界单位（地面 UV 平铺 / 河流长度）
    float z_lift = 5.f;          // 将表面抬升到地面之上以避免深度冲突（激战向上为 -z）
    // 选定的 GW .dat 纹理 ID。当选择多个时，它们以块状在地面上散布以增加变化；
    // 空列表 = 不绘制任何内容。通过 SettingsDoc 持久化（向量不是标量 SettingsRegistry 字段）。
    std::vector<uint32_t> textures = {0x45478};
    unsigned int lava_tint = 0xFFFFFFFFu; // ImGui 打包格式；白色显示原始纹理未染色
    float new_river_width = 200.f;        // 新创建河流的默认宽度

    bool cover_entire_ground = true; // true = 覆盖整个可行走地面；false = 仅使用创作的河流
    float wave_amplitude = 40.f;     // 世界单位中的波峰高度（由缓慢包络调制）
    float wave_scale = 1.0f;         // 波长乘数（越大 = 波越短、越汹涌）
    float cell_size = 100.f;         // 地面细分单元格（世界单位）；越小 = 更精细的波浪，更多顶点

    // 每地图创作的河流，以 MapID 为键。通过 SettingsDoc 持久化到 LavaRivers.json。
    std::map<uint32_t, std::vector<LavaRiver>> rivers_by_map;

    // 位置（世界，贴在地面上）+ D3DCOLOR 颜色 + UV（u 跨宽度，v 沿长度）。
    struct LavaVertex {
        float x, y, z;
        DWORD color;
        float u, v;
    };

    // 设置中提供的命名纹理（游戏内熔岩纹理 + 水面纹理）
    struct KnownTex {
        uint32_t id;
        const char* name;
    };
    constexpr KnownTex kKnownTextures[] = {
        {0x45478, "熔岩 1"}, {0x457eb, "熔岩 2"}, {0x23e8, "熔岩 3"},
        {0x1680b, "熔岩 4"}, {0x1680c, "熔岩 5"}, {0x20de, "熔岩 6"},
        {0x1baa1, "水面"},
    };

    // 地面块（cx,cy）周围使用哪种选定纹理 — 粗粒度的空间哈希，使相同纹理聚集在约 1024 单位的块中，而不是每梯形斑点化。
    int TexBucket(const float cx, const float cy, const int num)
    {
        if (num <= 1) return 0;
        const int qx = static_cast<int>(std::floor(cx / 1024.f));
        const int qy = static_cast<int>(std::floor(cy / 1024.f));
        uint32_t h = static_cast<uint32_t>(qx * 73856093) ^ static_cast<uint32_t>(qy * 19349663);
        h ^= h >> 13;
        h *= 0x85ebca6bu;
        h ^= h >> 16;
        return static_cast<int>(h % static_cast<uint32_t>(num));
    }

    std::vector<LavaVertex> mesh_vertices;
    struct TexRange {
        uint32_t file_id;
        size_t offset; // 第一个顶点
        size_t count;  // 顶点数（3 的倍数）
    };
    std::vector<TexRange> tex_ranges; // 每个选定纹理对应 mesh_vertices 中的一个连续段
    IDirect3DVertexBuffer9* mesh_vb = nullptr;
    size_t mesh_cap = 0; // 顶点数容量
    bool mesh_ready = false;
    bool mesh_dirty = true;
    uint32_t built_map_id = 0xFFFFFFFFu;

    constexpr int kQueriesPerFrame = 2000; // max QueryAltitude calls per frame while a build is in progress
    struct GroundGridVert {
        float x, y, z;
    };
    std::vector<GroundGridVert> ground_grid_scratch; // 每个梯形复用，避免重复分配
    struct GroundBuild {
        bool active = false;
        uint32_t map_id = 0;
        uint32_t n_planes = 0;
        int num = 0;        // 构建开始时捕获的纹理/桶数量
        uint32_t plane = 0; // 光标：正在细分的平面
        uint32_t trap = 0;  // 光标：平面内下一个梯形
        size_t total = 0;   // 已发射的顶点数（用于 kMaxVertices 上限）
        std::vector<std::vector<LavaVertex>> buckets;
    };
    GroundBuild gbuild;

    IDirect3DVertexShader9* lava_vs = nullptr;
    IDirect3DPixelShader9* lava_ps = nullptr;
    IDirect3DVertexDeclaration9* lava_decl = nullptr;

    float t_seconds = 0.f;             // 传递给波浪着色器的累计时间
    float scroll_u = 0.f, scroll_v = 0.f; // 累计纹理滚动（漂移流动）
    float flow_angle = 0.f;            // 当前流动方向（弧度）
    DWORD last_tick = 0;
    int compositor_token = 0;

    // 创作状态（不持久）：当前地图的哪个河流“添加点”附加到。
    int active_river = -1;

    // Build a draped, textured ribbon for one river and append it to the given vertex bucket.
    void BuildRiverMesh(const LavaRiver& river, const uint32_t n_planes, std::vector<LavaVertex>& out)
    {
        if (river.points.size() < 2) return;

        // 1. 对中心线重新采样，使每个线段跟随创作点之间的坡度/阶梯。
        std::vector<RiverPoint> dense;
        for (size_t i = 0; i < river.points.size(); ++i) {
            if (!dense.empty()) {
                const float dx = river.points[i].x - river.points[i - 1].x;
                const float dy = river.points[i].y - river.points[i - 1].y;
                const int steps = std::max(1, static_cast<int>(std::sqrt(dx * dx + dy * dy) / kSampleSpacing));
                for (int j = 1; j < steps; ++j) {
                    const float t = static_cast<float>(j) / static_cast<float>(steps);
                    dense.push_back({river.points[i - 1].x + dx * t, river.points[i - 1].y + dy * t});
                }
            }
            dense.push_back(river.points[i]);
        }
        if (dense.size() < 2) return;

        const float half = std::max(1.f, river.width) * 0.5f;
        const float tile = std::max(1.f, tile_length);
        const DWORD col = lava_tint;

        struct Cross {
            float lx, ly, lz, rx, ry, rz, v;
        };
        std::vector<Cross> xs;
        xs.reserve(dense.size());

        float arc = 0.f;
        float prevz = TerrainDrape::HighestZ(dense[0].x, dense[0].y, n_planes);
        if (prevz == kAltUnknown) prevz = 0.f;

        for (size_t i = 0; i < dense.size(); ++i) {
            const RiverPoint& a = dense[i > 0 ? i - 1 : i];
            const RiverPoint& b = dense[i + 1 < dense.size() ? i + 1 : i];
            float tx = b.x - a.x, ty = b.y - a.y;
            const float tl = std::sqrt(tx * tx + ty * ty);
            if (tl > 1e-4f) {
                tx /= tl;
                ty /= tl;
            }
            else {
                tx = 1.f;
                ty = 0.f;
            }
            const float px = -ty, py = tx; // XY 中的垂线

            if (i > 0) {
                const float dx = dense[i].x - dense[i - 1].x, dy = dense[i].y - dense[i - 1].y;
                arc += std::sqrt(dx * dx + dy * dy);
            }
            const float lx = dense[i].x + px * half, ly = dense[i].y + py * half;
            const float rx = dense[i].x - px * half, ry = dense[i].y - py * half;

            float lz = TerrainDrape::ClosestZ(lx, ly, n_planes, prevz);
            if (lz == kAltUnknown) lz = prevz;
            float rz = TerrainDrape::ClosestZ(rx, ry, n_planes, prevz);
            if (rz == kAltUnknown) rz = prevz;
            const float cz = TerrainDrape::ClosestZ(dense[i].x, dense[i].y, n_planes, prevz);
            if (cz != kAltUnknown) prevz = cz;

            xs.push_back({lx, ly, lz - z_lift, rx, ry, rz - z_lift, arc / tile});
        }

        for (size_t i = 0; i + 1 < xs.size(); ++i) {
            if (out.size() + 6 > kMaxVertices) return;
            const Cross& c0 = xs[i];
            const Cross& c1 = xs[i + 1];
            out.push_back({c0.lx, c0.ly, c0.lz, col, 0.f, c0.v});
            out.push_back({c0.rx, c0.ry, c0.rz, col, 1.f, c0.v});
            out.push_back({c1.rx, c1.ry, c1.rz, col, 1.f, c1.v});
            out.push_back({c0.lx, c0.ly, c0.lz, col, 0.f, c0.v});
            out.push_back({c1.rx, c1.ry, c1.rz, col, 1.f, c1.v});
            out.push_back({c1.lx, c1.ly, c1.lz, col, 0.f, c1.v});
        }
    }

    int EmitGroundTrapezoid(const GW::PathingTrapezoid& tz, const uint32_t p)
    {
        if (tz.YT == tz.YB) return 0; // 退化的连接器

        const float cell = std::max(16.f, cell_size);
        const float tile = std::max(1.f, tile_length);
        const DWORD col = lava_tint;

        const float maxw = std::max(std::fabs(tz.XTR - tz.XTL), std::fabs(tz.XBR - tz.XBL));
        const int rows = std::clamp(static_cast<int>(std::fabs(tz.YT - tz.YB) / cell), 1, 64);
        const int cols = std::clamp(static_cast<int>(maxw / cell), 1, 64);
        if (gbuild.total + static_cast<size_t>(rows) * cols * 6 > kMaxVertices) return -1;

        const float cx = (tz.XTL + tz.XTR + tz.XBL + tz.XBR) * 0.25f;
        const float cy = (tz.YT + tz.YB) * 0.5f;
        std::vector<LavaVertex>& out = gbuild.buckets[TexBucket(cx, cy, gbuild.num)]; // 此块使用哪种纹理

        int queries = 0;
        const float fallback = TerrainDrape::QueryAltAt(cx, cy, p); // 中心高度，用于无数据的子顶点
        ++queries;

        std::vector<GroundGridVert>& grid = ground_grid_scratch;
        grid.assign(static_cast<size_t>(rows + 1) * (cols + 1), {});
        for (int j = 0; j <= rows; ++j) {
            const float v = static_cast<float>(j) / static_cast<float>(rows);
            const float y = tz.YB + (tz.YT - tz.YB) * v;
            const float xl = tz.XBL + (tz.XTL - tz.XBL) * v;
            const float xr = tz.XBR + (tz.XTR - tz.XBR) * v;
            for (int i = 0; i <= cols; ++i) {
                const float u = static_cast<float>(i) / static_cast<float>(cols);
                const float x = xl + (xr - xl) * u;
                float z = TerrainDrape::QueryAltAt(x, y, p);
                ++queries;
                if (z == 0.f) z = fallback;
                grid[static_cast<size_t>(j) * (cols + 1) + i] = {x, y, z};
            }
        }
        auto push = [&](const GroundGridVert& g) { out.push_back({g.x, g.y, g.z - z_lift, col, g.x / tile, g.y / tile}); };
        for (int j = 0; j < rows; ++j) {
            for (int i = 0; i < cols; ++i) {
                const GroundGridVert& a = grid[static_cast<size_t>(j) * (cols + 1) + i];
                const GroundGridVert& b = grid[static_cast<size_t>(j) * (cols + 1) + i + 1];
                const GroundGridVert& c = grid[static_cast<size_t>(j + 1) * (cols + 1) + i];
                const GroundGridVert& d = grid[static_cast<size_t>(j + 1) * (cols + 1) + i + 1];
                push(a);
                push(b);
                push(d);
                push(a);
                push(d);
                push(c);
                gbuild.total += 6;
            }
        }
        return queries;
    }

    // 将完成后的各纹理桶组装到绘制缓冲区中，并标记地图已构建。
    void FinalizeGroundBuild()
    {
        mesh_vertices.clear();
        tex_ranges.clear();
        const int n = std::min(gbuild.num, static_cast<int>(textures.size())); // 构建过程中纹理可能已改变
        for (int i = 0; i < n; ++i) {
            if (gbuild.buckets[i].empty()) continue;
            tex_ranges.push_back({textures[i], mesh_vertices.size(), gbuild.buckets[i].size()});
            mesh_vertices.insert(mesh_vertices.end(), gbuild.buckets[i].begin(), gbuild.buckets[i].end());
        }
        Log::Info("[lava] 构建地图=%u 地面=1 顶点=%zu 平面=%u 纹理=%d 范围=%zu",
            gbuild.map_id, mesh_vertices.size(), gbuild.n_planes, gbuild.num, tex_ranges.size());
        built_map_id = gbuild.map_id;
        gbuild = {}; // active=false，丢弃桶
    }

    // 为 map_id 开始（或重新开始）增量式地面构建。立即清除当前绘制的网格，以便之前地图的熔岩立即停止显示；
    // 在接下来的几帧中，新地面通过 StepGroundBuild 逐步填充。
    void BeginGroundBuild(const uint32_t map_id)
    {
        const GW::PathingMapArray* pm = GW::Map::GetPathingMap();
        const uint32_t n_planes = pm ? static_cast<uint32_t>(pm->size()) : 0;
        const int num = static_cast<int>(textures.size());
        mesh_vertices.clear();
        tex_ranges.clear();
        built_map_id = 0xFFFFFFFFu;
        gbuild = {};
        if (!n_planes || num == 0) {
            mesh_dirty = true; // 寻路地图尚未加载（仍在加载中）；下一帧重试
            return;
        }
        gbuild.active = true;
        gbuild.map_id = map_id;
        gbuild.n_planes = n_planes;
        gbuild.num = num;
        gbuild.buckets.assign(num, {});
        mesh_dirty = false;
    }

    // 在当前帧的查询预算内推进正在进行的地面构建；绝不会阻塞渲染线程。
    void StepGroundBuild()
    {
        if (!gbuild.active) return;
        const GW::PathingMapArray* pm = GW::Map::GetPathingMap();
        if (!pm || static_cast<uint32_t>(pm->size()) != gbuild.n_planes) {
            // 地图几何体在我们下面改变了（切换地图）— 放弃；DrawInWorld 的地图变更检查会重新开始构建。
            gbuild = {};
            built_map_id = 0xFFFFFFFFu;
            mesh_dirty = true;
            return;
        }
        int queries = 0;
        while (gbuild.plane < gbuild.n_planes) {
            const GW::PathingMap& map = (*pm)[gbuild.plane];
            while (gbuild.trap < map.trapezoid_count) {
                const int q = EmitGroundTrapezoid(map.trapezoids[gbuild.trap], gbuild.plane);
                ++gbuild.trap;
                if (q < 0) { // 达到几何上限；用已有内容完成
                    FinalizeGroundBuild();
                    return;
                }
                queries += q;
                if (queries >= kQueriesPerFrame) return; // 预算用完；下一帧继续
            }
            ++gbuild.plane;
            gbuild.trap = 0;
        }
        FinalizeGroundBuild(); // 所有平面已细分
    }

    void BuildMesh(const uint32_t map_id)
    {
        mesh_vertices.clear();
        tex_ranges.clear();
        mesh_ready = false;
        built_map_id = map_id;
        mesh_dirty = false;

        const GW::PathingMapArray* pm = GW::Map::GetPathingMap();
        const uint32_t n_planes = pm ? static_cast<uint32_t>(pm->size()) : 0;
        if (!n_planes) {
            mesh_dirty = true; // 寻路地图尚未加载；下一帧重试
            return;
        }
        const int num = static_cast<int>(textures.size());
        if (num == 0) {
            Log::Info("[lava] 构建地图=%u：未选择纹理", map_id);
            return;
        }

        std::vector<std::vector<LavaVertex>> buckets(num);
        size_t river_count = 0;
        if (const auto it = rivers_by_map.find(map_id); it != rivers_by_map.end()) {
            river_count = it->second.size();
            for (size_t r = 0; r < it->second.size(); ++r)
                BuildRiverMesh(it->second[r], n_planes, buckets[r % static_cast<size_t>(num)]);
        }
        for (int i = 0; i < num; ++i) {
            if (buckets[i].empty()) continue;
            tex_ranges.push_back({textures[i], mesh_vertices.size(), buckets[i].size()});
            mesh_vertices.insert(mesh_vertices.end(), buckets[i].begin(), buckets[i].end());
        }
        Log::Info("[lava] 构建地图=%u 河流=%zu 顶点=%zu 平面=%u 纹理=%d 范围=%zu",
            map_id, river_count, mesh_vertices.size(), n_planes, num, tex_ranges.size());
    }

    bool EnsureVb(IDirect3DDevice9* device)
    {
        const size_t needed = mesh_vertices.size();
        if (needed == 0) return false;
        if (!mesh_vb || mesh_cap < needed) {
            if (mesh_vb) {
                mesh_vb->Release();
                mesh_vb = nullptr;
            }
            const size_t c = needed + needed / 2; // 预留空间，使小幅编辑不会每帧重新分配
            if (device->CreateVertexBuffer(static_cast<UINT>(c * sizeof(LavaVertex)), D3DUSAGE_WRITEONLY, 0, D3DPOOL_MANAGED, &mesh_vb, nullptr) != D3D_OK) {
                mesh_cap = 0;
                return false;
            }
            mesh_cap = c;
        }
        void* mem = nullptr;
        if (mesh_vb->Lock(0, static_cast<UINT>(needed * sizeof(LavaVertex)), &mem, 0) != D3D_OK || !mem) return false;
        memcpy(mem, mesh_vertices.data(), needed * sizeof(LavaVertex));
        mesh_vb->Unlock();
        return true;
    }

    bool EnsureShaders(IDirect3DDevice9* device)
    {
        if (lava_vs && lava_ps && lava_decl) return true;
        constexpr D3DVERTEXELEMENT9 decl[] = {
            {0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0}, {0, 12, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0}, {0, 16, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0}, D3DDECL_END()
        };
        if (!lava_decl && device->CreateVertexDeclaration(decl, &lava_decl) != D3D_OK) return false;
        if (!lava_vs && device->CreateVertexShader(reinterpret_cast<const DWORD*>(&river_vs), &lava_vs) != D3D_OK) return false;
        if (!lava_ps && device->CreatePixelShader(reinterpret_cast<const DWORD*>(&river_ps), &lava_ps) != D3D_OK) return false;
        return true;
    }
} // namespace

void RiverModule::DrawInWorld(IDirect3DDevice9* device)
{
    if (textures.empty()) return; // 未选择任何内容 → 不绘制

    const uint32_t cur_map = static_cast<uint32_t>(GW::Map::GetMapID());
    if (cover_entire_ground) {
        // 全地面洪泛在一帧内过重，且 QueryAltitude 不能离线运行 — 我们在查询预算下跨帧增量构建，
        // 使得进入地图时渲染线程从不停顿。
        if ((cur_map != built_map_id || mesh_dirty) && (!gbuild.active || gbuild.map_id != cur_map))
            BeginGroundBuild(cur_map);
        if (gbuild.active)
            StepGroundBuild();
    }
    else if (cur_map != built_map_id || mesh_dirty) {
        BuildMesh(cur_map); // 仅创作河流：足够轻量，可一帧完成
    }

    if (mesh_vertices.empty() || tex_ranges.empty()) return;
    mesh_ready = EnsureVb(device);
    if (!mesh_ready || !EnsureShaders(device)) return;

    const DWORD now = GetTickCount();
    float dt = last_tick ? static_cast<float>(now - last_tick) / 1000.f : 0.f;
    last_tick = now;
    dt = std::clamp(dt, 0.f, 0.1f); // 忽略 Alt-Tab / 断点间隙
    t_seconds += dt;

    // 流动方向缓慢漂移；沿该方向累积纹理滚动，使当前方向变化。
    flow_angle += dt * flow_dir_change;
    const float heading = flow_angle + 0.6f * std::sin(t_seconds * 0.05f);
    scroll_u = std::fmod(scroll_u + std::cos(heading) * flow_speed * dt, 1.f);
    scroll_v = std::fmod(scroll_v + std::sin(heading) * flow_speed * dt, 1.f);
    // 缓慢、不可公度的包络，使波浪起伏和缓和，没有明显的周期。
    const float env = std::max(0.15f, 1.f + 0.5f * std::sin(t_seconds * 0.13f) + 0.3f * std::sin(t_seconds * 0.071f));

    const D3DStateGuard state_guard(device); // restored on exit so GW's own rendering isn't corrupted
    if (device->SetVertexShader(lava_vs) == D3D_OK && device->SetPixelShader(lava_ps) == D3D_OK && device->SetVertexDeclaration(lava_decl) == D3D_OK && GameWorldCompositor::SetWorldViewProj(device)) {
        GameWorldCompositor::SetWorldRenderStates(device, GameWorldRenderer::GetOccludeBehindTerrain());
        GameWorldCompositor::SetDistanceFog(device, render_max_distance, fog_factor);
        const float wave_const[4] = {t_seconds, wave_amplitude * env, wave_scale, 0.f};
        device->SetVertexShaderConstantF(8, wave_const, 1);
        const float lava_params[4] = {scroll_u, scroll_v, glow, 0.f};
        device->SetPixelShaderConstantF(3, lava_params, 1);
        device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
        device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
        device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP); // 纹理平铺在地面上
        device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
        if (device->SetStreamSource(0, mesh_vb, 0, sizeof(LavaVertex)) == D3D_OK) {
            // 每个选定纹理一次绘制（每个纹理对应共享顶点缓冲区的连续段）。
            for (const auto& r : tex_ranges) {
                IDirect3DTexture9** pp = GwDatModule::LoadTextureFromFileId(r.file_id);
                IDirect3DTexture9* tex = pp ? *pp : nullptr;
                if (!tex || r.count < 3) continue;
                device->SetTexture(0, tex);
                device->DrawPrimitive(D3DPT_TRIANGLELIST, static_cast<UINT>(r.offset), static_cast<UINT>(r.count / 3));
            }
        }
    }
}

void RiverModule::RegisterSettings(ToolboxModule* module)
{
    SettingsRegistry::RegisterField(module, "render_max_distance", &render_max_distance);
    SettingsRegistry::RegisterField(module, "glow", &glow);
    SettingsRegistry::RegisterField(module, "flow_speed", &flow_speed);
    SettingsRegistry::RegisterField(module, "tile_length", &tile_length);
    SettingsRegistry::RegisterField(module, "z_lift", &z_lift);
    SettingsRegistry::RegisterField(module, "lava_tint", &lava_tint);
    SettingsRegistry::RegisterField(module, "new_river_width", &new_river_width);
    SettingsRegistry::RegisterField(module, "cover_entire_ground", &cover_entire_ground);
    SettingsRegistry::RegisterField(module, "wave_amplitude", &wave_amplitude);
    SettingsRegistry::RegisterField(module, "wave_scale", &wave_scale);
    SettingsRegistry::RegisterField(module, "flow_dir_change", &flow_dir_change);
    SettingsRegistry::RegisterField(module, "cell_size", &cell_size);
}

void RiverModule::LoadSettings(SettingsDoc& doc, ToolboxIni* legacy)
{
    ToolboxModule::LoadSettings(doc, legacy);
    doc.Get(Name(), "rivers", rivers_by_map); // 若键缺失则保留当前（空）地图
    doc.Get(Name(), "textures", textures);
    if (textures.empty()) textures = {0x45478}; // 永远不要持久化为永久空白选择
    mesh_dirty = true;
}

void RiverModule::SaveSettings(SettingsDoc& doc)
{
    ToolboxModule::SaveSettings(doc);
    doc.Set(Name(), "rivers", rivers_by_map);
    doc.Set(Name(), "textures", textures);
}

void RiverModule::DrawSettings()
{
    const auto red = ImGui::ColorConvertU32ToFloat4(Colors::Red());
    const auto green = ImGui::ColorConvertU32ToFloat4(Colors::Green());
    ImGui::TextColored(red, "警告：此为测试功能。");
    if (!GameWorldCompositor::IsActive())
        ImGui::TextColored(red, GameWorldCompositor::HasFailed() ? "  世界合成器安装失败。" : "  世界合成器：尚未安装。");

    ImGui::TextUnformatted("纹理");
    ImGui::ShowHelp("游戏内熔岩纹理（以及水面纹理）。选择一个以获得均匀表面，或选择多个以在约 1024 单位的块中混合使用。\n未选择任何纹理 = 不绘制任何内容。");
    for (const auto& kt : kKnownTextures) {
        const auto it = std::find(textures.begin(), textures.end(), kt.id);
        bool on = it != textures.end();
        ImGui::PushID(static_cast<int>(kt.id));
        if (ImGui::Checkbox(kt.name, &on)) {
            if (on) textures.push_back(kt.id);
            else textures.erase(std::find(textures.begin(), textures.end(), kt.id));
            mesh_dirty = true;
        }
        ImGui::SameLine(0.f, 8.f);
        ImGui::TextDisabled("0x%X", kt.id);
        if (on) {
            IDirect3DTexture9** pp = GwDatModule::LoadTextureFromFileId(kt.id);
            const bool ok = pp && *pp;
            ImGui::SameLine();
            ImGui::TextColored(ok ? green : red, ok ? ICON_FA_CHECK : "加载中/无效");
        }
        ImGui::PopID();
    }
    if (textures.empty()) ImGui::TextColored(red, "未选择纹理 — 将不会绘制任何内容。");

    ImGui::TextDisabled("地形后的遮挡遵循“游戏内渲染”模块的设置。");
    ImGui::DragFloat("最大渲染距离", &render_max_distance, 5.f, 10.f, 100000.f, "%.0f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat("发光强度", &glow, 0.02f, 0.f, 4.f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat("流动速度", &flow_speed, 0.01f, -2.f, 2.f, "%.2f");
    ImGui::DragFloat("流向漂移", &flow_dir_change, 0.005f, 0.f, 1.f, "%.3f 弧度/秒", ImGuiSliderFlags_AlwaysClamp);
    if (ImGui::DragFloat("纹理平铺长度", &tile_length, 4.f, 16.f, 4096.f, "%.0f", ImGuiSliderFlags_AlwaysClamp)) mesh_dirty = true;
    if (ImGui::DragFloat("高度提升", &z_lift, 0.5f, 0.f, 200.f, "%.1f", ImGuiSliderFlags_AlwaysClamp)) mesh_dirty = true;
    auto col = ImGui::ColorConvertU32ToFloat4(lava_tint);
    if (ImGui::ColorEdit4("染色", &col.x, ImGuiColorEditFlags_AlphaBar)) lava_tint = ImGui::ColorConvertFloat4ToU32(col);

    ImGui::Separator();
    ImGui::TextUnformatted("波浪");
    ImGui::DragFloat("波浪幅度", &wave_amplitude, 1.f, 0.f, 500.f, "%.0f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat("波浪尺度", &wave_scale, 0.05f, 0.05f, 8.f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::ShowHelp("波长乘数。越大 = 波越短、越汹涌。");

    ImGui::Separator();
    if (ImGui::Checkbox("覆盖整个地面", &cover_entire_ground)) mesh_dirty = true;
    ImGui::SameLine();
    ImGui::ShowHelp("启用：覆盖整个可行走地面。\n禁用：仅绘制您下方创作的河流。");
    if (cover_entire_ground) {
        if (ImGui::DragFloat("地面细分单元格大小", &cell_size, 5.f, 16.f, 1000.f, "%.0f", ImGuiSliderFlags_AlwaysClamp)) mesh_dirty = true;
        ImGui::ShowHelp("细分单元格（世界单位）。越小 = 波浪越精细，但几何体更多。");
        return;
    }

    const uint32_t map_id = static_cast<uint32_t>(GW::Map::GetMapID());
    ImGui::Text("此地图上的河流 (id %u)", map_id);
    auto& rivers = rivers_by_map[map_id];
    if (active_river >= static_cast<int>(rivers.size())) active_river = -1;

    int to_remove = -1;
    for (int i = 0; i < static_cast<int>(rivers.size()); i++) {
        auto& r = rivers[i];
        ImGui::PushID(i);
        const bool is_active = (i == active_river);
        if (ImGui::RadioButton("##active", is_active)) active_river = is_active ? -1 : i;
        ImGui::SameLine();
        if (ImGui::CollapsingHeader((std::string("河流 ") + std::to_string(i) + " (" + std::to_string(r.points.size()) + " 个点)").c_str())) {
            if (ImGui::DragFloat("宽度", &r.width, 5.f, 10.f, 5000.f, "%.0f", ImGuiSliderFlags_AlwaysClamp)) mesh_dirty = true;
            if (ImGui::SmallButton("撤销最后一点") && !r.points.empty()) {
                r.points.pop_back();
                mesh_dirty = true;
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("删除河流")) to_remove = i;
        }
        ImGui::PopID();
    }
    if (to_remove >= 0) {
        rivers.erase(rivers.begin() + to_remove);
        if (active_river == to_remove) active_river = -1;
        mesh_dirty = true;
    }

    if (ImGui::Button("新建河流")) {
        rivers.push_back({{}, new_river_width});
        active_river = static_cast<int>(rivers.size()) - 1;
    }
    ImGui::SameLine();
    const bool can_add = active_river >= 0 && active_river < static_cast<int>(rivers.size());
    ImGui::BeginDisabled(!can_add);
    if (ImGui::Button("在玩家位置添加点")) {
        if (const auto* me = GW::Agents::GetControlledCharacter()) {
            rivers[active_river].points.push_back({me->pos.x, me->pos.y});
            mesh_dirty = true;
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::DragFloat("新河流宽度", &new_river_width, 5.f, 10.f, 5000.f, "%.0f", ImGuiSliderFlags_AlwaysClamp);
    if (!can_add) ImGui::TextDisabled("选择一个河流（单选按钮）或“新建河流”，然后行走并点击“在玩家位置添加点”。");
}

void RiverModule::Initialize()
{
    ToolboxModule::Initialize();
    RegisterSettings(this);
    if (!compositor_token) compositor_token = GameWorldCompositor::RegisterDraw(&RiverModule::DrawInWorld);
}

void RiverModule::DrawSettingsInternal()
{
    DrawSettings();
}

void RiverModule::SignalTerminate()
{
    if (compositor_token) {
        GameWorldCompositor::UnregisterDraw(compositor_token);
        compositor_token = 0;
    }
}

void RiverModule::Terminate()
{
    SignalTerminate();
    if (mesh_vb) {
        mesh_vb->Release();
        mesh_vb = nullptr;
    }
    if (lava_vs) {
        lava_vs->Release();
        lava_vs = nullptr;
    }
    if (lava_ps) {
        lava_ps->Release();
        lava_ps = nullptr;
    }
    if (lava_decl) {
        lava_decl->Release();
        lava_decl = nullptr;
    }
    mesh_cap = 0;
    mesh_vertices.clear();
    tex_ranges.clear();
    mesh_ready = false;
    mesh_dirty = true;
    built_map_id = 0xFFFFFFFFu;
    ToolboxModule::Terminate();
}
