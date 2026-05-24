#include "stdafx.h"

#include <GWToolbox.h>
#include <ImGuiAddons.h>
#include <ToolboxUIElement.h>
#include <Windows/MainWindow.h>

#include <Modules/ToolboxSettings.h>

#include <GWCA/GameEntities/Frame.h>
#include <Utils/ToolboxUtils.h>


namespace {
    constexpr ImVec2 empty_imvec2 = {0, 0};
    constexpr GW::Vec2f empty_gwvec2f = {0, 0};

    struct FrameLabel {
        const char* label;
        const wchar_t* label_ws;
    };
    constexpr FrameLabel available_frame_labels[] = {
        {"Compass", L"Compass"},   {"Effects Monitor", L"Effects"}, {"Inventory", L"Inventory"},        {"Mission Map", L"MapWindow"}, {"Quest Log", L"Quest"},
        {"Skillbar", L"Skillbar"}, {"Target", L"Target"},           {"Upkeep Monitor", L"SkillUpkeep"}, {"Weapon Bar", L"WeaponBar"},
    };

    struct CachedFrameState {
        bool requested = false;
        ImVec2 position = {};
    };
    clock_t last_frame_check = TIMER_INIT();
    CachedFrameState frames_by_label[_countof(available_frame_labels)];



    CachedFrameState* GetCachedFrameState(const char* label)
    {
        for (size_t i = 0; label && i < _countof(available_frame_labels); i++) {
            if (strcmp(available_frame_labels[i].label, label) == 0) {
                frames_by_label[i].requested = true;
                return &frames_by_label[i];
            }
        }
        return nullptr;
    }

    bool ImVec2Eq(const ImVec2& a, const ImVec2& b) {
        return a.x == b.x && a.y == b.y;
    }
}

void ToolboxUIElement::UpdateCachedFrameStates()
{
    if (!ToolboxUtils::FrameRateCheck(last_frame_check, 30)) return;
    const GW::UI::Frame* root = nullptr;
    for (size_t i = 0; i < _countof(available_frame_labels); i++) {
        auto& state = frames_by_label[i];
        if (!state.requested) continue;
        state.requested = false;
        const auto frame = GW::UI::GetFrameByLabel(available_frame_labels[i].label_ws);
        if (!frame) continue;
        if (!root) root = GW::UI::GetFrameByLabel(L"Game");
        const auto pos = frame->position.GetTopLeftOnScreen(root);
        state.position = {std::round(pos.x), std::round(pos.y)};
    }
}
void ToolboxUIElement::UpdateLocationAgainstSnappedFrame()
{
    const std::string& active_snap = ToolboxSettings::is_in_mobile_mode ? mobile_snapped_frame_label : snapped_frame_label;
    if (active_snap.empty()) return;
    const auto snapped_frame_state = GetCachedFrameState(active_snap.c_str());
    if (!snapped_frame_state) return;
    const auto& frame_pos = snapped_frame_state->position;
    if (!ImVec2Eq(last_frame_pos, empty_imvec2) && !ImVec2Eq(last_frame_pos,frame_pos)) {
        const auto window = ImGui::FindWindowByName(Name());
        if (window) {
            // Window deviates from where we put it — moved externally, update offset
            const auto diff_x = last_frame_pos.x - frame_pos.x;
            const auto diff_y = last_frame_pos.y - frame_pos.y;
            ImGui::SetWindowPos(window, {window->Pos.x - diff_x, window->Pos.y - diff_y});
        }
    }
    last_frame_pos = frame_pos;
}

