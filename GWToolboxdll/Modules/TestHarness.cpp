#include "stdafx.h"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <set>
#include <sstream>
#include <string>
#include <system_error>

#include <GWCA/GameContainers/GamePos.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Pathing.h>
#include <GWCA/GameEntities/Party.h>
#include <GWCA/GameEntities/Skill.h>
#include <GWCA/GameEntities/Quest.h>
#include <GWCA/Context/CharContext.h>
#include <GWCA/Context/MapContext.h>
#include <GWCA/Context/PreGameContext.h>
#include <GWCA/Constants/Constants.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/CameraMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/ItemMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/MemoryMgr.h>
#include <GWCA/Managers/QuestMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <GWCA/Packets/StoC.h>

#include <Logger.h>
#include <Timer.h>

#include "GWToolbox.h"
#include "Modules/QuestModule.h"
#include "Modules/Resources.h"
#include "Modules/TestHarness.h"

#include <GWCA/Managers/PartyMgr.h>

#include "Modules/CartographerModule.h"
#include "Modules/SkillRangeRingsModule.h"
#include "Utils/GameWorldCompositor.h"
#include "Utils/PropSurfaceIndex.h"
#include "Utils/SettingsRegistry.h"
#include "Utils/TerrainDrape.h"
#include "Utils/TextUtils.h"
#include "Utils/ToolboxUtils.h"
#include "Widgets/WorldMapWidget.h"
#include "Windows/Pathfinding/PathfindingWindow.h"
#include "Windows/TravelWindow.h"
#include "Modules/GwDatModule.h"
#include "Utils/ArenaNetFileParser.h"

// 开发迭代工具：在 Debug (_DEBUG) 和 RelWithDebInfo (GWTB_HARNESS) 中编译，
// 日志写入 log.txt，在正式发布版中排除。
#if defined(_DEBUG) || defined(GWTB_HARNESS)
#define HARNESS_ENABLED 1
#endif

#ifdef HARNESS_ENABLED
namespace {
    constexpr long kPollMs = 250; // ~4 Hz；FindPath 可能卡顿一帧，不要更快轮询

    clock_t last_poll = 0;
    bool fired_waypoint_this_load = false;
    bool terminating = false; // 一旦发出关闭信号则设为 true；Update 不再操作
    int chest_on_load_remaining = 0; // 回归测试 Xunlai 自动打开崩溃：在加载后窗口内切换箱子，N 次加载
    int chest_burst = -1;            // -1 空闲；0..N 统计加载后切换爆发轮询次数

    // 效果注入实验（Ray-of-Judgment-at-a-point 研发）：记录传入的 PlayEffect id，
    // 以便发现技能的视觉效果 id，并本地模拟 PlayEffect 以确认其在指定坐标渲染。
    bool log_play_effects = false;
    GW::HookEntry PlayEffect_Entry;

    bool fps_active = false;
    int fps_frames = 0;
    clock_t fps_start = 0;
    long fps_duration_ms = 0;

    std::filesystem::path cmd_path() { return Resources::GetPath(L"harness_command.txt"); }
    std::filesystem::path status_path() { return Resources::GetPath(L"harness_status.txt"); }
    std::filesystem::path config_path() { return Resources::GetPath(L"harness_config.txt"); }

    void write_status(const std::string& s) { Resources::WriteFile(status_path(), s); }

    std::string trim(const std::string& s)
    {
        const auto a = s.find_first_not_of(" \t\r\n");
        if (a == std::string::npos) return {};
        const auto b = s.find_last_not_of(" \t\r\n");
        return s.substr(a, b - a + 1);
    }

    struct Config {
        float wx = 0, wy = 0;
        uint32_t wplane = 0;
        bool have_waypoint = false;
        bool autostart = false;
    };

    // 向 GW 窗口发送回车键（推进已预填的账户登录和角色选择）
    void press_enter()
    {
        if (HWND hwnd = GW::MemoryMgr::GetGWWindowHandle()) {
            PostMessageW(hwnd, WM_KEYDOWN, VK_RETURN, 0);
            PostMessageW(hwnd, WM_KEYUP, VK_RETURN, 0);
        }
    }

    Config read_config()
    {
        Config c;
        std::string body;
        if (!Resources::ReadFile(config_path(), body)) return c;
        std::istringstream is(body);
        std::string line;
        while (std::getline(is, line)) {
            line = trim(line);
            if (line.empty() || line[0] == '#') continue;
            const auto eq = line.find('=');
            if (eq == std::string::npos) continue;
            const std::string key = trim(line.substr(0, eq));
            const std::string val = trim(line.substr(eq + 1));
            if (key == "waypoint") {
                std::istringstream vs(val);
                if (vs >> c.wx >> c.wy >> c.wplane) c.have_waypoint = true;
            }
            else if (key == "autostart") {
                c.autostart = atoi(val.c_str()) != 0;
            }
        }
        return c;
    }

