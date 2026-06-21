#include "stdafx.h"

#include <bcrypt.h>

#include <File.h>
#include <Path.h>

#include <RestClient.h>
#include "Download.h"

#include "Install.h"
#include "Settings.h"

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
    std::string digest{}; // e.g. "sha256:<hex>"; absent on older releases
};

struct Release {
    std::string tag_name{};
    std::string body{};
    std::vector<Asset> assets{};
};

// One Github request shared by the exe and dll checks, so we only tap the (rate-limited) API once per launch.
static bool DownloadReleases(std::vector<Release>& releases)
{
    std::string content;
    if (!Download(content, "https://api.github.com/repos/gwdevhub/GWToolboxpp/releases?per_page=30"))
        return false;
    // Parse the JSON array, tolerating unknown keys so Github can add response fields without breaking us.
    constexpr glz::opts opts{.error_on_unknown_keys = false};
    if (auto ec = glz::read<opts>(releases, content); ec) {
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

// Lowercase hex sha256 of a file, streamed so we don't hold the whole file in memory; empty on failure.
static std::string Sha256Hex(const std::filesystem::path& path)
{
    BCRYPT_ALG_HANDLE alg = nullptr;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0)
        return {};

    std::string result;
    BCRYPT_HASH_HANDLE hash = nullptr;
    if (BCryptCreateHash(alg, &hash, nullptr, 0, nullptr, 0, 0) == 0) {
        std::ifstream file(path, std::ios::binary);
        bool ok = file.is_open();
        char buffer[64 * 1024];
        while (ok) {
            file.read(buffer, sizeof(buffer));
            const std::streamsize n = file.gcount();
            if (n <= 0)
                break;
            ok = BCryptHashData(hash, reinterpret_cast<PUCHAR>(buffer), static_cast<ULONG>(n), 0) == 0;
        }

        unsigned char digest[32];
        if (ok && BCryptFinishHash(hash, digest, sizeof(digest), 0) == 0) {
            char hex[2 * sizeof(digest) + 1];
            for (size_t i = 0; i < sizeof(digest); ++i)
                sprintf_s(hex + i * 2, 3, "%02x", digest[i]);
            result = hex;
        }
        BCryptDestroyHash(hash);
    }
    BCryptCloseAlgorithmProvider(alg, 0);
    return result;
}

// True if the installed file already matches the release asset. Prefers Github's sha256 digest (exact identity)
// and falls back to file size when the digest is absent or the local file can't be hashed.
static bool FileMatchesAsset(const std::filesystem::path& file_path, const Asset& asset, size_t file_size)
{
    constexpr std::string_view sha_prefix = "sha256:";
    if (asset.digest.starts_with(sha_prefix)) {
        const std::string local = Sha256Hex(file_path);
        if (!local.empty())
            return local == asset.digest.substr(sha_prefix.size());
    }
    return file_size == asset.size;
}

bool DownloadWindow::DownloadDll(const std::vector<Release>& releases, std::wstring& error)
{
    std::filesystem::path dllpath = GetInstallationDir();
    if (dllpath.empty())
        return error = L"GetInstallPath failed", false;
    dllpath = dllpath.parent_path() / "GWToolboxdll.dll";

    // Most recent release that ships the dll (normally the latest; scan to be safe).
    const Release* release = nullptr;
    const Asset* release_dll_asset = nullptr;
    for (const auto& candidate : releases) {
        for (const auto& asset : candidate.assets) {
            if (asset.name == "GWToolbox.dll" || asset.name == "GWToolboxdll.dll") {
                release = &candidate;
                release_dll_asset = &asset;
                break;
            }
        }
        if (release_dll_asset)
            break;
    }
    if (!release_dll_asset) {
        return error = L"Failed to find dll in latest release", false;
    }
    if (std::filesystem::exists(dllpath)) {
        std::error_code ec;
        const auto current_filesize = std::filesystem::file_size(dllpath, ec);
        if (!ec && FileMatchesAsset(dllpath, *release_dll_asset, current_filesize)) {
            return true;
        }
        const auto current_version = GetDllRelease(dllpath);
        const auto available_version = std::regex_replace(release->tag_name, std::regex(R"([^0-9.])"), "");
        if (current_version.starts_with(available_version)) {
            // NB: keeps locally-built betas (8.6 Beta1, 8.6 Beta123, ...) from being overwritten by the release.
            return true;
        }
    }

    // Convert tag_name to wstring for MessageBox
    std::wstring tag_name_w(release->tag_name.begin(), release->tag_name.end());
    std::wstring buffer = std::format(L"Downloading version '{}'", tag_name_w);
    MessageBoxW(nullptr, buffer.c_str(), L"Downloading...", 0);

    if (release_dll_asset->browser_download_url.empty())
        return error = L"Didn't find GWTooolboxdll.dll", false;


    const auto& url = release_dll_asset->browser_download_url;
    const auto& file_size = release_dll_asset->size;

    DownloadWindow window;
    window.Create();
    window.SetChangelog(release->body.c_str(), release->body.size());

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
static bool UpdateExe(const std::filesystem::path& exe_path, const Asset& asset, std::wstring& error)
{
    const std::string& url = asset.browser_download_url;
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

    // Confirm the new file actually landed; anti-virus has been seen to silently restore the old exe or quarantine
    // the replacement, which would otherwise leave us re-prompting to update on every launch.
    std::error_code ec;
    const auto written_size = std::filesystem::file_size(exe_path, ec);
    if (ec || !FileMatchesAsset(exe_path, asset, written_size))
        return error = std::format(L"The update didn't stick - GWToolbox.exe still doesn't match the new version.\n\n"
                                   L"Anti-virus software may be reverting or quarantining it. Add an exclusion for the "
                                   L"GWToolbox folder, or download the latest version manually from {}.", kReleasesPage), false;
    return true;
}

// Not every release ships a GWToolbox.exe, so we scan the release list newest-first for the most recent one
// that does, and offer it if it differs from ours.
static void CheckForExeUpdate(const std::vector<Release>& releases)
{
    std::filesystem::path exe_path;
    if (!PathGetExeFullPath(exe_path))
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
    if (!ec && !FileMatchesAsset(exe_path, *exe_asset, current_size)) {
        const std::wstring tag_w(tag_name.begin(), tag_name.end());
        const std::wstring prompt = std::format(
            L"A newer version of GWToolbox.exe ({}) is available.\n\n"
            L"Update now? GWToolbox will replace its own program file; the new version takes effect next launch.", tag_w);
        if (MessageBoxW(nullptr, prompt.c_str(), L"GWToolbox - Update available", MB_YESNO | MB_ICONINFORMATION | MB_TOPMOST) != IDYES)
            return;

        std::wstring error;
        if (!UpdateExe(exe_path, *exe_asset, error)) {
            MessageBoxW(nullptr, error.c_str(), L"GWToolbox - Update failed", MB_OK | MB_ICONERROR | MB_TOPMOST);
            return;
        }

        // Relaunch into the new exe (e.g. to retry an injection the old one failed). The single OK button is the
        // restart; we re-run from the original path, which now holds the new file.
        MessageBoxW(nullptr, L"GWToolbox.exe was updated.\n\nClick the button below to restart the launcher and start using the new version.",
                    L"GWToolbox - Update complete", MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
        RestartWithSameArgs();
    }

    // When running from outside the install directory (e.g. a freshly downloaded exe), CopyInstaller() is never
    // called because IsInstalled() is already true. The installed exe can therefore go stale while the user
    // believes they updated by re-downloading. Sync it here so the shortcut/installed copy stays current.
    const auto install_dir = GetInstallationDir();
    if (install_dir.empty())
        return;
    const auto installed_exe = install_dir.parent_path() / L"GWToolbox.exe";
    if (installed_exe == exe_path || !std::filesystem::exists(installed_exe))
        return;
    const auto installed_size = std::filesystem::file_size(installed_exe, ec);
    if (ec || FileMatchesAsset(installed_exe, *exe_asset, installed_size))
        return;

    std::wstring error;
    if (!UpdateExe(installed_exe, *exe_asset, error))
        MessageBoxW(nullptr, error.c_str(), L"GWToolbox - Update failed", MB_OK | MB_ICONERROR | MB_TOPMOST);
}

bool DownloadWindow::CheckForUpdates(std::wstring& error)
{
    // Fetch the release list once and run both checks off it, rather than tapping Github twice.
    std::vector<Release> releases;
    if (!DownloadReleases(releases))
        return error = L"Couldn't download the latest releases of GWToolboxpp", true; // non-fatal: don't block the launch

    // Exe first: if it's out of date its dialog wins and may relaunch before we ever touch the dll.
    if (!settings.noexecheck)
        CheckForExeUpdate(releases);

    if (settings.noupdate)
        return true;
    return DownloadDll(releases, error);
}

// dll-only entry (used by the installer); fetches its own release list since there's no exe check to share it with.
bool DownloadWindow::DownloadDll(std::wstring& error)
{
    std::vector<Release> releases;
    if (!DownloadReleases(releases))
        return error = L"Couldn't download the latest releases of GWToolboxpp", true;
    return DownloadDll(releases, error);
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
