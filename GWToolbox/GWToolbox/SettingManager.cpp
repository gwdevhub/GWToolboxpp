#include "SettingManager.h"

#include "MainWindow.h"

std::set<Setting*> SettingManager::settings;

void SettingManager::LoadAll() {
	for (Setting* setting : settings) {
		setting->Load();
	}
}

void SettingManager::SaveAll() {
	for (Setting* setting : settings) {
		setting->Save();
	}
}

void SettingManager::ApplyAll() {
	for (Setting* setting : settings) {
		setting->Apply();
	}
}