bool* ToolboxUIElement::GetVisiblePtr()
{
    if (!has_closebutton) return &visible;
    if (!show_closebutton) return nullptr;
    if (ToolboxSettings::is_in_explorable ? !ToolboxSettings::show_close_in_explorable : !ToolboxSettings::show_close_in_outpost) return nullptr;
    return &visible;
}

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
    LOAD_BOOL(auto_resize_on_collapse);
    LOAD_FLOAT(collapsed_size[0]); LOAD_FLOAT(collapsed_size[1]);
    LOAD_FLOAT(expanded_size[0]); LOAD_FLOAT(expanded_size[1]);
    LOAD_BOOL(show_titlebar);
    LOAD_BOOL(show_closebutton);
    LOAD_BOOL(show_breakout_button);
    LOAD_BOOL(lock_breakout_button);
    LOAD_STRING(snapped_frame_label);
    LOAD_BOOL(mobile_lock_move);
    LOAD_BOOL(mobile_lock_size);
    LOAD_BOOL(mobile_auto_size);
    LOAD_STRING(mobile_snapped_frame_label);
    has_normal_layout = ini->GetBoolValue(Name(), "has_normal_layout", false);
    if (has_normal_layout) {
        LOAD_FLOAT(normal_pos[0]); LOAD_FLOAT(normal_pos[1]);
        LOAD_FLOAT(normal_size[0]); LOAD_FLOAT(normal_size[1]);
    }
    has_mobile_layout = ini->GetBoolValue(Name(), "has_mobile_layout", false);
    if (has_mobile_layout) {
        LOAD_FLOAT(mobile_pos[0]); LOAD_FLOAT(mobile_pos[1]);
        LOAD_FLOAT(mobile_size[0]); LOAD_FLOAT(mobile_size[1]);
    }
}

void ToolboxUIElement::SaveSettings(ToolboxIni* ini)
{
    // Sync current mode's stored positions from the live window.
    // Guard with context check: SaveSettings is called a second time after ImGui is destroyed
    // (from UpdateTerminating -> ToggleModule), at which point FindWindowByName would crash.
    if (ImGui::GetCurrentContext()) {
        if (const auto window = ImGui::FindWindowByName(Name())) {
            if (ToolboxSettings::is_in_mobile_mode) {
                mobile_pos[0] = window->Pos.x; mobile_pos[1] = window->Pos.y;
                mobile_size[0] = window->SizeFull.x; mobile_size[1] = window->SizeFull.y;
                has_mobile_layout = true;
            } else {
                normal_pos[0] = window->Pos.x; normal_pos[1] = window->Pos.y;
                normal_size[0] = window->SizeFull.x; normal_size[1] = window->SizeFull.y;
                has_normal_layout = true;
            }
        }
    }
    ToolboxModule::SaveSettings(ini);
    SAVE_BOOL(visible);
    SAVE_BOOL(show_menubutton);
    SAVE_BOOL(lock_move);
    SAVE_BOOL(lock_size);
    SAVE_BOOL(auto_size);
    SAVE_BOOL(auto_resize_on_collapse);
    SAVE_FLOAT(collapsed_size[0]); SAVE_FLOAT(collapsed_size[1]);
    SAVE_FLOAT(expanded_size[0]); SAVE_FLOAT(expanded_size[1]);
    SAVE_BOOL(show_titlebar);
    SAVE_BOOL(show_closebutton);
    SAVE_BOOL(show_breakout_button);
    SAVE_BOOL(lock_breakout_button);
    SAVE_STRING(snapped_frame_label);
    SAVE_BOOL(mobile_lock_move);
    SAVE_BOOL(mobile_lock_size);
    SAVE_BOOL(mobile_auto_size);
    SAVE_STRING(mobile_snapped_frame_label);
    ini->SetBoolValue(Name(), "has_normal_layout", has_normal_layout);
    if (has_normal_layout) {
        SAVE_FLOAT(normal_pos[0]); SAVE_FLOAT(normal_pos[1]);
        SAVE_FLOAT(normal_size[0]); SAVE_FLOAT(normal_size[1]);
    }
    ini->SetBoolValue(Name(), "has_mobile_layout", has_mobile_layout);
    if (has_mobile_layout) {
        SAVE_FLOAT(mobile_pos[0]); SAVE_FLOAT(mobile_pos[1]);
        SAVE_FLOAT(mobile_size[0]); SAVE_FLOAT(mobile_size[1]);
    }
}

bool ToolboxUIElement::IsMoveLocked() const
{
    return ToolboxSettings::is_in_mobile_mode ? mobile_lock_move : lock_move;
}

bool ToolboxUIElement::IsSizeLocked() const
{
    return ToolboxSettings::is_in_mobile_mode ? mobile_lock_size : lock_size;
}

bool ToolboxUIElement::IsAutoSized() const
{
    return ToolboxSettings::is_in_mobile_mode ? mobile_auto_size : auto_size;
}

