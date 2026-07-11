#pragma once

#include <optional>

#include <RestClient.h>

#include "Window.h"

struct Asset {
    std::string name{};
    size_t size = 0;
    std::string browser_download_url{};
    std::string digest{}; // e.g. "sha256:<hex>"; absent on older releases
};

struct Release {
    std::string tag_name{};
    std::optional<std::string> body{}; // Github sends null when a release has no description text
    std::vector<Asset> assets{};
};

// Result of comparing the installed/running exe(s) against the latest release; asset is null when up to date.
struct ExeUpdateInfo {
    const Asset* asset = nullptr;
    std::string tag_name;
    std::vector<std::filesystem::path> targets; // installed copy and/or the running exe, whichever are stale
};

// Result of comparing the installed dll against the latest release.
struct DllUpdateInfo {
    bool asset_found = false; // false if no release ships a matching dll asset at all
    bool up_to_date = false;
    const Asset* asset = nullptr;
    std::string tag_name;
    std::filesystem::path dll_path;
};

class DownloadWindow : public Window {
public:
    DownloadWindow() = default;
    DownloadWindow(const DownloadWindow&) = delete;
    ~DownloadWindow() override = default;

    DownloadWindow& operator=(const DownloadWindow&) = delete;

    bool Create() override;
    // dll-only update, fetching its own release list; used by the installer.
    static bool DownloadDll(std::wstring& error);
    // Downloads and installs whichever of exe/dll are due for an update, sharing one progress window; the exe replacement takes effect on next launch, no restart.
    static bool ApplyUpdates(const std::vector<Release>& releases, const ExeUpdateInfo& exe_info, const DllUpdateInfo& dll_info, std::wstring& error);
    void SetChangelog(const char* str, size_t length) const;

private:
    static bool DownloadDll(const std::vector<Release>& releases, std::wstring& error);
    // Downloads `url` (expected size `file_size`) into `out_content`, pumping `window`'s message loop and progress bar as it goes.
    static bool DownloadAssetWithProgress(DownloadWindow& window, const std::string& url, size_t file_size, std::string& out_content, std::wstring& error);

    LRESULT WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) override;

    void OnCreate(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    HWND m_hProgressBar = nullptr;
    HWND m_hCloseButton = nullptr;
    HWND m_hChangelog = nullptr;
    HWND m_hStatusLabel = nullptr;
};

// Background-checks Github for exe/dll updates and, on request, installs whichever are stale via DownloadWindow; owned by InjectWindow.
class UpdateChecker {
public:
    // AsyncRestClient asserts it isn't mid-request when destroyed, so this aborts a still-pending check (e.g. the user launched before it finished).
    ~UpdateChecker();

    void Start(bool check_exe, bool check_dll);

    bool IsChecking() const { return m_Started && !m_Completed; }

    // Advances the async releases fetch; returns true only on the tick it finishes (successfully or not).
    bool Poll();

    bool IsExeUpdateAvailable() const { return m_ExeInfo.asset != nullptr; }
    bool IsDllUpdateAvailable() const { return m_DllInfo.asset != nullptr && !m_DllInfo.up_to_date; }
    bool IsAnyUpdateAvailable() const { return IsExeUpdateAvailable() || IsDllUpdateAvailable(); }

    bool ApplyUpdates(std::wstring& error);

private:
    AsyncRestClient m_ReleasesFetch;
    bool m_Started = false;
    bool m_Completed = false;
    bool m_CheckExe = false;
    bool m_CheckDll = false;

    std::vector<Release> m_Releases;
    ExeUpdateInfo m_ExeInfo;
    DllUpdateInfo m_DllInfo;
};
