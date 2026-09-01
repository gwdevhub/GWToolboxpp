#include "stdafx.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/GameContainers/GamePos.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Camera.h>
#include <GWCA/GameEntities/Item.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/CameraMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/ItemMgr.h>
#include <GWCA/Managers/MapMgr.h>

#include <Color.h>
#include <D3DContainers.h>
#include <Defines.h>
#include <ImGuiAddons.h>
#include <Modules/GwDatModule.h>
#include <Modules/LootBeaconsModule.h>
#include <Modules/PriceCheckerModule.h>
#include <Utils/EncString.h>
#include <Utils/GameWorldCompositor.h>
#include <Utils/SettingsRegistry.h>
#include <Utils/TerrainDrape.h>
#include <Utils/TextUtils.h>
#include <Utils/ToolboxUtils.h>

// 由 CMake（fxc）从共享着色器目录中的 .hlsl 生成。
#include "Widgets/Minimap/Shaders/loot_beacon_ring_ps.h"
#include "Widgets/Minimap/Shaders/loot_beacon_ring_vs.h"

// User-defined rules matched against the decoded item name. Must be at file scope (not anonymous
// namespace) so glaze's reflection can create the required external-linkage template specialisation.
struct NameBeacon {
    std::string match;
    Colors::SettingColor color = Colors::ARGB(170, 0, 255, 255);
    bool enabled = true;
};

namespace {
    constexpr int kMaxBuildsPerFrame = 4;   // 每帧地形高度场构建次数上限
    constexpr int kDrapeGrid = 16;          // 在信标覆盖区域内采样的高度场分辨率
    constexpr int kRingDivs = 16;           // 环四边形细分，使精灵贴合地面
    constexpr uint32_t kScanIntervalMs = 250; // 物品单位不会移动；分类只需粗粒度轮询
    constexpr uint32_t kRingTextureFileId = 0x2381; // GW dat 中脉冲环精灵的纹理

    // Not user-configurable.
    constexpr float kRingSpacing = 4.f;    // gwinches, how far the two rings start from the true diameter
    constexpr float kRingDiameter = 90.f;  // the true diameter the two rings pulse toward/away from
    constexpr float kTrueRingRadius = kRingDiameter * 0.5f;
    constexpr float kFieldRadius = kRingDiameter * 0.5f + kRingSpacing; // footprint half-extent = the largest ring radius
    constexpr float kBeamWidth = 35.f;     // beam width
    constexpr float kBeamOpacity = 1.f;    // beam is unpulsed and always drawn at full opacity
    constexpr float kPulseInterval = 0.85f; // seconds for the two rings to go from spread apart to meeting in the middle
    constexpr float kNoDistanceLimit = 1e9f;

    float beam_height = 225.f;
    bool show_reserved_for_others = false;

    struct ValueBeacon {
        const char* label;
        bool enabled;
        int threshold; // 金币，基于 Kamadan 交易价格
        Color color;
    };
    ValueBeacon value_low = {"value_low", true, 1000, Colors::ARGB(50, 255, 140, 0)};
    ValueBeacon value_high = {"value_high", true, 15000, Colors::ARGB(170, 255, 140, 0)};

    struct RarityBeacon {
        const char* label;
        bool enabled;
    };
    RarityBeacon rarity_white = {"White items", false};
    RarityBeacon rarity_blue = {"Blue items", false};
    RarityBeacon rarity_purple = {"Purple items", true};
    RarityBeacon rarity_gold = {"Gold items", true};
    RarityBeacon rarity_green = {"Green items", true};

    std::vector<NameBeacon> name_beacons;

    struct CachedName {
        GuiUtils::EncString enc;
        std::wstring decoded;
    };
    std::map<std::wstring, CachedName> decoded_item_names; // keyed by the encoded name

