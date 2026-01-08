#include "stdafx.h"

#include <Path.h>
#include "WindowsDefender.h"

#include "Settings.h"
#include "Registry.h"

namespace fs = std::filesystem;

namespace {
    std::wstring ReadFile(const std::filesystem::path& path)
    {
        std::wifstream file(path);
        if (!file.is_open()) {
            return L"";
        }

        std::wstringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }


    bool ExecutePowerShellCommand(const std::wstring& command, std::wstring& output)
    {
        // Create a temporary file for output
        wchar_t temp_path[MAX_PATH];
        wchar_t temp_file[MAX_PATH];

        if (GetTempPathW(MAX_PATH, temp_path) == 0) {
            fprintf(stderr, "GetTempPathW failed (%lu)\n", GetLastError());
            return false;
        }

        if (GetTempFileNameW(temp_path, L"GWT", 0, temp_file) == 0) {
            fprintf(stderr, "GetTempFileNameW failed (%lu)\n", GetLastError());
            return false;
        }

        std::wstring full_command = L"powershell.exe -NoProfile -ExecutionPolicy Bypass -Command \"";
        full_command += command;
        full_command += L" | Out-File -FilePath '";
        full_command += temp_file;
        full_command += L"' -Encoding UTF8\"";

        STARTUPINFOW si = {sizeof(STARTUPINFOW)};
        PROCESS_INFORMATION pi = {nullptr};

        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;

        // Create a modifiable copy of the command string
        std::vector cmd_buffer(full_command.begin(), full_command.end());
        cmd_buffer.push_back(L'\0');

        BOOL success = CreateProcessW(
            nullptr,
            cmd_buffer.data(),
            nullptr,
            nullptr,
            FALSE,
            CREATE_NO_WINDOW,
            nullptr,
            nullptr,
            &si,
            &pi);

        if (!success) {
            fprintf(stderr, "CreateProcessW failed (%lu)\n", GetLastError());
            DeleteFileW(temp_file);
            return false;
        }

        DWORD exit_code = STILL_ACTIVE;
        DWORD elapsed = 0;
        for (elapsed = 0; exit_code == STILL_ACTIVE && elapsed < 5000; elapsed += 10) {
            Sleep(10);
            GetExitCodeProcess(pi.hProcess, &exit_code);
        }

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        output = ReadFile(temp_file);

        DeleteFileW(temp_file);

        return exit_code == 0;
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
    std::wstring output;
    const std::wstring command = L"Get-MpPreference | Select-Object -ExpandProperty ExclusionPath";

    if (!ExecutePowerShellCommand(command, output)) {
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


bool AddDefenderExclusion(const std::filesystem::path& path, const bool quiet, std::wstring& error)
{
    if (IsPathExcludedFromDefender(path)) {
        return true;
    }

    const bool is_admin = IsRunningAsAdmin();

    if (!is_admin && !quiet) {
        const int iRet = MessageBoxW(
            nullptr,
            L"GWToolbox would like to add itself to Windows Defender exclusions to prevent false positives.\n\n"
            L"This requires administrator privileges. Would you like to add the exclusion?",
            L"Windows Defender Exclusion",
            MB_YESNO | MB_ICONINFORMATION);

        if (iRet != IDYES) {
            MessageBoxW(
                nullptr,
                L"GWToolbox will likely not function correctly without Windows Defender exclusions.\n\n"
                L"You will be prompted again next time until the exclusion is added.",
                L"Windows Defender Exclusion - Warning",
                MB_OK | MB_ICONWARNING);
            return true;
        }
    }

    std::wstring command = L"Add-MpPreference -ExclusionPath '";
    command += path.wstring();
    command += L"'";

    bool success;
    if (is_admin) {
        std::wstring output;
        success = ExecutePowerShellCommand(command, output);
    }
    else {
        success = ExecutePowerShellCommandElevated(command, error);
    }

    if (!success) {
        if (!quiet) {
            std::wstring message = L"Failed to add Windows Defender exclusion.\n\n"
                L"You can manually add an exclusion for:\n";
            message += path.wstring();
            message += L"\n\nIn Windows Security -> Virus & threat protection -> Manage settings -> Exclusions";

            MessageBoxW(
                nullptr,
                message.c_str(),
                L"Windows Defender Exclusion",
                MB_OK | MB_ICONWARNING);
        }

        return false;
    }

    bool verified = false;
    if (is_admin) {
        verified = IsPathExcludedFromDefender(path);
    }
    else {
        // Not admin, but elevated command succeeded - can't think of anything better than to trust it worked
        verified = true;
    }

    if (verified) {
        if (!quiet) {
            MessageBoxW(
                nullptr,
                L"Windows Defender exclusion added successfully!",
                L"Windows Defender Exclusion",
                MB_OK | MB_ICONINFORMATION);
        }
    }
    else {
        if (!quiet) {
            std::wstring message = L"Failed to verify Windows Defender exclusion was added.\n\n";
            message += L"GWToolbox may not function correctly without this exclusion.\n\n";
            message += L"Please manually add an exclusion for:\n";
            message += path.wstring();
            message += L"\n\nIn Windows Security -> Virus & threat protection -> Manage settings -> Exclusions";

            MessageBoxW(
                nullptr,
                message.c_str(),
                L"Windows Defender Exclusion - WARNING",
                MB_OK | MB_ICONWARNING);
        }
    }

    return verified;
}
