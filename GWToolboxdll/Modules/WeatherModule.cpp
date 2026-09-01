#include "stdafx.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/Constants/Maps.h>
#include <GWCA/GameContainers/GamePos.h>
#include <GWCA/GameEntities/Camera.h>
#include <GWCA/GameEntities/Map.h>
#include <GWCA/Managers/CameraMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/RenderMgr.h>

#include <Color.h>
#include <Defines.h>
#include <ImGuiAddons.h>
#include <Logger.h>
#include <Modules/AudioSettings.h>
#include <Modules/GwDatModule.h>
#include <Modules/Resources.h>
#include <Modules/WeatherModule.h>
#include <Timer.h>
#include <Utils/ArenaNetFileParser.h>
#include <Utils/GameWorldCompositor.h>
#include <Utils/GuiUtils.h>
#include <Utils/SettingsDoc.h>
#include <Utils/TerrainDrape.h>
#include <Utils/SettingsRegistry.h>
#include <Utils/TextUtils.h>
#include <Widgets/Minimap/GameWorldRenderer.h>

// 由 CMake (fxc) 从共享着色器目录中的 .hlsl 生成。
#include <GWCA/Context/MapContext.h>
#include <Utils/ToolboxUtils.h>
#include "Widgets/Minimap/Shaders/weather_billboard_ps.h"
#include "Widgets/Minimap/Shaders/weather_billboard_vs.h"
#include "Widgets/Minimap/Shaders/weather_instanced_vs.h"

// 外部链接（非匿名命名空间）以便 glaze 在（反）序列化天气条件列表时将其反射为向量元素。
// 普通聚合体，因此无需 glz::meta 即可进行反射。
namespace weather_module {
    constexpr int kTypeRain = 0; // 天气类型（下落粒子）
    constexpr int kTypeSnow = 1;
    constexpr int kTypeCount = 2;

    constexpr int kDecalNone = 0; // 每个撞击点留在地面上的贴花
    constexpr int kDecalSplash = 1;
    constexpr int kDecalSettle = 2;
    constexpr int kDecalCount = 3;
    constexpr int kDecalAuto = -1;     // 未设置：从类型派生（雨 -> 溅射，雪 -> 沉积）
    constexpr float kDriftAuto = -1.f; // 漂移未设置：从类型派生（雪横向飘动，雨不飘动）

    struct CloudCover {
        float base = 0.f;                // 云带底部，玩家上方 gwinch 数
        float top = 0.f;                 // 云带顶部；仅当 top > base 时激活
        unsigned int tint = 0x80808080u; // 云的颜色 + 不透明度（ImGui 打包；alpha 是云层的整体密度）
        int density = 20;                // 云团数量驱动（类似粒子密度百分比）
        float size = 600.f;              // 云团公告板大小（云很大且柔和）
        float speed = 0.f;               // 水平漂移速度；方向遵循条件的 Wind 方向
        float radius = 2500.f;           // 云层的水平半径，独立于粒子的 Range
    };

    struct WeatherCondition {
        std::string name = "Rain";
        int type = kTypeRain;
        bool active = false;
        int density = 30; // 粒子密度 1..100%；实际数量由此和体积面积派生
        float drop_size = 8.f;
        float fall_speed = 2000.f;    // gwinch/秒
        float spread_radius = 2500.f; // 以玩家为中心的体积半径（水平环绕 + 柱高）；加载时固定
        float wind_dir_min = 0.f; // 保留但未使用：风向始终在 0-360 度全圆上滚动
        float wind_dir_max = 0.f; //（保留以备将来需要方向范围时使用）
        float wind_tilt = 0.f; // 下落方向偏离垂直方向的角度，度（0 = 垂直，89 = 几乎水平）
        float splash_chance = 1.f;      // 0..1 下落粒子留下地面贴花的概率（溅射或沉积）
        std::vector<uint32_t> sounds;   // 激活时随机播放的 .dat 音效文件 ID
        float sound_min_interval = 8.f; // 音效间隔（秒）
        float sound_max_interval = 25.f;
        bool sound_3d = false;                    // 从随机附近位置播放（改变音量/声像）
        float ambient = 0.f;                      // 0..1 此条件对场景贡献的阴天变暗程度
        unsigned int tint = 0xFFFFFFFFu;          // 此条件的粒子色调（ImGui 打包）；白色 = 无色调
        unsigned int overcast_tint = 0xFFA09078u; // 此条件驱动阴天时的场景变暗颜色
        int floor_decal = kDecalAuto;             // 每个撞击点留在地面上的效果：无/溅射/沉积
        float drift = kDriftAuto;                 // 粒子下落时的横向漂移（gwinch/秒 振幅）；0 = 无
        bool wind_camera_relative = false;        // 风向相对于相机（旋转时保持屏幕固定）
        bool center_on_camera = false;            // 将体积聚焦在相机本身而非其目标上（紧密环绕观察者）
        float column_height = 2500.f;             // 聚焦点上方下落粒子柱的垂直范围，上限为 column_height_max
        CloudCover cloud;                         // 可选云层（头顶云/雾/沙尘暴），可与下落粒子组合
        bool enabled = true;                      // 如果为 false，则从手动切换和自动天气选择中排除
    };

    // 广义天气气候。地图/区域被分组为其中之一（参见 ClimateForRegion），
    // 因此天气是按气候编写的，而非按区域——同一区域稍后可承载多个气候。
    enum class Climate {
        Temperate,
        Arid,
        Tropical,
        Mountainous,
        Volcanic,
        Desertous,
        None
    };

    struct ClimateWeather {
        std::string condition; // WeatherCondition 的名称
        float weight = 0.3f;   // 0..1 每次天气滚动时此条件被选中的概率
    };
    struct ClimateProfile {
        Climate climate = Climate::Temperate;
        std::vector<ClimateWeather> entries;
    };
} // namespace weather_module

namespace {
    using namespace weather_module;

    // .dat 中的游戏天气资源：32px 雨滴、32px 雪花和 128x128 溅射图集（4x4 关键帧）。
    constexpr uint32_t kRaindropFileId = 0x1997d;
    constexpr uint32_t kSnowflakeFileId = 0xca1a;
    constexpr uint32_t kSplashFileId = 0x1baa1;
    constexpr int kSplashCols = 4, kSplashRows = 4, kSplashFrames = kSplashCols * kSplashRows;
    constexpr float kZNear = GameWorldCompositor::kZNear, kZFar = GameWorldCompositor::kZFar;
    constexpr float kMaxRadius = GW::Constants::Range::Spirit; // 2500 gwinch；天气体积半径

    // 在代码中固定（特意不在 UI 中，但调试时便于调整）。
    float particle_area_full = 500.f; // 每个粒子在 100% 密度下覆盖的 gwinch^2 面积（越小越密）；
                                      // 每粒子面积按此值 * 100 / 密度 缩放，因此 100% 比 1% 密 100 倍
    int max_particles = 30000;         // 条件粒子数的硬上限（FPS 保护）
    float column_height_max = GW::Constants::Range::Spirit; // 聚焦点上方粒子起始高度的上限，防止雪从过高处开始
    float fog_factor = 1.0f;          // 传递给着色器的距离淡出强度
    float splash_size = 8.f;          // 溅射公告板的世界大小
    float splash_duration = 0.5f;     // 播放 16 个关键帧的秒数
    float recycle_below = 600.f;      // 当没有地形高度可用时的回退回收带
    float splash_lift = 5.f;          // 将溅射底部抬高以避免 Z 冲突
    int max_splashes = 4000;          // 每个条件实时溅射的上限
    float snow_sway_speed = 1.5f;     // 雪横向摆动的弧度/秒
    float snow_sway_amp = 120.f;      // gwinch/秒 水平摆动速度（位置振幅 = amp / speed）
    float snow_settle_chance = 0.15f; // 落地雪花留下残留斑点的比例
    float snow_settle_size = 14.f;    // 沉积雪花公告板的世界大小
    float snow_settle_duration = 4.f; // 沉积雪花在完全淡出前保持的秒数
    float snow_settle_fade = 0.4f;    // 生命周期最后一部分用于淡出
    int max_settled = 4000;           // 每个条件沉积雪花的上限

    // 条件留下的地面贴花；从天气类型解析 kDecalAuto，使旧保存的条件和新添加的条件行为如预期（雨溅射，雪沉积）。
    constexpr int EffectiveDecal(const WeatherCondition& c)
    {
        return c.floor_decal != kDecalAuto ? c.floor_decal : (c.type == kTypeSnow ? kDecalSettle : kDecalSplash);
    }
    // 横向浮动振幅；从类型解析 kDriftAuto（雪默认浮动，雨直线下落）。
    float EffectiveDrift(const WeatherCondition& c)
    {
        return c.drift >= 0.f ? c.drift : (c.type == kTypeSnow ? snow_sway_amp : 0.f);
    }
    // 聚焦点上方下落粒子柱的高度，有上限以防止粒子从过高处开始。
    float ColumnHeight(const WeatherCondition& c)
    {
        return std::clamp(c.column_height, 1.f, column_height_max);
    }
    // 由密度百分比得出的粒子数：用每个粒子覆盖 (particle_area_full * 100 / density) gwinch^2 来覆盖体积的水平圆盘
    //（半径 = spread_radius），因此密度线性缩放粒子数。有上限。
    int DropCount(const WeatherCondition& c)
    {
        if (c.density <= 0) return 0; // 0 = 无下落粒子（例如仅有云层的条件如雾/沙尘暴）
        const float disk = 3.14159265f * c.spread_radius * c.spread_radius;
        const float per_particle = particle_area_full * 100.f / static_cast<float>(std::clamp(c.density, 1, 100));
        // 按实际填充高度占满半径柱的比例缩放，使较短的柱容纳比例更少的粒子，而不是将相同数量压缩得更紧。
        const float height_fraction = ColumnHeight(c) / c.spread_radius;
        return std::min(max_particles, static_cast<int>(disk / per_particle * height_fraction));
    }

    // 云层的云团数：用云层自身的密度覆盖相同的水平圆盘（云团很大且重叠，
    // 因此面积计数足够——带内高度对每个云团随机化）。
    int CloudCount(const WeatherCondition& c)
    {
        if (c.cloud.top <= c.cloud.base || c.cloud.density <= 0) return 0;
        const float disk = 3.14159265f * c.cloud.radius * c.cloud.radius;
        const float per_particle = particle_area_full * 100.f / static_cast<float>(std::clamp(c.cloud.density, 1, 100));
        return std::min(max_particles, static_cast<int>(disk / per_particle));
    }

    std::vector<WeatherCondition> DefaultConditions()
    {
        // 通过聚合初始化下落粒子效果；云层（玩家上方的带）在之后分配，
        // 因为大多数预设两者都需要。CloudCover = {base, top, tint(argb), density, size, speed[, radius]}。
        std::vector<WeatherCondition> v = {
            {"大雨", kTypeRain, false, 70, 10.f, 500.f, 2500.f, 0.f, 25.f, 0.f, 0.30f, {0x20ed0, 0x20ed1}, 6.f, 60.f, false, 1.0f},
            {"小雨", kTypeRain, false, 5, 10.f, 500.f, 1500.f, 0.f, 25.f, 10.f, 1.0f, {0x20ed0, 0x20ed1}, 6.f, 60.f, false, 0.40f},
            {"雪", kTypeSnow, false, 20, 8.f, 100.f, 1500.f, 30.f, 55.f, 10.f, 0.15f, {}, 10.f, 30.f, false, 0.40f, 0xFFFFFFFFu, 0xFF98E4FFu},
            // 灰烬：雪的漂移（无地面贴花），深暖灰色调，更重的阴天。
            {"灰烬", kTypeSnow, false, 10, 9.f, 350.f, 2500.f, 30.f, 55.f, 8.f, 0.f, {}, 12.f, 35.f, false, 0.45f, 0xFF42464Au, 0xFFA09078u, kDecalNone},
            // 仅有云层的条件（低密度 = 稀疏/无下落粒子）；外观主要由下方的云层决定。
            {"雾", kTypeRain, false, 2, 10.f, 300.f, 1500.f, 0.f, 25.f, 0.f, 1.0f, {}, 10.f, 30.f, false, 0.0f, 0xFFFFFFFFu, 0xFFFFFFFFu},
            {"沙尘暴", kTypeRain, false, 10, 4.f, 500.f, 1000.f, 90.f, 90.f, 85.f, 0.f, {}, 10.f, 30.f, false, 0.0f, 0xFFC8B080u, 0xFFA09078u, kDecalNone},
            // 暴风雪：大雪 + 低白色雾带。
            {"暴风雪", kTypeSnow, false, 85, 6.f, 700.f, 1500.f, 35.f, 60.f, 39.f, 0.f, {}, 10.f, 30.f, false, 0.55f, 0xFFFFFFFFu, 0xFFA09078u, kDecalAuto, 23.f},
        };
        v[1].column_height = 1500.f;
        v[2].column_height = 1500.f;
        v[4].column_height = 1500.f;
        v[5].column_height = 60.f;
        v[6].column_height = 1000.f;
        v[0].cloud = {1000.f, 1500.f, 0xB0303840u, 25, 700.f, 60.f};   // 大雨：高处的深色雨云
        v[1].cloud = {1000.f, 1500.f, 0x70404850u, 15, 700.f, 20.f};   // 小雨：较浅、较稀疏的云
        v[2].cloud = {1000.f, 1500.f, 0xFDC6C6C6u, 20, 500.f, 20.f};   // 雪：苍白的阴天云带
        v[4].cloud = {-500.f, 100.f, 0x11C8C8D0u, 10, 800.f, 10.f, 1500.f}; // 雾：白色、低、慢、薄
        v[5].cloud = {-400.f, 60.f, 0x38C8B080u, 10, 500.f, 700.f, 600.f};  // 沙尘暴：黄褐色、地面层、快、紧凑（风向决定方向）
        v[6].cloud = {0.f, 1000.f, 0x2ED0D8E0u, 3, 800.f, 0.f, 1500.f}; // 暴风雪：雪下的淡白色雾
        v[4].enabled = false; // 雾：对游戏玩法相当有侵入性（遮挡视线），因此默认关闭
        v[5].enabled = false; // 沙尘暴：同样——对游戏玩法有侵入性，默认关闭
        return v;
    }
    std::vector<WeatherCondition> conditions = DefaultConditions();

