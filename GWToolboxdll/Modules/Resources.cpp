#include "stdafx.h"

#include <d3dx9_dynamic.h>

#include <GWCA/GameEntities/Item.h>
#include <GWCA/GameEntities/Map.h>

#include <GWCA/Managers/MapMgr.h>


#include <Defines.h>
#include <GWToolbox.h>
#include <Logger.h>

#include <Utils/GuiUtils.h>

#include <RestClient.h>

#include <Modules/Resources.h>

#include <Timer.h>
#include <Path.h>
#include <Str.h>

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
        }
        static char out[32];
        snprintf(out, 32, "Unknown D3D error %#08x", code);
        return out;
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
    std::map<std::string, IDirect3DTexture9**> armor_images;
    std::map<uint32_t, IDirect3DTexture9**> profession_icons;
    std::map<GW::Constants::MapID, GuiUtils::EncString*> map_names;
    std::string ns;
    const size_t MAX_WORKERS = 5;
    const wchar_t* ARMOR_GALLERY_PATH = L"img\\armor_gallery";
    const wchar_t* SKILL_IMAGES_PATH = L"img\\skills";
    const wchar_t* ITEM_IMAGES_PATH = L"img\\items";
    const wchar_t* PROF_ICONS_PATH = L"img\\professions";

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

}

Resources::Resources() {
    InitCurl();
}
Resources::~Resources() {
    Cleanup();
    ShutdownCurl();
    for (const auto& it : skill_images) {
        delete it.second;
    }
    skill_images.clear();
    for (const auto& it : item_images) {
        delete it.second;
    }
    item_images.clear();
    for (const auto& it : map_names) {
        delete it.second;
    }
    map_names.clear();
};

void Resources::InitRestClient(RestClient* r) {
    char user_agent_str[32];
    ASSERT(snprintf(user_agent_str, sizeof(user_agent_str), "GWToolboxpp/%s", GWTOOLBOXDLL_VERSION) != -1);
    r->SetUserAgent(user_agent_str);
    r->SetFollowLocation(true);
    r->SetVerifyPeer(false); // idc about mitm or out of date certs
    r->SetMethod(HttpMethod::Get);
    r->SetVerifyPeer(false);
}

HRESULT Resources::ResolveShortcut(std::filesystem::path& in_shortcut_path, std::filesystem::path& out_actual_path)
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
    WIN32_FIND_DATA wfd;

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
    psl->QueryInterface(IID_IPersistFile, (void**)&ppf);

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
}
void Resources::Terminate() {
    ToolboxModule::Terminate();
    Cleanup();
}

void Resources::EndLoading() {
    EnqueueWorkerTask([this]() { should_stop = true; });
}

std::filesystem::path Resources::GetSettingsFolderPath()
{
    std::filesystem::path computer_name;
    ASSERT(PathGetComputerName(computer_name));

    std::filesystem::path docpath;
    ASSERT(PathGetDocumentsPath(docpath, L"GWToolboxpp"));
    docpath = docpath / computer_name;

    bool result;
    ASSERT(PathExistsSafe(docpath / "GWToolbox.ini", &result));
    if (result) {
        return docpath;
    }

    if (PathCreateDirectorySafe(docpath)) {
        return docpath;
    }

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
    return std::filesystem::exists(path) || std::filesystem::create_directory(path);
}

utf8::string Resources::GetPathUtf8(std::wstring file) {
    std::wstring path = GetPath(file);
    return Unicode16ToUtf8(path.c_str());
}

