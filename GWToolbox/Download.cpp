#include "stdafx.h"

using Json = nlohmann::json;

#include "Download.h"
#include "Path.h"
#include "Registry.h"

class DownloadStatusCallback : public IBindStatusCallback
{
public:
    DownloadStatusCallback(Window *window, HWND hWnd)
        : m_hWnd(hWnd)
        , m_Window(window)
    {
    }

private:
    HWND m_hWnd;
    Window *m_Window;

private: // from IBindStatusCallback
    HRESULT __stdcall QueryInterface(const IID&, void **) override
    {
        return E_NOINTERFACE;
    }

    ULONG __stdcall AddRef(void) override
    {
        return 1;
    }

    ULONG __stdcall Release(void) override
    {
        return 1;
    }

    HRESULT __stdcall OnStartBinding(DWORD, IBinding *) override
    {
        return E_NOTIMPL;
    }

    HRESULT __stdcall GetPriority(LONG *) override
    {
        return E_NOTIMPL;
    }

    HRESULT __stdcall OnLowResource(DWORD) override
    {
        return S_OK;
    }

    HRESULT __stdcall OnStopBinding(HRESULT, LPCWSTR) override
    {
        return E_NOTIMPL;
    }

    HRESULT __stdcall GetBindInfo(DWORD *, BINDINFO *) override
    {
        return E_NOTIMPL;
    }

    HRESULT __stdcall OnDataAvailable(DWORD, DWORD, FORMATETC *, STGMEDIUM *) override
    {
        return E_NOTIMPL;
    }

    HRESULT __stdcall OnObjectAvailable(REFIID, IUnknown *) override
    {
        return E_NOTIMPL;
    }

    HRESULT __stdcall OnProgress(
        ULONG ulProgress,
        ULONG ulProgressMax,
        ULONG ulStatusCode,
        LPCWSTR szStatusText) override
    {
        UNREFERENCED_PARAMETER(szStatusText);

        if (ulStatusCode == BINDSTATUS_DOWNLOADINGDATA) {
            ULONG Progress = (ulProgress * 100) / ulProgressMax;
            SendMessageW(m_hWnd, PBM_SETPOS, Progress, 0);
        }

        return S_OK;
    }
};

bool Download(std::string& content, const wchar_t *url)
{
    DeleteUrlCacheEntryW(url);

    IStream* stream;
    HRESULT result = URLOpenBlockingStreamW(nullptr, url, &stream, 0, nullptr);

    if (FAILED(result)) {
        fprintf(stderr, "URLOpenBlockingStreamW failed (HRESULT: 0x%lX)\n", result);
        return false;
    }

    STATSTG stats;
    result = stream->Stat(&stats, STATFLAG_NONAME);
    if (FAILED(result)) {
        fprintf(stderr, "IStream::Stat failed (HRESULT: 0x%lX)\n", result);
        stream->Release();
        return false;
    }

    if (stats.cbSize.HighPart != 0) {
        fprintf(stderr, "This file is way to big to download, it's over 4GB\n");
        stream->Release();
        return false;
    }

    content.resize(stats.cbSize.LowPart);
    result = stream->Read(&content[0], stats.cbSize.LowPart, nullptr);
    stream->Release();

    if (FAILED(result)) {
        fprintf(stderr, "IStream::Read failed (HRESULT: 0x%lX, Url: '%ls')\n", result, url);
        return false;
    }

    return true;
}

