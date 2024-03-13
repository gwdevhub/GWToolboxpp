#include "ToolboxUIPlugin.h"
#include <imgui.h>

#include <GWCA/Utilities/Hooker.h>
#include <GWCA/Managers/ChatMgr.h>

#include "GWCA/GWCA.h"

import PluginUtils;

namespace {
    // ReSharper disable once CppParameterMayBeConst
    // ReSharper disable once CppParameterMayBeConstPtrOrRef
    bool CmdTB(const wchar_t*, const int argc, const LPWSTR* argv)
    {
        const auto instance = static_cast<ToolboxUIPlugin*>(ToolboxPluginInstance());
        if (!instance) {
            return false;
        }
        if (argc < 3) {
            return false;
        }
        const std::wstring arg1 = PluginUtils::ToLower(argv[1]);
        auto pluginname = PluginUtils::ToLower(PluginUtils::StringToWString(instance->Name()));
        pluginname.erase(std::ranges::remove_if(pluginname, [](const wchar_t x) { return std::isspace(x); }).begin(), pluginname.end());
        if (arg1.empty()) {
            return false;
        }
        if (!(arg1 == L"all" || arg1 == L"plugins" || pluginname.find(arg1) == 0)) {
            return false;
        }
        const std::wstring arg2 = PluginUtils::ToLower(argv[2]);
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
        return arg1 == pluginname;
    }
}

bool* ToolboxUIPlugin::GetVisiblePtr()
{
    return &plugin_visible;
}

bool ToolboxUIPlugin::ShowInMainMenu() const
{
    return can_show_in_main_window && show_menubutton;
}

void ToolboxUIPlugin::Initialize(ImGuiContext* ctx, const ImGuiAllocFns allocator_fns, const HMODULE toolbox_dll)
{
    ToolboxPlugin::Initialize(ctx, allocator_fns, toolbox_dll);
    GW::Initialize();
    GW::Chat::CreateCommand(L"tb", CmdTB);
}

bool ToolboxUIPlugin::CanTerminate()
{
    return GW::HookBase::GetInHookCount() == 0;
}

void ToolboxUIPlugin::SignalTerminate()
{
    ToolboxPlugin::SignalTerminate();
    GW::Chat::DeleteCommand(L"tb");
    GW::DisableHooks();
}

void ToolboxUIPlugin::Terminate()
{
    ToolboxPlugin::Terminate();
    GW::Terminate();
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
    int count = 0;
    if (is_movable) {
        if (++count % 2 == 0) {
            ImGui::SameLine();
        }
        ImGui::Checkbox("Lock Position", &lock_move);
    }
    if (is_resizable) {
        if (++count % 2 == 0) {
            ImGui::SameLine();
        }
        ImGui::Checkbox("Lock Size", &lock_size);
    }
    if (can_close) {
        if (++count % 2 == 0) {
            ImGui::SameLine();
        }
        ImGui::Checkbox("Show close button", &show_closebutton);
    }
    if (can_show_in_main_window) {
        if (++count % 2 == 0) {
            ImGui::SameLine();
        }
        ImGui::Checkbox("Show in main window", &show_menubutton);
    }
    if (can_collapse) {
        if (++count % 2 == 0) {
            ImGui::SameLine();
        }
        ImGui::Checkbox("Show title", &show_title);
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
    show_closebutton = ini.GetBoolValue(Name(), VAR_NAME(show_closebutton), show_closebutton);
    show_title = ini.GetBoolValue(Name(), VAR_NAME(show_title), show_title);
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
    ini.SetBoolValue(Name(), VAR_NAME(show_closebutton), show_closebutton);
    ini.SetBoolValue(Name(), VAR_NAME(show_title), show_title);
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
    if (!can_collapse) {
        flags |= ImGuiWindowFlags_NoCollapse;
    }
    if (!show_title || !can_collapse) {
        flags |= ImGuiWindowFlags_NoTitleBar;
    }

    return flags;
}

bool ToolboxUIPlugin::DrawTabButton(const bool show_icon, const bool show_text, const bool center_align_text)
{
    ImGui::PushStyleColor(ImGuiCol_Button, plugin_visible ? ImGui::GetStyle().Colors[ImGuiCol_Button] : ImVec4(0, 0, 0, 0));
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImVec2 textsize = ImGui::CalcTextSize(Name());
    const float width = ImGui::GetContentRegionAvail().x;

    float img_size = 0;
    if (show_icon) {
        img_size = ImGui::GetTextLineHeightWithSpacing();
    }
    float text_x;
    if (center_align_text) {
        text_x = pos.x + img_size + (width - img_size - textsize.x) / 2;
    }
    else {
        text_x = pos.x + img_size + ImGui::GetStyle().ItemSpacing.x;
    }
    const bool clicked = ImGui::Button("", ImVec2(width, ImGui::GetTextLineHeightWithSpacing()));
    if (show_icon && Icon()) {
        ImGui::GetWindowDrawList()->AddText(ImVec2(pos.x, pos.y + ImGui::GetStyle().ItemSpacing.y / 2),
                                            ImColor(ImGui::GetStyle().Colors[ImGuiCol_Text]), Icon());
    }
    if (show_text) {
        ImGui::GetWindowDrawList()->AddText(ImVec2(text_x, pos.y + ImGui::GetStyle().ItemSpacing.y / 2),
                                            ImColor(ImGui::GetStyle().Colors[ImGuiCol_Text]), Name());
    }

    if (clicked) {
        plugin_visible = !plugin_visible;
    }
    ImGui::PopStyleColor();
    return clicked;
}
