#pragma once

#include <string>

// Runs a PowerShell command, capturing its stdout into `output`. Returns true on exit code 0.
bool RunPowerShellCommand(const std::wstring& command, std::wstring& output);

// Searches the Windows Defender Operational log for a recent malware/ASR/Controlled-Folder-Access block whose message mentions `needle`, within the last `seconds`; on a hit returns true and sets `detail` to the event message.
bool FindRecentDefenderBlock(const std::wstring& needle, unsigned seconds, std::wstring& detail);

// Deep links into the troubleshooting page so error popups can point users straight at the fix.
namespace Troubleshooting {
    inline constexpr wchar_t AntivirusExclusions[] = L"https://www.gwtoolbox.com/docs/troubleshooting/#antivirus-exclusions";
    inline constexpr wchar_t ControlledFolderAccess[] = L"https://www.gwtoolbox.com/docs/troubleshooting/#controlled-folder-access";
    inline constexpr wchar_t CantReadMemory[] = L"https://www.gwtoolbox.com/docs/troubleshooting/#toolbox-cant-read-guild-wars-memory";
    inline constexpr wchar_t CrashDumps[] = L"https://www.gwtoolbox.com/docs/troubleshooting/#crash-dump-errors";
    inline constexpr wchar_t BlockedFiles[] = L"https://www.gwtoolbox.com/docs/troubleshooting/#blocked-or-quarantined-files";
}

// Shows an error message box; when `url` is non-empty, adds a Yes/No prompt whose Yes opens that troubleshooting page in the browser. Pass extra MB_* flags (icon/modality) via `flags`.
void ShowTroubleshootingError(const std::wstring& message, const wchar_t* title, const wchar_t* url, unsigned flags = 0);
