#pragma once

#include <filesystem>

bool Install(bool quiet);
bool Uninstall(bool quiet);

bool IsInstalled();
bool GetInstallationLocation(std::filesystem::path& out);
