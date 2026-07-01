#include "stdafx.h"

#include <Defender.h>
#include <Path.h>
#include "WindowsDefender.h"

#include "Inject.h"
#include "Settings.h"

namespace fs = std::filesystem;

namespace {
    // A snapshot of the Windows Defender settings that decide whether Toolbox can use its folder.
    struct DefenderState {
        bool readable = false;             // false when Get-MpPreference couldn't be read (locked-down host, third-party AV, ...)
        std::vector<std::wstring> exclusion_paths;  // lowercased
        int controlled_folder_access = 0;  // 0 disabled, 1 enabled, 2 audit (only 1 actually blocks writes)
        std::vector<std::wstring> cfa_apps; // lowercased, Controlled Folder Access allowed applications
    };

    void TrimWhitespace(std::wstring& s)
    {
        const auto first = s.find_first_not_of(L" \t\r\n\xFEFF");
        if (first == std::wstring::npos) {
            s.clear();
            return;
        }
        s.erase(0, first);
        s.erase(s.find_last_not_of(L" \t\r\n") + 1);
    }

    std::wstring ToLower(std::wstring s)
    {
        std::ranges::transform(s, s.begin(), towlower);
        return s;
    }

    std::wstring LowerCanonical(const fs::path& path)
    {
        std::error_code ec;
        const fs::path canonical = fs::canonical(path, ec);
        return ToLower((ec ? path : canonical).wstring());
    }

    // True if `target` is one of `list`, or (when prefix is set) sits inside a listed folder.
    bool ListCoversPath(const std::vector<std::wstring>& list, const std::wstring& target, const bool prefix)
    {
        for (const auto& entry : list) {
            if (entry == target)
                return true;
            if (prefix && target.starts_with(entry))
                return true;
        }
        return false;
    }

    // One PowerShell spawn reads exclusion paths, CFA mode and allowed apps; the <<<OK>>> sentinel only prints on a successful read.
    bool QueryDefenderState(DefenderState& out)
    {
        const std::wstring command =
            L"&{ try { $p = Get-MpPreference -ErrorAction Stop } catch { return }; "
            L"'<<<OK>>>'; $p.ExclusionPath; "
            L"'<<<CFA>>>'; [int]$p.EnableControlledFolderAccess; "
            L"'<<<APPS>>>'; $p.ControlledFolderAccessAllowedApplications }";

        std::wstring output;
        if (!RunPowerShellCommand(command, output))
            return false;

        enum { Header, Exclusions, ControlledFolderAccess, Apps } section = Header;
        std::wstring line;
        std::wstringstream ss(output);
        while (std::getline(ss, line)) {
            TrimWhitespace(line);
            if (line.empty())
                continue;

            // .find rather than == so a UTF-8 BOM glued to the first line still matches the sentinel.
            if (line.find(L"<<<OK>>>") != std::wstring::npos) { out.readable = true; section = Exclusions; continue; }
            if (line.find(L"<<<CFA>>>") != std::wstring::npos) { section = ControlledFolderAccess; continue; }
            if (line.find(L"<<<APPS>>>") != std::wstring::npos) { section = Apps; continue; }
            if (!out.readable)
                continue;

            switch (section) {
                case Exclusions: out.exclusion_paths.push_back(ToLower(line)); break;
                case ControlledFolderAccess: out.controlled_folder_access = _wtoi(line.c_str()); break;
                case Apps: out.cfa_apps.push_back(ToLower(line)); break;
                default: break;
            }
        }

        return out.readable;
    }

    // Cmdlets needed to fix `state`: the exclusion unless already covered, plus CFA apps only while CFA is on.
    std::vector<std::wstring> RequiredCmdlets(const DefenderState& state,
                                              const fs::path& exclusion_path,
                                              const std::vector<fs::path>& cfa_apps)
    {
        std::vector<std::wstring> cmdlets;

        if (!state.readable || !ListCoversPath(state.exclusion_paths, LowerCanonical(exclusion_path), true))
            cmdlets.push_back(L"Add-MpPreference -ExclusionPath '" + exclusion_path.wstring() + L"'");

        if (state.controlled_folder_access == 1) {
            for (const auto& app : cfa_apps) {
                std::error_code ec;
                if (fs::exists(app, ec) && !ListCoversPath(state.cfa_apps, LowerCanonical(app), false))
                    cmdlets.push_back(L"Add-MpPreference -ControlledFolderAccessAllowedApplications '" + app.wstring() + L"'");
            }
        }

        return cmdlets;
    }

    bool ExecutePowerShellCommandElevated(const std::wstring& command, std::wstring& error)
    {
        std::wstring ps_command = L"-NoProfile -ExecutionPolicy Bypass -Command \"";
        ps_command += command;
        ps_command += L"\"";

        SHELLEXECUTEINFOW sei = {sizeof(SHELLEXECUTEINFOW)};
        sei.lpVerb = L"runas"; // Request elevation
        sei.lpFile = L"powershell.exe";
        sei.lpParameters = ps_command.c_str();
        sei.nShow = SW_HIDE;
        sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC;

        if (!ShellExecuteExW(&sei))
            return error = std::format(L"ShellExecuteExW failed ({})", GetLastError()), false;

        if (!sei.hProcess)
            return error = L"SHELLEXECUTEINFOW.hProcess is empty in ExecutePowerShellCommandElevated", false;

        DWORD exit_code = STILL_ACTIVE;
        DWORD elapsed = 0;
        for (elapsed = 0; exit_code == STILL_ACTIVE && elapsed < 5000; elapsed += 10) {
            Sleep(10);
            GetExitCodeProcess(sei.hProcess, &exit_code);
        }

        CloseHandle(sei.hProcess);

        if (exit_code != 0)
            return error = std::format(L"GetExitCodeProcess returned {}", exit_code), false;
        return true;
    }
}