    bool map_ready()
    {
        if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading) return false;
        const auto cc = GW::GetCharContext();
        return cc && cc->player_name && cc->player_name[0];
    }

    // 驱动真实的路径目标，完全等同于世界地图右键“放置标记”：
    // 设置自定义任务标记，让 QuestModule 计算并绘制真实的任务路径（通过可见图）。
    // 这取代了旧的 PathfindingWindow 开发覆盖层（它绘制自己的起点/终点十字和路径）。
    // 目标是当前地图的游戏坐标；SetCustomQuestMarker 接受世界地图坐标。
    void do_path_to(const GW::GamePos& goal, const char* src)
    {
        const auto self = GW::Agents::GetControlledCharacter();
        if (!self) { write_status("path_failed: 没有受控角色"); return; }
        GW::Vec2f world_pos;
        if (!WorldMapWidget::GamePosToWorldMap(goal, world_pos)) {
            write_status("path_failed: GamePosToWorldMap 失败");
            return;
        }
        QuestModule::SetCustomQuestMarker(world_pos, true);
        char buf[180];
        snprintf(buf, sizeof(buf), "path_set(%s): 从(%.0f,%.0f,z%u) 到(%.0f,%.0f,z%u)",
                 src, self->pos.x, self->pos.y, self->pos.zplane, goal.x, goal.y, goal.zplane);
        write_status(buf);
        Log::Log("[harness] %s", buf);
    }

    // 活动任务的标记（产生实时任务路径的目标点）。若无或无效则返回 false。
    bool quest_marker(GW::GamePos& out)
    {
        const GW::Quest* q = GW::QuestMgr::GetActiveQuest();
        if (!q || std::isinf(q->marker.x) || std::isinf(q->marker.y)) return false;
        out = q->marker;
        return true;
    }

    // 解析可靠目标：优先使用显式捕获的配置路径点，否则使用活动任务标记。
    // 配置路径点由 `setgoal`（玩家位置）捕获，并在重新注入时持久存在。
    bool resolve_goal(const Config& cfg, GW::GamePos& out, const char*& src)
    {
        if (cfg.have_waypoint) { out = GW::GamePos(cfg.wx, cfg.wy, cfg.wplane); src = "config"; return true; }
        if (quest_marker(out)) { src = "quest"; return true; }
        return false;
    }

    // 将玩家当前位置捕获为持久目标（覆盖 harness_config.txt）。
    void set_goal_here()
    {
        const auto self = GW::Agents::GetControlledCharacter();
        if (!self) { write_status("setgoal: 没有受控角色"); return; }
        char cfg[256];
        snprintf(cfg, sizeof(cfg), "# 已捕获目标（玩家位置）\nwaypoint=%.1f %.1f %u\nautostart=1\n",
                 self->pos.x, self->pos.y, self->pos.zplane);
        Resources::WriteFile(config_path(), cfg);
        char buf[128];
        snprintf(buf, sizeof(buf), "goal_captured: (%.0f,%.0f,z%u)", self->pos.x, self->pos.y, self->pos.zplane);
        write_status(buf);
        Log::Log("[harness] %s", buf);
    }

    // 执行一条通道命令。shutdown() 必须是最后一条，且之后不得触碰任何状态。
    void run_command(const std::string& line)
    {
        std::istringstream is(line);
        std::string verb;
        is >> verb;
        if (verb == "shutdown") {
            write_status("shutdown_signalled");
            Log::Log("[harness] 已发出关闭信号；卸载 DLL（GW 保持运行）");
            terminating = true;
            GWToolbox::SignalTerminate(true); // 干净地自我卸载；级联关闭各模块
            return;                            // 此后不得触碰任何东西
        }
        if (verb == "login") {
            press_enter();
            write_status("login: 已发送回车");
            return;
        }
        if (verb == "setgoal") { // 将玩家位置捕获为持久目标
            set_goal_here();
            return;
        }
        if (verb == "repath") { // 重新触发到已捕获目标（或活动任务标记）的路径
            GW::GamePos goal;
            const char* src = "";
            if (resolve_goal(read_config(), goal, src)) do_path_to(goal, src);
            else write_status("repath: 没有配置目标或活动任务标记（请先使用 'setgoal'）");
            return;
        }
        if (verb == "dumpnav") { // 转储某点附近的导航网格多边形：dumpnav [x y [radius]]（默认：配置目标/玩家位置，r=1500）
            float x = 0, y = 0, radius = 1500.f;
            bool have_xy = (bool)(is >> x >> y);
            is >> radius;
            GW::GamePos center;
            if (have_xy) { center = GW::GamePos(x, y, 0); }
            else {
                const char* src = "";
                if (!resolve_goal(read_config(), center, src)) {
                    const auto self = GW::Agents::GetControlledCharacter();
                    if (!self) { write_status("dumpnav: 没有坐标、目标和角色"); return; }
                    center = self->pos;
                }
            }
            const bool ok = PathfindingWindow::DebugDumpNavMeshNear(center, radius);
            if (const auto self = GW::Agents::GetControlledCharacter())
                Log::Log("[navdump] 玩家=(%.0f,%.0f,z%u) z=%.0f 朝向=%.3f 相机偏航=%.3f",
                         self->pos.x, self->pos.y, self->pos.zplane, self->z, self->rotation_angle, GW::CameraMgr::GetYaw());
            char buf[160];
            snprintf(buf, sizeof(buf), "dumpnav(%.0f,%.0f,r%.0f): %s", center.x, center.y, radius, ok ? "已转储" : "未就绪（重试）");
            write_status(buf);
            Log::Log("[harness] %s", buf);
            return;
        }
        if (verb == "heightline") { // heightline x1 y1 x2 y2 plane [n]：在指定平面上的线段上对 n 个点调用 QueryAltitude
            float x1, y1, x2, y2;
            uint32_t plane = 0, n = 16;
            if (is >> x1 >> y1 >> x2 >> y2 >> plane) {
                is >> n;
                if (n < 2) n = 2;
                if (n > 64) n = 64;
                Log::Log("[heightline] (%.0f,%.0f)->(%.0f,%.0f) 平面=%u n=%u", x1, y1, x2, y2, plane, n);
                for (uint32_t i = 0; i < n; ++i) {
                    const float t = (float)i / (float)(n - 1);
                    const float x = x1 + (x2 - x1) * t, y = y1 + (y2 - y1) * t;
                    const float a = TerrainDrape::QueryAltAt(x, y, plane);
                    Log::Log("[heightline]   t=%.2f (%.0f,%.0f) 平面%u_高度=%.1f", t, x, y, plane, a);
                }
                write_status("heightline: 已记录");
            }
            else {
                write_status("heightline: 参数错误（需要 x1 y1 x2 y2 plane [n]）");
            }
            return;
        }
        if (verb == "surfaceprobe") { // surfaceprobe [radius step]：比较玩家附近的首选平面地形与所有平面地表选择
            float radius = 1200.f, step = 120.f;
            is >> radius >> step;
            radius = std::clamp(radius, 100.f, 5000.f);
            step = std::clamp(step, 25.f, 500.f);
            const auto self = GW::Agents::GetControlledCharacter();
            const auto* pm = GW::Map::GetPathingMap();
            if (!self || !pm || !pm->size()) {
                write_status("surfaceprobe: 没有角色或路径地图");
                return;
            }
            Log::Log("[surfaceprobe] 地图=%d 玩家=(%.0f,%.0f,z%u) 玩家_z=%.1f 半径=%.0f 步长=%.0f 平面数=%u",
                     static_cast<int>(GW::Map::GetMapID()), self->pos.x, self->pos.y, self->pos.zplane, self->z, radius, step, pm->size());
            uint32_t samples = 0, diffs = 0, logged = 0;
            float max_delta = 0.f;
            for (float dx = -radius; dx <= radius; dx += step) {
                for (float dy = -radius; dy <= radius; dy += step) {
                    const float x = self->pos.x + dx, y = self->pos.y + dy;
                    const float preferred_z = TerrainDrape::QueryAltAt(x, y, self->pos.zplane);
                    const float surface_z = TerrainDrape::SurfaceZ(x, y, self->pos.zplane, pm->size());
                    const float draped_z = TerrainDrape::DrapeZ(x, y, self->pos.zplane, pm->size(), self->z);
                    ++samples;
                    if ((preferred_z == 0.f && surface_z == 0.f) || std::fabs(preferred_z - surface_z) <= 1.f) continue;
                    ++diffs;
                    max_delta = std::max(max_delta, std::fabs(preferred_z - surface_z));
                    if (logged++ < 20) {
                        Log::Log("[surfaceprobe]   diff (%.0f,%.0f) 首选=%.1f 地表=%.1f 拖拽=%.1f 差值=%.1f",
                                 x, y, preferred_z, surface_z, draped_z, surface_z - preferred_z);
                    }
                }
            }
            char b[128];
            snprintf(b, sizeof(b), "surfaceprobe: 采样数=%u 差异数=%u 最大差值=%.1f", samples, diffs, max_delta);
            Log::Log("[harness] %s", b);
            Log::FlushFile();
            write_status(b);
            return;
        }
        if (verb == "waypoint") {
            float x = 0, y = 0;
            uint32_t plane = 0;
            if (is >> x >> y) {
                is >> plane; // plane 可选，默认为 0
                do_path_to(GW::GamePos(x, y, plane), "cmd");
            }
            else {
                write_status("waypoint: 参数错误（需要: waypoint <x> <y> [plane]）");
            }
            return;
        }
        if (verb == "heropanels") {
            const auto party = GW::PartyMgr::GetPartyInfo();
            const auto hero_count = party ? party->heroes.size() : 0;
            Log::Log("[heropanels] party=%p heroes=%u instance=%d", static_cast<void*>(party), static_cast<unsigned>(hero_count), static_cast<int>(GW::Map::GetInstanceType()));
            wchar_t label[] = L"AgentCommander0";
            for (int i = 0; i < 7; i++) {
                label[_countof(label) - 2] = static_cast<wchar_t>(L'0' + i);
                const auto frame = GW::UI::GetFrameByLabel(label);
                if (!frame) continue;
                const auto* w = reinterpret_cast<const uint32_t*>(&frame->position);
                Log::Log("[heropanels] AgentCommander%d frame=%p created=%d visible=%d pos=[%u,%u,%u,%u,%u]", i, static_cast<void*>(frame),
                         frame->IsCreated(), frame->IsVisible(), w[0], w[1], w[2], w[3], w[4]);
            }
            Log::FlushFile();
            write_status("heropanels: 已记录");
            return;
        }
        if (verb == "chest") {
            int n = 1;
            is >> n;
            if (n < 0) n = 0;
            if (n > 1000) n = 1000;
            const bool can = GW::Items::CanAccessXunlaiChest();
            const auto inv = GW::UI::GetFrameByLabel(L"InvAccount");
            Log::Log("[harness] chest x%d: CanAccessXunlai=%d InvAccount=%p（只有当 CanAccess=0 时才会触发 OnShowXunlaiChest 崩溃分支）", n, static_cast<int>(can), static_cast<void*>(inv));
            for (int i = 0; i < n; ++i) GW::Chat::SendChat('/', L"chest");
            write_status("chest: 已发送");
            return;
        }
        if (verb == "chestonload") {
            int n = 50;
            is >> n;
            if (n < 1) n = 1;
            if (n > 5000) n = 5000;
            chest_on_load_remaining = n;
            chest_burst = -1;
            Log::Log("[chestonload] 已为 %d 次地图加载武装", n);
            write_status("chestonload: 已武装");
            return;
        }
        if (verb == "logeffects") { // logeffects <0|1>：记录传入的 PlayEffect {effect_id, coords} 以发现技能的 effect id
            int on = 1;
            is >> on;
            log_play_effects = on != 0;
            write_status(log_play_effects ? "logeffects: 开启（现在施放技能）" : "logeffects: 关闭");
            return;
        }
        if (verb == "playeffect") { // playeffect <effect_id> [x y plane]：本地模拟 PlayEffect（默认：玩家位置）
            uint32_t effect_id = 0, plane = 0;
            float x = 0, y = 0;
            if (!(is >> effect_id) || !effect_id) {
                write_status("playeffect: 参数错误（需要: playeffect <effect_id> [x y plane]）");
                return;
            }
            if (!(is >> x >> y)) {
                const auto self = GW::Agents::GetControlledCharacter();
                if (!self) { write_status("playeffect: 没有坐标且没有角色"); return; }
                x = self->pos.x;
                y = self->pos.y;
                plane = self->pos.zplane;
            }
            else {
                is >> plane;
            }
            GW::Packet::StoC::PlayEffect packet;
            packet.coords = {x, y};
            packet.plane = plane;
            packet.agent_id = 0;
            packet.effect_id = effect_id;
            packet.data5 = 0;
            packet.data6 = 0;
            GW::StoC::EmulatePacket(&packet);
            char b[96];
            snprintf(b, sizeof(b), "playeffect: id=%u 位置(%.0f,%.0f,z%u)", effect_id, x, y, plane);
            Log::Log("[harness] %s", b);
            write_status(b);
            return;
        }
        if (verb == "carto") { // carto <on|off|skip [forever]|point <game_x> <game_y>|pointwm <wm_x> <wm_y>|clearpoints|cleardeclines|status>
            std::string arg;
            is >> arg;
            if (arg == "on" || arg == "1") {
                CartographerModule::SetEnabled(true);
            }
            else if (arg == "off" || arg == "0") {
                CartographerModule::SetEnabled(false);
            }
            else if (arg == "skip") {
                std::string v;
                is >> v;
                CartographerModule::SkipCurrentTarget(v == "forever");
            }
            else if (arg == "point") {
                float x, y;
                GW::Vec2f wm;
                if ((is >> x >> y) && WorldMapWidget::GamePosToWorldMap(GW::GamePos(x, y, 0), wm)) {
                    CartographerModule::AddCustomPoint(wm);
                }
                else {
                    write_status("carto point: 参数错误或转换失败（需要游戏坐标 <x> <y>）");
                    return;
                }
            }
            else if (arg == "pointwm") {
                float x, y;
                if (is >> x >> y) {
                    CartographerModule::AddCustomPoint({x, y});
                }
                else {
                    write_status("carto pointwm: 参数错误（需要世界地图坐标 <x> <y>）");
                    return;
                }
            }
            else if (arg == "clearpoints") {
                CartographerModule::ClearCustomPoints();
            }
            else if (arg == "cleardeclines") {
                CartographerModule::ClearDeclined();
            }
            char buf[224];
            CartographerModule::GetStatus(buf, sizeof(buf));
            write_status(buf);
            Log::Log("[harness] %s", buf);
            Log::FlushFile();
            return;
        }
        if (verb == "hoverskill") { // hoverskill <skill_id>：强制显示技能范围环，如同悬停（0 清除）
            uint32_t skill_id = 0;
            is >> skill_id;
            SkillRangeRingsModule::SetDebugSkill(skill_id);
            char b[64];
            snprintf(b, sizeof(b), "hoverskill: %u", skill_id);
            Log::Log("[harness] %s", b);
            write_status(b);
            return;
        }
        if (verb == "hoverinfo") { // hoverinfo：报告实时工具提示载荷和解析出的悬停技能
            const auto tooltip = GW::UI::GetCurrentTooltip();
            const auto hovered = GW::SkillbarMgr::GetHoveredSkill();
            char b[192];
            if (tooltip) {
                snprintf(b, sizeof(b), "hoverinfo: 工具提示 payload_len=0x%x payload0=0x%x hovered_skill=%d campaign=%d",
                         tooltip->payload_len, tooltip->payload && tooltip->payload_len >= 4 ? tooltip->payload[0] : 0,
                         hovered ? static_cast<int>(hovered->skill_id) : -1,
                         hovered ? static_cast<int>(hovered->campaign) : -1);
            }
            else {
                snprintf(b, sizeof(b), "hoverinfo: 无工具提示");
            }
            Log::Log("[harness] %s", b);
            write_status(b);
            return;
        }
        if (verb == "skillinfo") { // skillinfo <skill_id>：记录技能范围环使用的常量数据字段
            uint32_t skill_id = 0;
            is >> skill_id;
            const auto skill = GW::SkillbarMgr::GetSkillConstantData(static_cast<GW::Constants::SkillID>(skill_id));
            char b[256];
            if (skill) {
                char rings[96];
                SkillRangeRingsModule::DebugSpecs(skill_id, rings, sizeof(rings));
                snprintf(b, sizeof(b), "skillinfo %u: type=%u target=%u special=0x%x aoe_range=%.1f const_effect=%.1f rings=[%s]",
                         skill_id, static_cast<uint32_t>(skill->type), skill->target, skill->special, skill->aoe_range, skill->const_effect, rings);
            }
            else {
                snprintf(b, sizeof(b), "skillinfo %u: 无数据", skill_id);
            }
            Log::Log("[harness] %s", b);
            write_status(b);
            return;
        }
        if (verb == "settingsearch") { // settingsearch <query>：记录匹配 <query> 的已注册设置，与设置窗口搜索扫描的字段相同
            std::string query;
            std::getline(is, query);
            query = TextUtils::ToLower(TextUtils::trim(query));
            if (query.empty()) {
                write_status("settingsearch: 参数错误（需要: settingsearch <query>）");
                return;
            }
            uint32_t hits = 0;
            for (const auto& e : SettingsRegistry::GetEntries()) {
                const bool hit = TextUtils::ToLower(e.label).find(query) != std::string::npos
                                 || TextUtils::ToLower(e.section).find(query) != std::string::npos
                                 || TextUtils::ToLower(e.description).find(query) != std::string::npos;
                if (!hit) continue;
                ++hits;
                Log::Log("[settingsearch] %s > %s (%s.%s)", e.module->SettingsName(), e.label.c_str(), e.section.c_str(), e.key.c_str());
            }
            char b[96];
            snprintf(b, sizeof(b), "settingsearch '%s': %u 个条目", query.c_str(), hits);
            Log::Log("[harness] %s", b);
            Log::FlushFile();
            write_status(b);
            return;
        }
        if (verb == "frcache") { // 记录接下来约 50 次调用的 FrCache 渲染缓冲区布局
            GameWorldCompositor::RequestBufferDump();
            write_status("frcache: 已排队转储");
            return;
        }
        if (verb == "mapinfo") {
            const auto self = GW::Agents::GetControlledCharacter();
            char b[160];
            snprintf(b, sizeof(b), "mapinfo: 地图=%d 实例=%d 位置=(%.0f,%.0f,z%u)",
                     static_cast<int>(GW::Map::GetMapID()), static_cast<int>(GW::Map::GetInstanceType()),
                     self ? self->pos.x : 0.f, self ? self->pos.y : 0.f, self ? self->pos.zplane : 0);
            Log::Log("[harness] %s", b);
            write_status(b);
            return;
        }
        if (verb == "dropitem") { // dropitem <bag 1-5> <slot 1-N>：在玩家位置丢弃一个物品（战利品信标测试）
            uint32_t bag_id = 0, slot = 0;
            if (is >> bag_id >> slot && slot > 0) {
                const auto bag = GW::Items::GetBag(static_cast<GW::Constants::Bag>(bag_id));
                const auto item = bag ? GW::Items::GetItemBySlot(bag, slot) : nullptr;
                const bool ok = item && GW::Items::DropItem(item, item->quantity);
                char b[96];
                snprintf(b, sizeof(b), "dropitem: bag=%u slot=%u item=%p queued=%d", bag_id, slot, static_cast<const void*>(item), static_cast<int>(ok));
                Log::Log("[harness] %s", b);
                write_status(b);
            }
            else {
                write_status("dropitem: 参数错误（需要: dropitem <bag 1-5> <slot 1-N>）");
            }
            return;
        }
        if (verb == "travel") {
            int mapid = 0;
            is >> mapid;
            if (mapid > 0) {
                // TravelWindow::Travel 处理原始 GW::Map::Travel 静默失败的情况（例如离开公会大厅）。
                const bool ok = TravelWindow::Instance().Travel(static_cast<GW::Constants::MapID>(mapid), GW::Constants::District::Current, 0);
                Log::Log("[harness] travel -> 地图 %d (queued=%d)", mapid, static_cast<int>(ok));
                char b[48];
                snprintf(b, sizeof(b), "travel: %d queued=%d", mapid, static_cast<int>(ok));
                write_status(b);
            }
            else {
                write_status("travel: 地图ID无效（需要: travel <mapid>）");
            }
            return;
        }
        if (verb == "dattex") {
            uint32_t id = 0;
            is >> std::hex >> id;
            if (!id) { write_status("dattex: id 无效（需要十六进制 file_id）"); return; }
            ArenaNetFileParser::GameAssetFile asset;
            const bool read_ok = asset.readFromDat(id, 0);
            char magic[8] = {0};
            const size_t sz = read_ok ? asset.data.size() : 0;
            if (sz >= 4) memcpy(magic, asset.data.data(), 4);
            Log::Log("[harness] dattex 0x%x: 读取=%d size=%u magic=[%s]", id, static_cast<int>(read_ok), static_cast<unsigned>(sz), magic);
            const std::wstring fn = L"dattex_" + std::to_wstring(id) + L".png";
            GwDatModule::SaveTextureFromFileIdToFile(id, Resources::GetPath(fn.c_str()));
            write_status("dattex: 已排队");
            return;
        }
        if (verb == "nativez") { // nativez [radius step]：在网格上比较原生地形 z 与 GW::Map::QueryAltitude（平面 0）
            float radius = 1200.f, step = 96.f;
            is >> radius >> step;
            radius = std::clamp(radius, 100.f, 5000.f);
            step = std::clamp(step, 24.f, 500.f);
            const auto self = GW::Agents::GetControlledCharacter();
            if (!self) { write_status("nativez: 没有角色"); return; }
            Log::Log("[nativez] 地图=%d 玩家=(%.0f,%.0f,z%u) 半径=%.0f 步长=%.0f",
                     static_cast<int>(GW::Map::GetMapID()), self->pos.x, self->pos.y, self->pos.zplane, radius, step);
            uint32_t samples = 0, both = 0, logged = 0;
            float max_delta = 0.f;
            double sum_delta = 0.0;
            for (float dx = -radius; dx <= radius; dx += step) {
                for (float dy = -radius; dy <= radius; dy += step) {
                    const float x = self->pos.x + dx, y = self->pos.y + dy;
                    const float native = TerrainDrape::NativeTerrainZ(x, y);
                    GW::GamePos p(x, y, 0);
                    const float game = GW::Map::QueryAltitude(&p);
                    ++samples;
                    if (native == 0.f || game == 0.f) continue; // 任一侧越界
                    ++both;
                    const float d = std::fabs(native - game);
                    max_delta = std::max(max_delta, d);
                    sum_delta += d;
                    if (d > 5.f && logged++ < 20)
                        Log::Log("[nativez]   (%.0f,%.0f) native=%.1f game=%.1f delta=%.1f", x, y, native, game, d);
                }
            }
            char b[160];
            snprintf(b, sizeof(b), "nativez: samples=%u both=%u max_delta=%.2f avg_delta=%.3f", samples, both, max_delta,
                     both ? sum_delta / both : 0.0);
            Log::Log("[harness] %s", b);
            Log::FlushFile();
            write_status(b);
            return;
        }
        if (verb == "propprobe") { // propprobe [radius]：枚举地图道具、其世界 z，以及哪些支持可行走的路径平面
            float radius = 2500.f;
            is >> radius;
            radius = std::clamp(radius, 200.f, 20000.f);
            const auto self = GW::Agents::GetControlledCharacter();
            const auto* mc = GW::GetMapContext();
            if (!self || !mc || !mc->props) { write_status("propprobe: 没有角色或道具"); return; }
            const auto& props = mc->props->propArray;
            const auto* pm = GW::Map::GetPathingMap();
            const auto n_planes = pm ? static_cast<uint32_t>(pm->size()) : 0u;

            // 支持可行走平面的道具索引：PathingMap+0x00（GWCA 中的 'zplane'）是非地面平面的道具索引（地面 = UINT_MAX）。
            std::set<uint32_t> walkable_props;
            for (uint32_t zp = 0; pm && zp < n_planes; ++zp) {
                const uint32_t pidx = (*pm)[zp].zplane;
                if (pidx != 0xFFFFFFFFu) walkable_props.insert(pidx);
            }

            Log::Log("[propprobe] 地图=%d props=%u 平面数=%u walkable_prop_planes=%u 玩家=(%.0f,%.0f,z%u,%.0f)",
                     static_cast<int>(GW::Map::GetMapID()), static_cast<unsigned>(props.size()), n_planes,
                     static_cast<unsigned>(walkable_props.size()), self->pos.x, self->pos.y, self->pos.zplane, self->z);

            uint32_t near_count = 0, elevated = 0, elevated_nonwalkable = 0, logged = 0;
            for (uint32_t i = 0; i < props.size(); ++i) {
                const GW::MapProp* p = props[i];
                if (!p) continue;
                const float dx = p->position.x - self->pos.x, dy = p->position.y - self->pos.y;
                if (dx * dx + dy * dy > radius * radius) continue;
                ++near_count;
                const float terrain = TerrainDrape::NativeTerrainZ(p->position.x, p->position.y);
                const float above = terrain != 0.f ? (terrain - p->position.z) : 0.f; // >0 表示道具原点位于地面之上（up=-z）
                const bool walkable = walkable_props.count(i) != 0;
                if (above > 50.f) { ++elevated; if (!walkable) ++elevated_nonwalkable; }
                if (logged++ < 25)
                    Log::Log("[propprobe]   prop[%u] file=0x%X pos=(%.0f,%.0f,z=%.0f) terrain_z=%.0f above=%.0f walkable=%d model=%p",
                             i, p->model_file_id, p->position.x, p->position.y, p->position.z, terrain, above,
                             static_cast<int>(walkable), static_cast<const void*>(p->interactive_model));
            }
            char b[224];
            snprintf(b, sizeof(b), "propprobe: props=%u near=%u elevated=%u elevated_nonwalkable=%u walkable_planes=%u",
                     static_cast<unsigned>(props.size()), near_count, elevated, elevated_nonwalkable,
                     static_cast<unsigned>(walkable_props.size()));
            Log::Log("[harness] %s", b);
            Log::FlushFile();
            write_status(b);
            return;
        }
        if (verb == "drapeverify") { // drapeverify [n radius]：新的 SurfaceZ 是否与旧的“所有平面最高点”查询匹配？
            uint32_t n = 1500;
            float radius = 1500.f;
            is >> n >> radius;
            n = std::clamp(n, 100u, 100000u);
            radius = std::clamp(radius, 100.f, 5000.f);
            const auto self = GW::Agents::GetControlledCharacter();
            const auto* pm = GW::Map::GetPathingMap();
            if (!self || !pm || !pm->size()) {
                write_status("drapeverify: 没有角色或路径地图");
                return;
            }
            const auto n_planes = static_cast<uint32_t>(pm->size());
            uint32_t seed = 0x1234567u;
            const auto next01 = [&seed] {
                seed = seed * 1664525u + 1013904223u;
                return (seed >> 8) * (1.f / 16777216.f);
            };
            Log::Log("[drapeverify] 地图=%d n=%u 平面数=%u 半径=%.0f", static_cast<int>(GW::Map::GetMapID()), n, n_planes, radius);
            uint32_t both = 0;             // 新旧方法都有数据的点
            uint32_t prune_mismatch = 0;   // 修剪选择与所有平面选择不同的点（真正的测试）
            uint32_t nonzero_hits = 0;     // 最高表面来自非零平面的点
            uint32_t logged = 0;
            float max_prune = 0.f, max_total = 0.f;
            double sum_total = 0.0;
            for (uint32_t i = 0; i < n; ++i) {
                const float ang = next01() * 6.2831853f, r = radius * std::sqrt(next01());
                const float x = self->pos.x + std::cos(ang) * r, y = self->pos.y + std::sin(ang) * r;
                float old_all = 0.f, new_z = 0.f, prune = 0.f;
                TerrainDrape::DrapeCompare(x, y, n_planes, &old_all, &new_z, &prune);

                // 修剪正确性：prune 使用与 old_all 相同的游戏值，仅在考虑的平面不同；任何差距 = 修剪错误地丢弃/添加了平面。
                const float dp = std::fabs(old_all - prune);
                if (dp > max_prune) max_prune = dp;
                if (dp > 0.5f) {
                    ++prune_mismatch;
                    if (logged++ < 15)
                        Log::Log("[drapeverify]   PRUNE 不匹配 (%.0f,%.0f) old_all=%.1f prune=%.1f new=%.1f d=%.1f",
                                 x, y, old_all, prune, new_z, dp);
                }
                // 此处最高表面是否来自非零平面？（即选择了桥/道具平面）
                if (old_all != 0.f) {
                    GW::GamePos g0{x, y, 0};
                    if (std::fabs(GW::Map::QueryAltitude(&g0) - old_all) > 0.5f) ++nonzero_hits;
                }
                // 新旧总数对比（包括本地点与平面0上半径-5的游戏值）
                if (old_all != 0.f && new_z != 0.f) {
                    ++both;
                    const float dt = std::fabs(old_all - new_z);
                    sum_total += dt;
                    if (dt > max_total) max_total = dt;
                    if (dt > 25.f && logged < 25) {
                        ++logged;
                        // 在微小十字上采样游戏地形以揭示悬崖（为什么半径-5 圆盘最大值 != 点值）
                        GW::GamePos c(x, y, 0), e(x + 8.f, y, 0), w(x - 8.f, y, 0), nq(x, y + 8.f, 0), s(x, y - 8.f, 0);
                        Log::Log("[drapeverify]   大差值 (%.0f,%.0f) old=%.1f new=%.1f d=%.1f | game@center=%.1f 邻点(+-8gw)=[%.0f %.0f %.0f %.0f]",
                                 x, y, old_all, new_z, dt, GW::Map::QueryAltitude(&c),
                                 GW::Map::QueryAltitude(&e), GW::Map::QueryAltitude(&w),
                                 GW::Map::QueryAltitude(&nq), GW::Map::QueryAltitude(&s));
                    }
                }
            }
            char b[240];
            snprintf(b, sizeof(b),
                     "drapeverify: n=%u both=%u nonzero_plane_hits=%u | PRUNE 不匹配数=%u max=%.2f | 总计 new-vs-old avg=%.2f max=%.2f",
                     n, both, nonzero_hits, prune_mismatch, max_prune, both ? sum_total / both : 0.0, max_total);
            Log::Log("[harness] %s", b);
            Log::FlushFile();
            write_status(b);
            return;
        }
        if (verb == "drapebench") { // drapebench [n radius]：A/B 对比旧的全平面 QueryAltitude 循环与新的修剪 SurfaceZ
            uint32_t n = 2000;
            float radius = 1200.f;
            is >> n >> radius;
            n = std::clamp(n, 100u, 200000u);
            radius = std::clamp(radius, 100.f, 5000.f);
            const auto self = GW::Agents::GetControlledCharacter();
            const auto* pm = GW::Map::GetPathingMap();
            if (!self || !pm || !pm->size()) {
                write_status("drapebench: 没有角色或路径地图");
                return;
            }
            const auto n_planes = static_cast<uint32_t>(pm->size());
            // 两种方法使用相同的伪随机圆盘采样点
            std::vector<std::pair<float, float>> pts(n);
            uint32_t seed = 0x9E3779B9u;
            const auto next01 = [&seed] {
                seed = seed * 1664525u + 1013904223u;
                return (seed >> 8) * (1.f / 16777216.f);
            };
            for (uint32_t i = 0; i < n; ++i) {
                const float ang = next01() * 6.2831853f, r = radius * std::sqrt(next01());
                pts[i] = {self->pos.x + std::cos(ang) * r, self->pos.y + std::sin(ang) * r};
            }

            // 旧路径：对每个平面调用 QueryAltitude，保留最高表面（最小 z）。这是 SurfaceZ 之前的行为。
            float sink_old = 0.f;
            const auto t_old0 = std::chrono::steady_clock::now();
            for (uint32_t i = 0; i < n; ++i) {
                float best = 0.f;
                for (uint32_t zp = 0; zp < n_planes; ++zp) {
                    GW::GamePos p(pts[i].first, pts[i].second, zp);
                    const float a = GW::Map::QueryAltitude(&p);
                    if (a != 0.f && (best == 0.f || a < best)) best = a;
                }
                sink_old += best;
            }
            const auto us_old = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - t_old0).count();

            // 新路径：本地平面0读取 + 梯形修剪的非零平面
            float sink_new = 0.f;
            const auto t_new0 = std::chrono::steady_clock::now();
            for (uint32_t i = 0; i < n; ++i)
                sink_new += TerrainDrape::SurfaceZ(pts[i].first, pts[i].second, self->pos.zplane, n_planes);
            const auto us_new = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - t_new0).count();

            char b[220];
            snprintf(b, sizeof(b),
                     "drapebench: n=%u 平面数=%u | 旧 %.2fms (%.2fus/q) | 新 %.2fms (%.2fus/q) | 加速 %.1fx | sink d=%.0f",
                     n, n_planes, us_old / 1000.0, static_cast<double>(us_old) / n, us_new / 1000.0,
                     static_cast<double>(us_new) / n, us_new ? static_cast<double>(us_old) / us_new : 0.0,
                     std::fabs(sink_old - sink_new));
            Log::Log("[harness] %s", b);
            Log::FlushFile();
            write_status(b);
            return;
        }
        if (verb == "propmesh") { // propmesh [prop_index]：转储一个道具的碰撞模型链；默认为玩家所在平面支持的道具
            uint32_t idx = 0xFFFFFFFFu;
            is >> idx;
            if (idx == 0xFFFFFFFFu) {
                const auto self = GW::Agents::GetControlledCharacter();
                const auto* pm = GW::Map::GetPathingMap();
                if (self && pm && self->pos.zplane < pm->size()) idx = (*pm)[self->pos.zplane].zplane;
            }
            if (idx == 0xFFFFFFFFu) {
                write_status("propmesh: 没有道具索引（需要: propmesh <idx>，或站在非地面平面上）");
                return;
            }
            PropSurface::DebugDumpProp(idx);
            Log::FlushFile();
            char b[96];
            snprintf(b, sizeof(b), "propmesh: 已转储道具 %u（查看 [propmesh] 日志行）", idx);
            write_status(b);
            return;
        }
        if (verb == "propverify") { // propverify [n radius qradius]：支持道具的 SurfaceZ 与游戏全平面 QueryAltitude 地面真值对比
            uint32_t n = 2000;
            float radius = 1500.f, qradius = 0.1f; // qradius ~0 = 点采样；默认的 5 圆盘最大值会在斜坡/边缘上放大游戏答案
            is >> n >> radius >> qradius;
            n = std::clamp(n, 100u, 100000u);
            radius = std::clamp(radius, 100.f, 5000.f);
            qradius = std::clamp(qradius, 0.01f, 10.f);
            const auto self = GW::Agents::GetControlledCharacter();
            const auto* pm = GW::Map::GetPathingMap();
            if (!self || !pm || !pm->size()) {
                write_status("propverify: 没有角色或路径地图");
                return;
            }
            if (!PropSurface::Ready()) {
                write_status("propverify: 道具索引尚未烘焙（查询会触发烘焙；请重试）");
                TerrainDrape::SurfaceZ(self->pos.x, self->pos.y, 0, 1);
                return;
            }
            const auto n_planes = static_cast<uint32_t>(pm->size());
            uint32_t seed = 0xBADC0DEu;
            const auto next01 = [&seed] {
                seed = seed * 1664525u + 1013904223u;
                return (seed >> 8) * (1.f / 16777216.f);
            };
            // 可行走基线无法看到不可行走道具：新的 HIGHER（更负）是特性；新的 LOWER = 预言机错过了可行走表面（变换错误）。
            uint32_t both = 0, match = 0, prop_higher = 0, missing = 0, logged = 0;
            float max_missing = 0.f, max_higher = 0.f;
            for (uint32_t i = 0; i < n; ++i) {
                const float ang = next01() * 6.2831853f, r = radius * std::sqrt(next01());
                const float x = self->pos.x + std::cos(ang) * r, y = self->pos.y + std::sin(ang) * r;
                float old_all = 0.f;
                for (uint32_t zp = 0; zp < n_planes; ++zp) {
                    GW::GamePos p{x, y, zp};
                    const float a = GW::Map::QueryAltitude(&p, qradius);
                    if (a != 0.f && (old_all == 0.f || a < old_all)) old_all = a;
                }
                const float new_z = TerrainDrape::SurfaceZ(x, y, 0, n_planes);
                if (old_all == 0.f || new_z == 0.f) continue;
                ++both;
                const float d = new_z - old_all; // <0: 新值更高（道具获胜）；>0: 新值更低（遗漏表面）
                if (d > 8.f) {
                    ++missing;
                    max_missing = std::max(max_missing, d);
                    if (logged++ < 15)
                        Log::Log("[propverify]   遗漏 (%.0f,%.0f) game=%.1f new=%.1f d=%.1f", x, y, old_all, new_z, d);
                }
                else if (d < -8.f) {
                    ++prop_higher;
                    max_higher = std::max(max_higher, -d);
                }
                else {
                    ++match;
                }
            }
            char b[224];
            snprintf(b, sizeof(b), "propverify: n=%u both=%u match=%u prop_higher=%u (max %.0f) 遗漏=%u (max %.0f)",
                     n, both, match, prop_higher, max_higher, missing, max_missing);
            Log::Log("[harness] %s", b);
            Log::FlushFile();
            write_status(b);
            return;
        }
        if (verb == "proptransect") { // proptransect [len step]：沿玩家周围 8 条射线对比仅可行走与支持道具的 SurfaceZ
            float len = 600.f, step = 25.f;
            is >> len >> step;
            len = std::clamp(len, 100.f, 5000.f);
            step = std::clamp(step, 5.f, 200.f);
            const auto self = GW::Agents::GetControlledCharacter();
            const auto* pm = GW::Map::GetPathingMap();
            if (!self || !pm || !pm->size()) {
                write_status("proptransect: 没有角色或路径地图");
                return;
            }
            const auto n_planes = static_cast<uint32_t>(pm->size());
            Log::Log("[proptransect] 地图=%d 玩家=(%.0f,%.0f,z%u,%.0f) len=%.0f step=%.0f ready=%d",
                     static_cast<int>(GW::Map::GetMapID()), self->pos.x, self->pos.y, self->pos.zplane, self->z,
                     len, step, static_cast<int>(PropSurface::Ready()));
            uint32_t wins = 0;
            for (int dir = 0; dir < 8; ++dir) {
                const float ang = dir * 0.7853981f;
                const float dx = std::cos(ang), dy = std::sin(ang);
                uint32_t dir_wins = 0;
                float peak = 0.f, peak_d = 0.f;
                for (float d = step; d <= len; d += step) {
                    const float x = self->pos.x + dx * d, y = self->pos.y + dy * d;
                    float old_all = 0.f, new_z = 0.f;
                    TerrainDrape::DrapeCompare(x, y, n_planes, &old_all, &new_z, nullptr);
                    if (old_all == 0.f || new_z == 0.f) continue;
                    const float lift = old_all - new_z; // >0: 道具表面高于可行走答案
                    if (lift > 5.f) {
                        ++dir_wins;
                        if (lift > peak) {
                            peak = lift;
                            peak_d = d;
                        }
                        if (dir_wins <= 4)
                            Log::Log("[proptransect]   dir=%d d=%.0f (%.0f,%.0f) walkable=%.1f prop=%.1f lift=%.1f",
                                     dir * 45, d, x, y, old_all, new_z, lift);
                    }
                }
                wins += dir_wins;
                if (dir_wins)
                    Log::Log("[proptransect] dir=%d: %u 个道具获胜采样，峰值 lift %.1f 在 d=%.0f", dir * 45, dir_wins, peak, peak_d);
            }
            char b[160];
            snprintf(b, sizeof(b), "proptransect: prop_wins=%u 跨 8 方向 (len=%.0f step=%.0f) ready=%d",
                     wins, len, step, static_cast<int>(PropSurface::Ready()));
            Log::Log("[harness] %s", b);
            Log::FlushFile();
            write_status(b);
            return;
        }
        if (verb == "propstats") { // propstats：发布的道具表面索引统计
            PropSurface::Stats st;
            if (!PropSurface::GetStats(&st)) {
                const auto self = GW::Agents::GetControlledCharacter();
                if (self) TerrainDrape::SurfaceZ(self->pos.x, self->pos.y, 0, 1); // 触发懒烘焙
                write_status("propstats: 尚未发布索引（已触发烘焙；请重试）");
                return;
            }
            char b[240];
            snprintf(b, sizeof(b), "propstats: ready=%d props=%u (render %u) skipped=%u meshes=%u tris=%u grid=%ux%u refs=%u snap=%.1fms bake=%.1fms",
                     static_cast<int>(PropSurface::Ready()), st.props, st.render_props, st.skipped_props, st.meshes,
                     st.triangles, st.cells_x, st.cells_y, st.refs, st.snapshot_ms, st.bake_ms);
            Log::Log("[harness] %s", b);
            Log::FlushFile();
            write_status(b);
            return;
        }
        if (verb == "propdrape") { // propdrape [0|1]：道具拖拽主开关（默认关闭 = 仅地形+平面）
            int on = -1;
            is >> on;
            if (on == 0 || on == 1) PropSurface::SetEnabled(on != 0);
            char b[96];
            snprintf(b, sizeof(b), "propdrape: %s%s", PropSurface::Enabled() ? "已启用" : "已禁用",
                     on == 0 || on == 1 ? " (已设置)" : " (状态)");
            Log::Log("[harness] %s", b);
            write_status(b);
            return;
        }
        if (verb == "fpsprobe") { // fpsprobe [seconds]：在一个窗口内计数帧数，报告平均 FPS
            float seconds = 3.f;
            is >> seconds;
            seconds = std::clamp(seconds, 0.5f, 30.f);
            fps_duration_ms = static_cast<long>(seconds * 1000.f);
            fps_frames = 0;
            fps_start = TIMER_INIT();
            fps_active = true;
            write_status("fpsprobe: 运行中");
            return;
        }
        write_status("unknown_command: " + verb);
    }
} // namespace
#endif

