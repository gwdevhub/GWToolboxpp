#include "stdafx.h"

#include <cmath>
#include <filesystem>
#include <sstream>
#include <string>
#include <system_error>

#include <GWCA/GameContainers/GamePos.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Quest.h>
#include <GWCA/Context/CharContext.h>
#include <GWCA/Context/PreGameContext.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/MemoryMgr.h>
#include <GWCA/Managers/QuestMgr.h>

#include <Logger.h>
#include <Timer.h>

#include "GWToolbox.h"
#include "Modules/QuestModule.h"
#include "Modules/Resources.h"
#include "Modules/TestHarness.h"
#include "Utils/ToolboxUtils.h"
#include "Widgets/WorldMapWidget.h"

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
        write_status("unknown_command: " + verb);
    }
} // namespace
#endif

void TestHarness::Initialize()
{
    ToolboxModule::Initialize();
#ifdef HARNESS_ENABLED
    std::string existing;
    if (!Resources::ReadFile(config_path(), existing)) {
        Resources::WriteFile(config_path(),
            "# GWToolbox test harness config (read every poll)\n"
            "# waypoint=<x> <y> <plane>        (fixed startup destination)\n"
            "# autostart=1                     (fire the waypoint once each map load)\n");
    }
    write_status("harness_initialized");
    Log::Log("[harness] initialized; command file: %s", cmd_path().string().c_str());
#endif
}

void TestHarness::Update(float)
{
#ifdef HARNESS_ENABLED
    if (terminating) return;
    if (last_poll && TIMER_DIFF(last_poll) < kPollMs) return;
    last_poll = TIMER_INIT();

    Log::FlushFile(); // flush buffered log lines so the host can read fresh [polyanya]/[visgraph] queries

    const Config cfg = read_config();

    if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading)
        fired_waypoint_this_load = false; // re-arm the auto-waypoint for the next map

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
    write_status("terminated");
#endif
}
