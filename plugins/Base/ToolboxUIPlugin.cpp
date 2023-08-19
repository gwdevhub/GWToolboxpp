#include "ToolboxUIPlugin.h"
#include <imgui.h>

#include <GWCA/Utilities/Hooker.h>
#include <GWCA/Managers/ChatMgr.h>

#include "GWCA/GWCA.h"

import PluginUtils;

namespace {
    // ReSharper disable once CppParameterMayBeConst
    // ReSharper disable once CppParameterMayBeConstPtrOrRef
    bool CmdTB(const wchar_t*, int argc, LPWSTR* argv)
    {
        const auto instance = static_cast<ToolboxUIPlugin*>(ToolboxPluginInstance());
        if (!instance) {
            return false;
        }
        if (argc < 3) {
            return false;
        }
        const std::wstring arg1 = PluginUtils::ToLower(argv[1]);
        const auto should_react = [instance](auto arg) {
            if (!arg.empty()) {
                const std::string name = PluginUtils::WStringToString(arg);
                if (arg == L"all" || arg == L"plugins" || PluginUtils::ToLower(instance->Name()).find(name) == 0) {
                    return true;
                }
            }
            return false;
        };
        const std::wstring arg2 = PluginUtils::ToLower(argv[2]);
        if (!should_react(arg1)) {
            return false;
        }
        if (arg2 == L"hide") {
            // /tb PluginName hide
            *instance->GetVisiblePtr() = false;
        }
        else if (arg2 == L"show") {
            // /tb PluginName hide
            *instance->GetVisiblePtr() = true;
        }
        else if (arg2 == L"toggle") {
            // /tb PluginName hide
            *instance->GetVisiblePtr() = !*instance->GetVisiblePtr();
        }
        return arg2 == PluginUtils::StringToWString(instance->Name()) || arg2 == L"plugins";
    }
}

bool* ToolboxUIPlugin::GetVisiblePtr()
{
    if (!has_closebutton || show_closebutton) {
        return &plugin_visible;
    }
    return nullptr;
}


void ToolboxUIPlugin::Initialize(ImGuiContext* ctx, const ImGuiAllocFns allocator_fns, const HMODULE toolbox_dll)
{
    ToolboxPlugin::Initialize(ctx, allocator_fns, toolbox_dll);
    GW::Chat::CreateCommand(L"tb", GW::Chat::CmdCB(CmdTB));
}

bool ToolboxUIPlugin::CanTerminate()
{
    return GW::HookBase::GetInHookCount() == 0;
}

void ToolboxUIPlugin::SignalTerminate()
{
    ToolboxPlugin::SignalTerminate();
    GW::DisableHooks();
}

void ToolboxUIPlugin::Terminate()
{
    GW::Chat::DeleteCommand(L"tb");
    ToolboxPlugin::Terminate();
}

void ToolboxUIPlugin::DrawSettings()
{
    ToolboxPlugin::DrawSettings();

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
        ImGui::Checkbox("Lock Position", &lock_move);
    }
    if (is_resizable) {
        ImGui::Checkbox("Lock Size", &lock_size);
    }
    if (has_closebutton) {
        ImGui::Checkbox("Show close button", &show_closebutton);
    }
    if (can_show_in_main_window) {
        ImGui::Checkbox("Show in main window", &show_menubutton);
    }
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
    PLUGIN_ASSERT(ini.SaveFile(GetSettingFile(folder).c_str()) == SI_OK);
}

int ToolboxUIPlugin::GetWinFlags(ImGuiWindowFlags flags) const
{
    if (lock_size || !is_resizable) {
        flags |= ImGuiWindowFlags_NoResize;
    }
    if (lock_move || !is_movable) {
        flags |= ImGuiWindowFlags_NoMove;
    }
    if (!show_closebutton) {
        flags |= ImGuiWindowFlags_NoCollapse;
    }

    return flags;
}
