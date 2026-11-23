#include "stdafx.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

#include "LauncherUpdate.h"
#include "Releases.h"
#include "Settings.h"

namespace fs = std::filesystem;

namespace {

fs::path oldLauncherPath(const fs::path& current)
{
    return fs::path(current.wstring() + L".old");
}

fs::path stagedLauncherPath(const fs::path& current)
{
    return current.parent_path() / (L".new" + current.extension().wstring());
}

}

fs::path RunningLauncherPath()
{
    static const fs::path path = [] {
        std::wstring buffer(MAX_PATH, L'\0');
        const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        buffer.resize(length);
        return fs::path(buffer);
    }();
    return path;
}

bool FetchLauncherRelease(DllRelease& release, std::string& error)
{
    std::vector<Release> releases;
    if (!FetchReleases(releases, error)) {
        return false;
    }
    const FoundAsset found = FindNewestAsset(releases, {"GWToolbox.exe"});
    if (!found.asset) {
        error.clear();
        return false;
    }

    const std::string sha256 = Sha256FromDigest(found.asset->digest);
    if (FileMatchesRelease(RunningLauncherPath(), found.asset->size, sha256)) {
        error.clear();
        return false;
    }

    release.version = VersionFromTag(found.release->tag_name);
    release.changelog = "GWToolbox " + found.release->tag_name + "\n\n" + found.release->body.value_or(std::string());
    release.url = found.asset->browser_download_url;
    release.size = found.asset->size;
    release.sha256 = sha256;
    return true;
}

bool DownloadLauncherRelease(const DllRelease& release, const fs::path& staged, const std::function<void(int)>& progress, std::string& error)
{
    std::string content;
    if (!DownloadVerified(release.url, release.size, release.sha256, progress, content, error)) {
        return false;
    }

    std::ofstream out(staged, std::ios::binary | std::ios::trunc);
    out.write(content.data(), static_cast<std::streamsize>(content.size()));
    if (!out) {
        error = "The update couldn't be written to " + staged.string() + ".";
        return false;
    }
    return true;
}

bool InstallLauncherRelease(const fs::path& current, const fs::path& staged, std::string& error)
{
    const fs::path old = oldLauncherPath(current);
    std::error_code ignored;
    fs::remove(old, ignored);

    if (!MoveFileW(current.c_str(), old.c_str())) {
        error = std::format("The launcher couldn't be moved aside for the update (error {}).", GetLastError());
        return false;
    }
    if (!MoveFileW(staged.c_str(), current.c_str())) {
        error = std::format("The update couldn't be put in place (error {}).", GetLastError());
        MoveFileW(old.c_str(), current.c_str());
        return false;
    }
    return true;
}

bool RestartLauncher(std::string& error)
{
    const fs::path exe = RunningLauncherPath();
    const std::wstring workdir = fs::current_path().wstring();
    if (!CreateProcessInt(exe.c_str(), GetCommandLineWithoutProgram(), workdir.c_str())) {
        error = "GWToolbox was updated, but it couldn't be restarted. Start it again from its folder.";
        return false;
    }
    return true;
}

void RemoveLauncherLeftovers(const fs::path& current)
{
    std::error_code ignored;
    fs::remove(oldLauncherPath(current), ignored);
    fs::remove(stagedLauncherPath(current), ignored);
}
