#include "HeartbeatPlugin.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/Managers/MapMgr.h>

#ifndef DBBOX_BUILD
DLLAPI ToolboxPlugin* ToolboxPluginInstance()
{
    static HeartbeatPlugin instance;
    return &instance;
}
#endif

void HeartbeatPlugin::Initialize(ImGuiContext* ctx, ImGuiAllocFns allocs, HMODULE toolbox_dll)
{
    // The plugin DLL must share Toolbox's ImGui context and allocators.
    ToolboxUIPlugin::Initialize(ctx, allocs, toolbox_dll);
}

void HeartbeatPlugin::Draw(IDirect3DDevice9*)
{
    if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Explorable || !GW::Map::GetIsMapLoaded()) {
        return;
    }

    const auto instance_time = GW::Map::GetInstanceTime();
    if (instance_time < 1'000u) {
        return;
    }

    const auto elapsed_in_cycle = (instance_time - 1'000u) % 3'000u;
    const auto remaining_ms = 3'000u - elapsed_in_cycle;
    const auto is_green = remaining_ms >= 1'500u && remaining_ms <= 2'800u;
    const auto timer_color = is_green
        ? ImVec4(0.f, 1.f, 0.f, 1.f)
        : ImVec4(1.f, 0.f, 0.f, 1.f);

    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.1f", remaining_ms / 1'000.f);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.f, 0.f, 0.f, 0.f));
    ImGui::SetNextWindowSize(ImVec2(100.0f, 50.0f), ImGuiCond_FirstUseEver);

    if (ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {
        ImGui::PushFont(nullptr, 20.f);
        const auto header_pos = ImGui::GetCursorPos();
        ImGui::SetCursorPos(ImVec2(header_pos.x + 1.f, header_pos.y + 1.f));
        ImGui::TextColored(ImVec4(1.f, 1.f, 1.f, 1.f), "Heartbeat");
        ImGui::PopFont();

        ImGui::PushFont(nullptr, 48.f);
        const auto timer_pos = ImGui::GetCursorPos();
        ImGui::SetCursorPos(ImVec2(timer_pos.x + 2.f, timer_pos.y + 2.f));
        ImGui::TextColored(ImVec4(0.f, 0.f, 0.f, 1.f), "%s", buffer);
        ImGui::SetCursorPos(timer_pos);
        ImGui::TextColored(timer_color, "%s", buffer);
        ImGui::PopFont();
    }
    ImGui::End();
    ImGui::PopStyleColor();
}
