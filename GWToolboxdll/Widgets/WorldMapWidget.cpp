#include "stdafx.h"

#include <GWCA/Utilities/MemoryPatcher.h>
#include <GWCA/Utilities/Scanner.h>

#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>

#include <Widgets/WorldMapWidget.h>

#include <Timer.h>

namespace {
    ImRect show_all_rect;
    ImRect hard_mode_rect;

    bool showing_all_outposts = false;

    bool drawn = false;

    GW::MemoryPatcher view_all_outposts_patch;
    GW::MemoryPatcher view_all_carto_areas_patch;

    uint32_t __cdecl GetCartographyFlagsForArea(uint32_t , uint32_t , uint32_t , uint32_t ) {
        return 0xffffffff;
    }

    clock_t show_map_at = 0;
}

void WorldMapWidget::Initialize()
{
    ToolboxWidget::Initialize();

    uintptr_t address = GW::Scanner::Find("\x8b\x45\xfc\xf7\x40\x10\x00\x00\x01\x00", "xxxxxxxxxx", 0xa);
    if (address) {
        view_all_outposts_patch.SetPatch(address, "\xeb", 1);
    }
    address = GW::Scanner::Find("\x8b\xd8\x83\xc4\x10\x8b\xcb\x8b\xf3\xd1\xe9","xxxxxxxxxxx",-0x5);
    if (address) {
        view_all_carto_areas_patch.SetRedirect(address, GetCartographyFlagsForArea);
    }

    ASSERT(view_all_outposts_patch.IsValid());
    ASSERT(view_all_carto_areas_patch.IsValid());
}
void WorldMapWidget::Terminate() {
    ToolboxWidget::Terminate();
    view_all_outposts_patch.Reset();
    view_all_carto_areas_patch.Reset();
}

void WorldMapWidget::ShowAllOutposts(const bool show = showing_all_outposts)
{
    if(view_all_outposts_patch.IsValid())
        view_all_outposts_patch.TogglePatch(show);
    if (view_all_carto_areas_patch.IsValid())
        view_all_carto_areas_patch.TogglePatch(show);
    // @Cleanup: Instead of using 500ms clock timer, figure out how to refresh cartography areas properly
    GW::GameThread::Enqueue([]() {
        if (GW::UI::GetIsWorldMapShowing()) {
            GW::UI::Keypress(GW::UI::ControlAction_OpenWorldMap);
            show_map_at = TIMER_INIT() + 500;
        }
        });
}

void WorldMapWidget::Draw(IDirect3DDevice9*)
{
    if (show_map_at && TIMER_INIT() > show_map_at) {
        GW::UI::Keypress(GW::UI::ControlAction_OpenWorldMap);
        show_map_at = 0;
    }
    if (!GW::UI::GetIsWorldMapShowing()) {
        //ShowAllOutposts(showing_all_outposts = false);
        drawn = false;
        return;
    }

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    ImGui::SetNextWindowPos(ImVec2(16.f, 16.f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(Name(), nullptr, GetWinFlags() | ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Checkbox("Show all areas", &showing_all_outposts);
        show_all_rect = ImGui::GetCurrentContext()->LastItemData.Rect;
        bool is_hard_mode = GW::PartyMgr::GetIsPartyInHardMode();
        ImGui::Checkbox("Hard mode", &is_hard_mode);
        hard_mode_rect = ImGui::GetCurrentContext()->LastItemData.Rect;
    }
    ImGui::End();
    ImGui::PopStyleColor();
    drawn = true;
}

bool WorldMapWidget::WndProc(const UINT Message, WPARAM, LPARAM lParam)
{
    switch (Message) {
        case WM_LBUTTONDOWN:
            if (!drawn || !GW::UI::GetIsWorldMapShowing()) {
                return false;
            }
            auto check_rect = [lParam](const ImRect& rect) {
                const auto x = GET_X_LPARAM(lParam);
                const auto y = GET_Y_LPARAM(lParam);
                return x > rect.Min.x && x < rect.Max.x && y > rect.Min.y && y < rect.Max.y;
            };
            if (check_rect(show_all_rect)) {
                showing_all_outposts = !showing_all_outposts;
                ShowAllOutposts(showing_all_outposts);
                return true;
            }
            if (check_rect(hard_mode_rect)) {
                GW::PartyMgr::SetHardMode(!GW::PartyMgr::GetIsPartyInHardMode());
                return true;
            }
            break;
    }
    return false;
}

void WorldMapWidget::DrawSettingsInternal()
{
    ImGui::Text("Note: only visible in Hard Mode explorable areas.");
}
