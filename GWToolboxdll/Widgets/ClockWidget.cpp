#include "stdafx.h"

#include <Utils/GuiUtils.h>

#include <Widgets/ClockWidget.h>
#include <Modules/ToolboxSettings.h>
#include <Defines.h>

#include "Utils/FontLoader.h"

namespace {
    ClockWidget::Settings settings;
}

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
        if (settings.use_24h_clock) {
            if (settings.show_seconds) {
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
            if (settings.show_seconds) {
                snprintf(timer, 32, "%d:%02d:%02d %s", hour, time.wMinute, time.wSecond, time.wHour >= 12 ? "p.m." : "a.m.");
            }
            else {
                snprintf(timer, 32, "%d:%02d %s", hour, time.wMinute, time.wHour >= 12 ? "p.m." : "a.m.");
            }
        }
        ImGui::PushFont(FontLoader::GetFont(), settings.font_size);
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

void ClockWidget::Initialize()
{
    ToolboxWidget::Initialize();
    SettingsRegistry::Register(this, settings);
}

void ClockWidget::LoadSettings(SettingsDoc& doc, ToolboxIni* legacy)
{
    ToolboxWidget::LoadSettings(doc, legacy);
    doc.GetStruct(Name(), settings);
}

void ClockWidget::SaveSettings(SettingsDoc& doc)
{
    ToolboxWidget::SaveSettings(doc);
    doc.SetStruct(Name(), settings);
}

void ClockWidget::DrawSettingsInternal()
{
    ImGui::Checkbox("Use 24h clock", &settings.use_24h_clock);
    ImGui::Checkbox("Show seconds", &settings.show_seconds);
    ImGui::DragFloat("Text size in px", &settings.font_size, 1.f, 0.f, 48.f, "%.f");

}