bool IsPathExcludedFromDefender(const std::filesystem::path& path)
{
    DefenderState state;
    if (!QueryDefenderState(state)) {
        // If we can't read Defender (e.g. not admin on a locked-down host), treat it as not excluded.
        return false;
    }
    return ListCoversPath(state.exclusion_paths, LowerCanonical(path), true);
}

bool AddDefenderExceptions(const std::filesystem::path& exclusion_path,
                           const std::vector<std::filesystem::path>& controlled_folder_access_apps,
                           const bool quiet, std::wstring& error)
{
    // Collect only the changes we actually need, so a re-run with nothing to do is a silent no-op.
    DefenderState state;
    QueryDefenderState(state);
    const std::vector<std::wstring> cmdlets = RequiredCmdlets(state, exclusion_path, controlled_folder_access_apps);

    if (cmdlets.empty())
        return true;

    // Controlled Folder Access fixes a process's allowed status at launch, so allowing an
    // already-running Gw.exe now won't let the injected Toolbox write to Documents until that
    // client restarts. Note it here so we can tell the user once the change lands.
    bool allowing_running_gw = false;
    if (state.controlled_folder_access == 1) {
        for (const auto& gw : GetGuildWarsExecutablePaths()) {
            if (!ListCoversPath(state.cfa_apps, LowerCanonical(gw), false)) {
                allowing_running_gw = true;
                break;
            }
        }
    }

    const bool is_admin = IsRunningAsAdmin();

    if (!is_admin && !quiet) {
        const int iRet = ShowMessageBoxW(
            nullptr,
            L"GWToolbox would like to add Windows Security exceptions for itself (a Defender exclusion and "
            L"Controlled Folder Access permissions) to prevent false positives and let it save settings and crash dumps.\n\n"
            L"This requires administrator privileges. Would you like to add them?",
            L"Windows Defender Exclusion",
            MB_YESNO | MB_ICONINFORMATION);

        if (iRet != IDYES) {
            ShowMessageBoxW(
                nullptr,
                L"GWToolbox will likely not function correctly without these exceptions.\n\n"
                L"You will be prompted again next time until they are added.",
                L"Windows Defender Exclusion - Warning",
                MB_OK | MB_ICONWARNING);
            return true;
        }
    }

    // One elevated invocation for every cmdlet, so the user sees at most a single UAC prompt.
    std::wstring command;
    for (size_t i = 0; i < cmdlets.size(); i++) {
        if (i) command += L"; ";
        command += cmdlets[i];
    }

    bool success;
    if (is_admin) {
        std::wstring output;
        success = RunPowerShellCommand(command, output);
    }
    else {
        success = ExecutePowerShellCommandElevated(command, error);
    }

    if (!success) {
        if (!quiet) {
            std::wstring message = L"Failed to add Windows Security exceptions for GWToolbox.\n\n"
                L"You can add them manually in Windows Security -> Virus & threat protection:\n"
                L"- Add a folder exclusion for:\n  ";
            message += exclusion_path.wstring();
            message += L"\n- Allow Guild Wars and GWToolbox through Controlled Folder Access (Ransomware protection).";

            ShowMessageBoxW(
                nullptr,
                message.c_str(),
                L"Windows Defender Exclusion",
                MB_OK | MB_ICONWARNING);
        }

        return false;
    }

    // The folder exclusion we can verify directly; Controlled Folder Access entries we trust to the exit code.
    const bool verified = !is_admin || IsPathExcludedFromDefender(exclusion_path);

    if (!quiet) {
        if (verified) {
            ShowMessageBoxW(
                nullptr,
                allowing_running_gw
                    ? L"Windows Security exceptions added successfully!\n\n"
                      L"Guild Wars is now allowed through Controlled Folder Access (Ransomware protection), "
                      L"but Windows only applies that to newly-started programs. Close and re-open Guild Wars, "
                      L"then start Toolbox again, so it can save your settings and crash dumps."
                    : L"Windows Security exceptions added successfully!",
                L"Windows Defender Exclusion",
                MB_OK | MB_ICONINFORMATION);
        }
        else {
            std::wstring message = L"Failed to verify the Windows Defender exclusion was added.\n\n";
            message += L"GWToolbox may not function correctly without this exclusion.\n\n";
            message += L"Please manually add an exclusion for:\n";
            message += exclusion_path.wstring();
            message += L"\n\nIn Windows Security -> Virus & threat protection -> Manage settings -> Exclusions";

            ShowMessageBoxW(
                nullptr,
                message.c_str(),
                L"Windows Defender Exclusion - WARNING",
                MB_OK | MB_ICONWARNING);
        }
    }

    return verified;
}

void EnsureDefenderReadiness(const std::filesystem::path& exclusion_path,
                             const std::vector<std::filesystem::path>& controlled_folder_access_apps)
{
    DefenderState state;
    if (!QueryDefenderState(state)) {
        // Can't read Defender state (locked-down host, third-party AV): don't nag on a guess.
        return;
    }

    if (RequiredCmdlets(state, exclusion_path, controlled_folder_access_apps).empty())
        return;

    // Something's missing: hand off to the prompt-and-fix path (it surfaces its own errors).
    std::wstring error;
    AddDefenderExceptions(exclusion_path, controlled_folder_access_apps, false, error);
}
