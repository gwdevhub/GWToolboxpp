#include "stdafx.h"

#include <GWCA/Constants/Constants.h>

#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/WorldContext.h>

#include <GWCA/GameEntities/Map.h>

#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/StoCMgr.h> 
#include <GWCA/Managers/PartyMgr.h> 

#include <GWCA/Packets/StoC.h>

#include <GuiUtils.h>
#include <Widgets/WorldMapWidget.h>
namespace {

    GW::Packet::StoC::MapsUnlocked all_maps_unlocked_packet;
    GW::Packet::StoC::MapsUnlocked actual_maps_unlocked_packet;

    bool actual_maps_unlocked_initialised = false;

    ImRect show_all_rect;
    ImRect hard_mode_rect;

    bool showing_all_outposts = false;

    bool drawn = false;

    bool IsHardModeUnlocked() {
        const GW::GameContext* g = GW::GameContext::instance();
        const GW::WorldContext* w = g ? g->world : nullptr;
        if (!w) return false;
        const GW::AreaInfo* map = GW::Map::GetCurrentMapInfo();
        if (!map) return false;
        auto ArrayBoolAt = [](const GW::Array<uint32_t>&array, uint32_t index) {
            uint32_t real_index = index / 32;
            if (!array.valid() || real_index >= array.size())
                return false;
            uint32_t shift = index % 32;
            uint32_t flag = 1 << shift;
            return (array[real_index] & flag) != 0;
        };
        uint32_t map_id = 0;
        switch (map->campaign) {
        case 0:
            map_id = static_cast<uint32_t>(GW::Constants::MapID::Hells_Precipice);
            break;
        case 1:
            map_id = static_cast<uint32_t>(GW::Constants::MapID::Imperial_Sanctum_outpost_mission);
            break;
        case 2:
            map_id = static_cast<uint32_t>(GW::Constants::MapID::Abaddons_Gate);
            break;
        case 3:
        case 4:
            map_id = static_cast<uint32_t>(GW::Constants::MapID::A_Time_for_Heroes_mission);
            break;
        }
        return map_id && ArrayBoolAt(w->missions_completed, map_id);
    }
}

