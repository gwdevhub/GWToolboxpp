#include "stdafx.h"

#include <GWCA/Constants/Constants.h>

#include <GWCA/Packets/StoC.h>

#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/MapMgr.h>

#include <Windows/DoorMonitorWindow.h>
#include <ImGuiAddons.h>

void DoorMonitorWindow::Draw(IDirect3DDevice9* pDevice) {
    UNREFERENCED_PARAMETER(pDevice);
    if (!visible) return;
    ImGui::SetNextWindowCenter(ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(512, 256), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags()))
        return ImGui::End();

    const float colWidth = 100.0f * ImGui::GetIO().FontGlobalScale;
    float offset = 0.0f;

    ImGui::Text("Door ID");
    ImGui::SameLine(offset += colWidth); ImGui::Text("First Load");
    ImGui::SameLine(offset += colWidth); ImGui::Text("First Open");
    ImGui::SameLine(offset += colWidth); ImGui::Text("First Close");
    ImGui::SameLine(offset += colWidth); ImGui::Text("Last Open");
    ImGui::SameLine(offset += colWidth); ImGui::Text("Last Close");
    ImGui::SameLine(offset += colWidth); ImGui::Text("Current State");
    ImGui::Separator();
    std::map<uint32_t, DoorObject*>::iterator it;
    
    for (it = doors.begin(); it != doors.end(); it++) {
        offset = 0.0f;
        DoorObject& o = *it->second;
        ImGui::PushID(o.object_id);
        ImGui::Text("%d", o.object_id);
        char mbstr[100];
        std::strftime(mbstr, 100, "%H:%M:%S", std::localtime(&o.first_load));
        ImGui::SameLine(offset += colWidth); ImGui::Text("%s", mbstr);
        mbstr[0] = '-'; mbstr[1] = 0;
        if(o.first_open)
            std::strftime(mbstr, 100, "%H:%M:%S", std::localtime(&o.first_open));
        ImGui::SameLine(offset += colWidth); ImGui::Text("%s", mbstr);
        mbstr[0] = '-'; mbstr[1] = 0;
        if (o.first_close)
            std::strftime(mbstr, 100, "%H:%M:%S", std::localtime(&o.first_close));
        ImGui::SameLine(offset += colWidth); ImGui::Text("%s", mbstr);
        mbstr[0] = '-'; mbstr[1] = 0;
        if (o.last_open)
            std::strftime(mbstr, 100, "%H:%M:%S", std::localtime(&o.last_open));
        ImGui::SameLine(offset += colWidth); ImGui::Text("%s", mbstr);
        mbstr[0] = '-'; mbstr[1] = 0;
        if (o.last_close)
            std::strftime(mbstr, 100, "%H:%M:%S", std::localtime(&o.last_close));
        ImGui::SameLine(offset += colWidth); ImGui::Text("%s", mbstr);
        ImGui::SameLine(offset += colWidth); 
        char name[128];
        snprintf(name,128,"%s (%d)", o.is_open ? "Open" : "Closed", o.animation_type);
        if (ImGui::Button(name)) {
            GW::Packet::StoC::ManipulateMapObject packet;
            packet.header = GW::Packet::StoC::ManipulateMapObject::STATIC_HEADER;
            packet.animation_stage = 3;
            switch (o.animation_type) {
            case 3:
                packet.animation_type = 9;
                break;
            case 9:
                packet.animation_type = 16;
                break;
            case 16:
                packet.animation_type = 3;
                break;
            }
            packet.object_id = o.object_id;
            GW::StoC::EmulatePacket(&packet);
            o.animation_type = packet.animation_type;
        }
        ImGui::PopID();
    }
    ImGui::End();

}

void DoorMonitorWindow::Initialize() {
    ToolboxWindow::Initialize();
    // Check on first load
    if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable) {
        doors.clear();
        in_zone = true;
    }

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::InstanceLoadInfo>(&InstanceLoadInfo_Callback, 
        [this](GW::HookStatus* status, GW::Packet::StoC::InstanceLoadInfo* packet) -> bool {
            UNREFERENCED_PARAMETER(status);
            if (!packet->is_explorable)
                return in_zone = false, false;
            doors.clear();
            return in_zone = true, false;
        });

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::ManipulateMapObject>(&ManipulateMapObject_Callback,
        [this](GW::HookStatus* status, GW::Packet::StoC::ManipulateMapObject* packet) -> bool {
            UNREFERENCED_PARAMETER(status);
            if (!in_zone)
                return false;
            DoorObject::DoorAnimation(packet->object_id, packet->animation_type, packet->animation_stage);
            return false;
        });
}
