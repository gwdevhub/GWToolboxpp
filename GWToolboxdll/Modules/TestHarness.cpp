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

// Dev iteration tool: compiled in Debug (_DEBUG) and RelWithDebInfo (GWTB_HARNESS, which
// logs to log.txt), excluded from the shipped Release build.
#if defined(_DEBUG) || defined(GWTB_HARNESS)
#define HARNESS_ENABLED 1
#endif

#ifdef HARNESS_ENABLED
namespace {
    constexpr long kPollMs = 250; // ~4 Hz; FindPath can stutter a frame, don't poll faster

    clock_t last_poll = 0;
    bool fired_waypoint_this_load = false;
    bool terminating = false; // set once shutdown is signalled; Update no-ops after
    int chest_on_load_remaining = 0; // regress the Xunlai auto-open crash: toggle the chest across the post-load window, N loads
    int chest_burst = -1;            // -1 idle; 0..N counts the post-load toggle burst polls

    // Effect-injection experiment (Ray-of-Judgment-at-a-point R&D): log incoming PlayEffect ids to discover a
    // skill's visual effect id, and locally emulate a PlayEffect to confirm it renders at chosen coords.
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

    // Press Enter on the GW window (advances the pre-filled account login and char-select).
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

    // Drive the REAL pathing target, exactly like the world-map right-click "Place Marker": set a custom
    // quest marker so QuestModule computes and draws the real quest path (via the visgraph) to it. This
    // replaces the old PathfindingWindow dev overlay (which drew its own from/to crosses + path).
    // The goal is current-map game coords; SetCustomQuestMarker takes world-map coords.
    void do_path_to(const GW::GamePos& goal, const char* src)
    {
        const auto self = GW::Agents::GetControlledCharacter();
        if (!self) { write_status("path_failed: no controlled character"); return; }
        GW::Vec2f world_pos;
        if (!WorldMapWidget::GamePosToWorldMap(goal, world_pos)) {
            write_status("path_failed: GamePosToWorldMap failed");
            return;
        }
        QuestModule::SetCustomQuestMarker(world_pos, true);
        char buf[180];
        snprintf(buf, sizeof(buf), "path_set(%s): from(%.0f,%.0f,z%u) to(%.0f,%.0f,z%u)",
                 src, self->pos.x, self->pos.y, self->pos.zplane, goal.x, goal.y, goal.zplane);
        write_status(buf);
        Log::Log("[harness] %s", buf);
    }

    // The active quest's marker (the goal that produces the live quest path). False if none/invalid.
    bool quest_marker(GW::GamePos& out)
    {
        const GW::Quest* q = GW::QuestMgr::GetActiveQuest();
        if (!q || std::isinf(q->marker.x) || std::isinf(q->marker.y)) return false;
        out = q->marker;
        return true;
    }

    // Resolve a reliable goal: prefer an explicitly-captured config waypoint, else the active quest
    // marker. The config waypoint is captured by `setgoal` (player's pos) and persists across reinjects.
    bool resolve_goal(const Config& cfg, GW::GamePos& out, const char*& src)
    {
        if (cfg.have_waypoint) { out = GW::GamePos(cfg.wx, cfg.wy, cfg.wplane); src = "config"; return true; }
        if (quest_marker(out)) { src = "quest"; return true; }
        return false;
    }

    // Capture the player's current position as the persistent goal (overwrites harness_config.txt).
    void set_goal_here()
    {
        const auto self = GW::Agents::GetControlledCharacter();
        if (!self) { write_status("setgoal: no controlled character"); return; }
        char cfg[256];
        snprintf(cfg, sizeof(cfg), "# captured goal (player position)\nwaypoint=%.1f %.1f %u\nautostart=1\n",
                 self->pos.x, self->pos.y, self->pos.zplane);
        Resources::WriteFile(config_path(), cfg);
        char buf[128];
        snprintf(buf, sizeof(buf), "goal_captured: (%.0f,%.0f,z%u)", self->pos.x, self->pos.y, self->pos.zplane);
        write_status(buf);
        Log::Log("[harness] %s", buf);
    }

