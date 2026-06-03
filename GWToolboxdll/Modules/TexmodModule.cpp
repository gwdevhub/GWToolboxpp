#include "TexmodModule.h"
#include "stdafx.h"

#include <ToolboxIni.h>
#include <d3d9.h>
#include <filesystem>
#include <imgui.h>
#include <string>
#include <vector>

#include <GWCA/Managers/RenderMgr.h>
#include <GWCA/Utilities/Hooker.h>

#include <GWCA/Utilities/Scanner.h>
#include <ImGuiAddons.h>
#include <Modules/Resources.h>

namespace {

    HMODULE FindLoadedModuleByName(const char* name)
    {
        HMODULE hModules[1024];
        HANDLE hProcess;
        DWORD cbNeeded;
        unsigned int i;

        hProcess = GetCurrentProcess();
        if (!EnumProcessModules(hProcess, hModules, sizeof(hModules), &cbNeeded)) return nullptr;
        for (i = 0; i < (cbNeeded / sizeof(HMODULE)); i++) {
            TCHAR szModuleName[MAX_PATH];
            ASSERT(GetModuleFileName(hModules[i], szModuleName, _countof(szModuleName)) > 0);
            const auto basename = strrchr(szModuleName, '\\');
            if (basename && _stricmp(basename + 1, name) == 0) return hModules[i];
        }
        return nullptr;
    }

    // =========================================================================
    // Types
    // =========================================================================

    using tDirect3DCreate9 = IDirect3D9*(APIENTRY*)(UINT);
    using tGModAddFile = int (*)(const wchar_t*);
    using tGModRemoveFile = int (*)(const wchar_t*);
    using tGModSetDevice = int (*)(IDirect3DDevice9*);
    using tGModGetFiles = int (*)(wchar_t* buf, int bufLen);
    using tCreateTexture = HRESULT(__stdcall*)(IDirect3DDevice9*, UINT, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL, IDirect3DTexture9**, HANDLE*);
    using tCreateVolumeTexture = HRESULT(__stdcall*)(IDirect3DDevice9*, UINT, UINT, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL, IDirect3DVolumeTexture9**, HANDLE*);
    using tCreateCubeTexture = HRESULT(__stdcall*)(IDirect3DDevice9*, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL, IDirect3DCubeTexture9**, HANDLE*);

    tGModAddFile pfnAddFile = nullptr;
    tGModRemoveFile pfnRemoveFile = nullptr;
    tGModGetFiles pfnGetFiles = nullptr;
    tGModSetDevice pfnSetDevice = nullptr;

    std::filesystem::path gmodDllLocation;

    struct TexturePackEntry {
        std::filesystem::path path;
        std::string label;
        bool loaded = false;
        bool external = false;
    };

    struct uMod_TextureClient {
        uint32_t OriginalTextures[4];       // std::vector<uMod_IDirect3DTexture9*> - 16 bytes debug
        uint32_t OriginalVolumeTextures[4]; // std::vector<uMod_IDirect3DVolumeTexture9*>
        uint32_t OriginalCubeTextures[4];   // std::vector<uMod_IDirect3DCubeTexture9*>
        IDirect3DDevice9* D3D9Device;
        uint32_t isDirectXExDevice;
        uint32_t should_update;
        HANDLE hMutex;
        // Don't care about the rest
    };
    static_assert(sizeof(uMod_TextureClient) == 0x40);

    struct uMod_IDirect3D9 {
        void** vtable;
        IDirect3D9* m_pIDirect3D9;
    };

    struct uMod_IDirect3DTexture9 {
        void** vtable;
        IDirect3DTexture9* m_D3Dtex;
        uMod_IDirect3DTexture9* CrossRef_D3Dtex;
        IDirect3DDevice9* m_D3Ddev;
    };
    static_assert(sizeof(uMod_IDirect3DTexture9) == 0x10);

    struct uMod_IDirect3DDevice9 {
        void** vtable;
        IDirect3DDevice9* m_pIDirect3DDevice9;
        int BackBufferCount;
        uint32_t NormalRendering;
        int uMod_Reference;
        uMod_IDirect3DTexture9* LastCreatedTexture;
        void* LastCreatedVolumeTexture;
        void* LastCreatedCubeTexture;
        uMod_TextureClient* uMod_Client;
    };
    static_assert(sizeof(uMod_IDirect3DDevice9) == 0x24);

    // =========================================================================
    // Module state
    // =========================================================================

    HMODULE gmodDll = nullptr;

    bool gmodPreloaded = false;

    std::vector<TexturePackEntry> packs;

    char pathInputBuf[MAX_PATH] = {};
    bool gmodReady = false;
    std::string statusMessage;
    bool gmodLoadAttempted = false;