    // Decoded item name, or nullptr while the async decode is still pending (a scan tick or two).
    const std::wstring* DecodedItemName(const GW::Item& item)
    {
        const wchar_t* name_enc = nullptr;
        if (item.single_item_name && *item.single_item_name) name_enc = item.single_item_name;
        else if (item.name_enc && *item.name_enc) name_enc = item.name_enc;
        if (!name_enc) return nullptr;
        auto& cached = decoded_item_names[name_enc];
        if (cached.decoded.empty()) {
            cached.decoded = cached.enc.reset(name_enc)->wstring();
            if (cached.decoded.empty()) return nullptr;
        }
        return &cached.decoded;
    }

    // Parsed form of the enabled rules; rebuilt only when the list changes, since building a regex isn't free.
    struct CompiledNameBeacon {
        TextUtils::SearchPattern<wchar_t> pattern;
        Color color = 0;
    };
    std::vector<CompiledNameBeacon> compiled_name_beacons;
    std::vector<size_t> invalid_name_beacons; // rules whose regex didn't parse, flagged in the settings panel
    bool name_beacons_dirty = true;

    void CompileNameBeacons()
    {
        name_beacons_dirty = false;
        compiled_name_beacons.clear();
        invalid_name_beacons.clear();
        for (size_t i = 0; i < name_beacons.size(); i++) {
            const auto& name_beacon = name_beacons[i];
            if (!name_beacon.enabled || name_beacon.match.empty()) continue;
            TextUtils::SearchPattern<wchar_t> pattern(TextUtils::StringToWString(name_beacon.match));
            if (!pattern.IsValid()) {
                invalid_name_beacons.push_back(i);
                continue;
            }
            compiled_name_beacons.emplace_back(std::move(pattern), name_beacon.color.value);
        }
    }

    constexpr int kRarityBeaconAlpha = 170; // beam/ring alpha; the palette itself is opaque (0xFF)

    // A rarity beacon isn't user-colourable: it always takes the exact hue the client paints the
    // item's name (GW::Chat::TextColor), so the beacon matches the nametag drawn for that drop.
    Color RarityBeaconColor(const GW::Constants::Rarity rarity)
    {
        uint32_t argb; // palette entries are 0xAARRGGBB
        switch (rarity) {
            case GW::Constants::Rarity::White: argb = GW::Chat::TextColor::ColorItemCommon; break;
            case GW::Constants::Rarity::Blue: argb = GW::Chat::TextColor::ColorItemEnhance; break;
            case GW::Constants::Rarity::Purple: argb = GW::Chat::TextColor::ColorItemUncommon; break;
            case GW::Constants::Rarity::Gold: argb = GW::Chat::TextColor::ColorItemRare; break;
            case GW::Constants::Rarity::Green: argb = GW::Chat::TextColor::ColorItemUnique; break;
            default: return 0;
        }
        return Colors::ARGB(kRarityBeaconAlpha, (argb >> 16) & 0xFF, (argb >> 8) & 0xFF, argb & 0xFF);
    }

    struct BeaconVertex {
        float x, y, z;
        DWORD color;
    };

    // 位置（世界坐标）+ D3DCOLOR 颜色 + UV，用于纹理环四边形（0x03F2F1BB）。
    struct RingVertex {
        float x, y, z;
        DWORD color;
        float u, v;
    };

    struct Beacon {
        GW::Vec2f pos;
        float z = 0.f; // 物品单位自身的世界高度；GW 的向上方向为 -z
        uint32_t zplane = 0;
        Color color = 0;
        bool draw = false;
        bool dimmed = false;
        bool draped = false; // heightfield resolved once (items don't move), then sampled every frame
        bool ring_cached = false;
        uint32_t seen = 0;
        float field[kDrapeGrid + 1][kDrapeGrid + 1] = {}; // terrain z sampled across pos +/- kFieldRadius
        std::vector<RingVertex> ring_outer, ring_inner;
    };

    std::unordered_map<uint32_t, Beacon> beacons;
    std::vector<BeaconVertex> scratch;
    std::vector<RingVertex> ring_scratch;
    uint32_t scan_counter = 0;
    uint64_t last_scan_tick = 0;
    bool beacons_dirty = false;
    int compositor_token = 0;