void TestHarness::Initialize()
{
    ToolboxModule::Initialize();
#ifdef HARNESS_ENABLED
    // 在此实例存在之前写入的命令已过时（例如重新加载脚本的 `shutdown` 在未加载工具箱时未被消耗）；
    // 执行它会杀死新实例。
    std::error_code ec;
    std::filesystem::remove(cmd_path(), ec);
    std::string existing;
    if (!Resources::ReadFile(config_path(), existing)) {
        Resources::WriteFile(config_path(),
            "# GWToolbox 测试工具配置（每次轮询读取）\n"
            "# waypoint=<x> <y> <plane>        （固定启动目的地）\n"
            "# autostart=1                     （每次地图加载触发一次路径点）\n");
    }
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::PlayEffect>(&PlayEffect_Entry, [](GW::HookStatus*, GW::Packet::StoC::PlayEffect* pak) {
        if (log_play_effects)
            Log::Log("[playeffect] id=%u coords=(%.0f,%.0f) plane=%u agent=%u d5=%u d6=%u",
                     pak->effect_id, pak->coords.x, pak->coords.y, pak->plane, pak->agent_id, pak->data5, pak->data6);
    });
    write_status("harness_initialized");
    Log::Log("[harness] 已初始化；命令文件：%s", cmd_path().string().c_str());
#endif
}

