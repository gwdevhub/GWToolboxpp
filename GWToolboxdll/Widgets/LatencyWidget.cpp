#include "GWCA/Packets/Opcodes.h"
#include "stdafx.h"
#include <GWCA/Constants/Constants.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Packets/StoC.h>
#include <GuiUtils.h>
#include <Widgets/LatencyWidget.h>

void LatencyWidget::Initialize() {
    ToolboxWidget::Initialize();
    GW::StoC::RegisterPacketCallback(&Ping_Entry, GAME_SMSG_PING_REPLY, PingUpdate, 0x800);
    GW::Chat::CreateCommand(L"pingping", LatencyWidget::pingping);
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
    if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading) return;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    ImGui::SetNextWindowSize(ImVec2(180.0f, 60.0f), ImGuiCond_FirstUseEver);
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
            char buffer[32];
            snprintf(buffer, sizeof(buffer), "My ping is %u ms", GetPing());
            GW::Chat::SendChat('#', buffer);
        }
    }

    ImGui::End();
    ImGui::PopStyleColor();
}

void LatencyWidget::LoadSettings(CSimpleIni* ini) {
    ToolboxWidget::LoadSettings(ini);

    red_threshold = ini->GetLongValue(Name(), "red_threshold", 300);
}

void LatencyWidget::SaveSettings(CSimpleIni* ini) {
    ToolboxWidget::SaveSettings(ini);
    ini->SetLongValue(Name(), VAR_NAME(red_threshold), red_threshold);
}

void LatencyWidget::DrawSettingInternal() { ImGui::SliderInt("Red ping threshold", &red_threshold, 0, 500); }

ImColor LatencyWidget::GetColorForPing(uint32_t ping) {
    LatencyWidget& instance = Instance();
    float x = ping / (float)instance.red_threshold;
    ImColor myColor = ImColor(2.0f * x, 2.0f * (1 - x), 0.0f);
    return myColor;
}

void LatencyWidget::pingping(const wchar_t*, int argc, LPWSTR* argv) {
    UNREFERENCED_PARAMETER(argc);
    UNREFERENCED_PARAMETER(argv);
    LatencyWidget& instance = Instance();
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "My ping is %u ms", instance.GetPing());
    GW::Chat::SendChat('#', buffer);
}