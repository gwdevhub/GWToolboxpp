#pragma once

#include <filesystem>
#include <string>
#include <vector>

// The filename the wasm build packages (see CMakeLists.txt's tools/package.py step); one definition so the installer and launch path don't drift out of sync.
inline constexpr wchar_t WASM_GWMOD_FILENAME[] = L"gwtoolbox.gwmod";
inline constexpr char WASM_GWMOD_FILENAME_A[] = "gwtoolbox.gwmod"; // for comparing against a WasmLoadedMod::label (narrow, see below)

// One gw_in_browser session file (%LOCALAPPDATA%\GuildWarsInBrowser\sessions\session-<port>.json): a running gw.py broker plus the injection token that gates talking to it.
struct WasmSession {
    std::string url; // e.g. "http://127.0.0.1:8123"
    std::string key; // injection token, url-safe already (secrets.token_urlsafe)
    uint32_t port = 0;
    uint32_t pid = 0; // gw.py's own pid, not the browser's
};

// One entry in status().mods - a module the page's GwInject.listLoaded() currently reports as loaded.
struct WasmLoadedMod {
    std::string key;   // content hash:length: identity, not a stable version id
    std::string label; // filename as injected, e.g. "gwtoolbox.gwmod"
};

struct WasmStatus {
    bool reachable = false; // false if gw.py itself didn't answer at all (closed, or a stale/wrong session file)
    bool page = false;      // true only if a browser page is actually listening, not merely that gw.py is up
    std::string state;      // "idle" | "queued" | "running" (an injection job's state, not the page's)
    std::vector<WasmLoadedMod> mods;
};

// Every gw.py running on this machine (one session file per port; a stale one just means GetWasmStatus comes back !reachable). Newest session file first.
std::vector<WasmSession> GetWasmSessions();

// GET /inject/status. Returns false (leaving `out` at defaults) only if gw.py never answered - distinct from it answering with no page listening yet (reachable && !page).
bool GetWasmStatus(const WasmSession& session, WasmStatus& out);

// True if `mods` has an entry whose label matches `mod_filename` exactly. Narrow/ASCII: a mod filename can't be anything else, see gw_in_browser's MOD_NAME_RE.
bool WasmModIsLoaded(const std::vector<WasmLoadedMod>& mods, std::string_view mod_filename);

// POSTs `gwmod_path` to `session`, then polls /inject/result until the page reports back or `wait_ms` elapses; `log_lines` carries whatever the loader logged either way, `error` is a short user-facing summary on failure.
bool InjectWasmMod(const WasmSession& session, const std::filesystem::path& gwmod_path, std::vector<std::string>& log_lines, std::wstring& error, uint32_t wait_ms = 60000);
