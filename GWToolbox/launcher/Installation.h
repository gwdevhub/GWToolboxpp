#pragma once

#include <filesystem>

std::filesystem::path FindInstalledDll();

void RecordInstalledDll(const std::filesystem::path& dll);

void SetLocalDllOnly(bool enabled);
