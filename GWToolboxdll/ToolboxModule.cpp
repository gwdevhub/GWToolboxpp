#include "stdafx.h"

#include <GWToolbox.h>
#include <ToolboxModule.h>

namespace {
    // static function to register content
    std::unordered_map<std::string, SectionDrawCallbackList> settings_draw_callbacks{};
    std::unordered_map<std::string, const char*> settings_icons{};
    std::unordered_map<std::string, ToolboxModule*> modules_loaded{};
} // namespace

const std::unordered_map<std::string, SectionDrawCallbackList>& ToolboxModule::GetSettingsCallbacks() { return settings_draw_callbacks; }
const std::unordered_map<std::string, const char*>& ToolboxModule::GetSettingsIcons() { return settings_icons; }
const std::unordered_map<std::string, ToolboxModule*>& ToolboxModule::GetModulesLoaded() { return modules_loaded; }

void ToolboxModule::Initialize()
{
    if (!GWToolbox::Instance().RegisterModule(this))
        return; // Already registered
    RegisterSettingsContent();
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
        settings_draw_callbacks.emplace(section, SectionDrawCallbackList());
    }
    if (icon) {
        if (!settings_icons.contains(section)) {
            settings_icons[section] = icon;
        }
        ASSERT(settings_icons.at(section) == icon && "Trying to set different icon for the same setting!");
    }
    const auto it = std::ranges::find_if(settings_draw_callbacks[section], [weighting](const auto& pair) {
        return pair.first > weighting;
    });
    settings_draw_callbacks[section].insert(it, {weighting, callback});
}
