#pragma once

#include <optional>

#include <RestClient.h>

#include "LauncherWindow.h"

struct Asset {
    std::string name{};
    size_t size = 0;
    std::string browser_download_url{};
    std::optional<std::string> digest{};
};

struct Release {
    std::string tag_name{};
    std::optional<std::string> body{};
    std::vector<Asset> assets{};
};

struct ExeUpdateInfo {
    const Asset* asset = nullptr;
    std::string tag_name;
    std::vector<std::filesystem::path> targets;
};

struct DllUpdateInfo {
    bool asset_found = false;
    bool up_to_date = false;
    const Asset* asset = nullptr;
    std::string tag_name;
    std::filesystem::path dll_path;
};

struct GwmodUpdateInfo {
    bool asset_found = false;
    bool up_to_date = false;
    const Asset* asset = nullptr;
    std::string tag_name;
    std::filesystem::path gwmod_path;
};

std::string GetGwmodVersion(const std::filesystem::path& gwmod_path);

class DownloadWindow : public LauncherWindow {
public:
    DownloadWindow();
    DownloadWindow(const DownloadWindow&) = delete;
    DownloadWindow(DownloadWindow&&) = delete;
    ~DownloadWindow() override = default;

    DownloadWindow& operator=(const DownloadWindow&) = delete;
    DownloadWindow& operator=(DownloadWindow&&) = delete;

    static bool DownloadDll(std::wstring& error);
    static bool ApplyUpdates(const std::vector<Release>& releases, const ExeUpdateInfo& exe_info, const DllUpdateInfo& dll_info, const GwmodUpdateInfo& gwmod_info, std::wstring& error);

    void Show(const std::string& changelog);
    void SetStatus(const std::wstring& text);
    void SetProgress(size_t percent);

private:
    static bool DownloadDll(const std::vector<Release>& releases, std::wstring& error);
    static bool DownloadAssetWithProgress(DownloadWindow& window, const Asset& asset, std::string& out_content, std::wstring& error);
};

class UpdateChecker {
public:
    ~UpdateChecker();

    void Start(bool check_exe, bool check_dll);

    bool IsChecking() const { return m_Started && !m_Completed; }

    bool Poll();

    bool IsExeUpdateAvailable() const { return m_ExeInfo.asset != nullptr; }
    bool IsDllUpdateAvailable() const { return m_DllInfo.asset != nullptr && !m_DllInfo.up_to_date; }
    bool IsGwmodUpdateAvailable() const { return m_GwmodInfo.asset != nullptr && !m_GwmodInfo.up_to_date; }
    bool IsAnyUpdateAvailable() const { return IsExeUpdateAvailable() || IsDllUpdateAvailable() || IsGwmodUpdateAvailable(); }

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
    GwmodUpdateInfo m_GwmodInfo;
};
