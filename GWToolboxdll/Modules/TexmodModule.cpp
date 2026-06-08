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
#include <fstream>
#include <imgui.h>
#include <map>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <vector>

#include <GWCA/Managers/RenderMgr.h>
#include <GWCA/Utilities/Hooker.h>

#include <GWToolbox.h>
#include <ImGuiAddons.h>
#include <Modules/Resources.h>
#include <Utils/FontLoader.h>

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

    // Mandatory gMod exports. GetFiles is required because reconciliation reads the
    // current load order from it.
    constexpr const char* kRequiredExports[] = {"AddFile", "RemoveFile", "GetFiles", "SetDevice"};

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
        while (prefix < current.size() && prefix < desired.size()
               && current[prefix] == desired[prefix]) {
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
    // etc.) without disturbing the enabled flags restored from settings, then push
    // the combined set so the packs the user had enabled are loaded again.
    void RestoreLoadedPacks()
    {
        SyncExternalPacks(/*clear_missing=*/false);
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
                for (const auto& path : QueryGmodFiles()) pfnRemoveFile(path.wstring().c_str());
            }
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
        constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove
            | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize
            | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
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
            if (ImGui::ConfirmButton(ICON_FA_CIRCLE " Record textures", &confirm_record,
                                     "Recording captures every texture the game creates as you play.\n"
                                     "Doing this continuously is expensive and will lower your framerate\n"
                                     "while it is active.\n\nStart recording textures?")) {
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
    ImGui::Separator();
    DrawTextureGallery();
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
