#pragma once

#include <string>

bool GetWineDiagnostics(std::wstring& out, bool include_launcher_environment = true);
std::wstring GetOperatingSystemDescription();
bool GetEmulationDiagnostics(std::wstring& out);
std::wstring GetProcessorDescription();
