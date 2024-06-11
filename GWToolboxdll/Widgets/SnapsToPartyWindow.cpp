#include "stdafx.h"

#include <GWCA/Constants/Constants.h>

#include <GWCA/GameEntities/Party.h>
#include <GWCA/GameEntities/Agent.h>

#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/AgentMgr.h>

#include "SnapsToPartyWindow.h"

namespace {

    bool GetFramePosition(GW::UI::Frame* frame, GW::UI::Frame* relative_to, ImVec2* top_left, ImVec2* bottom_right) {
        if (!(frame && relative_to))
            return false;
        const auto& rp = relative_to->position;
        const auto viewport_scale = rp.GetViewportScale();
        const auto& p = frame->position;

        if (top_left) {
            *top_left = {
                p.screen_left * viewport_scale.x,
                (rp.viewport_height - p.screen_top) * viewport_scale.y
            };
        }
        if (bottom_right) {
            *bottom_right = {
                p.screen_right * viewport_scale.x,
                (rp.viewport_height - p.screen_bottom) * viewport_scale.y
            };
        }
        return true;
    }

    

}

void SnapsToPartyWindow::Initialize()
{
    ToolboxWidget::Initialize();
    is_movable = is_resizable = false;
    // TODO: Attach hooks to trigger recalc of positions when party frame is updated
}

void SnapsToPartyWindow::Terminate()
{
    ToolboxWidget::Terminate();
    // TODO: Detach hooks to trigger recalc of positions when party frame is updated
}

ImGuiWindowFlags SnapsToPartyWindow::GetWinFlags(ImGuiWindowFlags flags, const bool noinput_if_frozen) const
{
    auto res = ToolboxWidget::GetWinFlags(flags, noinput_if_frozen);
    res |= ImGuiWindowFlags_NoResize;
    res |= ImGuiWindowFlags_NoMove;
    return res;
}

void SnapsToPartyWindow::Draw(IDirect3DDevice9* device)
{
    ToolboxWidget::Draw(device);
    // @Cleanup: don't do this every loop
    RecalculatePartyPositions();
}

bool SnapsToPartyWindow::RecalculatePartyPositions() {
    agent_health_bar_positions.clear();
    ImVec2 top_left;
    ImVec2 bottom_right;
    const auto party = GW::PartyMgr::GetPartyInfo();
    // @Cleanup: Fetch party frame once, only update when it has been destroyed
    const auto party_frame = party ? GW::UI::GetFrameByLabel(L"Party") : nullptr;
    if (!(party_frame && party_frame->IsVisible()))
        return false;

    GW::UI::Frame* party_window_health_bars = nullptr;

    // Traverse to health bars
    if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost) {
        auto sub_frame = GW::UI::GetChildFrame(party_frame, 1);
        sub_frame = GW::UI::GetChildFrame(sub_frame, 8);
        sub_frame = GW::UI::GetChildFrame(sub_frame, 0);
        sub_frame = GW::UI::GetChildFrame(sub_frame, 0);
        party_window_health_bars = GW::UI::GetChildFrame(sub_frame, 0);
    }
    else {
        auto sub_frame = GW::UI::GetChildFrame(party_frame, 0);
        sub_frame = GW::UI::GetChildFrame(sub_frame, 0);
        party_window_health_bars = GW::UI::GetChildFrame(sub_frame, 0);
    }

    const auto player_health_bars = GW::UI::GetChildFrame(party_window_health_bars, 0);
    if (!player_health_bars) // Child frames by player id (includes heroes)
        return false;

    const auto relative_to = party_frame;
    GetFramePosition(party_window_health_bars->relation.GetParent(), relative_to, &party_health_bars_position.first, &party_health_bars_position.second);

    // Add some padding left and right
    GetFramePosition(party_frame, relative_to, &top_left, &bottom_right);
    const auto diff = (party_health_bars_position.first.x - top_left.x) / 2.f;
    party_health_bars_position.first.x -= diff;
    party_health_bars_position.second.x += diff;

    for (auto& player : party->players) {
        const auto player_container = GW::UI::GetChildFrame(player_health_bars, player.login_number);
        const auto player_health_bar = GW::UI::GetChildFrame(player_container, 0);
        if (!player_health_bar)
            return false;
        const auto player_agent = GW::Agents::GetPlayerByID(player.login_number);
        if (!player_agent)
            return false;
        GetFramePosition(player_health_bar, relative_to, &top_left, &bottom_right);
        agent_health_bar_positions[player_agent->agent_id] = { top_left, bottom_right };
        for (auto& hero : party->heroes) {
            if (hero.owner_player_id != player.login_number)
                continue;
            const auto agent_health_bar = GW::UI::GetChildFrame(player_container, 4 + hero.agent_id);
            if (!agent_health_bar)
                continue;
            GetFramePosition(agent_health_bar, relative_to, &top_left, &bottom_right);
            agent_health_bar_positions[hero.agent_id] = { top_left, bottom_right };
        }
    }
    const auto henchmen_health_bars = GW::UI::GetChildFrame(party_window_health_bars, 1); // Find matching henchmen frames by agent_id
    for (auto& henchman : party->henchmen) {
        if (!henchmen_health_bars)
            return false;
        const auto agent_health_bar = GW::UI::GetChildFrame(henchmen_health_bars, henchman.agent_id);
        if (!agent_health_bar)
            continue;
        GetFramePosition(agent_health_bar, relative_to, &top_left, &bottom_right);
        agent_health_bar_positions[henchman.agent_id] = { top_left, bottom_right };
    }
    const auto other_health_bars = GW::UI::GetChildFrame(party_window_health_bars, 2); // Find matching agent frames frames by agent_id
    for (auto& agent_id : party->others) {
        if (!henchmen_health_bars)
            return false;
        const auto agent_health_bar = GW::UI::GetChildFrame(other_health_bars, agent_id);
        if (!agent_health_bar)
            continue;
        GetFramePosition(agent_health_bar, relative_to, &top_left, &bottom_right);
        agent_health_bar_positions[agent_id] = { top_left, bottom_right };
    }
    return true;
}
