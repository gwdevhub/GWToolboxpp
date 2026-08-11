#include "stdafx.h"

#include <GWCA/Constants/Constants.h>

#include <GWCA/Packets/StoC.h>

#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/MapMgr.h>

#include <Windows/DoorMonitorWindow.h>
#include <ImGuiAddons.h>

void DoorMonitorWindow::Draw(IDirect3DDevice9*)
{
    if (!visible) {
        return;
    }
    ImGui::SetNextWindowCenter(ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(512, 256), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {
        return ImGui::End();
    }

    const float colWidth = 100.0f * ImGui::FontScale();
    float offset = 0.0f;

    ImGui::Text("门 ID");
    ImGui::SameLine(offset += colWidth);
    ImGui::Text("首次加载");
    ImGui::SameLine(offset += colWidth);
    ImGui::Text("首次开启");
    ImGui::SameLine(offset += colWidth);
    ImGui::Text("首次关闭");
    ImGui::SameLine(offset += colWidth);
    ImGui::Text("最后开启");
    ImGui::SameLine(offset += colWidth);
    ImGui::Text("最后关闭");
    ImGui::SameLine(offset += colWidth);
    ImGui::Text("当前状态");
    ImGui::Separator();

    for (auto it = doors.begin(); it != doors.end(); ++it) {
        offset = 0.0f;
        DoorObject& o = *it->second;
        ImGui::PushID(o.object_id);
        ImGui::Text("%d", o.object_id);
        char mbstr[100];
        std::strftime(mbstr, 100, "%H:%M:%S", std::localtime(&o.first_load));
        ImGui::SameLine(offset += colWidth);
        ImGui::Text("%s", mbstr);
        mbstr[0] = '-';
        mbstr[1] = 0;
        if (o.first_open) {
            std::strftime(mbstr, 100, "%H:%M:%S", std::localtime(&o.first_open));
        }
        ImGui::SameLine(offset += colWidth);
        ImGui::Text("%s", mbstr);
        mbstr[0] = '-';
        mbstr[1] = 0;
        if (o.first_close) {
            std::strftime(mbstr, 100, "%H:%M:%S", std::localtime(&o.first_close));
        }
        ImGui::SameLine(offset += colWidth);
        ImGui::Text("%s", mbstr);
        mbstr[0] = '-';
        mbstr[1] = 0;
        if (o.last_open) {
            std::strftime(mbstr, 100, "%H:%M:%S", std::localtime(&o.last_open));
        }
        ImGui::SameLine(offset += colWidth);
        ImGui::Text("%s", mbstr);
        mbstr[0] = '-';
        mbstr[1] = 0;
        if (o.last_close) {
            std::strftime(mbstr, 100, "%H:%M:%S", std::localtime(&o.last_close));
        }
        ImGui::SameLine(offset += colWidth);
        ImGui::Text("%s", mbstr);
        ImGui::SameLine(offset += colWidth);
        char name[128];
        snprintf(name, 128, "%s（%d）", o.is_open ? "开启" : "关闭", o.animation_type);
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

void DoorMonitorWindow::Initialize()
{
    ToolboxWindow::Initialize();
    // 首次加载时检查
    if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable) {
        doors.clear();
        in_zone = true;
    }

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::InstanceLoadInfo>(
        &InstanceLoadInfo_Callback,
        [this](const GW::HookStatus*, const GW::Packet::StoC::InstanceLoadInfo* packet) -> bool {
            if (!packet->is_explorable) {
                return in_zone = false, false;
            }
            doors.clear();
            return in_zone = true, false;
        });

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::ManipulateMapObject>(
        &ManipulateMapObject_Callback,
        [this](const GW::HookStatus*, const GW::Packet::StoC::ManipulateMapObject* packet) -> bool {
            if (!in_zone) {
                return false;
            }
            DoorObject::DoorAnimation(packet->object_id, packet->animation_type, packet->animation_stage);
            return false;
        });
}
