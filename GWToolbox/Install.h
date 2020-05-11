#pragma once

void ParseRegistrySettings();

bool Install(bool quiet);
bool Uninstall(bool quiet);

bool IsInstalled();
bool GetInstallationLocation(wchar_t *path, size_t length);

void ParseRegistrySettings();
