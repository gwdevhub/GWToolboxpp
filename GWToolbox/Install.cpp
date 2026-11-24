#include "stdafx.h"

#include <Path.h>
#include "DllDownload.h"
#include "Install.h"
#include "Utf8.h"
#include "WasmInject.h"
#include "WindowsDefender.h"
#include "WindowsInject.h"

namespace fs = std::filesystem;

bool GetFileSize(const wchar_t* path, uint64_t* file_size)
{
    const HANDLE hFile = CreateFileW(
        path,
        FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr);

    if (hFile == INVALID_HANDLE_VALUE) {
        // @Enhancement:
        // Deal with access denied?
        fprintf(stderr, "CreateFileW failed (%lu)\n", GetLastError());
        return false;
    }

    LARGE_INTEGER FileSize;
    if (!GetFileSizeEx(hFile, &FileSize)) {
        fprintf(stderr, "GetFileSizeEx failed (%lu)\n", GetLastError());
        return false;
    }

    CloseHandle(hFile);

    *file_size = static_cast<uint64_t>(FileSize.QuadPart);
    return true;
}

fs::path GetInstallationDir()
{
    std::filesystem::path docpath;
    if (!PathGetDocumentsPath(docpath, L"GWToolboxpp")) {
        return {};
    }
    std::filesystem::path computer_name;
    if (!PathGetComputerName(computer_name)) {
        return {};
    }
    return docpath / computer_name;
}

fs::path EnsureInstallationDirectoryExist()
{
    const auto installation_dir = GetInstallationDir();
    if (!installation_dir.empty() && PathCreateDirectorySafe(installation_dir))
        return installation_dir;
    return {};
}

// Optional: copies gwtoolbox.gwmod alongside the installed dll if one sits next to the launcher (no release asset exists yet); its absence is the ordinary case and must never fail Install().
static void InstallLocalWasmGwmodIfPresent(const fs::path& install_path)
{
    fs::path source_dir;
    if (!PathGetProgramDirectory(source_dir)) return;
    const fs::path source = source_dir / WASM_GWMOD_FILENAME;
    if (!fs::exists(source)) return;

    const fs::path dest = install_path.parent_path() / WASM_GWMOD_FILENAME;
    if (source == dest) return;
    if (!PathSafeCopy(source, dest, true)) {
        fprintf(stderr, "Found %ls but failed to copy it to %ls\n", source.c_str(), dest.c_str());
    }
}

static bool DownloadDllForInstall(const fs::path& install_dir, std::wstring& error)
{
    DllRelease release;
    std::string failure;
    if (!FetchDllRelease(release, failure)) {
        error = FromUtf8(failure);
        return false;
    }
    const auto progress = [](const int percent) {
        fprintf(stderr, "\rDownloading GWToolboxdll.dll: %d%%", percent);
    };
    if (!DownloadDllFile(release, install_dir, progress, failure)) {
        error = FromUtf8(failure);
        return false;
    }
    fprintf(stderr, "\n");
    return true;
}

bool Install(const bool quiet, std::wstring& error)
{
    if (IsInstalled())
        return true;

    // Grant the exclusion and Controlled Folder Access permissions before writing into Documents, so CFA can't block the install.
    fs::path docs_dir;
    if (PathGetDocumentsPath(docs_dir, L"GWToolboxpp")) {
        std::vector<fs::path> cfa_apps = GetGuildWarsExecutablePaths();
        fs::path running_exe;
        if (PathGetExeFullPath(running_exe))
            cfa_apps.push_back(running_exe); // the launcher doing the writing right now (e.g. from Downloads)
        AddDefenderExceptions(docs_dir, cfa_apps, quiet, error); // Silent fail, it shows its own messages
    }

    fs::path install_path = EnsureInstallationDirectoryExist();
    if (install_path.empty())
        return error = L"EnsureInstallationDirectoryExist failed", false;

    if (!DownloadDllForInstall(install_path.parent_path(), error))
        return false;
    const auto dll_path = install_path.parent_path() / "GWToolboxdll.dll";
    if (!fs::exists(dll_path)) {
        return error = std::format(
                   L"Application @ {} not found after installation; it may have been quarantined by anti virus software!\n\nExclude the {} directory in your anti virus settings and re-launch.", dll_path.wstring(),
                   dll_path.parent_path().wstring()
               ), false;
    }
    InstallLocalWasmGwmodIfPresent(install_path);

    if (!quiet) {
        ShowMessageBoxW(nullptr, L"Installation successful", L"Installation", 0);
    }

    return true;
}

bool IsInstalled()
{
    const auto install_path = GetInstallationDir();
    if (install_path.empty())
        return false;

    return exists(install_path.parent_path() / L"GWToolboxdll.dll");
}