    std::vector<ClimateProfile> DefaultClimateProfiles()
    {
        const auto p = [](const Climate c, std::vector<ClimateWeather> e) { return ClimateProfile{c, std::move(e)}; };
        return {
            p(Climate::Temperate, {{"Light Rain", 0.01f}, {"Heavy Rain", 0.005f}, {"Fog", 0.002f}}),
            p(Climate::Tropical, {{"Heavy Rain", 0.02f}, {"Light Rain", 0.01f}, {"Fog", 0.005f}}),
            p(Climate::Arid, {{"Light Rain", 0.01f}}),
            p(Climate::Desertous, {{"Light Rain", 0.005f}, {"Sandstorm", 0.015f}}),
            p(Climate::Mountainous, {{"Snow", 0.04f}, {"Blizzard", 0.015f}}),
            p(Climate::Volcanic, {{"Ashfall", 0.04f}, {"Fog", 0.005f}}),
        };
    }
    std::vector<ClimateProfile> climate_profiles = DefaultClimateProfiles();

    // 地形遮挡遵循共享的“游戏内渲染”设置（GetOccludeBehindTerrain()），
    // 因此与其他世界内叠加层在同一个地方配置。render_max_distance 固定在罗盘范围。
    constexpr float render_max_distance = kMaxRadius;
    unsigned int ambient_color = 0xFFA09078u; // 当前驱动变暗的条件的阴天色调（运行时）
    unsigned int active_tint = 0xFFFFFFFFu;   // 活动条件的粒子色调，传递给实例化绘制（运行时）
    float ambient_strength = 0.f;             // 活动条件的缓动聚合变暗（运行时，不保存）
    float weather_intensity = 0.f;            // 缓动 0..1 显示条件的交叉淡入淡出（运行时）：1 = 完全，0 = 淡出

    bool auto_weather = false;    // drive which conditions are active from the climate->weather table
    float auto_change_min = 2.f; // minutes between automatic weather rolls (random in [min, max])
    float auto_change_max = 5.f;
    Climate auto_climate = Climate::Temperate; // 上次自动天气滚动时的气候（运行时）
    float auto_timer = -1.f;                   // 直到下次自动滚动的分钟数；<0 = 下次更新时滚动（运行时）
    Climate auto_climate_override = Climate::None; // /climate 强制此气候，无论地图如何；None = 跟随地图（运行时，不保存）

    // 选择器提供的气候及其显示名称（我们自己的概念，因此是纯文本——非 EncString）。
    constexpr struct {
        Climate climate;
        const char* name;
    } kClimates[] = {
        {Climate::Temperate, "温带"}, {Climate::Arid, "干旱"}, {Climate::Desertous, "沙漠"}, {Climate::Tropical, "热带"}, {Climate::Mountainous, "山地"}, {Climate::Volcanic, "火山"},
    };

    const char* ClimateName(const Climate climate)
    {
        for (const auto& k : kClimates)
            if (k.climate == climate) return k.name;
        return "(气候)";
    }

    // 按显示名称解析气候（不区分大小写）。若无匹配则返回 false。
    bool ClimateByName(const std::string& name, Climate& out)
    {
        const std::string want = TextUtils::ToLower(name);
        for (const auto& k : kClimates)
            if (TextUtils::ToLower(k.name) == want) { out = k.climate; return true; }
        return false;
    }

    Climate ClimateForMap(const GW::Constants::MapID map_id)
    {
        // @增强功能：我们可通过检查与地图匹配的 DAT 文件中的纹理并计数沙、雪等来处理边缘情况，但目前过于复杂。
        static const std::unordered_map<GW::Constants::MapID, Climate> overrides = {
            {GW::Constants::MapID::Dry_Top, Climate::Arid},
            {GW::Constants::MapID::Ettins_Back, Climate::Arid},
            {GW::Constants::MapID::Ventaris_Refuge_outpost, Climate::Arid},
            {GW::Constants::MapID::Druids_Overlook_outpost, Climate::Arid},
            {GW::Constants::MapID::Sage_Lands, Climate::Arid},
            {GW::Constants::MapID::The_Deep, Climate::None},
            {GW::Constants::MapID::Urgozs_Warren, Climate::None}
        };
        if (const auto it = overrides.find(map_id); it != overrides.end()) return it->second;
        const auto info = GW::Map::GetMapInfo(map_id);
        if (!GW::Map::HasMapDisplayInfo(info) && !info->GetIsOnWorldMap())
            return Climate::None;
        // 室内（地牢）没有天空，因此从不应用气候天气。在区域 switch 之前检查，
        // 这样即使区域内本应映射到气候，地牢仍解析为晴朗。
        if (info && info->type == GW::RegionType::Dungeon)
            return Climate::None;
        switch (info ? info->region : GW::Region_DevRegion) {
            case GW::Region_NorthernShiverpeaks:
            case GW::Region_FarShiverpeaks:
                return Climate::Mountainous;
            case GW::Region_DepthsOfTyria: // EotN 地下，无天空
                return Climate::None;
            case GW::Region_CrystalDesert:
            case GW::Region_Desolation:
            case GW::Region_Istan:
                return Climate::Desertous;
            case GW::Region_Kourna:
            case GW::Region_Vaabi:
                return Climate::Arid;
            case GW::Region_FissureOfWoe:
                return Climate::Volcanic;
            case GW::Region_Maguuma:
            case GW::Region_Kurzick:
            case GW::Region_TarnishedCoast:
                return Climate::Tropical;
            case GW::Region_DomainOfAnguish:
                return Climate::None;
        }
        return Climate::Temperate;
    }

    // POSITION（世界）+ D3DCOLOR 色调 + UV；四边形在 CPU 上按相机/地面对齐构建。
    struct WeatherVertex {
        float x, y, z;
        DWORD color;
        float u, v;
    };
    struct Raindrop {
        float x, y, z, ground_z, sway_sin, sway_cos;
    }; // 沿 +z 下落（GW 向上为 -z）；ground_z 在种子生成时查询；（sway_sin, sway_cos）是每帧旋转的单位向量，
       // 驱动雪的横向摆动——比每帧每个粒子重新计算相位正弦/余弦更便宜
    struct Splash {
        float x, y, z, age;
    };
    struct Settle {
        float x, y, z, age;
    }; // 平躺在地面上的雪花；随时间老化并淡出
    struct CloudPuff {
        float x, y, h;
    }; // 云层云团：世界 x/y（漂移并环绕在气泡内）；h = 玩家上方高度（世界 z = cz - h）
    struct Particles {
        std::vector<Raindrop> raindrops;
        std::vector<Splash> splashes;
        std::vector<Settle> settled;
        std::vector<CloudPuff> clouds; // 云层云团（与下落粒子分开）
        float sound_timer = -1.f;      // 直到下一个音效的秒数；<0 = 尚未安排
    };
    Particles active_particles;
    int active_condition = -1;
    float active_wind_dir = 0.f; // 条件激活时在全圆上滚动的风向（度）
    float center_z = 0.f;        // 上次更新的聚焦高度；每帧柱随其变化而偏移，使慢速下落条件（很少重新播种顶部）
                                 // 仍能跟踪玩家的高度
    bool reset_requested = false;     // 由 WeatherModule::Reset() 设置；在下次更新时消费

    struct WeatherInstance {
        float cx, cy, cz;
        float alpha; // 色调常量的每实例 alpha 乘数
    };
    // 每个实例化绘制的公告板半轴（以前烘焙到每个记录中；现在设置为 VS 常量 c10/c11）。
    struct InstAxes { float x[3], y[3]; };
    InstAxes rain_axes{}, snow_axes{}, cloud_axes{}, settle_axes{};
    size_t snow_flake_count = 0; // snow_instances 包含 [雪花 .. 沉积]；此处是沉积子范围起始位置
    struct GeomVert {
        float sx, sy, u, v;
    }; // 静态单位四边形：角符号 + uv

    std::vector<WeatherInstance> rain_instances, snow_instances, cloud_instances; // GPU 实例化；cloud = 云层
    std::vector<WeatherVertex> splash_vertices;                  // 溅射保留在 CPU 构建（每帧精灵表 UV）
    IDirect3DVertexBuffer9 *rain_inst_vb = nullptr, *snow_inst_vb = nullptr, *cloud_inst_vb = nullptr, *splash_vb = nullptr;
    IDirect3DVertexBuffer9* quad_geom_vb = nullptr;   // 共享单位四边形（实例化的流 0）
    size_t rain_inst_cap = 0, snow_inst_cap = 0, cloud_inst_cap = 0; // 字节
    size_t splash_cap = 0;                            // 顶点数
    IDirect3DIndexBuffer9* quad_ib = nullptr;       // 每个 4 顶点四边形的共享 0-1-2 / 0-2-3 索引
    size_t quad_ib_quads = 0;                       // 四边形容量
    IDirect3DVertexShader9* weather_vs = nullptr;
    IDirect3DVertexShader9* weather_inst_vs = nullptr;
    IDirect3DPixelShader9* weather_ps = nullptr;
    IDirect3DVertexDeclaration9* weather_decl = nullptr;
    IDirect3DVertexDeclaration9* weather_inst_decl = nullptr;
    IDirect3DTexture9 **raindrop_tex_pp = nullptr, **snowflake_tex_pp = nullptr, **splash_tex_pp = nullptr; // 纹理缓存中的稳定槽位
    bool textures_requested = false;
    IDirect3DTexture9* cloud_tex = nullptr;       // 运行时生成的柔和径向云团（无 .dat 资源）；首次使用时惰性构建
    bool active_has_cloud = false;                // 活动条件在本帧有要绘制的云层
    unsigned int active_cloud_tint = 0x80808080u; // 活动条件的云色调，传递给云层绘制
    clock_t last_update = 0;                  // 上次节流更新时的 TIMER_INIT()（0 = 尚未运行）
    constexpr clock_t kUpdateIntervalMs = 16; // 以 ~60 Hz 重建物理 + 几何；绘制保持每帧
    uint32_t rng = 0x1234567u;
    bool rain_ready = false, snow_ready = false, cloud_ready = false, splash_ready = false;
    int compositor_token = 0;
    GW::HookEntry chat_hook_entry;

#ifdef _DEBUG
    // 调试覆盖层：活动条件粒子柱（红色）和云带（青色）的线框框，
    // 绘制为世界空间线列表，以便查看模拟使用的体积。
    bool debug_wireframe = false;
    std::vector<WeatherVertex> wire_vertices;
    IDirect3DVertexBuffer9* wire_vb = nullptr;
    size_t wire_cap = 0;
    bool wire_ready = false;
#endif

    float frand(const float lo, const float hi)
    {
        rng = rng * 1664525u + 1013904223u;
        return lo + static_cast<float>(rng >> 8) / static_cast<float>(0xFFFFFFu) * (hi - lo);
    }

    void cross3(const float a[3], const float b[3], float o[3])
    {
        o[0] = a[1] * b[2] - a[2] * b[1];
        o[1] = a[2] * b[0] - a[0] * b[2];
        o[2] = a[0] * b[1] - a[1] * b[0];
    }

    void normalize3(float v[3])
    {
        const float l = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
        if (l > 1e-6f) {
            v[0] /= l;
            v[1] /= l;
            v[2] /= l;
        }
    }

