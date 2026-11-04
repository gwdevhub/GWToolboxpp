#pragma once

#include <string>

bool GetWineDiagnostics(std::wstring& out, bool include_launcher_environment = true);
const std::wstring& GetOperatingSystemDescription();
bool GetEmulationDiagnostics(std::wstring& out);
const std::wstring& GetProcessorDescription();