    IDirect3DVertexShader9* ring_vs = nullptr;
    IDirect3DPixelShader9* ring_ps = nullptr;
    IDirect3DVertexDeclaration9* ring_decl = nullptr;
    IDirect3DTexture9** ring_tex_pp = nullptr;
    bool ring_texture_requested = false;

    DWORD WithAlpha(const Color color, const float alpha_factor)
    {
        const auto base_a = static_cast<float>((color >> IM_COL32_A_SHIFT) & 0xFF);
        const auto a = static_cast<DWORD>(std::clamp(base_a * alpha_factor, 0.f, 255.f));
        return (color & ~(0xFFu << IM_COL32_A_SHIFT)) | (a << IM_COL32_A_SHIFT);
    }

    bool EnsureRingShaders(IDirect3DDevice9* device)
    {
        if (ring_vs && ring_ps && ring_decl) return true;
        constexpr D3DVERTEXELEMENT9 decl[] = {
            {0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
            {0, 12, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0},
            {0, 16, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
            D3DDECL_END()
        };
        if (!ring_decl && device->CreateVertexDeclaration(decl, &ring_decl) != D3D_OK) return false;
        if (!ring_vs && device->CreateVertexShader(reinterpret_cast<const DWORD*>(&loot_beacon_ring_vs), &ring_vs) != D3D_OK) return false;
        if (!ring_ps && device->CreatePixelShader(reinterpret_cast<const DWORD*>(&loot_beacon_ring_ps), &ring_ps) != D3D_OK) return false;
        return true;
    }

    // 在高度场中双线性采样世界坐标 (wx, wy)；超出覆盖范围则钳制到边缘。
    float SampleDrape(const Beacon& beacon, const float wx, const float wy)
    {
        const float fx = std::clamp(((wx - beacon.pos.x) / kFieldRadius * 0.5f + 0.5f) * kDrapeGrid, 0.f, static_cast<float>(kDrapeGrid));
        const float fy = std::clamp(((wy - beacon.pos.y) / kFieldRadius * 0.5f + 0.5f) * kDrapeGrid, 0.f, static_cast<float>(kDrapeGrid));
        const int x0 = std::min(static_cast<int>(fx), kDrapeGrid - 1);
        const int y0 = std::min(static_cast<int>(fy), kDrapeGrid - 1);
        const float tx = fx - static_cast<float>(x0);
        const float ty = fy - static_cast<float>(y0);
        const float z0 = std::lerp(beacon.field[x0][y0], beacon.field[x0 + 1][y0], tx);
        const float z1 = std::lerp(beacon.field[x0][y0 + 1], beacon.field[x0 + 1][y0 + 1], tx);
        return std::lerp(z0, z1, ty);
    }

    // 在信标覆盖区域解析一次可见表面，使用物品自身高度来选择路径层
    //（因此多层地图会贴合物品实际所在的楼层）。
    void BuildDrape(Beacon& beacon, const uint32_t n_planes)
    {
        for (int i = 0; i <= kDrapeGrid; ++i) {
            const float wx = beacon.pos.x + (static_cast<float>(i) / kDrapeGrid * 2.f - 1.f) * kFieldRadius;
            for (int j = 0; j <= kDrapeGrid; ++j) {
                const float wy = beacon.pos.y + (static_cast<float>(j) / kDrapeGrid * 2.f - 1.f) * kFieldRadius;
                const float z = TerrainDrape::SurfaceZ(wx, wy, beacon.zplane, n_planes);
                beacon.field[i][j] = z ? z : beacon.z;
            }
        }
        beacon.draped = true;
    }

    // 追加一个纹理环四边形，细分为 kRingDivs 网格，每个顶点贴合缓存的地面
    // 使精灵贴合地形而非在斜坡上浮空。
    void EmitDrapedRing(std::vector<RingVertex>& out, const Beacon& beacon, const float radius, const DWORD col)
    {
        const float r = std::max(1.f, radius);
        RingVertex grid[kRingDivs + 1][kRingDivs + 1];
        for (int i = 0; i <= kRingDivs; ++i) {
            const float u = static_cast<float>(i) / kRingDivs;
            const float wx = beacon.pos.x + (u * 2.f - 1.f) * r;
            for (int j = 0; j <= kRingDivs; ++j) {
                const float v = static_cast<float>(j) / kRingDivs;
                const float wy = beacon.pos.y + (v * 2.f - 1.f) * r;
                grid[i][j] = {wx, wy, SampleDrape(beacon, wx, wy), col, u, 1.f - v};
            }
        }
        for (int i = 0; i < kRingDivs; ++i) {
            for (int j = 0; j < kRingDivs; ++j) {
                const RingVertex& a = grid[i][j];
                const RingVertex& b = grid[i + 1][j];
                const RingVertex& c = grid[i + 1][j + 1];
                const RingVertex& d = grid[i][j + 1];
                out.push_back(a);
                out.push_back(b);
                out.push_back(c);
                out.push_back(a);
                out.push_back(c);
                out.push_back(d);
            }
        }
    }

    void BuildRingCache(Beacon& beacon)
    {
        constexpr DWORD placeholder_col = 0xFFFFFFFF;
        EmitDrapedRing(beacon.ring_outer, beacon, kTrueRingRadius, placeholder_col);
        EmitDrapedRing(beacon.ring_inner, beacon, kTrueRingRadius, placeholder_col);
        beacon.ring_cached = true;
    }

    void EmitCachedRing(std::vector<RingVertex>& out, const std::vector<RingVertex>& cache, const Beacon& beacon, const float radius, const DWORD col)
    {
        const float factor = radius / kTrueRingRadius;
        for (const RingVertex& v : cache) {
            out.push_back({beacon.pos.x + (v.x - beacon.pos.x) * factor,
                           beacon.pos.y + (v.y - beacon.pos.y) * factor,
                           beacon.z + (v.z - beacon.z) * factor,
                           col, v.u, v.v});
        }
    }

    void Classify(const GW::AgentItem& agent_item, const GW::Item& item, const uint32_t my_agent_id, Beacon& beacon)
    {
        const bool mine = !agent_item.owner || agent_item.owner == my_agent_id;
        beacon.dimmed = !mine;
        Color color = 0;
        bool draw = false;
        if (mine || show_reserved_for_others) {
            // Name rules outrank value and rarity, and the first one that matches wins - so the user
            // can order the list by priority.
            if (const auto* item_name = compiled_name_beacons.empty() ? nullptr : DecodedItemName(item)) {
                for (const auto& name_beacon : compiled_name_beacons) {
                    if (!name_beacon.pattern.Matches(*item_name)) continue;
                    beacon.color = name_beacon.color;
                    beacon.draw = true;
                    return;
                }
            }
            const uint32_t price = PriceCheckerModule::GetPriceByItem(&item);
            const ValueBeacon* by_value = nullptr;
            for (const auto* value : {&value_low, &value_high}) {
                if (value->enabled && value->threshold > 0 && price >= static_cast<uint32_t>(value->threshold)) {
                    if (!by_value || value->threshold > by_value->threshold) by_value = value;
                }
            }
            if (by_value) {
                color = by_value->color;
                draw = true;
            }
            else {
                const auto rarity = GW::Items::GetRarity(&item);
                const RarityBeacon* by_rarity = nullptr;
                switch (rarity) {
                    case GW::Constants::Rarity::White: by_rarity = &rarity_white; break;
                    case GW::Constants::Rarity::Blue: by_rarity = &rarity_blue; break;
                    case GW::Constants::Rarity::Purple: by_rarity = &rarity_purple; break;
                    case GW::Constants::Rarity::Gold: by_rarity = &rarity_gold; break;
                    case GW::Constants::Rarity::Green: by_rarity = &rarity_green; break;
                    default: break;
                }
                if (by_rarity && by_rarity->enabled) {
                    color = RarityBeaconColor(rarity);
                    draw = true;
                }
            }
        }
        beacon.color = color;
        beacon.draw = draw;
    }

    void ScanItems()
    {
        ++scan_counter;
        const auto* agents = GW::Agents::GetAgentArray();
        if (!agents) {
            beacons.clear();
            return;
        }
        const auto my_agent_id = GW::Agents::GetControlledCharacterId();
        if (name_beacons_dirty) CompileNameBeacons();
        for (const auto* agent : *agents) {
            const auto* agent_item = agent ? agent->GetAsAgentItem() : nullptr;
            if (!agent_item) continue;
            const auto* item = GW::Items::GetItemById(agent_item->item_id);
            if (!item) continue;
            auto& beacon = beacons[agent_item->agent_id];
            beacon.seen = scan_counter;
            beacon.pos = {agent_item->pos.x, agent_item->pos.y};
            beacon.z = agent_item->z;
            beacon.zplane = agent_item->pos.zplane;
            Classify(*agent_item, *item, my_agent_id, beacon);
        }
        std::erase_if(beacons, [](const auto& entry) { return entry.second.seen != scan_counter; });
    }

    constexpr float kBeamSolidFraction = 0.25f; // 光束底部保持完全不透明的部分

    void EmitBeamQuad(std::vector<BeaconVertex>& out, const GW::Vec2f& pos, const float ground_z, const float right_x, const float right_y, const Color base_color, const float base_alpha)
    {
        const float half = kBeamWidth * 0.5f;
        const float height = beam_height;
        const float z_solid = ground_z - height * kBeamSolidFraction;
        const float z_top = ground_z - height;

        const DWORD col_full = WithAlpha(base_color, base_alpha);
        const DWORD col_zero = WithAlpha(base_color, 0.f);

        struct GridVert {
            float x, y, z;
            DWORD color;
        };
        const auto at = [&](const float t, const float z, const DWORD color) {
            return GridVert{pos.x + right_x * half * t, pos.y + right_y * half * t, z, color};
        };
        // 列：左边缘（t=-1）、中心（t=0）、右边缘（t=1）- 只有中心携带 alpha，
        // 因此每一行在两端都渐变为透明。
        const GridVert row_base[3] = {at(-1.f, ground_z, col_zero), at(0.f, ground_z, col_full), at(1.f, ground_z, col_zero)};
        const GridVert row_solid[3] = {at(-1.f, z_solid, col_zero), at(0.f, z_solid, col_full), at(1.f, z_solid, col_zero)};
        const GridVert row_top[3] = {at(-1.f, z_top, col_zero), at(0.f, z_top, col_zero), at(1.f, z_top, col_zero)};

        const auto quad = [&](const GridVert& a0, const GridVert& a1, const GridVert& b0, const GridVert& b1) {
            const BeaconVertex v00{a0.x, a0.y, a0.z, a0.color}, v10{a1.x, a1.y, a1.z, a1.color};
            const BeaconVertex v01{b0.x, b0.y, b0.z, b0.color}, v11{b1.x, b1.y, b1.z, b1.color};
            out.push_back(v00);
            out.push_back(v10);
            out.push_back(v11);
            out.push_back(v00);
            out.push_back(v11);
            out.push_back(v01);
        };
        // row_base -> row_solid 两端 alpha 相同（保持实体）；row_solid -> row_top 渐变为零。
        quad(row_base[0], row_base[1], row_solid[0], row_solid[1]);
        quad(row_base[1], row_base[2], row_solid[1], row_solid[2]);
        quad(row_solid[0], row_solid[1], row_top[0], row_top[1]);
        quad(row_solid[1], row_solid[2], row_top[1], row_top[2]);
    }

    // 垂直于摄像机视线方向的水平轴，用于垂直（直立）公告板 -
    // world_up 为 (0,0,-1)，因为 GW 的向上方向为 -z，所以 right = cross(world_up, fwd) 简化为 (fwd.y, -fwd.x)。
    void GetCameraRight(float& right_x, float& right_y)
    {
        right_x = 1.f;
        right_y = 0.f;
        const auto* cam = GW::CameraMgr::GetCamera();
        if (!cam) return;
        const float fx = cam->look_at_target.x - cam->position.x;
        const float fy = cam->look_at_target.y - cam->position.y;
        const float len = std::sqrt(fx * fx + fy * fy);
        if (len < 0.0001f) return;
        right_x = fy / len;
        right_y = -fx / len;
    }
} // namespace

void LootBeaconsModule::DrawInWorld(IDirect3DDevice9* device)
{
    if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Explorable) {
        beacons.clear();
        return;
    }
    if (beacons_dirty) {
        last_scan_tick = 0;
        beacons_dirty = false;
    }

    const auto now = GetTickCount64();
    if (now - last_scan_tick >= kScanIntervalMs) {
        last_scan_tick = now;
        ScanItems();
    }
    if (beacons.empty()) return;

    if (!ring_texture_requested) {
        ring_tex_pp = GwDatModule::LoadTextureFromFileId(kRingTextureFileId);
        ring_texture_requested = true;
    }

    const auto n_planes = TerrainDrape::PathingPlaneCount();
    const float t_seconds = static_cast<float>(now % 3600000) / 1000.f;

    // 所有信标共享同一个计时器 - 全局间隔，而非每个信标独立，
    // 因此环半径是每帧单一值，而非每个顶点数据。光束完全不脉动，只有环脉动。
    const float cycle = t_seconds * (DirectX::XM_PIDIV2 / kPulseInterval);
    const float osc01 = 0.5f - 0.5f * std::cos(cycle);
    const float true_radius = kRingDiameter * 0.5f;
    const float r_min = std::max(1.f, true_radius - kRingSpacing);
    const float r_max = true_radius + kRingSpacing;
    const float r_outer = std::lerp(r_max, r_min, osc01);
    const float r_inner = std::lerp(r_min, r_max, osc01);

    float right_x, right_y;
    GetCameraRight(right_x, right_y);

    const auto* cam = GW::CameraMgr::GetCamera();
    const float focus_x = cam ? cam->look_at_target.x : 0.f;
    const float focus_y = cam ? cam->look_at_target.y : 0.f;

    scratch.clear();
    ring_scratch.clear();
    int builds = 0;
    for (auto& [id, beacon] : beacons) {
        if (!beacon.draw) continue;
        const float focus_dx = beacon.pos.x - focus_x;
        const float focus_dy = beacon.pos.y - focus_y;
        if (cam && focus_dx * focus_dx + focus_dy * focus_dy > GW::Constants::SqrRange::Compass) continue;
        if (!beacon.draped) {
            if (!n_planes || builds >= kMaxBuildsPerFrame) continue;
            ++builds;
            BuildDrape(beacon, n_planes);
        }
        if (!beacon.ring_cached) BuildRingCache(beacon);
        EmitBeamQuad(scratch, beacon.pos, beacon.z, right_x, right_y, beacon.color, beacon.dimmed ? kBeamOpacity * 0.4f : kBeamOpacity);

        // 环透明度直接来自信标颜色的 alpha 通道；变暗（保留给其他队伍成员）是唯一的例外，与光束相同。
        const DWORD ring_col = beacon.dimmed ? WithAlpha(beacon.color, 0.4f) : beacon.color;
        EmitCachedRing(ring_scratch, beacon.ring_outer, beacon, r_outer, ring_col);
        EmitCachedRing(ring_scratch, beacon.ring_inner, beacon, r_inner, ring_col);
    }

    if (!scratch.empty()) {
        const D3DStateGuard state_guard(device);
        // Static depth keeps walls/props occluding overlays; agents draw later in GW's pass.
        if (GameWorldCompositor::SetupPipeline(device, true, kNoDistanceLimit, 0.f)) {
            constexpr BOOL dotted_off[1] = {FALSE};
            device->SetPixelShaderConstantB(0, dotted_off, 1);
            device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, static_cast<UINT>(scratch.size() / 3), scratch.data(), sizeof(BeaconVertex));
        }
    }

