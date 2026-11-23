#pragma once

#include <string>

bool LauncherSettingEnabled(const std::string& name);
void SetLauncherSetting(const std::string& name, bool value);