    void WindDir(const float dir_deg, const float tilt_deg, float out[3])
    {
        constexpr float kDeg2Rad = 0.01745329f;
        const float st = std::sin(tilt_deg * kDeg2Rad), ct = std::cos(tilt_deg * kDeg2Rad);
        out[0] = st * std::cos(dir_deg * kDeg2Rad);
        out[1] = st * std::sin(dir_deg * kDeg2Rad);
        out[2] = ct;
    }

    constexpr float kNoGround = std::numeric_limits<float>::max(); // “此处无地形”哨兵
    constexpr float kGroundCell = 64.f;                            // 高度缓存的世界网格分辨率（gwinch）
    constexpr size_t kGroundCacheMax = 1u << 16;                   // 条目上限；超过则整体清除
    std::unordered_map<uint64_t, float> ground_cache;              // (x,y) 网格 -> 地形高度，在地图内有效

    // (x,y) 处所有平面中最高的静态表面（GW 向上为 -z，因此最高 = 最小高度），若无则返回 kNoGround。
    float RawGroundZAt(const float x, const float y)
    {
        const GW::PathingMapArray* pm = GW::Map::GetPathingMap();
        const uint32_t n = pm ? static_cast<uint32_t>(pm->size()) : 0;
        float best = kNoGround;
        for (uint32_t zp = 0; zp < n; ++zp) {
            const float a = TerrainDrape::QueryAltAt(x, y, zp);
            if (a != 0.f && a < best) best = a;
        }
        return best;
    }

    // 按粗略世界网格记忆化：每个 (x,y) 的地形是静态的，但每平面查询成本较高，
    // 且我们对每个落地的粒子都进行采样，因此附近的粒子重用同一网格。在地图变化时在 SyncWeather 中失效。
    float GroundZAt(const float x, const float y, const float fallback)
    {
        const int gx = static_cast<int>(std::floor(x / kGroundCell));
        const int gy = static_cast<int>(std::floor(y / kGroundCell));
        const uint64_t key = static_cast<uint64_t>(static_cast<uint32_t>(gx)) << 32 | static_cast<uint32_t>(gy);
        if (const auto it = ground_cache.find(key); it != ground_cache.end()) return it->second == kNoGround ? fallback : it->second;
        const float z = RawGroundZAt(x, y);
        if (z != kNoGround) { // 不缓存未命中：它们很少见（虚空上方的粒子）且有 poisoning 风险
            if (ground_cache.size() >= kGroundCacheMax) ground_cache.clear();
            ground_cache.emplace(key, z);
        }
        return z == kNoGround ? fallback : z;
    }

    // .dat 文件是否为音效（其类型字节看起来像 ANet/MP3 流，镜像 FavorTracker）。
    bool IsValidSound(const uint32_t id)
    {
        static std::map<uint32_t, bool> cache;
        if (!id) return false;
        if (const auto it = cache.find(id); it != cache.end()) return it->second;
        ArenaNetFileParser::GameAssetFile f;
        f.readFromDat(id);
        const char* ft = f.fileType();
        const bool ok = f.file_id == id && ft && static_cast<unsigned char>(ft[0]) == 0xFF && (static_cast<unsigned char>(ft[1]) & 0xE6) == 0xE2;
        cache[id] = ok;
        return ok;
    }

    void UpdateSounds(const WeatherCondition& c, Particles& p, const float dt, const float cx, const float cy, const float cz)
    {
        if (c.sounds.empty() || c.sound_max_interval <= 0.f) return;
        if (p.sound_timer < 0.f) { // 首次激活帧：安排，不立即播放
            p.sound_timer = frand(c.sound_min_interval, c.sound_max_interval);
            return;
        }
        if ((p.sound_timer -= dt) > 0.f) return;
        const size_t idx = std::min(c.sounds.size() - 1, static_cast<size_t>(frand(0.f, static_cast<float>(c.sounds.size()))));
        if (c.sounds[idx]) {
            // PlaySound 自身会编组到游戏线程，因此从此处调用是安全的。
            if (c.sound_3d) {
                const float ang = frand(0.f, 6.2831853f), dist = frand(200.f, 1500.f);
                const GW::Vec3f pos{cx + dist * std::cos(ang), cy + dist * std::sin(ang), cz - frand(0.f, 1000.f)};
                AudioSettings::PlaySoundFileId(c.sounds[idx], &pos);
            }
            else {
                AudioSettings::PlaySoundFileId(c.sounds[idx]);
            }
        }
        p.sound_timer = frand(c.sound_min_interval, c.sound_max_interval);
    }

    void RerollAutoWeather(const Climate climate)
    {
        const ClimateProfile* prof = nullptr;
        for (const auto& cp : climate_profiles)
            if (cp.climate == climate) { prof = &cp; break; }

        int chosen = -1; // conditions 中的索引；-1 = 晴朗
        if (prof) {
            float sum = 0.f;
            for (const auto& e : prof->entries) sum += std::max(0.f, e.weight);
            float r = frand(0.f, std::max(sum, 1.f));
            for (const auto& e : prof->entries) {
                const float w = std::max(0.f, e.weight);
                if (w <= 0.f) continue;
                if (r < w) { // 此条目赢得了滚动；将其名称解析为实时启用的条件（如果已消失/禁用则为晴朗）
                    for (int i = 0; i < static_cast<int>(conditions.size()); i++)
                        if (conditions[i].name == e.condition && conditions[i].enabled) { chosen = i; break; }
                    break;
                }
                r -= w;
            }
        }
        for (int i = 0; i < static_cast<int>(conditions.size()); i++)
            conditions[i].active = i == chosen;
    }

    // 启用时，从气候表驱动活动条件：当气候变化时重新滚动（包括首次运行），
    // 然后每隔（随机）几分钟重新滚动。dt 以秒为单位。
    void UpdateAutoWeather(const float dt)
    {
        if (!auto_weather) return;
        if (!GW::Map::GetCurrentMapInfo()) return; // 地图尚未加载；保持当前显示
        // 强制气候（来自 /climate <name>）优先；否则跟随当前地图的气候。
        const Climate climate = auto_climate_override != Climate::None ? auto_climate_override : ClimateForMap(GW::Map::GetMapID());
        if (climate != auto_climate || auto_timer < 0.f) {
            auto_climate = climate;
            RerollAutoWeather(climate);
            auto_timer = frand(auto_change_min, auto_change_max);
            return;
        }
        if ((auto_timer -= dt / 60.f) <= 0.f) { // 计时器以分钟为单位
            RerollAutoWeather(climate);
            auto_timer = frand(auto_change_min, auto_change_max);
        }
    }

    // 平铺生成区域的正方形网格的边数，用于分层放置。
    int SpawnGrid(const int count)
    {
        return std::max(1, static_cast<int>(std::ceil(std::sqrt(static_cast<float>(std::max(1, count))))));
    }

    float LandingGroundZ(const WeatherCondition& c, const float x, const float y, const float top, const float vx, const float vy, const float vz, const float cz)
    {
        const float fallback = cz + recycle_below;
        const float local = GroundZAt(x, y, fallback);
        if (vz < 1.f) return local; // 水平飘移——无有意义的着陆投影（同时避免除以 vz）
        const float drop = std::max(0.f, local - top);  // 从柱顶到地面的近似垂直距离
        float hx = vx / vz * drop, hy = vy / vz * drop; // 在该下落过程中累积的水平行程（方向 + 倾斜）
        if (const float h = std::sqrt(hx * hx + hy * hy); h > c.spread_radius) { const float s = c.spread_radius / h; hx *= s; hy *= s; } // 粒子在气泡内环绕，因此将投影限制在气泡内
        return GroundZAt(x + hx, y + hy, fallback);
    }

    void seed_drop(Raindrop& d, const WeatherCondition& c, const float cx, const float cy, const float cz, const int index, const int grid, const float vx, const float vy, const float vz)
    {
        const float top_z = cz - ColumnHeight(c); // 下落柱顶部（有上限，防止从过高处开始）
        const float cell = 2.f * c.spread_radius / static_cast<float>(grid);
        d.x = cx - c.spread_radius + (static_cast<float>(index % grid) + frand(0.f, 1.f)) * cell;
        d.y = cy - c.spread_radius + (static_cast<float>(index / grid) + frand(0.f, 1.f)) * cell;
        d.ground_z = LandingGroundZ(c, d.x, d.y, top_z, vx, vy, vz, cz); // 它将在其上着陆的地形，而非生成柱
        // 将填充散布在可见柱（top..ground）中，使粒子不会同步成一块下落。
        d.z = top_z + frand(0.f, std::max(0.f, d.ground_z - top_z));
        const float a = frand(0.f, 6.2831853f); // 初始随机摆动方向（单位向量，每帧旋转）
        d.sway_sin = std::sin(a);
        d.sway_cos = std::cos(a);
    }

    // 云层的初始填充：在气泡上分层放置，在玩家上方带内随机高度。
    // 世界 z 每帧从实时玩家高度派生（cz - h），因此层跟随玩家。
    void seed_cloud(CloudPuff& cl, const WeatherCondition& c, const float cx, const float cy, const int index, const int grid)
    {
        const float cell = 2.f * c.cloud.radius / static_cast<float>(grid);
        cl.x = cx - c.cloud.radius + (static_cast<float>(index % grid) + frand(0.f, 1.f)) * cell;
        cl.y = cy - c.cloud.radius + (static_cast<float>(index / grid) + frand(0.f, 1.f)) * cell;
        cl.h = frand(c.cloud.base, c.cloud.top);
    }

    void UpdateCondition(const WeatherCondition& c, Particles& p, const float dt, const float cx, const float cy, const float cz, const float wind_dir, const float center_dz)
    {
        const int count = DropCount(c);
        const int grid = SpawnGrid(count);
        // 速度是（单位）下落方向乘以 fall_speed，因此风设置方向而非速度。
        // 提前计算，因为着陆投影在（重新）生成粒子时需要它。
        float vel[3];
        WindDir(wind_dir, c.wind_tilt, vel);
        const float vx = vel[0] * c.fall_speed, vy = vel[1] * c.fall_speed, vz = vel[2] * c.fall_speed;
        if (static_cast<int>(p.raindrops.size()) != count) {
            p.raindrops.resize(std::max(0, count));
            for (int i = 0; i < static_cast<int>(p.raindrops.size()); i++)
                seed_drop(p.raindrops[i], c, cx, cy, cz, i, grid, vx, vy, vz);
        }
        const int decal = EffectiveDecal(c);
        const bool splash = decal == kDecalSplash;
        const float drift = EffectiveDrift(c);
        const bool settle = decal == kDecalSettle;
        const float top_z = cz - ColumnHeight(c); // 粒子在此处重新启动（散布高度仅用于种子生成）
        const float diameter = 2.f * c.spread_radius;
        // 雪的横向摆动：每帧将每个粒子的单位摆动向量旋转相同的小角度。
        // 两次三角函数调用是每帧的，而非每个粒子——每粒子步骤只是 2x2 旋转（几次乘加）。
        const bool has_sway = drift > 0.f;
        const float rs = std::sin(snow_sway_speed * dt), rc = std::cos(snow_sway_speed * dt);
        for (int i = 0; i < static_cast<int>(p.raindrops.size()); i++) {
            auto& d = p.raindrops[i];
            d.z += vz * dt + center_dz; // 下落，加上整个柱跟踪玩家的垂直移动
            float sway_x = 0.f, sway_y = 0.f;
            if (has_sway) {
                sway_x = drift * d.sway_sin;
                sway_y = drift * d.sway_cos;
                const float ns = d.sway_sin * rc + d.sway_cos * rs; // 旋转单位向量（保持长度）
                d.sway_cos = d.sway_cos * rc - d.sway_sin * rs;
                d.sway_sin = ns;
            }
            d.x += (vx + sway_x) * dt;
            d.y += (vy + sway_y) * dt;
            if (const float rx = d.x - cx; rx > c.spread_radius) d.x -= diameter; else if (rx < -c.spread_radius) d.x += diameter;
            if (const float ry = d.y - cy; ry > c.spread_radius) d.y -= diameter; else if (ry < -c.spread_radius) d.y += diameter;
            if (d.z >= d.ground_z) {
                if (splash && frand(0.f, 1.f) < c.splash_chance && static_cast<int>(p.splashes.size()) < max_splashes) p.splashes.push_back({d.x, d.y, GroundZAt(d.x, d.y, d.ground_z), 0.f});
                if (settle && frand(0.f, 1.f) < c.splash_chance && static_cast<int>(p.settled.size()) < max_settled) p.settled.push_back({d.x, d.y, GroundZAt(d.x, d.y, d.ground_z), 0.f});
                d.x = cx - c.spread_radius + frand(0.f, 2.f * c.spread_radius);
                d.y = cy - c.spread_radius + frand(0.f, 2.f * c.spread_radius);
                d.z = top_z + frand(0.f, std::max(1.f, vz * dt));
                d.ground_z = LandingGroundZ(c, d.x, d.y, top_z, vx, vy, vz, cz);
                // 摆动向量保持不变——它已是一个旋转单位向量，无需重新随机化。
            }
        }
        for (size_t i = 0; i < p.splashes.size();) {
            if ((p.splashes[i].age += dt) >= splash_duration) {
                p.splashes[i] = p.splashes.back();
                p.splashes.pop_back();
            }
            else
                ++i;
        }
        for (size_t i = 0; i < p.settled.size();) {
            if ((p.settled[i].age += dt) >= snow_settle_duration) {
                p.settled[i] = p.settled.back();
                p.settled.pop_back();
            }
            else
                ++i;
        }
    }

