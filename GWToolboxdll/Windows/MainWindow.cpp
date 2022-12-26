#include "stdafx.h"

#include <Utils/GuiUtils.h>
#include <GWToolbox.h>

#include <Windows/MainWindow.h>

void MainWindow::LoadSettings(ToolboxIni* ini) {
    ToolboxWindow::LoadSettings(ini);
    one_panel_at_time_only = ini->GetBoolValue(Name(), VAR_NAME(one_panel_at_time_only), one_panel_at_time_only);
    show_icons = ini->GetBoolValue(Name(), VAR_NAME(show_icons), show_icons);
    center_align_text = ini->GetBoolValue(Name(), VAR_NAME(center_align_text), center_align_text);
    show_menubutton = false;
    pending_refresh_buttons = true;
}

void MainWindow::SaveSettings(ToolboxIni* ini) {
    ToolboxWindow::SaveSettings(ini);
    ini->SetBoolValue(Name(), VAR_NAME(one_panel_at_time_only), one_panel_at_time_only);
    ini->SetBoolValue(Name(), VAR_NAME(show_icons), show_icons);
    ini->SetBoolValue(Name(), VAR_NAME(center_align_text), center_align_text);
}

void MainWindow::DrawSettingInternal() {
    ImGui::Checkbox("Close other windows when opening a new one", &one_panel_at_time_only);
    ImGui::ShowHelp("Only affects windows (with a title bar), not widgets");

    ImGui::Checkbox("Show Icons", &show_icons);
    ImGui::Checkbox("Center-align text", &center_align_text);
}

void MainWindow::RegisterSettingsContent() {
    ToolboxModule::RegisterSettingsContent(
        SettingsName(),
        Icon(),
        [this](const std::string&, bool is_showing) {
            // ShowVisibleRadio();
            if (!is_showing)
                return;
            ImGui::Text("Main Window Visibility");
            ShowVisibleRadio();
            DrawSizeAndPositionSettings();
            DrawSettingInternal();
        }, SettingsWeighting());
}

void MainWindow::RefreshButtons() {
    pending_refresh_buttons = false;
    const std::vector<ToolboxUIElement*>& ui = GWToolbox::Instance().GetUIElements();
    modules_to_draw.clear();
    for (auto& ui_module : ui) {
        if (!ui_module->show_menubutton)
            continue;
        float weighting = GetModuleWeighting(ui_module);
        auto it = modules_to_draw.begin();
        for (it = modules_to_draw.begin(); it != modules_to_draw.end(); it++) {
            if (it->first > weighting)
                break;
        }
        modules_to_draw.insert(it, { weighting,ui_module });
    }
}

void MainWindow::Draw(IDirect3DDevice9* device) {
    if (!visible) return;
    if (pending_refresh_buttons) RefreshButtons();
    static bool open = true;
    ImGui::SetNextWindowSize(ImVec2(110.0f, 300.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(Name(), show_closebutton ? &open : nullptr, GetWinFlags())) {
        ImGui::PushFont(GuiUtils::GetFont(GuiUtils::FontSize::header2));
        bool drawn = false;
        const size_t msize = modules_to_draw.size();
        for (size_t i = 0; i < msize;i++) {
            ImGui::PushID(static_cast<int>(i));
            if(drawn) ImGui::Separator();
            drawn = true;
            auto &ui_module = modules_to_draw[i].second;
            if (ui_module->DrawTabButton(device, show_icons, true, center_align_text)) {
                if (one_panel_at_time_only && ui_module->visible && ui_module->IsWindow()) {
                    for (const auto& ui_module2 : modules_to_draw) {
                        if (ui_module2.second == ui_module) continue;
                        if (!ui_module2.second->IsWindow()) continue;
                        ui_module2.second->visible = false;
                    }
                }
            }
            ImGui::PopID();
        }
        ImGui::PopFont();
    }
    ImGui::End();

    if (!open) GWToolbox::Instance().StartSelfDestruct();
}
