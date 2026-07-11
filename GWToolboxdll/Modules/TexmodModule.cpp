#include "TexmodModule.h"
#include "stdafx.h"

#include <ToolboxIni.h>
#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <d3d9.h>
#include <filesystem>
#include <format>
#include <imgui.h>
#include <map>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <vector>

#include <GWCA/Managers/RenderMgr.h>
#include <GWCA/Utilities/Hooker.h>

#include <ImGuiAddons.h>
#include <Defender.h>
#include <Modules/Resources.h>
#include <Utils/FontLoader.h>
#include <Utils/TextUtils.h>

// gMod GitHub release shape; at namespace scope as glaze reflection can't bind anonymous-namespace types.
namespace gmod_github {
    struct ReleaseAsset {
        std::string name;
        std::string browser_download_url;
    };
    struct Release {
        std::string tag_name;
        std::vector<ReleaseAsset> assets;
    };
}

namespace {

    // =========================================================================
    // gMod exports (resolved from gMod.dll)
    // =========================================================================

    using tGModAddFile = int (*)(const wchar_t*);
    using tGModRemoveFile = int (*)(const wchar_t*);
    using tGModSetDevice = int (*)(IDirect3DDevice9*);
    using tGModGetFiles = int (*)(wchar_t* buf, int bufLen);

    tGModAddFile pfnAddFile = nullptr;
    tGModRemoveFile pfnRemoveFile = nullptr;
    tGModGetFiles pfnGetFiles = nullptr;
    tGModSetDevice pfnSetDevice = nullptr;

    // gMod return codes (see gMod's Error.h); AddFile returns EXISTS when a pack is already loaded, which we treat as success.
    constexpr int GMOD_RETURN_OK = 0;
    constexpr int GMOD_RETURN_EXISTS = -70;

    // gMod version check / download (mirrors Updater): fetch the latest release and download it into the GWToolbox folder if newer.
    constexpr glz::opts gmod_json_opts{.error_on_unknown_keys = false};

    enum class GmodUpdateStep { Idle, Checking, Downloading };
    GmodUpdateStep gmodUpdateStep = GmodUpdateStep::Idle;

    std::string gmodLatestVersion;        // latest release tag, 'v' stripped (e.g. "1.8.0.2")
    std::string gmodLocalVersion;         // file version of the managed gMod.dll
    bool gmodLocalVersionChecked = false; // recompute gmodLocalVersion when false
    std::string gmodUpdateStatus;         // one-line status for the version row

    struct TexturePackEntry {
        std::filesystem::path path;
        std::string label;
        bool loaded = false;
        bool external = false;
    };

    // =========================================================================
    // Module state
    // =========================================================================

    HMODULE gmodDll = nullptr;

    std::vector<TexturePackEntry> packs;

    bool gmodReady = false;
    std::string statusMessage;
    bool gmodLoadAttempted = false;
    // Set when gMod is no longer needed (last pack removed); the unload runs from Update() not the draw pass.
    bool gmodUnloadRequested = false;
    // True only when toolbox loaded gMod itself; false for one injected by a launcher.
    bool gmodLoadedByToolbox = false;

    // A launcher pre-loaded a gMod lacking our exports; we won't load our own on top (two swappers corrupt our originals). Cleared only by a restart.
    bool gmodIncompatible = false;
    std::filesystem::path gmodIncompatiblePath;

    // Cached pre-loaded-gMod probe (a launcher injects before toolbox starts, so the state is fixed; probe once to avoid per-frame work).
    bool preloadProbed = false;
    HMODULE preloadGmod = nullptr;      // a gMod injected by something other than us, if any
    bool preloadGmodCompatible = false; // ...and it exposes the API we can drive

    constexpr const char* INI_SECTION = "TexmodModule";
    constexpr const char* INI_PACK_COUNT = "PackCount";
    constexpr const char* INI_PACK_PATH = "Pack";
    constexpr const char* INI_PACK_ENABLED = "PackEnabled";

    // Encode a path as UTF-8 (path::string() would narrow to ANSI and lose non-ASCII); pair with StringToWString to decode.
    std::string PathToUtf8(const std::filesystem::path& p)
    {
        return TextUtils::WStringToString(p.wstring());
    }

    // =========================================================================
    // gMod TextureClient helpers
    // =========================================================================

    bool ResolveTextureClientFunctions()
    {
        pfnAddFile = reinterpret_cast<tGModAddFile>(GetProcAddress(gmodDll, "AddFile"));
        pfnRemoveFile = reinterpret_cast<tGModRemoveFile>(GetProcAddress(gmodDll, "RemoveFile"));
        pfnGetFiles = reinterpret_cast<tGModGetFiles>(GetProcAddress(gmodDll, "GetFiles"));
        pfnSetDevice = reinterpret_cast<tGModSetDevice>(GetProcAddress(gmodDll, "SetDevice"));
        // GetFiles is required: we reconcile against the load order it reports, so a build lacking it can't be driven.
        if (!pfnAddFile || !pfnRemoveFile || !pfnGetFiles || !pfnSetDevice) {
            statusMessage = "Error: gMod.dll is missing AddFile/RemoveFile/GetFiles/SetDevice exports. "
                            "Please use a gMod build that includes these functions.";
            return false;
        }
        return true;
    }

    // Read gMod's loaded files in load order (== priority), what we diff against to reconcile; empty if GetFiles absent/errors.
    std::vector<std::filesystem::path> QueryGmodFiles()
    {
        std::vector<std::filesystem::path> result;
        if (!pfnGetFiles) return result;
        const int count = pfnGetFiles(nullptr, 0);
        if (count <= 0) return result;
        const int bufLen = count * MAX_PATH;
        std::vector<wchar_t> buf(bufLen, L'\0');
        const int written = pfnGetFiles(buf.data(), bufLen);
        const wchar_t* ptr = buf.data();
        const wchar_t* const end = buf.data() + written;
        while (ptr < end && *ptr) {
            result.emplace_back(ptr);
            ptr += wcslen(ptr) + 1;
        }
        return result;
    }