ImGuiWindowFlags ToolboxUIElement::GetWinFlags(ImGuiWindowFlags flags) const
{
    if (!ToolboxSettings::move_all) {
        if (IsMoveLocked()) flags |= ImGuiWindowFlags_NoMove;
        if (IsSizeLocked()) flags |= ImGuiWindowFlags_NoResize;
        if (IsAutoSized()) flags |= ImGuiWindowFlags_AlwaysAutoResize;
        if (!show_titlebar) flags |= ImGuiWindowFlags_NoTitleBar;
    }
    if (auto_resize_on_collapse && has_titlebar && show_titlebar) {
        if (const auto* window = ImGui::FindWindowByName(Name())) {
            const bool is_collapsed = window->Collapsed;
            if (!collapse_size_initialized || is_collapsed != prev_was_collapsed) {
                collapse_size_initialized = true;
                prev_was_collapsed = is_collapsed;
                const float* sz = is_collapsed ? collapsed_size : expanded_size;
                const float w = sz[0] > 0.f ? sz[0] : window->SizeFull.x;
                const float h = sz[1] > 0.f ? sz[1] : window->SizeFull.y;
                ImGui::SetNextWindowSize({w, h});
            }
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

void ToolboxUIElement::OnMobileModeChanged(const bool is_mobile)
{
    const auto window = ImGui::FindWindowByName(Name());
    if (is_mobile) {
        // Save current (normal) position before switching
        if (window) {
            normal_pos[0] = window->Pos.x; normal_pos[1] = window->Pos.y;
            normal_size[0] = window->SizeFull.x; normal_size[1] = window->SizeFull.y;
            has_normal_layout = true;
        }
        // Apply stored mobile position if available
        if (has_mobile_layout && window) {
            ImGui::SetWindowPos(window, {mobile_pos[0], mobile_pos[1]});
            ImGui::SetWindowSize(window, {mobile_size[0], mobile_size[1]});
        }
    } else {
        // Save current (mobile) position before switching
        if (window) {
            mobile_pos[0] = window->Pos.x; mobile_pos[1] = window->Pos.y;
            mobile_size[0] = window->SizeFull.x; mobile_size[1] = window->SizeFull.y;
            has_mobile_layout = true;
        }
        // Restore stored normal position if available
        if (has_normal_layout && window) {
            ImGui::SetWindowPos(window, {normal_pos[0], normal_pos[1]});
            ImGui::SetWindowSize(window, {normal_size[0], normal_size[1]});
        }
    }
    // Reset settings tab to reflect new mode
    settings_active_tab = is_mobile ? 1 : 0;
}

void ToolboxUIElement::DrawSizeAndPositionSettings()
{
    const bool is_mobile = ToolboxSettings::is_in_mobile_mode;

    // Auto-select tab based on current mode on first open
    if (settings_active_tab < 0) {
        settings_active_tab = is_mobile ? 1 : 0;
    }

    const auto window = ImGui::FindWindowByName(Name());

    // Sync the current mode's stored positions from the live window
    if (window) {
        if (is_mobile) {
            mobile_pos[0] = window->Pos.x; mobile_pos[1] = window->Pos.y;
            mobile_size[0] = window->SizeFull.x; mobile_size[1] = window->SizeFull.y;
            has_mobile_layout = true;
        } else {
            normal_pos[0] = window->Pos.x; normal_pos[1] = window->Pos.y;
            normal_size[0] = window->SizeFull.x; normal_size[1] = window->SizeFull.y;
            has_normal_layout = true;
        }
    }

    bool& lm = is_mobile ? mobile_lock_move : lock_move;
    bool& ls          = is_mobile ? mobile_lock_size           : lock_size;
    bool& as_ = is_mobile ? mobile_auto_size : auto_size;
    std::string& snap = is_mobile ? mobile_snapped_frame_label : snapped_frame_label;
    float* cur_pos = is_mobile ? mobile_pos : normal_pos;
    float* cur_size = is_mobile ? mobile_size : normal_size;

    char need_show_buf[128];
    snprintf(need_show_buf, sizeof(need_show_buf), "You need to show the %s for this control to work", TypeName());

{
        static const char* frame_label_options[_countof(available_frame_labels) + 1];
        for (size_t i = 0; i < _countof(available_frame_labels); i++) {
            frame_label_options[i] = available_frame_labels[i].label;
        }
        frame_label_options[_countof(available_frame_labels)] = nullptr;

        int current_idx = -1;
        for (size_t i = 0; i < _countof(available_frame_labels); i++) {
            if (available_frame_labels[i].label == snap) {
                current_idx = static_cast<int>(i);
                break;
            }
        }
        const char* preview = current_idx >= 0 ? frame_label_options[current_idx] : "None";

        const bool snap_disabled = !is_movable || lm;
        ImGui::BeginDisabled(snap_disabled);
        if (ImGui::BeginCombo("Snap to Frame", preview)) {
            if (ImGui::Selectable("None", current_idx < 0)) {
                snap.clear();
            }
            for (size_t i = 0; i < _countof(available_frame_labels); i++) {
                const bool selected = (static_cast<int>(i) == current_idx);
                if (ImGui::Selectable(frame_label_options[i], selected)) {
                    snap = available_frame_labels[i].label;
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        ImGui::EndDisabled();
        if (snap_disabled && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            if (!is_movable) {
                ImGui::SetTooltip("This %s cannot be moved", TypeName());
            }
            else {
                ImGui::SetTooltip("Uncheck 'Lock Position' to enable snap-to-frame");
            }
        }
        else {
            ImGui::ShowHelp(need_show_buf);
        }
    }

    // Position
    {
        const bool pos_disabled = !is_movable || lm;
        ImGui::BeginDisabled(pos_disabled);
        if (ImGui::DragFloat2("Position", cur_pos, 1.0f, 0.0f, 0.0f, "%.0f")) {
            if (window) {
                ImGui::SetWindowPos(window, {cur_pos[0], cur_pos[1]});
            }
        }
        ImGui::EndDisabled();
        if (pos_disabled && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            if (!is_movable) {
                ImGui::SetTooltip("This %s cannot be moved", TypeName());
            }
            else {
                ImGui::SetTooltip("Uncheck 'Lock Position' to adjust position");
            }
        }
        else {
            ImGui::ShowHelp(need_show_buf);
        }
    }

    // Size
    {
        const bool size_disabled = !is_resizable || ls || as_;
        ImGui::BeginDisabled(size_disabled);
        if (ImGui::DragFloat2("Size", cur_size, 1.0f, 0.0f, 0.0f, "%.0f")) {
            if (window) {
                ImGui::SetWindowSize(window, {cur_size[0], cur_size[1]});
            }
        }
        ImGui::EndDisabled();
        if (size_disabled && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            if (!is_resizable) {
                ImGui::SetTooltip("This %s cannot be resized", TypeName());
            }
            else if (as_) {
                ImGui::SetTooltip("Uncheck 'Auto Size' to adjust size");
            }
            else {
                ImGui::SetTooltip("Uncheck 'Lock Size' to adjust size");
            }
        }
        else {
            ImGui::ShowHelp(need_show_buf);
        }
    }

    // Lock / auto checkboxes
    ImGui::StartSpacedElements(180.f);

    ImGui::NextSpacedElement();
    ImGui::BeginDisabled(!is_movable);
    ImGui::Checkbox("Lock Position", &lm);
    ImGui::EndDisabled();
    if (!is_movable && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("This %s cannot be moved", TypeName());
    }

    ImGui::NextSpacedElement();
    ImGui::BeginDisabled(!is_resizable);
    ImGui::Checkbox("Lock Size", &ls);
    ImGui::EndDisabled();
    if (!is_resizable && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("This %s cannot be resized", TypeName());
    }

    ImGui::NextSpacedElement();
    ImGui::BeginDisabled(!is_resizable);
    ImGui::Checkbox("Auto Size", &as_);
    ImGui::EndDisabled();
    if (!is_resizable && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("This %s cannot be resized", TypeName());
    }

    // Auto-resize on collapse/expand (only relevant when the window has a title bar)
    if (has_titlebar) {
        if (ImGui::Checkbox("Auto-resize on collapse/expand", &auto_resize_on_collapse)) {
            collapse_size_initialized = false;
        }
        ImGui::ShowHelp("Automatically resize this window when it is collapsed or expanded");
        if (auto_resize_on_collapse) {
            ImGui::Indent();
            if (ImGui::DragFloat2("Collapsed size", collapsed_size, 1.f, 0.f, 0.f, "%.0f")) {
                collapse_size_initialized = false;
            }
            ImGui::ShowHelp("Width and height when the title bar is collapsed; 0 = keep current");
            if (ImGui::DragFloat2("Expanded size", expanded_size, 1.f, 0.f, 0.f, "%.0f")) {
                collapse_size_initialized = false;
            }
            ImGui::ShowHelp("Width and height when the window is expanded; 0 = keep current");
            ImGui::Unindent();
        }
    }

    // Shared settings (not per-mode) drawn below the two-column layout
    ImGui::StartSpacedElements(180.f);

    ImGui::NextSpacedElement();
    ImGui::BeginDisabled(!has_titlebar);
    ImGui::Checkbox("Show titlebar", &show_titlebar);
    ImGui::EndDisabled();
    if (!has_titlebar && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("This %s has no titlebar", TypeName());
    }

    ImGui::NextSpacedElement();
    ImGui::BeginDisabled(!has_closebutton);
    ImGui::Checkbox("Show close button", &show_closebutton);
    ImGui::EndDisabled();
    if (!has_closebutton && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("This %s has no close button", TypeName());
    }

    ImGui::NextSpacedElement();
    ImGui::BeginDisabled(!can_show_in_main_window);
    if (ImGui::Checkbox("Show in main window", &show_menubutton)) {
        if (can_show_in_main_window) {
            MainWindow::Instance().pending_refresh_buttons = true;
        }
    }
    ImGui::EndDisabled();
    if (!can_show_in_main_window && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("This %s cannot be shown in the main window", TypeName());
    }

    ImGui::CheckboxWithHelp("Show breakout button", &show_breakout_button,
        "Shows a small floating button on screen that toggles this window.\nRight-click the button to remove it.");
    if (show_breakout_button) {
        ImGui::Indent();
        ImGui::Checkbox("Lock breakout button position", &lock_breakout_button);
        if (!lock_breakout_button) {
            char breakout_window_id[256];
            snprintf(breakout_window_id, sizeof(breakout_window_id), "%s##breakout_btn", Name());
            const auto breakout_window = ImGui::FindWindowByName(breakout_window_id);
            ImVec2 breakout_pos(0, 0);
            if (breakout_window) {
                breakout_pos = breakout_window->Pos;
            }
            if (ImGui::DragFloat2("Breakout position", reinterpret_cast<float*>(&breakout_pos), 1.0f, 0.0f, 0.0f, "%.0f")) {
                ImGui::SetWindowPos(breakout_window_id, breakout_pos);
            }
            ImGui::ShowHelp("You need to show the breakout button for this control to work");
        }
        ImGui::Unindent();
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

void ToolboxUIElement::DrawBreakoutButton(IDirect3DDevice9*)
{
    if (!show_breakout_button) return;

    char window_id[256];
    snprintf(window_id, sizeof(window_id), "%s##breakout_btn", Name());

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav;

    if (!ToolboxSettings::move_all && lock_breakout_button) {
        flags |= ImGuiWindowFlags_NoMove;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {6.f, 6.f});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, {10.f, 10.f});

    if (ImGui::Begin(window_id, nullptr, flags)) {
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {6.f, 4.f});
        const float btn_size = ImGui::GetTextLineHeight() + ImGui::GetStyle().FramePadding.y * 2.f;
        const char* icon = Icon();

        const auto active_col = ImGui::GetStyle().Colors[ImGuiCol_ButtonActive];
        const auto inactive_col = ImVec4(0.15f, 0.15f, 0.15f, 0.8f);
        ImGui::PushStyleColor(ImGuiCol_Button, visible ? active_col : inactive_col);

        bool clicked;
        if (icon && *icon) {
            clicked = ImGui::Button(icon, {btn_size, btn_size});
        } else {
            char label[4] = {};
            const auto* name = Name();
            for (size_t i = 0; i < 2 && name[i]; i++) {
                label[i] = name[i];
            }
            clicked = ImGui::Button(label, {btn_size, btn_size});
        }

        ImGui::PopStyleColor();
        ImGui::PopStyleVar(); // FramePadding

        if (clicked) {
            ToggleVisible();
        }

        if (ImGui::IsItemHovered()) {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {8.f, 6.f});
            ImGui::BeginTooltip();
            ImGui::Text("%s", Name());
            ImGui::EndTooltip();
            ImGui::PopStyleVar();
        }

        if (ImGui::BeginPopupContextWindow()) {
            if (ImGui::MenuItem("Remove breakout button")) {
                show_breakout_button = false;
            }
            ImGui::EndPopup();
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
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
