#include "stdafx.h"

#include <filesystem>
#include <sstream>
#include <format>
#include <string>
#include <system_error>

#include <GWCA/Constants/Constants.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/MemoryMgr.h>

#include <Logger.h>
#include <Timer.h>

#include "GWToolbox.h"
#include "Modules/Resources.h"
#include "Utils/TextUtils.h"
#include "Utils/ToolboxUtils.h"
#include "Modules/TestHarness.h"
#include "Widgets/CartographerWidget.h"
#include "Windows/TravelWindow.h"

#ifdef _DEBUG
namespace {
    constexpr long kPollMs = 250;

    clock_t last_poll = 0;
    bool terminating = false;

    std::filesystem::path cmd_path() { return Resources::GetPath(L"harness_command.txt"); }
    std::filesystem::path status_path() { return Resources::GetPath(L"harness_status.txt"); }

    void write_status(const std::string& s) { Resources::WriteFile(status_path(), s); }

    std::string trim(const std::string& s)
    {
        const auto first = s.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) return {};
        return s.substr(first, s.find_last_not_of(" \t\r\n") - first + 1);
    }

    // Advances the pre-filled account login and character select.
    void press_enter()
    {
        if (const HWND hwnd = GW::MemoryMgr::GetGWWindowHandle()) {
            PostMessageW(hwnd, WM_KEYDOWN, VK_RETURN, 0);
            PostMessageW(hwnd, WM_KEYUP, VK_RETURN, 0);
        }
    }

    // shutdown() must be LAST and never touch state after.
    void run_command(const std::string& line)
    {
        std::istringstream is(line);
        std::string verb;
        is >> verb;
        if (verb == "status") {
            const auto map_id = static_cast<int>(GW::Map::GetMapID());
            const auto instance = static_cast<int>(GW::Map::GetInstanceType());
            char b[96];
            snprintf(b, sizeof(b), "status: map=%d instance=%d loaded=%d", map_id, instance, static_cast<int>(GW::Map::GetIsMapLoaded()));
            Log::Log("[harness] %s", b);
            Log::FlushFile();
            write_status(b);
            return;
        }
        if (verb == "login") {
            press_enter();
            write_status("login: Enter sent");
            return;
        }
        // PostMessage does not advance character select, so pick the character through the game's
        // own selector instead: "play" with no name takes the first one on the account.
        if (verb == "play") {
            std::string want;
            std::getline(is, want);
            want = trim(want);
            std::wstring name = TextUtils::StringToWString(want);
            if (name.empty()) {
                const auto chars = GW::AccountMgr::GetAvailableChars();
                if (chars && chars->size()) name = (*chars)[0].player_name;
            }
            const bool ok = !name.empty() && GW::LoginMgr::SelectCharacterToPlay(name.c_str());
            Log::Log("[harness] play '%S' ok=%d char_select_ready=%d", name.c_str(), static_cast<int>(ok),
                     static_cast<int>(GW::LoginMgr::IsCharSelectReady()));
            Log::FlushFile();
            write_status(std::format("play: ok={} name_len={}", static_cast<int>(ok), name.size()));
            return;
        }
        if (verb == "travel") {
            int mapid = 0;
            is >> mapid;
            if (mapid <= 0) {
                write_status("travel: bad mapid (need: travel <mapid>)");
                return;
            }
            // TravelWindow::Travel handles the cases raw GW::Map::Travel silently drops (e.g. leaving a guild hall).
            const bool ok = TravelWindow::Instance().Travel(static_cast<GW::Constants::MapID>(mapid), GW::Constants::District::Current, 0);
            Log::Log("[harness] travel -> map %d (queued=%d)", mapid, static_cast<int>(ok));
            Log::FlushFile();
            char b[64];
            snprintf(b, sizeof(b), "travel: %d queued=%d", mapid, static_cast<int>(ok));
            write_status(b);
            return;
        }
        if (verb == "cartoprobe") {
            int cx = INT_MIN, cy = INT_MIN;
            is >> cx >> cy;
            if (cx == INT_MIN || cy == INT_MIN) {
                write_status("cartoprobe: bad args (need: cartoprobe <cx> <cy>)");
                return;
            }
            Log::Log("[harness] cartoprobe (%d,%d) in map %d", cx, cy, static_cast<int>(GW::Map::GetMapID()));
            CartographerWidget::LogProbeAtCell(cx, cy);
            char b[64];
            snprintf(b, sizeof(b), "cartoprobe: %d %d map=%d", cx, cy, static_cast<int>(GW::Map::GetMapID()));
            write_status(b);
            return;
        }
        if (verb == "glitch") {
            int on = 0;
            is >> on;
            CartographerWidget::SetGateGlitchAllowed(on != 0);
            Log::Log("[harness] gate glitching = %d", on);
            Log::FlushFile();
            write_status(std::format("glitch: {}", on));
            return;
        }
        if (verb == "cartobake") {
            CartographerWidget::StartContinentBake();
            Log::Log("[harness] continent bake started");
            Log::FlushFile();
            write_status("cartobake: started");
            return;
        }
        if (verb == "bakestatus") {
            const bool running = CartographerWidget::ContinentBakeRunning();
            char b[48];
            snprintf(b, sizeof(b), "bakestatus: running=%d", static_cast<int>(running));
            write_status(b);
            return;
        }
        if (verb == "shutdown") {
            write_status("shutdown_signalled");
            Log::Log("[harness] shutdown signalled; unloading DLL (GW stays open)");
            Log::FlushFile();
            terminating = true;
            GWToolbox::SignalTerminate(true);
            return;
        }
        write_status("unknown_command: " + verb);
    }
}
#endif

void TestHarness::Initialize()
{
    ToolboxModule::Initialize();
#ifdef _DEBUG
    std::error_code ec;
    std::filesystem::remove(cmd_path(), ec);
    write_status("harness_initialized");
    Log::Log("[harness] initialized; command file: %s", cmd_path().string().c_str());
#endif
}

void TestHarness::Update(float)
{
#ifdef _DEBUG
    if (terminating) return;
    if (last_poll && TIMER_DIFF(last_poll) < kPollMs) return;
    last_poll = TIMER_INIT();

    Log::FlushFile();

    std::string body;
    if (!Resources::ReadFile(cmd_path(), body)) return;
    std::error_code ec;
    std::filesystem::remove(cmd_path(), ec);
    std::istringstream is(body);
    std::string first;
    std::getline(is, first);
    first = trim(first);
    if (!first.empty()) run_command(first);
#endif
}

void TestHarness::Terminate()
{
    ToolboxModule::Terminate();
#ifdef _DEBUG
    write_status("terminated");
#endif
}