    // Reconcile our list with gMod's loaded set (GetFiles), adopting externally-loaded packs; clear_missing=false on startup so the restored state isn't wiped by gMod's still-empty set.
    void SyncExternalPacks(bool clear_missing = true)
    {
        if (!pfnGetFiles) return;

        const int count = pfnGetFiles(nullptr, 0);
        if (count <= 0) return;

        const int bufLen = count * MAX_PATH;
        std::vector<wchar_t> buf(bufLen, L'\0');
        const int written = pfnGetFiles(buf.data(), bufLen);

        std::vector<std::filesystem::path> packs_loaded;
        const wchar_t* ptr = buf.data();
        for (int i = 0; i < written; i++) {
            if (!*ptr) break;
            std::filesystem::path p(ptr);
            packs_loaded.push_back(p);
            ptr += wcslen(ptr) + 1;

            const auto it = std::ranges::find_if(packs, [&](const TexturePackEntry& e) {
                return e.path == p;
            });
            if (it != packs.end()) {
                it->loaded = true;
                continue;
            }
            packs.push_back({p, PathToUtf8(p.stem()), /*loaded=*/true, /*external=*/true});
        }
        if (clear_missing) {
            for (auto& pack : packs) {
                if (std::ranges::find(packs_loaded, pack.path) == packs_loaded.end()) pack.loaded = false;
            }
        }
    }

    // Push the saved enabled state to gMod once ready; forward-declared for InitGMod, defined after ApplyLoadOrder.
    void RestoreLoadedPacks();

    // Check GitHub and download a newer gMod if needed; forward-declared so adding a pack can trigger it.
    void CheckAndUpdateGmod();

    // Where the toolbox keeps its managed gMod.dll (auto-downloaded).
    std::filesystem::path ManagedGmodDllPath()
    {
        return Resources::GetPath("gmod") / "gMod.dll";
    }

    // True if the module exports the gMod texture API we drive.
    bool ModuleHasGmodExports(HMODULE h)
    {
        return h && GetProcAddress(h, "AddFile") && GetProcAddress(h, "RemoveFile") && GetProcAddress(h, "GetFiles") && GetProcAddress(h, "SetDevice");
    }

    // Read a named string from a file's version resource (e.g. "ProductName"); empty if absent.
    std::string GetDllVersionString(const std::filesystem::path& path, const wchar_t* name)
    {
        if (path.empty()) return {};
        const auto wp = path.wstring();
        DWORD handle = 0;
        const DWORD size = GetFileVersionInfoSizeW(wp.c_str(), &handle);
        if (!size) return {};
        std::vector<BYTE> data(size);
        if (!GetFileVersionInfoW(wp.c_str(), handle, size, data.data())) return {};
        struct LangCodepage {
            WORD language, codepage;
        }* tr = nullptr;
        UINT trLen = 0;
        if (!VerQueryValueW(data.data(), L"\\VarFileInfo\\Translation", reinterpret_cast<LPVOID*>(&tr), &trLen) || !tr || trLen < sizeof(LangCodepage)) return {};
        wchar_t query[64];
        swprintf_s(query, L"\\StringFileInfo\\%04x%04x\\%s", tr->language, tr->codepage, name);
        wchar_t* val = nullptr;
        UINT valLen = 0;
        if (!VerQueryValueW(data.data(), query, reinterpret_cast<LPVOID*>(&val), &valLen) || !val) return {};
        return TextUtils::WStringToString(val);
    }

