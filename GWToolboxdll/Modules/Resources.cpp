#include "stdafx.h"

#include <DDSTextureLoader/DDSTextureLoader9.h>
#include <WICTextureLoader/WICTextureLoader9.h>

#include <GWCA/GameEntities/Map.h>
#include <GWCA/GameEntities/Item.h>
#include <GWCA/GameEntities/Skill.h>

#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/ItemMgr.h>

#include <EmbeddedResource.h>
#include <GWToolbox.h>
#include <Logger.h>
#include <Path.h>
#include <RestClient.h>
#include <Str.h>

#include <GWCA/Constants/Constants.h>
#include <Modules/Resources.h>
#include <Utils/GuiUtils.h>

#pragma warning(push) // Save current warning state
#pragma warning(disable : 4189) // local variable is initialized but not referenced
#include <include/nfd.h>
#include <nfd_common.c>
#include <nfd_win.cpp>
#pragma warning(pop)
#include <wolfssl/wolfcrypt/asn.h>

#include <Modules/GwDatTextureModule.h>
#include <Constants/EncStrings.h>
#include <Utils/TextUtils.h>

namespace {
    bool initialised_curl = false;

    const char* d3dErrorMessage(HRESULT code)
    {
        switch (code) {
            case E_INVALIDARG:
                return "E_INVALIDARG One or more arguments are invalid.";
            case D3DERR_NOTAVAILABLE:
                return "D3DERR_NOTAVAILABLE";
            case D3DERR_OUTOFVIDEOMEMORY:
                return "D3DERR_OUTOFVIDEOMEMORY";
            case D3DERR_INVALIDCALL:
                return "D3DERR_INVALIDCALL";
            case E_OUTOFMEMORY:
                return "E_OUTOFMEMORY";
            case D3D_OK:
                return "D3D_OK";
            default:
                static std::string str;
                str = std::format("Unknown D3D error {:#08x}", code);
                return str.c_str();
        }
    }

    constexpr std::array profession_icon_urls = {
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
    std::map<uint32_t, IDirect3DTexture9**> profession_icons;
    std::map<GW::Constants::SkillID, IDirect3DTexture9**> skill_images;
    std::map<std::wstring, IDirect3DTexture9**> item_images;
    std::map<std::string, IDirect3DTexture9**> guild_wars_wiki_images;
    const std::map<std::string, const char*> damagetype_icon_urls = {
        {"Blunt damage", "1/19/Blunt_damage.png/60px-Blunt_damage.png"},
        {"Piercing damage", "1/1a/Piercing_damage.png/60px-Piercing_damage.png"},
        {"Slashing damage", "3/3c/Slashing_damage.png/60px-Slashing_damage.png"},
        {"Cold damage", "4/48/Cold_damage.png/60px-Cold_damage.png"},
        {"Earth damage", "b/bb/Earth_damage.png/60px-Earth_damage.png"},
        {"Fire damage", "6/6a/Fire_damage.png/60px-Fire_damage.png"},
        {"Lightning damage", "0/06/Lightning_damage.png/60px-Lightning_damage.png"},
    };

    std::map<std::string, IDirect3DTexture9**> damagetype_icons;
    std::map<GW::Constants::MapID, GuiUtils::EncString*> map_names;
    std::map<GW::Constants::MapID, GuiUtils::EncString*> region_names;
    std::unordered_map<GW::Constants::Language, std::unordered_map<uint32_t, GuiUtils::EncString*>> encoded_string_ids;
    std::filesystem::path current_settings_folder;
    constexpr size_t MAX_WORKERS = 5;
    const wchar_t* GUILD_WARS_WIKI_FILES_PATH = L"img\\gww_files";
    const wchar_t* SKILL_IMAGES_PATH = L"img\\skills";
    const wchar_t* ITEM_IMAGES_PATH = L"img\\items";
    const wchar_t* PROF_ICONS_PATH = L"img\\professions";
    const wchar_t* DMGTYPE_ICONS_PATH = L"img\\damagetypes";

    std::recursive_mutex worker_mutex;
    std::recursive_mutex main_mutex;
    std::recursive_mutex dx_mutex;

    // tasks to be done async by the worker thread
    std::queue<std::function<void()>> thread_jobs;
    // tasks to be done in the render thread
    std::queue<std::function<void(IDirect3DDevice9*)>> dx_jobs;
    // tasks to be done in main thread
    std::queue<std::function<void()>> main_jobs;


    IDirect3DTexture9* empty_texture_ptr = 0;

    bool should_stop = false;



    // snprintf error message, pass to callback as a failure. Used internally.
    void trigger_failure_callback(const std::function<void(bool, const std::wstring&)>& callback, const wchar_t* format, ...)
    {
        std::wstring out;
        va_list vl;
        va_start(vl, format);
        const auto written = StrVswprintf(out, format, vl);
        va_end(vl);
        ASSERT(written != -1);
        callback(false, out);
    };

    float cached_ui_scale = .0f;

    GW::HookEntry OnUIMessage_Hook;

    void OnUIMessage(GW::HookStatus*, const GW::UI::UIMessage message_id, void* wparam, void*)
    {
        switch (message_id) {
            case GW::UI::UIMessage::kPreferenceEnumChanged:
                if (wparam && *static_cast<GW::UI::EnumPreference*>(wparam) == GW::UI::EnumPreference::InterfaceSize) {
                    Resources::GetGWScaleMultiplier(true); // Re-fetch ui scale indicator
                }
                break;
        }
    }

    class WorkerThread {
    public:
        bool is_running = false;
        std::jthread thread;

        WorkerThread()
        {
            ASSERT(!is_running);
            is_running = true;
            thread = std::jthread([&] {
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
                is_running = false;
            });
        }

        ~WorkerThread()
        {
            ASSERT(!is_running);
        }
    };

    std::vector<WorkerThread*> workers;

    void InitRestClient(RestClient* r)
    {
        char user_agent_str[32];
        ASSERT(snprintf(user_agent_str, sizeof(user_agent_str), "GWToolboxpp/%s", GWTOOLBOXDLL_VERSION) != -1);
        r->SetUserAgent(user_agent_str);
        r->SetFollowLocation(true);
        r->SetVerifyPeer(false); // idc about mitm or out of date certs
        r->SetMethod(HttpMethod::Get);
        r->SetVerifyHost(false);
    }

    const std::string HashStr(const std::string& str)
    {
        const auto bytes_to_hash = std::vector<byte>(str.begin(), str.end());
        auto hash = std::vector<byte>(WC_SHA256_DIGEST_SIZE);
        wc_Sha256Hash(bytes_to_hash.data(), bytes_to_hash.size(), hash.data());
        std::stringstream hexstream;
        hexstream << std::hex << std::setfill('0');
        for (auto b : hash) {
            hexstream << std::setw(2) << static_cast<unsigned>(b);
        }
        return hexstream.str();
    };
} // namespace

Resources::Resources()
{
    InitCurl();
    initialised_curl = true;
    co_initialized = SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED));
}

