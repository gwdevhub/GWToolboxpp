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
}

void ToolboxPlugin::SaveSettings(const wchar_t* folder)
{
    settings.SaveFile(GetSettingFile(folder));
}
