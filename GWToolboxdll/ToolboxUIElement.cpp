#include "stdafx.h"

#include <GWToolbox.h>
#include <ImGuiAddons.h>
#include <ToolboxUIElement.h>
#include <Windows/MainWindow.h>

#include <Modules/ToolboxSettings.h>


const char* ToolboxUIElement::UIName() const
{
    if (Icon()) {
        static char buf[128];
        sprintf(buf, "%s  %s", Icon(), Name());
        return buf;
    }
    return Name();
}

void ToolboxUIElement::Initialize()
{
    ToolboxModule::Initialize();
}

void ToolboxUIElement::Terminate()
{
    ToolboxModule::Terminate();
}

void ToolboxUIElement::LoadSettings(ToolboxIni* ini)
{
    ToolboxModule::LoadSettings(ini);
    LOAD_BOOL(visible);
    LOAD_BOOL(show_menubutton);
    LOAD_BOOL(lock_move);
    LOAD_BOOL(lock_size);
    LOAD_BOOL(auto_size);
    LOAD_BOOL(show_titlebar);
    LOAD_BOOL(show_closebutton);
}

void ToolboxUIElement::SaveSettings(ToolboxIni* ini)
{
    ToolboxModule::SaveSettings(ini);
    SAVE_BOOL(visible);
    SAVE_BOOL(show_menubutton);
    SAVE_BOOL(lock_move);
    SAVE_BOOL(lock_size);
    SAVE_BOOL(auto_size);
    SAVE_BOOL(show_titlebar);
    SAVE_BOOL(show_closebutton);
}

ImGuiWindowFlags ToolboxUIElement::GetWinFlags(ImGuiWindowFlags flags) const
{
    if (!ToolboxSettings::move_all) {
        if (lock_move) {
            flags |= ImGuiWindowFlags_NoMove;
        }
        if (lock_size) {
            flags |= ImGuiWindowFlags_NoResize;
        }
        if (auto_size) {
            flags |= ImGuiWindowFlags_AlwaysAutoResize;
        }
        if (!show_titlebar) {
            flags |= ImGuiWindowFlags_NoTitleBar;
        }
    }
    return flags;
}

void ToolboxUIElement::RegisterSettingsContent()
{
    ToolboxModule::RegisterSettingsContent(
        SettingsName(),
        Icon(),
        [this](const std::string&, const bool is_showing) {
            ShowVisibleRadio();
            if (!is_showing) {
                return;
            }
            DrawSizeAndPositionSettings();
            DrawSettingsInternal();
        },
        SettingsWeighting());
}

void ToolboxUIElement::DrawSizeAndPositionSettings()
{
    ImVec2 pos(0, 0);
    ImVec2 size(100.0f, 100.0f);
    if (const auto window = ImGui::FindWindowByName(Name())) {
        pos = window->Pos;
        size = window->Size;
    }
    if (is_movable || is_resizable) {
        char buf[128];
        sprintf(buf, "You need to show the %s for this control to work", TypeName());
        if (is_movable && !lock_move) {
            if (ImGui::DragFloat2("Position", reinterpret_cast<float*>(&pos), 1.0f, 0.0f, 0.0f, "%.0f")) {
                ImGui::SetWindowPos(Name(), pos);
            }
            ImGui::ShowHelp(buf);
        }
        if (is_resizable && !lock_size && !auto_size) {
            if (ImGui::DragFloat2("Size", reinterpret_cast<float*>(&size), 1.0f, 0.0f, 0.0f, "%.0f")) {
                ImGui::SetWindowSize(Name(), size);
            }
            ImGui::ShowHelp(buf);
        }
    }
    ImGui::StartSpacedElements(180.f);
    if (is_movable) {
        ImGui::NextSpacedElement();
        ImGui::Checkbox("Lock Position", &lock_move);
    }
    if (is_resizable) {
        ImGui::NextSpacedElement();
        ImGui::Checkbox("Lock Size", &lock_size);
        ImGui::NextSpacedElement();
        ImGui::Checkbox("Auto Size", &auto_size);
    }
    ImGui::StartSpacedElements(180.f);
    if (has_titlebar) {
        ImGui::NextSpacedElement();
        ImGui::Checkbox("Show titlebar", &show_titlebar);
    }
    if (has_closebutton) {
        ImGui::NextSpacedElement();
        ImGui::Checkbox("Show close button", &show_closebutton);
    }
    if (can_show_in_main_window) {
        ImGui::NextSpacedElement();
        if (ImGui::Checkbox("Show in main window", &show_menubutton)) {
            MainWindow::Instance().pending_refresh_buttons = true;
        }
    }
}

void ToolboxUIElement::ShowVisibleRadio()
{
    const auto style = ImGui::GetStyle();
    const auto btn_width = ImGui::GetTextLineHeight() * 1.6f;
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - btn_width + (style.FramePadding.x * 3));
    ImGui::PushID(Name());
    ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0, 0.5f));
    const auto color = visible ? ImGui::GetStyleColorVec4(ImGuiCol_Text) : ImVec4(0.1f, 0.1f, 0.1f, 1.f);
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    if (ImGui::Button(visible ? ICON_FA_EYE : ICON_FA_EYE_SLASH, { btn_width ,0})) {
        visible = !visible;
    }
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    ImGui::PopID();
}

bool ToolboxUIElement::DrawTabButton(const bool show_icon, const bool show_text, const bool center_align_text)
{
    ImGui::PushStyleColor(ImGuiCol_Button, visible ? ImGui::GetStyle().Colors[ImGuiCol_Button] : ImVec4(0, 0, 0, 0));
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
    if (show_icon) {
        if (Icon()) {
            ImGui::GetWindowDrawList()->AddText(ImVec2(pos.x, pos.y + ImGui::GetStyle().ItemSpacing.y / 2),
                                                ImColor(ImGui::GetStyle().Colors[ImGuiCol_Text]), Icon());
        }
    }
    if (show_text) {
        ImGui::GetWindowDrawList()->AddText(ImVec2(text_x, pos.y + ImGui::GetStyle().ItemSpacing.y / 2),
                                            ImColor(ImGui::GetStyle().Colors[ImGuiCol_Text]), Name());
    }

    if (clicked) {
        visible = !visible;
    }
    ImGui::PopStyleColor();
    return clicked;
}
