#include "stdafx.h"

#include <GWCA/Constants/Constants.h>

#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/PartyMgr.h>

#include <Widgets/VanquishWidget.h>
#include <Widgets/VanquishMapOverlayWidget.h>

#include "Utils/FontLoader.h"

void VanquishWidget::Draw(IDirect3DDevice9*)
{
    if (!visible) {
        return;
    }

    const DWORD tokill = GW::Map::GetFoesToKill();
    const DWORD killed = GW::Map::GetFoesKilled();

    if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Explorable ||
        !GW::PartyMgr::GetIsPartyInHardMode() ||
        tokill <= 0) {
        return;
    }

    const bool ctrl_pressed = ImGui::IsKeyDown(ImGuiMod_Ctrl);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    ImGui::SetNextWindowSize(ImVec2(250.0f, 90.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(Name(), nullptr, GetWinFlags(0, !ctrl_pressed))) {
        static char foes_count[32] = "";
        snprintf(foes_count, 32, "%lu / %lu", killed, tokill + killed);

        ImGui::PushFont(FontLoader::GetFont(), static_cast<float>(FontLoader::FontSize::header1));
        ImVec2 cur = ImGui::GetCursorPos();
        ImGui::SetCursorPos(ImVec2(cur.x + 1, cur.y + 1));
        ImGui::TextColored(ImColor(0, 0, 0), "已征服");
        ImGui::SetCursorPos(cur);
        ImGui::Text("已征服");
        ImGui::PopFont();

        ImGui::PushFont(FontLoader::GetFont(), static_cast<float>(FontLoader::FontSize::widget_small));
        cur = ImGui::GetCursorPos();
        ImGui::SetCursorPos(ImVec2(cur.x + 2, cur.y + 2));
        ImGui::TextColored(ImColor(0, 0, 0), foes_count);
        ImGui::SetCursorPos(cur);
        ImGui::Text(foes_count);
        ImGui::PopFont();

        // 检查小部件是否被点击
        const ImVec2 size = ImGui::GetWindowSize();
        const ImVec2 min = ImGui::GetWindowPos();
        const ImVec2 max(min.x + size.x, min.y + size.y);
        if (ctrl_pressed && ImGui::IsMouseReleased(0) && ImGui::IsMouseHoveringRect(min, max)) {
            int alive = 0, stale = 0;
            VanquishMapOverlayWidget::GetTrackedEnemyCounts(alive, stale);
            const int located = alive + stale;
            char buffer[256];
            if (located > 0) {
                snprintf(buffer, sizeof(buffer),
                         "我们已征服 %lu 个敌人！还剩 %lu 个，已定位 %d 个。",
                         killed, tokill, located);
            } else {
                snprintf(buffer, sizeof(buffer),
                         "我们已征服 %lu 个敌人！还剩 %lu 个。",
                         killed, tokill);
            }
            GW::Chat::SendChat('#', buffer);
        }
    }
    ImGui::End();
    ImGui::PopStyleColor();
}

void VanquishWidget::DrawSettingsInternal()
{
    ImGui::Text("注意：仅在困难模式探索区域可见。");
}