bool Download(const wchar_t *path_to_file, const wchar_t *url, IBindStatusCallback *callback)
{
    DeleteUrlCacheEntryW(url);

    HRESULT result = URLDownloadToFileW(
        nullptr,
        url,
        path_to_file,
        0,
        callback);

    if (FAILED(result)) {
        fprintf(stderr, "URLDownloadToFileW failed: hresult:0x%lX\n", result);
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
    } catch(Json::exception&) {
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

    std::wstring url;
    for (size_t i = 0; i < release.assets.size(); i++) {
        Asset *asset = &release.assets[i];
        if (asset->name == "GWToolbox.dll") {
            fprintf(stderr, "browser_download_url: '%s'\n", asset->browser_download_url.c_str());
            const char *purl = asset->browser_download_url.c_str();
            url = std::wstring(purl, purl + asset->browser_download_url.size());
            break;
        }
    }

    if (url.empty()) {
        fprintf(stderr, "Didn't find GWTooolboxdll.dll\n");
        return false;
    }

    DownloadWindow window;
    window.Create();
    window.SetChangelog(release.body.c_str(), release.body.size());
    window.ProcessMessages();

    DownloadStatusCallback callback(&window, window.m_hProgressBar);
    if (!Download(dll_path, url.c_str(), &callback)) {
        fprintf(stderr, "Download failed: path_to_file = '%ls', url = '%ls'\n",
                dll_path, url.c_str());
        return false;
    }

    SendMessageW(window.m_hProgressBar, PBM_SETPOS, 100, 0);
    window.WaitMessages();

    RegWriteRelease(release.tag_name.c_str(), release.tag_name.size());
    return true;
}

DownloadWindow::DownloadWindow()
    : m_hProgressBar(nullptr)
    , m_hCloseButton(nullptr)
    , m_hChangelog(nullptr)
{
}

DownloadWindow::~DownloadWindow()
{
}

bool DownloadWindow::Create()
{
    // WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
    SetWindowName(L"GWToolbox - Download");
    SetWindowDimension(500, 300);
    return Window::Create();
}

void DownloadWindow::SetChangelog(const char *str, size_t length)
{
    std::wstring content(str, str + length);
    SendMessageW(m_hChangelog, WM_SETTEXT, 0, reinterpret_cast<LPARAM>(content.c_str()));
}

LRESULT DownloadWindow::WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
        OnCreate(hWnd, uMsg, wParam, lParam);
        break;

    case WM_CLOSE:
        DestroyWindow(hWnd);
        break;

    case WM_DESTROY:
        SignalStop();
        break;

    case WM_CTLCOLORSTATIC: {
        HDC hDc = reinterpret_cast<HDC>(wParam);
        HWND hEditCtl = reinterpret_cast<HWND>(lParam);

        if (hEditCtl == m_hChangelog) {
            SetBkMode(hDc, TRANSPARENT);
            return (LRESULT)GetStockObject(WHITE_BRUSH);
        }

        break;
    }

    case WM_COMMAND: {
        HWND hControl = reinterpret_cast<HWND>(lParam);
        LONG ControlId = LOWORD(wParam);
        if ((hControl == m_hCloseButton) && (ControlId == STN_CLICKED)) {
            DestroyWindow(m_hWnd);
        }
        break;
    }

    case WM_KEYUP:
        if (wParam == VK_ESCAPE)
            DestroyWindow(hWnd);
        else if (wParam == VK_RETURN)
            DestroyWindow(hWnd);
        break;
    }

    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

void DownloadWindow::OnCreate(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(uMsg);
    UNREFERENCED_PARAMETER(wParam);
    UNREFERENCED_PARAMETER(lParam);

    m_hProgressBar = CreateWindowW(
        PROGRESS_CLASSW,
        L"Inject",
        WS_VISIBLE | WS_CHILD,
        5,
        215,
        475,
        15,
        hWnd,
        nullptr,
        m_hInstance,
        nullptr);
    SendMessageW(m_hProgressBar, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
    SendMessageW(m_hProgressBar, PBM_SETPOS, 0, 0);

    m_hCloseButton = CreateWindowW(
        WC_BUTTONW,
        L"Close",
        WS_VISIBLE | WS_CHILD | WS_TABSTOP | BS_DEFPUSHBUTTON,
        400, // x
        235, // y
        80,  // width
        25,  // height
        hWnd,
        nullptr,
        m_hInstance,
        nullptr);
    SendMessageW(m_hCloseButton, WM_SETFONT, (WPARAM)m_hFont, MAKELPARAM(TRUE, 0));

    m_hChangelog = CreateWindowW(
        WC_EDITW,
        L"",
        WS_VISIBLE | WS_CHILD | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | ES_AUTOHSCROLL,
        5,
        5,
        475,
        175,
        hWnd,
        nullptr,
        m_hInstance,
        nullptr);
    SendMessageW(m_hChangelog, WM_SETFONT, (WPARAM)m_hFont, MAKELPARAM(TRUE, 0));
}
