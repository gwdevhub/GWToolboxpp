#include "stdafx.h"
#include "ToolboxModule.h"

#include <Windows/SettingsWindow.h>

#include "GWToolbox.h"

namespace {
	// static function to register content
	static std::unordered_map<std::string, SectionDrawCallbackList> settings_draw_callbacks{};
	static std::unordered_map<std::string, ToolboxModule*> modules_loaded{};
}

const std::unordered_map<std::string, SectionDrawCallbackList>* ToolboxModule::GetSettingsCallbacks() {
	return &settings_draw_callbacks;
}

const std::unordered_map<std::string, ToolboxModule*>* ToolboxModule::GetModulesLoaded() {
	return &modules_loaded;
}

void ToolboxModule::Initialize() {
	if (!GWToolbox::Instance().RegisterModule(this))
		return; // Already registered
	RegisterSettingsContent();
}
void ToolboxModule::RegisterSettingsContent() {
	if (!HasSettings()) return;
	RegisterSettingsContent(SettingsName(),
        [this](const std::string* section, bool is_showing) {
            UNREFERENCED_PARAMETER(section);
		    if (is_showing)
			    DrawSettingInternal();
		},
        SettingsWeighting());
}
void ToolboxModule::RegisterSettingsContent(const char* section, SectionDrawCallback callback, float weighting) {
	if (settings_draw_callbacks.find(section) == settings_draw_callbacks.end()) {
		settings_draw_callbacks.emplace(section, SectionDrawCallbackList());
	}
	auto it = settings_draw_callbacks[section].begin();
	for (it = settings_draw_callbacks[section].begin(); it != settings_draw_callbacks[section].end(); it++) {
		if (it->first > weighting)
			break;
	}
	settings_draw_callbacks[section].insert(it, { weighting,callback });
}

