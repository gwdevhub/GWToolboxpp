#include "stdafx.h"

using Json = nlohmann::json;

#include <File.h>
#include <Path.h>

#include <RestClient.h>

#include "Download.h"

class AsyncFileDownloader : public AsyncRestClient
{
public:
    AsyncFileDownloader()
        : m_DownloadLength{0}
    {
    }

    AsyncFileDownloader(const AsyncFileDownloader&) = delete;

    size_t GetDownloadCount()
    {
        return m_DownloadLength.load(std::memory_order_relaxed);
    }

private: // From AsyncRestClient
    void OnContent(const char *bytes, size_t count) override
    {
        AsyncRestClient::OnContent(bytes, count);
        m_DownloadLength += count;
    }

    std::atomic<size_t> m_DownloadLength;
};

bool Download(std::string& content, const char *url)
{
    RestClient client;
    client.SetUrl(url);
    client.SetVerifyPeer(false);
    client.SetTimeoutSec(5);
    client.SetUserAgent("curl/7.71.1");
    client.Execute();

    if (!client.IsSuccessful()) {
        fprintf(stderr, "Failed to download '%s'. (Status: %s, StatusCode: %d)\n",
            url, client.GetStatusStr(), client.GetStatusCode());
        return false;
    }

    content = std::move(client.GetContent());
    return true;
}

void AsyncDownload(const char *url, AsyncFileDownloader *downloader)
{
    downloader->SetUrl(url);
    downloader->SetVerifyPeer(false);
    downloader->SetFollowLocation(true);
    downloader->SetUserAgent("curl/7.71.1");
    downloader->ExecuteAsync();
}

struct Asset
{
    std::string name{};
    size_t size = 0;
    std::string browser_download_url{};
};

struct Release
{
    std::string tag_name{};
    std::string body{};
    std::vector<Asset> assets{};
};

static bool ParseRelease(const std::string& json_text, Release *release)
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

    release->tag_name = it_tag_name->get<std::string>();
    release->body = it_body->get<std::string>();

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
        asset.name = it_name->get<std::string>();
        asset.size = it_size->get<size_t>();
        asset.browser_download_url = it_browser_download_url->get<std::string>();

        release->assets.emplace_back(std::move(asset));
    }

    return true;
}

std::string GetDllRelease(const std::filesystem::path dllpath) {
    using namespace std::filesystem;

    if (!exists(dllpath)) {
        return {};
    }

    const std::wstring dllpathstr = dllpath.wstring();
    const LPCWSTR filename = dllpathstr.c_str();
    char buffer[MAX_PATH];

    DWORD dwHandle, sz = GetFileVersionInfoSizeW(filename, &dwHandle);
    if (sz == 0) {
        return {};
    }
    char* buf = new char[sz];
    if (!GetFileVersionInfoW(filename, dwHandle, sz, &buf[0])) {
        delete[] buf;
        return {};
    }
    VS_FIXEDFILEINFO* pvi{};
    sz = sizeof(VS_FIXEDFILEINFO);
    if (!VerQueryValueW(&buf[0], L"\\", reinterpret_cast<LPVOID*>(&pvi), reinterpret_cast<unsigned int*>(&sz))) {
        delete[] buf;
        return {};
    }
    sprintf(buffer, "%d.%d.%d.%d", pvi->dwProductVersionMS >> 16, pvi->dwFileVersionMS & 0xFFFF,
        pvi->dwFileVersionLS >> 16, pvi->dwFileVersionLS & 0xFFFF);
    delete[] buf;
    return {buffer};

}

bool DownloadWindow::DownloadAllFiles()
{
    std::string content;
    if (!Download(content, "https://api.github.com/repos/HasKha/GWToolboxpp/releases/latest")) {
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

    std::filesystem::path dllpath;
    PathGetDocumentsPath(dllpath, L"GWToolboxpp\\GWToolboxdll.dll");
    const auto release_string = GetDllRelease(dllpath);
    if (!release_string.empty()) {
        std::string release_tag = release.tag_name;
        const auto current_version  = std::regex_replace(release_tag, std::regex(R"([^0-9.])"), "");
        if (release_string.starts_with(current_version))
            return true;
    }

    std::filesystem::path dll_path;
    if (!PathGetDocumentsPath(dll_path, L"GWToolboxpp\\GWToolboxdll.dll")) {
        return false;
    }

    char buffer[64];
    snprintf(buffer, 64, "Downloading version '%s'", release.tag_name.c_str());
    MessageBoxA(nullptr, buffer, "Downloading...", 0);

    std::string url;
    size_t file_size = 0;
    for (auto& asset : release.assets) {
        if (asset.name == "GWToolbox.dll" || asset.name == "GWToolboxdll.dll") {
            fprintf(stderr, "browser_download_url: '%s'\n", asset.browser_download_url.c_str());
            const char* purl = asset.browser_download_url.c_str();
            file_size = asset.size;
            url = std::string(purl, purl + asset.browser_download_url.size());
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

    AsyncFileDownloader downloader;
    AsyncDownload(url.c_str(), &downloader);

    while (!window.ShouldClose()) {
        window.PollMessages(16);

        if (!downloader.IsCompleted()) {
            size_t BytesDownloaded = downloader.GetDownloadCount();
            const auto progress = BytesDownloaded * 100 / file_size;
            SendMessageW(window.m_hProgressBar, PBM_SETPOS, progress, 0);
        } else {
            if (!downloader.IsSuccessful()) {
                fprintf(stderr, "Failed to download '%s'. (Status: %s, StatusCode: %d)\n",
                    url.c_str(), downloader.GetStatusStr(), downloader.GetStatusCode());
                return false;
            }

            std::string& file_content = downloader.GetContent();
            if (!WriteEntireFile(dll_path.wstring().c_str(), file_content.c_str(), file_content.size())) {
                fwprintf(stderr, L"WriteEntireFile failed on '%s' with %zu bytes\n",
                    dll_path.wstring().c_str(), file_content.size());
                return false;
            }

            downloader.Clear();
            SendMessageW(window.m_hProgressBar, PBM_SETPOS, 100, 0);
            SendMessageW(window.m_hWnd, WM_CLOSE, 0, 0);
        }
    }

    //
    // The user could close the window, before the download is complete
    //
    if (downloader.IsPending()) {
        downloader.Abort();
        return false;
    }

    return true;
}

bool DownloadWindow::Create()
{
    // WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
    SetWindowName(L"GWToolbox - Download");
    SetWindowDimension(500, 300);
    return Window::Create();
}

void DownloadWindow::SetChangelog(const char* str, size_t length) const
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