void Resources::EnqueueWorkerTask(const std::function<void()>& f)
{
    worker_mutex.lock();
    thread_jobs.push(f);
    worker_mutex.unlock();
}

void Resources::EnqueueMainTask(const std::function<void()>& f)
{
    main_mutex.lock();
    main_jobs.push(f);
    main_mutex.unlock();
}

void Resources::EnqueueDxTask(const std::function<void(IDirect3DDevice9*)>& f)
{
    dx_mutex.lock();
    dx_jobs.push(f);
    dx_mutex.unlock();
}

void Resources::OpenFileDialog(std::function<void(const char*)> callback, const char* filterList, const char* defaultPath)
{
    auto filterList_cpy = new std::string(filterList);
    auto defaultPath_cpy = new std::string(defaultPath);
    EnqueueWorkerTask([callback, filterList_cpy, defaultPath_cpy] {
        nfdchar_t* outPath = nullptr;
        const nfdresult_t result = NFD_OpenDialog(filterList_cpy->c_str(), defaultPath_cpy->c_str(), &outPath);
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
        if (outPath) {
            free(outPath);
        }
    });
}

void Resources::SaveFileDialog(std::function<void(const char*)> callback, const char* filterList, const char* defaultPath)
{
    auto filterList_cpy = new std::string(filterList);
    auto defaultPath_cpy = new std::string(defaultPath);

    EnqueueWorkerTask([callback, filterList_cpy, defaultPath_cpy] {
        nfdchar_t* outPath = nullptr;
        const nfdresult_t result = NFD_SaveDialog(filterList_cpy->c_str(), defaultPath_cpy->c_str(), &outPath);
        delete filterList_cpy;
        delete defaultPath_cpy;

        switch (result) {
            case NFD_OKAY:
                callback(outPath);
                break;
            case NFD_CANCEL:
                callback(nullptr);
                break;
            default:
                Log::Log("NFD_OpenDialog Error: %s\n", NFD_GetError());
                break;
        }
        if (outPath) {
            free(outPath);
        }
    });
}

float Resources::GetGWScaleMultiplier(const bool force)
{
    if (force || cached_ui_scale == .0f) {
        const auto interfacesize = static_cast<GW::Constants::InterfaceSize>(GetPreference(GW::UI::EnumPreference::InterfaceSize));

        switch (interfacesize) {
            case GW::Constants::InterfaceSize::SMALL:
                cached_ui_scale = .9f;
                break;
            case GW::Constants::InterfaceSize::LARGE:
                cached_ui_scale = 1.166666f;
                break;
            case GW::Constants::InterfaceSize::LARGER:
                cached_ui_scale = 1.3333333f;
                break;
            default:
                cached_ui_scale = 1.f;
                break;
        }
    }
    return cached_ui_scale;
}

