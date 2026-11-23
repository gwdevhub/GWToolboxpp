#include "stdafx.h"

#include <filesystem>
#include <string>

#include "Elevation.h"
#include "LauncherUpdate.h"
#include "Settings.h"

namespace fs = std::filesystem;

bool CanRestartLauncherAsAdmin()
{
    return !IsRunningAsAdmin();
}

bool RestartLauncherAsAdmin(std::string& error)
{
    const fs::path exe = RunningLauncherPath();
    const std::wstring workdir = fs::current_path().wstring();
    if (!CreateProcessInt(exe.c_str(), GetCommandLineWithoutProgram(), workdir.c_str(), true)) {
        error = "GWToolbox couldn't be restarted as administrator. Approve the Windows prompt, or start it from an administrator command prompt.";
        return false;
    }
    return true;
}
