#include "stdafx.h"

#include "Installation.h"

#include <Path.h>
#include "Install.h"
#include "LauncherSettings.h"
#include "WebViewWindow.h"

namespace fs = std::filesystem;

fs::path launcherSettingsFile()
{
    fs::path directory;
    PathGetProgramDirectory(directory);
    return directory / L"GWToolbox.json";
}

fs::path LocalDllPath()
{
    fs::path directory;
    if (!PathGetProgramDirectory(directory)) {
        return {};
    }
    return directory / L"GWToolboxdll.dll";
}

fs::path DefaultDllPath()
{
    const fs::path installation = GetInstallationDir().parent_path();
    if (installation.empty()) {
        return {};
    }
    return installation / L"GWToolboxdll.dll";
}