    // Does this module look like gMod (any version)? Named gMod.dll, or its version resource says so - catches an older gMod (or renamed d3d9 proxy) that lacks our exports.
    bool ModuleIsGmod(HMODULE h)
    {
        if (!h) return false;
        wchar_t buf[MAX_PATH] = {};
        if (!GetModuleFileNameW(h, buf, MAX_PATH)) return false;
        const std::filesystem::path p = buf;
        if (_wcsicmp(p.filename().c_str(), L"gMod.dll") == 0) return true;
        for (const wchar_t* key : {L"ProductName", L"InternalName", L"OriginalFilename"}) {
            std::string s = GetDllVersionString(p, key);
            std::ranges::transform(s, s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (s.contains("gmod")) return true;
        }
        return false;
    }

    // Probe once for a launcher-injected gMod: prefer one we can drive, else note an incompatible one; skips our own copy and the system d3d9.dll.
    void ProbePreloadedGmodOnce()
    {
        if (preloadProbed) return;
        if (!GW::Render::GetDevice()) return; // wait until the process is up so an injected gMod is present
        preloadProbed = true;
        for (const wchar_t* name : {L"gMod.dll", L"d3d9.dll"}) {
            HMODULE h = GetModuleHandleW(name);
            if (!h || h == gmodDll) continue;
            if (ModuleHasGmodExports(h)) {
                preloadGmod = h;
                preloadGmodCompatible = true;
                return;
            }
            if (!preloadGmod && ModuleIsGmod(h)) preloadGmod = h; // gMod we can't drive
        }
    }

    // Bring gMod up: adopt one already in the process if present, else load our copy.
    bool InitGMod()
    {
        if (gmodReady) return true;
        if (gmodIncompatible) return false; // a restart is the only way out

        const auto device = GW::Render::GetDevice();
        if (!device) {
            statusMessage = "Error: Could not get IDirect3DDevice9 from GW::Render::GetDevice().";
            return false;
        }

        ProbePreloadedGmodOnce();

        // 1a. A pre-loaded gMod we can drive: adopt it; we never own it.
        if (preloadGmod && preloadGmodCompatible) {
            gmodDll = preloadGmod;
            ResolveTextureClientFunctions(); // exports confirmed by the probe
            gmodLoadedByToolbox = false;
            pfnSetDevice(device);
            gmodReady = true;
            statusMessage = "gMod was already active (pre-loaded). Texture pack loading enabled.";
            RestoreLoadedPacks();
            return true;
        }

        // 1b. A pre-loaded gMod we can't drive: loading ours on top would double-swap and corrupt our originals, so refuse and ask the user to update/remove it (needs a restart).
        if (preloadGmod) {
            gmodIncompatible = true;
            wchar_t buf[MAX_PATH] = {};
            if (GetModuleFileNameW(preloadGmod, buf, MAX_PATH)) gmodIncompatiblePath = buf;
            statusMessage = "An older, incompatible gMod is already loaded by your launcher. Update it (or remove it so GWToolbox can manage gMod), then restart Guild Wars.";
            return false;
        }

        // 2. Otherwise load our managed copy; with no packs, leave gMod unloaded.
        if (packs.empty()) return false;
        if (gmodLoadAttempted) return false;
        gmodLoadAttempted = true;

        const auto path = ManagedGmodDllPath();
        std::error_code ec;
        if (!std::filesystem::exists(path, ec)) {
            statusMessage = "gMod.dll not found yet - it is downloaded automatically. Check your internet connection.";
            return false;
        }

        gmodDll = LoadLibraryW(path.wstring().c_str());
        if (!gmodDll) {
            const DWORD err = GetLastError();
            statusMessage = "Error: Could not load gMod.dll.";
            std::wstring detail;
            if ((err == ERROR_VIRUS_INFECTED || err == ERROR_VIRUS_DELETED) && FindRecentDefenderBlock(L"gMod.dll", 15, detail))
                statusMessage += " Windows Defender blocked it: " + TextUtils::WStringToString(detail);
            return false;
        }
        if (!ResolveTextureClientFunctions()) {
            FreeLibrary(gmodDll);
            gmodDll = nullptr;
            return false;
        }
        gmodLoadedByToolbox = true; // we loaded it, so we own its lifetime
        pfnSetDevice(device);
        gmodReady = true;
        RestoreLoadedPacks();
        return true;
    }
    TexturePackEntry* FindPack(const std::filesystem::path& path, bool create_if_not_found = false);

    // AddFile is slow (decode + DDS-compress), so reconciles run on a worker; apply_mutex serialises them and apply_generation drops superseded requests so rapid toggles can't apply out of order.
    std::mutex apply_mutex;
    std::atomic<uint64_t> apply_generation = 0;

    // Build the desired load order: every pack flagged loaded, in list order.
    std::vector<std::filesystem::path> BuildDesiredList()
    {
        std::vector<std::filesystem::path> desired;
        for (const auto& pack : packs) {
            if (pack.loaded) desired.push_back(pack.path);
        }
        return desired;
    }

    // Reconcile gMod's loaded set to `desired`: keep the longest matching prefix (first-loaded wins conflicts), remove the tail, re-add desired in order; returns last gMod error. Caller holds apply_mutex.
    int ReconcileLocked(const std::vector<std::filesystem::path>& desired)
    {
        const auto current = QueryGmodFiles();
        size_t prefix = 0;
        while (prefix < current.size() && prefix < desired.size() && current[prefix] == desired[prefix]) {
            ++prefix;
        }
        int error = GMOD_RETURN_OK;
        // Remove the diverging tail, deepest first; a failure (e.g. an old gMod that can't match the path) is reported, not swallowed, so the pack isn't silently left loaded.
        for (size_t i = current.size(); i-- > prefix;) {
            const int ret = pfnRemoveFile ? pfnRemoveFile(current[i].wstring().c_str()) : GMOD_RETURN_OK;
            if (ret < 0 && ret != GMOD_RETURN_EXISTS) error = ret;
        }
        // Re-add the desired tail in order; earlier adds win conflicts.
        for (size_t i = prefix; i < desired.size(); ++i) {
            const int ret = pfnAddFile ? pfnAddFile(desired[i].wstring().c_str()) : GMOD_RETURN_OK;
            if (ret < 0 && ret != GMOD_RETURN_EXISTS) error = ret;
        }
        return error;
    }

    // The single point that changes what gMod has loaded; load, unload and reorder all funnel through here.
    void ApplyLoadOrder()
    {
        if (!gmodReady || !pfnAddFile || !pfnRemoveFile) {
            statusMessage = "gMod is not initialised.";
            return;
        }
        auto desired = std::make_shared<std::vector<std::filesystem::path>>(BuildDesiredList());
        const uint64_t my_generation = ++apply_generation;

        Resources::EnqueueWorkerTask([desired, my_generation] {
            // Hold apply_mutex across the reconcile so add/remove sequences are serialised and only the newest request runs.
            std::scoped_lock lk(apply_mutex);
            if (my_generation != apply_generation.load()) return; // superseded while queued
            if (!gmodReady) return;
            const int error = ReconcileLocked(*desired);
            Resources::EnqueueMainTask([error, my_generation] {
                if (my_generation != apply_generation.load()) return; // a newer apply is in flight
                if (error < 0) statusMessage = "gMod failed to apply a pack change (error " + std::to_string(error) + ").";
                SyncExternalPacks();
            });
        });
    }

    bool UnloadTexturePack(const std::filesystem::path& path)
    {
        auto pack = FindPack(path, false);
        if (!pack) return false;
        pack->loaded = false;
        ApplyLoadOrder();
        return true;
    }

    void UnloadAllTexturePacks()
    {
        for (auto& pack : packs)
            pack.loaded = false;
        ApplyLoadOrder();
    }

    // Move the pack at `index` up (-1) or down (+1), changing its priority, then re-apply the order to gMod.
    bool MovePack(int index, int direction)
    {
        const int other = index + direction;
        if (index < 0 || index >= static_cast<int>(packs.size())) return false;
        if (other < 0 || other >= static_cast<int>(packs.size())) return false;
        std::swap(packs[index], packs[other]);
        if (gmodReady) ApplyLoadOrder();
        return true;
    }

    // Called once gMod is ready: adopt anything it already loaded (modlist.txt etc.) then push the user's enabled set.
    void RestoreLoadedPacks()
    {
        gmodLocalVersionChecked = false; // gMod is now loaded; refresh the displayed version
        SyncExternalPacks(/*clear_missing=*/false);
        ApplyLoadOrder();
    }

    void ShutdownGMod()
    {
        // Only tear down a gMod we loaded; a pre-loaded one is left running with its packs.
        if (gmodReady && gmodLoadedByToolbox) {
            // Unload synchronously (worker threads are stopping): hold apply_mutex, bump the generation to skip queued reconciles, then remove everything here so nothing reloads after.
            std::scoped_lock lk(apply_mutex);
            ++apply_generation;
            for (auto& pack : packs)
                pack.loaded = false;
            if (pfnRemoveFile) {
                for (const auto& path : QueryGmodFiles())
                    pfnRemoveFile(path.wstring().c_str());
            }
        }

        const bool owned = gmodLoadedByToolbox;

        gmodReady = false;
        pfnAddFile = nullptr;
        pfnRemoveFile = nullptr;
        pfnGetFiles = nullptr;
        pfnSetDevice = nullptr;

        // Free only our own dll (its DllMain reverts gMod's D3D9 hooks); never a pre-loaded one.
        if (owned && gmodDll) FreeLibrary(gmodDll);
        gmodDll = nullptr;
        gmodLoadedByToolbox = false;
        gmodLoadAttempted = false; // allow a fresh load later (e.g. when a pack is added)
        gmodLocalVersionChecked = false;
    }

    // =========================================================================
    // Texture pack management
    // =========================================================================

    TexturePackEntry* FindPack(const std::filesystem::path& path, bool create_if_not_found)
    {
        const auto it = std::ranges::find_if(packs, [&](const TexturePackEntry& e) {
            return e.path == path;
        });
        if (it == packs.end()) {
            if (create_if_not_found) {
                packs.push_back({path, PathToUtf8(path.stem()), false, false});
                return &packs.back();
            }
            return nullptr;
        }
        return &(*it);
    }

    bool LoadTexturePack(const std::filesystem::path& path)
    {
        if (!std::filesystem::exists(path)) {
            statusMessage = "File not found: " + PathToUtf8(path);
            return false;
        }
        auto pack = FindPack(path, true);
        const auto filename = PathToUtf8(pack->path.filename());
        pack->loaded = true;
        if (!gmodReady) {
            // First pack configured but gMod isn't loaded yet: fetch it now; the pack applies once gMod is ready.
            statusMessage = "Preparing gMod for: " + filename;
            CheckAndUpdateGmod();
            return true;
        }
        // The load runs on a worker thread and reports on reconcile; show an optimistic status meanwhile.
        statusMessage = "Loading: " + filename;
        ApplyLoadOrder();
        return true;
    }



    // =========================================================================
    // gMod version check / download
    // =========================================================================

    // Read a DLL's file-version resource as "major.minor.patch.build" (gMod.dll's matches its release tag); empty if none.
    std::string GetDllFileVersion(const std::filesystem::path& path)
    {
        if (path.empty()) return {};
        const auto wp = path.wstring();
        DWORD handle = 0;
        const DWORD size = GetFileVersionInfoSizeW(wp.c_str(), &handle);
        if (!size) return {};
        std::vector<BYTE> data(size);
        if (!GetFileVersionInfoW(wp.c_str(), handle, size, data.data())) return {};
        VS_FIXEDFILEINFO* fi = nullptr;
        UINT len = 0;
        if (!VerQueryValueW(data.data(), L"\\", reinterpret_cast<LPVOID*>(&fi), &len) || !fi) return {};
        return std::format("{}.{}.{}.{}", HIWORD(fi->dwFileVersionMS), LOWORD(fi->dwFileVersionMS), HIWORD(fi->dwFileVersionLS), LOWORD(fi->dwFileVersionLS));
    }

    // Path of the gMod.dll in use: the live module if active, else the managed copy.
    std::filesystem::path CurrentGmodDllPath()
    {
        if (gmodReady && gmodDll) {
            wchar_t buf[MAX_PATH] = {};
            if (GetModuleFileNameW(gmodDll, buf, MAX_PATH)) return buf;
        }
        return ManagedGmodDllPath();
    }

    void RefreshLocalGmodVersion()
    {
        gmodLocalVersion = GetDllFileVersion(CurrentGmodDllPath());
        gmodLocalVersionChecked = true;
    }

    // Compare dotted versions ("1.8.0.2"): >0 if a newer, <0 if older, 0 if equal; missing trailing parts count as 0.
    int CompareVersions(const std::string& a, const std::string& b)
    {
        const auto parse = [](const std::string& v) {
            std::vector<int> out;
            for (size_t i = 0; i < v.size();) {
                if (v[i] < '0' || v[i] > '9') {
                    ++i;
                    continue;
                }
                int n = 0;
                while (i < v.size() && v[i] >= '0' && v[i] <= '9')
                    n = n * 10 + (v[i++] - '0');
                out.push_back(n);
            }
            return out;
        };
        const auto va = parse(a), vb = parse(b);
        for (size_t i = 0; i < std::max(va.size(), vb.size()); ++i) {
            const int x = i < va.size() ? va[i] : 0;
            const int y = i < vb.size() ? vb[i] : 0;
            if (x != y) return x < y ? -1 : 1;
        }
        return 0;
    }

    // Load the managed gMod.dll after it changed on disk (caller already unloaded any active copy): clear the latch and init.
    void ReloadGmod()
    {
        gmodLocalVersionChecked = false;
        gmodLoadAttempted = false;
        InitGMod();
    }

    // Is a gMod we did not load already in the process (compatible or not)? If so we never download/replace it.
    bool ForeignGmodLoaded()
    {
        if (gmodReady) return !gmodLoadedByToolbox;
        ProbePreloadedGmodOnce();
        return preloadGmod != nullptr;
    }

    // Fetch the latest release; download+load it if newer, but only for a gMod we own.
    void CheckAndUpdateGmod()
    {
        if (gmodUpdateStep != GmodUpdateStep::Idle) return;
        gmodUpdateStep = GmodUpdateStep::Checking;
        gmodUpdateStatus.clear();

        // We may replace gMod only when we manage the copy; a pre-loaded one is checked, not touched.
        const bool auto_managed = !ForeignGmodLoaded();
        const auto in_use_path = CurrentGmodDllPath();

        Resources::EnqueueWorkerTask([auto_managed, in_use_path] {
            std::string response;
            unsigned int tries = 0;
            const auto url = "https://api.github.com/repos/gwdevhub/gMod/releases/latest";
            bool success = false;
            do {
                success = Resources::Download(url, response);
                tries++;
            } while (!success && tries < 5);

            gmod_github::Release release;
            const bool parsed = success && !glz::read<gmod_json_opts>(release, response);

            std::string version, dll_url;
            if (parsed) {
                version = release.tag_name;
                if (!version.empty() && (version[0] == 'v' || version[0] == 'V')) version.erase(0, 1);
                for (const auto& asset : release.assets) {
                    if (asset.name == "gMod.dll") dll_url = asset.browser_download_url;
                }
            }

            if (!parsed || dll_url.empty()) {
                Resources::EnqueueMainTask([] {
                    gmodUpdateStep = GmodUpdateStep::Idle;
                    gmodUpdateStatus = "Could not check for gMod updates.";
                });
                return;
            }

            const auto managed = ManagedGmodDllPath();
            const std::string local = GetDllFileVersion(in_use_path);
            if (!local.empty() && CompareVersions(version, local) <= 0) {
                // Already have the latest (or newer).
                Resources::EnqueueMainTask([version, local] {
                    gmodUpdateStep = GmodUpdateStep::Idle;
                    gmodLatestVersion = version;
                    gmodLocalVersion = local;
                    gmodUpdateStatus = "gMod is up to date (" + version + ").";
                });
                return;
            }

            if (!auto_managed) {
                // Newer exists but the active gMod isn't ours; report it, leave updating to its owner.
                Resources::EnqueueMainTask([version, local] {
                    gmodUpdateStep = GmodUpdateStep::Idle;
                    gmodLatestVersion = version;
                    if (!local.empty()) gmodLocalVersion = local;
                    gmodUpdateStatus = "A newer gMod (" + version + ") is available; it was loaded by your launcher, so update it there.";
                });
                return;
            }

            Resources::EnqueueMainTask([version] {
                gmodUpdateStep = GmodUpdateStep::Downloading;
                gmodLatestVersion = version;
                gmodUpdateStatus = "Downloading gMod " + version + "...";
            });

            // Download to a temp file since our gMod.dll may still be loaded (locked); swapped in on the main thread below.
            const auto tmp = managed.parent_path() / "gMod.tmp.dll";
            Resources::EnsureFolderExists(managed.parent_path());
            std::wstring error;
            const bool ok = Resources::Download(tmp, dll_url, error);

            Resources::EnqueueMainTask([ok, version, error, managed, tmp] {
                gmodUpdateStep = GmodUpdateStep::Idle;
                if (!ok) {
                    gmodUpdateStatus = "gMod download failed: " + TextUtils::WStringToString(error);
                    return;
                }
                // Unload (unlocks the file), rename temp over the original, reload; remember enabled packs since ShutdownGMod clears them.
                std::vector<std::filesystem::path> enabled;
                for (const auto& pack : packs) {
                    if (pack.loaded) enabled.push_back(pack.path);
                }
                ShutdownGMod();
                std::error_code ec;
                std::filesystem::rename(tmp, managed, ec);
                if (ec) {
                    std::filesystem::remove(tmp, ec);
                    gmodUpdateStatus = "gMod update failed: could not replace gMod.dll.";
                    return;
                }
                for (auto& pack : packs) {
                    pack.loaded = std::ranges::find(enabled, pack.path) != enabled.end();
                }
                gmodLocalVersion = version;
                gmodUpdateStatus = "Updated gMod to " + version + ".";
                ReloadGmod();
            });
        });
    }

    // =========================================================================
    // UI helpers
    // =========================================================================

    void OnTexmodFileChosen(const char* path)
    {
        if (!path) return;
        // The dialog returns UTF-8; decode to a wide string first so non-ASCII names survive (a char* path would decode as ANSI).
        const std::filesystem::path p = TextUtils::StringToWString(path);
        if (!std::filesystem::exists(p)) return;
        LoadTexturePack(p);
    }

    void DrawStatusBar()
    {
        if (gmodReady) {
            ImGui::TextColored({0.4f, 1.0f, 0.4f, 1.0f}, ICON_FA_CHECK " gMod active");
        }
        else if (gmodIncompatible) {
            ImGui::TextColored({1.0f, 0.4f, 0.4f, 1.0f}, ICON_FA_TIMES " gMod blocked (incompatible version pre-loaded)");
        }
        else if (packs.empty()) {
            ImGui::TextDisabled(ICON_FA_INFO_CIRCLE " gMod inactive - add a texture pack to enable");
        }
        else {
            ImGui::TextColored({1.0f, 0.4f, 0.4f, 1.0f}, ICON_FA_TIMES " gMod not initialised");
            ImGui::SameLine();
            if (ImGui::SmallButton("Retry")) {
                gmodLoadAttempted = false;
                InitGMod();
            }
        }
        if (!statusMessage.empty()) {
            ImGui::SameLine();
            ImGui::TextDisabled("  %s", statusMessage.c_str());
        }
    }

    // gMod.dll status: show the installed version against the latest release and offer a manual re-check.
    void DrawGmodSource()
    {
        // A launcher pre-loaded a gMod we can't drive: warn and stop (the version row below would be misleading).
        if (gmodIncompatible) {
            const std::string old_version = GetDllFileVersion(gmodIncompatiblePath);
            ImGui::TextColored({1.0f, 0.85f, 0.4f, 1.0f}, "An incompatible gMod%s is already loaded by your launcher.",
                               old_version.empty() ? "" : (" (" + old_version + ")").c_str());
            ImGui::TextDisabled("Update it to a current gMod%s, or remove it so GWToolbox manages gMod itself, then restart Guild Wars.",
                                gmodLatestVersion.empty() ? "" : (" (latest: " + gmodLatestVersion + ")").c_str());
            return;
        }

        if (!gmodLocalVersionChecked) RefreshLocalGmodVersion();

        if (!gmodLocalVersion.empty())
            ImGui::Text("Installed gMod version: %s", gmodLocalVersion.c_str());
        else
            ImGui::TextDisabled("No gMod.dll installed yet.");

        if (!gmodLatestVersion.empty()) {
            ImGui::SameLine();
            if (!gmodLocalVersion.empty() && CompareVersions(gmodLatestVersion, gmodLocalVersion) <= 0)
                ImGui::TextColored({0.4f, 1.0f, 0.4f, 1.0f}, ICON_FA_CHECK " up to date");
            else
                ImGui::TextColored({1.0f, 0.85f, 0.4f, 1.0f}, "(latest: %s)", gmodLatestVersion.c_str());
        }

        const bool busy = gmodUpdateStep != GmodUpdateStep::Idle;
        ImGui::BeginDisabled(busy);
        const char* label = gmodUpdateStep == GmodUpdateStep::Downloading ? "Downloading..." : gmodUpdateStep == GmodUpdateStep::Checking ? "Checking..." : "Check for gMod updates";
        if (ImGui::Button(label)) CheckAndUpdateGmod();
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("gMod is downloaded automatically and kept in your GWToolbox folder.\nClick to check for a newer version now.");

        if (!gmodUpdateStatus.empty()) ImGui::TextDisabled("%s", gmodUpdateStatus.c_str());
    }

    void DrawPackList()
    {
        ImGui::Text("Texture packs:");
        if (gmodReady) {
            ImGui::SameLine();
            if (ImGui::SmallButton(ICON_FA_SYNC " Refresh")) SyncExternalPacks();
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Re-query gMod for any packs it loaded independently.");
        }

        if (packs.empty()) {
            ImGui::TextDisabled("  No packs added. Use the button below to add one.");
            return;
        }

        constexpr ImGuiTableFlags tableFlags = ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit;

        if (!ImGui::BeginTable("##packs", 4, tableFlags)) return;

        const auto font_size = ImGui::CalcTextSize(" ");
        const ImGuiStyle& style = ImGui::GetStyle();
        const auto btn_width = [&](const char* label) {
            return ImGui::CalcTextSize(label).x + style.FramePadding.x * 2.f;
        };
        // Actions column holds the move-up / move-down / delete buttons.
        const float actions_width = btn_width(ICON_FA_ARROW_UP) + btn_width(ICON_FA_ARROW_DOWN) + btn_width(ICON_FA_TRASH) + style.ItemSpacing.x * 2.f;
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, font_size.y * 2.f);
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Path", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, actions_width);
        ImGui::TableHeadersRow();

        for (int i = 0; i < static_cast<int>(packs.size()); i++) {
            auto& pack = packs[i];
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::PushID(i);
            bool loaded = pack.loaded;
            if (ImGui::Checkbox("##en", &loaded)) {
                if (loaded)
                    LoadTexturePack(pack.path);
                else
                    UnloadTexturePack(pack.path);
                ImGui::PopID();
                break;
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip(loaded ? "Click to unload" : "Click to load");
            ImGui::PopID();

            ImGui::TableSetColumnIndex(1);
            if (pack.loaded && !pack.external)
                ImGui::TextUnformatted(pack.label.c_str());
            else if (pack.loaded)
                ImGui::TextDisabled("%s *", pack.label.c_str());
            else
                ImGui::TextDisabled("%s", pack.label.c_str());

            ImGui::TableSetColumnIndex(2);
            const std::string pathStr = PathToUtf8(pack.path);
            ImGui::TextDisabled("%s", pathStr.c_str());
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", pathStr.c_str());

            ImGui::TableSetColumnIndex(3);
            ImGui::PushID(i);

            const bool is_first = (i == 0);
            const bool is_last = (i == static_cast<int>(packs.size()) - 1);

            ImGui::BeginDisabled(is_first);
            const bool move_up = ImGui::Button(ICON_FA_ARROW_UP);
            ImGui::EndDisabled();
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Move up (higher priority on texture conflicts)");
            if (move_up) {
                MovePack(i, -1);
                ImGui::PopID();
                break;
            }

            ImGui::SameLine();
            ImGui::BeginDisabled(is_last);
            const bool move_down = ImGui::Button(ICON_FA_ARROW_DOWN);
            ImGui::EndDisabled();
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Move down (lower priority on texture conflicts)");
            if (move_down) {
                MovePack(i, 1);
                ImGui::PopID();
                break;
            }

            ImGui::SameLine();
            bool del_bool = false;
            if (ImGui::ConfirmButton(ICON_FA_TRASH, &del_bool)) {
                if (pack.loaded) UnloadTexturePack(pack.path);
                packs.erase(packs.begin() + i);
                // No packs left to serve: drop gMod so it stops hooking the device.
                if (packs.empty()) gmodUnloadRequested = true;
                ImGui::PopID();
                break;
            }
            ImGui::PopID();
        }

        ImGui::EndTable();
        if (!packs.empty()) {
            if (ImGui::Button(ICON_FA_BAN " Unload all")) UnloadAllTexturePacks();
        }
    }

    // =========================================================================
    // DirectX9 texture capture
    // =========================================================================

    // Hook D3D9 CreateTexture to gather textures by gMod hash for DDS/PNG export; opt-in (recording) since hashing costs FPS, with a Release hook so the map never keeps a freed pointer.
    std::map<uint32_t, IDirect3DTexture9*> dx9_textures_created_by_hash;
    bool recording = false;

    typedef HRESULT(WINAPI* CreateDx9Texture_pt)(IDirect3DDevice9*, UINT, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL, IDirect3DTexture9**, HANDLE*);
    CreateDx9Texture_pt CreateDx9Texture_Func = 0, CreateDx9Texture_Ret = 0;

    using TextureRelease_pt = ULONG(WINAPI*)(IDirect3DTexture9*);
    TextureRelease_pt TextureRelease_Func = nullptr;
    TextureRelease_pt TextureRelease_Ret = nullptr;

    IDirect3DTexture9* LastCreatedTexture = nullptr;

    // Hook Release to clean up tracking
    ULONG WINAPI OnD3D9TextureRelease(IDirect3DTexture9* texture)
    {
        GW::Hook::EnterHook();
        ULONG ref = TextureRelease_Ret(texture);
        if (ref == 0) {
            if (texture == LastCreatedTexture) LastCreatedTexture = 0;
            // Remove from hash map if present
            for (auto it = dx9_textures_created_by_hash.begin(); it != dx9_textures_created_by_hash.end();) {
                if (it->second == texture) {
                    it = dx9_textures_created_by_hash.erase(it);
                }
                else {
                    ++it;
                }
            }
        }
        GW::Hook::LeaveHook();

        return ref;
    }

    HRESULT WINAPI OnD3D9CreateTexture(IDirect3DDevice9* device, UINT Width, UINT Height, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DTexture9** ppTexture, HANDLE* pSharedHandle)
    {
        GW::Hook::EnterHook();
        HRESULT result = CreateDx9Texture_Ret(device, Width, Height, Levels, Usage, Format, Pool, ppTexture, pSharedHandle);

        // Hash the texture from the *previous* call (deferred so the game has filled it); our reference keeps it alive across the gap.
        if (auto tex = LastCreatedTexture) {
            LastCreatedTexture = nullptr; // clear before Release so the re-entrant release hook is a no-op
            const auto hash = Resources::GetTexmodHash(tex);
            if (hash && !dx9_textures_created_by_hash.contains(hash)) {
                dx9_textures_created_by_hash[hash] = tex;
            }
            tex->Release(); // drop our deferred-hash reference
        }

        // Only track managed/system-mem textures (lockable, survive reset); skip D3DPOOL_DEFAULT (unsafe to lock, and a held ref would block device reset).
        if (SUCCEEDED(result) && *ppTexture && Pool != D3DPOOL_DEFAULT) {
            // Hook Release on first texture creation
            if (!TextureRelease_Func) {
                uintptr_t* texture_vtable = *reinterpret_cast<uintptr_t**>(*ppTexture);
                constexpr int RELEASE_INDEX = 2;
                TextureRelease_Func = (TextureRelease_pt)texture_vtable[RELEASE_INDEX];
                GW::Hook::CreateHook((void**)&TextureRelease_Func, OnD3D9TextureRelease, (void**)&TextureRelease_Ret);
                GW::Hook::EnableHooks(TextureRelease_Func);
            }
            LastCreatedTexture = *ppTexture;
            LastCreatedTexture->AddRef(); // keep alive until we hash it next call
        }

        GW::Hook::LeaveHook();
        return result;
    }

    // Toggle the CreateTexture capture hook to match `record`; the Release hook installs on first start and stays after stopping.
    void SetTextureCapture(bool record)
    {
        if (record) {
            if (!CreateDx9Texture_Func) {
                IDirect3DDevice9* device = GW::Render::GetDevice();
                if (!device) return;
                // Get vtable pointer
                uintptr_t* vtable = *reinterpret_cast<uintptr_t**>(device);
                // CreateTexture is at index 23 in IDirect3DDevice9 vtable
                constexpr int CREATE_TEXTURE_INDEX = 23;
                CreateDx9Texture_Func = (CreateDx9Texture_pt)vtable[CREATE_TEXTURE_INDEX];
                GW::Hook::CreateHook((void**)&CreateDx9Texture_Func, OnD3D9CreateTexture, (void**)&CreateDx9Texture_Ret);
                GW::Hook::EnableHooks(CreateDx9Texture_Func);
            }

            // Hook texture Release to track when textures are destroyed
            if (!TextureRelease_Func) {
                // Create a temporary dummy texture to get its vtable.
                IDirect3DTexture9* temp_texture = nullptr;
                IDirect3DDevice9* device = GW::Render::GetDevice();
                if (device && SUCCEEDED(device->CreateTexture(1, 1, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &temp_texture, nullptr))) {
                    uintptr_t* texture_vtable = *reinterpret_cast<uintptr_t**>(temp_texture);
                    // Release is at index 2 in IUnknown (inherited by IDirect3DTexture9)
                    constexpr int RELEASE_INDEX = 2;
                    TextureRelease_Func = (TextureRelease_pt)texture_vtable[RELEASE_INDEX];
                    GW::Hook::CreateHook((void**)&TextureRelease_Func, OnD3D9TextureRelease, (void**)&TextureRelease_Ret);
                    GW::Hook::EnableHooks(TextureRelease_Func);
                    temp_texture->Release(); // Clean up temp texture
                }
            }
            return;
        }
        // Stop capturing, but keep the Release hook and map so the gallery stays valid.
        if (CreateDx9Texture_Func) {
            GW::Hook::DisableHooks(CreateDx9Texture_Func);
            GW::Hook::RemoveHook(CreateDx9Texture_Func);
            CreateDx9Texture_Func = nullptr;
        }
        if (auto tex = LastCreatedTexture) {
            LastCreatedTexture = nullptr;
            tex->Release(); // drop our deferred-hash reference
        }
    }

    // Full teardown for module shutdown: remove both hooks and forget every capture.
    void TeardownTextureCapture()
    {
        recording = false;
        if (CreateDx9Texture_Func) {
            GW::Hook::DisableHooks(CreateDx9Texture_Func);
            GW::Hook::RemoveHook(CreateDx9Texture_Func);
            CreateDx9Texture_Func = nullptr;
        }
        if (TextureRelease_Func) {
            GW::Hook::DisableHooks(TextureRelease_Func);
            GW::Hook::RemoveHook(TextureRelease_Func);
            TextureRelease_Func = nullptr;
        }
        dx9_textures_created_by_hash.clear();
        if (LastCreatedTexture) {
            LastCreatedTexture->Release(); // drop our deferred-hash reference
            LastCreatedTexture = nullptr;
        }
    }

    // While recording, show an on-screen reminder (top-centre) with a button to stop.
    void DrawRecordingOverlay()
    {
        if (!recording) return;

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        const float center_x = viewport->Pos.x + viewport->Size.x * 0.5f;
        const float top_y = viewport->Pos.y + viewport->Size.y * 0.04f;

        // Big title on the background draw list (over the game, behind toolbox windows).
        ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
        ImFont* font = FontLoader::GetFont();
        const char* title = ICON_FA_CIRCLE " Recording textures";
        constexpr float title_size = static_cast<float>(FontLoader::FontSize::widget_small);
        const ImVec2 title_dim = font->CalcTextSizeA(title_size, FLT_MAX, 0.f, title);
        const ImVec2 title_pos(center_x - title_dim.x * 0.5f, top_y);
        ImGui::PushFont(font, draw_list, title_size);
        draw_list->AddText({title_pos.x + 2.f, title_pos.y + 2.f}, IM_COL32(0, 0, 0, 200), title);
        draw_list->AddText(title_pos, IM_COL32(255, 80, 80, 255), title);
        ImGui::PopFont(draw_list);

        // Stop button in a transparent window below (a button needs a real window).
        const float below_y = title_pos.y + title_dim.y + ImGui::GetStyle().ItemSpacing.y;
        ImGui::SetNextWindowPos({center_x, below_y}, ImGuiCond_Always, {0.5f, 0.f});
        ImGui::SetNextWindowBgAlpha(0.f);
        constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
        if (ImGui::Begin("##texmod_recording_overlay", nullptr, flags)) {
            ImGui::TextUnformatted("Capturing every texture as you play is expensive and lowers your framerate.");
            const char* btn = ICON_FA_STOP " Stop recording textures";
            const float btn_w = ImGui::CalcTextSize(btn).x + ImGui::GetStyle().FramePadding.x * 2.f;
            ImGui::SetCursorPosX((ImGui::GetWindowWidth() - btn_w) * 0.5f);
            if (ImGui::Button(btn)) {
                recording = false;
            }
        }
        ImGui::End();
    }

    void DrawTextureGallery()
    {
        if (!ImGui::CollapsingHeader("Loaded DirectX9 Textures")) return;
        constexpr ImVec2 scaled_size = {64.f, 64.f};
        constexpr ImVec4 tint(1, 1, 1, 1);
        const auto normal_bg = ImColor(IM_COL32(0, 0, 0, 0));
        constexpr auto uv0 = ImVec2(0, 0);

        if (recording) {
            if (ImGui::Button(ICON_FA_STOP " Stop recording")) recording = false;
        }
        else {
            static bool confirm_record = false;
            if (ImGui::ConfirmButton(
                    ICON_FA_CIRCLE " Record textures", &confirm_record,
                    "Recording captures every texture the game creates as you play.\n"
                    "Doing this continuously is expensive and will lower your framerate\n"
                    "while it is active.\n\nStart recording textures?"
                )) {
                recording = true;
                confirm_record = false;
            }
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Reset")) {
            dx9_textures_created_by_hash.clear();
        }
        ImGui::SameLine();
        ImGui::TextDisabled("%d captured", static_cast<int>(dx9_textures_created_by_hash.size()));

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.f, 0.5f));

