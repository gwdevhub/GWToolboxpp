#include "stdafx.h"

#include <Widgets/Minimap/AgentRenderer.h>
#include <Windows/AgentAppearanceWindow.h>

void AgentAppearanceWindow::Show()
{
    visible = true;
    pending_focus = true;
}

void AgentAppearanceWindow::Draw(IDirect3DDevice9*)
{
    if (!visible) {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(650.f, 650.f), ImGuiCond_FirstUseEver);
    if (pending_focus) {
        ImGui::SetNextWindowFocus();
        ImGui::SetNextWindowCollapsed(false);
        pending_focus = false;
    }
    if (ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {
        auto& renderer = AgentRenderer::Instance();
        renderer.DrawSettings();
        ImGui::SliderFloat("Agent Border thickness", &renderer.agent_border_thickness, 0.f, 100.f, "%.0f");
        ImGui::SliderFloat("Target Border thickness", &renderer.target_border_thickness, 0.f, 100.f, "%.0f");
    }
    ImGui::End();
}
