#include "stdafx.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/Packets/Opcodes.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/StoCMgr.h>

#include <Utils/GuiUtils.h>
#include <Defines.h>

#include <Widgets/LatencyWidget.h>

#include "Utils/FontLoader.h"
#include <GWCA/Managers/EventMgr.h>

namespace {
    GW::HookEntry ChatCmd_HookEntry;
    constexpr size_t ping_history_len = 10; // GW checks last 10 pings for avg
    uint32_t ping_history[ping_history_len] = {0};
    size_t ping_index = 0;

    GW::HookEntry Ping_Entry;
    int red_threshold = 250;
    bool show_avg_ping = false;
    int font_size = 0;
    float text_size = 40.f;

    GW::HookEntry ChatCommand_Hook;

    void OnServerPing(GW::HookStatus*, GW::EventMgr::EventID, void* packet, uint32_t)
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

    void CHAT_CMD_FUNC(CmdPing)
    {
        LatencyWidget::SendPing();
    }
}

void LatencyWidget::SendPing() {
    char buffer[48];
    snprintf(buffer, sizeof(buffer), "Current Ping: %ums, Avg Ping: %ums", LatencyWidget::GetPing(), LatencyWidget::GetAveragePing());
    GW::Chat::SendChat('#', buffer);
}
void LatencyWidget::Initialize()
{
    ToolboxWidget::Initialize();
    GW::EventMgr::RegisterEventCallback(&Ping_Entry, GW::EventMgr::EventID::kRecvPing, OnServerPing, 0x800);
    GW::Chat::CreateCommand(&ChatCmd_HookEntry, L"ping", CmdPing);
}
void LatencyWidget::Terminate() {
    ToolboxWidget::Terminate();
    GW::Chat::DeleteCommand(&ChatCmd_HookEntry);
    GW::EventMgr::RemoveEventCallback(&Ping_Entry);
}

void LatencyWidget::Update(const float) { }

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
    const bool ctrl_pressed = ImGui::IsKeyDown(ImGuiMod_Ctrl);

    if (ImGui::Begin(Name(), nullptr, GetWinFlags(0, !ctrl_pressed))) {
        const ImVec2 cur = ImGui::GetCursorPos();
        ImGui::PushFont(FontLoader::GetFontByPx(text_size), text_size);
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
    LOAD_FLOAT(text_size);
    // Legacy font size
    LOAD_UINT(font_size);
    switch (font_size) {
    case static_cast<int>(FontLoader::FontSize::widget_label):
    case static_cast<int>(FontLoader::FontSize::widget_small):
    case static_cast<int>(FontLoader::FontSize::widget_large):
        text_size = static_cast<float>(font_size);
        break;
    }
}

void LatencyWidget::SaveSettings(ToolboxIni* ini)
{
    ToolboxWidget::SaveSettings(ini);
    SAVE_UINT(red_threshold);
    SAVE_BOOL(show_avg_ping);
    SAVE_FLOAT(text_size);
}

void LatencyWidget::DrawSettingsInternal()
{
    ImGui::SliderInt("Red ping threshold", &red_threshold, 0, 1000);
    ImGui::Checkbox("Show average ping", &show_avg_ping);
    ImGui::DragFloat("Text size", &text_size, 1.f, FontLoader::text_size_min, FontLoader::text_size_max, "%.f");
}

ImColor LatencyWidget::GetColorForPing(const uint32_t ping)
{
    const float x = ping / static_cast<float>(red_threshold);
    const auto myColor = ImColor(2.0f * x, 2.0f * (1 - x), 0.0f);
    return myColor;
}
