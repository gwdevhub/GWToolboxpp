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

bool isLauncherAsset(const std::string& name)
{
    return name == "GWToolbox.exe";
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
    std::string listing;
    if (!FetchReleaseListing(listing, error)) {
        return false;
    }
    if (!ReleaseFromListing(listing, isLauncherAsset, std::string(), release, error)) {
        return false;
    }
    if (FileMatchesRelease(RunningLauncherPath(), release.size, release.sha256)) {
        error.clear();
        return false;
    }
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
