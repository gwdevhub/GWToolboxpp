#include "stdafx.h"

#include <bcrypt.h>

#include <File.h>
#include <Path.h>

#include <RestClient.h>
#include "Download.h"

#include "Install.h"
#include "Settings.h"
#include "WindowsDefender.h"

namespace {
    std::unordered_map<std::string, std::string> cached_download_data;

}

class AsyncFileDownloader : public AsyncRestClient {
public:
    AsyncFileDownloader() : m_DownloadLength{0} {}

    AsyncFileDownloader(const AsyncFileDownloader&) = delete;

    size_t GetDownloadCount() const { return m_DownloadLength.load(std::memory_order_relaxed); }

private: // From AsyncRestClient
    void OnContent(const char* bytes, const size_t count) override
    {
        AsyncRestClient::OnContent(bytes, count);
        m_DownloadLength += count;
    }

    std::atomic<size_t> m_DownloadLength;
};

bool Download(std::string& content, const char* url, int timeout_sec = 5, bool fresh = false)
{
    if (!fresh || !cached_download_data.contains(url)) {
        RestClient client;
        client.SetUrl(url);
        client.SetFollowLocation(true);
        client.SetVerifyPeer(false);
        client.SetTimeoutSec(timeout_sec);
        client.SetUserAgent("curl/7.71.1");
        client.Execute();

        if (!client.IsSuccessful()) {
            fprintf(stderr, "Failed to download '%s'. (Status: %s, StatusCode: %d)\n", url, client.GetStatusStr(), client.GetStatusCode());
            return false;
        }

        cached_download_data[url] = std::move(client.GetContent());
    }
    content = cached_download_data[url];
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

static bool ParseReleases(const std::string& content, std::vector<Release>& releases)
{
    // Parse the JSON array, tolerating unknown keys so Github can add response fields without breaking us.
    constexpr glz::opts opts{.error_on_unknown_keys = false};
    if (auto ec = glz::read<opts>(releases, content); ec) {
        fprintf(stderr, "Failed to parse releases list\n");
        return false;
    }
    return true;
}

// One Github request shared by the exe and dll checks, so we only tap the (rate-limited) API once per launch.
static bool DownloadReleases(std::vector<Release>& releases)
{
    std::string content;
    if (!Download(content, "https://api.github.com/repos/gwdevhub/GWToolboxpp/releases?per_page=30")) return false;
    return ParseReleases(content, releases);
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
    sprintf_s(buffer, "%lu.%lu.%lu.%lu", pvi->dwProductVersionMS >> 16, pvi->dwFileVersionMS & 0xFFFF, pvi->dwFileVersionLS >> 16, pvi->dwFileVersionLS & 0xFFFF);
    delete[] buf;
    return {buffer};
}

// Lowercase hex sha256 of a file, streamed so we don't hold the whole file in memory; empty on failure.
static std::string Sha256Hex(const std::filesystem::path& path)
{
    BCRYPT_ALG_HANDLE alg = nullptr;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0) return {};

    std::string result;
    BCRYPT_HASH_HANDLE hash = nullptr;
    if (BCryptCreateHash(alg, &hash, nullptr, 0, nullptr, 0, 0) == 0) {
        std::ifstream file(path, std::ios::binary);
        bool ok = file.is_open();
        char buffer[64 * 1024];
        while (ok) {
            file.read(buffer, sizeof(buffer));
            const std::streamsize n = file.gcount();
            if (n <= 0) break;
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

// True if the installed file already matches the release asset: prefers Github's sha256 digest, falls back to file size.
static bool FileMatchesAsset(const std::filesystem::path& file_path, const Asset& asset, size_t file_size)
{
    std::error_code ec;
    const auto current_size = std::filesystem::file_size(file_path, ec);
    if (current_size != file_size) return false;

    constexpr std::string_view sha_prefix = "sha256:";
    if (asset.digest.starts_with(sha_prefix)) {
        const std::string local = Sha256Hex(file_path);
        if (!local.empty()) return local == asset.digest.substr(sha_prefix.size());
    }
    return false;
}

// Compares the installed dll against the latest release; doesn't download anything.
static DllUpdateInfo FindDllUpdate(const std::vector<Release>& releases)
{
    DllUpdateInfo info;

    info.dll_path = GetInstallationDir();
    if (info.dll_path.empty()) return info;
    info.dll_path = info.dll_path.parent_path() / "GWToolboxdll.dll";

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
        if (release_dll_asset) break;
    }
    if (!release_dll_asset) return info;
    info.asset_found = true;

    if (std::filesystem::exists(info.dll_path)) {
        std::error_code ec;
        const auto current_filesize = std::filesystem::file_size(info.dll_path, ec);
        if (!ec && FileMatchesAsset(info.dll_path, *release_dll_asset, current_filesize)) {
            info.up_to_date = true;
            return info;
        }
        const auto current_version = GetDllRelease(info.dll_path);
        const auto available_version = std::regex_replace(release->tag_name, std::regex(R"([^0-9.])"), "");
        if (current_version.starts_with(available_version)) {
            // NB: keeps locally-built betas (8.6 Beta1, 8.6 Beta123, ...) from being overwritten by the release.
            info.up_to_date = true;
            return info;
        }
    }

    info.asset = release_dll_asset;
    info.tag_name = release->tag_name;
    return info;
}

// Scans releases newest-first for the most recent one shipping a GWToolbox.exe, then compares both the installed copy and the currently-running exe (if different) against it; doesn't download.
static ExeUpdateInfo FindExeUpdate(const std::vector<Release>& releases)
{
    ExeUpdateInfo info;

    std::filesystem::path exe_path;
    if (!PathGetExeFullPath(exe_path)) return info;

    const auto install_dir = GetInstallationDir();
    const auto installed_exe = install_dir.empty() ? std::filesystem::path{} : install_dir.parent_path() / L"GWToolbox.exe";

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
        if (exe_asset) break;
    }
    if (!exe_asset || exe_asset->browser_download_url.empty()) return info;

    std::error_code ec;
    const auto current_size = std::filesystem::file_size(exe_path, ec);

    if (!installed_exe.empty() && std::filesystem::exists(installed_exe) && !FileMatchesAsset(installed_exe, *exe_asset, current_size))
        info.targets.push_back(installed_exe);
    if (!exe_path.empty() && exe_path != installed_exe && std::filesystem::exists(exe_path) && !FileMatchesAsset(exe_path, *exe_asset, current_size))
        info.targets.push_back(exe_path);

    if (info.targets.empty()) return info; // up-to-date

    info.asset = exe_asset;
    info.tag_name = tag_name;
    return info;
}

static constexpr wchar_t kReleasesPage[] = L"https://github.com/gwdevhub/GWToolboxpp/releases";

// Windows won't let a running exe be overwritten but does allow it to be renamed, so we move it aside and drop the already-downloaded `data` in its place; the swap takes effect next launch since this process keeps running its already-loaded image.
static bool ReplaceExeFile(const std::filesystem::path& exe_path, const std::string& data, const Asset& asset, std::wstring& error)
{
    const auto asset_filename = exe_path.filename();

    auto new_path = exe_path;
    new_path += L".new";
    auto old_path = exe_path;
    old_path += L".old";

    if (!WriteEntireFile(new_path.wstring().c_str(), data.c_str(), data.size()))
        return error = std::format(
                   L"Couldn't write the update to {}.\n\nThe folder may be read-only, or anti-virus may have quarantined the file.", new_path.wstring()
               ),
               false;

    DeleteFileW(old_path.wstring().c_str()); // leftover from a previous update, if any

    if (!MoveFileW(exe_path.wstring().c_str(), old_path.wstring().c_str())) {
        const DWORD err = GetLastError();
        DeleteFileW(new_path.wstring().c_str());
        return error = std::format(
                   L"Couldn't replace {} (error {}).\n\nIf it lives in a protected folder such as Program Files, run GWToolbox as administrator.", asset_filename.wstring(), err
               ),
               false;
    }
    if (!MoveFileW(new_path.wstring().c_str(), exe_path.wstring().c_str())) {
        const DWORD err = GetLastError();
        MoveFileW(old_path.wstring().c_str(), exe_path.wstring().c_str()); // roll back the rename
        return error = std::format(L"Couldn't move the update into place (error {}).", err), false;
    }

    // Confirm the new file actually landed; anti-virus has been seen to silently restore or quarantine the replacement.
    std::error_code ec;
    const auto written_size = std::filesystem::file_size(exe_path, ec);
    if (ec || !FileMatchesAsset(exe_path, asset, written_size))
        return error = std::format(
                   L"The update didn't stick - {} still doesn't match the new version.\n\nAnti-virus software may be reverting or quarantining it. Add an exclusion for the GWToolbox folder, or download the latest version manually from {}.",
                   asset_filename.wstring(), kReleasesPage
               ),
               false;
    return true;
}

// Bodies of whichever releases are being applied, newest/most-relevant first, deduped when exe and dll share a tag.
static std::string BuildChangelog(const std::vector<Release>& releases, const ExeUpdateInfo& exe_info, const DllUpdateInfo& dll_info)
{
    std::string changelog;
    const auto append_body = [&](const std::string& tag_name) {
        for (const auto& release : releases) {
            if (release.tag_name != tag_name) continue;
            if (!changelog.empty()) changelog += "\n\n----------\n\n";
            changelog += release.body.value_or("");
            return;
        }
    };

    if (exe_info.asset) append_body(exe_info.tag_name);
    if (dll_info.asset && !dll_info.up_to_date && dll_info.tag_name != exe_info.tag_name) append_body(dll_info.tag_name);
    return changelog;
}

bool DownloadWindow::DownloadAssetWithProgress(DownloadWindow& window, const std::string& url, const size_t file_size, std::string& out_content, std::wstring& error)
{
    AsyncFileDownloader downloader;
    AsyncDownload(url.c_str(), &downloader);

    while (!downloader.IsCompleted()) {
        if (window.ShouldClose()) {
            downloader.Abort();
            return error = L"Update cancelled", false;
        }
        window.PollMessages(16);
        const size_t bytes_downloaded = downloader.GetDownloadCount();
        const auto progress = file_size ? (bytes_downloaded * 100 / file_size) : 0;
        SendMessageW(window.m_hProgressBar, PBM_SETPOS, static_cast<WPARAM>(progress), 0);
    }

    if (!downloader.IsSuccessful()) {
        std::wstring url_w(url.begin(), url.end());
        std::string status_str = downloader.GetStatusStr();
        std::wstring status_w(status_str.begin(), status_str.end());
        return error = std::format(L"Failed to download '{}'. (Status: {}, StatusCode: {})", url_w, status_w, downloader.GetStatusCode()), false;
    }

    out_content = std::move(downloader.GetContent());
    SendMessageW(window.m_hProgressBar, PBM_SETPOS, 100, 0);
    return true;
}

bool DownloadWindow::DownloadDll(const std::vector<Release>& releases, std::wstring& error)
{
    const DllUpdateInfo info = FindDllUpdate(releases);
    if (!info.asset_found) return error = L"Failed to find dll in latest release", false;
    if (info.up_to_date) return true;

    DownloadWindow window;
    window.Create();

    const std::string changelog = BuildChangelog(releases, {}, info);
    window.SetChangelog(changelog.c_str(), changelog.size());

    std::string data;
    if (!DownloadAssetWithProgress(window, info.asset->browser_download_url, info.asset->size, data, error)) return false;

    if (!WriteEntireFile(info.dll_path.wstring().c_str(), data.c_str(), data.size()))
        return error = std::format(L"WriteEntireFile failed on '{}' with {} bytes", info.dll_path.wstring(), data.size()), false;

    SetWindowTextW(window.m_hStatusLabel, L"Download complete! Review the release notes above, then click 'Close' to continue.");
    if (!window.ShouldClose()) window.WaitMessages();

    return true;
}

bool DownloadWindow::ApplyUpdates(const std::vector<Release>& releases, const ExeUpdateInfo& exe_info, const DllUpdateInfo& dll_info, std::wstring& error)
{
    const bool exe_available = exe_info.asset != nullptr;
    const bool dll_available = dll_info.asset != nullptr && !dll_info.up_to_date;
    if (!exe_available && !dll_available) return true;

    DownloadWindow window;
    window.Create();

    const std::string changelog = BuildChangelog(releases, exe_info, dll_info);
    window.SetChangelog(changelog.c_str(), changelog.size());

    bool ok = true;
    if (exe_available) {
        SetWindowTextW(window.m_hStatusLabel, L"Downloading GWToolbox.exe...");
        std::string data;
        ok = DownloadAssetWithProgress(window, exe_info.asset->browser_download_url, exe_info.asset->size, data, error);
        for (size_t i = 0; ok && i < exe_info.targets.size(); i++) {
            ok = ReplaceExeFile(exe_info.targets[i], data, *exe_info.asset, error);
        }
    }

    if (ok && dll_available) {
        SetWindowTextW(window.m_hStatusLabel, L"Downloading GWToolboxdll.dll...");
        SendMessageW(window.m_hProgressBar, PBM_SETPOS, 0, 0);
        std::string data;
        ok = DownloadAssetWithProgress(window, dll_info.asset->browser_download_url, dll_info.asset->size, data, error);
        if (ok && !WriteEntireFile(dll_info.dll_path.wstring().c_str(), data.c_str(), data.size())) {
            error = std::format(L"WriteEntireFile failed on '{}' with {} bytes", dll_info.dll_path.wstring(), data.size());
            ok = false;
        }
    }

    SetWindowTextW(window.m_hStatusLabel, ok ? L"Update complete! Any exe update takes effect next launch." : error.c_str());
    if (!window.ShouldClose()) window.WaitMessages();

    return ok;
}

UpdateChecker::~UpdateChecker()
{
    m_ReleasesFetch.Abort(); // safe no-op if the fetch was never started or already finished
}

void UpdateChecker::Start(const bool check_exe, const bool check_dll)
{
    if (m_Started || (!check_exe && !check_dll)) return;
    m_Started = true;
    m_CheckExe = check_exe;
    m_CheckDll = check_dll;

    m_ReleasesFetch.SetUrl("https://api.github.com/repos/gwdevhub/GWToolboxpp/releases?per_page=30");
    m_ReleasesFetch.SetFollowLocation(true);
    m_ReleasesFetch.SetVerifyPeer(false);
    m_ReleasesFetch.SetTimeoutSec(10);
    m_ReleasesFetch.SetUserAgent("curl/7.71.1");
    m_ReleasesFetch.ExecuteAsync();
}

bool UpdateChecker::Poll()
{
    if (!m_Started || m_Completed) return false;
    if (!m_ReleasesFetch.IsCompleted()) return false;

    m_Completed = true;
    if (m_ReleasesFetch.IsSuccessful() && ParseReleases(m_ReleasesFetch.GetContent(), m_Releases)) {
        if (m_CheckExe) m_ExeInfo = FindExeUpdate(m_Releases);
        if (m_CheckDll) m_DllInfo = FindDllUpdate(m_Releases);
    }
    else {
        fprintf(stderr, "UpdateChecker: failed to fetch/parse releases\n");
    }
    return true;
}

bool UpdateChecker::ApplyUpdates(std::wstring& error)
{
    if (!IsAnyUpdateAvailable()) return true;

    // Unlike main.cpp's pre-injection check (which only covers Gw.exe), these writes are made by this launcher process itself.
    if (!settings.quiet) {
        const auto install_dir = GetInstallationDir();
        if (!install_dir.empty()) {
            std::vector<std::filesystem::path> cfa_apps = m_ExeInfo.targets; // installed and/or running exe, whichever are stale
            std::filesystem::path running_exe;
            if (PathGetExeFullPath(running_exe)) cfa_apps.push_back(running_exe); // covers a dll-only update, where m_ExeInfo.targets is empty
            EnsureDefenderReadiness(install_dir.parent_path(), cfa_apps);
        }
    }

    if (!DownloadWindow::ApplyUpdates(m_Releases, m_ExeInfo, m_DllInfo, error)) return false;

    m_ExeInfo = {};
    m_DllInfo = {};
    return true;
}

bool DownloadWindow::DownloadDll(std::wstring& error)
{
    std::vector<Release> releases;
    if (!DownloadReleases(releases)) return error = L"Couldn't download the latest releases of GWToolboxpp", true;
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
    // Win32 Edit control requires CRLF; GitHub release bodies use bare LF
    std::wstring content;
    content.reserve(length);
    for (const char* p = str; p != str + length; ++p) {
        if (*p == '\n' && (p == str || p[-1] != '\r'))
            content += L'\r';
        content += static_cast<wchar_t>(*p);
    }
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

        case WM_DPICHANGED:
            OnDpiChanged(wParam, lParam);
            break;
    }

    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

void DownloadWindow::OnCreate(HWND hWnd, UINT, WPARAM, LPARAM)
{
    ApplyDpiScaling(hWnd);

    m_hProgressBar = CreateWindowW(PROGRESS_CLASSW, L"Inject", WS_VISIBLE | WS_CHILD, Scale(5), Scale(215), Scale(475), Scale(15), hWnd, nullptr, m_hInstance, nullptr);
    SendMessageW(m_hProgressBar, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
    SendMessageW(m_hProgressBar, PBM_SETPOS, 0, 0);

    m_hCloseButton = CreateWindowW(
        WC_BUTTONW, L"Close", WS_VISIBLE | WS_CHILD | WS_TABSTOP | BS_DEFPUSHBUTTON,
        Scale(400), // x
        Scale(235), // y
        Scale(80),  // width
        Scale(25),  // height
        hWnd, nullptr, m_hInstance, nullptr
    );
    SendMessageW(m_hCloseButton, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), MAKELPARAM(TRUE, 0));

    m_hChangelog = CreateWindowW(WC_EDITW, L"", WS_VISIBLE | WS_CHILD | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | ES_AUTOHSCROLL, Scale(5), Scale(5), Scale(475), Scale(170), hWnd, nullptr, m_hInstance, nullptr);
    SendMessageW(m_hChangelog, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), MAKELPARAM(TRUE, 0));

    m_hStatusLabel = CreateWindowW(WC_STATICW, L"Downloading...", WS_VISIBLE | WS_CHILD | SS_LEFT, Scale(5), Scale(180), Scale(475), Scale(30), hWnd, nullptr, m_hInstance, nullptr);
    SendMessageW(m_hStatusLabel, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), MAKELPARAM(TRUE, 0));
}