    constexpr const char* INI_SECTION = "TexmodModule";
    constexpr const char* INI_PACK_COUNT = "PackCount";
    constexpr const char* INI_PACK_PATH = "Pack";

    // =========================================================================
    // gMod TextureClient helpers
    // =========================================================================

    bool ResolveTextureClientFunctions()
    {
        pfnAddFile = reinterpret_cast<tGModAddFile>(GetProcAddress(gmodDll, "AddFile"));
        pfnRemoveFile = reinterpret_cast<tGModRemoveFile>(GetProcAddress(gmodDll, "RemoveFile"));
        pfnGetFiles = reinterpret_cast<tGModGetFiles>(GetProcAddress(gmodDll, "GetFiles"));
        pfnSetDevice = reinterpret_cast<tGModSetDevice>(GetProcAddress(gmodDll, "SetDevice"));
        if (!pfnAddFile || !pfnRemoveFile || !pfnSetDevice) {
            statusMessage = "Error: gMod.dll is missing AddFile/RemoveFile/SetDevice exports. "
                            "Please use a gMod build that includes these functions.";
            return false;
        }
        return true;
    }

    void SyncExternalPacks()
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
        for (auto& pack : packs) {
            if (std::ranges::find(packs_loaded, pack.path) == packs_loaded.end()) pack.loaded = false;
        }
    }

    // =========================================================================
    // gMod lifecycle
    // =========================================================================

    std::filesystem::path FindGmodDll()
    {
        if (!gmodDllLocation.empty() && std::filesystem::exists(gmodDllLocation)) return gmodDllLocation;

        wchar_t buf[MAX_PATH] = {};
        GetModuleFileNameW(GetModuleHandleW(L"GWToolbox.dll"), buf, MAX_PATH);
        std::filesystem::path candidate(buf);
        candidate.replace_filename(L"gMod.dll");
        if (exists(candidate)) return candidate;

        HMODULE h = GetModuleHandleW(L"gMod.dll");
        if (h) {
            wchar_t pathBuf[MAX_PATH] = {};
            GetModuleFileNameW(h, pathBuf, MAX_PATH);
            return std::filesystem::path(pathBuf);
        }
        return {};
    }

    bool InitGMod()
    {
        if (gmodReady) return true;

        auto GuildWars_IDirect3DDevice9_Instance = GW::Render::GetDevice();
        if (!GuildWars_IDirect3DDevice9_Instance) {
            statusMessage = "Error: Could not get IDirect3DDevice9 from GW::Render::GetDevice().";
            return false;
        }

        // 1. Check whether gMod is already loaded in this process.
        gmodDll = GetModuleHandleW(L"gMod.dll");
        if (gmodDll) {
            gmodPreloaded = true;
            if (!ResolveTextureClientFunctions()) {
                gmodPreloaded = false;
                gmodDll = nullptr;
                statusMessage = "Error: gMod was already active (pre-loaded), but AddFile/RemoveFile were not found - old version?";
                return false;
            }
            pfnSetDevice(GuildWars_IDirect3DDevice9_Instance);
            gmodReady = true;
            SyncExternalPacks();
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
        SyncExternalPacks();
        return true;
    }
    TexturePackEntry* FindPack(const std::filesystem::path& path, bool create_if_not_found = false);
    bool UnloadTexturePack(const std::filesystem::path& path, bool sync = true)
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
        if (!(pfnRemoveFile && (pfnRemoveFile(pack->path.wstring().c_str()), true))) {
            statusMessage = "gMod failed to unload: " + pack->path.filename().string();
            return false;
        }
        if (sync) SyncExternalPacks();
        return std::ranges::find(packs, pack->path, &TexturePackEntry::path) != packs.end();
    }

    void UnloadAllTexturePacks()
    {
        for (auto& pack : packs)
            UnloadTexturePack(pack.path, false);
        SyncExternalPacks();
    }

    void ShutdownGMod()
    {
        if (!gmodReady) return;

        UnloadAllTexturePacks();

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

    bool LoadTexturePack(const std::filesystem::path& path, bool sync = true)
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
        if (!(pfnAddFile && pfnAddFile(pack->path.wstring().c_str()) == 0)) {
            statusMessage = "gMod failed to load: " + path.filename().string();
            return false;
        }
        statusMessage = "Loaded: " + pack->path.filename().string();
        if (sync) SyncExternalPacks();
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
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, font_size.y * 2.f);
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Path", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, font_size.y * 2.f);
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
            ImGui::PushID(i * 100 + 1);
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
    UnloadAllTexturePacks();
    ShutdownGMod();
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
        packs.push_back({p, p.stem().string(), false});
    }
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
    }
}
