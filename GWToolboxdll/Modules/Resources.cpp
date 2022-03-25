#include "stdafx.h"

#include <d3dx9_dynamic.h>

#include <Defines.h>
#include <GWToolbox.h>
#include <Logger.h>

#include <GuiUtils.h>

#include <RestClient.h>

#include <Modules/Resources.h>
#include <GWCA/GameEntities/Item.h>
#include <Timer.h>

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

    char* profession_icon_urls[] = {
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
    std::map<uint32_t, IDirect3DTexture9**> skill_images;
    std::map<std::wstring, IDirect3DTexture9**> item_images;
    std::map<uint32_t, IDirect3DTexture9**> profession_icons;
    std::string ns;
    const size_t MAX_WORKERS = 5;
    const wchar_t* SKILL_IMAGES_PATH = L"img\\skills";
    const wchar_t* ITEM_IMAGES_PATH = L"img\\items";
    const wchar_t* PROF_ICONS_PATH = L"img\\professions";

    // snprintf to a std::string (using a copy) - return false for chaining. Log to console if out is nullptr.
    int wstring_printf(std::wstring* out = 0, const wchar_t* format = 0, ...) {
        wchar_t* err_buf = new wchar_t[512];
        int written = -1;
        va_list vl;
        va_start(vl, format);
        written = swprintf(err_buf, 512, format, vl);
        va_end(vl);
        ASSERT(written != -1);
        if (out) {
            out->assign(err_buf);
        }
        else {
            Log::LogW(err_buf);
        }
        delete[] err_buf;
        return written;
    }
    int string_printf(std::string* out = 0, const char* format = 0, ...) {
        char* err_buf = new char[512];
        int written = -1;
        va_list vl;
        va_start(vl, format);
        written = snprintf(err_buf, 512, format, vl);
        va_end(vl);
        ASSERT(written != -1);
        if (out) {
            out->assign(err_buf);
        }
        else {
            Log::Log(err_buf);
        }
        delete[] err_buf;
        return written;
    }

    // snprintf error message, pass to callback as a failure. Used internally.
    void trigger_failure_callback(std::function<void(bool,const std::wstring&)> callback,const wchar_t* format, ...) {
        wchar_t* err_buf = new wchar_t[MAX_PATH];
        int written = -1;
        va_list vl;
        va_start(vl, format);
        written = swprintf(err_buf, MAX_PATH, format, vl);
        va_end(vl);
        ASSERT(written != -1);
        callback(false, err_buf);
        delete[] err_buf;
    };

}

