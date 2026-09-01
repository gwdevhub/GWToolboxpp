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
    // 游戏内部界面名称（不可汉化，否则查找失败）
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

    bool ImVec2Eq(const ImVec2& a, const ImVec2& b)
    {
        return a.x == b.x && a.y == b.y;
    }

    // Live rects of currently-shown breakout buttons, keyed by the owning element.
    // Lets a newly-shown button pick a spot near the screen centre that doesn't overlap the others.
    std::unordered_map<const ToolboxUIElement*, ImRect> breakout_button_rects;

} // namespace

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
    const bool is_mobile = ToolboxSettings::is_in_mobile_mode;
    const std::string& active_snap = is_mobile ? mobile_snapped_frame_label : snapped_frame_label;
    if (active_snap.empty()) return;

    const auto snapped_frame_state = GetCachedFrameState(active_snap.c_str());
    if (!snapped_frame_state) return;

    const auto& frame_pos = snapped_frame_state->position;
    if (ImVec2Eq(frame_pos, empty_imvec2)) return; // 位置尚未填充

    float* snap_off = (is_mobile ? mobile_snap_offset : snap_offset).data();
    bool& needs_init = is_mobile ? mobile_snap_offset_needs_init : snap_offset_needs_init;

    const auto window = ImGui::FindWindowByName(Name());

    // 在第一次获取到有效帧位置时，根据当前窗口位置计算偏移量
    if (needs_init) {
        needs_init = false;
        if (window) {
            snap_off[0] = window->Pos.x - frame_pos.x;
            snap_off[1] = window->Pos.y - frame_pos.y;
        }
    }

    const float target_x = frame_pos.x + snap_off[0];
    const float target_y = frame_pos.y + snap_off[1];

    // 保留备用屏幕坐标，以便帧消失时仍能维持有效位置
    float* cur_pos = (is_mobile ? mobile_pos : normal_pos).data();
    cur_pos[0] = target_x;
    cur_pos[1] = target_y;
    if (is_mobile)
        has_mobile_layout = true;
    else
        has_normal_layout = true;

    if (window) {
        ImGui::SetWindowPos(window, {target_x, target_y});
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
    SettingsRegistry::RegisterField(this, "visible", &visible);
    SettingsRegistry::RegisterField(this, "show_menubutton", &show_menubutton);
    SettingsRegistry::RegisterField(this, "lock_move", &lock_move);
    SettingsRegistry::RegisterField(this, "lock_size", &lock_size);
    SettingsRegistry::RegisterField(this, "auto_size", &auto_size);
    SettingsRegistry::RegisterField(this, "auto_resize_on_collapse", &auto_resize_on_collapse);
    SettingsRegistry::RegisterField(this, "collapsed_size", &collapsed_size);
    SettingsRegistry::RegisterField(this, "expanded_size", &expanded_size);
    SettingsRegistry::RegisterField(this, "show_titlebar", &show_titlebar);
    SettingsRegistry::RegisterField(this, "show_closebutton", &show_closebutton);
    SettingsRegistry::RegisterField(this, "show_breakout_button", &show_breakout_button);
    SettingsRegistry::RegisterField(this, "lock_breakout_button", &lock_breakout_button);
    SettingsRegistry::RegisterField(this, "breakout_pos", &breakout_pos);
    SettingsRegistry::RegisterField(this, "snapped_frame_label", &snapped_frame_label);
    SettingsRegistry::RegisterField(this, "snap_offset", &snap_offset);
    SettingsRegistry::RegisterField(this, "mobile_lock_move", &mobile_lock_move);
    SettingsRegistry::RegisterField(this, "mobile_lock_size", &mobile_lock_size);
    SettingsRegistry::RegisterField(this, "mobile_auto_size", &mobile_auto_size);
    SettingsRegistry::RegisterField(this, "mobile_snapped_frame_label", &mobile_snapped_frame_label);
    SettingsRegistry::RegisterField(this, "mobile_snap_offset", &mobile_snap_offset);
    SettingsRegistry::RegisterField(this, "has_normal_layout", &has_normal_layout);
    SettingsRegistry::RegisterField(this, "normal_pos", &normal_pos);
    SettingsRegistry::RegisterField(this, "normal_size", &normal_size);
    SettingsRegistry::RegisterField(this, "has_mobile_layout", &has_mobile_layout);
    SettingsRegistry::RegisterField(this, "mobile_pos", &mobile_pos);
    SettingsRegistry::RegisterField(this, "mobile_size", &mobile_size);
}

