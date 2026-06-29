#include "stdafx.h"

#include <format>
#include <sstream>

#include <Defender.h>
#include "AvReadiness.h"

namespace fs = std::filesystem;

namespace {
    // Windows Security deep links. Exact page monikers drift across Windows builds; these land the user on the right
    // area (the window falls back to Windows Security's home if a build rejects them). Verify on target builds.
    constexpr wchar_t kUriExclusions[] = L"windowsdefender://threat";
    constexpr wchar_t kUriControlledFolderAccess[] = L"windowsdefender://controlledfolderaccess";

    std::wstring Lower(std::wstring s)
    {
        std::ranges::transform(s, s.begin(), towlower);
        return s;
    }

    // True if `path` is the same as, or sits under, any excluded path.
    bool CoveredByExclusion(const std::wstring& path, const std::vector<std::wstring>& exclusions)
    {
        const std::wstring p = Lower(path);
        for (const auto& e : exclusions) {
            const std::wstring ex = Lower(e);
            if (p == ex || p.starts_with(ex)) return true;
        }
        return false;
    }

    // True if an app with this filename appears anywhere in the Controlled Folder Access allow-list.
    bool AppAllowed(const std::wstring& filename, const std::vector<std::wstring>& allowed)
    {
        const std::wstring want = L"\\" + Lower(filename);
        for (const auto& a : allowed)
            if (Lower(a).ends_with(want)) return true;
        return false;
    }
}

AvReadiness GetAvReadiness(const fs::path& program_dir, const fs::path& data_dir)
{
    AvReadiness r;

    // One read-only call, wrapped in &{} so every line (not just the last statement) is captured by Out-File.
    const std::wstring command =
        L"&{$ErrorActionPreference='SilentlyContinue';"
        L"'MODE='+(Get-MpComputerStatus).AMRunningMode;"
        L"$p=Get-MpPreference;'CFA='+[int]$p.EnableControlledFolderAccess;"
        L"$p.ExclusionPath|%{'EX='+$_};"
        L"$p.ControlledFolderAccessAllowedApplications|%{'APP='+$_};"
        L"Get-CimInstance -Namespace root/SecurityCenter2 -ClassName AntiVirusProduct|%{'AV='+$_.displayName}}";

    std::wstring output;
    r.read_ok = RunPowerShellCommand(command, output);

    std::wstring mode;
    int cfa = 0;
    std::vector<std::wstring> exclusions, allowed_apps, av_names;
    std::wstring line;
    std::wstringstream ss(output);
    while (std::getline(ss, line)) {
        while (!line.empty() && (line.back() == L'\r' || line.back() == L' ')) line.pop_back();
        if (line.starts_with(L"MODE=")) mode = line.substr(5);
        else if (line.starts_with(L"CFA=")) cfa = _wtoi(line.substr(4).c_str());
        else if (line.starts_with(L"EX=")) exclusions.push_back(line.substr(3));
        else if (line.starts_with(L"APP=")) allowed_apps.push_back(line.substr(4));
        else if (line.starts_with(L"AV=")) av_names.push_back(line.substr(3));
    }

    r.defender_active = Lower(mode) == L"normal"; // any other mode means a third-party AV has taken over real-time scanning
    for (const auto& name : av_names)
        if (!Lower(name).contains(L"defender") && !Lower(name).contains(L"windows")) { r.third_party_av = name; break; }

    // The data folder always holds gwca.dll / gMod.dll / the dll / plugins; the program folder holds the launcher + dll.
    r.checks.push_back({std::format(L"Antivirus folder exclusion for {}", data_dir.wstring()), CoveredByExclusion(data_dir.wstring(), exclusions), kUriExclusions, L"Add a folder exclusion so Toolbox's files aren't quarantined."});
    if (Lower(program_dir.wstring()) != Lower(data_dir.wstring()))
        r.checks.push_back({std::format(L"Antivirus folder exclusion for {}", program_dir.wstring()), CoveredByExclusion(program_dir.wstring(), exclusions), kUriExclusions, L"Add a folder exclusion for the program folder."});

    // Controlled Folder Access only matters when it is switched on; then Gw.exe (which hosts Toolbox) and the launcher must be allowed.
    if (cfa == 1) {
        r.checks.push_back({L"Allow Guild Wars through Controlled Folder Access", AppAllowed(L"Gw.exe", allowed_apps), kUriControlledFolderAccess, L"Allow Gw.exe so Toolbox can save settings to your Documents."});
        r.checks.push_back({L"Allow GWToolbox through Controlled Folder Access", AppAllowed(L"GWToolbox.exe", allowed_apps), kUriControlledFolderAccess, L"Allow GWToolbox.exe through Controlled Folder Access."});
    }

    return r;
}