void TestHarness::Update(float)
{
#ifdef HARNESS_ENABLED
    if (terminating) return;
    if (fps_active) {
        ++fps_frames;
        const long elapsed = TIMER_DIFF(fps_start);
        if (elapsed >= fps_duration_ms) {
            fps_active = false;
            char b[96];
            snprintf(b, sizeof(b), "fpsprobe: frames=%d secs=%.2f avg_fps=%.1f", fps_frames, elapsed / 1000.f, fps_frames * 1000.f / elapsed);
            Log::Log("[harness] %s", b);
            Log::FlushFile();
            write_status(b);
        }
    }
    if (last_poll && TIMER_DIFF(last_poll) < kPollMs) return;
    last_poll = TIMER_INIT();

    Log::FlushFile(); // 刷新缓冲日志行，以便主机可以读取新的 [polyanya]/[visgraph] 查询

    const Config cfg = read_config();

    if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading) {
        fired_waypoint_this_load = false;                       // 为下一个地图重新武装自动路径点
        if (chest_on_load_remaining > 0) chest_burst = 0;       // 为加载后箱子切换爆发武装
    }

    // 每次轮询（~250ms）切换 Xunlai 面板，持续约 3 秒从地图就绪开始——即 /chest 入队的确切工作，
    // 减去当 GW 不是活动窗口时静默丢弃 SendChat 的聊天+前台门控。
    if (chest_on_load_remaining > 0 && chest_burst >= 0 && GW::Map::GetIsMapLoaded()) {
        constexpr int kChestBurstPolls = 13;
        if (chest_burst == 0) {
            Log::Log("[chestonload] 触发 map=%d remaining=%d", static_cast<int>(GW::Map::GetMapID()), chest_on_load_remaining - 1);
            Log::FlushFile();
        }
        if (const auto frame = GW::UI::GetFrameByLabel(L"InvAccount"))
            GW::UI::DestroyUIComponent(frame);
        else
            GW::Items::OpenXunlaiWindow();
        if (++chest_burst >= kChestBurstPolls) {
            chest_burst = -1;
            chest_on_load_remaining--;
        }
    }

    // 每次轮询首先消耗一条排队命令，以便 shutdown 在任何游戏状态（包括登录屏幕）下都能工作——
    // 我必须始终能够卸载 DLL 以重新链接。
    std::string body;
    if (Resources::ReadFile(cmd_path(), body)) {
        std::error_code ec;
        std::filesystem::remove(cmd_path(), ec); // 消耗掉，只运行一次
        std::istringstream is(body);
        std::string first;
        std::getline(is, first);
        first = trim(first);
        if (!first.empty()) run_command(first);
        return; // run_command 可能已发出关闭信号；不再执行其他操作
    }

    // 预游戏：账户登录和角色都已预填，因此只需按回车（限速）推进——
    // 回车提交账户登录，再回车选择预选角色——直到进入游戏。
    // （注意：GW 似乎在账户屏幕上忽略合成的 PostMessage 输入；真正的点击/GW 内部登录调用是解决方案。）
    if (GW::GetPreGameContext()) {
        static clock_t last_enter = 0;
        if (!last_enter || TIMER_DIFF(last_enter) > 1500) {
            last_enter = TIMER_INIT();
            press_enter();
            write_status(GW::LoginMgr::IsCharSelectReady() ? "login: 在角色选择界面按回车" : "login: 在账户屏幕按回车");
        }
        return;
    }

    // 地图就绪后自动触发路径（每次新注入一次）——优先使用活动任务标记，回退到配置路径点——
    // 这样重新注入时无需手动标记即可重新建立路径。仅在目标实际解析后锁定（在此之前重试）。
    if (cfg.autostart && !fired_waypoint_this_load && map_ready()) {
        GW::GamePos goal;
        const char* src = "";
        if (resolve_goal(cfg, goal, src)) {
            fired_waypoint_this_load = true;
            do_path_to(goal, src);
        }
    }
#endif
}

void TestHarness::Terminate()
{
    ToolboxModule::Terminate();
#ifdef HARNESS_ENABLED
    GW::StoC::RemoveCallback<GW::Packet::StoC::PlayEffect>(&PlayEffect_Entry);
    write_status("terminated");
#endif
}