    IDirect3DTexture9* ring_tex = ring_tex_pp ? *ring_tex_pp : nullptr;
    if (!ring_scratch.empty() && ring_tex && EnsureRingShaders(device)) {
        const D3DStateGuard state_guard(device);
        if (device->SetVertexShader(ring_vs) == D3D_OK && device->SetPixelShader(ring_ps) == D3D_OK && device->SetVertexDeclaration(ring_decl) == D3D_OK &&
            GameWorldCompositor::SetWorldViewProj(device)) {
            // Static depth keeps walls/props occluding overlays; agents draw later in GW's pass.
            GameWorldCompositor::SetWorldRenderStates(device, true);
            constexpr float slope_bias = -1.5f;
            constexpr float const_bias = -1e-5f;
            device->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS, *reinterpret_cast<const DWORD*>(&slope_bias));
            device->SetRenderState(D3DRS_DEPTHBIAS, *reinterpret_cast<const DWORD*>(&const_bias));
            GameWorldCompositor::SetDistanceFog(device, kNoDistanceLimit, 0.f);
            device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
            device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
            device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
            device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
            device->SetTexture(0, ring_tex);
            device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, static_cast<UINT>(ring_scratch.size() / 3), ring_scratch.data(), sizeof(RingVertex));
        }
    }
}