    // ImGui 将颜色打包为 0xAABBGGRR（R 低字节），但 D3DDECLTYPE_D3DCOLOR（CPU 公告板顶点颜色）需要
    // 0xAARRGGBB——因此交换 R 和 B。实例化雨路径传递 float4 RGBA 而非此格式，无需转换。
    DWORD ToD3DColor(const unsigned int imgui_col)
    {
        return (imgui_col & 0xFF00FF00u) | ((imgui_col & 0xFFu) << 16) | ((imgui_col >> 16) & 0xFFu);
    }

    // 四边形的四个角，中心为 ± 半轴向量 ax, ay，具有给定 UV 矩形。
    // 通过共享索引缓冲区绘制为两个三角形（0-1-2, 0-2-3），因此每个四边形仅存储 4 个顶点。
    void emit_quad(std::vector<WeatherVertex>& out, const float cx, const float cy, const float cz, const float ax[3], const float ay[3], const DWORD col, const float u0, const float v0, const float u1, const float v1)
    {
        const WeatherVertex c00{cx - ax[0] - ay[0], cy - ax[1] - ay[1], cz - ax[2] - ay[2], col, u0, v1};
        const WeatherVertex c10{cx + ax[0] - ay[0], cy + ax[1] - ay[1], cz + ax[2] - ay[2], col, u1, v1};
        const WeatherVertex c11{cx + ax[0] + ay[0], cy + ax[1] + ay[1], cz + ax[2] + ay[2], col, u1, v0};
        const WeatherVertex c01{cx - ax[0] + ay[0], cy - ax[1] + ay[1], cz - ax[2] + ay[2], col, u0, v0};
        out.insert(out.end(), {c00, c10, c11, c01});
    }

    bool InFront(const Raindrop& d, const float eye[3], const float fwd[3])
    {
        return (d.x - eye[0]) * fwd[0] + (d.y - eye[1]) * fwd[1] + (d.z - eye[2]) * fwd[2] >= kZNear;
    }

    bool InView(const float px, const float py, const float pz, const float eye[3], const float fwd[3], const float cone_tan_sq)
    {
        const float dx = px - eye[0], dy = py - eye[1], dz = pz - eye[2];
        const float depth = dx * fwd[0] + dy * fwd[1] + dz * fwd[2];
        if (depth < kZNear) return false;
        const float lat_sq = dx * dx + dy * dy + dz * dz - depth * depth; // 到视轴的平方垂直距离
        return lat_sq <= depth * depth * cone_tan_sq;
    }

    void AppendSnowInstances(std::vector<WeatherInstance>& out, const WeatherCondition& c, const std::vector<Raindrop>& drops, const float right[3], const float up[3], const float eye[3], const float fwd[3])
    {
        const float h = c.drop_size * 0.5f;
        snow_axes = {{right[0] * h, right[1] * h, right[2] * h}, {up[0] * h, up[1] * h, up[2] * h}};
        out.reserve(out.size() + drops.size());
        for (const auto& d : drops)
            if (InFront(d, eye, fwd)) out.push_back({d.x, d.y, d.z, 1.f});
    }

    void UpdateCloudCover(const WeatherCondition& c, Particles& p, const float dt, const float cx, const float cy, const float wind_dir)
    {
        const int count = CloudCount(c);
        const int grid = SpawnGrid(count);
        if (static_cast<int>(p.clouds.size()) != count) {
            p.clouds.resize(std::max(0, count));
            for (int i = 0; i < static_cast<int>(p.clouds.size()); i++)
                seed_cloud(p.clouds[i], c, cx, cy, i, grid);
        }
        float dir[3];
        WindDir(wind_dir, 90.f, dir); // 倾斜 90 => 沿航向的纯水平单位向量
        const float vx = dir[0] * c.cloud.speed, vy = dir[1] * c.cloud.speed;
        const float r = c.cloud.radius, diameter = 2.f * r;
        for (auto& cl : p.clouds) {
            cl.x += vx * dt;
            cl.y += vy * dt;
            if (const float rx = cl.x - cx; rx > r) cl.x -= diameter; else if (rx < -r) cl.x += diameter;
            if (const float ry = cl.y - cy; ry > r) cl.y -= diameter; else if (ry < -r) cl.y += diameter;
        }
    }

    void AppendCloudCoverInstances(std::vector<WeatherInstance>& out, const WeatherCondition& c, const std::vector<CloudPuff>& puffs, const float right[3], const float up[3], const float eye[3], const float fwd[3], const float cz)
    {
        const float hs = c.cloud.size * 0.5f;
        cloud_axes = {{right[0] * hs, right[1] * hs, right[2] * hs}, {up[0] * hs, up[1] * hs, up[2] * hs}};
        const float span = std::max(1.f, c.cloud.top - c.cloud.base);
        out.reserve(out.size() + puffs.size());
        for (const auto& cl : puffs) {
            const float z = cz - cl.h; // 玩家上方高度（GW 向上为 -z）
            const float dx = cl.x - eye[0], dy = cl.y - eye[1], dz = z - eye[2];
            if (dx * fwd[0] + dy * fwd[1] + dz * fwd[2] < kZNear) continue; // 在近平面后：跳过构建
            const float t = (cl.h - c.cloud.base) / span;                  // 0 在带底部，1 在带顶部
            const float a = t < 0.2f ? t / 0.2f : t > 0.8f ? (1.f - t) / 0.2f : 1.f; // 两端羽化
            out.push_back({cl.x, cl.y, z, a});
        }
    }

    void AppendRainInstances(std::vector<WeatherInstance>& out, const WeatherCondition& c, const std::vector<Raindrop>& drops, const float right[3], const float fwd[3], const float wind_dir, const float eye[3])
    {
        const float h = c.drop_size * 0.5f;
        float vel[3];
        WindDir(wind_dir, c.wind_tilt, vel); // 单位下落方向（雨滴沿此方向延伸）
        float w[3];
        cross3(vel, fwd, w);
        const float wl = std::sqrt(w[0] * w[0] + w[1] * w[1] + w[2] * w[2]);
        if (wl > 1e-4f) {
            w[0] /= wl;
            w[1] /= wl;
            w[2] /= wl;
        }
        else {
            w[0] = right[0];
            w[1] = right[1];
            w[2] = right[2];
        }
        rain_axes = {{w[0] * h, w[1] * h, w[2] * h}, {vel[0] * h, vel[1] * h, vel[2] * h}};
        out.reserve(out.size() + drops.size());
        for (const auto& d : drops)
            if (InFront(d, eye, fwd)) out.push_back({d.x, d.y, d.z, 1.f}); // 跳过相机后的粒子（无论如何都被裁剪）
    }

    // 竖立在地面上的公告板：水平轴面向观察者，垂直轴为世界向上（-z），锚定使底部位于撞击点。
    void AppendSplashes(const std::vector<Splash>& s, const float right[3], const unsigned int tint, const float eye[3], const float fwd[3], const float cone_tan_sq)
    {
        const float hs = splash_size * 0.5f;
        const float ax[3] = {right[0] * hs, right[1] * hs, right[2] * hs};
        const float ay[3] = {0.f, 0.f, -hs};
        const DWORD col = ToD3DColor(tint);
        const float u_step = 1.f / kSplashCols, v_step = 1.f / kSplashRows;
        for (const auto& sp : s) {
            if (!InView(sp.x, sp.y, sp.z, eye, fwd, cone_tan_sq)) continue;
            const int f = std::clamp(static_cast<int>(sp.age / splash_duration * kSplashFrames), 0, kSplashFrames - 1);
            const float u0 = static_cast<float>(f % kSplashCols) * u_step, v0 = static_cast<float>(f / kSplashCols) * v_step;
            emit_quad(splash_vertices, sp.x, sp.y, sp.z - hs - splash_lift, ax, ay, col, u0, v0, u0 + u_step, v0 + v_step);
        }
    }


    void AppendSettledInstances(std::vector<WeatherInstance>& out, const std::vector<Settle>& s, const float eye[3], const float fwd[3], const float cone_tan_sq)
    {
        const float hs = snow_settle_size * 0.5f;
        settle_axes = {{hs, 0.f, 0.f}, {0.f, hs, 0.f}}; // 平放在世界 XY 平面上
        const float fade_span = std::clamp(snow_settle_fade, 0.01f, 1.f);
        out.reserve(out.size() + s.size());
        for (const auto& sp : s) {
            if (!InView(sp.x, sp.y, sp.z, eye, fwd, cone_tan_sq)) continue;
            const float life = snow_settle_duration > 0.f ? sp.age / snow_settle_duration : 1.f;
            const float fade = life < 1.f - fade_span ? 1.f : std::max(0.f, (1.f - life) / fade_span);
            out.push_back({sp.x, sp.y, sp.z - splash_lift, fade});
        }
    }

    // 增长并将原始字节 blob 上传到只写托管顶点缓冲区（用于实例流）。
    bool UploadVB(IDirect3DDevice9* device, IDirect3DVertexBuffer9*& vb, size_t& cap_bytes, const void* data, const size_t bytes)
    {
        if (bytes == 0) return false;
        if (!vb || cap_bytes < bytes) {
            if (vb) {
                vb->Release();
                vb = nullptr;
            }
            const size_t c = bytes + bytes / 2; // 预留余量，使计数调整不会每帧重新分配
            if (device->CreateVertexBuffer(static_cast<UINT>(c), D3DUSAGE_WRITEONLY, 0, D3DPOOL_MANAGED, &vb, nullptr) != D3D_OK) {
                cap_bytes = 0;
                return false;
            }
            cap_bytes = c;
        }
        void* mem = nullptr;
        if (vb->Lock(0, static_cast<UINT>(bytes), &mem, 0) != D3D_OK || !mem) return false;
        memcpy(mem, data, bytes);
        vb->Unlock();
        return true;
    }

    bool EnsureVb(IDirect3DDevice9* device, IDirect3DVertexBuffer9*& vb, size_t& cap, const std::vector<WeatherVertex>& verts)
    {
        const size_t needed = verts.size();
        if (needed == 0) return false;
        if (!vb || cap < needed) {
            if (vb) {
                vb->Release();
                vb = nullptr;
            }
            const size_t c = needed + needed / 2; // 预留余量，使计数调整不会每帧重新分配
            if (device->CreateVertexBuffer(static_cast<UINT>(c * sizeof(WeatherVertex)), D3DUSAGE_WRITEONLY, 0, D3DPOOL_MANAGED, &vb, nullptr) != D3D_OK) {
                cap = 0;
                return false;
            }
            cap = c;
        }
        void* mem = nullptr;
        if (vb->Lock(0, static_cast<UINT>(needed * sizeof(WeatherVertex)), &mem, 0) != D3D_OK || !mem) return false;
        memcpy(mem, verts.data(), needed * sizeof(WeatherVertex));
        vb->Unlock();
        return true;
    }

    // 四边形索引模式是静态的（仅数量增长），因此构建一次并在所有绘制中重用。
    bool EnsureQuadIB(IDirect3DDevice9* device, const size_t quads)
    {
        if (quads == 0) return false;
        if (quad_ib && quad_ib_quads >= quads) return true;
        if (quad_ib) {
            quad_ib->Release();
            quad_ib = nullptr;
        }
        const size_t cap = quads + quads / 2; // 预留余量，使计数调整不会每帧重新分配
        if (device->CreateIndexBuffer(static_cast<UINT>(cap * 6 * sizeof(uint32_t)), D3DUSAGE_WRITEONLY, D3DFMT_INDEX32, D3DPOOL_MANAGED, &quad_ib, nullptr) != D3D_OK) {
            quad_ib_quads = 0;
            return false;
        }
        void* mem = nullptr;
        if (quad_ib->Lock(0, 0, &mem, 0) != D3D_OK || !mem) {
            quad_ib->Release();
            quad_ib = nullptr;
            quad_ib_quads = 0;
            return false;
        }
        auto* idx = static_cast<uint32_t*>(mem);
        for (size_t q = 0; q < cap; q++) {
            const uint32_t b = static_cast<uint32_t>(q * 4);
            idx[q * 6 + 0] = b + 0;
            idx[q * 6 + 1] = b + 1;
            idx[q * 6 + 2] = b + 2;
            idx[q * 6 + 3] = b + 0;
            idx[q * 6 + 4] = b + 2;
            idx[q * 6 + 5] = b + 3;
        }
        quad_ib->Unlock();
        quad_ib_quads = cap;
        return true;
    }

