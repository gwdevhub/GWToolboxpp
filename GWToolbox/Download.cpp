#include "stdafx.h"

#include <json.hpp>
using Json = nlohmann::json;

#include "Download.h"
#include "Path.h"
#include "Registry.h"

bool Download(std::string& content, const wchar_t *url)
{
    DeleteUrlCacheEntryW(url);

    IStream* stream;
    HRESULT result = URLOpenBlockingStreamW(NULL, url, &stream, 0, NULL);

    if (FAILED(result)) {
        fprintf(stderr, "URLOpenBlockingStreamW failed (HRESULT: 0x%X)\n", result);
        return false;
    }

    STATSTG stats;
    result = stream->Stat(&stats, STATFLAG_NONAME);
    if (FAILED(result)) {
        fprintf(stderr, "IStream::Stat failed (HRESULT: 0x%X)\n", result);
        stream->Release();
        return false;
    }

    if (stats.cbSize.HighPart != 0) {
        fprintf(stderr, "This file is way to big to download, it's over 4GB\n");
        stream->Release();
        return false;
    }

    content.resize(stats.cbSize.LowPart);
    result = stream->Read(&content[0], stats.cbSize.LowPart, NULL);
    stream->Release();

    if (FAILED(result)) {
        fprintf(stderr, "IStream::Read failed (HRESULT: 0x%X, Url: '%ls')\n", result, url);
        return false;
    }

    return true;
}

bool Download(const wchar_t *path_to_file, const wchar_t *url)
{
    DeleteUrlCacheEntryW(url);

    HRESULT result = URLDownloadToFileW(
        NULL,
        url,
        path_to_file,
        0,
        NULL);

    if (FAILED(result)) {
        fprintf(stderr, "URLDownloadToFileW failed: hresult:0x%X\n", result);
        return false;
    }

    return true;
}

struct Asset
{
    std::string name;
    size_t size;
    std::string browser_download_url;
};

struct Release
{
    std::string tag_name;
    std::string body;
    std::vector<Asset> assets;
};

static bool ParseRelease(std::string& json_text, Release *release)
{
    Json json;
    try {
        json = Json::parse(json_text.c_str());
    } catch(...) {
        fprintf(stderr, "Json::parse failed\n");
        return false;
    }

    auto it_tag_name = json.find("tag_name");
    if (it_tag_name == json.end() || !it_tag_name->is_string()) {
        fprintf(stderr, "Key 'tag_name' not found or not a string in '%s'\n", json_text.c_str());
        return false;
    }

    auto it_body = json.find("body");
    if (it_body == json.end() || !it_body->is_string()) {
        fprintf(stderr, "Key 'body' not found or not a string in '%s'\n", json_text.c_str());
        return false;
    }

    auto it_assets = json.find("assets");
    if (it_assets == json.end() || !it_assets->is_array()) {
        fprintf(stderr, "Key 'assets' not found or not an array in '%s'\n", json_text.c_str());
        return false;
    }

    release->tag_name = *it_tag_name;
    release->body = *it_body;

    for (size_t i = 0; i < it_assets->size(); i++) {
        Json& entry = it_assets->at(i);

        auto it_name = entry.find("name");
        if (it_name == entry.end() || !it_name->is_string()) {
            fprintf(stderr, "Key 'name' not found or not a string in (assert:%zu) '%s'\n",
                i, json_text.c_str());
            return false;
        }

        auto it_size = entry.find("size");
        if (it_size == entry.end() || !it_size->is_number()) {
            fprintf(stderr, "Key 'size' not found or not a number in (assert:%zu) '%s'\n",
                i, json_text.c_str());
            return false;
        }

        auto it_browser_download_url = entry.find("browser_download_url");
        if (it_browser_download_url == entry.end() || !it_browser_download_url->is_string()) {
            fprintf(stderr, "Key 'browser_download_url' not found or not a string in (assert:%zu) '%s'\n",
                i, json_text.c_str());
            return false;
        }

        Asset asset;
        asset.name = *it_name;
        asset.size = *it_size;
        asset.browser_download_url = *it_browser_download_url;

        release->assets.emplace_back(std::move(asset));
    }

    return true;
}

