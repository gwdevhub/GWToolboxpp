#include "stdafx.h"

#include <GWCA/Constants/Constants.h>

#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/PartyMgr.h>

#include <Utils/GuiUtils.h>
#include <Widgets/VanquishWidget.h>

void VanquishWidget::Draw(IDirect3DDevice9 *pDevice) {
    UNREFERENCED_PARAMETER(pDevice);
    if (!visible) return;

    DWORD tokill = GW::Map::GetFoesToKill();
    DWORD killed = GW::Map::GetFoesKilled();

    if ((GW::Map::GetInstanceType() != GW::Constants::InstanceType::Explorable) ||
        !GW::PartyMgr::GetIsPartyInHardMode() ||
        tokill <= 0) return;

    const bool ctrl_pressed = ImGui::IsKeyDown(ImGuiKey_ModCtrl);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    ImGui::SetNextWindowSize(ImVec2(250.0f, 90.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(Name(), nullptr, GetWinFlags(0, !ctrl_pressed))) {
        static char foes_count[32] = "";
        snprintf(foes_count, 32, "%lu / %lu", killed, tokill + killed);

        // vanquished
        ImGui::PushFont(GuiUtils::GetFont(GuiUtils::FontSize::header1));
        ImVec2 cur = ImGui::GetCursorPos();
        ImGui::SetCursorPos(ImVec2(cur.x + 1, cur.y + 1));
        ImGui::TextColored(ImColor(0, 0, 0), "Vanquished");
        ImGui::SetCursorPos(cur);
        ImGui::Text("Vanquished");
        ImGui::PopFont();

        // count
        ImGui::PushFont(GuiUtils::GetFont(GuiUtils::FontSize::widget_small));
        cur = ImGui::GetCursorPos();
        ImGui::SetCursorPos(ImVec2(cur.x + 2, cur.y + 2));
        ImGui::TextColored(ImColor(0, 0, 0), foes_count);
        ImGui::SetCursorPos(cur);
        ImGui::Text(foes_count);
        ImGui::PopFont();

        // Check if the widget was clicked
        ImVec2 size = ImGui::GetWindowSize();
        ImVec2 min = ImGui::GetWindowPos();
        ImVec2 max(min.x + size.x, min.y + size.y);
        if (ctrl_pressed && ImGui::IsMouseReleased(0) && ImGui::IsMouseHoveringRect(min, max)) {
            char buffer[256];
            snprintf(buffer, sizeof(buffer),
                "We have vanquished %lu %s! %lu %s remaining.",
                killed,
                killed == 1 ? "foe" : "foes",
                tokill,
                tokill == 1 ? "foe" : "foes");
            GW::Chat::SendChat('#', buffer);
        }
    }
    ImGui::End();
    ImGui::PopStyleColor();
}

void VanquishWidget::DrawSettingInternal() {
    ImGui::Text("Note: only visible in Hard Mode explorable areas.");
}