        ImGui::StartSpacedElements(scaled_size.x);

        size_t i = 0;
        for (const auto& it : dx9_textures_created_by_hash) {
            const auto texture = it.second;
            const auto hash = it.first;
            if (!texture) continue;
            ImGui::PushID(i++);

            const auto uv1 = ImGui::CalculateUvCrop(texture, scaled_size);
            ImGui::NextSpacedElement();
            const auto clicked = ImGui::ImageButton(texture, scaled_size, uv0, uv1, -1, normal_bg, tint);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("gMod/uMod/Texmod Hash: 0x%08x (Click to download)", hash);
            }
            if (clicked) {
                ImGui::SetContextMenu([hash](void*) {
                    ImGui::Text("gMod/uMod/Texmod Hash: 0x%08x", hash);
                    ImGui::Separator();
                    const char* ext = 0;
                    if (ImGui::Button("Download as DDS (for gMod/uMod/Texmod)")) {
                        ext = "dds";
                    }
                    if (ImGui::Button("Download as PNG")) {
                        ext = "png";
                    }
                    if (ext) {
                        const auto filename = std::format("GW.EXE_0x{:08X}.{}", hash, ext);
                        const auto write_to = Resources::GetPath("extracted_textures", filename);
                        Resources::EnsureFolderExists(Resources::GetPath("extracted_textures"));
                        const auto found = dx9_textures_created_by_hash.find(hash);
                        if (found != dx9_textures_created_by_hash.end()) {
                            Resources::SaveTextureToFile(found->second, write_to);
                        }
                        return false;
                    }
                    return true;
                });
            }
            ImGui::PopID();
        }

        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
        ImGui::PopStyleVar();
    }

} // namespace

