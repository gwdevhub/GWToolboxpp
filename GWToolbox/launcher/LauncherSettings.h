#pragma once

#include <filesystem>
#include <string>

std::filesystem::path launcherSettingsFile();
bool LauncherSettingEnabled(const std::string& name);
void SetLauncherSetting(const std::string& name, bool value);
