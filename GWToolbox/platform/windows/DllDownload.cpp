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
    std::string listing;
    if (!FetchReleaseListing(listing, error)) {
        return false;
    }
    return DllReleaseFromListing(listing, release, error);
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