HRESULT Resources::ResolveShortcut(const std::filesystem::path& in_shortcut_path, std::filesystem::path& out_actual_path)
{
    if (in_shortcut_path.extension() != ".lnk") {
        out_actual_path = in_shortcut_path;
        return S_OK;
    }
    HRESULT hRes = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (!SUCCEEDED(hRes)) {
        return hRes;
    }
    IShellLink* psl = nullptr;

    // buffer that receives the null-terminated string
    // for the drive and path
    TCHAR szPath[MAX_PATH];
    // buffer that receives the null-terminated
    // string for the description
    TCHAR szDesc[MAX_PATH];
    // structure that receives the information about the shortcut
    WIN32_FIND_DATA wfd{};

    // Get a pointer to the IShellLink interface
    hRes = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLink, (void**)&psl);
    if (!SUCCEEDED(hRes)) {
        return hRes;
    }
    // Get a pointer to the IPersistFile interface
    IPersistFile* ppf = nullptr;
    psl->QueryInterface(IID_IPersistFile, reinterpret_cast<void**>(&ppf));

    // IPersistFile is using LPCOLESTR,
    // Open the shortcut file and initialize it from its contents
    hRes = ppf->Load(in_shortcut_path.wstring().c_str(), STGM_READ);
    if (!SUCCEEDED(hRes)) {
        return hRes;
    }
    // Try to find the target of a shortcut,
    // even if it has been moved or renamed
    hRes = psl->Resolve(nullptr, SLR_UPDATE);
    if (!SUCCEEDED(hRes)) {
        return hRes;
    }
    // Get the path to the shortcut target
    hRes = psl->GetPath(szPath, MAX_PATH, &wfd, SLGP_RAWPATH);
    if (!SUCCEEDED(hRes)) {
        return hRes;
    }

    // Get the description of the target
    hRes = psl->GetDescription(szDesc, MAX_PATH);
    if (!SUCCEEDED(hRes)) {
        return hRes;
    }
    out_actual_path = szPath;
    return hRes;
}

void Resources::Initialize()
{
    ToolboxModule::Initialize();
    for (size_t i = 0; i < MAX_WORKERS; i++) {
        workers.push_back(new WorkerThread());
    }
    RegisterUIMessageCallback(&OnUIMessage_Hook, GW::UI::UIMessage::kPreferenceEnumChanged, OnUIMessage, 0x8000);
}

void Resources::Cleanup()
{
    for (const auto worker : workers) {
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
    for (const auto& enc_strings : encoded_string_ids | std::views::values) {
        for (const auto& enc_string : enc_strings | std::views::values) {
            delete enc_string;
        }
    }
    encoded_string_ids.clear();
    map_names.clear(); // NB: Map names are pointers to encoded_string_ids
}

void Resources::Terminate()
{
    ToolboxModule::Terminate();

    GW::UI::RemoveUIMessageCallback(&OnUIMessage_Hook);

    Cleanup();
    if (initialised_curl)
        ShutdownCurl();
    initialised_curl = false;
    if (co_initialized) {
        CoUninitialize();
    }
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
}

bool Resources::CanTerminate()
{
    for (const auto worker : workers) {
        if (worker->is_running)
            return false;
    }
    return true;
}

void Resources::SignalTerminate()
{
    ToolboxModule::SignalTerminate();
    should_stop = true;
}

void Resources::EndLoading() const
{
    EnqueueWorkerTask([this] {
        should_stop = true;
    });
}

std::filesystem::path Resources::GetComputerFolderPath()
{
    std::filesystem::path computer_name;
    ASSERT(PathGetComputerName(computer_name));

    std::filesystem::path docpath;
    ASSERT(PathGetDocumentsPath(docpath, L"GWToolboxpp"));
    docpath = docpath / computer_name;

    ASSERT(PathCreateDirectorySafe(docpath));

    return docpath;
}

std::filesystem::path Resources::GetSettingsFolderName()
{
    return current_settings_folder;
}

std::filesystem::path Resources::GetSettingsFolderPath()
{
    const auto computer_path = GetComputerFolderPath();
    return current_settings_folder.empty() ? computer_path : computer_path / current_settings_folder;
}

bool Resources::SetSettingsFolder(const std::filesystem::path& foldername)
{
    if (foldername.empty()) {
        current_settings_folder.clear();
    }
    else {
        current_settings_folder = L"configs" / foldername;
    }
    return EnsureFolderExists(GetSettingsFolderPath());
}

std::filesystem::path Resources::GetSettingFile(const std::filesystem::path& file)
{
    return GetSettingsFolderPath() / file;
}

std::filesystem::path Resources::GetPath(const std::filesystem::path& file)
{
    return GetComputerFolderPath() / file;
}

std::filesystem::path Resources::GetPath(const std::filesystem::path& folder, const std::filesystem::path& file)
{
    return GetComputerFolderPath() / folder / file;
}

bool Resources::EnsureFolderExists(const std::filesystem::path& path)
{
    return exists(path) || create_directory(path);
}

utf8::string Resources::GetPathUtf8(const std::wstring& file)
{
    const std::wstring path = GetPath(file);
    return Unicode16ToUtf8(path.c_str());
}

bool Resources::Download(const std::filesystem::path& path_to_file, const std::string& url, std::wstring& response)
{
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
        return StrSwprintf(response, L"%S", content.c_str()), false;
    }
    if (!content.length()) {
        return StrSwprintf(response, L"Failed to download %S, no content length", url.c_str()), false;
    }
    FILE* fp = fopen(path_to_file.string().c_str(), "wb");
    if (!fp) {
        return StrSwprintf(response, L"Failed to call fopen for %s, err %d", path_to_file.wstring().c_str(), GetLastError()), false;
    }
    const auto written = fwrite(content.data(), content.size() + 1, 1, fp);
    fclose(fp);
    if (written != 1) {
        return StrSwprintf(response, L"Failed to call fwrite for %s, err %d", path_to_file.wstring().c_str(), GetLastError()), false;
    }
    return true;
}

