#pragma once

#include <filesystem>

// The installed GWToolboxdll.dll: the path in the launcher's GWToolbox.json when that file points at a DLL that exists,
// else the platform's default install location (which is then recorded). Empty when there's none.
std::filesystem::path FindInstalledDll();

// Writes the DLL's path to GWToolbox.json, next to the launcher, so later checks find it first.
void RecordInstalledDll(const std::filesystem::path& dll);
