#include "stdafx.h"

#include <Utils/GuiUtils.h>

#include <Widgets/ClockWidget.h>
#include <Modules/ToolboxSettings.h>
#include <Defines.h>

void ClockWidget::Draw(IDirect3DDevice9*)
{
    if (!visible) {
        return;
    }

    SYSTEMTIME time;
    GetLocalTime(&time);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    ImGui::SetNextWindowSize(ImVec2(250.0f, 90.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(Name(), nullptr, GetWinFlags())) {
        static char timer[32];
        if (use_24h_clock) {
            if (show_seconds) {
                snprintf(timer, 32, "%02d:%02d:%02d", time.wHour, time.wMinute, time.wSecond);
            }
            else {
                snprintf(timer, 32, "%02d:%02d", time.wHour, time.wMinute);
            }
        }
        else {
            int hour = time.wHour % 12;
            if (hour == 0) {
                hour = 12;
            }
            if (show_seconds) {
                snprintf(timer, 32, "%d:%02d:%02d %s", hour, time.wMinute, time.wSecond, time.wHour >= 12 ? "p.m." : "a.m.");
            }
            else {
                snprintf(timer, 32, "%d:%02d %s", hour, time.wMinute, time.wHour >= 12 ? "p.m." : "a.m.");
            }
        }
        ImGui::PushFont(GetFont(GuiUtils::FontSize::widget_large));
        const ImVec2 cur = ImGui::GetCursorPos();
        ImGui::SetCursorPos(ImVec2(cur.x + 2, cur.y + 2));
        ImGui::TextColored(ImColor(0, 0, 0), timer);
        ImGui::SetCursorPos(cur);
        ImGui::Text(timer);
        ImGui::PopFont();
    }
    ImGui::End();
    ImGui::PopStyleColor();
}

void ClockWidget::LoadSettings(ToolboxIni* ini)
{
    ToolboxWidget::LoadSettings(ini);
    LOAD_BOOL(use_24h_clock);
    LOAD_BOOL(show_seconds);
}

void ClockWidget::SaveSettings(ToolboxIni* ini)
{
    ToolboxWidget::SaveSettings(ini);
    SAVE_BOOL(use_24h_clock);
    SAVE_BOOL(show_seconds);
}

void ClockWidget::DrawSettingsInternal()
{
    ImGui::Checkbox("Use 24h clock", &use_24h_clock);
    ImGui::Checkbox("Show seconds", &show_seconds);
}
