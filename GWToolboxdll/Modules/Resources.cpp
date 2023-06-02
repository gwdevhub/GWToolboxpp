#include "stdafx.h"

#include <DDSTextureLoader/DDSTextureLoader9.h>
#include <WICTextureLoader/WICTextureLoader9.h>

#include <GWCA/GameEntities/Map.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <EmbeddedResource.h>
#include <Defines.h>
#include <GWToolbox.h>
#include <RestClient.h>
#include <Logger.h>
#include <Path.h>
#include <Str.h>

#include <Utils/GuiUtils.h>
#include <GWCA/Constants/Constants.h>
#include <Modules/Resources.h>

#include <include/nfd.h>
#include <nfd_common.h>
#include <nfd_common.c>
#include <nfd_win.cpp>

namespace {
    const char* d3dErrorMessage(HRESULT code) {
        switch (code) {
        case D3DERR_NOTAVAILABLE:
            return  "D3DERR_NOTAVAILABLE";
        case D3DERR_OUTOFVIDEOMEMORY:
            return  "D3DERR_OUTOFVIDEOMEMORY";
        case D3DERR_INVALIDCALL:
            return  "D3DERR_INVALIDCALL";
        case E_OUTOFMEMORY:
            return  "E_OUTOFMEMORY";
        case D3D_OK:
            return "D3D_OK";
        default:
            static std::string str;
            str = std::format("Unknown D3D error {:#08x}", code);
            return str.c_str();
        }
    }

    const char* profession_icon_urls[] = {
        "",
        "8/87/Warrior-tango-icon-48",
        "e/e8/Ranger-tango-icon-48",
        "5/53/Monk-tango-icon-48",
        "b/b1/Necromancer-tango-icon-48",
        "b/b1/Mesmer-tango-icon-48",
        "4/47/Elementalist-tango-icon-48",
        "2/2b/Assassin-tango-icon-48",
        "5/5b/Ritualist-tango-icon-48",
        "5/5e/Paragon-tango-icon-48",
        "3/38/Dervish-tango-icon-48"
    };
    std::map<GW::Constants::SkillID, IDirect3DTexture9**> skill_images;
    std::map<std::wstring, IDirect3DTexture9**> item_images;
    std::map<std::string, IDirect3DTexture9**> guild_wars_wiki_images;
    std::map<uint32_t, IDirect3DTexture9**> profession_icons;
    std::map<GW::Constants::MapID, GuiUtils::EncString*> map_names;
    std::map<uint32_t, GuiUtils::EncString*> encoded_string_ids;
    std::string ns;
    constexpr size_t MAX_WORKERS = 5;
    const wchar_t* GUILD_WARS_WIKI_FILES_PATH = L"img\\gww_files";
    const wchar_t* ARMOR_GALLERY_PATH = L"img\\armor_gallery";
    const wchar_t* SKILL_IMAGES_PATH = L"img\\skills";
    const wchar_t* ITEM_IMAGES_PATH = L"img\\items";
    const wchar_t* PROF_ICONS_PATH = L"img\\professions";


    std::recursive_mutex worker_mutex;
    std::recursive_mutex main_mutex;
    std::recursive_mutex dx_mutex;

    // tasks to be done async by the worker thread
    std::queue<std::function<void()>> thread_jobs;
    // tasks to be done in the render thread
    std::queue<std::function<void(IDirect3DDevice9*)>> dx_jobs;
    // tasks to be done in main thread
    std::queue<std::function<void()>> main_jobs;

    bool should_stop = false;

    std::vector<std::thread*> workers;

    // snprintf error message, pass to callback as a failure. Used internally.
    void trigger_failure_callback(std::function<void(bool,const std::wstring&)> callback,const wchar_t* format, ...) {
        std::wstring out;
        va_list vl;
        va_start(vl, format);
        int written = StrVswprintf(out, format, vl);
        va_end(vl);
        ASSERT(written != -1);
        callback(false, out);
    };

    float cached_ui_scale = .0f;

    GW::HookEntry OnUIMessage_Hook;
    void OnUIMessage(GW::HookStatus*, GW::UI::UIMessage message_id, void* wparam, void*) {
        switch (message_id) {
        case GW::UI::UIMessage::kEnumPreference:
            if(wparam && *(GW::UI::EnumPreference*)wparam == GW::UI::EnumPreference::InterfaceSize)
                Resources::GetGWScaleMultiplier(true); // Re-fetch ui scale indicator
            break;
        }
    }