    bool EnsureShaders(IDirect3DDevice9* device)
    {
        if (weather_vs && weather_ps && weather_decl && weather_inst_vs && weather_inst_decl && quad_geom_vb) return true;
        constexpr D3DVERTEXELEMENT9 decl[] = {
            {0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0}, {0, 12, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0}, {0, 16, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0}, D3DDECL_END()
        };
        if (!weather_decl && device->CreateVertexDeclaration(decl, &weather_decl) != D3D_OK) return false;
        if (!weather_vs && device->CreateVertexShader(reinterpret_cast<const DWORD*>(&weather_billboard_vs), &weather_vs) != D3D_OK) return false;
        if (!weather_ps && device->CreatePixelShader(reinterpret_cast<const DWORD*>(&weather_billboard_ps), &weather_ps) != D3D_OK) return false;

        constexpr D3DVERTEXELEMENT9 inst_decl[] = {{0, 0, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0}, {0, 8, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
                                                   {1, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 1}, {1, 12, D3DDECLTYPE_FLOAT1, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 2}, D3DDECL_END()};
        if (!weather_inst_decl && device->CreateVertexDeclaration(inst_decl, &weather_inst_decl) != D3D_OK) return false;
        if (!weather_inst_vs && device->CreateVertexShader(reinterpret_cast<const DWORD*>(&weather_instanced_vs), &weather_inst_vs) != D3D_OK) return false;
        if (!quad_geom_vb) {
            constexpr GeomVert quad[4] = {{-1.f, -1.f, 0.f, 1.f}, {1.f, -1.f, 1.f, 1.f}, {1.f, 1.f, 1.f, 0.f}, {-1.f, 1.f, 0.f, 0.f}};
            if (device->CreateVertexBuffer(sizeof(quad), D3DUSAGE_WRITEONLY, 0, D3DPOOL_MANAGED, &quad_geom_vb, nullptr) != D3D_OK) return false;
            void* mem = nullptr;
            if (quad_geom_vb->Lock(0, 0, &mem, 0) != D3D_OK || !mem) return false;
            memcpy(mem, quad, sizeof(quad));
            quad_geom_vb->Unlock();
        }
        return true;
    }

    // Build a soft round puff texture at runtime (no .dat asset): white RGB with a smooth radial alpha falloff,
    // so overlapping cloud billboards blend into a continuous fog bank instead of showing hard quad edges.
    bool BuildCloudTexture(IDirect3DDevice9* device)
    {
        if (cloud_tex) return true;
        constexpr int N = 64;
        if (device->CreateTexture(N, N, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &cloud_tex, nullptr) != D3D_OK) {
            cloud_tex = nullptr;
            return false;
        }
        D3DLOCKED_RECT lr;
        if (cloud_tex->LockRect(0, &lr, nullptr, 0) != D3D_OK) {
            cloud_tex->Release();
            cloud_tex = nullptr;
            return false;
        }
        auto* const base = static_cast<uint8_t*>(lr.pBits);
        constexpr float c = (N - 1) * 0.5f;
        for (int y = 0; y < N; y++) {
            auto* const px = reinterpret_cast<uint32_t*>(base + y * lr.Pitch);
            for (int x = 0; x < N; x++) {
                const float dx = (static_cast<float>(x) - c) / c, dy = (static_cast<float>(y) - c) / c;
                const float r = std::sqrt(dx * dx + dy * dy);
                const float a = std::clamp(1.f - r, 0.f, 1.f);
                const auto alpha = static_cast<uint8_t>(a * a * 255.f + 0.5f); // 平方 = 更柔和、核心更胖的边缘
                px[x] = (static_cast<uint32_t>(alpha) << 24) | 0x00FFFFFFu;    // A8R8G8B8：白色，径向 alpha
            }
        }
        cloud_tex->UnlockRect(0);
        return true;
    }

#ifdef _DEBUG
    // 将轴对齐盒子的 12 条边（中心 cx,cy；半宽 r；z 范围为 z0..z1）作为世界空间线段
    // 追加到调试覆盖层。UV 为纹理中心，因此（不透明）纹素给出纯色。
    void AppendBoxWire(std::vector<WeatherVertex>& out, const float cx, const float cy, const float r, const float z0, const float z1, const DWORD col)
    {
        const float x0 = cx - r, x1 = cx + r, y0 = cy - r, y1 = cy + r;
        const WeatherVertex c[8] = {
            {x0, y0, z0, col, 0.5f, 0.5f}, {x1, y0, z0, col, 0.5f, 0.5f}, {x1, y1, z0, col, 0.5f, 0.5f}, {x0, y1, z0, col, 0.5f, 0.5f},
            {x0, y0, z1, col, 0.5f, 0.5f}, {x1, y0, z1, col, 0.5f, 0.5f}, {x1, y1, z1, col, 0.5f, 0.5f}, {x0, y1, z1, col, 0.5f, 0.5f},
        };
        const int edges[12][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6}, {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
        for (const auto& e : edges) { out.push_back(c[e[0]]); out.push_back(c[e[1]]); }
    }
#endif

    void SyncWeather(IDirect3DDevice9* device, const GW::Camera* cam, const float dt)
    {
        rain_ready = snow_ready = cloud_ready = splash_ready = false;
        active_has_cloud = false;
        snow_flake_count = 0;
        rain_instances.clear();
        snow_instances.clear();
        cloud_instances.clear();
        splash_vertices.clear();
#ifdef _DEBUG
        wire_vertices.clear();
        wire_ready = false;
#endif
        UpdateAutoWeather(dt); // 在下面读取前可能切换活动条件
        if (!cam) return;

        bool reset = reset_requested; // 显式 Reset() 请求
        reset_requested = false;

        if (!GW::Map::GetIsMapLoaded() || GW::UI::IsLoadingScreenShown()) reset = true;
        const auto ready = !reset && !GW::UI::GetIsWorldMapShowing();
        if (reset) ground_cache.clear(); // 地形高度按地图变化；地图改变时丢弃

        const float eye[3] = {cam->position.x, cam->position.y, cam->position.z};
        // 视线方向始终为相机 -> 目标，即使体积本身以相机为中心。
        float fwd[3] = {cam->look_at_target.x - eye[0], cam->look_at_target.y - eye[1], cam->look_at_target.z - eye[2]};
        normalize3(fwd);
        constexpr float world_up[3] = {0.f, 0.f, -1.f};
        float right[3];
        cross3(world_up, fwd, right);
        normalize3(right);
        float up[3];
        cross3(fwd, right, up);

        // 唯一期望的条件（第一个标记为激活的；单激活在其被切换的任何地方都强制执行）。
        int desired = -1;
        for (int i = 0; i < static_cast<int>(conditions.size()); i++)
            if (conditions[i].active) { desired = i; break; }

        // 通过一个缓动的 0..1 强度交叉淡入淡出条件变化：保持当前显示的条件直到其完全淡出，
        // 然后切换并让新条件淡入——因此切换读起来像是短暂的间歇，而非硬跳变。
        const float intensity_target = (!reset && desired == active_condition && active_condition >= 0) ? 1.f : 0.f;
        weather_intensity += (intensity_target - weather_intensity) * std::clamp(dt * 2.f, 0.f, 1.f); // ~1/2 秒缓动
        bool just_swapped = false;
        if (reset || (desired != active_condition && weather_intensity < 0.02f)) {
            active_particles = {}; // 丢弃旧粒子；新条件在下面重新播种
            active_condition = desired;
            if (reset) weather_intensity = 0.f; // 隐藏（加载/世界地图）：从晴朗开始新条件
            just_swapped = true;
            if (active_condition >= 0) active_wind_dir = frand(0.f, 360.f); // 风向始终在全圆上随机化
        }

        // 体积中心：对于以相机为参照的条件（沙尘暴紧密环绕观察者），否则为相机的目标——玩家，
        // 因此大多数天气跟随您注视的方向。
        const bool cam_centred = active_condition >= 0 && conditions[active_condition].center_on_camera;
        const float cx = cam_centred ? eye[0] : cam->look_at_target.x;
        const float cy = cam_centred ? eye[1] : cam->look_at_target.y;
        const float cz = cam_centred ? eye[2] : cam->look_at_target.z;
        if (just_swapped) center_z = cz; // 在当前高度附近重新播种——本帧无偏移
        const float dcz = cz - center_z; // 本帧聚焦点的垂直移动；柱因此偏移，使其
        center_z = cz;                   // 即使粒子几乎不下降（沙尘暴）也能跟踪玩家高度

        // 用于裁剪屏幕外地面标记的视锥半角（平方正切）：从垂直 FOV + 宽高比外接屏幕矩形，
        // 带 10% 余量。若 FOV 看起来无效则回退为无裁剪。
        float cone_tan_sq = 1e30f;
        if (const float fov = GW::Render::GetFieldOfView(); fov > 0.1f) {
            const int vh = GW::Render::GetViewportHeight();
            const float aspect = vh > 0 ? static_cast<float>(GW::Render::GetViewportWidth()) / static_cast<float>(vh) : 1.7778f;
            const float tan_v = std::tan(fov * 0.5f);
            cone_tan_sq = tan_v * tan_v * (1.f + aspect * aspect) * 1.21f;
        }

        float ambient_target = 0.f;
        if (active_condition >= 0 && ready) {
            auto& c = conditions[active_condition];
            ambient_target = c.ambient * weather_intensity; // 变暗跟随相同的交叉淡入淡出
            ambient_color = c.overcast_tint;
            active_tint = c.tint; // 一个活动条件 -> 整个实例化绘制的一个色调（着色器常量）
            // 对于以相机为参照的风，加上相机的航向（偏航，原点朝东），使风暴在您旋转时保持相同的屏幕方向；
            // 每帧重新计算以实时跟随相机。
            const float heading = active_wind_dir + (c.wind_camera_relative ? cam->yaw * 57.29578f : 0.f);
            // 下落粒子（雨/雪），密度为 0 时跳过（仅有云层的条件）。
            UpdateCondition(c, active_particles, dt, cx, cy, cz, heading, dcz);
            if (c.type == kTypeSnow)
                AppendSnowInstances(snow_instances, c, active_particles.raindrops, right, up, eye, fwd);
            else
                AppendRainInstances(rain_instances, c, active_particles.raindrops, right, fwd, heading, eye);
            // 新生成的溅射也跟随交叉淡入淡出（alpha 是最高字节，无论通道顺序如何）。
            const unsigned int splash_tint = (c.tint & 0x00FFFFFFu) | (static_cast<unsigned int>(((c.tint >> 24) & 0xFFu) * weather_intensity + 0.5f) << 24);
            AppendSplashes(active_particles.splashes, right, splash_tint, eye, fwd, cone_tan_sq);
            snow_flake_count = snow_instances.size(); // 沉积在雪花之后追加；记住它们的起始位置
            AppendSettledInstances(snow_instances, active_particles.settled, eye, fwd, cone_tan_sq);
            // 云层（头顶云/雾/沙尘暴），与上述独立绘制。
            if (c.cloud.top > c.cloud.base) {
                active_has_cloud = true;
                active_cloud_tint = c.cloud.tint;
                UpdateCloudCover(c, active_particles, dt, cx, cy, heading);
                AppendCloudCoverInstances(cloud_instances, c, active_particles.clouds, right, up, eye, fwd, cz);
            }
            else {
                active_particles.clouds.clear();
            }
            UpdateSounds(c, active_particles, dt, cx, cy, cz);
        }
        ambient_strength += (ambient_target - ambient_strength) * std::clamp(dt * 3.f, 0.f, 1.f); // ~1/3 秒缓动
        rain_ready = UploadVB(device, rain_inst_vb, rain_inst_cap, rain_instances.data(), rain_instances.size() * sizeof(WeatherInstance));
        snow_ready = UploadVB(device, snow_inst_vb, snow_inst_cap, snow_instances.data(), snow_instances.size() * sizeof(WeatherInstance));
        cloud_ready = UploadVB(device, cloud_inst_vb, cloud_inst_cap, cloud_instances.data(), cloud_instances.size() * sizeof(WeatherInstance));
        splash_ready = EnsureVb(device, splash_vb, splash_cap, splash_vertices);
#ifdef _DEBUG
        if (debug_wireframe && active_condition >= 0 && ready) {
            const auto& c = conditions[active_condition];
            AppendBoxWire(wire_vertices, cx, cy, c.spread_radius, cz - ColumnHeight(c), cz, 0xFFFF0000u); // 粒子柱（红色）
            if (c.cloud.top > c.cloud.base)
                AppendBoxWire(wire_vertices, cx, cy, c.cloud.radius, cz - c.cloud.top, cz - c.cloud.base, 0xFF00FFFFu); // 云带（青色）
        }
        wire_ready = EnsureVb(device, wire_vb, wire_cap, wire_vertices);
#endif
    }

    // 阴天：在 HUD 下方乘算混合一个铺满屏幕的四边形，使场景像云层一样变暗。
    void DrawAmbient(IDirect3DDevice9* device, const float strength)
    {
        const GW::Camera* cam = GW::CameraMgr::GetCamera();
        if (!cam || strength <= 0.003f) return;

        // 逐通道：按强度将场景向色调缩放（白色 = 不变），然后乘算混合。
        const auto t = ImGui::ColorConvertU32ToFloat4(ambient_color);
        const auto chan = [&](const float c) {
            return static_cast<DWORD>(std::clamp(1.f - strength * (1.f - c), 0.f, 1.f) * 255.f + 0.5f);
        };
        const DWORD col = D3DCOLOR_ARGB(255, chan(t.x), chan(t.y), chan(t.z));

        float fwd[3] = {cam->look_at_target.x - cam->position.x, cam->look_at_target.y - cam->position.y, cam->look_at_target.z - cam->position.z};
        normalize3(fwd);
        constexpr float world_up[3] = {0.f, 0.f, -1.f};
        float right[3];
        cross3(world_up, fwd, right);
        normalize3(right);
        float up[3];
        cross3(fwd, right, up);

        const float d = kZNear + 1.f, hw = d * 4.f; // 紧贴近平面之后，超大以覆盖任何 FOV
        const float qx = cam->position.x + fwd[0] * d, qy = cam->position.y + fwd[1] * d, qz = cam->position.z + fwd[2] * d;
        struct ColVtx {
            float x, y, z;
            DWORD c;
        };
        const auto corner = [&](const float sx, const float sy) -> ColVtx {
            return {qx + (right[0] * sx + up[0] * sy) * hw, qy + (right[1] * sx + up[1] * sy) * hw, qz + (right[2] * sx + up[2] * sy) * hw, col};
        };
        const ColVtx q[6] = {corner(-1, -1), corner(1, -1), corner(1, 1), corner(-1, -1), corner(1, 1), corner(-1, 1)};

        const D3DStateGuard state_guard(device);
        if (GameWorldCompositor::SetupPipeline(device, false, kZFar, 0.f)) {
            device->SetRenderState(D3DRS_ZENABLE, FALSE);
            device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE); // 绝不能触碰深度缓冲，否则会裁剪场景/粒子
            device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
            device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_DESTCOLOR); // 结果 = 场景 * 四边形颜色（乘算）
            device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ZERO);
            device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 2, q, sizeof(ColVtx));
        }
    }

    // 开启自动天气，跟随当前地图的气候（清除任何强制气候）。由 /weather auto 和 /climate auto 共享。
    void EnableAutoWeatherFollowMap()
    {
        auto_climate_override = Climate::None;
        auto_weather = true;
        auto_timer = -1.f; // 立即滚动
        Log::Info("自动天气已开启（按地图）：%s", ClimateName(ClimateForMap(GW::Map::GetMapID())));
    }

    // 关闭自动天气并停止任何正在运行的天气（清除每个活动条件）。由 /weather off 和 /climate off 共享。
    void StopWeather()
    {
        auto_weather = false;
        for (auto& c : conditions)
            c.active = false;
        Log::Info("天气已关闭");
    }

    // 取消当前天气（停用每个条件）而不影响自动天气。自动天气开启时，这会清除天空直到下次计划滚动；
    // /weather off 完全停止它。由 /weather clear 使用。
    void ClearWeather()
    {
        for (auto& c : conditions)
            c.active = false;
        Log::Info("天气已清除");
    }

    // /weather <条件名称> [on|off|toggle|1|0] - 按名称切换条件（名称可包含多个单词）。
    // /weather auto|off|clear - 镜像 /climate auto|off，或取消当前条件（clear）而不停止自动天气。
    void CHAT_CMD_FUNC(CmdWeather)
    {
        if (argc < 2) {
            Log::Info("天气条件：");
            for (const auto& c : conditions)
                Log::Info("  %s：%s", c.name.c_str(), c.active ? "开" : "关");
            Log::Info("用法：/weather <条件> [on|off|toggle] | /weather [auto|off|clear]");
            return;
        }
        if (argc == 2) {
            const std::string only = TextUtils::ToLower(TextUtils::WStringToString(argv[1]));
            if (only == "auto") return EnableAutoWeatherFollowMap();
            if (only == "off") return StopWeather();
            if (only == "clear") return ClearWeather();
        }
        // 末尾单词如果是状态关键词则设置状态；否则整个尾部为名称，命令执行切换。名称可包含空格，
        // 因此拼接所有非状态单词的部分。
        const std::string last = TextUtils::ToLower(TextUtils::WStringToString(argv[argc - 1]));
        int state = -1; // -1 未设置，0 关闭，1 开启，2 切换
        if (last == "on" || last == "1")
            state = 1;
        else if (last == "off" || last == "0")
            state = 0;
        else if (last == "toggle")
            state = 2;
        const int name_end = state < 0 ? argc : argc - 1;
        std::string name;
        for (int i = 1; i < name_end; i++) {
            if (i > 1) name += ' ';
            name += TextUtils::WStringToString(argv[i]);
        }
        if (name.empty()) return Log::Error("用法：/weather <条件> [on|off|toggle]");
        if (state < 0) state = 2; // 未给出显式状态 -> 切换

        const std::string want = TextUtils::ToLower(name);
        for (size_t i = 0; i < conditions.size(); i++) {
            if (TextUtils::ToLower(conditions[i].name) != want) continue;
            const bool on = state == 2 ? !conditions[i].active : state == 1;
            if (on && !conditions[i].enabled) return Log::Error("%s 已禁用；请先在天气设置中启用它。", conditions[i].name.c_str());
            // 一次只有一个条件：激活此条件并清除其他条件（停用只是将其关闭）。
            for (auto& o : conditions)
                o.active = false;
            conditions[i].active = on;
            Log::Info("%s：%s", conditions[i].name.c_str(), on ? "开" : "关");
            return;
        }
        Log::Error("没有名为 '%s' 的天气条件", name.c_str());
    }

    // /climate [auto|off|<气候名称>] - 控制自动天气：'auto' 跟随当前地图的气候，
    // 气候名称强制该气候（无论地图如何），'off' 停止自动天气。
    void CHAT_CMD_FUNC(CmdClimate)
    {
        if (argc < 2) {
            const Climate effective = auto_climate_override != Climate::None ? auto_climate_override : ClimateForMap(GW::Map::GetMapID());
            Log::Info("自动天气：%s", !auto_weather ? "关闭" : auto_climate_override != Climate::None ? "开启（强制气候）" : "开启（按地图）");
            Log::Info("当前气候：%s", ClimateName(effective));
            std::string names;
            for (const auto& k : kClimates) names += (names.empty() ? "" : ", ") + std::string(k.name);
            Log::Info("用法：/climate [auto|off|<气候>] - 气候：%s", names.c_str());
            return;
        }

        // 拼接尾部以便多词气候名称仍可解析（目前没有，但无害）。
        std::string arg;
        for (int i = 1; i < argc; i++) {
            if (i > 1) arg += ' ';
            arg += TextUtils::WStringToString(argv[i]);
        }
        const std::string key = TextUtils::ToLower(arg);

        if (key == "off") return StopWeather();
        if (key == "auto") return EnableAutoWeatherFollowMap();

        Climate climate;
        if (!ClimateByName(arg, climate)) {
            Log::Error("未知气候 '%s'。请使用 'auto'、'off' 或气候名称。", arg.c_str());
            return;
        }
        auto_climate_override = climate;
        auto_weather = true;
        auto_timer = -1.f; // 立即为强制气候滚动
        Log::Info("气候强制为 %s", ClimateName(climate));
    }
} // namespace

