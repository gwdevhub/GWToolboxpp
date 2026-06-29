#include "stdafx.h"

#include <Defender.h>
#include <Path.h>
#include "WindowsDefender.h"

#include "Settings.h"

namespace fs = std::filesystem;

namespace {
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
    std::wstring output;
    const std::wstring command = L"Get-MpPreference | Select-Object -ExpandProperty ExclusionPath";

    if (!RunPowerShellCommand(command, output)) {
        // If we're not admin, this will fail, treat it as if it wasn't excluded but check registry later
        return false;
    }

    std::wstring normalized_path;
    try {
        normalized_path = fs::canonical(path).wstring();
        std::ranges::transform(normalized_path, normalized_path.begin(), towlower);
    } catch (...) {
        normalized_path = path.wstring();
        std::ranges::transform(normalized_path, normalized_path.begin(), towlower);
    }

    std::wstring line;
    std::wstringstream ss(output);

    while (std::getline(ss, line)) {
        line.erase(0, line.find_first_not_of(L" \t\r\n"));
        line.erase(line.find_last_not_of(L" \t\r\n") + 1);

        if (line.empty()) {
            continue;
        }

        std::wstring excluded_path = line;
        std::ranges::transform(excluded_path, excluded_path.begin(), towlower);

        if (excluded_path == normalized_path) {
            return true;
        }

        if (normalized_path.starts_with(excluded_path)) {
            return true;
        }
    }

    return false;
}

bool AddDefenderExceptions(const std::filesystem::path& exclusion_path,
                           const std::vector<std::filesystem::path>& controlled_folder_access_apps,
                           const bool quiet, std::wstring& error)
{
    // Collect only the changes we actually need, so a re-run with nothing to do is a silent no-op.
    // Re-allowing an app already on the Controlled Folder Access list is idempotent, so those are
    // unconditional; the folder exclusion we can cheaply check first.
    std::vector<std::wstring> cmdlets;
    if (!IsPathExcludedFromDefender(exclusion_path))
        cmdlets.push_back(L"Add-MpPreference -ExclusionPath '" + exclusion_path.wstring() + L"'");

    for (const auto& app : controlled_folder_access_apps) {
        std::error_code ec;
        if (std::filesystem::exists(app, ec))
            cmdlets.push_back(L"Add-MpPreference -ControlledFolderAccessAllowedApplications '" + app.wstring() + L"'");
    }

    if (cmdlets.empty())
        return true;

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
                L"Windows Security exceptions added successfully!",
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
