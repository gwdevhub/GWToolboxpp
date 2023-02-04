#include "stdafx.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/Managers/MapMgr.h>

#include <Defines.h>
#include <Utils/GuiUtils.h>
#include <GWToolbox.h>

#include <Modules/ChatCommands.h>
#include <Modules/Resources.h>
#include <Modules/ToolboxSettings.h>
#include <Modules/ToolboxTheme.h>
#include <Modules/Updater.h>
#include <Windows/SettingsWindow.h>

#include <ToolboxWidget.h>

void SettingsWindow::LoadSettings(ToolboxIni* ini) {
    ToolboxWindow::LoadSettings(ini);
    hide_when_entering_explorable = ini->GetBoolValue(Name(), VAR_NAME(hide_when_entering_explorable), hide_when_entering_explorable);
}
void SettingsWindow::SaveSettings(ToolboxIni* ini) {
    ToolboxWindow::SaveSettings(ini);
    ini->SetBoolValue(Name(), VAR_NAME(hide_when_entering_explorable), hide_when_entering_explorable);
}

void SettingsWindow::Draw(IDirect3DDevice9* pDevice) {
    UNREFERENCED_PARAMETER(pDevice);
    static GW::Constants::InstanceType last_instance_type = GW::Constants::InstanceType::Loading;
    const GW::Constants::InstanceType instance_type = GW::Map::GetInstanceType();

    if (instance_type != last_instance_type) {
        if (hide_when_entering_explorable && instance_type == GW::Constants::InstanceType::Explorable)
            visible = false;
        last_instance_type = instance_type;
    }

    if (!visible) return;
    ImGui::SetNextWindowCenter(ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(768, 768), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {
        drawn_settings.clear();
        const ImColor sCol(102, 187, 238, 255);
        ImGui::PushTextWrapPos();
        ImGui::Text("GWToolbox++");
        ImGui::SameLine(0, 0);
        ImGui::TextColored(sCol, " v%s ", GWTOOLBOXDLL_VERSION);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Go to %s", GWTOOLBOX_WEBSITE);
        if(ImGui::IsItemClicked())
            ShellExecute(NULL, "open", GWTOOLBOX_WEBSITE, NULL, NULL, SW_SHOWNORMAL);
        if constexpr (!std::string(GWTOOLBOXDLL_VERSION_BETA).empty()) {
            ImGui::SameLine();
            ImGui::Text("- %s", GWTOOLBOXDLL_VERSION_BETA);
        } else {
            const std::string server_version = Updater::GetServerVersion();
            if (!server_version.empty()) {
                if (server_version == GWTOOLBOXDLL_VERSION) {
                    ImGui::SameLine();
                    ImGui::Text("(Up to date)");
                } else {
                    ImGui::Text("Version %s is available!", server_version.c_str());
                }
            }
        }
#ifdef _DEBUG
            ImGui::SameLine();
            ImGui::Text("(Debug)");
#endif
        const float w = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) / 2;
        if (ImGui::Button("Open Settings Folder", ImVec2(w, 0))) {
            if (SUCCEEDED(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE)))
                ShellExecuteW(NULL, L"open", Resources::GetSettingsFolderPath().c_str(), NULL, NULL, SW_SHOWNORMAL);
        }
        ImGui::SameLine();
        if (ImGui::Button("Open GWToolbox++ Website", ImVec2(w, 0))) {
            if (SUCCEEDED(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE)))
                ShellExecuteA(NULL, "open", GWTOOLBOX_WEBSITE, NULL, NULL, SW_SHOWNORMAL);
        }

        ToolboxSettings::Instance().DrawFreezeSetting();
        ImGui::SameLine();
        ImGui::Checkbox("Hide Settings when entering explorable area", &hide_when_entering_explorable);

        ImGui::Text("General:");

        if (ImGui::CollapsingHeader("Help")) {
            if (ImGui::TreeNodeEx("General Interface", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
                ImGui::Bullet(); ImGui::Text("Double-click on the title bar to collapse a window.");
                ImGui::Bullet(); ImGui::Text("Click and drag on the lower right corner to resize a window.");
                ImGui::Bullet(); ImGui::Text("Click and drag on any empty space to move a window.");
                ImGui::Bullet(); ImGui::Text("Mouse Wheel to scroll.");
                if (ImGui::GetIO().FontAllowUserScaling) {
                    ImGui::Bullet(); ImGui::Text("CTRL+Mouse Wheel to zoom window contents.");
                }
                ImGui::Bullet(); ImGui::Text("TAB or SHIFT+TAB to cycle through keyboard editable fields.");
                ImGui::Bullet(); ImGui::Text("CTRL+Click or Double Click on a slider or drag box to input text.");
                ImGui::Bullet(); ImGui::Text(
                    "While editing text:\n"
                    "- Hold SHIFT or use mouse to select text\n"
                    "- CTRL+Left/Right to word jump\n"
                    "- CTRL+A or double-click to select all\n"
                    "- CTRL+X,CTRL+C,CTRL+V clipboard\n"
                    "- CTRL+Z,CTRL+Y undo/redo\n"
                    "- ESCAPE to revert\n"
                    "- You can apply arithmetic operators +,*,/ on numerical values. Use +- to subtract.\n");
                ImGui::TreePop();
            }
            if (ImGui::TreeNodeEx("Opening and closing windows", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
                ImGui::Text("There are several ways to open and close toolbox windows and widgets:");
                ImGui::Bullet(); ImGui::Text("Buttons in the main window.");
                ImGui::Bullet(); ImGui::Text("Checkboxes in the Info window.");
                ImGui::Bullet(); ImGui::Text("Checkboxes on the right of each setting header below.");
                ImGui::Bullet(); ImGui::Text("Chat command '/hide <name>' to hide a window or widget.");
                ImGui::Bullet(); ImGui::Text("Chat command '/show <name>' to show a window or widget.");
                ImGui::Bullet(); ImGui::Text("Chat command '/tb <name>' to toggle a window or widget.");
                ImGui::Indent();
                ImGui::Text("In the commands above, <name> is the title of the window as shown in the title bar. For example, try '/hide settings' and '/show settings'.");
                ImGui::Text("Note: the names of the widgets without a visible title bar are the same as in the setting headers below.");
                ImGui::Unindent();
                ImGui::Bullet(); ImGui::Text("Send Chat hotkey to enter one of the commands above.");
                ImGui::TreePop();
            }
            for (const auto module : GWToolbox::Instance().GetAllModules()) {
                module->DrawHelp();
            }
        }

        DrawSettingsSection(ToolboxTheme::Instance().SettingsName());
        DrawSettingsSection(ToolboxSettings::Instance().SettingsName());

        const auto sort = [](const ToolboxModule* a, const ToolboxModule* b) {
            return strcmp(a->Name(), b->Name()) < 0;
        };

        auto modules = GWToolbox::Instance().GetModules();
        std::ranges::sort(modules, sort);
        for (const auto m : modules) {
            if (m->HasSettings())
                DrawSettingsSection(m->SettingsName());
        }
        auto windows = GWToolbox::Instance().GetWindows();
        std::ranges::sort(windows, sort);
        if(!windows.empty())
            ImGui::Text("Windows:");
        for (const auto m : windows) {
            if (m->HasSettings())
                DrawSettingsSection(m->SettingsName());
        }
        auto widgets = GWToolbox::Instance().GetWidgets();
        std::ranges::sort(widgets, sort);
        if(!widgets.empty())
            ImGui::Text("Widgets:");
        for (const auto m : widgets) {
            if (m->HasSettings())
                DrawSettingsSection(m->SettingsName());
        }

        if (ImGui::Button("Save Now", ImVec2(w, 0))) {
            GWToolbox::Instance().SaveSettings();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Toolbox normally saves settings on exit.\nClick to save to disk now.");
        ImGui::SameLine();
        if (ImGui::Button("Load Now", ImVec2(w, 0))) {
            GWToolbox::Instance().LoadSettings();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Toolbox normally loads settings on launch.\nClick to re-load from disk now.");
        ImGui::PopTextWrapPos();
    }
    ImGui::End();
}

bool SettingsWindow::DrawSettingsSection(const char* section)
{
    if (strcmp(section, "") == 0) return true;
    const auto& callbacks = ToolboxModule::GetSettingsCallbacks();
    const auto& icons = ToolboxModule::GetSettingsIcons();

    const auto& settings_section = callbacks.find(section);
    if (settings_section == callbacks.end()) return false;
    if (drawn_settings.contains(section)) return true; // Already drawn
    drawn_settings[section] = true;

    static char buf[128];
    sprintf(buf, "      %s", section);
    const auto pos = ImGui::GetCursorScreenPos();
    const bool is_showing = ImGui::CollapsingHeader(buf, ImGuiTreeNodeFlags_AllowItemOverlap);

    const char* icon = nullptr;
    if (const auto it = icons.find(section); it != icons.end())
        icon = it->second;
    if (icon) {
        const auto& style = ImGui::GetStyle();
        const float text_offset_x = ImGui::GetTextLineHeightWithSpacing() + 4.0f; // TODO: find a proper number
        ImGui::GetWindowDrawList()->AddText(ImVec2(pos.x + text_offset_x, pos.y + style.ItemSpacing.y / 2), ImColor(style.Colors[ImGuiCol_Text]), icon);
    }

    ImGui::PushID(section);
    size_t i = 0;
    for (const auto& setting_callback : settings_section->second) {
        //if (i && is_showing) ImGui::Separator();
        ImGui::PushID(i);
        setting_callback.callback(settings_section->first, is_showing);
        i++;
        ImGui::PopID();
    }
    ImGui::PopID();
    return true;
}