void ToolboxUIElement::Terminate()
{
    breakout_button_rects.erase(this);
    ToolboxModule::Terminate();
}

void ToolboxUIElement::LoadSettings(SettingsDoc& doc, ToolboxIni* legacy)
{
    ToolboxModule::LoadSettings(doc, legacy);
    if (doc.Has(Name(), "breakout_pos") || (legacy && legacy->KeyExists(Name(), "breakout_pos[0]"))) {
        pending_breakout_pos = true;
    }
    if (!snapped_frame_label.empty() && !doc.Has(Name(), "snap_offset") && !(legacy && legacy->KeyExists(Name(), "snap_offset[0]"))) {
        snap_offset_needs_init = true;
    }
    if (!mobile_snapped_frame_label.empty() && !doc.Has(Name(), "mobile_snap_offset") && !(legacy && legacy->KeyExists(Name(), "mobile_snap_offset[0]"))) {
        mobile_snap_offset_needs_init = true;
    }
    if (!has_normal_layout) {
        normal_pos = {};
        normal_size = {};
    }
    if (!has_mobile_layout) {
        mobile_pos = {};
        mobile_size = {};
    }
}

void ToolboxUIElement::SaveSettings(SettingsDoc& doc)
{
    if (ImGui::GetCurrentContext()) {
        if (const auto window = ImGui::FindWindowByName(Name())) {
            if (ToolboxSettings::is_in_mobile_mode) {
                mobile_pos[0] = window->Pos.x;
                mobile_pos[1] = window->Pos.y;
                mobile_size[0] = window->SizeFull.x;
                mobile_size[1] = window->SizeFull.y;
                has_mobile_layout = true;
            }
            else {
                normal_pos[0] = window->Pos.x;
                normal_pos[1] = window->Pos.y;
                normal_size[0] = window->SizeFull.x;
                normal_size[1] = window->SizeFull.y;
                has_normal_layout = true;
            }
        }
    }
    if (ImGui::GetCurrentContext() && show_breakout_button) {
        char breakout_window_id[256];
        snprintf(breakout_window_id, sizeof(breakout_window_id), "%s##breakout_btn", Name());
        if (const auto bw = ImGui::FindWindowByName(breakout_window_id)) {
            breakout_pos[0] = bw->Pos.x;
            breakout_pos[1] = bw->Pos.y;
        }
    }
    ToolboxModule::SaveSettings(doc);
    if (!has_normal_layout) {
        doc.EraseKey(Name(), "normal_pos");
        doc.EraseKey(Name(), "normal_size");
    }
    if (!has_mobile_layout) {
        doc.EraseKey(Name(), "mobile_pos");
        doc.EraseKey(Name(), "mobile_size");
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
                const float* sz = (is_collapsed ? collapsed_size : expanded_size).data();
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
        SettingsName(), Icon(),
        [this](const std::string&, const bool is_showing) {
            ShowVisibleRadio();
            if (!is_showing) {
                return;
            }
            DrawSizeAndPositionSettings();
            DrawSettingsInternal();
        },
        SettingsWeighting()
    );
}