void WeatherModule::DrawInWorld(IDirect3DDevice9* device)
{
    if (!textures_requested) {
        raindrop_tex_pp = GwDatModule::LoadTextureFromFileId(kRaindropFileId);
        snowflake_tex_pp = GwDatModule::LoadTextureFromFileId(kSnowflakeFileId);
        splash_tex_pp = GwDatModule::LoadTextureFromFileId(kSplashFileId);
        textures_requested = true;
    }

    // 将物理 + 几何重建节流到 ~30 Hz；缓存的顶点缓冲区仍在下面每帧绘制，
    // 因此运动以 30 Hz 步进前进（使用完整的经过时间 dt），而渲染保持流畅。
    if (!last_update || TIMER_DIFF(last_update) >= kUpdateIntervalMs) {
        const float dt = last_update ? std::clamp(static_cast<float>(TIMER_DIFF(last_update)) / 1000.f, 0.f, 0.1f) : 0.f;
        last_update = TIMER_INIT();
        SyncWeather(device, GW::CameraMgr::GetCamera(), dt);
    }

    DrawAmbient(device, ambient_strength); // 先使场景变暗，使粒子在顶部保持明亮

    IDirect3DTexture9* rain_tex = raindrop_tex_pp ? *raindrop_tex_pp : nullptr;
    IDirect3DTexture9* snow_tex = snowflake_tex_pp ? *snowflake_tex_pp : nullptr;
    IDirect3DTexture9* splash_tex = splash_tex_pp ? *splash_tex_pp : nullptr;
    // 云层使用运行时软云团纹理（无 .dat 资源），首次使用时构建。
    if (active_has_cloud && !cloud_tex) BuildCloudTexture(device);
    const bool have_rain = rain_ready && rain_tex && !rain_instances.empty();
    const bool have_snow = snow_ready && snow_tex && !snow_instances.empty();
    const bool have_cloud = cloud_ready && cloud_tex && !cloud_instances.empty();
    const bool have_splash = splash_ready && splash_tex && !splash_vertices.empty();
    bool have_any = have_rain || have_snow || have_cloud || have_splash;
#ifdef _DEBUG
    have_any = have_any || (wire_ready && !wire_vertices.empty()); // 仅有线框的帧（无粒子）仍绘制
#endif
    if (!have_any || !EnsureShaders(device)) return;

    const D3DStateGuard state_guard(device); // restored on exit so GW's own rendering isn't corrupted
    if (device->SetPixelShader(weather_ps) == D3D_OK && GameWorldCompositor::SetWorldViewProj(device)) {
        GameWorldCompositor::SetWorldRenderStates(device, GameWorldRenderer::GetOccludeBehindTerrain());
        GameWorldCompositor::SetDistanceFog(device, render_max_distance, fog_factor);
        device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
        device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
        device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
        device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
        // 一个共享索引缓冲区服务于两个绘制：实例化绘制仅需要其第一个四边形的 6 个索引。
        const bool ib_ok = EnsureQuadIB(device, std::max<size_t>(splash_vertices.size() / 4, 1)) && device->SetIndices(quad_ib) == D3D_OK;

        // 索引绘制：仅溅射——CPU 构建的四边形，因其每帧精灵表 UV 不适合简单的实例记录。
        if (ib_ok && have_splash && device->SetVertexShader(weather_vs) == D3D_OK && device->SetVertexDeclaration(weather_decl) == D3D_OK && device->SetStreamSource(0, splash_vb, 0, sizeof(WeatherVertex)) == D3D_OK) {
            device->SetTexture(0, splash_tex);
            device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, static_cast<UINT>(splash_vertices.size()), 0, static_cast<UINT>(splash_vertices.size() / 2));
        }

        if (ib_ok && (have_rain || have_snow || have_cloud) && device->SetVertexShader(weather_inst_vs) == D3D_OK && device->SetVertexDeclaration(weather_inst_decl) == D3D_OK && device->SetStreamSource(0, quad_geom_vb, 0, sizeof(GeomVert)) == D3D_OK &&
            device->SetStreamSourceFreq(1, D3DSTREAMSOURCE_INSTANCEDATA | 1u) == D3D_OK) {
            // first = 起始实例（允许雪缓冲区绘制为两个子范围：相机对齐的雪花，然后是地面平铺的沉积），
            // 公告板轴作为 VS 常量 c10/c11 在每个（子）绘制中提供。
            const auto draw_instanced = [&](IDirect3DVertexBuffer9* vb, const UINT first, const UINT ninst, IDirect3DTexture9* tex, const float flip, const unsigned int tint, const InstAxes& ax) {
                if (ninst == 0) return;
                const auto t = ImGui::ColorConvertU32ToFloat4(tint);
                const float tintf[4] = {t.x, t.y, t.z, t.w * weather_intensity}; // 整个绘制跟随交叉淡入淡出
                device->SetVertexShaderConstantF(8, tintf, 1);
                const float flags[4] = {flip, 0.f, 0.f, 0.f};
                device->SetVertexShaderConstantF(9, flags, 1);
                const float axf[4] = {ax.x[0], ax.x[1], ax.x[2], 0.f}, ayf[4] = {ax.y[0], ax.y[1], ax.y[2], 0.f};
                device->SetVertexShaderConstantF(10, axf, 1);
                device->SetVertexShaderConstantF(11, ayf, 1);
                if (device->SetStreamSourceFreq(0, D3DSTREAMSOURCE_INDEXEDDATA | ninst) == D3D_OK && device->SetStreamSource(1, vb, first * sizeof(WeatherInstance), sizeof(WeatherInstance)) == D3D_OK) {
                    device->SetTexture(0, tex);
                    device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, 4, 0, 2);
                }
            };
            if (have_rain) draw_instanced(rain_inst_vb, 0, static_cast<UINT>(rain_instances.size()), rain_tex, 1.f, active_tint, rain_axes);
            if (have_snow) {
                const UINT total = static_cast<UINT>(snow_instances.size()), flakes = static_cast<UINT>(snow_flake_count);
                draw_instanced(snow_inst_vb, 0, flakes, snow_tex, 0.f, active_tint, snow_axes);                       // 相机对齐的雪花
                if (total > flakes) draw_instanced(snow_inst_vb, flakes, total - flakes, snow_tex, 0.f, active_tint, settle_axes); // 地面平铺的沉积
            }
            if (have_cloud) draw_instanced(cloud_inst_vb, 0, static_cast<UINT>(cloud_instances.size()), cloud_tex, 0.f, active_cloud_tint, cloud_axes);
            device->SetStreamSourceFreq(0, 1); // 恢复非实例化频率（状态块也会恢复）
            device->SetStreamSourceFreq(1, 1);
        }

