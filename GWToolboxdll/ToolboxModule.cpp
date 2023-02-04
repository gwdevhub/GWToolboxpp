#include "stdafx.h"

#include <GWToolbox.h>
#include <ToolboxModule.h>

namespace {
    // static function to register content
    std::unordered_map<std::string, SectionDrawCallbackList> settings_draw_callbacks{};
    std::unordered_map<std::string, const char*> settings_icons{};
    std::unordered_map<std::string, ToolboxModule*> modules_loaded{};

    std::unordered_map<ToolboxModule*, std::vector<SectionDrawCallback>> module_setting_draw_callbacks;
} // namespace

const std::unordered_map<std::string, SectionDrawCallbackList>& ToolboxModule::GetSettingsCallbacks() { return settings_draw_callbacks; }
const std::unordered_map<std::string, const char*>& ToolboxModule::GetSettingsIcons() { return settings_icons; }
const std::unordered_map<std::string, ToolboxModule*>& ToolboxModule::GetModulesLoaded() { return modules_loaded; }

void ToolboxModule::Initialize()
{
    RegisterSettingsContent();
}
void ToolboxModule::Terminate() {
    // Remove any settings draw callbacks associated with this module
    auto callbacks_it = settings_draw_callbacks.begin();
    while (callbacks_it != settings_draw_callbacks.end()) {
        auto modules_it = callbacks_it->second.begin();
        while (modules_it != callbacks_it->second.end()) {
            if (modules_it->module == this) {
                callbacks_it->second.erase(modules_it);
                modules_it = callbacks_it->second.begin();
                continue;
            }
            modules_it++;
        }
        if (callbacks_it->second.empty()) {
            settings_draw_callbacks.erase(callbacks_it);
            callbacks_it = settings_draw_callbacks.begin();
            continue;
        }
        callbacks_it++;
    }
}
void ToolboxModule::RegisterSettingsContent()
{
    if (!HasSettings())
        return;
    RegisterSettingsContent(
        SettingsName(), Icon(),
        [this](const std::string&, const bool is_showing) {
            if (is_showing)
                DrawSettingInternal();
        },
        SettingsWeighting());
}
void ToolboxModule::RegisterSettingsContent(const char* section, const char* icon, const SectionDrawCallback& callback, float weighting)
{
    if (!settings_draw_callbacks.contains(section)) {
        settings_draw_callbacks[section] = {};
    }
    if (icon) {
        if (!settings_icons.contains(section)) {
            settings_icons[section] = icon;
        }
        ASSERT(settings_icons.at(section) == icon && "Trying to set different icon for the same setting!");
    }
    const auto it = std::ranges::find_if(settings_draw_callbacks[section], [weighting](const auto& pair) {
        return pair.weighting > weighting;
    });
    settings_draw_callbacks[section].insert(it, {weighting, callback, this});
}