    // Runs one channel command. shutdown() must be LAST and never touch state after.
    void run_command(const std::string& line)
    {
        std::istringstream is(line);
        std::string verb;
        is >> verb;
        if (verb == "shutdown") {
            write_status("shutdown_signalled");
            Log::Log("[harness] shutdown signalled; unloading DLL (GW stays open)");
            terminating = true;
            GWToolbox::SignalTerminate(true); // clean self-unload; cascade tears modules down
            return;                            // do NOT touch anything after this
        }
        if (verb == "login") {
            press_enter();
            write_status("login: Enter sent");
            return;
        }
        if (verb == "setgoal") { // capture player's position as the persistent goal
            set_goal_here();
            return;
        }
        if (verb == "repath") { // re-fire the path to the captured goal (or active quest marker)
            GW::GamePos goal;
            const char* src = "";
            if (resolve_goal(read_config(), goal, src)) do_path_to(goal, src);
            else write_status("repath: no config goal or active quest marker (use 'setgoal' first)");
            return;
        }
        if (verb == "dumpnav") { // dump navmesh polys near a point: dumpnav [x y [radius]] (default: config goal / player pos, r=1500)
            float x = 0, y = 0, radius = 1500.f;
            bool have_xy = (bool)(is >> x >> y);
            is >> radius;
            GW::GamePos center;
            if (have_xy) { center = GW::GamePos(x, y, 0); }
            else {
                const char* src = "";
                if (!resolve_goal(read_config(), center, src)) {
                    const auto self = GW::Agents::GetControlledCharacter();
                    if (!self) { write_status("dumpnav: no coords, no goal, no character"); return; }
                    center = self->pos;
                }
            }
            const bool ok = PathfindingWindow::DebugDumpNavMeshNear(center, radius);
            if (const auto self = GW::Agents::GetControlledCharacter())
                Log::Log("[navdump] player=(%.0f,%.0f,z%u) z=%.0f facing=%.3f camyaw=%.3f",
                         self->pos.x, self->pos.y, self->pos.zplane, self->z, self->rotation_angle, GW::CameraMgr::GetYaw());
            char buf[160];
            snprintf(buf, sizeof(buf), "dumpnav(%.0f,%.0f,r%.0f): %s", center.x, center.y, radius, ok ? "dumped" : "not ready (retry)");
            write_status(buf);
            Log::Log("[harness] %s", buf);
            return;
        }
        if (verb == "heightline") { // heightline x1 y1 x2 y2 plane [n] : QueryAltitude at n points along a segment on `plane`
            float x1, y1, x2, y2;
            uint32_t plane = 0, n = 16;
            if (is >> x1 >> y1 >> x2 >> y2 >> plane) {
                is >> n;
                if (n < 2) n = 2;
                if (n > 64) n = 64;
                Log::Log("[heightline] (%.0f,%.0f)->(%.0f,%.0f) plane=%u n=%u", x1, y1, x2, y2, plane, n);
                for (uint32_t i = 0; i < n; ++i) {
                    const float t = (float)i / (float)(n - 1);
                    const float x = x1 + (x2 - x1) * t, y = y1 + (y2 - y1) * t;
                    const float a = TerrainDrape::QueryAltAt(x, y, plane);
                    Log::Log("[heightline]   t=%.2f (%.0f,%.0f) plane%u_alt=%.1f", t, x, y, plane, a);
                }
                write_status("heightline: logged");
            }
            else {
                write_status("heightline: bad args (x1 y1 x2 y2 plane [n])");
            }
            return;
        }
        if (verb == "surfaceprobe") { // surfaceprobe [radius step]: compare preferred-plane terrain with all-plane surface choices near the player
            float radius = 1200.f, step = 120.f;
            is >> radius >> step;
            radius = std::clamp(radius, 100.f, 5000.f);
            step = std::clamp(step, 25.f, 500.f);
            const auto self = GW::Agents::GetControlledCharacter();
            const auto* pm = GW::Map::GetPathingMap();
            if (!self || !pm || !pm->size()) {
                write_status("surfaceprobe: no character or pathing map");
                return;
            }
            Log::Log("[surfaceprobe] map=%d player=(%.0f,%.0f,z%u) player_z=%.1f radius=%.0f step=%.0f planes=%u",
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
                        Log::Log("[surfaceprobe]   diff (%.0f,%.0f) preferred=%.1f surface=%.1f draped=%.1f delta=%.1f",
                                 x, y, preferred_z, surface_z, draped_z, surface_z - preferred_z);
                    }
                }
            }
            char b[128];
            snprintf(b, sizeof(b), "surfaceprobe: samples=%u diffs=%u max_delta=%.1f", samples, diffs, max_delta);
            Log::Log("[harness] %s", b);
            Log::FlushFile();
            write_status(b);
            return;
        }
        if (verb == "waypoint") {
            float x = 0, y = 0;
            uint32_t plane = 0;
            if (is >> x >> y) {
                is >> plane; // plane optional, defaults 0
                do_path_to(GW::GamePos(x, y, plane), "cmd");
            }
            else {
                write_status("waypoint: bad args (need: waypoint <x> <y> [plane])");
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
            write_status("heropanels: logged");
            return;
        }
        if (verb == "chest") {
            int n = 1;
            is >> n;
            if (n < 0) n = 0;
            if (n > 1000) n = 1000;
            const bool can = GW::Items::CanAccessXunlaiChest();
            const auto inv = GW::UI::GetFrameByLabel(L"InvAccount");
            Log::Log("[harness] chest x%d: CanAccessXunlai=%d InvAccount=%p (OnShowXunlaiChest crash branch runs only when CanAccess=0)", n, static_cast<int>(can), static_cast<void*>(inv));
            for (int i = 0; i < n; ++i) GW::Chat::SendChat('/', L"chest");
            write_status("chest: sent");
            return;
        }
        if (verb == "chestonload") {
            int n = 50;
            is >> n;
            if (n < 1) n = 1;
            if (n > 5000) n = 5000;
            chest_on_load_remaining = n;
            chest_burst = -1;
            Log::Log("[chestonload] armed for %d map loads", n);
            write_status("chestonload: armed");
            return;
        }
        if (verb == "logeffects") { // logeffects <0|1>: log incoming PlayEffect {effect_id, coords} to discover a skill's effect id
            int on = 1;
            is >> on;
            log_play_effects = on != 0;
            write_status(log_play_effects ? "logeffects: on (cast the skill now)" : "logeffects: off");
            return;
        }
        if (verb == "playeffect") { // playeffect <effect_id> [x y plane]: locally emulate a PlayEffect (default: player pos)
            uint32_t effect_id = 0, plane = 0;
            float x = 0, y = 0;
            if (!(is >> effect_id) || !effect_id) {
                write_status("playeffect: bad args (need: playeffect <effect_id> [x y plane])");
                return;
            }
            if (!(is >> x >> y)) {
                const auto self = GW::Agents::GetControlledCharacter();
                if (!self) { write_status("playeffect: no coords and no character"); return; }
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
            snprintf(b, sizeof(b), "playeffect: id=%u at (%.0f,%.0f,z%u)", effect_id, x, y, plane);
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
                    write_status("carto point: bad args or conversion failed (need game <x> <y>)");
                    return;
                }
            }
            else if (arg == "pointwm") {
                float x, y;
                if (is >> x >> y) {
                    CartographerModule::AddCustomPoint({x, y});
                }
                else {
                    write_status("carto pointwm: bad args (need world-map <x> <y>)");
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
        if (verb == "hoverskill") { // hoverskill <skill_id>: force skill-range rings as if hovering (0 clears)
            uint32_t skill_id = 0;
            is >> skill_id;
            SkillRangeRingsModule::SetDebugSkill(skill_id);
            char b[64];
            snprintf(b, sizeof(b), "hoverskill: %u", skill_id);
            Log::Log("[harness] %s", b);
            write_status(b);
            return;
        }
        if (verb == "hoverinfo") { // hoverinfo: report the live tooltip payload and resolved hovered skill
            const auto tooltip = GW::UI::GetCurrentTooltip();
            const auto hovered = GW::SkillbarMgr::GetHoveredSkill();
            char b[192];
            if (tooltip) {
                snprintf(b, sizeof(b), "hoverinfo: tooltip payload_len=0x%x payload0=0x%x hovered_skill=%d campaign=%d",
                         tooltip->payload_len, tooltip->payload && tooltip->payload_len >= 4 ? tooltip->payload[0] : 0,
                         hovered ? static_cast<int>(hovered->skill_id) : -1,
                         hovered ? static_cast<int>(hovered->campaign) : -1);
            }
            else {
                snprintf(b, sizeof(b), "hoverinfo: no tooltip");
            }
            Log::Log("[harness] %s", b);
            write_status(b);
            return;
        }
        if (verb == "skillinfo") { // skillinfo <skill_id>: log the constant skill data fields the range rings use
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
                snprintf(b, sizeof(b), "skillinfo %u: no data", skill_id);
            }
            Log::Log("[harness] %s", b);
            write_status(b);
            return;
        }
        if (verb == "settingsearch") { // settingsearch <query>: log registered settings matching <query>, same fields the settings-window search scans
            std::string query;
            std::getline(is, query);
            query = TextUtils::ToLower(TextUtils::trim(query));
            if (query.empty()) {
                write_status("settingsearch: bad args (need: settingsearch <query>)");
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
            snprintf(b, sizeof(b), "settingsearch '%s': %u entries", query.c_str(), hits);
            Log::Log("[harness] %s", b);
            Log::FlushFile();
            write_status(b);
            return;
        }
        if (verb == "frcache") { // log the FrCache render buffer layout for the next ~50 calls
            GameWorldCompositor::RequestBufferDump();
            write_status("frcache: dump queued");
            return;
        }
        if (verb == "mapinfo") {
            const auto self = GW::Agents::GetControlledCharacter();
            char b[160];
            snprintf(b, sizeof(b), "mapinfo: map=%d instance=%d pos=(%.0f,%.0f,z%u)",
                     static_cast<int>(GW::Map::GetMapID()), static_cast<int>(GW::Map::GetInstanceType()),
                     self ? self->pos.x : 0.f, self ? self->pos.y : 0.f, self ? self->pos.zplane : 0);
            Log::Log("[harness] %s", b);
            write_status(b);
            return;
        }
        if (verb == "dropitem") { // dropitem <bag 1-5> <slot 1-N>: drop an inventory item at the player (loot beacon testing)
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
                write_status("dropitem: bad args (need: dropitem <bag 1-5> <slot 1-N>)");
            }
            return;
        }
        if (verb == "travel") {
            int mapid = 0;
            is >> mapid;
            if (mapid > 0) {
                // TravelWindow::Travel handles the cases raw GW::Map::Travel silently drops (e.g. leaving a guild hall).
                const bool ok = TravelWindow::Instance().Travel(static_cast<GW::Constants::MapID>(mapid), GW::Constants::District::Current, 0);
                Log::Log("[harness] travel -> map %d (queued=%d)", mapid, static_cast<int>(ok));
                char b[48];
                snprintf(b, sizeof(b), "travel: %d queued=%d", mapid, static_cast<int>(ok));
                write_status(b);
            }
            else {
                write_status("travel: bad mapid (need: travel <mapid>)");
            }
            return;
        }
        if (verb == "dattex") {
            uint32_t id = 0;
            is >> std::hex >> id;
            if (!id) { write_status("dattex: bad id (need hex file_id)"); return; }
            ArenaNetFileParser::GameAssetFile asset;
            const bool read_ok = asset.readFromDat(id, 0);
            char magic[8] = {0};
            const size_t sz = read_ok ? asset.data.size() : 0;
            if (sz >= 4) memcpy(magic, asset.data.data(), 4);
            Log::Log("[harness] dattex 0x%x: read=%d size=%u magic=[%s]", id, static_cast<int>(read_ok), static_cast<unsigned>(sz), magic);
            const std::wstring fn = L"dattex_" + std::to_wstring(id) + L".png";
            GwDatModule::SaveTextureFromFileIdToFile(id, Resources::GetPath(fn.c_str()));
            write_status("dattex: queued");
            return;
        }
        if (verb == "nativez") { // nativez [radius step]: compare native terrain z vs GW::Map::QueryAltitude(plane 0) on a grid
            float radius = 1200.f, step = 96.f;
            is >> radius >> step;
            radius = std::clamp(radius, 100.f, 5000.f);
            step = std::clamp(step, 24.f, 500.f);
            const auto self = GW::Agents::GetControlledCharacter();
            if (!self) { write_status("nativez: no character"); return; }
            Log::Log("[nativez] map=%d player=(%.0f,%.0f,z%u) radius=%.0f step=%.0f",
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
                    if (native == 0.f || game == 0.f) continue; // OOB on either side
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
        if (verb == "propprobe") { // propprobe [radius]: enumerate map props, their world z, and which ones back a walkable pathing plane
            float radius = 2500.f;
            is >> radius;
            radius = std::clamp(radius, 200.f, 20000.f);
            const auto self = GW::Agents::GetControlledCharacter();
            const auto* mc = GW::GetMapContext();
            if (!self || !mc || !mc->props) { write_status("propprobe: no character or props"); return; }
            const auto& props = mc->props->propArray;
            const auto* pm = GW::Map::GetPathingMap();
            const auto n_planes = pm ? static_cast<uint32_t>(pm->size()) : 0u;

            // Which prop indices back a walkable pathing plane? PathingMap+0x00 (GWCA 'zplane') is the prop index
            // for non-ground planes (ground plane = UINT_MAX).
            std::set<uint32_t> walkable_props;
            for (uint32_t zp = 0; pm && zp < n_planes; ++zp) {
                const uint32_t pidx = (*pm)[zp].zplane;
                if (pidx != 0xFFFFFFFFu) walkable_props.insert(pidx);
            }

            Log::Log("[propprobe] map=%d props=%u planes=%u walkable_prop_planes=%u player=(%.0f,%.0f,z%u,%.0f)",
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
                const float above = terrain != 0.f ? (terrain - p->position.z) : 0.f; // >0 means prop origin sits above ground (up=-z)
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
        if (verb == "drapeverify") { // drapeverify [n radius]: does new SurfaceZ match the old all-planes-highest-point query?
            uint32_t n = 1500;
            float radius = 1500.f;
            is >> n >> radius;
            n = std::clamp(n, 100u, 100000u);
            radius = std::clamp(radius, 100.f, 5000.f);
            const auto self = GW::Agents::GetControlledCharacter();
            const auto* pm = GW::Map::GetPathingMap();
            if (!self || !pm || !pm->size()) {
                write_status("drapeverify: no character or pathing map");
                return;
            }
            const auto n_planes = static_cast<uint32_t>(pm->size());
            uint32_t seed = 0x1234567u;
            const auto next01 = [&seed] {
                seed = seed * 1664525u + 1013904223u;
                return (seed >> 8) * (1.f / 16777216.f);
            };
            Log::Log("[drapeverify] map=%d n=%u planes=%u radius=%.0f", static_cast<int>(GW::Map::GetMapID()), n, n_planes, radius);
            uint32_t both = 0;             // points with data on both old and new
            uint32_t prune_mismatch = 0;   // points where pruned selection != all-planes selection (the real test)
            uint32_t nonzero_hits = 0;     // points whose highest surface came from a non-zero plane
            uint32_t logged = 0;
            float max_prune = 0.f, max_total = 0.f;
            double sum_total = 0.0;
            for (uint32_t i = 0; i < n; ++i) {
                const float ang = next01() * 6.2831853f, r = radius * std::sqrt(next01());
                const float x = self->pos.x + std::cos(ang) * r, y = self->pos.y + std::sin(ang) * r;
                float old_all = 0.f, new_z = 0.f, prune = 0.f;
                TerrainDrape::DrapeCompare(x, y, n_planes, &old_all, &new_z, &prune);

                // Pruning correctness: prune uses the SAME game values as old_all, differing only in which
                // planes are considered. Any gap here is a plane the trapezoid pruning wrongly dropped/added.
                const float dp = std::fabs(old_all - prune);
                if (dp > max_prune) max_prune = dp;
                if (dp > 0.5f) {
                    ++prune_mismatch;
                    if (logged++ < 15)
                        Log::Log("[drapeverify]   PRUNE MISMATCH (%.0f,%.0f) old_all=%.1f prune=%.1f new=%.1f d=%.1f",
                                 x, y, old_all, prune, new_z, dp);
                }
                // Did the highest surface come from a non-zero plane here? (i.e. was a bridge/prop plane picked)
                if (old_all != 0.f) {
                    GW::GamePos g0{x, y, 0};
                    if (std::fabs(GW::Map::QueryAltitude(&g0) - old_all) > 0.5f) ++nonzero_hits;
                }
                // Total new-vs-old (includes native point vs game radius-5 on plane 0).
                if (old_all != 0.f && new_z != 0.f) {
                    ++both;
                    const float dt = std::fabs(old_all - new_z);
                    sum_total += dt;
                    if (dt > max_total) max_total = dt;
                    if (dt > 25.f && logged < 25) {
                        ++logged;
                        // Sample the game terrain in a tiny cross to reveal a cliff (why radius-5 disk-max != point).
                        GW::GamePos c(x, y, 0), e(x + 8.f, y, 0), w(x - 8.f, y, 0), nq(x, y + 8.f, 0), s(x, y - 8.f, 0);
                        Log::Log("[drapeverify]   BIG DELTA (%.0f,%.0f) old=%.1f new=%.1f d=%.1f | game@center=%.1f nbrs(+-8gw)=[%.0f %.0f %.0f %.0f]",
                                 x, y, old_all, new_z, dt, GW::Map::QueryAltitude(&c),
                                 GW::Map::QueryAltitude(&e), GW::Map::QueryAltitude(&w),
                                 GW::Map::QueryAltitude(&nq), GW::Map::QueryAltitude(&s));
                    }
                }
            }
            char b[240];
            snprintf(b, sizeof(b),
                     "drapeverify: n=%u both=%u nonzero_plane_hits=%u | PRUNE mismatches=%u max=%.2f | total new-vs-old avg=%.2f max=%.2f",
                     n, both, nonzero_hits, prune_mismatch, max_prune, both ? sum_total / both : 0.0, max_total);
            Log::Log("[harness] %s", b);
            Log::FlushFile();
            write_status(b);
            return;
        }
        if (verb == "drapebench") { // drapebench [n radius]: A/B the old all-planes QueryAltitude loop vs the new pruned SurfaceZ
            uint32_t n = 2000;
            float radius = 1200.f;
            is >> n >> radius;
            n = std::clamp(n, 100u, 200000u);
            radius = std::clamp(radius, 100.f, 5000.f);
            const auto self = GW::Agents::GetControlledCharacter();
            const auto* pm = GW::Map::GetPathingMap();
            if (!self || !pm || !pm->size()) {
                write_status("drapebench: no character or pathing map");
                return;
            }
            const auto n_planes = static_cast<uint32_t>(pm->size());
            // Same pseudo-random disk of sample points for both methods.
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

            // OLD path: QueryAltitude on every plane, keep highest surface (min z). This is what SurfaceZ did before.
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

            // NEW path: native plane-0 read + trapezoid-pruned non-zero planes.
            float sink_new = 0.f;
            const auto t_new0 = std::chrono::steady_clock::now();
            for (uint32_t i = 0; i < n; ++i)
                sink_new += TerrainDrape::SurfaceZ(pts[i].first, pts[i].second, self->pos.zplane, n_planes);
            const auto us_new = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - t_new0).count();

            char b[220];
            snprintf(b, sizeof(b),
                     "drapebench: n=%u planes=%u | OLD %.2fms (%.2fus/q) | NEW %.2fms (%.2fus/q) | speedup %.1fx | sink d=%.0f",
                     n, n_planes, us_old / 1000.0, static_cast<double>(us_old) / n, us_new / 1000.0,
                     static_cast<double>(us_new) / n, us_new ? static_cast<double>(us_old) / us_new : 0.0,
                     std::fabs(sink_old - sink_new));
            Log::Log("[harness] %s", b);
            Log::FlushFile();
            write_status(b);
            return;
        }
        if (verb == "propmesh") { // propmesh [prop_index]: dump one prop's collision-model chain; default = the prop backing the player's plane
            uint32_t idx = 0xFFFFFFFFu;
            is >> idx;
            if (idx == 0xFFFFFFFFu) {
                const auto self = GW::Agents::GetControlledCharacter();
                const auto* pm = GW::Map::GetPathingMap();
                if (self && pm && self->pos.zplane < pm->size()) idx = (*pm)[self->pos.zplane].zplane;
            }
            if (idx == 0xFFFFFFFFu) {
                write_status("propmesh: no prop index (need: propmesh <idx>, or stand on a non-ground plane)");
                return;
            }
            PropSurface::DebugDumpProp(idx);
            Log::FlushFile();
            char b[96];
            snprintf(b, sizeof(b), "propmesh: dumped prop %u (see [propmesh] log lines)", idx);
            write_status(b);
            return;
        }
        if (verb == "propverify") { // propverify [n radius qradius]: prop-aware SurfaceZ vs the game's all-planes QueryAltitude ground truth
            uint32_t n = 2000;
            float radius = 1500.f, qradius = 0.1f; // qradius ~0 = point sample; the default-5 disk-max inflates the game answer on slopes/edges
            is >> n >> radius >> qradius;
            n = std::clamp(n, 100u, 100000u);
            radius = std::clamp(radius, 100.f, 5000.f);
            qradius = std::clamp(qradius, 0.01f, 10.f);
            const auto self = GW::Agents::GetControlledCharacter();
            const auto* pm = GW::Map::GetPathingMap();
            if (!self || !pm || !pm->size()) {
                write_status("propverify: no character or pathing map");
                return;
            }
            if (!PropSurface::Ready()) {
                write_status("propverify: prop index not baked yet (queries trigger the bake; retry)");
                TerrainDrape::SurfaceZ(self->pos.x, self->pos.y, 0, 1);
                return;
            }
            const auto n_planes = static_cast<uint32_t>(pm->size());
            uint32_t seed = 0xBADC0DEu;
            const auto next01 = [&seed] {
                seed = seed * 1664525u + 1013904223u;
                return (seed >> 8) * (1.f / 16777216.f);
            };
            // The walkable baseline can't see non-walkable props, so new being HIGHER (more negative) is
            // the feature; new being LOWER means the oracle missed a walkable surface = transform bug.
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
                const float d = new_z - old_all; // <0: new is higher (prop win); >0: new is lower (MISSING surface)
                if (d > 8.f) {
                    ++missing;
                    max_missing = std::max(max_missing, d);
                    if (logged++ < 15)
                        Log::Log("[propverify]   MISSING (%.0f,%.0f) game=%.1f new=%.1f d=%.1f", x, y, old_all, new_z, d);
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
            snprintf(b, sizeof(b), "propverify: n=%u both=%u match=%u prop_higher=%u (max %.0f) MISSING=%u (max %.0f)",
                     n, both, match, prop_higher, max_higher, missing, max_missing);
            Log::Log("[harness] %s", b);
            Log::FlushFile();
            write_status(b);
            return;
        }
        if (verb == "proptransect") { // proptransect [len step]: walkable-only vs prop-aware SurfaceZ along 8 rays from the player
            float len = 600.f, step = 25.f;
            is >> len >> step;
            len = std::clamp(len, 100.f, 5000.f);
            step = std::clamp(step, 5.f, 200.f);
            const auto self = GW::Agents::GetControlledCharacter();
            const auto* pm = GW::Map::GetPathingMap();
            if (!self || !pm || !pm->size()) {
                write_status("proptransect: no character or pathing map");
                return;
            }
            const auto n_planes = static_cast<uint32_t>(pm->size());
            Log::Log("[proptransect] map=%d player=(%.0f,%.0f,z%u,%.0f) len=%.0f step=%.0f ready=%d",
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
                    const float lift = old_all - new_z; // >0: prop surface rides ABOVE the walkable answer
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
                    Log::Log("[proptransect] dir=%d: %u prop-win samples, peak lift %.1f at d=%.0f", dir * 45, dir_wins, peak, peak_d);
            }
            char b[160];
            snprintf(b, sizeof(b), "proptransect: prop_wins=%u across 8 dirs (len=%.0f step=%.0f) ready=%d",
                     wins, len, step, static_cast<int>(PropSurface::Ready()));
            Log::Log("[harness] %s", b);
            Log::FlushFile();
            write_status(b);
            return;
        }
        if (verb == "propstats") { // propstats: published prop-surface index stats
            PropSurface::Stats st;
            if (!PropSurface::GetStats(&st)) {
                const auto self = GW::Agents::GetControlledCharacter();
                if (self) TerrainDrape::SurfaceZ(self->pos.x, self->pos.y, 0, 1); // kick the lazy bake
                write_status("propstats: no index published yet (bake kicked; retry)");
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
        if (verb == "propdrape") { // propdrape [0|1]: master switch for prop draping (off by default = terrain+planes only)
            int on = -1;
            is >> on;
            if (on == 0 || on == 1) PropSurface::SetEnabled(on != 0);
            char b[96];
            snprintf(b, sizeof(b), "propdrape: %s%s", PropSurface::Enabled() ? "ENABLED" : "disabled",
                     on == 0 || on == 1 ? " (set)" : " (state)");
            Log::Log("[harness] %s", b);
            write_status(b);
            return;
        }
        if (verb == "fpsprobe") { // fpsprobe [seconds]: count frames over a window, report avg fps
            float seconds = 3.f;
            is >> seconds;
            seconds = std::clamp(seconds, 0.5f, 30.f);
            fps_duration_ms = static_cast<long>(seconds * 1000.f);
            fps_frames = 0;
            fps_start = TIMER_INIT();
            fps_active = true;
            write_status("fpsprobe: running");
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
    // A command written before this instance existed is stale (e.g. the reload script's `shutdown`
    // left unconsumed when no toolbox was loaded); executing it would kill the fresh instance.
    std::error_code ec;
    std::filesystem::remove(cmd_path(), ec);
    std::string existing;
    if (!Resources::ReadFile(config_path(), existing)) {
        Resources::WriteFile(config_path(),
            "# GWToolbox test harness config (read every poll)\n"
            "# waypoint=<x> <y> <plane>        (fixed startup destination)\n"
            "# autostart=1                     (fire the waypoint once each map load)\n");
    }
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::PlayEffect>(&PlayEffect_Entry, [](GW::HookStatus*, GW::Packet::StoC::PlayEffect* pak) {
        if (log_play_effects)
            Log::Log("[playeffect] id=%u coords=(%.0f,%.0f) plane=%u agent=%u d5=%u d6=%u",
                     pak->effect_id, pak->coords.x, pak->coords.y, pak->plane, pak->agent_id, pak->data5, pak->data6);
    });
    write_status("harness_initialized");
    Log::Log("[harness] initialized; command file: %s", cmd_path().string().c_str());
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

    Log::FlushFile(); // flush buffered log lines so the host can read fresh [polyanya]/[visgraph] queries

    const Config cfg = read_config();

    if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading) {
        fired_waypoint_this_load = false;                       // re-arm the auto-waypoint for the next map
        if (chest_on_load_remaining > 0) chest_burst = 0;       // arm the post-load chest-toggle burst
    }

    // Toggle the Xunlai panel every poll (~250ms) for ~3s from map-ready -- the exact work /chest enqueues,
    // minus the chat+foreground gates that silently drop SendChat when GW isn't the active window.
    if (chest_on_load_remaining > 0 && chest_burst >= 0 && GW::Map::GetIsMapLoaded()) {
        constexpr int kChestBurstPolls = 13;
        if (chest_burst == 0) {
            Log::Log("[chestonload] fire map=%d remaining=%d", static_cast<int>(GW::Map::GetMapID()), chest_on_load_remaining - 1);
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

    // Consume one queued command per poll FIRST, so shutdown works in ANY game state
    // (incl. the login screen) -- I must always be able to unload the DLL to relink.
    std::string body;
    if (Resources::ReadFile(cmd_path(), body)) {
        std::error_code ec;
        std::filesystem::remove(cmd_path(), ec); // consume so it runs once
        std::istringstream is(body);
        std::string first;
        std::getline(is, first);
        first = trim(first);
        if (!first.empty()) run_command(first);
        return; // run_command may have signalled shutdown; do nothing else
    }

    // Pre-game: account login + character are both pre-filled, so just press Enter
    // (rate-limited) to advance -- Enter submits the account login, Enter again plays the
    // pre-selected character -- until we're in-game. (NOTE: GW appears to ignore synthetic
    // PostMessage input on the account screen; a real click / GW-internal login call is the fix.)
    if (GW::GetPreGameContext()) {
        static clock_t last_enter = 0;
        if (!last_enter || TIMER_DIFF(last_enter) > 1500) {
            last_enter = TIMER_INIT();
            press_enter();
            write_status(GW::LoginMgr::IsCharSelectReady() ? "login: Enter @ char-select" : "login: Enter @ account screen");
        }
        return;
    }

    // Auto-fire the path once the map is ready (and once per fresh inject) -- prefer the active
    // quest marker, fall back to the configured waypoint -- so reinjecting re-establishes pathing
    // with no manual marker. Only latches once a goal actually resolves (retries until then).
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
