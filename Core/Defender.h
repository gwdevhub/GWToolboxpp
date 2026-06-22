#pragma once

#include <string>

// Runs a PowerShell command, capturing its stdout into `output`. Returns true on exit code 0.
bool RunPowerShellCommand(const std::wstring& command, std::wstring& output);

// Searches the Windows Defender Operational log for a recent malware/ASR/Controlled-Folder-Access block whose message mentions `needle`, within the last `seconds`; on a hit returns true and sets `detail` to the event message.
bool FindRecentDefenderBlock(const std::wstring& needle, unsigned seconds, std::wstring& detail);
