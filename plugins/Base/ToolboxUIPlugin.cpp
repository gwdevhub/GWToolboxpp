#include "ToolboxUIPlugin.h"
#include <imgui.h>

#include <GWCA/Utilities/Hooker.h>
#include <GWCA/Managers/ChatMgr.h>

import PluginUtils;

namespace {
    // ReSharper disable once CppParameterMayBeConst
    // ReSharper disable once CppParameterMayBeConstPtrOrRef
    void CmdTB(const wchar_t*, int argc, LPWSTR* argv)
    {
        const std::wstring arg1 = PluginUtils::ToLower(argv[1]);
        if (argc < 3) {
            if (arg1 == L"hide") {
                // e.g. /tb hide
                if (ToolboxPluginInstance()->GetVisiblePtr()) {
                    *ToolboxPluginInstance()->GetVisiblePtr() = false;
                }
            }
            else if (arg1 == L"show") {
                // e.g. /tb show
                if (ToolboxPluginInstance()->GetVisiblePtr()) {
                    *ToolboxPluginInstance()->GetVisiblePtr() = true;
                }
            }
            return;
        }
        const auto should_react = [](auto arg) {
            if (!arg.empty()) {
                const std::string name = PluginUtils::WStringToString(arg);
                if (arg == L"all" || PluginUtils::ToLower(ToolboxPluginInstance()->Name()).find(name) == 0) {
                    return true;
                }
            }
            return false;
        };
        const std::wstring arg = PluginUtils::ToLower(argv[1]);

        const std::wstring arg2 = PluginUtils::ToLower(argv[2]);
        if (arg2 == L"hide") {
            // e.g. /tb travel hide
            if (should_react(arg)) {
                if (ToolboxPluginInstance()->GetVisiblePtr()) {
                    *ToolboxPluginInstance()->GetVisiblePtr() = false;
                }
            }
        }
        else if (arg2 == L"show") {
            // e.g. /tb travel show
            if (ToolboxPluginInstance()->GetVisiblePtr()) {
                *ToolboxPluginInstance()->GetVisiblePtr() = true;
            }
        }
        else if (arg2 == L"toggle") {
            // e.g. /tb travel show
            if (ToolboxPluginInstance()->GetVisiblePtr()) {
                *ToolboxPluginInstance()->GetVisiblePtr() = !*ToolboxPluginInstance()->GetVisiblePtr();
            }
        }
    }
}

void ToolboxUIPlugin::DrawSizeAndPositionSettings()
{
    ImVec2 pos(0, 0);
    ImVec2 size(100.0f, 100.0f);
    if (const auto window = ImGui::FindWindowByName(Name())) {
        pos = window->Pos;
        size = window->Size;
    }
    if (is_movable) {
        if (ImGui::DragFloat2("Position", reinterpret_cast<float*>(&pos), 1.0f, 0.0f, 0.0f, "%.0f")) {
            ImGui::SetWindowPos(Name(), pos);
        }
    }
    if (is_resizable) {
        if (ImGui::DragFloat2("Size", reinterpret_cast<float*>(&size), 1.0f, 0.0f, 0.0f, "%.0f")) {
            ImGui::SetWindowSize(Name(), size);
        }
    }
    if (is_movable) {
        ImGui::SameLine();
        ImGui::Checkbox("Lock Position", &lock_move);
    }
    if (is_resizable) {
        ImGui::SameLine();
        ImGui::Checkbox("Lock Size", &lock_size);
    }
    if (has_closebutton) {
        ImGui::SameLine();
        ImGui::Checkbox("Show close button", &show_closebutton);
    }
    if (can_show_in_main_window) {
        ImGui::SameLine();
        ImGui::Checkbox("Show in main window", &show_menubutton);
    }
}

void ToolboxUIPlugin::Initialize(ImGuiContext* ctx, const ImGuiAllocFns allocator_fns, const HMODULE toolbox_dll)
{
    ToolboxPlugin::Initialize(ctx, allocator_fns, toolbox_dll);
    GW::Chat::CreateCommand(L"tb", CmdTB);
}

bool ToolboxUIPlugin::CanTerminate()
{
    return GW::HookBase::GetInHookCount() == 0;
}

void ToolboxUIPlugin::Terminate()
{
    GW::Chat::DeleteCommand(L"tb");
    ToolboxPlugin::Terminate();
}

void ToolboxUIPlugin::LoadSettings(const wchar_t* folder)
{
    if (!HasSettings()) {
        return;
    }
    ini.LoadFile(GetSettingFile(folder).c_str());
    plugin_visible = ini.GetBoolValue(Name(), VAR_NAME(plugin_visible), plugin_visible);
    lock_move = ini.GetBoolValue(Name(), VAR_NAME(lock_move), lock_move);
    lock_size = ini.GetBoolValue(Name(), VAR_NAME(lock_size), lock_size);
    show_menubutton = ini.GetBoolValue(Name(), VAR_NAME(show_menubutton), show_menubutton);
}

void ToolboxUIPlugin::SaveSettings(const wchar_t* folder)
{
    if (!HasSettings()) {
        return;
    }
    ini.SetBoolValue(Name(), VAR_NAME(plugin_visible), plugin_visible);
    ini.SetBoolValue(Name(), VAR_NAME(lock_move), lock_move);
    ini.SetBoolValue(Name(), VAR_NAME(lock_size), lock_size);
    ini.SetBoolValue(Name(), VAR_NAME(show_menubutton), show_menubutton);
    ini.SaveFile(GetSettingFile(folder).c_str());
}
