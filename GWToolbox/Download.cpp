#include "stdafx.h"

#include <File.h>
#include <Path.h>

#include <RestClient.h>
#include "Download.h"

#include "Install.h"

class AsyncFileDownloader : public AsyncRestClient {
public:
    AsyncFileDownloader()
        : m_DownloadLength{0} {}

    AsyncFileDownloader(const AsyncFileDownloader&) = delete;

    size_t GetDownloadCount() const
    {
        return m_DownloadLength.load(std::memory_order_relaxed);
    }

private: // From AsyncRestClient
    void OnContent(const char* bytes, const size_t count) override
    {
        AsyncRestClient::OnContent(bytes, count);
        m_DownloadLength += count;
    }

    std::atomic<size_t> m_DownloadLength;
};

bool Download(std::string& content, const char* url, int timeout_sec = 5)
{
    RestClient client;
    client.SetUrl(url);
    client.SetFollowLocation(true);
    client.SetVerifyPeer(false);
    client.SetTimeoutSec(timeout_sec);
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

void AsyncDownload(const char* url, AsyncFileDownloader* downloader)
{
    downloader->SetUrl(url);
    downloader->SetVerifyPeer(false);
    downloader->SetFollowLocation(true);
    downloader->SetUserAgent("curl/7.71.1");
    downloader->ExecuteAsync();
}

struct Asset {
    std::string name{};
    size_t size = 0;
    std::string browser_download_url{};
};

struct Release {
    std::string tag_name{};
    std::string body{};
    std::vector<Asset> assets{};
};

static bool ParseRelease(const std::string& json_text, Release* release)
{
    constexpr glz::opts opts{.error_on_unknown_keys = false};
    if (auto ec = glz::read<opts>(*release, json_text); ec) {
        fprintf(stderr, "Json::parse failed\n");
        return false;
    }
    if (release->tag_name.empty() || release->assets.empty()) {
        fprintf(stderr, "Required fields missing in '%s'\n", json_text.c_str());
        return false;
    }
    return true;
}

static bool ParseReleases(const std::string& json_text, std::vector<Release>& releases)
{
    constexpr glz::opts opts{.error_on_unknown_keys = false};
    if (auto ec = glz::read<opts>(releases, json_text); ec) {
        fprintf(stderr, "Failed to parse releases list\n");
        return false;
    }
    return true;
}

std::string GetDllRelease(const std::filesystem::path& dllpath)
{
    using namespace std::filesystem;

    if (!exists(dllpath)) {
        return {};
    }

    const std::wstring dllpathstr = dllpath.wstring();
    const LPCWSTR filename = dllpathstr.c_str();
    char buffer[MAX_PATH];

    DWORD dw_handle, sz = GetFileVersionInfoSizeW(filename, &dw_handle);
    if (sz == 0) {
        return {};
    }
    const auto buf = new char[sz];
    if (!GetFileVersionInfoW(filename, dw_handle, sz, &buf[0])) {
        delete[] buf;
        return {};
    }
    VS_FIXEDFILEINFO* pvi{};
    sz = sizeof(VS_FIXEDFILEINFO);
    if (!VerQueryValueW(&buf[0], L"\\", reinterpret_cast<LPVOID*>(&pvi), reinterpret_cast<unsigned int*>(&sz))) {
        delete[] buf;
        return {};
    }
    sprintf_s(buffer, "%lu.%lu.%lu.%lu", pvi->dwProductVersionMS >> 16, pvi->dwFileVersionMS & 0xFFFF,
              pvi->dwFileVersionLS >> 16, pvi->dwFileVersionLS & 0xFFFF);
    delete[] buf;
    return {buffer};
}

bool DownloadWindow::DownloadAllFiles(std::wstring& error)
{
    std::filesystem::path dllpath = GetInstallationDir();
    if (dllpath.empty())
        return error = L"GetInstallPath failed", false;
    dllpath = dllpath.parent_path() / "GWToolboxdll.dll";

    std::string content;
    if (!Download(content, "https://api.github.com/repos/gwdevhub/GWToolboxpp/releases/latest")) {
        // @Remark:
        // We may not be able to grep Github api. (For instance, if we spam it)
        return error = L"Couldn't download the latest release of GWToolboxpp", true;
    }

    Release release;
    if (!ParseRelease(content, &release))
        return error = L"ParseRelease failed", false;

    Asset* release_dll_asset = nullptr;
    for (auto& asset : release.assets) {
        if (asset.name == "GWToolbox.dll" || asset.name == "GWToolboxdll.dll") {
            release_dll_asset = &asset;
            break;
        }
    }
    if (!release_dll_asset) {
        return error = L"Failed to find dll in latest release", false;
    }
    if (std::filesystem::exists(dllpath)) {
        const auto current_filesize = std::filesystem::file_size(dllpath);
        if (current_filesize == release_dll_asset->size) {
            const auto current_version = GetDllRelease(dllpath);
            const auto available_version = std::regex_replace(release.tag_name, std::regex(R"([^0-9.])"), "");
            if (current_version == available_version
                && current_filesize == release_dll_asset->size) {
                return true;
            }
            if (current_version.starts_with(available_version)) {
                // NB: This allows 8.6 Beta1, 8.6 Beta123 etc
                return true;
            }
        }
    }

    // Convert tag_name to wstring for MessageBox
    std::wstring tag_name_w(release.tag_name.begin(), release.tag_name.end());
    std::wstring buffer = std::format(L"Downloading version '{}'", tag_name_w);
    MessageBoxW(nullptr, buffer.c_str(), L"Downloading...", 0);

    if (release_dll_asset->browser_download_url.empty())
        return error = L"Didn't find GWTooolboxdll.dll", false;


    const auto& url = release_dll_asset->browser_download_url;
    const auto& file_size = release_dll_asset->size;

    DownloadWindow window;
    window.Create();
    window.SetChangelog(release.body.c_str(), release.body.size());

    AsyncFileDownloader downloader;
    AsyncDownload(url.c_str(), &downloader);

    bool download_complete = false;
    while (!window.ShouldClose()) {
        window.PollMessages(16);

        if (!download_complete && !downloader.IsCompleted()) {
            size_t BytesDownloaded = downloader.GetDownloadCount();
            const auto progress = BytesDownloaded * 100 / file_size;
            SendMessageW(window.m_hProgressBar, PBM_SETPOS, progress, 0);
        }
        else if (!download_complete) {
            if (!downloader.IsSuccessful()) {
                // Convert error message to wstring
                std::wstring url_w(url.begin(), url.end());
                std::string status_str = downloader.GetStatusStr();
                std::wstring status_w(status_str.begin(), status_str.end());
                return error = std::format(L"Failed to download '{}'. (Status: {}, StatusCode: {})",
                                           url_w, status_w, downloader.GetStatusCode()), false;
            }

            std::string& file_content = downloader.GetContent();
            if (!WriteEntireFile(dllpath.wstring().c_str(), file_content.c_str(), file_content.size())) {
                std::wstring dllpath_str = dllpath.wstring();
                return error = std::format(L"WriteEntireFile failed on '{}' with {} bytes",
                                           dllpath_str, file_content.size()), false;
            }

            downloader.Clear();
            SendMessageW(window.m_hProgressBar, PBM_SETPOS, 100, 0);
            SetWindowTextW(window.m_hStatusLabel, L"Download complete! Review the release notes above, then click 'Continue' to proceed.");
            SetWindowTextW(window.m_hCloseButton, L"Continue");
            download_complete = true;
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

static constexpr wchar_t kReleasesPage[] = L"https://github.com/gwdevhub/GWToolboxpp/releases";

// Windows locks a running exe against being overwritten, but still allows it to be renamed; so we move
// the running file aside and drop the new one in its place. The new exe takes effect on the next launch.
static bool UpdateExe(const std::filesystem::path& exe_path, const std::string& url, std::wstring& error)
{
    std::string data;
    if (!Download(data, url.c_str(), 30) || data.empty())
        return error = std::format(L"Couldn't download the update.\n\nAnti-virus software may be blocking it. "
                                   L"Download the latest GWToolbox.exe manually from {}.", kReleasesPage), false;

    auto new_path = exe_path;
    new_path += L".new";
    auto old_path = exe_path;
    old_path += L".old";

    if (!WriteEntireFile(new_path.wstring().c_str(), data.c_str(), data.size()))
        return error = std::format(L"Couldn't write the update to {}.\n\nThe folder may be read-only, or anti-virus may "
                                   L"have quarantined the file. Run GWToolbox from a writable folder, or download the "
                                   L"latest version manually from {}.", new_path.wstring(), kReleasesPage), false;

    DeleteFileW(old_path.wstring().c_str()); // leftover from a previous update, if any

    if (!MoveFileW(exe_path.wstring().c_str(), old_path.wstring().c_str())) {
        const DWORD err = GetLastError();
        DeleteFileW(new_path.wstring().c_str());
        return error = std::format(L"Couldn't replace GWToolbox.exe (error {}).\n\nIf it lives in a protected folder such "
                                   L"as Program Files, run GWToolbox as administrator or move it to a normal folder. You "
                                   L"can also download the latest version manually from {}.", err, kReleasesPage), false;
    }
    if (!MoveFileW(new_path.wstring().c_str(), exe_path.wstring().c_str())) {
        const DWORD err = GetLastError();
        MoveFileW(old_path.wstring().c_str(), exe_path.wstring().c_str()); // roll back the rename
        return error = std::format(L"Couldn't move the update into place (error {}).\n\nDownload the latest GWToolbox.exe "
                                   L"manually from {}.", err, kReleasesPage), false;
    }
    return true;
}

// Runs on a background thread so it never delays injection. Not every release ships a GWToolbox.exe, so we scan
// the release list newest-first for the most recent one that does, and offer it if its size differs from ours.
void CheckForExeUpdate()
{
    std::filesystem::path exe_path;
    if (!PathGetExeFullPath(exe_path))
        return;

    std::string content;
    if (!Download(content, "https://api.github.com/repos/gwdevhub/GWToolboxpp/releases?per_page=30"))
        return; // GitHub unreachable or rate-limited; never block a launch over an update check

    std::vector<Release> releases;
    if (!ParseReleases(content, releases))
        return;

    const Asset* exe_asset = nullptr;
    std::string tag_name;
    for (const auto& release : releases) {
        for (const auto& asset : release.assets) {
            if (asset.name == "GWToolbox.exe") {
                exe_asset = &asset;
                tag_name = release.tag_name;
                break;
            }
        }
        if (exe_asset)
            break;
    }
    if (!exe_asset || exe_asset->browser_download_url.empty())
        return;

    std::error_code ec;
    const auto current_size = std::filesystem::file_size(exe_path, ec);
    if (ec || current_size == exe_asset->size)
        return; // size is the only signal Github gives us without downloading; same size means up to date

    const std::wstring tag_w(tag_name.begin(), tag_name.end());
    const std::wstring prompt = std::format(
        L"A newer version of GWToolbox.exe ({}) is available.\n\n"
        L"Update now? GWToolbox will replace its own program file; the new version takes effect next launch.", tag_w);
    if (MessageBoxW(nullptr, prompt.c_str(), L"GWToolbox - Update available", MB_YESNO | MB_ICONINFORMATION | MB_TOPMOST) != IDYES)
        return;

    std::wstring error;
    if (!UpdateExe(exe_path, exe_asset->browser_download_url, error))
        MessageBoxW(nullptr, error.c_str(), L"GWToolbox - Update failed", MB_OK | MB_ICONERROR | MB_TOPMOST);
    else
        MessageBoxW(nullptr, L"GWToolbox.exe was updated. The new version will be used next time you start GWToolbox.",
                    L"GWToolbox - Update complete", MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
}

bool DownloadWindow::Create()
{
    // WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
    SetWindowName(L"GWToolbox - Download");
    SetWindowDimension(500, 300);
    return Window::Create();
}

void DownloadWindow::SetChangelog(const char* str, const size_t length) const
{
    const std::wstring content(str, str + length);
    SendMessageW(m_hChangelog, WM_SETTEXT, 0, reinterpret_cast<LPARAM>(content.c_str()));
}

LRESULT DownloadWindow::WndProc(HWND hWnd, const UINT uMsg, const WPARAM wParam, const LPARAM lParam)
{
    switch (uMsg) {
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
            const auto hDc = reinterpret_cast<HDC>(wParam);
            const auto hEditCtl = reinterpret_cast<HWND>(lParam);

            if (hEditCtl == m_hChangelog) {
                SetBkMode(hDc, TRANSPARENT);
                return reinterpret_cast<LRESULT>(GetStockObject(WHITE_BRUSH));
            }

            break;
        }

        case WM_COMMAND: {
            const auto hControl = reinterpret_cast<HWND>(lParam);
            const LONG ControlId = LOWORD(wParam);
            if (hControl == m_hCloseButton && ControlId == STN_CLICKED) {
                DestroyWindow(m_hWnd);
            }
            break;
        }

        case WM_KEYUP:
            if (wParam == VK_ESCAPE || wParam == VK_RETURN) {
                DestroyWindow(hWnd);
            }
            break;
    }

    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

void DownloadWindow::OnCreate(HWND hWnd, UINT, WPARAM, LPARAM)
{
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
        80, // width
        25, // height
        hWnd,
        nullptr,
        m_hInstance,
        nullptr);
    SendMessageW(m_hCloseButton, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), MAKELPARAM(TRUE, 0));

    m_hChangelog = CreateWindowW(
        WC_EDITW,
        L"",
        WS_VISIBLE | WS_CHILD | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | ES_AUTOHSCROLL,
        5,
        5,
        475,
        170,
        hWnd,
        nullptr,
        m_hInstance,
        nullptr);
    SendMessageW(m_hChangelog, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), MAKELPARAM(TRUE, 0));

    m_hStatusLabel = CreateWindowW(
        WC_STATICW,
        L"Downloading...",
        WS_VISIBLE | WS_CHILD | SS_LEFT,
        5,
        180,
        475,
        30,
        hWnd,
        nullptr,
        m_hInstance,
        nullptr);
    SendMessageW(m_hStatusLabel, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), MAKELPARAM(TRUE, 0));
}
