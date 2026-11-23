#include "stdafx.h"

#include <filesystem>
#include <fstream>
#include <system_error>
#include <vector>

#include "DllDownload.h"
#include "Install.h"
#include "Installation.h"
#include "Releases.h"

namespace fs = std::filesystem;

bool FetchDllRelease(DllRelease& release, std::string& error)
{
    std::vector<Release> releases;
    if (!FetchReleases(releases, error)) {
        return false;
    }
    const FoundAsset found = FindNewestAsset(releases, {"GWToolbox.dll", "GWToolboxdll.dll"});
    if (!found.asset) {
        error = "No GWToolboxdll.dll was found in the GWToolbox releases.";
        return false;
    }
    if (found.asset->browser_download_url.empty()) {
        error = "The GWToolboxdll.dll release has no download address.";
        return false;
    }
    release.version = VersionFromTag(found.release->tag_name);
    release.changelog = "GWToolbox " + found.release->tag_name + "\n\n" + found.release->body.value_or(std::string());
    release.url = found.asset->browser_download_url;
    release.size = found.asset->size;
    release.sha256 = Sha256FromDigest(found.asset->digest);
    return true;
}

bool DownloadDllFile(const DllRelease& release, const fs::path& directory, const std::function<void(int)>& progress, std::string& error)
{
    std::string content;
    if (!DownloadVerified(release.url, release.size, release.sha256, progress, content, error)) {
        return false;
    }

    const fs::path target = directory / "GWToolboxdll.dll";
    const fs::path partial = directory / "GWToolboxdll.dll.part";
    {
        std::ofstream out(partial, std::ios::binary | std::ios::trunc);
        out.write(content.data(), static_cast<std::streamsize>(content.size()));
        if (!out) {
            error = "GWToolboxdll.dll couldn't be saved to " + partial.string() + ".";
            return false;
        }
    }

    if (!MoveFileExW(partial.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        std::error_code ignored;
        fs::remove(partial, ignored);
        error = std::format("GWToolboxdll.dll couldn't replace {} (error {}). Close any game using it and try again.", target.string(), GetLastError());
        return false;
    }
    RecordInstalledDll(target);
    return true;
}

fs::path SuggestedInstallDirectory()
{
    return GetInstallationDir().parent_path();
}

bool PrepareInstallDirectory(const fs::path& directory, std::string& error)
{
    std::error_code ec;
    fs::create_directories(directory, ec);
    if (ec || !fs::is_directory(directory, ec)) {
        error = "The folder " + directory.string() + " couldn't be created.";
        return false;
    }
    const fs::path probe = directory / ".gwtoolbox-write-test";
    std::ofstream(probe) << "";
    const bool writable = fs::exists(probe, ec);
    fs::remove(probe, ec);
    if (!writable) {
        error = "The folder " + directory.string() + " isn't writable.";
        return false;
    }
    return true;
}

bool UseDll(const fs::path& dll, std::string& error)
{
    std::error_code ec;
    if (dll.extension() != ".dll" || !fs::is_regular_file(dll, ec)) {
        error = "Choose a GWToolboxdll.dll file.";
        return false;
    }
    RecordInstalledDll(dll);
    return true;
}