void ToolboxUIElement::OnMobileModeChanged(const bool is_mobile)
{
    const auto window = ImGui::FindWindowByName(Name());
    if (is_mobile) {
        if (window) {
            normal_pos[0] = window->Pos.x;
            normal_pos[1] = window->Pos.y;
            normal_size[0] = window->SizeFull.x;
            normal_size[1] = window->SizeFull.y;
            has_normal_layout = true;
        }
        if (has_mobile_layout && window) {
            ImGui::SetWindowPos(window, {mobile_pos[0], mobile_pos[1]});
            ImGui::SetWindowSize(window, {mobile_size[0], mobile_size[1]});
        }
    }
    else {
        if (window) {
            mobile_pos[0] = window->Pos.x;
            mobile_pos[1] = window->Pos.y;
            mobile_size[0] = window->SizeFull.x;
            mobile_size[1] = window->SizeFull.y;
            has_mobile_layout = true;
        }
        if (has_normal_layout && window) {
            ImGui::SetWindowPos(window, {normal_pos[0], normal_pos[1]});
            ImGui::SetWindowSize(window, {normal_size[0], normal_size[1]});
        }
    }
    settings_active_tab = is_mobile ? 1 : 0;
}

void ToolboxUIElement::DrawSizeAndPositionSettings()
{
    const bool is_mobile = ToolboxSettings::is_in_mobile_mode;

    // 首次打开时根据当前模式自动选择选项卡
    if (settings_active_tab < 0) {
        settings_active_tab = is_mobile ? 1 : 0;
    }

    const auto window = ImGui::FindWindowByName(Name());

    if (window) {
        if (is_mobile) {
            mobile_pos[0] = window->Pos.x;
            mobile_pos[1] = window->Pos.y;
            mobile_size[0] = window->SizeFull.x;
            mobile_size[1] = window->SizeFull.y;
            has_mobile_layout = true;
        }
        else {
            normal_pos[0] = window->Pos.x;
            normal_pos[1] = window->Pos.y;
            normal_size[0] = window->SizeFull.x;
            normal_size[1] = window->SizeFull.y;
            has_normal_layout = true;
        }
    }

    bool& lm = is_mobile ? mobile_lock_move : lock_move;
    bool& ls = is_mobile ? mobile_lock_size : lock_size;
    bool& as_ = is_mobile ? mobile_auto_size : auto_size;
    std::string& snap = is_mobile ? mobile_snapped_frame_label : snapped_frame_label;
    float* cur_pos = (is_mobile ? mobile_pos : normal_pos).data();
    float* cur_size = (is_mobile ? mobile_size : normal_size).data();
    float* snap_off = (is_mobile ? mobile_snap_offset : snap_offset).data();
    bool& needs_init_ref = is_mobile ? mobile_snap_offset_needs_init : snap_offset_needs_init;

    char need_show_buf[128];
    snprintf(need_show_buf, sizeof(need_show_buf), "你需要显示 %s 才能使此控件生效", TypeName());

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
        const char* preview = current_idx >= 0 ? frame_label_options[current_idx] : "无";

        const bool snap_disabled = !is_movable || lm;
        ImGui::BeginDisabled(snap_disabled);
        const std::string prev_snap = snap;
        if (ImGui::BeginCombo("吸附到界面元素", preview)) {
            if (ImGui::Selectable("无", current_idx < 0)) {
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
        // 当吸附目标切换为新界面时，计划从当前窗口位置初始化偏移量
        if (snap != prev_snap && !snap.empty()) {
            needs_init_ref = true;
            snap_off[0] = 0.f;
            snap_off[1] = 0.f;
        }
        if (snap_disabled && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            if (!is_movable) {
                ImGui::SetTooltip("此 %s 不可移动", TypeName());
            }
            else {
                ImGui::SetTooltip("取消勾选“锁定位置”以启用吸附功能");
            }
        }
        else {
            ImGui::ShowHelp(need_show_buf);
        }
    }

    // 位置 / 吸附偏移 — 二者互斥
    {
        const bool pos_disabled = !is_movable || lm;
        ImGui::BeginDisabled(pos_disabled);
        if (!snap.empty()) {
            if (ImGui::DragFloat2("Snap Offset", snap_off, 1.0f, 0.0f, 0.0f, "%.0f")) {
                needs_init_ref = false; // user explicitly set offset; cancel pending init
            }
        }
        else {
            if (ImGui::DragFloat2("Position", cur_pos, 1.0f, 0.0f, 0.0f, "%.0f")) {
                if (window) {
                    ImGui::SetWindowPos(window, {cur_pos[0], cur_pos[1]});
                }
            }
        }
        ImGui::EndDisabled();
        if (pos_disabled && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            if (!is_movable) {
                ImGui::SetTooltip("此 %s 不可移动", TypeName());
            }
            else {
                ImGui::SetTooltip("取消勾选“锁定位置”以调整位置");
            }
        }
        else if (!snap.empty()) {
            ImGui::ShowHelp("相对于吸附界面左上角的像素偏移量");
        }
        else {
            ImGui::ShowHelp(need_show_buf);
        }
    }

    {
        const bool size_disabled = !is_resizable || ls || as_;
        ImGui::BeginDisabled(size_disabled);
        if (ImGui::DragFloat2("大小", cur_size, 1.0f, 0.0f, 0.0f, "%.0f")) {
            if (window) {
                ImGui::SetWindowSize(window, {cur_size[0], cur_size[1]});
            }
        }
        ImGui::EndDisabled();
        if (size_disabled && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            if (!is_resizable) {
                ImGui::SetTooltip("此 %s 不可调整大小", TypeName());
            }
            else if (as_) {
                ImGui::SetTooltip("取消勾选“自动大小”以调整尺寸");
            }
            else {
                ImGui::SetTooltip("取消勾选“锁定大小”以调整尺寸");
            }
        }
        else {
            ImGui::ShowHelp(need_show_buf);
        }
    }

    ImGui::StartSpacedElements(180.f);

    ImGui::NextSpacedElement();
    ImGui::BeginDisabled(!is_movable);
    ImGui::Checkbox("锁定位置", &lm);
    ImGui::EndDisabled();
    if (!is_movable && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("此 %s 不可移动", TypeName());
    }

    ImGui::NextSpacedElement();
    ImGui::BeginDisabled(!is_resizable);
    ImGui::Checkbox("锁定大小", &ls);
    ImGui::EndDisabled();
    if (!is_resizable && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("此 %s 不可调整大小", TypeName());
    }

    ImGui::NextSpacedElement();
    ImGui::BeginDisabled(!is_resizable);
    ImGui::Checkbox("自动大小", &as_);
    ImGui::EndDisabled();
    if (!is_resizable && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("此 %s 不可调整大小", TypeName());
    }

    // 折叠/展开时自动调整大小（仅当窗口有标题栏时有效）
    ImGui::BeginDisabled(!has_titlebar);
    if (ImGui::Checkbox("折叠/展开时自动调整大小", &auto_resize_on_collapse)) {
        collapse_size_initialized = false;
    }
    ImGui::EndDisabled();
    if (!has_titlebar && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("此 %s 没有标题栏", TypeName());
    }
    else {
        ImGui::ShowHelp("当窗口折叠或展开时自动调整其大小");
    }
    ImGui::Indent();
    ImGui::BeginDisabled(!auto_resize_on_collapse || !has_titlebar);
    if (ImGui::DragFloat2("折叠时大小", collapsed_size.data(), 1.f, 0.f, 0.f, "%.0f")) {
        collapse_size_initialized = false;
    }
    ImGui::ShowHelp("标题栏折叠时的宽度和高度；0 表示保持当前尺寸");
    if (ImGui::DragFloat2("展开时大小", expanded_size.data(), 1.f, 0.f, 0.f, "%.0f")) {
        collapse_size_initialized = false;
    }
    ImGui::ShowHelp("窗口展开时的宽度和高度；0 表示保持当前尺寸");
    ImGui::EndDisabled();
    ImGui::Unindent();

    // 以下为通用设置（不区分模式）
    ImGui::StartSpacedElements(180.f);

    ImGui::NextSpacedElement();
    ImGui::BeginDisabled(!has_titlebar);
    ImGui::Checkbox("显示标题栏", &show_titlebar);
    ImGui::EndDisabled();
    if (!has_titlebar && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("此 %s 没有标题栏", TypeName());
    }

    ImGui::NextSpacedElement();
    ImGui::BeginDisabled(!has_closebutton);
    ImGui::Checkbox("显示关闭按钮", &show_closebutton);
    ImGui::EndDisabled();
    if (!has_closebutton && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("此 %s 没有关闭按钮", TypeName());
    }

    ImGui::NextSpacedElement();
    ImGui::BeginDisabled(!can_show_in_main_window);
    if (ImGui::Checkbox("在主窗口中显示", &show_menubutton)) {
        if (can_show_in_main_window) {
            MainWindow::Instance().pending_refresh_buttons = true;
        }
    }
    ImGui::EndDisabled();
    if (!can_show_in_main_window && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("此 %s 无法在主窗口中显示", TypeName());
    }

    ImGui::CheckboxWithHelp("显示浮动按钮", &show_breakout_button, "显示一个小型浮动按钮，用于切换此窗口的显示。\n右键点击该按钮可移除它。");
    if (show_breakout_button) {
        ImGui::Indent();
        ImGui::Checkbox("锁定浮动按钮位置", &lock_breakout_button);
        if (!lock_breakout_button) {
            char breakout_window_id[256];
            snprintf(breakout_window_id, sizeof(breakout_window_id), "%s##breakout_btn", Name());
            const auto breakout_window = ImGui::FindWindowByName(breakout_window_id);
            ImVec2 _breakout_pos(0, 0);
            if (breakout_window) {
                _breakout_pos = breakout_window->Pos;
            }
            if (ImGui::DragFloat2("浮动按钮位置", reinterpret_cast<float*>(&_breakout_pos), 1.0f, 0.0f, 0.0f, "%.0f")) {
                ImGui::SetWindowPos(breakout_window_id, _breakout_pos);
            }
            ImGui::ShowHelp("你需要显示浮动按钮才能调整其位置");
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
    if (ImGui::Button(visible ? ICON_FA_EYE : ICON_FA_EYE_SLASH, {btn_width, 0})) {
        visible = !visible;
    }
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    ImGui::PopID();
}

namespace {
    bool BreakoutRectsOverlap(const ImRect& a, const ImRect& b)
    {
        return a.Min.x < b.Max.x && a.Max.x > b.Min.x && a.Min.y < b.Max.y && a.Max.y > b.Min.y;
    }

    // Minimum translation needed to push `self` out of every overlapping breakout button.
    // Returns {0,0} when it already clears all of them.
    ImVec2 ResolveBreakoutOverlap(const ToolboxUIElement* self_element, const ImRect& self)
    {
        ImVec2 push = {0.f, 0.f};
        for (const auto& [element, other] : breakout_button_rects) {
            if (element == self_element) continue;
            const ImRect moved({self.Min.x + push.x, self.Min.y + push.y}, {self.Max.x + push.x, self.Max.y + push.y});
            const float ox = ImMin(moved.Max.x, other.Max.x) - ImMax(moved.Min.x, other.Min.x);
            const float oy = ImMin(moved.Max.y, other.Max.y) - ImMax(moved.Min.y, other.Min.y);
            if (ox <= 0.f || oy <= 0.f) continue; // no overlap
            if (ox < oy) {
                push.x += moved.GetCenter().x < other.GetCenter().x ? -ox : ox;
            }
            else {
                push.y += moved.GetCenter().y < other.GetCenter().y ? -oy : oy;
            }
        }
        return push;
    }

    // Pick a position starting from the centre of the screen, cascading until it clears every other breakout button.
    ImVec2 GetDefaultBreakoutPos(const ToolboxUIElement* self_element, const ImVec2& size)
    {
        const ImGuiViewport* vp = ImGui::GetMainViewport();
        const ImVec2 start = {vp->WorkPos.x + (vp->WorkSize.x - size.x) * 0.5f, vp->WorkPos.y + (vp->WorkSize.y - size.y) * 0.5f};
        const ImVec2 max = {vp->WorkPos.x + vp->WorkSize.x, vp->WorkPos.y + vp->WorkSize.y};
        ImVec2 pos = start;
        for (int i = 0; i < 256; i++) {
            const ImRect candidate = {pos, {pos.x + size.x, pos.y + size.y}};
            bool overlaps = false;
            for (const auto& [element, rect] : breakout_button_rects) {
                if (element != self_element && BreakoutRectsOverlap(candidate, rect)) {
                    overlaps = true;
                    break;
                }
            }
            if (!overlaps) break;
            pos.x += size.x + 6.f;
            if (pos.x + size.x > max.x) {
                pos.x = start.x;
                pos.y += size.y + 6.f;
                if (pos.y + size.y > max.y) pos.y = vp->WorkPos.y;
            }
        }
        return pos;
    }
}

void ToolboxUIElement::DrawBreakoutButton(IDirect3DDevice9*)
{
    // Runs for every enabled element every frame, so bail before building the window id.
    if (!show_breakout_button) {
        breakout_button_rects.erase(this);
        return;
    }

    char window_id[256];
    snprintf(window_id, sizeof(window_id), "%s##breakout_btn", Name());

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;

    if (!ToolboxSettings::move_all && lock_breakout_button) {
        flags |= ImGuiWindowFlags_NoMove;
    }

    if (pending_breakout_pos) {
        ImGui::SetNextWindowPos({breakout_pos[0], breakout_pos[1]}, ImGuiCond_Always);
        pending_breakout_pos = false;
        breakout_pos_set = true;
    }
    else if (!breakout_pos_set) {
        // Brand-new button: default to the middle of the screen, nudged so it doesn't land on another button.
        const float est = ImGui::GetFrameHeight() + 16.f;
        const ImVec2 pos = GetDefaultBreakoutPos(this, {est, est});
        ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
        breakout_pos[0] = pos.x;
        breakout_pos[1] = pos.y;
        breakout_pos_set = true;
    }
    else if (const auto bw = ImGui::FindWindowByName(window_id); bw && !(flags & ImGuiWindowFlags_NoMove)) {
        const ImGuiContext* g = ImGui::GetCurrentContext();
        const bool being_moved = g && g->MovingWindow && g->MovingWindow->RootWindow == bw->RootWindow;
        if (!being_moved) {
            const ImVec2 push = ResolveBreakoutOverlap(this, ImRect(bw->Pos, {bw->Pos.x + bw->Size.x, bw->Pos.y + bw->Size.y}));
            if (push.x != 0.f || push.y != 0.f) {
                ImGui::SetNextWindowPos({bw->Pos.x + push.x, bw->Pos.y + push.y}, ImGuiCond_Always);
            }
        }
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
        }
        else {
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
            if (ImGui::MenuItem("移除浮动按钮")) {
                show_breakout_button = false;
            }
            ImGui::EndPopup();
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(2);

    // Keep breakout_pos current so SaveSettings captures the right position even without a live ImGui context.
    // Also record the live rect so other breakout buttons can avoid overlapping this one.
    if (const auto bw = ImGui::FindWindowByName(window_id)) {
        breakout_pos[0] = bw->Pos.x;
        breakout_pos[1] = bw->Pos.y;
        breakout_button_rects[this] = ImRect(bw->Pos, {bw->Pos.x + bw->Size.x, bw->Pos.y + bw->Size.y});
    }
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
            ImGui::GetWindowDrawList()->AddText(ImVec2(pos.x, pos.y + ImGui::GetStyle().ItemSpacing.y / 2), ImColor(ImGui::GetStyle().Colors[ImGuiCol_Text]), Icon());
        }
    }
    if (show_text) {
        ImGui::GetWindowDrawList()->AddText(ImVec2(text_x, pos.y + ImGui::GetStyle().ItemSpacing.y / 2), ImColor(ImGui::GetStyle().Colors[ImGuiCol_Text]), Name());
    }

    if (clicked) {
        visible = !visible;
    }
    ImGui::PopStyleColor();
    return clicked;
}
