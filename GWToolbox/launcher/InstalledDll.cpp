#include "Installation.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

namespace fs = std::filesystem;

namespace {

bool localDllOnly = false;

}

void SetLocalDllOnly(const bool enabled)
{
    localDllOnly = enabled;
}

fs::path FindInstalledDll()
{
    std::error_code ec;
    const fs::path local = LocalDllPath();
    if (localDllOnly) {
        return fs::is_regular_file(local, ec) ? local : fs::path{};
    }
    if (const auto dll = dllFromSettings()) {
        return *dll;
    }
    const fs::path installed = DefaultDllPath();
    if (fs::is_regular_file(installed, ec)) {
        RecordInstalledDll(installed);
        return installed;
    }
    return {};
}

fs::path SuggestedInstallDirectory()
{
    return DefaultDllPath().parent_path();
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