Resources::Resources() {
    InitCurl();
}
Resources::~Resources() {
    Cleanup();
    ShutdownCurl();
    for (auto it : skill_images) {
        delete it.second;
    }
    skill_images.clear();
    for (auto it : item_images) {
        delete it.second;
    }
    item_images.clear();
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
    WCHAR path[MAX_PATH];
    SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA | CSIDL_FLAG_CREATE, NULL, 0, path);
    return std::filesystem::path(path) / "GWToolboxpp";
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
    auto err = [response](const wchar_t* format, ...) {
        va_list vl;
        va_start(vl, format);
        if (response) {
            wstring_printf(response, format, vl);
        }
        else {
            Log::ErrorW(format, vl);
        }
        va_end(vl);
        return false;
    };
    if (std::filesystem::exists(path_to_file)) {
        if (!std::filesystem::remove(path_to_file)) {
            return err(L"Failed to delete existing file %s, err %d", path_to_file.wstring().c_str(), GetLastError());
        }
    }
    if (std::filesystem::exists(path_to_file)) {
        return err(L"File already exists @ %s", path_to_file.wstring().c_str());
    }

    std::string content;
    if (!Download(url, &content)) {
        return err(L"%S", content.c_str());
    }
    if (!content.length()) {
        return err(L"Failed to download %S, no content length", url.c_str());
    }
    FILE* fp = fopen(path_to_file.string().c_str(), "wb");
    if (!fp) {
        return err(L"Failed to call fopen for %s, err %d", path_to_file.wstring().c_str(), GetLastError());
    }
    int written = fwrite(content.data(), content.size() + 1, 1, fp);
    fclose(fp);
    if(written != 1) {
        return err(L"Failed to call fwrite for %s, err %d", path_to_file.wstring().c_str(), GetLastError());
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
        string_printf(response, "Failed to download %s, curl status %d %s", url.c_str(), r.GetStatusCode(), r.GetStatusStr());
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
        wstring_printf(error, L"Error loading resource from file %s - Error is %S", path_to_file.filename().wstring().c_str(), d3dErrorMessage(res));
    }
    else if (!*texture) {
        res = D3DERR_NOTFOUND;
        wstring_printf(error, L"Error loading resource from file %s - texture loaded is null", path_to_file.filename().wstring().c_str());
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
        wstring_printf(error, L"Error loading resource for id %S, module %p - Error is %S", id, hSrcModule, d3dErrorMessage(res));
    }
    else if (!*texture) {
        res = D3DERR_NOTFOUND;
        wstring_printf(error, L"Error loading resource for id %S, module %p - texture loaded is null", id, hSrcModule);
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
        wstring_printf(error, L"Error calling FindResourceA on resource id %u - Error is %lu", id, GetLastError());
        return false;
    }
    HGLOBAL hRes = LoadResource(GWToolbox::GetDLLModule(), hResInfo);
    if (!hRes) {
        wstring_printf(error, L"Error calling LoadResource on resource id %u - Error is %lu", id, GetLastError());
        return false;
    }
    DWORD size = SizeofResource(GWToolbox::GetDLLModule(), hResInfo);
    if (!size) {
        wstring_printf(error, L"Error calling SizeofResource on resource id %u - Error is %lu", id, GetLastError());
        return false;
    }
    // write to file so the user can customize his icons
    HANDLE hFile = CreateFileW(path_to_file.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD bytesWritten;
    BOOL wfRes = WriteFile(hFile, hRes, size, &bytesWritten, NULL);
    if (wfRes != TRUE) {
        wstring_printf(error, L"Error writing file %s - Error is %lu", path_to_file.filename().wstring().c_str(), GetLastError());
        return false;
    }
    else if (bytesWritten != size) {
        wstring_printf(error, L"Wrote %lu of %lu bytes for %s", bytesWritten, size, path_to_file.filename().wstring().c_str());
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
// Fetches skill page from GWW, parses out the image for the skill then downloads that to disk
// Not elegent, but without a proper API to provide images, and to avoid including libxml, this is the next best thing.
IDirect3DTexture9** Resources::GetSkillImage(uint32_t skill_id) {
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
        ASSERT(snprintf(regex_str, sizeof(regex_str), "<img[^>]+alt=['\"].*%d.*['\"][^>]+src=['\"]([^\"']+)([.](png|jpg))", skill_id) != -1);
        const std::regex image_finder(regex_str);
        std::smatch m;
        if (!std::regex_search(response, m, image_finder)) {
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
    char url[128];
    std::string search_str = GuiUtils::UrlEncode(GuiUtils::WStringToString(GuiUtils::RemoveDiacritics(item_name)));
    snprintf(url, _countof(url), "https://wiki.guildwars.com/index.php?search=%s", search_str.c_str());
    Instance().Download(url, [texture, item_name, callback](bool ok, const std::string& response) {
        if (!ok) {
            callback(ok, GuiUtils::StringToWString(response));
            return;
        }
        std::string item_name_str = GuiUtils::WStringToString(item_name);
        std::smatch m;
        // Find first png image that has an alt tag matching the html encoded title of the page
        char regex_str[255];
        snprintf(regex_str, sizeof(regex_str), "<img[^>]+alt=['\"][^>]*%s[^>]*['\"][^>]+src=['\"]([^\"']+)([.](png))", item_name_str.c_str());
        if (!std::regex_search(response, m, std::regex(regex_str))) {
            // Failed to find via item name; try via page title
            const std::regex title_finder("<title>(.*) - Guild Wars Wiki.*</title>");
            if (!std::regex_search(response, m, title_finder)) {
                trigger_failure_callback(callback, L"Failed to find title HTML for %s from wiki", item_name.c_str());
                return;
            }
            std::string html_item_name = GuiUtils::HtmlEncode(m[1].str());
            snprintf(regex_str, sizeof(regex_str), "<img[^>]+alt=['\"][^>]*%s[^>]*['\"][^>]+src=['\"]([^\"']+)([.](png))", item_name_str.c_str());
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


