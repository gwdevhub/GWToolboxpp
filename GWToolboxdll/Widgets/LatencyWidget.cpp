#include "stdafx.h"

#include <GWCA/Constants/Constants.h>

#include <GWCA/Packets/Opcodes.h>

#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/StoCMgr.h>

#include <Utils/GuiUtils.h>

#include <Widgets/LatencyWidget.h>

namespace {
    constexpr size_t ping_history_len = 10; // GW checks last 10 pings for avg
    uint32_t ping_history[ping_history_len] = { 0 };
    size_t ping_index = 0;
}

void LatencyWidget::Initialize() {
    ToolboxWidget::Initialize();
    GW::StoC::RegisterPacketCallback(&Ping_Entry, GAME_SMSG_PING_REPLY, OnServerPing, 0x800);
    GW::Chat::CreateCommand(L"ping", LatencyWidget::SendPing);
}

void LatencyWidget::Update(float delta) { UNREFERENCED_PARAMETER(delta); }

void LatencyWidget::OnServerPing(GW::HookStatus*, void* packet) {
    uint32_t* packet_as_uint_array = (uint32_t*)packet;
    uint32_t ping = packet_as_uint_array[1];
    if (ping > 4999)
        return; // GW checks this too.
    if (ping_history[ping_index]) {
        ping_index++;
        if (ping_index == ping_history_len) {
            ping_index = 0;
        }
    }
    ping_history[ping_index] = ping;

}

uint32_t LatencyWidget::GetPing() { return ping_history[ping_index]; }
uint32_t LatencyWidget::GetAveragePing() {
    size_t count = 0;
    size_t sum = 0;
    for (size_t i = 0; ping_history[i] && i < ping_history_len; i++) {
        count++;
        sum += ping_history[i];
    }
    return count ? sum / count : 0;
}

void LatencyWidget::Draw(IDirect3DDevice9* pDevice) {
    UNREFERENCED_PARAMETER(pDevice);
    if (!visible) return;
    if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading) return;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    ImGui::SetNextWindowSize(ImVec2(180.0f, 60.0f), ImGuiCond_FirstUseEver);
    const bool ctrl_pressed = ImGui::IsKeyDown(ImGuiKey_ModCtrl);

    if (ImGui::Begin(Name(), nullptr, GetWinFlags(0, !ctrl_pressed))) {
        ImVec2 cur = ImGui::GetCursorPos();
        ImGui::PushFont(GuiUtils::GetFont((GuiUtils::FontSize)font_size));
        ImGui::SetCursorPos(cur);
        uint32_t ping = GetPing();
        ImGui::TextColored(GetColorForPing(ping), "%ums", ping);
        if (show_avg_ping) {
            ping = GetAveragePing();
            ImGui::TextColored(GetColorForPing(ping), "%ums", ping);
        }
        
        ImGui::PopFont();

        ImVec2 size = ImGui::GetWindowSize();
        ImVec2 min = ImGui::GetWindowPos();
        ImVec2 max(min.x + size.x, min.y + size.y);
        if (ctrl_pressed && ImGui::IsMouseReleased(0) && ImGui::IsMouseHoveringRect(min, max)) {
            SendPing();
        }
    }

    ImGui::End();
    ImGui::PopStyleColor();
}

void LatencyWidget::LoadSettings(ToolboxIni* ini) {
    ToolboxWidget::LoadSettings(ini);

    red_threshold = ini->GetLongValue(Name(), VAR_NAME(red_threshold), red_threshold);
    show_avg_ping = ini->GetBoolValue(Name(), VAR_NAME(show_avg_ping), show_avg_ping);
    red_threshold = ini->GetLongValue(Name(), VAR_NAME(red_threshold), red_threshold);
    font_size = ini->GetLongValue(Name(), VAR_NAME(font_size), (int)font_size);
    switch (font_size) {
    case (int)GuiUtils::FontSize::widget_label:
    case (int)GuiUtils::FontSize::widget_small:
    case (int)GuiUtils::FontSize::widget_large:
        break;
    default:
        font_size = (int)GuiUtils::FontSize::widget_small;
        break;
    }
}

void LatencyWidget::SaveSettings(ToolboxIni* ini) {
    ToolboxWidget::SaveSettings(ini);
    ini->SetLongValue(Name(), VAR_NAME(red_threshold), red_threshold);
    ini->SetBoolValue(Name(), VAR_NAME(show_avg_ping), show_avg_ping);
    ini->SetLongValue(Name(), VAR_NAME(font_size), font_size);
}

void LatencyWidget::DrawSettingInternal() { 
    ImGui::SliderInt("Red ping threshold", &red_threshold, 0, 1000);
    ImGui::Checkbox("Show average ping", &show_avg_ping);
    ImGui::Text("Font Size");
    ImGui::Indent();
    if (ImGui::RadioButton("Small", font_size == (int)GuiUtils::FontSize::widget_label))
        font_size = (int)GuiUtils::FontSize::widget_label;
    ImGui::SameLine();
    if (ImGui::RadioButton("Medium", font_size == (int)GuiUtils::FontSize::widget_small))
        font_size = (int)GuiUtils::FontSize::widget_small;
    ImGui::SameLine();
    if (ImGui::RadioButton("Large", font_size == (int)GuiUtils::FontSize::widget_large))
        font_size = (int)GuiUtils::FontSize::widget_large;
    ImGui::Unindent();
}

ImColor LatencyWidget::GetColorForPing(uint32_t ping) {
    LatencyWidget& instance = Instance();
    float x = ping / (float)instance.red_threshold;
    ImColor myColor = ImColor(2.0f * x, 2.0f * (1 - x), 0.0f);
    return myColor;
}

void LatencyWidget::SendPing(const wchar_t*, int , LPWSTR* ) {
    LatencyWidget& instance = Instance();
    char buffer[48];
    snprintf(buffer, sizeof(buffer), "Current Ping: %ums, Avg Ping: %ums", instance.GetPing(), instance.GetAveragePing());
    GW::Chat::SendChat('#', buffer);
}