#include "stdafx.h"

#include "Defender.h"

#include <shellapi.h>

#include <fstream>
#include <sstream>
#include <vector>

namespace {
    std::wstring ReadFileToWString(const std::filesystem::path& path)
    {
        std::wifstream file(path);
        if (!file.is_open()) {
            return L"";
        }
        std::wstringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    void TrimWhitespace(std::wstring& s)
    {
        const auto first = s.find_first_not_of(L" \t\r\n");
        if (first == std::wstring::npos) {
            s.clear();
            return;
        }
        s.erase(0, first);
        s.erase(s.find_last_not_of(L" \t\r\n") + 1);
    }
}

bool RunPowerShellCommand(const std::wstring& command, std::wstring& output)
{
    wchar_t temp_path[MAX_PATH];
    wchar_t temp_file[MAX_PATH];

    if (GetTempPathW(MAX_PATH, temp_path) == 0) {
        fwprintf(stderr, L"%S: GetTempPathW failed (%lu)\n", __func__, GetLastError());
        return false;
    }
    if (GetTempFileNameW(temp_path, L"GWT", 0, temp_file) == 0) {
        fwprintf(stderr, L"%S: GetTempFileNameW failed (%lu)\n", __func__, GetLastError());
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

    std::vector<wchar_t> cmd_buffer(full_command.begin(), full_command.end());
    cmd_buffer.push_back(L'\0');

    const BOOL success = CreateProcessW(nullptr, cmd_buffer.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    if (!success) {
        fwprintf(stderr, L"%S: CreateProcessW failed (%lu)\n", __func__, GetLastError());
        DeleteFileW(temp_file);
        return false;
    }

    DWORD exit_code = STILL_ACTIVE;
    for (DWORD elapsed = 0; exit_code == STILL_ACTIVE && elapsed < 5000; elapsed += 10) {
        Sleep(10);
        GetExitCodeProcess(pi.hProcess, &exit_code);
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    output = ReadFileToWString(temp_file);
    DeleteFileW(temp_file);

    return exit_code == 0;
}

bool FindRecentDefenderBlock(const std::wstring& needle, const unsigned seconds, std::wstring& detail)
{
    // Double up single quotes so the needle is safe inside the PowerShell single-quoted string.
    std::wstring escaped;
    for (const wchar_t c : needle) {
        escaped += c;
        if (c == L'\'') escaped += c;
    }

    // 1116-1119 malware detection/remediation, 1121 ASR block, 1123 Controlled Folder Access block.
    const std::wstring command =
        L"Get-WinEvent -FilterHashtable @{LogName='Microsoft-Windows-Windows Defender/Operational';"
        L"Id=1116,1117,1118,1119,1121,1123;StartTime=(Get-Date).AddSeconds(-" + std::to_wstring(seconds) + L")} -ErrorAction SilentlyContinue|"
        L"Where-Object{$_.Message -like '*" + escaped + L"*'}|"
        L"Select-Object -First 1 -ExpandProperty Message";

    std::wstring output;
    if (!RunPowerShellCommand(command, output))
        return false;

    TrimWhitespace(output);
    if (output.empty())
        return false;

    detail = output;
    return true;
}

void ShowTroubleshootingError(const std::wstring& message, const wchar_t* title, const wchar_t* url, const unsigned flags)
{
    const unsigned base = flags ? flags : MB_ICONERROR | MB_SYSTEMMODAL | MB_SETFOREGROUND | MB_TOPMOST;
    if (url && *url) {
        const std::wstring body = message + L"\n\nOpen the troubleshooting guide for step-by-step instructions?\n" + url;
        if (MessageBoxW(nullptr, body.c_str(), title, base | MB_YESNO) == IDYES)
            ShellExecuteW(nullptr, L"open", url, nullptr, nullptr, SW_SHOWNORMAL);
        return;
    }
    MessageBoxW(nullptr, message.c_str(), title, base | MB_OK);
}
