#pragma once

bool Install(bool quiet, std::wstring& error);

bool IsInstalled();

std::filesystem::path GetInstallationDir();
