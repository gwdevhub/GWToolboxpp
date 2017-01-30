#pragma once

#include <set>

#include "Settings.h"

class SettingManager {
	friend class Setting;
public:

	static void LoadAll();
	static void SaveAll();
	static void ApplyAll();

private:
	SettingManager();
	static void Register(Setting* setting) {
		settings.insert(setting);
	}

	static std::set<Setting*> settings;
};