void Resources::Download(const std::filesystem::path& path_to_file, const std::string& url, const AsyncLoadCallback& callback) const
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

bool Resources::ReadFile(const std::filesystem::path& path, std::string& response)
{
    if (!std::filesystem::exists(path))
        return false;
    std::ifstream file(path);
    if (!file.is_open())
        return false;
    std::stringstream ss;
    ss << file.rdbuf();
    response = ss.str();
    return !response.empty();
}

bool Resources::ReadFile(const std::filesystem::path& path, std::wstring& response)
{
    if (!std::filesystem::exists(path))
        return false;
    std::ifstream file(path);
    if (!file.is_open())
        return false;
    std::wstringstream ss;
    ss << file.rdbuf();
    response = ss.str();
    return !response.empty();
}

bool Resources::Download(const std::string& url, std::string& response)
{
    int statusCode = 0;
    return Download(url, response, statusCode);
}

bool Resources::Download(const std::string& url, std::string& response, int& statusCode)
{
    RestClient r;
    InitRestClient(&r);
    r.SetUrl(url.c_str());
    r.Execute();
    statusCode = r.GetStatusCode();
    if (!r.IsSuccessful()) {
        response = std::format("Failed to download {}, curl status {} {}", url, r.GetStatusCode(), r.GetStatusStr());
        return false;
    }
    response = std::move(r.GetContent());
    return true;
}

void Resources::Download(const std::string& url, AsyncLoadMbCallback callback, void* context)
{
    EnqueueWorkerTask([url, callback, context] {
        std::string response;
        int statusCode = 0;
        bool ok = Download(url, response, statusCode);
        EnqueueMainTask([callback, ok, response, context] {
            callback(ok, response, context);
        });
    });
}

void Resources::Download(const std::string& url, AsyncLoadMbCallback callback, void* context, std::chrono::seconds cache_duration)
{
    EnqueueWorkerTask([url, callback, context, &cache_duration] {
        const auto get_cache_modified_time = [](const std::filesystem::path& file_name) -> std::optional<std::filesystem::file_time_type> {
            if (!std::filesystem::exists(file_name)) {
                return std::optional<std::filesystem::file_time_type>();
            }

            const auto file_time = std::filesystem::last_write_time(file_name);
            return file_time;
        };

        const auto load_from_cache = [](const std::filesystem::path& file_name) -> std::optional<std::string> {
            std::ifstream cache_file(file_name);
            if (!cache_file.is_open()) {
                return {};
            }

            std::string contents((std::istreambuf_iterator<char>(cache_file)), std::istreambuf_iterator<char>());
            return contents;
        };

        const auto save_to_cache = [](const std::filesystem::path& file_name, const std::string& content) -> bool {
            std::filesystem::create_directories(file_name.parent_path());
            std::ofstream cache_file(file_name);
            if (!cache_file.is_open()) {
                return false;
            }

            cache_file << content;
            return true;
        };

        const auto remove_protocol = [](const std::string& url) -> std::string {
            const std::string http = "http://";
            const std::string https = "https://";

            // Check if the URL starts with http:// or https:// and remove it
            if (url.substr(0, http.size()) == http) {
                return url.substr(http.size());
            }
            if (url.substr(0, https.size()) == https) {
                return url.substr(https.size());
            }
            return url; // Return the original if no match is found
        };
        const auto cache_path = Resources::GetPath("cache") / HashStr(remove_protocol(url));
        const auto expiration = get_cache_modified_time(cache_path);
        if (expiration.has_value() &&
            expiration.value() - std::chrono::file_clock::now() < cache_duration) {
            const auto response = load_from_cache(cache_path);
            if (response.has_value()) {
                EnqueueMainTask([callback, context, response] {
                    callback(true, response.value(), context);
                });
                return;
            }
        }
        std::string response;
        int statusCode = 0;
        bool ok = Download(url, response, statusCode);
        if (ok ||
            (statusCode >= 300 && statusCode < 500)) {
            save_to_cache(cache_path, response);
        }
        EnqueueMainTask([callback, ok, response, context] {
            callback(ok, response, context);
        });
    });
}

bool Resources::Post(const std::string& url, const std::string& payload, std::string& response)
{
    RestClient r;
    InitRestClient(&r);
    r.SetMethod(HttpMethod::Post);
    r.SetPostContent(payload.c_str(), payload.size(), ContentFlag::ByRef);

    std::string content_type = nlohmann::json::accept(payload) ? "application/json" : "application/x-www-form-urlencoded";
    r.SetHeader("Content-Type", content_type.c_str());
    r.SetUrl(url.c_str());
    r.Execute();
    if (!(r.IsSuccessful() || r.GetStatusCode() == 415)) {
        StrSprintf(response, "Failed to POST %s, curl status %d %s", url.c_str(), r.GetStatusCode(), r.GetStatusStr());
        return false;
    }
    response = std::move(r.GetContent());
    return true;
}

void Resources::Post(const std::string& url, const std::string& payload, AsyncLoadMbCallback callback, void* wparam)
{
    EnqueueWorkerTask([url, payload, callback, wparam] {
        std::string response;
        bool ok = Post(url, payload, response);
        EnqueueMainTask([callback, ok, response, wparam] {
            callback(ok, response, wparam);
        });
    });
}

