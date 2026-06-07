#include "TexmodModule.h"
#include "stdafx.h"

#include <ToolboxIni.h>
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <d3d9.h>
#include <filesystem>
#include <fstream>
#include <imgui.h>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <vector>

#include <GWCA/Managers/RenderMgr.h>

#include <GWToolbox.h>
#include <ImGuiAddons.h>
#include <Modules/Resources.h>

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

    std::filesystem::path gmodDllLocation;

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

    constexpr const char* INI_SECTION = "TexmodModule";
    constexpr const char* INI_PACK_COUNT = "PackCount";
    constexpr const char* INI_PACK_PATH = "Pack";
    constexpr const char* INI_PACK_ENABLED = "PackEnabled";

    // =========================================================================
    // gMod TextureClient helpers
    // =========================================================================

    bool ResolveTextureClientFunctions()
    {
        pfnAddFile = reinterpret_cast<tGModAddFile>(GetProcAddress(gmodDll, "AddFile"));
        pfnRemoveFile = reinterpret_cast<tGModRemoveFile>(GetProcAddress(gmodDll, "RemoveFile"));
        pfnGetFiles = reinterpret_cast<tGModGetFiles>(GetProcAddress(gmodDll, "GetFiles"));
        pfnSetDevice = reinterpret_cast<tGModSetDevice>(GetProcAddress(gmodDll, "SetDevice"));
        // GetFiles is optional: older gMod builds lack it, and we only use it to
        // discover packs gMod loaded on its own (SyncExternalPacks no-ops without it).
        if (!pfnAddFile || !pfnRemoveFile || !pfnSetDevice) {
            statusMessage = "Error: gMod.dll is missing AddFile/RemoveFile/SetDevice exports. "
                            "Please use a gMod build that includes these functions.";
            return false;
        }
        return true;
    }

    // Read gMod's currently-loaded files (best-effort; empty if GetFiles is absent
    // or errors). Note: stock gMod keeps loaded_files in a std::map keyed by path,
    // so these come back sorted by path - not in load order - and the result is
    // only reliable as a set, not as a priority order.
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
            packs.push_back({p, p.stem().string(), /*loaded=*/true, /*external=*/true});
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

    // Translate an image RVA to a raw file offset via the section table (0 = not found).
    uint32_t RvaToFileOffset(const IMAGE_SECTION_HEADER* sec, unsigned n, uint32_t rva)
    {
        for (unsigned i = 0; i < n; ++i) {
            if (rva >= sec[i].VirtualAddress && rva < sec[i].VirtualAddress + sec[i].SizeOfRawData) return sec[i].PointerToRawData + (rva - sec[i].VirtualAddress);
        }
        return 0;
    }

    // Read the exported function names from a PE file on disk. Empty on any error
    // or if the file is not a 32-bit PE (gMod and Guild Wars are always x86).
    std::vector<std::string> ReadDllExportNames(const std::filesystem::path& path)
    {
        std::vector<std::string> names;
        std::error_code ec;
        const auto size = std::filesystem::file_size(path, ec);
        if (ec || size < sizeof(IMAGE_DOS_HEADER)) return names;

        std::vector<uint8_t> img(static_cast<size_t>(size));
        {
            std::ifstream f(path, std::ios::binary);
            if (!f.read(reinterpret_cast<char*>(img.data()), static_cast<std::streamsize>(img.size()))) return names;
        }

        // Every read below is bounds-checked: the file is untrusted.
        const auto ok = [&](size_t off, size_t len) {
            return off <= img.size() && len <= img.size() - off;
        };

        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(img.data());
        if (dos->e_magic != IMAGE_DOS_SIGNATURE) return names;
        const size_t nt = static_cast<size_t>(dos->e_lfanew);
        if (!ok(nt, sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER))) return names;
        if (*reinterpret_cast<const DWORD*>(img.data() + nt) != IMAGE_NT_SIGNATURE) return names;

        const auto* fh = reinterpret_cast<const IMAGE_FILE_HEADER*>(img.data() + nt + sizeof(DWORD));
        const size_t opt = nt + sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER);
        if (!ok(opt, sizeof(WORD))) return names;
        if (*reinterpret_cast<const WORD*>(img.data() + opt) != IMAGE_NT_OPTIONAL_HDR32_MAGIC) return names;
        if (!ok(opt, sizeof(IMAGE_OPTIONAL_HEADER32))) return names;

        const auto* oh = reinterpret_cast<const IMAGE_OPTIONAL_HEADER32*>(img.data() + opt);
        if (oh->NumberOfRvaAndSizes <= static_cast<DWORD>(IMAGE_DIRECTORY_ENTRY_EXPORT)) return names;
        const DWORD expRva = oh->DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
        if (!expRva) return names; // no export directory

        const size_t secOff = opt + fh->SizeOfOptionalHeader;
        const unsigned nSec = fh->NumberOfSections;
        if (!ok(secOff, static_cast<size_t>(nSec) * sizeof(IMAGE_SECTION_HEADER))) return names;
        const auto* sec = reinterpret_cast<const IMAGE_SECTION_HEADER*>(img.data() + secOff);

        const uint32_t expOff = RvaToFileOffset(sec, nSec, expRva);
        if (!ok(expOff, sizeof(IMAGE_EXPORT_DIRECTORY))) return names;
        const auto* exp = reinterpret_cast<const IMAGE_EXPORT_DIRECTORY*>(img.data() + expOff);

        const uint32_t namesOff = RvaToFileOffset(sec, nSec, exp->AddressOfNames);
        if (!ok(namesOff, static_cast<size_t>(exp->NumberOfNames) * sizeof(DWORD))) return names;
        const auto* nameRvas = reinterpret_cast<const DWORD*>(img.data() + namesOff);

        names.reserve(exp->NumberOfNames);
        for (DWORD i = 0; i < exp->NumberOfNames; ++i) {
            const uint32_t so = RvaToFileOffset(sec, nSec, nameRvas[i]);
            if (!so || so >= img.size()) continue;
            const char* s = reinterpret_cast<const char*>(img.data() + so);
            const size_t len = strnlen(s, img.size() - so);
            if (len < img.size() - so) names.emplace_back(s, len); // skip unterminated
        }
        return names;
    }

    // Mandatory gMod exports. GetFiles is intentionally absent: older gMod builds
    // lack it and ResolveTextureClientFunctions already treats it as optional.
    constexpr const char* kRequiredExports[] = {"AddFile", "RemoveFile", "SetDevice"};

    std::filesystem::path DirOfModule(HMODULE h)
    {
        wchar_t buf[MAX_PATH] = {};
        if (!GetModuleFileNameW(h, buf, MAX_PATH)) return {};
        return std::filesystem::path(buf).parent_path();
    }

    std::filesystem::path FindGmodDll()
    {
        // Explicit user override (Browse...) wins if it still exists.
        if (!gmodDllLocation.empty() && std::filesystem::exists(gmodDllLocation)) return gmodDllLocation;

        // Search next to the running exe (Gw.exe) and next to GWToolbox.dll.
        std::vector<std::filesystem::path> dirs;
        const auto add_dir = [&](std::filesystem::path d) {
            if (!d.empty() && std::ranges::find(dirs, d) == dirs.end()) dirs.push_back(std::move(d));
        };
        add_dir(DirOfModule(nullptr));                   // running exe
        add_dir(DirOfModule(GWToolbox::GetDLLModule())); // toolbox dll

        std::filesystem::path found;

        // Prefer a dedicated gMod.dll over a d3d9.dll proxy; sniff exports off
        // disk so an incompatible file (e.g. the system d3d9.dll) is never loaded.
        for (const auto& dir : dirs) {
            for (const wchar_t* name : {L"gMod.dll", L"d3d9.dll"}) {
                std::error_code ec;
                auto candidate = dir / name;
                if (!std::filesystem::exists(candidate, ec)) continue;
                const auto names = ReadDllExportNames(candidate);
                if (std::ranges::all_of(kRequiredExports, [&](const char* req) {
                        return std::ranges::find(names, req) != names.end();
                    })) {
                    found = candidate;
                    break;
                }
            }
        }
        return found;
    }

    bool InitGMod()
    {
        if (gmodReady) return true;

        auto GuildWars_IDirect3DDevice9_Instance = GW::Render::GetDevice();
        if (!GuildWars_IDirect3DDevice9_Instance) {
            statusMessage = "Error: Could not get IDirect3DDevice9 from GW::Render::GetDevice().";
            return false;
        }

        // 1. Check whether a compatible module is already loaded in this process
        //    (a dedicated gMod.dll, or a d3d9.dll proxy). GetProcAddress here loads
        //    nothing new. A loaded module that lacks the exports (e.g. the system
        //    d3d9.dll) is simply skipped rather than treated as an error.
        for (const wchar_t* name : {L"gMod.dll", L"d3d9.dll"}) {
            HMODULE h = GetModuleHandleW(name);
            if (!h) continue;
            gmodDll = h;
            if (!ResolveTextureClientFunctions()) {
                gmodDll = nullptr;
                continue;
            }
            pfnSetDevice(GuildWars_IDirect3DDevice9_Instance);
            gmodReady = true;
            RestoreLoadedPacks();
            statusMessage = "gMod was already active (pre-loaded). Texture pack loading enabled.";
            return true;
        }

        if (gmodLoadAttempted) {
            // statusMessage = "Error: gMod initialization already attempted and failed. Please fix the issue and click Retry.";
            return false;
        }
        gmodLoadAttempted = true;
        // 2. Find and load gMod.dll.
        const auto found = FindGmodDll();
        if (found.empty()) {
            statusMessage = "Error: Could not find gMod.dll. Place it next to GWToolbox.dll or use Browse.";
            return false;
        }

        gmodDll = LoadLibraryW(found.wstring().c_str());
        if (!gmodDll) {
            statusMessage = "Error: Could not load gMod.dll. Make sure it is next to GWToolbox.dll.";
            return false;
        }

        // 3. Resolve TextureClient shim exports.
        if (!ResolveTextureClientFunctions()) {
            FreeLibrary(gmodDll);
            gmodDll = nullptr;
            return false;
        }
        pfnSetDevice(GuildWars_IDirect3DDevice9_Instance);
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

    // What we have told gMod to load, in add order (== priority). Stock gMod can't
    // report its own load order (GetFiles is a path-sorted std::map), so we track
    // it ourselves to compute a minimal add/remove diff. Only touched under apply_mutex.
    std::vector<std::filesystem::path> applied_order;

    // Build the desired load order: every pack flagged loaded, in list order.
    std::vector<std::filesystem::path> BuildDesiredList()
    {
        std::vector<std::filesystem::path> desired;
        for (const auto& pack : packs) {
            if (pack.loaded) desired.push_back(pack.path);
        }
        return desired;
    }

    // Reconcile gMod's loaded set to `desired` with AddFile/RemoveFile. gMod gives
    // the first-loaded pack priority on a texture conflict, so to honour the
    // requested order we keep the longest matching prefix, remove everything after
    // it, and re-add the desired tail in order. Returns the last gMod error (or 0).
    // Caller must hold apply_mutex.
    int ReconcileLocked(const std::vector<std::filesystem::path>& desired)
    {
        size_t prefix = 0;
        while (prefix < applied_order.size() && prefix < desired.size()
               && applied_order[prefix] == desired[prefix]) {
            ++prefix;
        }
        // Remove the diverging tail, deepest first, leaving the kept prefix intact.
        for (size_t i = applied_order.size(); i-- > prefix;) {
            if (pfnRemoveFile) pfnRemoveFile(applied_order[i].wstring().c_str());
        }
        // Re-add the desired tail in order; earlier adds win conflicts.
        int error = GMOD_RETURN_OK;
        for (size_t i = prefix; i < desired.size(); ++i) {
            const int ret = pfnAddFile ? pfnAddFile(desired[i].wstring().c_str()) : GMOD_RETURN_OK;
            if (ret < 0 && ret != GMOD_RETURN_EXISTS) error = ret;
        }
        applied_order = desired;
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
                if (error < 0)
                    statusMessage = "gMod failed to load a pack (error " + std::to_string(error) + ").";
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
        for (auto& pack : packs) pack.loaded = false;
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
    // etc.) without disturbing the enabled flags restored from settings, seed
    // applied_order with gMod's current set so the first reconcile is minimal,
    // then push the combined set so the packs the user had enabled are loaded.
    void RestoreLoadedPacks()
    {
        SyncExternalPacks(/*clear_missing=*/false);
        {
            std::lock_guard lk(apply_mutex);
            applied_order = QueryGmodFiles();
        }
        ApplyLoadOrder();
    }

    void ShutdownGMod()
    {
        if (!gmodReady) return;

        // Unload synchronously: on teardown the worker threads are stopping, so a
        // queued ApplyLoadOrder might never run. Holding apply_mutex waits out any
        // in-flight worker reconcile; bumping the generation makes queued/running
        // ones skip; then we remove everything here, last, so nothing reloads after.
        {
            std::lock_guard lk(apply_mutex);
            ++apply_generation;
            for (auto& pack : packs) pack.loaded = false;
            if (pfnRemoveFile) {
                for (const auto& path : applied_order) pfnRemoveFile(path.wstring().c_str());
            }
            applied_order.clear();
        }

        gmodReady = false;
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
                packs.push_back({path, path.stem().string(), false, false});
                return &packs.back();
            }
            return nullptr;
        }
        return &(*it);
    }

    bool LoadTexturePack(const std::filesystem::path& path)
    {
        if (!gmodReady) {
            statusMessage = "gMod is not initialised.";
            return false;
        }
        if (!std::filesystem::exists(path)) {
            statusMessage = "File not found: " + path.string();
            return false;
        }
        auto pack = FindPack(path, true);
        const auto filename = pack->path.filename().string();
        pack->loaded = true;
        // The actual load runs on a worker thread; the result (and any failure)
        // is reported when it reconciles. Show an optimistic status meanwhile.
        statusMessage = "Loading: " + filename;
        ApplyLoadOrder();
        return true;
    }



    // =========================================================================
    // UI helpers
    // =========================================================================

    void OnTexmodFileChosen(const char* path)
    {
        if (!path) return;
        std::filesystem::path p = path;
        if (!std::filesystem::exists(p)) return;
        LoadTexturePack(p);
    }

    void OnGmodDllFileChosen(const char* path)
    {
        if (!path) return;
        std::filesystem::path p = path;
        if (!std::filesystem::exists(p)) return;
        gmodDllLocation = p;
        statusMessage = "gMod DLL set to: " + gmodDllLocation.string();
        gmodLoadAttempted = false;
        InitGMod();
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
            ImGui::SameLine();
            if (ImGui::SmallButton("Browse")) Resources::OpenFileDialog(OnGmodDllFileChosen, "dll", Resources::GetSettingsFolderPath().string().c_str());
        }
        if (!statusMessage.empty()) {
            ImGui::SameLine();
            ImGui::TextDisabled("  %s", statusMessage.c_str());
        }
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
        const float actions_width = btn_width(ICON_FA_ARROW_UP) + btn_width(ICON_FA_ARROW_DOWN)
            + btn_width(ICON_FA_TRASH) + style.ItemSpacing.x * 2.f;
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
            const std::string pathStr = pack.path.string();
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

} // namespace

