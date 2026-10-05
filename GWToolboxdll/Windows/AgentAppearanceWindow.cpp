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
        AgentRenderer::DrawRuleEditor();
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(650.f, 650.f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(650.f, 350.f), ImVec2(FLT_MAX, FLT_MAX));
    if (pending_focus) {
        ImGui::SetNextWindowFocus();
        ImGui::SetNextWindowCollapsed(false);
        pending_focus = false;
    }
    if (ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {
        AgentRenderer::DrawSettings();
    }
    ImGui::End();
    AgentRenderer::DrawRuleEditor();
}
