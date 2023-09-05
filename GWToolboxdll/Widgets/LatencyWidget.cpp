#include "stdafx.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/Packets/Opcodes.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/StoCMgr.h>

#include <Utils/GuiUtils.h>
#include <Defines.h>

#include <Widgets/LatencyWidget.h>

namespace {
    constexpr size_t ping_history_len = 10; // GW checks last 10 pings for avg
    uint32_t ping_history[ping_history_len] = {0};
    size_t ping_index = 0;
}

void LatencyWidget::Initialize()
{
    ToolboxWidget::Initialize();
    GW::StoC::RegisterPacketCallback(&Ping_Entry, GAME_SMSG_PING_REPLY, OnServerPing, 0x800);
    GW::Chat::CreateCommand(L"ping", SendPing);
}

void LatencyWidget::Update(const float) { }

void LatencyWidget::OnServerPing(GW::HookStatus*, void* packet)
{
    const auto packet_as_uint_array = static_cast<uint32_t*>(packet);
    const uint32_t ping = packet_as_uint_array[1];
    if (ping > 4999) {
        return; // GW checks this too.
    }
    if (ping_history[ping_index]) {
        ping_index++;
        if (ping_index == ping_history_len) {
            ping_index = 0;
        }
    }
    ping_history[ping_index] = ping;
}

uint32_t LatencyWidget::GetPing() { return ping_history[ping_index]; }

uint32_t LatencyWidget::GetAveragePing()
{
    size_t count = 0;
    size_t sum = 0;
    for (size_t i = 0; ping_history[i] && i < ping_history_len; i++) {
        count++;
        sum += ping_history[i];
    }
    return count ? sum / count : 0;
}

void LatencyWidget::Draw(IDirect3DDevice9*)
{
    if (!visible) {
        return;
    }
    if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading) {
        return;
    }

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    ImGui::SetNextWindowSize(ImVec2(180.0f, 60.0f), ImGuiCond_FirstUseEver);
    const bool ctrl_pressed = ImGui::IsKeyDown(ImGuiKey_ModCtrl);

    if (ImGui::Begin(Name(), nullptr, GetWinFlags(0, !ctrl_pressed))) {
        const ImVec2 cur = ImGui::GetCursorPos();
        ImGui::PushFont(GetFont(static_cast<GuiUtils::FontSize>(font_size)));
        ImGui::SetCursorPos(cur);
        uint32_t ping = GetPing();
        ImGui::TextColored(GetColorForPing(ping), "%ums", ping);
        if (show_avg_ping) {
            ping = GetAveragePing();
            ImGui::TextColored(GetColorForPing(ping), "%ums", ping);
        }

        ImGui::PopFont();

        const ImVec2 size = ImGui::GetWindowSize();
        const ImVec2 min = ImGui::GetWindowPos();
        const ImVec2 max(min.x + size.x, min.y + size.y);
        if (ctrl_pressed && ImGui::IsMouseReleased(0) && ImGui::IsMouseHoveringRect(min, max)) {
            SendPing();
        }
    }

    ImGui::End();
    ImGui::PopStyleColor();
}

void LatencyWidget::LoadSettings(ToolboxIni* ini)
{
    ToolboxWidget::LoadSettings(ini);

    LOAD_UINT(red_threshold);
    LOAD_BOOL(show_avg_ping);
    LOAD_UINT(red_threshold);
    LOAD_UINT(font_size);
    switch (font_size) {
        case static_cast<int>(GuiUtils::FontSize::widget_label):
        case static_cast<int>(GuiUtils::FontSize::widget_small):
        case static_cast<int>(GuiUtils::FontSize::widget_large):
            break;
        default:
            font_size = static_cast<int>(GuiUtils::FontSize::widget_small);
            break;
    }
}

void LatencyWidget::SaveSettings(ToolboxIni* ini)
{
    ToolboxWidget::SaveSettings(ini);
    SAVE_UINT(red_threshold);
    SAVE_BOOL(show_avg_ping);
    SAVE_UINT(font_size);
}

void LatencyWidget::DrawSettingsInternal()
{
    ImGui::SliderInt("Red ping threshold", &red_threshold, 0, 1000);
    ImGui::Checkbox("Show average ping", &show_avg_ping);
    ImGui::Text("Font Size");
    ImGui::Indent();
    if (ImGui::RadioButton("Small", font_size == static_cast<int>(GuiUtils::FontSize::widget_label))) {
        font_size = static_cast<int>(GuiUtils::FontSize::widget_label);
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Medium", font_size == static_cast<int>(GuiUtils::FontSize::widget_small))) {
        font_size = static_cast<int>(GuiUtils::FontSize::widget_small);
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Large", font_size == static_cast<int>(GuiUtils::FontSize::widget_large))) {
        font_size = static_cast<int>(GuiUtils::FontSize::widget_large);
    }
    ImGui::Unindent();
}

ImColor LatencyWidget::GetColorForPing(const uint32_t ping)
{
    const LatencyWidget& instance = Instance();
    const float x = ping / static_cast<float>(instance.red_threshold);
    const auto myColor = ImColor(2.0f * x, 2.0f * (1 - x), 0.0f);
    return myColor;
}

void LatencyWidget::SendPing(const wchar_t*, int, LPWSTR*)
{
    LatencyWidget& instance = Instance();
    char buffer[48];
    snprintf(buffer, sizeof(buffer), "Current Ping: %ums, Avg Ping: %ums", instance.GetPing(), instance.GetAveragePing());
    GW::Chat::SendChat('#', buffer);
}