bool Resources::Download(const std::filesystem::path& path_to_file, const std::string& url, std::wstring* response) {
    if (std::filesystem::exists(path_to_file)) {
        if (!std::filesystem::remove(path_to_file)) {
            return StrSwprintf(*response, L"Failed to delete existing file %s, err %d", path_to_file.wstring().c_str(), GetLastError()), false;
        }
    }
    if (std::filesystem::exists(path_to_file)) {
        return StrSwprintf(*response, L"File already exists @ %s", path_to_file.wstring().c_str()), false;
    }

    std::string content;
    if (!Download(url, &content)) {
        return StrSwprintf(*response,L"%S", content.c_str()), false;
    }
    if (!content.length()) {
        return StrSwprintf(*response,L"Failed to download %S, no content length", url.c_str()), false;
    }
    FILE* fp = fopen(path_to_file.string().c_str(), "wb");
    if (!fp) {
        return StrSwprintf(*response,L"Failed to call fopen for %s, err %d", path_to_file.wstring().c_str(), GetLastError()), false;
    }
    int written = fwrite(content.data(), content.size() + 1, 1, fp);
    fclose(fp);
    if(written != 1) {
        return StrSwprintf(*response,L"Failed to call fwrite for %s, err %d", path_to_file.wstring().c_str(), GetLastError()), false;
    }
    return true;
}
void Resources::Download(
    const std::filesystem::path& path_to_file, const std::string& url, AsyncLoadCallback callback)
{
    EnqueueWorkerTask([this,path_to_file,url,callback]() {
        std::wstring* error_message = new std::wstring();
        bool success = Download(path_to_file, url, error_message);
        // and call the callback in the main thread
        if (callback) {
            EnqueueMainTask([callback, success, error_message]() {
                callback(success, *error_message);
                delete error_message;
                });
        }
        else if (!success) {
            Log::LogW(L"Failed to download %s from %S\n%S", path_to_file.wstring().c_str(), url.c_str(), error_message->c_str());
            delete error_message;
        }

        });
}

bool Resources::Download(const std::string& url, std::string* response)
{
    RestClient r;
    InitRestClient(&r);
    r.SetUrl(url.c_str());
    r.Execute();
    if (!r.IsSuccessful()) {
        StrSprintf(*response, "Failed to download %s, curl status %d %s", url.c_str(), r.GetStatusCode(), r.GetStatusStr());
        return false;
    }
    *response = std::move(r.GetContent());
    return true;
}
void Resources::Download(const std::string& url, AsyncLoadMbCallback callback)
{
    EnqueueWorkerTask([this,url,callback]() {
        std::string* response = new std::string();
        bool ok = Download(url, response);
        EnqueueMainTask([callback, ok, response]() {
            callback(ok, *response);
            delete response;
            });
    });
}

void Resources::EnsureFileExists(
    const std::filesystem::path& path_to_file, const std::string& url, AsyncLoadCallback callback)
{
    if (std::filesystem::exists(path_to_file)) {
        // if file exists, run the callback immediately in the same thread
        callback(true, L"");
    } else {
        // otherwise try to download it in the worker
        Download(path_to_file, url, callback);
    }
}

