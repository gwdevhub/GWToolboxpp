#include "stdafx.h"

#include <ToolboxModule.h>
#include <GWToolbox.h>

namespace {
    uint64_t QpcToMicroseconds(LONGLONG ticks)
    {
        static LARGE_INTEGER freq = [] { LARGE_INTEGER f; QueryPerformanceFrequency(&f); return f; }();
        return static_cast<uint64_t>(ticks * 1000000 / freq.QuadPart);
    }

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

void ToolboxModule::LoadSettings(SettingsDoc& doc, ToolboxIni* legacy)
{
    SettingsRegistry::LoadFieldsFromDoc(this, doc);
    // Fills struct members too (entries point into them) for keys absent from the doc;
    // the module's own override then overlays doc values via doc.GetStruct(Name(), settings).
    SettingsRegistry::LoadFromIniFallback(this, legacy, doc);
}

void ToolboxModule::SaveSettings(SettingsDoc& doc)
{
    SettingsRegistry::SaveFieldsToDoc(this, doc);
}

void ToolboxModule::Terminate()
{
    SettingsRegistry::Unregister(this);
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
            ++modules_it;
        }
        if (callbacks_it->second.empty()) {
            settings_draw_callbacks.erase(callbacks_it);
            callbacks_it = settings_draw_callbacks.begin();
            continue;
        }
        ++callbacks_it;
    }
}

void ToolboxModule::RegisterSettingsContent()
{
    if (!HasSettings()) {
        return;
    }
    RegisterSettingsContent(
        SettingsName(), Icon(),
        [this](const std::string&, const bool is_showing) {
            if (is_showing) {
                DrawSettingsInternal();
            }
        },
        SettingsWeighting());
}

void ToolboxModule::RegisterUIMessageCallback(
    GW::HookEntry* entry,
    GW::UI::UIMessage message_id,
    const GW::UI::UIMessageCallback& callback,
    int altitude)
{
    GW::UI::RegisterUIMessageCallback(entry, message_id,
        [this, callback](GW::HookStatus* status, GW::UI::UIMessage msg, void* wparam, void* lparam) {
            if (GWToolbox::IsProfilingEnabled()) {
                LARGE_INTEGER t0, t1;
                QueryPerformanceCounter(&t0);
                callback(status, msg, wparam, lparam);
                QueryPerformanceCounter(&t1);
                last_ui_message_times_us_[static_cast<uint32_t>(msg)] += QpcToMicroseconds(t1.QuadPart - t0.QuadPart);
            }
            else {
                callback(status, msg, wparam, lparam);
            }
        },
        altitude);
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
    auto& callbacks = settings_draw_callbacks[section];
    const auto found = std::ranges::find_if(callbacks, [this](const auto& pair) {
        return pair.module == this;
        });
    if (found != callbacks.end()) {
        return; // Already added this callback for this section.
    }
    const auto it = std::ranges::find_if(callbacks, [weighting](const auto& pair) {
        return pair.weighting > weighting;
    });
    callbacks.insert(it, {weighting, callback, this});
}
