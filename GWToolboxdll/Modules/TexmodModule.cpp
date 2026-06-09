#include "TexmodModule.h"
#include "stdafx.h"

#include <ToolboxIni.h>
#include <algorithm>
#include <atomic>
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
#include <Modules/Resources.h>
#include <Utils/FontLoader.h>
#include <Utils/TextUtils.h>

// gMod GitHub release shape, parsed from the releases API. Kept at namespace scope
// (external linkage) because glaze reflection can't bind types from an anonymous
// namespace - mirrors Updater's github_api structs.
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

    // gMod return codes we care about (see gMod's Error.h). AddFile returns
    // EXISTS - a negative value - when the pack is already loaded, which for our
    // purposes is success, not an error.
    constexpr int GMOD_RETURN_OK = 0;
    constexpr int GMOD_RETURN_EXISTS = -70;

    // gMod version check / download (mirrors Updater's GitHub-release logic). gMod is
    // managed automatically: on startup we fetch the latest release and, if it is
    // newer than the copy kept in the GWToolbox folder (or none is present), download
    // it there and load it.
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
    // Set when gMod is no longer needed (last pack removed); the unload is performed
    // from Update() rather than the draw pass.
    bool gmodUnloadRequested = false;
    // True only when toolbox loaded gMod itself (LoadLibrary on our managed copy). When
    // gMod was already injected by something else (gwlauncher, daybreak, or a d3d9.dll
    // proxy) this stays false: we drive it but never update, replace or unload it.
    bool gmodLoadedByToolbox = false;

    constexpr const char* INI_SECTION = "TexmodModule";
    constexpr const char* INI_PACK_COUNT = "PackCount";
    constexpr const char* INI_PACK_PATH = "Pack";
    constexpr const char* INI_PACK_ENABLED = "PackEnabled";

    // path::string() would narrow to the ANSI codepage and lose non-ASCII characters
    // (e.g. a "µ" in a pack name). Encode as UTF-8 instead, which round-trips through
    // the INI and renders correctly in ImGui. Pair with TextUtils::StringToWString to
    // decode back to a path.
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
        // GetFiles is required: we reconcile against the load order it reports, so
        // a build whose GetFiles is missing (or doesn't report load order) can't be
        // driven correctly.
        if (!pfnAddFile || !pfnRemoveFile || !pfnGetFiles || !pfnSetDevice) {
            statusMessage = "Error: gMod.dll is missing AddFile/RemoveFile/GetFiles/SetDevice exports. "
                            "Please use a gMod build that includes these functions.";
            return false;
        }
        return true;
    }

    // Read gMod's currently-loaded files, in load order (== priority): gMod hands
    // them back in the order it loaded them, so this is what we diff against to
    // reconcile. Best-effort - empty if GetFiles is absent or errors.
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

    // Reconcile our pack list with what gMod actually has loaded (via GetFiles):
    // mark known packs loaded and adopt any gMod loaded on its own (e.g. from
    // modlist.txt) as external entries. When clear_missing is true, packs gMod
    // is not currently serving are marked unloaded - the right behaviour after an
    // operation. Pass false on startup, before the restored enabled state has
    // been pushed to gMod, so it is not wiped by gMod's still-empty set.
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

    // Push the saved enabled state to gMod once it becomes ready. Defined below,
    // after ApplyLoadOrder; forward-declared here for InitGMod.
    void RestoreLoadedPacks();

    // Check GitHub and download a newer gMod if needed. Forward-declared so adding a
    // pack can trigger the download when gMod isn't loaded yet. Defined further down.
    void CheckAndUpdateGmod();

    // Where the toolbox keeps its managed gMod.dll (auto-downloaded).
    std::filesystem::path ManagedGmodDllPath()
    {
        return Resources::GetPath("gmod") / "gMod.dll";
    }

    // Bring gMod up: adopt one already injected into the process if present, otherwise
    // load our own managed copy and hand it the game's device.
    bool InitGMod()
    {
        if (gmodReady) return true;

        auto device = GW::Render::GetDevice();
        if (!device) {
            statusMessage = "Error: Could not get IDirect3DDevice9 from GW::Render::GetDevice().";
            return false;
        }

        // 1. Adopt a gMod already loaded into this process by something else (gwlauncher,
        //    daybreak, or a d3d9.dll proxy). GetProcAddress loads nothing new. Probe the
        //    exports first so the system d3d9.dll (always present, lacks them) is skipped
        //    without ResolveTextureClientFunctions clobbering statusMessage with an error.
        //    We don't own an adopted gMod, so we never update, replace or unload it.
        for (const wchar_t* name : {L"gMod.dll", L"d3d9.dll"}) {
            HMODULE h = GetModuleHandleW(name);
            if (!h || !GetProcAddress(h, "AddFile") || !GetProcAddress(h, "RemoveFile") || !GetProcAddress(h, "GetFiles") || !GetProcAddress(h, "SetDevice")) continue;
            gmodDll = h;
            ResolveTextureClientFunctions(); // exports confirmed above, so this succeeds
            gmodLoadedByToolbox = false;
            pfnSetDevice(device);
            gmodReady = true;
            statusMessage = "gMod was already active (pre-loaded). Texture pack loading enabled.";
            RestoreLoadedPacks();
            return true;
        }

        // 2. Otherwise load our own managed copy. Nothing to mod with no packs: leave
        //    gMod unloaded rather than hook the device for nothing.
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
            statusMessage = "Error: Could not load gMod.dll.";
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

    // gMod's AddFile decodes and DDS-compresses textures, which is slow enough to
    // hitch the game if run on the render/main thread. So snapshot the desired set
    // here (reading packs on the calling thread, where it is valid) and hand the
    // heavy add/remove work to a worker thread; reconcile back on the main thread.
    //
    // apply_mutex serialises the worker-side reconciles, and apply_generation -
    // bumped per request - lets a stale request (superseded while queued) skip its
    // work, so rapid toggles can't apply out of order with 20 worker threads.
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

    // Reconcile gMod's loaded set to `desired` with AddFile/RemoveFile. We read
    // gMod's current load order straight from GetFiles (it reports files in load
    // order). gMod gives the first-loaded pack priority on a texture conflict, so
    // to honour the requested order we keep the longest matching prefix, remove
    // everything after it, and re-add the desired tail in order. Returns the last
    // gMod error (or 0). Caller must hold apply_mutex.
    int ReconcileLocked(const std::vector<std::filesystem::path>& desired)
    {
        const auto current = QueryGmodFiles();
        size_t prefix = 0;
        while (prefix < current.size() && prefix < desired.size() && current[prefix] == desired[prefix]) {
            ++prefix;
        }
        // Remove the diverging tail, deepest first, leaving the kept prefix intact.
        for (size_t i = current.size(); i-- > prefix;) {
            if (pfnRemoveFile) pfnRemoveFile(current[i].wstring().c_str());
        }
        // Re-add the desired tail in order; earlier adds win conflicts.
        int error = GMOD_RETURN_OK;
        for (size_t i = prefix; i < desired.size(); ++i) {
            const int ret = pfnAddFile ? pfnAddFile(desired[i].wstring().c_str()) : GMOD_RETURN_OK;
            if (ret < 0 && ret != GMOD_RETURN_EXISTS) error = ret;
        }
        return error;
    }

    // The single point that changes what gMod has loaded; load, unload and reorder
    // all funnel through here.
    void ApplyLoadOrder()
    {
        if (!gmodReady || !pfnAddFile || !pfnRemoveFile) {
            statusMessage = "gMod is not initialised.";
            return;
        }
        auto desired = std::make_shared<std::vector<std::filesystem::path>>(BuildDesiredList());
        const uint64_t my_generation = ++apply_generation;

        Resources::EnqueueWorkerTask([desired, my_generation] {
            // Hold apply_mutex across the whole reconcile so add/remove sequences
            // are serialised and only the newest request actually runs.
            std::lock_guard lk(apply_mutex);
            if (my_generation != apply_generation.load()) return; // superseded while queued
            if (!gmodReady) return;
            const int error = ReconcileLocked(*desired);
            Resources::EnqueueMainTask([error, my_generation] {
                if (my_generation != apply_generation.load()) return; // a newer apply is in flight
                if (error < 0) statusMessage = "gMod failed to load a pack (error " + std::to_string(error) + ").";
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

    // Move the pack at `index` one slot up (direction -1) or down (+1) in the
    // list, changing its priority, then re-apply the order to gMod.
    bool MovePack(int index, int direction)
    {
        const int other = index + direction;
        if (index < 0 || index >= static_cast<int>(packs.size())) return false;
        if (other < 0 || other >= static_cast<int>(packs.size())) return false;
        std::swap(packs[index], packs[other]);
        if (gmodReady) ApplyLoadOrder();
        return true;
    }

    // Called once gMod is ready: adopt anything it already loaded (modlist.txt
    // etc.) without disturbing the enabled flags restored from settings, then push
    // the combined set so the packs the user had enabled are loaded again.
    void RestoreLoadedPacks()
    {
        gmodLocalVersionChecked = false; // gMod is now loaded; refresh the displayed version
        SyncExternalPacks(/*clear_missing=*/false);
        ApplyLoadOrder();
    }

    void ShutdownGMod()
    {
        // Only tear down a gMod we loaded ourselves. One pre-loaded by gwlauncher /
        // daybreak (or a d3d9 proxy) is left running with its packs intact - whoever
        // injected it owns its shutdown; we just drop our references to it below.
        if (gmodReady && gmodLoadedByToolbox) {
            // Unload synchronously: on teardown the worker threads are stopping, so a
            // queued ApplyLoadOrder might never run. Holding apply_mutex waits out any
            // in-flight worker reconcile; bumping the generation makes queued/running
            // ones skip; then we remove everything here, last, so nothing reloads after.
            std::lock_guard lk(apply_mutex);
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

        // Free only our own dll. gMod's DllMain(DLL_PROCESS_DETACH) reverts all its D3D9
        // vtable hooks (RemoveAllD3D9Hooks), so this is its clean shutdown path - but a
        // pre-loaded gMod must keep running, so we never free a handle we didn't load.
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
            // First pack configured but gMod isn't loaded yet: fetch/download it now.
            // The pack stays flagged loaded and is applied once gMod becomes ready.
            statusMessage = "Preparing gMod for: " + filename;
            CheckAndUpdateGmod();
            return true;
        }
        // The actual load runs on a worker thread; the result (and any failure)
        // is reported when it reconciles. Show an optimistic status meanwhile.
        statusMessage = "Loading: " + filename;
        ApplyLoadOrder();
        return true;
    }



    // =========================================================================
    // gMod version check / download
    // =========================================================================

    // Read a DLL's file-version resource as "major.minor.patch.build". Empty if the
    // file has no version info; gMod.dll's matches its GitHub release tag.
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

    // Full path of the gMod.dll currently in use: the live module if gMod is active
    // (which may be a pre-loaded one), otherwise the managed copy we would load.
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

    // Compare dotted version strings ("1.8.0.2"): >0 if a is newer, <0 if older, 0 if
    // equal. Missing trailing components count as 0, so "1.8" == "1.8.0.0".
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

    // Load the managed gMod.dll after it has changed on disk. The caller unloads any
    // active copy first, so we just clear the load-attempt latch and init afresh.
    void ReloadGmod()
    {
        gmodLocalVersionChecked = false;
        gmodLoadAttempted = false;
        InitGMod();
    }

    // Is a usable gMod already present in the process that we did not load (gwlauncher,
    // daybreak, or a d3d9 proxy)? Distinguished from the system d3d9.dll by its exports.
    bool ForeignGmodLoaded()
    {
        if (gmodReady) return !gmodLoadedByToolbox;
        for (const wchar_t* name : {L"gMod.dll", L"d3d9.dll"}) {
            HMODULE h = GetModuleHandleW(name);
            if (h && h != gmodDll && GetProcAddress(h, "AddFile") && GetProcAddress(h, "RemoveFile") && GetProcAddress(h, "GetFiles") && GetProcAddress(h, "SetDevice")) return true;
        }
        return false;
    }

    // Fetch the latest gMod release and, if it is newer than the copy in use (or none
    // exists), download it into the GWToolbox folder and load it. A gMod we don't own
    // (pre-loaded by a launcher) is only checked and reported on, never replaced. All
    // network/disk work runs on a worker thread; state changes post back to the main one.
    void CheckAndUpdateGmod()
    {
        if (gmodUpdateStep != GmodUpdateStep::Idle) return;
        gmodUpdateStep = GmodUpdateStep::Checking;
        gmodUpdateStatus.clear();

        // We may replace gMod only when toolbox manages the copy. A pre-loaded gMod -
        // whether already active or merely injected into the process - is checked, not
        // touched, so users on gwlauncher/daybreak never get a redundant download.
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
                // A newer gMod exists, but the active one was injected by a launcher and
                // is not ours to replace. Surface the version (the row shows it as out of
                // date) and leave updating to whoever loaded it.
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

            // Download to a temp file: our gMod.dll may still be loaded (and thus locked),
            // so we can't write the real file yet. We swap it in on the main thread below.
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
                // Unload our copy so the file is no longer locked, then rename the temp
                // download over the original and load the new version. ShutdownGMod clears
                // the loaded flags, so remember the enabled packs to restore them after.
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
        // The file dialog hands back the path as UTF-8. Building a filesystem::path
        // straight from a char* would decode it in the ANSI codepage instead, mangling
        // non-ASCII names (e.g. a "µ" in the filename) into a path that doesn't exist.
        // Decode the UTF-8 to a wide string first so the full Unicode path survives.
        const std::filesystem::path p = TextUtils::StringToWString(path);
        if (!std::filesystem::exists(p)) return;
        LoadTexturePack(p);
    }

    void DrawStatusBar()
    {
        if (gmodReady) {
            ImGui::TextColored({0.4f, 1.0f, 0.4f, 1.0f}, ICON_FA_CHECK " gMod active");
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

    // gMod.dll status: gMod is downloaded and kept up to date automatically; show the
    // installed version against the latest release and offer a manual re-check.
    void DrawGmodSource()
    {
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
    //
    // Hook D3D9 CreateTexture to gather every texture the game makes, keyed by
    // gMod/uMod/Texmod hash, for export as DDS/PNG. Capturing is opt-in (`recording`)
    // because hashing every created texture costs framerate. The cheap Release hook,
    // once installed, stays in place so the map never keeps a freed texture pointer.
    // =========================================================================

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

        // Hash the texture from the *previous* call (deferred so the game has had a
        // chance to fill it). Our reference (taken below) keeps it alive across the
        // gap, so it can't be freed before we hash it.
        if (auto tex = LastCreatedTexture) {
            LastCreatedTexture = nullptr; // clear before Release so the re-entrant release hook is a no-op
            const auto hash = Resources::GetTexmodHash(tex);
            if (hash && !dx9_textures_created_by_hash.contains(hash)) {
                dx9_textures_created_by_hash[hash] = tex;
            }
            tex->Release(); // drop our deferred-hash reference
        }

        // Only track managed/system-mem content textures (lockable, survive a device
        // reset). Skip D3DPOOL_DEFAULT (render targets/dynamic): unsafe to lock,
        // recreated on reset, and a held reference would block that reset.
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

    // Toggle the CreateTexture capture hook to match `record`. The Release hook is
    // installed alongside it on first start and left in place after stopping.
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
                // We need any texture to get its vtable
                // Create a temporary dummy texture to get the vtable
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

    // Unload gMod once nothing needs it (requested when the last pack was removed).
    if (gmodUnloadRequested) {
        gmodUnloadRequested = false;
        ShutdownGMod();
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

void TexmodModule::LoadSettings(ToolboxIni* ini)
{
    ToolboxModule::LoadSettings(ini);
    const int count = ini->GetLongValue(INI_SECTION, INI_PACK_COUNT, 0);
    packs.clear();
    for (int i = 0; i < count; i++) {
        const std::string key = std::string(INI_PACK_PATH) + std::to_string(i);
        const char* val = ini->GetValue(INI_SECTION, key.c_str(), nullptr);
        if (!val || !*val) continue;
        // Stored as UTF-8 (see SaveSettings); decode to a wide path so non-ASCII names
        // survive. Falls back to the ANSI codepage for any pre-existing legacy value.
        std::filesystem::path p = TextUtils::StringToWString(val);
        const std::string enabled_key = std::string(INI_PACK_ENABLED) + std::to_string(i);
        const bool enabled = ini->GetBoolValue(INI_SECTION, enabled_key.c_str(), false);
        packs.push_back({p, PathToUtf8(p.stem()), /*loaded=*/enabled});
    }
    // gMod is not ready yet (no device); the restored enabled flags are pushed to
    // it later by RestoreLoadedPacks(). This call is a no-op until then.
    SyncExternalPacks();

    // Only fetch gMod for users who actually use texture packs: if any are configured,
    // check GitHub and download a newer gMod automatically. Loading (Update ->
    // InitGMod) then picks up whatever copy is in the GWToolbox folder. Users with no
    // packs get the download lazily the moment they add their first one.
    if (!packs.empty()) CheckAndUpdateGmod();
}

void TexmodModule::SaveSettings(ToolboxIni* ini)
{
    ToolboxModule::SaveSettings(ini);
    ini->SetLongValue(INI_SECTION, INI_PACK_COUNT, static_cast<long>(packs.size()));
    for (size_t i = 0; i < packs.size(); i++) {
        const std::string key = std::string(INI_PACK_PATH) + std::to_string(i);
        ini->SetValue(INI_SECTION, key.c_str(), PathToUtf8(packs[i].path).c_str());
        const std::string enabled_key = std::string(INI_PACK_ENABLED) + std::to_string(i);
        ini->SetBoolValue(INI_SECTION, enabled_key.c_str(), packs[i].loaded);
    }
}
