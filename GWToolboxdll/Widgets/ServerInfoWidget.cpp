#include "stdafx.h"

#include <GWCA/Packets/StoC.h>

#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/CharContext.h>

#include <GWCA/GameEntities/Title.h>

#include <GWCA/Managers/StoCMgr.h>

#include <Utils/GuiUtils.h>

#include <Modules/Resources.h>
#include <Widgets/ServerInfoWidget.h>

//#define IPGEO_API_KEY "144de1673b1c473d9bab94f528acc214"
#define IPGEO_API_KEY "161f3834252a4ec6988e49bb75ccd902"

namespace {
    static char server_ip[32];
    static char server_location[255];
    static bool server_string_dirty = false;
}

static int
sockaddr_sprint(char* s, size_t n, const sockaddr* host, bool inc_port = false)
{
    if (host->sa_family == AF_INET) {
        const sockaddr_in* in = (const sockaddr_in*)host;
        uint8_t* addr = (uint8_t*) & in->sin_addr.s_addr;
        if(inc_port)
            return snprintf(s, n, "%d.%d.%d.%d:%d", addr[0], addr[1], addr[2], addr[3], ntohs(in->sin_port));
        return snprintf(s, n, "%d.%d.%d.%d", addr[0], addr[1], addr[2], addr[3]);
    }
    else if (host->sa_family == AF_INET6) {
        const sockaddr_in6* in6 = (const sockaddr_in6*)host;
        uint8_t* addr = (uint8_t*) & in6->sin6_addr;
        return snprintf(s, n, "%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x",
            addr[0], addr[1], addr[2], addr[3],
            addr[4], addr[5], addr[6], addr[7],
            addr[8], addr[9], addr[10], addr[11],
            addr[12], addr[13], addr[14], addr[15]);
    }
    return 0;
}

void ServerInfoWidget::Initialize() {
    ToolboxWidget::Initialize();
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::InstanceLoadInfo>(
        &InstanceLoadInfo_HookEntry, [this](GW::HookStatus*, GW::Packet::StoC::InstanceLoadInfo*) {
            current_server_info = nullptr;
            server_ip[0] = 0;
            server_location[0] = 0;
        });
    GetServerInfo();
}
ServerInfoWidget::ServerInfo* ServerInfoWidget::GetServerInfo() {
    if (current_server_info)
        return current_server_info;
    //if (!GW::Map::GetIsMapLoaded())
    //  return nullptr;
    GW::GameContext* g = GW::GetGameContext();
    if (!g) return nullptr;
    GW::CharContext* c = g->character;
    if (!c) return nullptr;
    char current_ip[32] = { 0 };
    sockaddr_sprint(current_ip, 32, (const sockaddr*) &c->host, false);

    std::map<std::string, ServerInfo*>::iterator it = servers_by_ip.find(current_ip);
    if (it != servers_by_ip.end()) {
        current_server_info = it->second;
        return current_server_info;
    }
    current_server_info = new ServerInfo(current_ip);
    servers_by_ip.emplace(current_server_info->ip, current_server_info);
    return current_server_info;
}
void ServerInfoWidget::Update(float) {
    if (current_server_info && current_server_info->country.empty() && current_server_info->last_update < time(nullptr) - 60) {
        if (server_info_fetcher.joinable())
            server_info_fetcher.join(); // Wait for thread to end.
        current_server_info->last_update = time(nullptr);
        server_info_fetcher = std::thread([this]() {
            // Need to check details
            const std::string url = "https://api.ipgeolocation.io/ipgeo?apiKey=" IPGEO_API_KEY "&ip=" + current_server_info->ip;
            int tries = 0;
            std::string response;
            bool success = false;
            do {
                success = Resources::Instance().Download(url, response);
                tries++;
            } while (!success && tries < 5);
            if (!success) {
                Log::Log("Failed to download %s\n%s", url.c_str(), response.c_str());
                return;
            }
            if (!response.empty()) {
                using Json = nlohmann::json;
                Json json = Json::parse(response.c_str());
                if (current_server_info->city.empty() && json["city"].is_string())
                    current_server_info->city = json["city"];
                if (current_server_info->country.empty() && json["country_name"].is_string())
                    current_server_info->country = json["country_name"];
                server_string_dirty = true;
            }
            });
    }
}

void ServerInfoWidget::Draw(IDirect3DDevice9*) {
    if (!visible) return;
    if (!server_location && !server_ip) return;
    if (!current_server_info) {
        if (!GetServerInfo()) return;
        server_string_dirty = true;
    }
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    ImGui::SetNextWindowSize(ImVec2(200.0f, 90.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(Name(), nullptr, GetWinFlags(0, true))) {
        ImGui::End();
        ImGui::PopStyleColor();
        return;
    }
    if (server_string_dirty) {
        server_string_dirty = false;
        snprintf(server_location, sizeof(server_location) - 1, "%s, %s", current_server_info->city.c_str(), current_server_info->country.c_str());
        snprintf(server_ip, sizeof(server_ip) - 1, "%s", current_server_info->ip.c_str());
    }
    static ImVec2 cur;
    ImGui::PushFont(GuiUtils::GetFont(GuiUtils::FontSize::header1));
    cur = ImGui::GetCursorPos();
    ImGui::SetCursorPos(ImVec2(cur.x + 1, cur.y + 1));
    ImGui::TextColored(ImColor(0, 0, 0), server_ip);
    ImGui::SetCursorPos(cur);
    ImGui::Text(server_ip);
    ImGui::PopFont();

    if (server_location) {
        cur = ImGui::GetCursorPos();
        ImGui::SetCursorPos(ImVec2(cur.x + 1, cur.y + 1));
        ImGui::TextColored(ImColor(0, 0, 0), server_location);
        ImGui::SetCursorPos(cur);
        ImGui::Text(server_location);
    }

    ImGui::End();
    ImGui::PopStyleColor();
}

void ServerInfoWidget::DrawSettingInternal() {
    ImGui::Text("Displays current server IP Address and location if available");
}
void ServerInfoWidget::SaveSettings(ToolboxIni* ini) {
    ToolboxWidget::SaveSettings(ini);


}
void ServerInfoWidget::LoadSettings(ToolboxIni* ini) {
    ToolboxWidget::LoadSettings(ini);
}