bool DownloadWindow::DownloadAllFiles()
{
    std::string content;
    if (!Download(content, L"https://api.github.com/repos/HasKha/GWToolboxpp/releases/latest")) {
        fprintf(stderr, "Couldn't download the latest release of GWToolboxpp\n");
        // @Remark:
        // We may not be able to grep Github api. (For instance, if we spam it)
        return true;
    }

    Release release;
    if (!ParseRelease(content, &release)) {
        fprintf(stderr, "ParseRelease failed\n");
        return false;
    }

    char release_string[256];
    if (RegReadRelease(release_string, ARRAYSIZE(release_string))) {
        if (_stricmp(release.tag_name.c_str(), release_string) == 0)
            return true;
    }

    // @Remark:
    // release_string could be uninitialize if "RegReadRelease" failed, so
    // don't used it!
    release_string[0] = 0;

    wchar_t dll_path[MAX_PATH];
    PathGetAppDataPath(dll_path, MAX_PATH, L"GWToolboxpp\\GWToolboxdll.dll");

    char buffer[64];
    snprintf(buffer, 64, "Downloading version '%s'", release.tag_name.c_str());
    MessageBoxA(0, buffer, "Downloading...", 0);

    // @Remark:
    // We need to properly update the progress bar

    DownloadWindow window;
    window.Create();

    for (size_t i = 0; i < release.assets.size(); i++) {
        Asset *asset = &release.assets[i];
        if (asset->name == "GWToolbox.dll") {
            fprintf(stderr, "browser_download_url: '%s'\n", asset->browser_download_url.c_str());
            const char *purl = asset->browser_download_url.c_str();
            // @Cleanup: Utf8 -> Unicode (WCHAR)
            std::wstring url(purl, purl + asset->browser_download_url.size());
            if (!Download(dll_path, url.c_str())) {
                fprintf(stderr, "Download failed: path_to_file = '%ls', url = '%ls'\n",
                    dll_path, url.c_str());
                return false;
            }
            SendMessageW(window.m_hProgressBar, PBM_SETPOS, 100, 0);
        }
    }

    window.WaitMessages();

    RegWriteRelease(release.tag_name.c_str(), release.tag_name.size());
    return true;
}

void DownloadWindow::OnWindowCreate(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    LPCREATESTRUCTW param = reinterpret_cast<LPCREATESTRUCTW>(lParam);
    DownloadWindow *window = static_cast<DownloadWindow *>(param->lpCreateParams);

    SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG>(window));

    window->m_hProgressBar = CreateWindowW(
        PROGRESS_CLASSW,
        L"Inject",
        WS_VISIBLE | WS_CHILD,
        5,
        44,
        475,
        12,
        hWnd,
        nullptr,
        window->m_hInstance,
        nullptr);

    SendMessageW(window->m_hProgressBar, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
    SendMessageW(window->m_hProgressBar, PBM_SETPOS, 0, 0);
}

LRESULT CALLBACK DownloadWindow::MainWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    LONG_PTR ud = GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    DownloadWindow *window = reinterpret_cast<DownloadWindow *>(ud);

    switch (uMsg)
    {
    case WM_CREATE:
        DownloadWindow::OnWindowCreate(hWnd, uMsg, wParam, lParam);
        break;

    default:
        return window->WndProc(hWnd, uMsg, wParam, lParam);
    }

    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

DownloadWindow::DownloadWindow()
    : m_hWnd(nullptr)
    , m_hEvent(nullptr)
    , m_hInstance(nullptr)
{
}

DownloadWindow::~DownloadWindow()
{
    if (m_hEvent)
        CloseHandle(m_hEvent);
}

bool DownloadWindow::Create()
{
    m_hInstance = GetModuleHandleW(nullptr);

    m_hEvent = CreateEventW(0, FALSE, FALSE, nullptr);
    if (m_hEvent == nullptr)
    {
        fprintf(stderr, "CreateEventW failed (%lu)\n", GetLastError());
        return false;
    }

    WNDCLASSW wc = {0};
    // wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = MainWndProc;
    wc.hInstance = m_hInstance;
    wc.hbrBackground = GetSysColorBrush(COLOR_3DFACE);
    wc.lpszClassName = L"GWToolbox-Download-Window-Class";

    if (!RegisterClassW(&wc))
    {
        fprintf(stderr, "RegisterClassW failed (%lu)\n", GetLastError());
        return false;
    }

    m_hWnd = CreateWindowW(
        wc.lpszClassName,
        L"Download",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        CW_USEDEFAULT, // x
        CW_USEDEFAULT, // y
        500, // width
        100, // height
        nullptr,
        nullptr,
        m_hInstance,
        this);

    if (m_hWnd == nullptr)
    {
        fprintf(stderr, "CreateWindowW failed (%lu)\n", GetLastError());
        return false;
    }

    return true;
}

bool DownloadWindow::WaitMessages()
{
    MSG msg;
    for (;;)
    {
        DWORD dwRet = MsgWaitForMultipleObjects(
            1,
            &m_hEvent,
            FALSE,
            INFINITE,
            QS_ALLINPUT);

        if (dwRet == WAIT_OBJECT_0)
            break;

        if (dwRet == WAIT_FAILED)
        {
            fprintf(stderr, "MsgWaitForMultipleObjects failed (%lu)\n", GetLastError());
            return false;
        }

        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    return true;
}

LRESULT DownloadWindow::WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CLOSE:
        DestroyWindow(hWnd);
        break;

    case WM_DESTROY:
        SetEvent(m_hEvent);
        break;

    case WM_KEYUP:
        if (wParam == VK_ESCAPE)
            DestroyWindow(hWnd);
        if (wParam == 'L')
            SendMessageW(m_hProgressBar, PBM_DELTAPOS, 10, 0);
        break;
    }

    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}