#ifdef _DEBUG
        // 调试体积线框：世界空间线列表，绘制在所有内容之上（深度关闭），无距离淡出，
        // 因此框完全可见。重用公告板 VS/PS，使用纯色纹素。
        if (wire_ready && !wire_vertices.empty()) {
            if (!cloud_tex) BuildCloudTexture(device);
            if (cloud_tex && device->SetVertexShader(weather_vs) == D3D_OK && device->SetVertexDeclaration(weather_decl) == D3D_OK && device->SetStreamSource(0, wire_vb, 0, sizeof(WeatherVertex)) == D3D_OK) {
                GameWorldCompositor::SetDistanceFog(device, 1.0e9f, 0.f); // 覆盖层无距离丢弃/淡出
                device->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);       // 始终可见，即使穿过地形
                device->SetTexture(0, cloud_tex);                        // 中心纹素处不透明（uv 0.5,0.5）
                device->DrawPrimitive(D3DPT_LINELIST, 0, static_cast<UINT>(wire_vertices.size() / 2));
            }
        }
#endif
    }
}

void WeatherModule::RegisterSettings(ToolboxModule* module)
{
    SettingsRegistry::RegisterField(module, "auto_weather", &auto_weather);
    SettingsRegistry::RegisterField(module, "auto_change_min", &auto_change_min);
    SettingsRegistry::RegisterField(module, "auto_change_max", &auto_change_max);
}

void WeatherModule::OnSettingsLoaded()
{
    bool seen_active = false; // 单激活：仅保留第一个活动条件（旧配置可能包含多个）
    for (auto& c : conditions) {
        if (!c.enabled) c.active = false; // 禁用的条件不能是活动条件
        if (c.active && std::exchange(seen_active, true)) c.active = false;
        c.type = std::clamp(c.type, 0, kTypeCount - 1);
        c.floor_decal = std::clamp(c.floor_decal, kDecalAuto, kDecalCount - 1);
        c.drift = std::max(c.drift, kDriftAuto);
        c.wind_dir_max = std::max(c.wind_dir_max, c.wind_dir_min);
        c.wind_tilt = std::clamp(c.wind_tilt, 0.f, 90.f);
        c.density = std::clamp(c.density, 0, 100); // 0 = 无下落粒子（仅有云层的条件）
        c.spread_radius = std::clamp(c.spread_radius, 250.f, kMaxRadius); // 用户可编辑，但限制在罗盘半径范围内
        c.splash_chance = std::clamp(c.splash_chance, 0.f, 1.f);
        c.sound_min_interval = std::max(c.sound_min_interval, 0.f);
        c.sound_max_interval = std::max(c.sound_max_interval, c.sound_min_interval);
        c.ambient = std::clamp(c.ambient, 0.f, 1.f);
    }
    auto_change_min = std::max(auto_change_min, 0.1f);
    auto_change_max = std::max(auto_change_max, auto_change_min);
    for (auto& cp : climate_profiles)
        for (auto& e : cp.entries)
            e.weight = std::clamp(e.weight, 0.f, 1.f);
}

void WeatherModule::LoadSettings(SettingsDoc& doc, ToolboxIni* legacy)
{
    ToolboxModule::LoadSettings(doc, legacy);
    doc.Get(Name(), "conditions", conditions);             // 若键缺失则保留默认值
    doc.Get(Name(), "climate_profiles", climate_profiles); // 自动天气的气候->天气表
    OnSettingsLoaded();
}

void WeatherModule::SaveSettings(SettingsDoc& doc)
{
    ToolboxModule::SaveSettings(doc);
    doc.Set(Name(), "conditions", conditions);
    doc.Set(Name(), "climate_profiles", climate_profiles);
}