HRESULT Resources::TryCreateTexture(IDirect3DDevice9* device, const std::filesystem::path& path_to_file, IDirect3DTexture9** texture, std::wstring* error) {
    // NB: Some Graphics cards seem to spit out D3DERR_NOTAVAILABLE when loading textures, haven't figured out why but retry if this error is reported

    HRESULT res = D3DERR_NOTAVAILABLE;
    size_t tries = 0;
    do {
        tries++;
        res = D3DXCreateTextureFromFileExW(device, path_to_file.c_str(), D3DX_DEFAULT, D3DX_DEFAULT, D3DX_DEFAULT, 0, D3DFMT_UNKNOWN, D3DPOOL_MANAGED, D3DX_DEFAULT, D3DX_DEFAULT, 0, NULL, NULL, texture);
    } while (res == D3DERR_NOTAVAILABLE && tries < 3);
    if (res != D3D_OK) {
        StrSwprintf(*error, L"Error loading resource from file %s - Error is %S", path_to_file.filename().wstring().c_str(), d3dErrorMessage(res));
    }
    else if (!*texture) {
        res = D3DERR_NOTFOUND;
        StrSwprintf(*error, L"Error loading resource from file %s - texture loaded is null", path_to_file.filename().wstring().c_str());
    }
    return res;
}
HRESULT Resources::TryCreateTexture(IDirect3DDevice9* device, HMODULE hSrcModule, LPCSTR id, IDirect3DTexture9** texture, std::wstring* error) {
    // NB: Some Graphics cards seem to spit out D3DERR_NOTAVAILABLE when loading textures, haven't figured out why but retry if this error is reported
    HRESULT res = D3DERR_NOTAVAILABLE;
    size_t tries = 0;
    do {
        tries++;
        res = D3DXCreateTextureFromResourceExA(device, hSrcModule, id, D3DX_DEFAULT, D3DX_DEFAULT, D3DX_DEFAULT, 0, D3DFMT_UNKNOWN, D3DPOOL_MANAGED, D3DX_DEFAULT, D3DX_DEFAULT, 0, NULL, NULL, texture);
    } while (res == D3DERR_NOTAVAILABLE && tries < 3);
    if (res != D3D_OK) {
        StrSwprintf(*error, L"Error loading resource for id %p, module %p - Error is %S", id, hSrcModule, d3dErrorMessage(res));
    }
    else if (!*texture) {
        res = D3DERR_NOTFOUND;
        StrSwprintf(*error, L"Error loading resource for id %p, module %p - texture loaded is null", id, hSrcModule);
    }
    return res;
}
void Resources::LoadTexture(IDirect3DTexture9** texture, const std::filesystem::path& path_to_file, AsyncLoadCallback callback)
{
    EnqueueDxTask([path_to_file, texture, callback](IDirect3DDevice9* device) {
        std::wstring* error = new std::wstring();
        bool success = TryCreateTexture(device, path_to_file.c_str(), texture, error) == D3D_OK;
        if (callback) {
            callback(success, *error);
        }
        else if (!success) {
            Log::LogW(L"Failed to load texture from file %s\n%s", path_to_file.wstring().c_str(), error->c_str());
        }
        delete error;
        });
}
void Resources::LoadTexture(IDirect3DTexture9** texture, WORD id, AsyncLoadCallback callback)
{
    EnqueueDxTask([id, texture, callback](IDirect3DDevice9* device) {
        std::wstring* error = new std::wstring();
        bool success = TryCreateTexture(device, GWToolbox::GetDLLModule(), MAKEINTRESOURCE(id), texture, error) == D3D_OK;
        if (callback) {
            callback(success, *error);
        }
        else if (!success) {
            Log::LogW(L"Failed to load texture from id %d\n%s", id, error->c_str());
        }
        delete error;
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
bool Resources::ResourceToFile(WORD id, const std::filesystem::path& path_to_file, std::wstring* error) {
    // otherwise try to install it from resource
    HRSRC hResInfo = FindResourceA(GWToolbox::GetDLLModule(), MAKEINTRESOURCE(id), RT_RCDATA);
    if (!hResInfo) {
        StrSwprintf(*error, L"Error calling FindResourceA on resource id %u - Error is %lu", id, GetLastError());
        return false;
    }
    HGLOBAL hRes = LoadResource(GWToolbox::GetDLLModule(), hResInfo);
    if (!hRes) {
        StrSwprintf(*error, L"Error calling LoadResource on resource id %u - Error is %lu", id, GetLastError());
        return false;
    }
    DWORD size = SizeofResource(GWToolbox::GetDLLModule(), hResInfo);
    if (!size) {
        StrSwprintf(*error, L"Error calling SizeofResource on resource id %u - Error is %lu", id, GetLastError());
        return false;
    }
    // write to file so the user can customize his icons
    HANDLE hFile = CreateFileW(path_to_file.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD bytesWritten;
    BOOL wfRes = WriteFile(hFile, hRes, size, &bytesWritten, NULL);
    if (wfRes != TRUE) {
        StrSwprintf(*error, L"Error writing file %s - Error is %lu", path_to_file.filename().wstring().c_str(), GetLastError());
        return false;
    }
    else if (bytesWritten != size) {
        StrSwprintf(*error, L"Wrote %lu of %lu bytes for %s", bytesWritten, size, path_to_file.filename().wstring().c_str());
        return false;
    }

    CloseHandle(hFile);
    return true;
}


void Resources::DxUpdate(IDirect3DDevice9* device) {
    while (true) {
        dx_mutex.lock();
        if (dx_jobs.empty()) {
            dx_mutex.unlock();
            return;
        }
        std::function<void(IDirect3DDevice9*)> func = dx_jobs.front();
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
    std::function<void()> func = main_jobs.front();
    main_jobs.pop();
    main_mutex.unlock();
    func();
}
void Resources::WorkerUpdate() {
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

IDirect3DTexture9** Resources::GetProfessionIcon(GW::Constants::Profession p) {
    uint32_t prof_id = (uint32_t)p;
    auto found = profession_icons.find(prof_id);
    if (found != profession_icons.end()) {
        return found->second;
    }
    IDirect3DTexture9** texture = (IDirect3DTexture9**)malloc(sizeof(IDirect3DTexture9*));
    *texture = 0;
    profession_icons[prof_id] = texture;
    if (profession_icon_urls[prof_id][0]) {
        auto path = Resources::GetPath(PROF_ICONS_PATH);
        Resources::EnsureFolderExists(path);
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


IDirect3DTexture9** Resources::GetSkillImage(GW::Constants::SkillID skill_id) {

    auto found = skill_images.find(skill_id);
    if (found != skill_images.end()) {
        return found->second;
    }
    const auto callback = [skill_id](bool success, const std::wstring& error) {
        if (!success) {
            Log::ErrorW(L"Failed to load skill image %d\n%s", skill_id, error.c_str());
        }
        else {
            Log::LogW(L"Loaded skill image %d", skill_id);
        }
    };
    IDirect3DTexture9** texture = (IDirect3DTexture9**)malloc(sizeof(IDirect3DTexture9*));
    *texture = 0;
    skill_images[skill_id] = texture;
    if (skill_id == (GW::Constants::SkillID)0)
        return texture;
    static std::filesystem::path path = Resources::GetPath(SKILL_IMAGES_PATH);
    if (!Resources::EnsureFolderExists(path)) {
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
    auto found = map_names.find(map_id);
    if (found != map_names.end()) {
        return found->second;
    }
    GW::AreaInfo* area = GW::Map::GetMapInfo(map_id);
    ASSERT(area);
    GuiUtils::EncString* enc_string = new GuiUtils::EncString(area->name_id, false);
    map_names[map_id] = enc_string;
    return enc_string;
}

IDirect3DTexture9** Resources::GetItemImage(const std::wstring& item_name) {
    if (item_name.empty()) {
        return nullptr;
    }
    auto found = item_images.find(item_name);
    if (found != item_images.end()) {
        return found->second;
    }
    const auto callback = [item_name](bool success, const std::wstring& error) {
        if (!success) {
            Log::ErrorW(L"Failed to load item image %s\n%s", item_name.c_str(), error.c_str());
        }
        else {
            Log::LogW(L"Loaded item image %s", item_name.c_str());
        }
    };
    IDirect3DTexture9** texture = (IDirect3DTexture9**)malloc(sizeof(IDirect3DTexture9*));
    *texture = 0;
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
    std::string search_str = GuiUtils::WikiUrl(item_name);
    Instance().Download(search_str, [texture, item_name, callback](bool ok, const std::string& response) {
        if (!ok) {
            callback(ok, GuiUtils::StringToWString(response));
            return;
        }
        std::string item_name_str = GuiUtils::WStringToString(item_name);
        // matches any characters that need to be escaped in RegEx
        std::regex specialChars{ R"([-[\]{}()*+?.,\^$|#\s])" };
        std::string sanitized = std::regex_replace(item_name_str, specialChars, R"(\$&)");
        std::smatch m;
        // Find first png image that has an alt tag matching the html encoded title of the page
        char regex_str[255];
        snprintf(regex_str, sizeof(regex_str), "<img[^>]+alt=['\"][^>]*%s[^>]*['\"][^>]+src=['\"]([^\"']+)([.](png))", sanitized.c_str());
        if (!std::regex_search(response, m, std::regex(regex_str))) {
            // Failed to find via item name; try via page title
            const std::regex title_finder("<title>(.*) - Guild Wars Wiki.*</title>");
            if (!std::regex_search(response, m, title_finder)) {
                trigger_failure_callback(callback, L"Failed to find title HTML for %s from wiki", item_name.c_str());
                return;
            }
            std::string html_item_name = GuiUtils::HtmlEncode(m[1].str());
            snprintf(regex_str, sizeof(regex_str), "<img[^>]+alt=['\"][^>]*%s[^>]*['\"][^>]+src=['\"]([^\"']+)([.](png))", html_item_name.c_str());
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

IDirect3DTexture9** Resources::GetArmorArt(const char* armor_name, GW::Constants::Profession p, char gender) {
    uint32_t profession_uint = static_cast<uint32_t>(p);
    const char* profession_names[] = {
        "None",
        "Warrior",
        "Ranger",
        "Monk",
        "Necromancer",
        "Mesmer",
        "Elementalist",
        "Assassin",
        "Ritualist",
        "Paragon",
        "Dervish"
    };
    const char* profession_name = profession_names[profession_uint];
    size_t armor_name_len = strlen(armor_name) + strlen(profession_name) + 8;
    char* armor_name_full = new char[armor_name_len];
    snprintf(armor_name_full, armor_name_len, "%s %s %c", profession_name, armor_name, gender);

    auto found = armor_images.find(armor_name_full);
    if (found != armor_images.end()) {
        delete[] armor_name_full;
        return found->second;
    }
    IDirect3DTexture9** texture = (IDirect3DTexture9**)malloc(sizeof(IDirect3DTexture9*));
    *texture = 0;
    armor_images[armor_name_full] = texture;
    if (profession_uint == 0) {
        delete[] armor_name_full;
        return texture;
    }
    const auto callback = [armor_name_full](bool success, const std::wstring& error) {
        if (!success) {
            Log::ErrorW(L"Failed to load armor image %S\n%s", armor_name_full, error.c_str());
        }
        else {
            Log::LogW(L"Loaded armor image %s", armor_name_full);
        }
        delete[] armor_name_full;
    };


    static std::filesystem::path path = Resources::GetPath(ARMOR_GALLERY_PATH);
    if (!Resources::EnsureFolderExists(path)) {
        trigger_failure_callback(callback, L"Failed to create folder %s", path.wstring().c_str());
        return texture;
    }
    wchar_t path_to_file[MAX_PATH];
    // Check for local jpg file
    swprintf(path_to_file, _countof(path_to_file), L"%s\\%S.jpg", path.wstring().c_str(), armor_name_full);
    if (std::filesystem::exists(path_to_file)) {
        Instance().LoadTexture(texture, path_to_file, callback);
        return texture;
    }
    // Check for local png file
    swprintf(path_to_file, _countof(path_to_file), L"%s\\%S.png", path.wstring().c_str(), armor_name_full);
    if (std::filesystem::exists(path_to_file)) {
        Instance().LoadTexture(texture, path_to_file, callback);
        return texture;
    }
    // No local file found; download from wiki via skill link URL
    std::string armor_name_wiki_term = GuiUtils::UrlEncode(armor_name, '_');
    char url[128];
    snprintf(url, _countof(url), "https://wiki.guildwars.com/wiki/%s", armor_name_wiki_term.c_str());
    Instance().Download(url, [texture, armor_name_full, callback](bool ok, const std::string& response) {
        if (!ok) {
            callback(ok, GuiUtils::StringToWString(response));
            return; // Already logged whatever errors
        }

        char regex_str[255];
        // Find a valid png or jpg image inside the HTML response, <img alt="<skill_id>" src="<location_of_image>"
        ASSERT(snprintf(regex_str, sizeof(regex_str), "alt=\"%s\\.(?:jpg|png)\" src=['\"]([^\"']+)([.](png|jpg))", armor_name_full) != -1);
        std::regex image_finder(regex_str, std::regex_constants::icase);
        std::smatch m;
        std::regex_search(response, m, image_finder);
        if (!m.size()) {
            trigger_failure_callback(callback, L"Regex failed loading armor art for %S", armor_name_full);
            return;
        }
        const std::string& image_path = m[1].str();
        const std::string& image_extension = m[2].str();
        wchar_t path_to_file[MAX_PATH];
        ASSERT(swprintf(path_to_file, _countof(path_to_file), L"%s\\%S%S", path.wstring().c_str(), armor_name_full, image_extension.c_str()) != -1);
        char url[128];

        if (strncmp(image_path.c_str(), "http", 4) == 0) {
            // Image URL is absolute
            if (snprintf(url, _countof(url), "%s%s", image_path.c_str(), image_extension.c_str()) == -1) {
                trigger_failure_callback(callback, L"Failed to snprintf url");
                return;
            }
        }
        else {
            // Image URL is relative to domain
            if (snprintf(url, _countof(url), "https://wiki.guildwars.com%s%s", image_path.c_str(), image_extension.c_str()) == -1) {
                trigger_failure_callback(callback, L"Failed to snprintf url");
                return;
            }
        }
        Instance().LoadTexture(texture, path_to_file, url, callback);
        });
    return texture;
}