void LootBeaconsModule::RegisterSettings(ToolboxModule* module)
{
    SettingsRegistry::RegisterField(module, "beam_height", &beam_height);
    SettingsRegistry::RegisterField(module, "show_reserved_for_others", &show_reserved_for_others);
    SettingsRegistry::RegisterField(module, "value_low_enabled", &value_low.enabled);
    SettingsRegistry::RegisterField(module, "value_low_threshold", &value_low.threshold);
    SettingsRegistry::RegisterField(module, "value_low_color", &value_low.color);
    SettingsRegistry::RegisterField(module, "value_high_enabled", &value_high.enabled);
    SettingsRegistry::RegisterField(module, "value_high_threshold", &value_high.threshold);
    SettingsRegistry::RegisterField(module, "value_high_color", &value_high.color);
    SettingsRegistry::RegisterField(module, "white_enabled", &rarity_white.enabled);
    SettingsRegistry::RegisterField(module, "blue_enabled", &rarity_blue.enabled);
    SettingsRegistry::RegisterField(module, "purple_enabled", &rarity_purple.enabled);
    SettingsRegistry::RegisterField(module, "gold_enabled", &rarity_gold.enabled);
    SettingsRegistry::RegisterField(module, "green_enabled", &rarity_green.enabled);
}

void LootBeaconsModule::Initialize()
{
    ToolboxModule::Initialize();
    RegisterSettings(this);
    if (!compositor_token) compositor_token = GameWorldCompositor::RegisterDraw(&LootBeaconsModule::DrawInWorld);
}