void Resources::EnsureFileExists(const std::filesystem::path& path_to_file, const std::string& url, const AsyncLoadCallback& callback)
{
    if (exists(path_to_file)) {
        // if file exists, run the callback immediately in the same thread
        callback(true, L"");
    }
    else {
        // otherwise try to download it in the worker
        Instance().Download(path_to_file, url, callback);
    }
}

HRESULT Resources::TryCreateTexture(IDirect3DDevice9* device, const std::filesystem::path& path_to_file, IDirect3DTexture9** texture, std::wstring& error)
{
    // NB: Some Graphics cards seem to spit out D3DERR_NOTAVAILABLE when loading textures, haven't figured out why but retry if this error is reported
    HRESULT res = D3DERR_NOTAVAILABLE;
    size_t tries = 0;
    const auto ext = path_to_file.extension();
    while (res == D3DERR_NOTAVAILABLE && tries++ < 5) {
        if (ext == ".dds") {
            res = DirectX::CreateDDSTextureFromFileEx(device, path_to_file.c_str(), 0, D3DPOOL_MANAGED, true, texture);
        }
        else {
            res = CreateWICTextureFromFileEx(device, path_to_file.c_str(), 0, 0, D3DPOOL_MANAGED, DirectX::WIC_LOADER_FLAGS::WIC_LOADER_DEFAULT, texture);
        }
    }
    if (res != D3D_OK) {
        std::filesystem::remove(path_to_file);
        StrSwprintf(error, L"Error loading resource from file %s - Error is %S", path_to_file.filename().wstring().c_str(), d3dErrorMessage(res));
        return res;
    }
    if (!*texture) {
        res = D3DERR_NOTFOUND;
        StrSwprintf(error, L"Error loading resource from file %s - texture loaded is null", path_to_file.filename().wstring().c_str());
    }
    return res;
}

HRESULT Resources::TryCreateTexture(IDirect3DDevice9* pDevice, const HMODULE hSrcModule, const LPCSTR pSrcResource, IDirect3DTexture9** texture, std::wstring& error)
{
    // NB: Some Graphics cards seem to spit out D3DERR_NOTAVAILABLE when loading textures, haven't figured out why but retry if this error is reported
    HRESULT res = D3DERR_NOTAVAILABLE;
    size_t tries = 0;
    while (res == D3DERR_NOTAVAILABLE && tries++ < 3) {
        EmbeddedResource resource(pSrcResource, RT_RCDATA, hSrcModule);
        if (!resource.data()) {
            StrSwprintf(error, L"Error loading resource for id %p, module %p - texture not found", pSrcResource, hSrcModule);
            return D3DERR_NOTFOUND;
        }
        res = CreateWICTextureFromMemoryEx(pDevice, static_cast<const uint8_t*>(resource.data()), resource.size(), 0, 0, D3DPOOL_MANAGED, DirectX::WIC_LOADER_DEFAULT, texture);
        if (res != S_OK) {
            res = DirectX::CreateDDSTextureFromMemoryEx(pDevice, static_cast<const uint8_t*>(resource.data()), resource.size(), 0, D3DPOOL_MANAGED, false, texture);
        }
    }
    if (res != D3D_OK) {
        StrSwprintf(error, L"Error loading resource for id %p, module %p - Error is %s", pSrcResource, hSrcModule, d3dErrorMessage(res));
    }
    else if (!*texture) {
        res = D3DERR_NOTFOUND;
        StrSwprintf(error, L"Error loading resource for id %p, module %p - texture loaded is null", pSrcResource, hSrcModule);
    }
    return res;
}