WorldMapWidget::WorldMapWidget() {

}
WorldMapWidget::~WorldMapWidget() {
}
void WorldMapWidget::InitializeMapsUnlockedArrays() {
    const GW::GameContext* g = GW::GameContext::instance();
    if (!g)
        return;
    GW::WorldContext* w = g->world;
    if (!w)
        return;
    actual_maps_unlocked_packet.missions_bonus_length = 0;
    GW::Array<uint32_t>* arr = &w->missions_bonus;
    if (arr->valid()) {
        actual_maps_unlocked_packet.missions_bonus_length = arr->size();
        memcpy(&actual_maps_unlocked_packet.missions_bonus, arr->m_buffer, sizeof(actual_maps_unlocked_packet.missions_bonus));
    }
    arr = &w->missions_bonus_hm;
    actual_maps_unlocked_packet.missions_bonus_hm_length = 0;
    if (arr->valid()) {
        actual_maps_unlocked_packet.missions_bonus_hm_length = arr->size();
        memcpy(&actual_maps_unlocked_packet.missions_bonus_hm, arr->m_buffer, sizeof(actual_maps_unlocked_packet.missions_bonus_hm));
    }
    arr = &w->missions_completed;
    actual_maps_unlocked_packet.missions_completed_length = 0;
    if (arr->valid()) {
        actual_maps_unlocked_packet.missions_completed_length = arr->size();
        memcpy(&actual_maps_unlocked_packet.missions_completed, arr->m_buffer, sizeof(actual_maps_unlocked_packet.missions_completed));
    }
    arr = &w->missions_completed_hm;
    actual_maps_unlocked_packet.missions_completed_hm_length = 0;
    if (arr->valid()) {
        actual_maps_unlocked_packet.missions_completed_hm_length = arr->size();
        memcpy(&actual_maps_unlocked_packet.missions_completed_hm, arr->m_buffer, sizeof(actual_maps_unlocked_packet.missions_completed_hm));
    }
    arr = &w->unlocked_map;
    actual_maps_unlocked_packet.unlocked_map_length = 0;
    if (arr->valid()) {
        actual_maps_unlocked_packet.unlocked_map_length = arr->size();
        memcpy(&actual_maps_unlocked_packet.unlocked_map, arr->m_buffer, sizeof(actual_maps_unlocked_packet.unlocked_map));
    }
    all_maps_unlocked_packet = actual_maps_unlocked_packet;
    all_maps_unlocked_packet.unlocked_map_length = 32;
    memset(&all_maps_unlocked_packet.unlocked_map, 0xff, sizeof(all_maps_unlocked_packet.unlocked_map));
    actual_maps_unlocked_initialised = true;
}
void WorldMapWidget::Initialize() {
    ToolboxWidget::Initialize();
    InitializeMapsUnlockedArrays();

    static GW::HookEntry e;
    GW::StoC::RegisterPostPacketCallback<GW::Packet::StoC::MapsUnlocked>(&e, [](GW::HookStatus*, GW::Packet::StoC::MapsUnlocked* packet) {
        UNREFERENCED_PARAMETER(packet);
        Instance().InitializeMapsUnlockedArrays();
        });
}
void WorldMapWidget::ShowAllOutposts(bool show = showing_all_outposts) {
    static bool showing = false;
    //GW::WorldContext* world = GW::GameContext::instance()->world;
    if (showing == show)
        return;
    ASSERT(actual_maps_unlocked_initialised);
    if(show) {
        // Show all areas
        GW::StoC::EmulatePacket<GW::Packet::StoC::MapsUnlocked>(&all_maps_unlocked_packet);
    }
    else {
        // Restore area visibility
        GW::StoC::EmulatePacket<GW::Packet::StoC::MapsUnlocked>(&actual_maps_unlocked_packet);
    }
    showing = show;
}
void WorldMapWidget::Draw(IDirect3DDevice9 *pDevice) {
    UNREFERENCED_PARAMETER(pDevice);
    if (!GW::UI::GetIsWorldMapShowing()) {
        //ShowAllOutposts(showing_all_outposts = false);
        drawn = false;
        return;
    }
    if (!visible) {
        drawn = false;
        return;
    }

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    ImGui::SetNextWindowPos(ImVec2(16.f, 16.f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(Name(), nullptr, GetWinFlags() | ImGuiWindowFlags_AlwaysAutoResize)) {
        GW::WorldContext* world = GW::GameContext::instance()->world;
        UNREFERENCED_PARAMETER(world);
        ImGui::Checkbox("Show all areas", &showing_all_outposts);
        show_all_rect = ImGui::GetCurrentWindow()->DC.LastItemRect;
        if (IsHardModeUnlocked()) {
            bool is_hard_mode = GW::PartyMgr::GetIsPartyInHardMode();
            ImGui::Checkbox("Hard mode", &is_hard_mode);
            hard_mode_rect = ImGui::GetCurrentWindow()->DC.LastItemRect;
        }
    }
    ImGui::End();
    ImGui::PopStyleColor();
    drawn = true;
}
bool WorldMapWidget::WndProc(UINT Message, WPARAM, LPARAM lParam) {
    switch (Message) {
    case WM_LBUTTONDOWN:
        if (!drawn || !GW::UI::GetIsWorldMapShowing())
            return false;
        auto check_rect = [lParam](ImRect& rect) {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            return x > rect.Min.x && x < rect.Max.x && y > rect.Min.y && y < rect.Max.y;
        };
        if(check_rect(show_all_rect)) {
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

void WorldMapWidget::DrawSettingInternal() {
    ImGui::Text("Note: only visible in Hard Mode explorable areas.");
}
