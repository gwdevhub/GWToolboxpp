#include "stdafx.h"

#include <cmath>
#include <filesystem>
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

#include "Modules/SkillRangeRingsModule.h"
#include "Utils/TerrainDrape.h"
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
                    GW::GamePos p(x, y, plane);
                    const float a = GW::Map::QueryAltitude(&p);
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
