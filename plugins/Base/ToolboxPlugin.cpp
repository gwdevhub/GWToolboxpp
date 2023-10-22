#pragma once

#include "ToolboxPlugin.h"

import PluginUtils;

std::filesystem::path ToolboxPlugin::GetSettingFile(const wchar_t* folder) const
{
    const auto wname = PluginUtils::StringToWString(Name());
    const auto ininame = wname + L".ini";
    return std::filesystem::path(folder) / ininame;
}

void ToolboxPlugin::Initialize(ImGuiContext* ctx, const ImGuiAllocFns allocator_fns, const HMODULE toolbox_dll)
{
    ImGui::SetCurrentContext(ctx);
    ImGui::SetAllocatorFunctions(allocator_fns.alloc_func, allocator_fns.free_func, allocator_fns.user_data);
    toolbox_handle = toolbox_dll;
}
