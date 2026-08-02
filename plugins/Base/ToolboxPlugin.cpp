#include "ToolboxPlugin.h"
#include "PluginUtils.h"

std::filesystem::path ToolboxPlugin::GetSettingFile(const wchar_t* folder) const
{
    const auto wname = PluginUtils::StringToWString(Name());
    return std::filesystem::path(folder) / (wname + L".json");
}

std::filesystem::path ToolboxPlugin::GetLegacySettingFile(const wchar_t* folder) const
{
    const auto wname = PluginUtils::StringToWString(Name());
    return std::filesystem::path(folder) / (wname + L".ini");
}

ToolboxIni ToolboxPlugin::LoadIni(const wchar_t* folder) {
    ToolboxIni ini;
    const auto ini_path = GetSettingFile(folder);
    PLUGIN_ASSERT(ini.LoadIfExists(ini_path) == SI_OK);
    ini.location_on_disk = ini_path;
    return ini;
}

void ToolboxPlugin::Initialize(ImGuiContext* ctx, const ImGuiAllocFns allocator_fns, const HMODULE toolbox_dll)
{
    ImGui::SetCurrentContext(ctx);
    ImGui::SetAllocatorFunctions(allocator_fns.alloc_func, allocator_fns.free_func, allocator_fns.user_data);
    toolbox_handle = toolbox_dll;
}

void ToolboxPlugin::LoadSettings(const wchar_t* folder)
{
    settings.LoadFile(GetSettingFile(folder));
    legacy_ini.Reset();
    legacy_ini.LoadIfExists(GetLegacySettingFile(folder));

    // Some plugins used to (or still) save raw INI text under the .json path instead of
    // real JSON. Recover any such values here so LoadSetting() still finds them: parsing
    // valid JSON as INI yields no sections and is a harmless no-op. Values already present
    // in legacy_ini (from the real .ini file) take priority and are left untouched.
    ToolboxIni half_migrated;
    if (half_migrated.LoadIfExists(GetSettingFile(folder)) == SI_OK && !half_migrated.IsEmpty()) {
        TNamesDepend sections;
        half_migrated.GetAllSections(sections);
        for (const auto& section : sections) {
            TNamesDepend keys;
            half_migrated.GetAllKeys(section.pItem, keys);
            for (const auto& key : keys) {
                if (legacy_ini.KeyExists(section.pItem, key.pItem)) {
                    continue;
                }
                legacy_ini.SetValue(section.pItem, key.pItem, half_migrated.GetValue(section.pItem, key.pItem));
            }
        }
    }
}

void ToolboxPlugin::SaveSettings(const wchar_t* folder)
{
    settings.SaveFile(GetSettingFile(folder));
}