    void WorkerUpdate() {
        while (!should_stop) {
            worker_mutex.lock();
            if (thread_jobs.empty()) {
                worker_mutex.unlock();
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            else {
                std::function<void()> func = thread_jobs.front();
                thread_jobs.pop();
                worker_mutex.unlock();
                func();
            }
        }
    }

    void InitRestClient(RestClient* r) {
        char user_agent_str[32];
        ASSERT(snprintf(user_agent_str, sizeof(user_agent_str), "GWToolboxpp/%s", GWTOOLBOXDLL_VERSION) != -1);
        r->SetUserAgent(user_agent_str);
        r->SetFollowLocation(true);
        r->SetVerifyPeer(false); // idc about mitm or out of date certs
        r->SetMethod(HttpMethod::Get);
        r->SetVerifyHost(false);
    }
}

Resources::Resources() {
    InitCurl();
}
Resources::~Resources() {
    Cleanup();
    ShutdownCurl();
    for (const auto& tex : skill_images | std::views::values) {
        delete tex;
    }
    skill_images.clear();
    for (const auto& tex : item_images | std::views::values) {
        delete tex;
    }
    item_images.clear();
    for (const auto& tex : map_names | std::views::values) {
        delete tex;
    }
    map_names.clear();

};

void Resources::EnqueueWorkerTask(std::function<void()> f) { worker_mutex.lock(); thread_jobs.push(f); worker_mutex.unlock();}
void Resources::EnqueueMainTask(std::function<void()> f) { main_mutex.lock(); main_jobs.push(f); main_mutex.unlock(); }
void Resources::EnqueueDxTask(std::function<void(IDirect3DDevice9*)> f) { dx_mutex.lock(); dx_jobs.push(f); dx_mutex.unlock(); }

void Resources::OpenFileDialog(std::function<void(const char*)> callback, const char* filterList, const char* defaultPath) {

    std::string* filterList_cpy = new std::string(filterList);
    std::string* defaultPath_cpy = new std::string(defaultPath);
    EnqueueWorkerTask([callback, filterList_cpy,defaultPath_cpy]() {
        nfdchar_t *outPath = NULL;
        nfdresult_t result = NFD_OpenDialog( filterList_cpy->c_str(), defaultPath_cpy->c_str(), &outPath);
        delete filterList_cpy;
        delete defaultPath_cpy;

        switch (result) {
        case NFD_OKAY:
        case NFD_CANCEL:
            break;
        default:
            Log::Log("NFD_OpenDialog Error: %s\n", NFD_GetError());
            break;
        }

        callback(outPath);
        if(outPath)
            free(outPath);
        });
}
void Resources::SaveFileDialog(std::function<void(const char*)> callback, const char* filterList, const char* defaultPath) {

    std::string* filterList_cpy = new std::string(filterList);
    std::string* defaultPath_cpy = new std::string(defaultPath);

    EnqueueWorkerTask([callback, filterList_cpy,defaultPath_cpy]() {
        nfdchar_t *outPath = NULL;
        nfdresult_t result = NFD_SaveDialog( filterList_cpy->c_str(), defaultPath_cpy->c_str(), &outPath);
        delete filterList_cpy;
        delete defaultPath_cpy;

        switch (result) {
        case NFD_OKAY:
            callback(outPath);
            break;
        case NFD_CANCEL:
            callback(NULL);
            break;
        default:
            Log::Log("NFD_OpenDialog Error: %s\n", NFD_GetError());
            break;
        }
        if(outPath)
            free(outPath);
        });
}
float Resources::GetGWScaleMultiplier(bool force) {
    if (force || cached_ui_scale == .0f) {
        const auto interfacesize =
            static_cast<GW::Constants::InterfaceSize>(GW::UI::GetPreference(GW::UI::EnumPreference::InterfaceSize));

        switch (interfacesize) {
        case GW::Constants::InterfaceSize::SMALL: cached_ui_scale = .9f; break;
        case GW::Constants::InterfaceSize::LARGE: cached_ui_scale = 1.166666f; break;
        case GW::Constants::InterfaceSize::LARGER: cached_ui_scale = 1.3333333f; break;
        default: cached_ui_scale = 1.f; break;
        }
    }
    return cached_ui_scale;
}

HRESULT Resources::ResolveShortcut(const std::filesystem::path& in_shortcut_path, std::filesystem::path& out_actual_path)
{
    HRESULT hRes = E_FAIL;
    if (in_shortcut_path.extension() != ".lnk") {
        out_actual_path = in_shortcut_path;
        return S_OK;
    }
    hRes = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (!SUCCEEDED(hRes))
        return hRes;
    IShellLink* psl = NULL;

    // buffer that receives the null-terminated string
    // for the drive and path
    TCHAR szPath[MAX_PATH];
    // buffer that receives the null-terminated
    // string for the description
    TCHAR szDesc[MAX_PATH];
    // structure that receives the information about the shortcut
    WIN32_FIND_DATA wfd{};

    // Get a pointer to the IShellLink interface
    hRes = CoCreateInstance(CLSID_ShellLink,
        NULL,
        CLSCTX_INPROC_SERVER,
        IID_IShellLink,
        (void**)&psl);
    if (!SUCCEEDED(hRes)) {
        CoUninitialize();
        return hRes;
    }
    // Get a pointer to the IPersistFile interface
    IPersistFile* ppf = NULL;
    psl->QueryInterface(IID_IPersistFile, reinterpret_cast<void**>(&ppf));

    // IPersistFile is using LPCOLESTR,
    // Open the shortcut file and initialize it from its contents
    hRes = ppf->Load(in_shortcut_path.wstring().c_str(), STGM_READ);
    if (!SUCCEEDED(hRes)) {
        CoUninitialize();
        return hRes;
    }
    // Try to find the target of a shortcut,
    // even if it has been moved or renamed
    hRes = psl->Resolve(NULL, SLR_UPDATE);
    if (!SUCCEEDED(hRes)) {
        CoUninitialize();
        return hRes;
    }
    // Get the path to the shortcut target
    hRes = psl->GetPath(szPath, MAX_PATH, &wfd, SLGP_RAWPATH);
    if (!SUCCEEDED(hRes)) {
        CoUninitialize();
        return hRes;
    }

    // Get the description of the target
    hRes = psl->GetDescription(szDesc, MAX_PATH);
    if (!SUCCEEDED(hRes)) {
        CoUninitialize();
        return hRes;
    }
    out_actual_path = szPath;
    CoUninitialize();
    return hRes;
}

void Resources::Initialize() {
    ToolboxModule::Initialize();
    for (size_t i = 0; i < MAX_WORKERS; i++) {
        workers.push_back(new std::thread([this]() {
            WorkerUpdate();
            }));
    }
    GW::UI::RegisterUIMessageCallback(&OnUIMessage_Hook, GW::UI::UIMessage::kEnumPreference, OnUIMessage, 0x8000);
}
void Resources::Cleanup() {
    should_stop = true;
    for (std::thread* worker : workers) {
        if (!worker)
            continue;
        ASSERT(worker->joinable());
        worker->join();
        delete worker;
    }
    workers.clear();
    for (const auto& tex : skill_images | std::views::values) {
        delete tex;
    }
    skill_images.clear();
    for (const auto& tex : item_images | std::views::values) {
        delete tex;
    }
    item_images.clear();
    for (const auto& img : guild_wars_wiki_images | std::views::values) {
        delete img;
    }
    guild_wars_wiki_images.clear();
    for (const auto& img : encoded_string_ids | std::views::values) {
        delete img;
    }
    encoded_string_ids.clear();
    map_names.clear(); // NB: Map names are pointers to encoded_string_ids
}
void Resources::Terminate() {
    ToolboxModule::Terminate();

    GW::UI::RemoveUIMessageCallback(&OnUIMessage_Hook);

    Cleanup();
}

void Resources::EndLoading() {
    EnqueueWorkerTask([this] { should_stop = true; });
}

std::filesystem::path Resources::GetSettingsFolderPath()
{
    std::filesystem::path computer_name;
    ASSERT(PathGetComputerName(computer_name));

    std::filesystem::path docpath;
    ASSERT(PathGetDocumentsPath(docpath, L"GWToolboxpp"));
    docpath = docpath / computer_name;

    ASSERT(PathCreateDirectorySafe(docpath));

    return docpath;
}
std::filesystem::path Resources::GetPath(const std::filesystem::path& file)
{
    return GetSettingsFolderPath() / file;
}
std::filesystem::path Resources::GetPath(const std::filesystem::path& folder, const std::filesystem::path& file)
{
    return GetSettingsFolderPath() / folder / file;
}

bool Resources::EnsureFolderExists(const std::filesystem::path& path)
{
    return exists(path) || create_directory(path);
}

utf8::string Resources::GetPathUtf8(const std::wstring& file) {
    const std::wstring path = GetPath(file);
    return Unicode16ToUtf8(path.c_str());
}

bool Resources::Download(const std::filesystem::path& path_to_file, const std::string& url, std::wstring& response) {
    if (exists(path_to_file)) {
        if (!std::filesystem::remove(path_to_file)) {
            return StrSwprintf(response, L"Failed to delete existing file %s, err %d", path_to_file.wstring().c_str(), GetLastError()), false;
        }
    }
    if (exists(path_to_file)) {
        return StrSwprintf(response, L"File already exists @ %s", path_to_file.wstring().c_str()), false;
    }

    std::string content;
    if (!Download(url, content)) {
        return StrSwprintf(response,L"%S", content.c_str()), false;
    }
    if (!content.length()) {
        return StrSwprintf(response,L"Failed to download %S, no content length", url.c_str()), false;
    }
    FILE* fp = fopen(path_to_file.string().c_str(), "wb");
    if (!fp) {
        return StrSwprintf(response,L"Failed to call fopen for %s, err %d", path_to_file.wstring().c_str(), GetLastError()), false;
    }
    const int written = fwrite(content.data(), content.size() + 1, 1, fp);
    fclose(fp);
    if(written != 1) {
        return StrSwprintf(response,L"Failed to call fwrite for %s, err %d", path_to_file.wstring().c_str(), GetLastError()), false;
    }
    return true;
}

void Resources::Download(const std::filesystem::path& path_to_file, const std::string& url, AsyncLoadCallback callback)
{
    EnqueueWorkerTask([this, path_to_file, url, callback] {
        std::wstring error_message;
        bool success = Download(path_to_file, url, error_message);
        // and call the callback in the main thread
        if (callback) {
            EnqueueMainTask([callback, success, error_message] {
                callback(success, error_message);
                });
        }
        else if (!success) {
            Log::LogW(L"Failed to download %s from %S\n%S", path_to_file.wstring().c_str(), url.c_str(), error_message.c_str());
        }

        });
}

bool Resources::Download(const std::string& url, std::string& response)
{
    RestClient r;
    InitRestClient(&r);
    r.SetUrl(url.c_str());
    r.Execute();
    if (!r.IsSuccessful()) {
        StrSprintf(response, "Failed to download %s, curl status %d %s", url.c_str(), r.GetStatusCode(), r.GetStatusStr());
        return false;
    }
    response = std::move(r.GetContent());
    return true;
}
void Resources::Download(const std::string& url, AsyncLoadMbCallback callback)
{
    EnqueueWorkerTask([this, url, callback] {
        std::string response;
        bool ok = Download(url, response);
        EnqueueMainTask([callback, ok, response]() {
            callback(ok, response);
        });
    });
}

bool Resources::Post(const std::string& url, const std::string& payload, std::string& response)
{
    RestClient r;
    InitRestClient(&r);
    r.SetMethod(HttpMethod::Post);
    r.SetPostContent(payload.c_str(),payload.size(), ContentFlag::ByRef);

    const nlohmann::json& is_json = nlohmann::json::parse(payload);
    if (is_json != nlohmann::json::value_t::discarded) {
        r.SetHeader("Content-Type", "application/json");
    }
    r.SetUrl(url.c_str());
    r.Execute();
    if (!(r.IsSuccessful() || r.GetStatusCode() == 415)) {
        StrSprintf(response, "Failed to POST %s, curl status %d %s", url.c_str(), r.GetStatusCode(), r.GetStatusStr());
        return false;
    }
    response = std::move(r.GetContent());
    return true;
}
void Resources::Post(const std::string& url, const std::string& payload, AsyncLoadMbCallback callback)
{
    EnqueueWorkerTask([url, payload, callback] {
        std::string response;
        bool ok = Post(url, payload, response);
        EnqueueMainTask([callback, ok, response]() {
            callback(ok, response);
            });
        });
}

void Resources::EnsureFileExists(
    const std::filesystem::path& path_to_file, const std::string& url, AsyncLoadCallback callback)
{
    if (exists(path_to_file)) {
        // if file exists, run the callback immediately in the same thread
        callback(true, L"");
    } else {
        // otherwise try to download it in the worker
        Instance().Download(path_to_file, url, callback);
    }
}

HRESULT Resources::TryCreateTexture(IDirect3DDevice9* device, const std::filesystem::path& path_to_file, IDirect3DTexture9** texture, std::wstring& error) {
    // NB: Some Graphics cards seem to spit out D3DERR_NOTAVAILABLE when loading textures, haven't figured out why but retry if this error is reported
    HRESULT res = D3DERR_NOTAVAILABLE;
    size_t tries = 0;
    auto ext = path_to_file.extension();
    while (res == D3DERR_NOTAVAILABLE && tries++ < 3) {
        if(ext == ".dds")
            res = DirectX::CreateDDSTextureFromFileEx(device, path_to_file.c_str(), 0, D3DPOOL_MANAGED, true, texture);
        else
            res = CreateWICTextureFromFileEx(device, path_to_file.c_str(), 0, 0, D3DPOOL_MANAGED, DirectX::WIC_LOADER_FLAGS::WIC_LOADER_DEFAULT, texture);
    }
    if (res != D3D_OK) {
        StrSwprintf(error, L"Error loading resource from file %s - Error is %S", path_to_file.filename().wstring().c_str(), d3dErrorMessage(res));
        return res;
    }
    if (!*texture) {
        res = D3DERR_NOTFOUND;
        StrSwprintf(error, L"Error loading resource from file %s - texture loaded is null", path_to_file.filename().wstring().c_str());
    }
    return res;
}
HRESULT Resources::TryCreateTexture(IDirect3DDevice9* device, HMODULE hSrcModule, LPCSTR id, IDirect3DTexture9** texture, std::wstring& error) {
    // NB: Some Graphics cards seem to spit out D3DERR_NOTAVAILABLE when loading textures, haven't figured out why but retry if this error is reported
    using namespace std::string_literals;
    HRESULT res = D3DERR_NOTAVAILABLE;
    size_t tries = 0;
    while (res == D3DERR_NOTAVAILABLE && tries++ < 3) {
        EmbeddedResource resource(id, "RCDATA"s, GWToolbox::GetDLLModule());
        res = CreateWICTextureFromMemoryEx(device, static_cast<const uint8_t*>(resource.data()), resource.size(), 0, 0, D3DPOOL_MANAGED, DirectX::WIC_LOADER_DEFAULT, texture);
        if (res != S_OK) {
            res = CreateDDSTextureFromMemoryEx(device, static_cast<const uint8_t*>(resource.data()),
                                               resource.size(), 0, D3DPOOL_MANAGED, DirectX::WIC_LOADER_DEFAULT, texture);

        }
    }
    if (res != D3D_OK) {
        StrSwprintf(error, L"Error loading resource for id %p, module %p - Error is %S", id, hSrcModule, d3dErrorMessage(res));
    }
    else if (!*texture) {
        res = D3DERR_NOTFOUND;
        StrSwprintf(error, L"Error loading resource for id %p, module %p - texture loaded is null", id, hSrcModule);
    }
    return res;
}
void Resources::LoadTexture(IDirect3DTexture9** texture, const std::filesystem::path& path_to_file, AsyncLoadCallback callback)
{
    EnqueueDxTask([path_to_file, texture, callback](IDirect3DDevice9* device) {
        std::wstring error{};
        const bool success = TryCreateTexture(device, path_to_file.c_str(), texture, error) == D3D_OK;
        if (callback) {
            callback(success, error);
        }
        else if (!success) {
            Log::LogW(L"Failed to load texture from file %s\n%s", path_to_file.wstring().c_str(), error.c_str());
        }
    });
}
void Resources::LoadTexture(IDirect3DTexture9** texture, WORD id, AsyncLoadCallback callback)
{
    EnqueueDxTask([id, texture, callback](IDirect3DDevice9* device) {
        std::wstring error{};
        const bool success = TryCreateTexture(device, GWToolbox::GetDLLModule(), MAKEINTRESOURCE(id), texture, error) == D3D_OK;
        if (callback) {
            callback(success, error);
        }
        else if (!success) {
            Log::LogW(L"Failed to load texture from id %d\n%s", id, error.c_str());
        }
    });
}
void Resources::LoadTexture(IDirect3DTexture9** texture, const std::filesystem::path& path_to_file, const std::string& url, AsyncLoadCallback callback)
{
    EnsureFileExists(path_to_file, url, [texture,path_to_file,callback](bool success, const std::wstring& error) {
        if (success) {
            Instance().LoadTexture(texture, path_to_file, callback);
        }
        else {
            if (callback) {
                callback(success, error);
            }
            else {
                Log::LogW(L"Failed to EnsureFileExists %s\n%S", path_to_file.wstring().c_str(), error.c_str());
            }
        }
    });
}
void Resources::LoadTexture(IDirect3DTexture9** texture, const std::filesystem::path& path_to_file, WORD id, AsyncLoadCallback callback)
{
    LoadTexture(texture, path_to_file, [texture, id, callback](bool success, const std::wstring& error) {
        if (!success) {
            Instance().LoadTexture(texture, id, callback);
        }
        else if(callback) {
            callback(success, error);
        }
        });
}
bool Resources::ResourceToFile(WORD id, const std::filesystem::path& path_to_file, std::wstring& error) {
    // otherwise try to install it from resource
    HRSRC hResInfo = FindResourceA(GWToolbox::GetDLLModule(), MAKEINTRESOURCE(id), RT_RCDATA);
    if (!hResInfo) {
        StrSwprintf(error, L"Error calling FindResourceA on resource id %u - Error is %lu", id, GetLastError());
        return false;
    }
    HGLOBAL hRes = LoadResource(GWToolbox::GetDLLModule(), hResInfo);
    if (!hRes) {
        StrSwprintf(error, L"Error calling LoadResource on resource id %u - Error is %lu", id, GetLastError());
        return false;
    }
    DWORD size = SizeofResource(GWToolbox::GetDLLModule(), hResInfo);
    if (!size) {
        StrSwprintf(error, L"Error calling SizeofResource on resource id %u - Error is %lu", id, GetLastError());
        return false;
    }
    // write to file so the user can customize his icons
    HANDLE hFile = CreateFileW(path_to_file.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD bytesWritten;
    BOOL wfRes = WriteFile(hFile, hRes, size, &bytesWritten, NULL);
    if (wfRes != TRUE) {
        StrSwprintf(error, L"Error writing file %s - Error is %lu", path_to_file.filename().wstring().c_str(), GetLastError());
        return false;
    }
    if (bytesWritten != size) {
        StrSwprintf(error, L"Wrote %lu of %lu bytes for %s", bytesWritten, size, path_to_file.filename().wstring().c_str());
        return false;
    }

    CloseHandle(hFile);
    return true;
}

// Load from resource file name on disk with 3 retries
int Resources::LoadIniFromFile(const wchar_t* resource_path, ToolboxIni* inifile) {
    ASSERT(resource_path && *resource_path);
    const auto absolute_path = Resources::GetPath(resource_path);
    return LoadIniFromFile(absolute_path, inifile);
}
// Load from absolute file path on disk with 3 retries
int Resources::LoadIniFromFile(const std::filesystem::path& absolute_path, ToolboxIni* inifile) {
    return inifile->LoadFile(absolute_path);
}
int Resources::SaveIniToFile(const wchar_t* resource_filename, const ToolboxIni* inifile) {
    ASSERT(resource_filename && *resource_filename);
    const auto absolute_path = Resources::GetPath(resource_filename);
    return SaveIniToFile(absolute_path, inifile);
}
int Resources::SaveIniToFile(const std::filesystem::path& absolute_path, const ToolboxIni* ini) {
    auto tmp_file = std::filesystem::path(absolute_path);
    tmp_file += ".tmp";
    const SI_Error res = ini->SaveFile(tmp_file.c_str());
    if (res < 0)
        return res;
    std::error_code ec;
    std::filesystem::rename(tmp_file, absolute_path, ec);
    if (ec.value() != 0)
        return ec.value();
    if (!(!std::filesystem::exists(tmp_file) && std::filesystem::exists(absolute_path)))
        return -1; // renmae failed
    return 0;
}

void Resources::DxUpdate(IDirect3DDevice9* device) {
    while (true) {
        dx_mutex.lock();
        if (dx_jobs.empty()) {
            dx_mutex.unlock();
            return;
        }
        const std::function<void(IDirect3DDevice9*)> func = std::move(dx_jobs.front());
        dx_jobs.pop();
        dx_mutex.unlock();
        func(device);
    }
}

void Resources::Update(float) {
    main_mutex.lock();
    if (main_jobs.empty()) {
        main_mutex.unlock();
        return;
    }
    const std::function<void()> func = std::move(main_jobs.front());
    main_jobs.pop();
    main_mutex.unlock();
    func();
}

IDirect3DTexture9** Resources::GetProfessionIcon(GW::Constants::Profession p) {
    auto prof_id = static_cast<uint32_t>(p);
    if (profession_icons.contains(prof_id)) {
        return profession_icons.at(prof_id);
    }
    const auto texture = new IDirect3DTexture9*;
    *texture = nullptr;
    profession_icons[prof_id] = texture;
    if (profession_icon_urls[prof_id][0]) {
        const auto path = GetPath(PROF_ICONS_PATH);
        EnsureFolderExists(path);
        wchar_t local_image[MAX_PATH];
        swprintf(local_image, _countof(local_image), L"%s\\%d.png", path.c_str(), p);
        char remote_image[128];
        snprintf(remote_image, _countof(remote_image), "https://wiki.guildwars.com/images/%s.png", profession_icon_urls[prof_id]);
        Instance().LoadTexture(texture, local_image, remote_image, [prof_id](bool success, const std::wstring& error) {
            if (!success) {
                Log::ErrorW(L"Failed to load icon for profession %d\n%s", prof_id, error.c_str());
            }
            });
    }
    return texture;
}

IDirect3DTexture9** Resources::GetGuildWarsWikiImage(const char* filename, size_t width) {
    ASSERT(filename && filename[0]);
    std::string filename_on_disk;
    if (width > 0) {
        StrSprintf(filename_on_disk, "%dpx_%s", width, filename);
    }
    else {
        filename_on_disk = filename;
    }
    const auto filename_sanitised = GuiUtils::SanitiseFilename(filename_on_disk);
    if (guild_wars_wiki_images.contains(filename_sanitised))
    {
        return guild_wars_wiki_images.at(filename_sanitised);
    }
    const auto callback = [filename_sanitised](bool success, const std::wstring& error) {
        if (!success) {
            Log::ErrorW(L"Failed to load Guild Wars Wiki file%S\n%s", filename_sanitised.c_str(), error.c_str());
        }
        else {
            Log::LogW(L"Loaded Guild Wars Wiki file %S", filename_sanitised.c_str());
        }
    };
    const auto texture = new IDirect3DTexture9*;
    *texture = nullptr;
    guild_wars_wiki_images[filename] = texture;
    static std::filesystem::path path = GetPath(GUILD_WARS_WIKI_FILES_PATH);
    if (!EnsureFolderExists(path)) {
        trigger_failure_callback(callback, L"Failed to create folder %s", path.wstring().c_str());
        return texture;
    }
    std::wstring path_to_file;
    // Check for local file
    StrSwprintf(path_to_file, L"%s\\%S", path.wstring().c_str(), filename_sanitised.c_str());
    if (std::filesystem::exists(path_to_file)) {
        Instance().LoadTexture(texture, path_to_file, callback);
        return texture;
    }
    // No local file found; download from wiki via skill link URL
    std::string wiki_url = "https://wiki.guildwars.com/wiki/File:";
    wiki_url.append(GuiUtils::UrlEncode(filename, '_'));
    Instance().Download(wiki_url.c_str(), [texture, filename_sanitised, callback, width](bool ok, const std::string& response) {
        if (!ok) {
            callback(ok, GuiUtils::StringToWString(response));
            return; // Already logged whatever errors
        }

        std::string tmp_str;
        // Find a valid png or jpg image inside the HTML response, <img alt="<skill_id>" src="<location_of_image>"
        StrSprintf(tmp_str, R"(class="fullMedia"[\s\S]*?href=['"]([^"']+))");
        std::regex image_finder(tmp_str);
        std::smatch m;
        std::regex_search(response, m, image_finder);
        if (!m.size()) {
            trigger_failure_callback(callback, L"Regex failed loading file %S", filename_sanitised.c_str());
            return;
        }
        std::string image_url = m[1].str();
        std::wstring path_to_file2;
        StrSwprintf(path_to_file2, L"%s\\%S", path.c_str(), filename_sanitised.c_str());
        if (width) {
            // Divert to resized version using mediawiki's method
            image_finder = "/images/(.*)/([^/]+)$";
            std::regex_search(image_url, m, image_finder);
            if (!m.size()) {
                trigger_failure_callback(callback, L"Regex failed evaluating GWW thumbnail from %S",image_url.c_str());
                return;
            }
            StrSprintf(tmp_str, "/images/thumb/%s/%s/%dpx-%s", m[1].str().c_str(), m[2].str().c_str(), width, m[2].str().c_str());
            image_url = tmp_str;
        }
        // https://wiki.guildwars.com/images/thumb/5/5c/Eternal_Protector_of_Tyria.jpg/150px-Eternal_Protector_of_Tyria.jpg
        if (strncmp(image_url.c_str(), "http", 4) != 0) {
            StrSprintf(tmp_str, "https://wiki.guildwars.com%s", image_url.c_str());
            image_url = tmp_str;
        }
        Instance().LoadTexture(texture, path_to_file2, image_url, callback);
        });
    return texture;
}

IDirect3DTexture9** Resources::GetSkillImage(GW::Constants::SkillID skill_id) {

    if (skill_images.contains(skill_id)) {
        return skill_images.at(skill_id);
    }
    const auto callback = [skill_id](bool success, const std::wstring& error) {
        if (!success) {
            Log::ErrorW(L"Failed to load skill image %d\n%s", skill_id, error.c_str());
        }
        else {
            Log::LogW(L"Loaded skill image %d", skill_id);
        }
    };
    const auto texture = new IDirect3DTexture9*;
    *texture = nullptr;
    skill_images[skill_id] = texture;
    if (skill_id == static_cast<GW::Constants::SkillID>(0))
        return texture;
    static std::filesystem::path path = GetPath(SKILL_IMAGES_PATH);
    if (!EnsureFolderExists(path)) {
        trigger_failure_callback(callback, L"Failed to create folder %s", path.wstring().c_str());
        return texture;
    }
    wchar_t path_to_file[MAX_PATH];
    // Check for local jpg file
    swprintf(path_to_file, _countof(path_to_file), L"%s\\%d.jpg", path.wstring().c_str(), skill_id);
    if (std::filesystem::exists(path_to_file)) {
        Instance().LoadTexture(texture, path_to_file, callback);
        return texture;
    }
    // Check for local png file
    swprintf(path_to_file, _countof(path_to_file), L"%s\\%d.png", path.wstring().c_str(), skill_id);
    if (std::filesystem::exists(path_to_file)) {
        Instance().LoadTexture(texture, path_to_file, callback);
        return texture;
    }
    // No local file found; download from wiki via skill link URL
    char url[128];
    snprintf(url, _countof(url), "https://wiki.guildwars.com/wiki/Game_link:Skill_%d", skill_id);
    Instance().Download(url, [texture, skill_id, callback](bool ok, const std::string& response) {
        if (!ok) {
            callback(ok, GuiUtils::StringToWString(response));
            return; // Already logged whatever errors
        }

        char regex_str[255];
        // Find a valid png or jpg image inside the HTML response, <img alt="<skill_id>" src="<location_of_image>"
        ASSERT(snprintf(regex_str, sizeof(regex_str), "class=\"skill-image\"[\\s\\S]*?<img[^>]+src=['\"]([^\"']+)([.](png|jpg))") != -1);
        std::regex image_finder(regex_str);
        std::smatch m;
        std::regex_search(response, m, image_finder);
        if (!m.size()) {
            // Search condition e..g Crippled
            ASSERT(snprintf(regex_str, sizeof(regex_str), "<blockquote[\\s\\S]*?<img[^>]+src=['\"]([^\"']+)([.](png|jpg))") != -1);
            image_finder = regex_str;
            std::regex_search(response, m, image_finder);
        }
        if (!m.size()) {
            // Search blessing
            ASSERT(snprintf(regex_str, sizeof(regex_str), "class=\"blessing-infobox\"[\\s\\S]*?<img[^>]+src=['\"]([^\"']+)([.](png|jpg))") != -1);
            image_finder = regex_str;
            std::regex_search(response, m, image_finder);
        }
        if (!m.size()) {
            // Search bounty
            ASSERT(snprintf(regex_str, sizeof(regex_str), "class=\"bounty-infobox\"[\\s\\S]*?<img[^>]+src=['\"]([^\"']+)([.](png|jpg))") != -1);
            image_finder = regex_str;
            std::regex_search(response, m, image_finder);
        }
        if (!m.size()) {
            trigger_failure_callback(callback, L"Regex failed loading skill id %d", skill_id);
            return;
        }
        const std::string& image_path = m[1].str();
        const std::string& image_extension = m[2].str();
        wchar_t path_to_file[MAX_PATH];
        ASSERT(swprintf(path_to_file, _countof(path_to_file), L"%s\\%d%S", path.c_str(), skill_id, image_extension.c_str()) != -1);
        char url[128];

        if (strncmp(image_path.c_str(), "http", 4) == 0) {
            // Image URL is absolute
            snprintf(url, _countof(url), "%s%s", image_path.c_str(), image_extension.c_str());
        }
        else {
            // Image URL is relative to domain
            snprintf(url, _countof(url), "https://wiki.guildwars.com%s%s", image_path.c_str(), image_extension.c_str());
        }
        Instance().LoadTexture(texture, path_to_file, url, callback);
        });
    return texture;
}

GuiUtils::EncString* Resources::GetMapName(GW::Constants::MapID map_id) {
    const auto found = map_names.find(map_id);
    if (found != map_names.end()) {
        return found->second;
    }
    const auto area = GW::Map::GetMapInfo(map_id);
    ASSERT(area);
    const auto ret = DecodeStringId(area->name_id);
    map_names[map_id] = ret;
    return ret;
}
GuiUtils::EncString* Resources::DecodeStringId(uint32_t enc_str_id) {
    const auto found = encoded_string_ids.find(enc_str_id);
    if (found != encoded_string_ids.end()) {
        return found->second;
    }
    const auto enc_string = new GuiUtils::EncString(enc_str_id, false);
    encoded_string_ids[enc_str_id] = enc_string;
    return enc_string;
}

IDirect3DTexture9** Resources::GetItemImage(const std::wstring& item_name) {
    if (item_name.empty()) {
        return nullptr;
    }
    if (item_images.contains(item_name)) {
        return item_images.at(item_name);
    }
    const auto callback = [item_name](bool success, const std::wstring& error) {
        if (!success) {
            Log::ErrorW(L"Failed to load item image %s\n%s", item_name.c_str(), error.c_str());
        }
        else {
            Log::LogW(L"Loaded item image %s", item_name.c_str());
        }
    };
    const auto texture = new IDirect3DTexture9*;
    *texture = nullptr;
    item_images[item_name] = texture;
    static std::filesystem::path path = GetPath(ITEM_IMAGES_PATH);
    ASSERT(EnsureFolderExists(path));

    wchar_t path_to_file[MAX_PATH];
    // Check for local png image
    swprintf(path_to_file, _countof(path_to_file), L"%s\\%s.png", path.c_str(), item_name.c_str());
    if (std::filesystem::exists(path_to_file)) {
        Instance().LoadTexture(texture, path_to_file, callback);
        return texture;
    }

    // No local file found; download from wiki via searching by the item name; the wiki will usually return a 302 redirect if its an exact item match
    const std::string search_str = GuiUtils::WikiUrl(item_name);
    Instance().Download(search_str, [texture, item_name, callback](bool ok, const std::string& response) {
        if (!ok) {
            callback(ok, GuiUtils::StringToWString(response));
            return;
        }
        const std::string item_name_str = GuiUtils::WStringToString(item_name);
        // matches any characters that need to be escaped in RegEx
        const std::regex specialChars{ R"([-[\]{}()*+?.,\^$|#\s])" };
        const std::string sanitized = std::regex_replace(item_name_str, specialChars, R"(\$&)");
        std::smatch m;
        // Find first png image that has an alt tag matching the html encoded title of the page
        char regex_str[255];
        snprintf(regex_str, sizeof(regex_str), R"(<img[^>]+alt=['"][^>]*%s[^>]*['"][^>]+src=['"]([^"']+)([.](png)))", sanitized.c_str());
        if (!std::regex_search(response, m, std::regex(regex_str))) {
            // Failed to find via item name; try via page title
            const std::regex title_finder("<title>(.*) - Guild Wars Wiki.*</title>");
            if (!std::regex_search(response, m, title_finder)) {
                trigger_failure_callback(callback, L"Failed to find title HTML for %s from wiki", item_name.c_str());
                return;
            }
            std::string html_item_name = GuiUtils::HtmlEncode(m[1].str());
            snprintf(regex_str, sizeof(regex_str),
                     R"(<img[^>]+alt=['"][^>]*%s[^>]*['"][^>]+src=['"]([^"']+)([.](png)))", html_item_name.c_str());
            if (!std::regex_search(response, m, std::regex(regex_str))) {
                trigger_failure_callback(callback, L"Failed to find image HTML for %s from wiki", item_name.c_str());
                return;
            }
        }
        const std::string& image_path = m[1].str();
        const std::string& image_extension = m[2].str();
        wchar_t path_to_file[MAX_PATH];
        swprintf(path_to_file, _countof(path_to_file), L"%s\\%s%S", path.c_str(), item_name.c_str(), image_extension.c_str());
        char url[128];
        if (strncmp(image_path.c_str(), "http", 4) == 0) {
            // Image URL is absolute
            snprintf(url, _countof(url), "%s%s", image_path.c_str(), image_extension.c_str());
        }
        else {
            // Image URL is relative to domain
            snprintf(url, _countof(url), "https://wiki.guildwars.com%s%s", image_path.c_str(), image_extension.c_str());
        }
        Instance().LoadTexture(texture, path_to_file, url, callback);
        });
    return texture;
}