void TexmodModule::Update(float)
{
    if (gmodReady) return;
    InitGMod();
}

void TexmodModule::Terminate()
{
    ShutdownGMod(); // synchronously unloads every pack from gMod
    ToolboxModule::Terminate();
}

void TexmodModule::DrawSettingsInternal()
{
    DrawStatusBar();
    ImGui::Separator();
    DrawPackList();
    ImGui::Separator();
    if (ImGui::Button("Add texture pack")) Resources::OpenFileDialog(OnTexmodFileChosen, "tpf,zip,dds", Resources::GetSettingsFolderPath().string().c_str());
    ImGui::TextDisabled("Supported: .tpf, .zip (texmod format), .dds");
}

void TexmodModule::LoadSettings(ToolboxIni* ini)
{
    ToolboxModule::LoadSettings(ini);
    gmodDllLocation = ini->GetValue(INI_SECTION, "gmod_dll_location", "");
    const int count = ini->GetLongValue(INI_SECTION, INI_PACK_COUNT, 0);
    packs.clear();
    for (int i = 0; i < count; i++) {
        const std::string key = std::string(INI_PACK_PATH) + std::to_string(i);
        const char* val = ini->GetValue(INI_SECTION, key.c_str(), nullptr);
        if (!val || !*val) continue;
        std::filesystem::path p = val;
        const std::string enabled_key = std::string(INI_PACK_ENABLED) + std::to_string(i);
        const bool enabled = ini->GetBoolValue(INI_SECTION, enabled_key.c_str(), false);
        packs.push_back({p, p.stem().string(), /*loaded=*/enabled});
    }
    // gMod is not ready yet (no device); the restored enabled flags are pushed to
    // it later by RestoreLoadedPacks(). This call is a no-op until then.
    SyncExternalPacks();
}

void TexmodModule::SaveSettings(ToolboxIni* ini)
{
    ToolboxModule::SaveSettings(ini);
    ini->SetValue(INI_SECTION, "gmod_dll_location", gmodDllLocation.string().c_str());
    ini->SetLongValue(INI_SECTION, INI_PACK_COUNT, static_cast<long>(packs.size()));
    for (size_t i = 0; i < packs.size(); i++) {
        const std::string key = std::string(INI_PACK_PATH) + std::to_string(i);
        ini->SetValue(INI_SECTION, key.c_str(), packs[i].path.string().c_str());
        const std::string enabled_key = std::string(INI_PACK_ENABLED) + std::to_string(i);
        ini->SetBoolValue(INI_SECTION, enabled_key.c_str(), packs[i].loaded);
    }
}
