#include "stdafx.h"

#include <RestClient.h>

#include "WasmInject.h"

// glaze's reflection needs external linkage, so these JSON-shape structs live at file scope, not in the anonymous namespace below.

// Mirrors gw.py's write_session(): {"url":..., "key":..., "pid":..., "port":...}.
struct SessionFileJson {
    std::string url;
    std::string key;
    uint32_t pid = 0;
    uint32_t port = 0;
};

// Mirrors Injector.status() in gw.py.
struct InjectStatusJson {
    bool page = false;
    bool busy = false;
    std::string state;
    double since = 0;
    std::vector<WasmLoadedMod> mods;
};

// Mirrors the {"id": N} a successful POST /inject returns.
struct InjectSubmitJson {
    int id = 0;
};

// Mirrors /inject/result: {"pending": true} while waiting, otherwise {"id", "ok", "error", "lines"}.
struct InjectResultJson {
    bool pending = false;
    bool ok = false;
    std::optional<std::string> error;
    std::vector<std::string> lines;
};

namespace {
    std::wstring GwInBrowserSessionsDir()
    {
        wchar_t buf[MAX_PATH];
        const DWORD len = GetEnvironmentVariableW(L"LOCALAPPDATA", buf, _countof(buf));
        if (!len || len >= _countof(buf)) return {};
        return std::wstring(buf, len) + L"\\GuildWarsInBrowser\\sessions";
    }

    // Blocking GET, parsed as JSON. False on any transport/parse failure - GetWasmStatus treats both the same, as no usable answer.
    template <typename T>
    bool HttpGetJson(const std::string& url, T& out, int timeout_sec = 5)
    {
        RestClient client;
        client.SetUrl(url.c_str());
        client.SetTimeoutSec(timeout_sec);
        client.SetConnectTimeoutSec(timeout_sec);
        client.Execute();
        if (!client.IsSuccessful()) return false;
        constexpr glz::opts opts{.error_on_unknown_keys = false};
        return glz::read<opts>(out, client.GetContent()) == glz::error_code::none;
    }
}

std::vector<WasmSession> GetWasmSessions()
{
    std::vector<WasmSession> out;

    const std::wstring dir = GwInBrowserSessionsDir();
    if (dir.empty() || !std::filesystem::is_directory(dir)) return out;

    std::vector<std::pair<std::filesystem::file_time_type, WasmSession>> found;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (ec || !entry.is_regular_file()) continue;
        const auto& path = entry.path();
        const std::wstring name = path.filename().wstring();
        // session-<port>.json, exactly - matches gw.py's session_file() naming.
        if (!name.starts_with(L"session-") || !name.ends_with(L".json")) continue;

        std::ifstream file(path, std::ios::binary);
        if (!file) continue;
        std::string content((std::istreambuf_iterator(file)), std::istreambuf_iterator<char>());

        SessionFileJson parsed;
        constexpr glz::opts opts{.error_on_unknown_keys = false};
        if (glz::read<opts>(parsed, content) != glz::error_code::none) continue;
        if (parsed.url.empty() || parsed.key.empty()) continue;

        std::error_code mtime_ec;
        const auto mtime = std::filesystem::last_write_time(path, mtime_ec);
        found.emplace_back(mtime, WasmSession{std::move(parsed.url), std::move(parsed.key), parsed.port, parsed.pid});
    }

    // Newest first, matching gw.py's list_all_sessions().
    std::ranges::sort(found, [](const auto& a, const auto& b) { return a.first > b.first; });
    out.reserve(found.size());
    for (auto& [mtime, session] : found) out.push_back(std::move(session));
    return out;
}

bool GetWasmStatus(const WasmSession& session, WasmStatus& out)
{
    out = {};
    InjectStatusJson parsed;
    if (!HttpGetJson(session.url + "/inject/status?k=" + session.key, parsed)) return false;
    out.reachable = true;
    out.page = parsed.page;
    out.state = std::move(parsed.state);
    out.mods = std::move(parsed.mods);
    return true;
}

bool WasmModIsLoaded(const std::vector<WasmLoadedMod>& mods, const std::string_view mod_filename)
{
    return std::ranges::any_of(mods, [&](const WasmLoadedMod& m) { return m.label == mod_filename; });
}

bool InjectWasmMod(const WasmSession& session, const std::filesystem::path& gwmod_path, std::vector<std::string>& log_lines, std::wstring& error, const uint32_t wait_ms)
{
    log_lines.clear();
    error.clear();

    std::ifstream file(gwmod_path, std::ios::binary);
    if (!file) {
        error = L"Couldn't open " + gwmod_path.wstring();
        return false;
    }
    std::string content((std::istreambuf_iterator(file)), std::istreambuf_iterator<char>());
    if (content.empty()) {
        error = L"Bundle at " + gwmod_path.wstring() + L" is empty";
        return false;
    }

    const std::string name = gwmod_path.filename().string();

    RestClient submit;
    submit.SetUrl((session.url + "/inject?k=" + session.key + "&name=" + name).c_str());
    submit.SetMethod(HttpMethod::Post);
    submit.SetHeader("Content-Type", "application/octet-stream");
    submit.SetPostContent(content.data(), content.size(), ContentFlag::ByRef);
    submit.SetTimeoutSec(40);
    submit.SetConnectTimeoutSec(5);
    submit.Execute();

    if (!submit.IsSuccessful()) {
        error = std::format(L"gw_in_browser refused the injection (HTTP {})", submit.GetStatusCode());
        return false;
    }

    InjectSubmitJson submitted;
    constexpr glz::opts opts{.error_on_unknown_keys = false};
    if (glz::read<opts>(submitted, submit.GetContent()) != glz::error_code::none) {
        error = L"gw_in_browser's response to the submission didn't parse";
        return false;
    }

    // Poll /inject/result, matching tools/inject.py's own cadence.
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(wait_ms);
    for (;;) {
        RestClient poll;
        poll.SetUrl(std::format("{}/inject/result?k={}&id={}", session.url, session.key, submitted.id).c_str());
        poll.SetTimeoutSec(10);
        poll.Execute();

        if (poll.IsSuccessful()) {
            InjectResultJson result;
            if (glz::read<opts>(result, poll.GetContent()) == glz::error_code::none) {
                if (!result.pending) {
                    log_lines = std::move(result.lines);
                    if (result.error) {
                        error = std::wstring(result.error->begin(), result.error->end());
                        return false;
                    }
                    if (!result.ok) {
                        error = L"gw_in_browser's inject() returned false";
                        return false;
                    }
                    return true;
                }
            }
        }

        if (std::chrono::steady_clock::now() >= deadline) {
            error = L"Timed out waiting for the game window to load the mod. Is it open and past the loading screen?";
            return false;
        }
        Sleep(250);
    }
}