void TexmodModule::Update(float)
{
    // Keep the D3D9 CreateTexture capture hook matched to the recording toggle.
    SetTextureCapture(recording);

    // Unload gMod once nothing needs it, but only one we loaded: tearing down a pre-loaded one would cancel the in-flight unload and re-adopt it next frame, resurrecting its packs.
    if (gmodUnloadRequested) {
        gmodUnloadRequested = false;
        if (gmodLoadedByToolbox) ShutdownGMod();
    }

    if (gmodReady) return;
    InitGMod();
}

void TexmodModule::Draw(IDirect3DDevice9*)
{
    DrawRecordingOverlay();
}

void TexmodModule::Terminate()
{
    TeardownTextureCapture();
    ShutdownGMod(); // unloads every pack and frees gMod.dll
    ToolboxModule::Terminate();
}

void TexmodModule::DrawSettingsInternal()
{
    DrawStatusBar();
    ImGui::Separator();
    DrawGmodSource();
    ImGui::Separator();
    DrawPackList();
    ImGui::Separator();
    if (ImGui::Button("Add texture pack")) Resources::OpenFileDialog(OnTexmodFileChosen, "tpf,zip,dds", PathToUtf8(Resources::GetSettingsFolderPath()).c_str());
    ImGui::TextDisabled("Supported: .tpf, .zip (texmod format), .dds");
    ImGui::Separator();
    DrawTextureGallery();
}