void LootBeaconsModule::SignalTerminate()
{
    if (compositor_token) {
        GameWorldCompositor::UnregisterDraw(compositor_token);
        compositor_token = 0;
    }
    beacons.clear();
    decoded_item_names.clear();
}

void LootBeaconsModule::LoadSettings(SettingsDoc& doc, ToolboxIni* legacy)
{
    ToolboxModule::LoadSettings(doc, legacy);
    doc.Get(Name(), VAR_NAME(name_beacons), name_beacons);
    beacons_dirty = true;
    name_beacons_dirty = true;
}

void LootBeaconsModule::SaveSettings(SettingsDoc& doc)
{
    ToolboxModule::SaveSettings(doc);
    doc.Set(Name(), VAR_NAME(name_beacons), name_beacons);
}

void LootBeaconsModule::DrawSettingsInternal()
{
    const auto red = ImGui::ColorConvertU32ToFloat4(Colors::Red());
    if (!GameWorldCompositor::IsActive())
        ImGui::TextColored(red, GameWorldCompositor::HasFailed() ? "世界内合成器安装失败。" : "世界内合成器：尚未安装。");

    ImGui::TextUnformatted("Beacon by gold value");
    ImGui::ShowHelp("Any drop whose trader price (Kamadan, or presearing.com's price sheet while pre-searing) meets a threshold gets a beacon,\nregardless of rarity - catches ectos, gemstones, dyes and other white-rarity valuables.\nAn item that clears both thresholds uses the higher tier's colour.");
    for (auto* value : {&value_low, &value_high}) {
        ImGui::PushID(value->label);
        if (ImGui::Checkbox("##enabled", &value->enabled)) beacons_dirty = true;
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100.f);
        ImGui::DragInt("##threshold", &value->threshold, 50.f, 0, 1000000);
        ImGui::SameLine(180.f);
        Colors::DrawSettingHueWheel("##color", &value->color);
        ImGui::PopID();
    }
    ImGui::Separator();
    ImGui::TextUnformatted("Beacon by item name");
    ImGui::ShowHelp("Any drop whose name contains the text gets a beacon in the colour next to it, whatever its rarity or value.\n"
                    "Matching ignores case and uses the item name as it's shown in-game, e.g. \"scroll\" or \"glob of ectoplasm\".\n"
                    "Text wrapped in slashes is a regular expression instead, e.g. /^Superb Charr Carving$/ - add flags after\n"
                    "the closing slash as in the chat filter (I turns case sensitivity back on).\n"
                    "Name rules are checked first; the first matching rule in this list wins.");
    if (name_beacons_dirty) CompileNameBeacons();
    for (size_t i = 0; i < name_beacons.size(); i++) {
        auto& name_beacon = name_beacons[i];
        ImGui::PushID(static_cast<int>(i));
        bool changed = ImGui::Checkbox("##enabled", &name_beacon.enabled);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(160.f);
        changed |= ImGui::InputText("##match", name_beacon.match, 64);
        ImGui::SameLine(240.f);
        changed |= Colors::DrawSettingHueWheel("##color", &name_beacon.color.value);
        ImGui::SameLine();
        const bool remove = ImGui::Button("x##delete");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Delete");
        if (std::ranges::contains(invalid_name_beacons, i)) {
            ImGui::SameLine();
            ImGui::TextColored(red, "Invalid regex");
        }
        ImGui::PopID();
        if (changed) {
            beacons_dirty = true;
            name_beacons_dirty = true;
        }
        if (remove) {
            name_beacons.erase(name_beacons.begin() + static_cast<int>(i));
            beacons_dirty = true;
            name_beacons_dirty = true;
            break;
        }
    }
    if (ImGui::Button("Add item name")) {
        name_beacons.emplace_back();
        beacons_dirty = true;
        name_beacons_dirty = true;
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Beacon by rarity");
    ImGui::ShowHelp("The beacon colour always matches the item's name tag, so it can't be changed here.");
    for (auto* rarity : {&rarity_gold, &rarity_green, &rarity_purple, &rarity_blue, &rarity_white}) {
        if (ImGui::Checkbox(rarity->label, &rarity->enabled)) beacons_dirty = true;
    }
    if (ImGui::Checkbox("在保留给其他队伍成员的物品上显示信标", &show_reserved_for_others)) beacons_dirty = true;
    ImGui::ShowHelp("以变暗方式绘制。关闭：仅显示未保留的掉落物和分配给您自己的掉落物。");

    ImGui::Separator();
    ImGui::DragFloat("光束高度", &beam_height, 5.f, 10.f, 2000.f, "%.0f", ImGuiSliderFlags_AlwaysClamp);
}