void WeatherModule::DrawSettings()
{
    const auto red = ImGui::ColorConvertU32ToFloat4(Colors::Red());
    const auto green = ImGui::ColorConvertU32ToFloat4(Colors::Green());
    if (!GameWorldCompositor::IsActive()) ImGui::TextColored(red, GameWorldCompositor::HasFailed() ? "  世界内合成器安装失败。" : "  世界内合成器：尚未安装。");

    // 信息：当前地图的气候，以及当前显示的条件。
    if (const GW::AreaInfo* info = GW::Map::GetCurrentMapInfo())
        ImGui::Text("当前地图：%s（气候：%s）", Resources::GetRegionName(info->region)->string().c_str(), ClimateName(ClimateForMap(GW::Map::GetMapID())));
    if (auto_climate_override != Climate::None)
        ImGui::Text("强制气候：%s（取消下方勾选，或勾选“自动天气（跟随地图气候）”）", ClimateName(auto_climate_override));
    std::string showing;
    for (const auto& c : conditions)
        if (c.active) showing += (showing.empty() ? "" : ", ") + c.name;
    ImGui::Text("当前显示：%s", showing.empty() ? "晴朗" : showing.c_str());
#ifdef _DEBUG
    ImGui::Text("实时：%d 下落粒子，%d 云团 | 贴花：%d 溅射，%d 沉积",
                static_cast<int>(active_particles.raindrops.size()), static_cast<int>(active_particles.clouds.size()),
                static_cast<int>(active_particles.splashes.size()), static_cast<int>(active_particles.settled.size()));
#endif
#ifdef _DEBUG
    ImGui::Checkbox("显示体积线框（调试）", &debug_wireframe);
    ImGui::ShowHelp("在活动条件的粒子柱（红色）和云带（青色）周围绘制框，以便查看模拟使用的体积。仅调试版本。");
#endif

    ImGui::SeparatorText("天气条件");
    int to_remove = -1, to_duplicate = -1;
    for (int i = 0; i < static_cast<int>(conditions.size()); i++) {
        auto& c = conditions[i];
        ImGui::PushID(i);
        ImGui::BeginDisabled(auto_weather || !c.enabled); // 自动天气拥有活动状态；禁用条件不能被激活
        if (ImGui::Checkbox("##active", &c.active) && c.active) // 一次只有一个条件：开启一个会关闭其他
            for (int j = 0; j < static_cast<int>(conditions.size()); j++)
                if (j != i) conditions[j].active = false;
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) { // 解释禁用状态
            if (!c.enabled) ImGui::SetTooltip("此条件已禁用。\n在其设置中勾选“启用”以允许使用。");
            else if (auto_weather) ImGui::SetTooltip("自动天气已开启，控制哪个条件激活。\n关闭下方的“自动天气”以手动切换条件。");
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        char header[80];
        snprintf(header, sizeof(header), "%s%s###cond", c.name.empty() ? "(未命名)" : c.name.c_str(), c.enabled ? "" : " [已禁用]"); // 稳定 ID，使编辑名称不会折叠/取消聚焦此节
        if (ImGui::CollapsingHeader(header)) {
            if (ImGui::Checkbox("启用", &c.enabled) && !c.enabled) c.active = false; // 禁用条件不能保持激活
            ImGui::ShowHelp("取消勾选以将此条件从手动选择和自动天气滚动中排除。");
            ImGui::InputText("名称", c.name, 32);
            ImGui::TextDisabled("下落粒子（对于仅有云层的条件如雾，将密度设为 0）。");
            const char* type_names[kTypeCount] = {"雨", "雪"};
            ImGui::Combo("类型", &c.type, type_names, kTypeCount);
            ImGui::DragInt("密度", &c.density, 1.f, 0, 100, "%d%%", ImGuiSliderFlags_AlwaysClamp);
            ImGui::ShowHelp("体积填充密度，0-100%。0 = 无下落粒子（仅有云层）。\n粒子数由此和散布面积派生，因此半径变化时保持一致。越高对 FPS 影响越大。");
            ImGui::Text("  ~%d 粒子", DropCount(c));
            ImGui::DragFloat("范围", &c.spread_radius, 25.f, 250.f, kMaxRadius, "%.0f", ImGuiSliderFlags_AlwaysClamp);
            ImGui::ShowHelp("聚焦点周围天气体积的半径（也是云层的水平范围）。");
            ImGui::DragFloat("柱高", &c.column_height, 25.f, 50.f, column_height_max, "%.0f", ImGuiSliderFlags_AlwaysClamp);
            ImGui::ShowHelp("你上方下落粒子柱的高度。");
            ImGui::DragFloat("粒子大小", &c.drop_size, 1.f, 1.f, 500.f, "%.0f");
            ImGui::DragFloat("下落速度", &c.fall_speed, 50.f, 0.f, 30000.f, "%.0f");
            ImGui::ShowHelp("粒子运动的恒定速度。风倾斜其方向而不改变此速度。");
            // 风向始终在条件激活时在全 0-360 度圆上滚动；
            // wind_dir_min/max 字段保留（不再暴露于 UI），以备将来需要方向范围时使用。
            ImGui::DragFloat("风倾斜", &c.wind_tilt, 1.f, 0.f, 90.f, "%.0f 度", ImGuiSliderFlags_AlwaysClamp);
            ImGui::ShowHelp("下落方向偏离垂直方向的角度：0 = 垂直，90 = 完全水平（无下落——\n水平飘移，永不沉降，例如沙尘暴）。");
            ImGui::Checkbox("风相对于相机", &c.wind_camera_relative);
            ImGui::ShowHelp("从相机而非世界测量风向，使风暴在旋转相机时保持相同的屏幕方向\n（例如始终横穿视野）。");
            ImGui::Checkbox("以相机为中心", &c.center_on_camera);
            ImGui::ShowHelp("将体积聚焦在相机本身而非其目标上，使效果紧密环绕观察者并填充近景。\n最好与较小范围搭配使用（例如沙尘暴）。");
            if (c.type == kTypeSnow) {
                float drift = EffectiveDrift(c);
                if (ImGui::DragFloat("飘移", &drift, 1.f, 0.f, 1000.f, "%.0f", ImGuiSliderFlags_AlwaysClamp)) c.drift = drift;
                ImGui::ShowHelp("雪花下落时横向摆动的幅度。0 = 直线下落。");
            }
            const char* decal_names[kDecalCount] = {"无", "溅射", "沉积"};
            int decal = EffectiveDecal(c);
            if (ImGui::Combo("地面贴花", &decal, decal_names, kDecalCount)) c.floor_decal = decal;
            ImGui::ShowHelp("每个撞击点留在地面上的效果：无、水花溅射或沉积的雪花/斑点。");
            if (decal != kDecalNone) {
                ImGui::DragFloat("地面贴花概率", &c.splash_chance, 0.01f, 0.f, 1.f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
                ImGui::ShowHelp("粒子着陆时留下地面贴花（溅射或沉积）的概率（0..1）。");
            }
            auto ct = ImGui::ColorConvertU32ToFloat4(c.tint);
            if (ImGui::ColorEdit4("色调", &ct.x, ImGuiColorEditFlags_AlphaBar)) c.tint = ImGui::ColorConvertFloat4ToU32(ct);
            ImGui::ShowHelp("此条件的粒子颜色——例如灰烬的深灰色。");
            ImGui::DragFloat("阴天强度", &c.ambient, 0.01f, 0.f, 1.f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
            ImGui::ShowHelp("此条件激活时使场景向其阴天色调变暗的强度。");
            auto oc = ImGui::ColorConvertU32ToFloat4(c.overcast_tint);
            if (ImGui::ColorEdit3("阴天色调", &oc.x)) c.overcast_tint = ImGui::ColorConvertFloat4ToU32(oc);
            ImGui::ShowHelp("此条件驱动阴天时场景变暗的颜色。");

            ImGui::Separator();
            bool cloud_on = c.cloud.top > c.cloud.base;
            if (ImGui::Checkbox("云层", &cloud_on)) {
                if (cloud_on && c.cloud.top <= c.cloud.base) { c.cloud.base = 0.f; c.cloud.top = 1000.f; } // 启用时使用默认雾带
                else if (!cloud_on) c.cloud.top = c.cloud.base;                                            // 禁用
            }
            ImGui::ShowHelp("在您上方的带内添加柔和的云/雾层，覆盖在下落粒子之上。\n这就是组合效果的方式——例如在雪上启用此项可得到暴风雪（雪 + 雾）。");
            if (c.cloud.top > c.cloud.base) {
                ImGui::DragFloatRange2("云带（你上方）", &c.cloud.base, &c.cloud.top, 10.f, -500.f, 2500.f, "%.0f", "%.0f", ImGuiSliderFlags_AlwaysClamp);
                ImGui::ShowHelp("云带的底部和顶部，玩家上方 gwinch 数。雨云 ~1000-1500，雾 ~0-1000，地面沙尘暴 ~0-200。");
                ImGui::DragFloat("云半径", &c.cloud.radius, 25.f, 250.f, kMaxRadius, "%.0f", ImGuiSliderFlags_AlwaysClamp);
                ImGui::ShowHelp("云层的水平半径，独立于上方的粒子 Range（例如宽大的头顶云层覆盖小雨体积）。");
                ImGui::DragInt("云密度", &c.cloud.density, 1.f, 1, 100, "%d%%", ImGuiSliderFlags_AlwaysClamp);
                ImGui::Text("  ~%d 云团", CloudCount(c));
                ImGui::DragFloat("云团大小", &c.cloud.size, 5.f, 50.f, 3000.f, "%.0f");
                ImGui::DragFloat("云漂移速度", &c.cloud.speed, 5.f, 0.f, 5000.f, "%.0f");
                ImGui::ShowHelp("云层的水平漂移；方向遵循风向。高值用于吹沙尘暴。");
                auto cct = ImGui::ColorConvertU32ToFloat4(c.cloud.tint);
                if (ImGui::ColorEdit4("云色调", &cct.x, ImGuiColorEditFlags_AlphaBar)) c.cloud.tint = ImGui::ColorConvertFloat4ToU32(cct);
                ImGui::ShowHelp("云的颜色和不透明度（alpha）——深灰色雨云、白色雾、黄褐色沙尘。");
            }

            ImGui::TextUnformatted("音效（文件 ID，激活时随机播放）");
            int snd_remove = -1;
            for (int s = 0; s < static_cast<int>(c.sounds.size()); s++) {
                ImGui::PushID(s);
                ImGui::SetNextItemWidth(120.f);
                ImGui::InputScalar("##sid", ImGuiDataType_U32, &c.sounds[s], nullptr, nullptr, "%X", ImGuiInputTextFlags_CharsHexadecimal);
                const bool ok = IsValidSound(c.sounds[s]);
                ImGui::SameLine();
                ImGui::TextColored(ok ? green : red, ok ? ICON_FA_CHECK : ICON_FA_TIMES);
                ImGui::SameLine();
                if (ImGui::SmallButton("试听")) AudioSettings::PlaySoundFileId(c.sounds[s]);
                ImGui::SameLine();
                if (ImGui::SmallButton("移除##snd")) snd_remove = s;
                ImGui::PopID();
            }
            if (snd_remove >= 0) c.sounds.erase(c.sounds.begin() + snd_remove);
            if (ImGui::SmallButton("添加音效")) c.sounds.push_back(0);
            ImGui::DragFloat2("音效间隔（秒）", &c.sound_min_interval, 0.5f, 0.f, 600.f, "%.0f");
            ImGui::Checkbox("3D 音效", &c.sound_3d);
            ImGui::ShowHelp("从随机附近位置播放每个音效，使游戏的 3D 音频改变其\n音量和立体声声像——在只有一个音效时减少重复感。\n如果音效不是位置性的则无效。");

            if (ImGui::Button("复制")) to_duplicate = i;
            ImGui::SameLine();
            if (ImGui::Button("移除条件")) to_remove = i;
        }
        ImGui::PopID();
    }
    if (to_remove >= 0) {
        conditions.erase(conditions.begin() + to_remove);
        active_particles = {};  // 释放实时缓冲区，让 SyncWeather 重新解析（索引已偏移）
        active_condition = -1;
    }
    if (to_duplicate >= 0) {
        WeatherCondition copy = conditions[to_duplicate]; // 先复制：insert 可能重新分配向量
        copy.name += " 副本";
        copy.active = false; // 单激活：副本从关闭开始
        conditions.insert(conditions.begin() + to_duplicate + 1, std::move(copy));
    }

    if (ImGui::Button("添加条件")) conditions.push_back({});
    ImGui::SameLine();
    if (ImGui::Button("重置为默认")) ImGui::OpenPopup("重置天气条件？");
    if (ImGui::BeginPopupModal("重置天气条件？", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("将所有条件替换为 %d 个默认值？", static_cast<int>(DefaultConditions().size()));
        if (ImGui::Button("重置")) {
            conditions = DefaultConditions();
            active_particles = {};
            active_condition = -1;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("取消")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    ImGui::SeparatorText("气候");
    // “跟随地图”自动 = 从当前地图的气候滚动（override 为 None）。强制特定气候通过下方的每个气候勾选框完成；
    // 两者互斥，因此跟随地图会禁用这些勾选框。
    bool follow_map = auto_weather && auto_climate_override == Climate::None;
    if (ImGui::Checkbox("自动天气（跟随地图气候）", &follow_map)) {
        auto_weather = follow_map;
        auto_climate_override = Climate::None;
        if (follow_map) auto_timer = -1.f; // 启用时立即滚动
    }
    ImGui::ShowHelp("从当前地图的气候自动滚动活动天气，每隔几分钟重新滚动一次。\n开启时，手动条件切换和下方的每个气候勾选框将被禁用。\n\n/climate 聊天命令镜像此功能：/climate auto（跟随地图），/climate <名称>（强制气候），/climate off。");
    if (auto_weather) // 两个独立的全局变量（不是连续的一对），因此使用 DragFloatRange2 的两个指针——而非 DragFloat2
        ImGui::DragFloatRange2("变化间隔（分钟）", &auto_change_min, &auto_change_max, 0.25f, 0.1f, 240.f, "%.1f", "%.1f", ImGuiSliderFlags_AlwaysClamp);

    ImGui::PushID("climates"); // 使这些部件 ID 不与上面的条件循环冲突
    int climate_remove = -1;
    for (int i = 0; i < static_cast<int>(climate_profiles.size()); i++) {
        auto& cp = climate_profiles[i];
        ImGui::PushID(i);
        // 每个气候勾选框：将自动天气强制为此气候（单选），跟随地图时禁用。
        bool forced = auto_weather && auto_climate_override == cp.climate;
        ImGui::BeginDisabled(follow_map);
        if (ImGui::Checkbox("##climate_active", &forced)) {
            if (forced) { auto_climate_override = cp.climate; auto_weather = true; auto_timer = -1.f; }
            else { auto_climate_override = Climate::None; auto_weather = false; }
        }
        if (follow_map && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) // 解释禁用状态
            ImGui::SetTooltip("自动天气正在跟随地图气候。\n关闭上方的“自动天气（跟随地图气候）”以强制特定气候。");
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::CollapsingHeader(ClimateName(cp.climate))) {
            int ent_remove = -1;
            for (int e = 0; e < static_cast<int>(cp.entries.size()); e++) {
                ImGui::PushID(e);
                ImGui::SetNextItemWidth(160.f);
                if (ImGui::BeginCombo("##cond", cp.entries[e].condition.c_str())) {
                    for (const auto& c : conditions)
                        if (ImGui::Selectable(c.name.c_str(), c.name == cp.entries[e].condition)) cp.entries[e].condition = c.name;
                    ImGui::EndCombo();
                }
                ImGui::SameLine();
                ImGui::SetNextItemWidth(110.f);
                // Shown as a 0-100% chance, but stored as the 0..1 fraction it has always been (existing configs load unchanged).
                float weight_pct = cp.entries[e].weight * 100.f;
                if (ImGui::DragFloat("##weight", &weight_pct, 0.1f, 0.f, 100.f, "%.1f%%", ImGuiSliderFlags_AlwaysClamp))
                    cp.entries[e].weight = std::clamp(weight_pct / 100.f, 0.f, 1.f);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Chance this condition is picked for the climate on each weather roll.");
                ImGui::SameLine();
                if (ImGui::SmallButton("移除##ent")) ent_remove = e;
                ImGui::PopID();
            }
            if (ent_remove >= 0) cp.entries.erase(cp.entries.begin() + ent_remove);
            if (ImGui::SmallButton("添加条件##ent")) cp.entries.push_back({conditions.empty() ? "" : conditions.front().name, 0.3f});
            float sum = 0.f;
            for (const auto& e : cp.entries) sum += std::max(0.f, e.weight);
            ImGui::Text("Clear weather: %.1f%%", std::max(0.f, 1.f - sum) * 100.f);
            if (ImGui::Button("Remove climate")) climate_remove = i;
        }
        ImGui::PopID();
    }
    if (climate_remove >= 0) climate_profiles.erase(climate_profiles.begin() + climate_remove);

    if (ImGui::Button("添加气候")) ImGui::OpenPopup("add_climate");
    if (ImGui::BeginPopup("add_climate")) {
        for (const auto& k : kClimates) {
            if (std::any_of(climate_profiles.begin(), climate_profiles.end(), [&](const ClimateProfile& cp) { return cp.climate == k.climate; })) continue;
            if (ImGui::Selectable(k.name)) climate_profiles.push_back({k.climate, {}});
        }
        ImGui::EndPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("重置气候表")) ImGui::OpenPopup("重置气候表？");
    if (ImGui::BeginPopupModal("重置气候表？", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("将气候->天气表替换为 %d 个默认值？", static_cast<int>(DefaultClimateProfiles().size()));
        if (ImGui::Button("重置")) {
            climate_profiles = DefaultClimateProfiles();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("取消")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    ImGui::PopID();
}

void WeatherModule::Reset()
{
    reset_requested = true; // 由 SyncWeather 在下次更新时消费（清除粒子 -> 重新播种）
}

void WeatherModule::Initialize()
{
    ToolboxModule::Initialize();
    RegisterSettings(this);
    if (!compositor_token) compositor_token = GameWorldCompositor::RegisterDraw(&WeatherModule::DrawInWorld);
    GW::Chat::CreateCommand(&chat_hook_entry, L"weather", CmdWeather);
    GW::Chat::CreateCommand(&chat_hook_entry, L"climate", CmdClimate);
}

void WeatherModule::DrawSettingsInternal()
{
    DrawSettings();
}

void WeatherModule::SignalTerminate()
{
    if (compositor_token) {
        GameWorldCompositor::UnregisterDraw(compositor_token);
        compositor_token = 0;
    }
    GW::Chat::DeleteCommand(&chat_hook_entry);
}

void WeatherModule::Terminate()
{
    SignalTerminate();
    for (auto** p : {&rain_inst_vb, &snow_inst_vb, &cloud_inst_vb, &splash_vb, &quad_geom_vb})
        if (*p) {
            (*p)->Release();
            *p = nullptr;
        }
    if (quad_ib) {
        quad_ib->Release();
        quad_ib = nullptr;
    }
    if (cloud_tex) { // 运行时构建；下次使用时惰性重建
        cloud_tex->Release();
        cloud_tex = nullptr;
    }
#ifdef _DEBUG
    if (wire_vb) {
        wire_vb->Release();
        wire_vb = nullptr;
    }
    wire_cap = 0;
#endif
    for (auto** s : {&weather_vs, &weather_inst_vs})
        if (*s) {
            (*s)->Release();
            *s = nullptr;
        }
    if (weather_ps) {
        weather_ps->Release();
        weather_ps = nullptr;
    }
    for (auto** d : {&weather_decl, &weather_inst_decl})
        if (*d) {
            (*d)->Release();
            *d = nullptr;
        }
    rain_inst_cap = snow_inst_cap = cloud_inst_cap = splash_cap = quad_ib_quads = 0;
    active_particles = {};
    active_condition = -1;
    raindrop_tex_pp = snowflake_tex_pp = splash_tex_pp = nullptr; // 由 GwDatModule 的缓存拥有
    textures_requested = false;
    last_update = 0;
    auto_climate = Climate::Temperate;
    auto_timer = -1.f;
    ToolboxModule::Terminate();
}