void TexmodModule::LoadSettings(SettingsDoc& doc, ToolboxIni* legacy)
{
    ToolboxModule::LoadSettings(doc, legacy);
    std::vector<PackSetting> stored;
    if (doc.Get(Name(), "packs", stored)) {
        packs.clear();
        for (const auto& entry : stored) {
            if (entry.path.empty()) continue;
            // Stored as UTF-8 (see SaveSettings); decode to a wide path so non-ASCII names survive.
            std::filesystem::path p = TextUtils::StringToWString(entry.path);
            packs.push_back({p, PathToUtf8(p.stem()), /*loaded=*/entry.loaded});
        }
    }
    else {
        const int count = legacy ? legacy->GetLongValue(INI_SECTION, INI_PACK_COUNT, 0) : 0;
        packs.clear();
        for (int i = 0; i < count; i++) {
            const std::string key = std::string(INI_PACK_PATH) + std::to_string(i);
            const char* val = legacy->GetValue(INI_SECTION, key.c_str(), nullptr);
            if (!val || !*val) continue;
            std::filesystem::path p = TextUtils::StringToWString(val);
            const std::string enabled_key = std::string(INI_PACK_ENABLED) + std::to_string(i);
            const bool enabled = legacy->GetBoolValue(INI_SECTION, enabled_key.c_str(), false);
            packs.push_back({p, PathToUtf8(p.stem()), /*loaded=*/enabled});
        }
    }
    // gMod isn't ready yet (no device); the restored enabled flags are pushed later by RestoreLoadedPacks(), so this is a no-op until then.
    SyncExternalPacks();

    // Only fetch gMod for users who actually use texture packs; those with none get it lazily on their first added pack.
    if (!packs.empty()) CheckAndUpdateGmod();
}

void TexmodModule::SaveSettings(SettingsDoc& doc)
{
    ToolboxModule::SaveSettings(doc);
    std::vector<PackSetting> stored;
    stored.reserve(packs.size());
    for (const auto& pack : packs) {
        stored.push_back({PathToUtf8(pack.path), pack.loaded});
    }
    doc.Set(Name(), "packs", stored);
}
