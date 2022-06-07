#include "GWCA/Packets/Opcodes.h"
#include "stdafx.h"
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Packets/StoC.h>
#include <GuiUtils.h>
#include <Widgets/LatencyWidget.h>

void LatencyWidget::Initialize() {
    ToolboxWidget::Initialize();
    GW::StoC::RegisterPacketCallback(&Ping_Entry, GAME_SMSG_PING_REPLY, PingUpdate, 0x800);
}

void LatencyWidget::Update(float delta) { UNREFERENCED_PARAMETER(delta); }

void LatencyWidget::PingUpdate(GW::HookStatus*, void* packet) {
    LatencyWidget& instance = Instance();
    uint32_t* packet_as_uint_array = (uint32_t*)packet;
    instance.ping_ms = packet_as_uint_array[1];
}

uint32_t LatencyWidget::GetPing() { return ping_ms; }

void LatencyWidget::Draw(IDirect3DDevice9* pDevice) {
    UNREFERENCED_PARAMETER(pDevice);
    if (!visible) return;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    ImGui::SetNextWindowSize(ImVec2(200.0f, 90.0f), ImGuiCond_FirstUseEver);
    bool ctrl_pressed = ImGui::IsKeyDown(VK_CONTROL);

    if (ImGui::Begin(Name(), nullptr, GetWinFlags(0, !ctrl_pressed))) {
        ImVec2 cur = ImGui::GetCursorPos();
        ImGui::PushFont(GuiUtils::GetFont(GuiUtils::FontSize::widget_large));
        ImGui::SetCursorPos(cur);
        ImGui::TextColored(GetColorForPing(GetPing()), "%u ms", GetPing());
        ImGui::PopFont();

        ImVec2 size = ImGui::GetWindowSize();
        ImVec2 min = ImGui::GetWindowPos();
        ImVec2 max(min.x + size.x, min.y + size.y);
        if (ctrl_pressed && ImGui::IsMouseReleased(0) && ImGui::IsMouseHoveringRect(min, max)) {
            char buffer[256];
            snprintf(buffer, sizeof(buffer), "My ping is %u ms", GetPing());
            GW::Chat::SendChat('#', buffer);
        }
    }

    ImGui::End();
    ImGui::PopStyleColor();
}

void LatencyWidget::LoadSettings(CSimpleIni* ini) {
    ToolboxWidget::LoadSettings(ini);

    red_threshold = (float)ini->GetDoubleValue(Name(), "red_threshold", 300);
}

void LatencyWidget::SaveSettings(CSimpleIni* ini) {
    ToolboxWidget::SaveSettings(ini);
    ini->SetDoubleValue(Name(), VAR_NAME(red_threshold), red_threshold);
}

void LatencyWidget::DrawSettingInternal() { ImGui::SliderInt("Red ping threshold", &red_threshold, 0, 1000); }

ImColor LatencyWidget::GetColorForPing(uint32_t ping) {
    LatencyWidget& instance = Instance();
    float x = ping / (float)instance.red_threshold;
    ImColor myColor = ImColor(2.0f * x, 2.0f * (1 - x), 0.0f);
    return myColor;
}