void Resources::LoadTexture(IDirect3DTexture9** texture, const std::filesystem::path& path_to_file, AsyncLoadCallback callback)
{
    EnqueueDxTask([path_to_file, texture, callback](IDirect3DDevice9* device) {
        std::wstring error;
        const HRESULT res = TryCreateTexture(device, path_to_file.c_str(), texture, error);
        const bool success = res == D3D_OK;
        if (callback) {
            callback(success, error);
        }
        else if (!success) {
            Log::LogW(L"Failed to load texture from file %s\n%s", TextUtils::PrintFilename(path_to_file.wstring()).c_str(), error.c_str());
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
    EnsureFileExists(path_to_file, url, [texture, path_to_file, callback](const bool success, const std::wstring& error) {
        if (success) {
            LoadTexture(texture, path_to_file, callback);
        }
        else {
            if (callback) {
                callback(success, error);
            }
            else {
                Log::LogW(L"Failed to EnsureFileExists %s\n%S", TextUtils::PrintFilename(path_to_file.wstring()).c_str(), error.c_str());
            }
        }
    });
}

void Resources::LoadTexture(IDirect3DTexture9** texture, const std::filesystem::path& path_to_file, WORD id, AsyncLoadCallback callback)
{
    LoadTexture(texture, path_to_file, [texture, id, callback](const bool success, const std::wstring& error) {
        if (!success) {
            LoadTexture(texture, id, callback);
        }
        else if (callback) {
            callback(success, error);
        }
    });
}

bool Resources::ResourceToFile(const WORD id, const std::filesystem::path& path_to_file, std::wstring& error)
{
    // otherwise try to install it from resource
    const HRSRC hResInfo = FindResourceA(GWToolbox::GetDLLModule(), MAKEINTRESOURCE(id), RT_RCDATA);
    if (!hResInfo) {
        StrSwprintf(error, L"Error calling FindResourceA on resource id %u - Error is %lu", id, GetLastError());
        return false;
    }
    const HGLOBAL hRes = LoadResource(GWToolbox::GetDLLModule(), hResInfo);
    if (!hRes) {
        StrSwprintf(error, L"Error calling LoadResource on resource id %u - Error is %lu", id, GetLastError());
        return false;
    }
    const DWORD size = SizeofResource(GWToolbox::GetDLLModule(), hResInfo);
    if (!size) {
        StrSwprintf(error, L"Error calling SizeofResource on resource id %u - Error is %lu", id, GetLastError());
        return false;
    }
    // write to file so the user can customize his icons
    const HANDLE hFile = CreateFileW(path_to_file.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    DWORD bytesWritten;
    const BOOL wfRes = WriteFile(hFile, hRes, size, &bytesWritten, nullptr);
    if (wfRes != TRUE) {
        StrSwprintf(error, L"Error writing file %s - Error is %lu", TextUtils::PrintFilename(path_to_file.filename().wstring()).c_str(), GetLastError());
        return false;
    }
    if (bytesWritten != size) {
        StrSwprintf(error, L"Wrote %lu of %lu bytes for %s", bytesWritten, size, path_to_file.filename().wstring().c_str());
        return false;
    }

    CloseHandle(hFile);
    return true;
}

// Load from absolute file path on disk with 3 retries
int Resources::LoadIniFromFile(const std::filesystem::path& absolute_path, ToolboxIni* inifile)
{
    return inifile->LoadFile(absolute_path);
}

int Resources::SaveIniToFile(const std::filesystem::path& absolute_path, const ToolboxIni* ini)
{
    auto tmp_file = std::filesystem::path(absolute_path);
    tmp_file += ".tmp";
    const SI_Error res = ini->SaveFile(tmp_file.c_str());
    if (res < 0) {
        return res;
    }
    std::error_code ec;
    std::filesystem::rename(tmp_file, absolute_path, ec);
    if (ec.value() != 0) {
        return ec.value();
    }
    if (!(!exists(tmp_file) && exists(absolute_path))) {
        return -1; // rename failed
    }
    return 0;
}

void Resources::DxUpdate(IDirect3DDevice9* device)
{
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

void Resources::Update(float)
{
    main_mutex.lock();
    if (main_jobs.empty()) {
        main_mutex.unlock();
        return;
    }
    const auto func = std::move(main_jobs.front());
    main_jobs.pop();
    main_mutex.unlock();
    func();
}

IDirect3DTexture9** Resources::GetProfessionIcon(GW::Constants::Profession p)
{
    const auto prof_id = std::to_underlying(p);
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
        LoadTexture(texture, local_image, remote_image, [prof_id](const bool success, const std::wstring& error) {
            if (!success) {
                Log::ErrorW(L"Failed to load icon for profession %d\n%s", prof_id, error.c_str());
            }
        });
    }
    return texture;
}

IDirect3DTexture9** Resources::GetDamagetypeImage(std::string dmg_type)
{
    if (damagetype_icons.contains(dmg_type)) {
        return damagetype_icons.at(dmg_type);
    }
    const auto texture = new IDirect3DTexture9*;
    *texture = nullptr;
    damagetype_icons[dmg_type] = texture;
    if (damagetype_icon_urls.contains(dmg_type)) {
        const auto path = GetPath(DMGTYPE_ICONS_PATH);
        EnsureFolderExists(path);
        const auto local_path = path / TextUtils::SanitiseFilename(dmg_type + ".png");
        const auto remote_path = std::format("https://wiki.guildwars.com/images/thumb/{}", damagetype_icon_urls.at(dmg_type));
        LoadTexture(texture, local_path, remote_path, [dmg_type](const bool success, const std::wstring& error) {
            if (!success) {
                const auto dmg_type_wstr = TextUtils::StringToWString(dmg_type);
                Log::ErrorW(L"Failed to load icon for %d\n%s", dmg_type_wstr.c_str(), error.c_str());
            }
        });
    }
    return texture;
}

IDirect3DTexture9** Resources::GetGuildWarsWikiImage(const char* filename, size_t width)
{
    ASSERT(filename && filename[0]);
    std::string filename_on_disk;
    if (width > 0) {
        filename_on_disk = std::format("{}px_{}", width, filename);
    }
    else {
        filename_on_disk = filename;
    }
    const auto filename_sanitised = TextUtils::SanitiseFilename(filename_on_disk);
    if (guild_wars_wiki_images.contains(filename_sanitised)) {
        return guild_wars_wiki_images.at(filename_sanitised);
    }
    const auto callback = [filename_sanitised](const bool success, const std::wstring& error) {
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
    const auto path_to_file = std::format("{}\\{}", path.string(), filename_sanitised);
    // Check for local file
    if (std::filesystem::exists(path_to_file)) {
        LoadTexture(texture, path_to_file, callback);
        return texture;
    }
    // No local file found; download from wiki via skill link URL
    std::string wiki_url = "https://wiki.guildwars.com/wiki/File:";
    wiki_url.append(TextUtils::UrlEncode(filename, '_'));
    Instance().Download(wiki_url, [texture, filename_sanitised, callback, width](const bool ok, const std::string& response, void*) {
        if (!ok) {
            callback(ok, TextUtils::StringToWString(response));
            return; // Already logged whatever errors
        }

        // Find a valid png or jpg image inside the HTML response, <img alt="<skill_id>" src="<location_of_image>"
        std::regex image_finder(R"(class="fullMedia"[\s\S]*?href=['"]([^"']+))");
        std::smatch m;
        std::regex_search(response, m, image_finder);
        if (!m.size()) {
            trigger_failure_callback(callback, L"Regex failed loading file %S", filename_sanitised.c_str());
            return;
        }
        std::string image_url = m[1].str();
        const auto path_to_file2 = std::format("{}\\{}", path.string(), filename_sanitised);
        if (width) {
            // Divert to resized version using mediawiki's method
            image_finder = "/images/(.*)/([^/]+)$";
            std::regex_search(image_url, m, image_finder);
            if (!m.size()) {
                trigger_failure_callback(callback, L"Regex failed evaluating GWW thumbnail from %S", image_url.c_str());
                return;
            }
            image_url = std::format("/images/thumb/{}/{}/{}px-{}", m[1].str(), m[2].str(), width, m[2].str());
        }
        // https://wiki.guildwars.com/images/thumb/5/5c/Eternal_Protector_of_Tyria.jpg/150px-Eternal_Protector_of_Tyria.jpg
        if (!image_url.starts_with("http")) {
            image_url = std::format("https://wiki.guildwars.com{}", image_url);
        }
        LoadTexture(texture, path_to_file2, image_url, callback);
    });
    return texture;
}

#pragma comment(linker, "/EXPORT:?GetSkillImage@Resources@@SAPAPAUIDirect3DTexture9@@W4SkillID@Constants@GW@@@Z")

IDirect3DTexture9** Resources::GetSkillImage(GW::Constants::SkillID skill_id)
{
    const auto skill = GW::SkillbarMgr::GetSkillConstantData(skill_id);
    return skill && skill->icon_file_id ? GwDatTextureModule::LoadTextureFromFileId(skill->icon_file_id) : &empty_texture_ptr;
}

IDirect3DTexture9** Resources::GetSkillImageFromGWW(GW::Constants::SkillID skill_id)
{
    if (skill_images.contains(skill_id)) {
        return skill_images.at(skill_id);
    }
    const auto callback = [skill_id](const bool success, const std::wstring& error) {
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
    if (skill_id == static_cast<GW::Constants::SkillID>(0)) {
        return texture;
    }
    static std::filesystem::path path = GetPath(SKILL_IMAGES_PATH);
    if (!EnsureFolderExists(path)) {
        trigger_failure_callback(callback, L"Failed to create folder %s", path.wstring().c_str());
        return texture;
    }
    wchar_t path_to_file[MAX_PATH];
    // Check for local jpg file
    swprintf(path_to_file, _countof(path_to_file), L"%s\\%d.jpg", path.wstring().c_str(), skill_id);
    if (std::filesystem::exists(path_to_file)) {
        LoadTexture(texture, path_to_file, callback);
        return texture;
    }
    // Check for local png file
    swprintf(path_to_file, _countof(path_to_file), L"%s\\%d.png", path.wstring().c_str(), skill_id);
    if (std::filesystem::exists(path_to_file)) {
        LoadTexture(texture, path_to_file, callback);
        return texture;
    }
    // No local file found; download from wiki via skill link URL
    char url[128];
    snprintf(url, _countof(url), "https://wiki.guildwars.com/wiki/Game_link:Skill_%d", skill_id);
    Instance().Download(url, [texture, skill_id, callback](const bool ok, const std::string& response, void*) {
        if (!ok) {
            callback(ok, TextUtils::StringToWString(response));
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
        LoadTexture(texture, path_to_file, url, callback);
    });
    return texture;
}

GuiUtils::EncString* Resources::GetMapName(const GW::Constants::MapID map_id)
{
    const auto found = map_names.find(map_id);
    if (found != map_names.end()) {
        return found->second;
    }
    if (map_id == GW::Constants::MapID::None) {
        map_names[map_id] = DecodeStringId(0x3);
        return map_names[map_id];
    }
    const auto area = GW::Map::GetMapInfo(map_id);
    if (!(area && area->name_id)) {
        map_names[map_id] = DecodeStringId(0x3);
        return map_names[map_id];
    }
    map_names[map_id] = DecodeStringId(area->name_id);
    return map_names[map_id];
}

const wchar_t* Resources::GetRegionName(const GW::Constants::MapID map_id)
{
    const auto area_info = GW::Map::GetMapInfo(map_id);
    switch (area_info ? area_info->region : GW::Region_DevRegion) {
        case GW::Region_BattleIslands:
            return GW::EncStrings::MapRegion::BattleIsles;

        // Prophecies
        case GW::Region::Region_Maguuma:
            return GW::EncStrings::MapRegion::MaguumaJungle;
        case GW::Region::Region_Ascalon:
        case GW::Region::Region_Presearing:
            return GW::EncStrings::MapRegion::Ascalon;
        case GW::Region::Region_Kryta:
            return GW::EncStrings::MapRegion::Kryta;
        case GW::Region::Region_NorthernShiverpeaks: {
            // TODO: Southern vs northern shivers
            return GW::EncStrings::MapRegion::NorthernShiverpeaks;
        }
        case GW::Region_CrystalDesert:
            return GW::EncStrings::MapRegion::CrystalDesert;
        case GW::Region_FissureOfWoe: {
            // TODO: Ring of fire?
            // TODO: Underworld
            return GW::EncStrings::MapRegion::FissureOfWoe;
        }

        // Factions
        case GW::Region::Region_Kurzick:
            return GW::EncStrings::MapRegion::EchovaldForest;
        case GW::Region::Region_Luxon:
            return GW::EncStrings::MapRegion::TheJadeSea;
        case GW::Region::Region_ShingJea:
            return GW::EncStrings::MapRegion::ShingJeaIsland;
        case GW::Region::Region_Kaineng:
            return GW::EncStrings::MapRegion::KainengCity;

        // Nightfall
        case GW::Region::Region_Kourna:
            return GW::EncStrings::MapRegion::Kourna;
        case GW::Region::Region_Vaabi:
            return GW::EncStrings::MapRegion::Vabbi;
        case GW::Region::Region_Istan:
            return GW::EncStrings::MapRegion::Istan;
        case GW::Region::Region_DomainOfAnguish:
            return GW::EncStrings::MapRegion::RealmOfTorment;

        // Eye of the north
        case GW::Region::Region_CharrHomelands:
            return GW::EncStrings::MapRegion::CharrHomelands;
        case GW::Region::Region_DepthsOfTyria:
            return GW::EncStrings::MapRegion::DepthsOfTyria;
        case GW::Region::Region_FarShiverpeaks:
            return GW::EncStrings::MapRegion::FarShiverpeaks;
        case GW::Region::Region_TarnishedCoast:
            return GW::EncStrings::MapRegion::TarnishedCoast;
    }
    return L"\x108\107No region name yet :(\x1";
}

GuiUtils::EncString* Resources::DecodeStringId(const uint32_t enc_str_id, GW::Constants::Language language)
{
    if (language == (GW::Constants::Language)0xff)
        language = GW::UI::GetTextLanguage();
    const auto by_language = encoded_string_ids.find(language);
    if (by_language != encoded_string_ids.end()) {
        const auto found = by_language->second.find(enc_str_id);
        if (found != by_language->second.end())
            return found->second;
    }
    const auto enc_string = new GuiUtils::EncString(enc_str_id, false);
    encoded_string_ids[language][enc_str_id] = enc_string;
    return enc_string;
}

IDirect3DTexture9** Resources::GetItemImage(GW::Item* item)
{
    if (!(item && item->model_file_id))
        return nullptr;
    uint32_t model_id_to_load = 0;
    const bool is_composite_item = (item->interaction & 4) != 0;

    const bool is_female = true;

    if (is_composite_item) {
        // Armor/runes
        const auto model_file_info = GW::Items::GetCompositeModelInfo(item->model_file_id);
        if (model_file_info && !model_id_to_load)
            model_id_to_load = model_file_info->file_ids[0xa];
        if (model_file_info && !model_id_to_load)
            model_id_to_load = is_female ? model_file_info->file_ids[5] : model_file_info->file_ids[0];
    }
    if (!model_id_to_load)
        model_id_to_load = item->model_file_id;
    return GwDatTextureModule::LoadTextureFromFileId(model_id_to_load);
    // @Enhancement: How to apply dye_info to the result?
}

IDirect3DTexture9** Resources::GetItemImage(const std::wstring& item_name)
{
    if (item_name.empty()) {
        return nullptr;
    }
    if (item_images.contains(item_name)) {
        return item_images.at(item_name);
    }
    const auto callback = [item_name](const bool success, const std::wstring& error) {
        if (!success) {
            Log::LogW(L"Error: Failed to load item image %s\n%s", item_name.c_str(), error.c_str());
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
        LoadTexture(texture, path_to_file, callback);
        return texture;
    }

    // No local file found; download from wiki via searching by the item name; the wiki will usually return a 302 redirect if its an exact item match
    const std::string search_str = GuiUtils::WikiUrl(item_name);
    Instance().Download(search_str, [texture, item_name, callback](const bool ok, const std::string& response, void*) {
        if (!ok) {
            callback(ok, TextUtils::StringToWString(response));
            return;
        }
        const std::string item_name_str = TextUtils::WStringToString(item_name);
        // matches any characters that need to be escaped in RegEx
        static const std::regex specialChars{R"([-[\]{}()*+?.,\^$|#\s])"};
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
            const std::string html_item_name = TextUtils::HtmlEncode(m[1].str());
            snprintf(regex_str, sizeof(regex_str), R"(<img[^>]+alt=['"][^>]*%s[^>]*['"][^>]+src=['"]([^"']+)([.](png)))", html_item_name.c_str());
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
        LoadTexture(texture, path_to_file, url, callback);
    });
    return texture;
}
