#include "stdafx.h"

#include <GWCA/Constants/Constants.h>

#include <GWCA/GameEntities/Party.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Player.h>


#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/AgentMgr.h>

#include "SnapsToPartyWindow.h"
#include <GWCA/Managers/PlayerMgr.h>
#include <GWCA/Context/WorldContext.h>

#include <GWCA/Utilities/Hooker.h>

namespace {

    GW::UI::Frame* GetPartyWindowHealthBars(GW::UI::Frame** party_frame_out = nullptr) {
        const auto party = GW::PartyMgr::GetPartyInfo();
        // @Cleanup: Fetch party frame once, only update when it has been destroyed
        const auto party_frame = party ? GW::UI::GetFrameByLabel(L"Party") : nullptr;
        if (party_frame_out) *party_frame_out = party_frame;
        if (!(party_frame && party_frame->IsVisible())) return nullptr;

        // Traverse to health bars
        if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost) {
            auto sub_frame = GW::UI::GetChildFrame(party_frame, 1);
            sub_frame = GW::UI::GetChildFrame(sub_frame, 8);
            sub_frame = GW::UI::GetChildFrame(sub_frame, 0);
            sub_frame = GW::UI::GetChildFrame(sub_frame, 0);
            return GW::UI::GetChildFrame(sub_frame, 0);
        }
        auto sub_frame = GW::UI::GetChildFrame(party_frame, 0);
        sub_frame = GW::UI::GetChildFrame(sub_frame, 0);
        return GW::UI::GetChildFrame(sub_frame, 0);
    }

    bool GetFramePosition(const GW::UI::Frame* frame, const GW::UI::Frame* relative_to, ImVec2* top_left, ImVec2* bottom_right) {
        if (!(frame && relative_to && frame->IsVisible()))
            return false;
        if (!GImGui)
            return false;
        // Imgui viewport may not be limited to the game area.
        const auto imgui_viewport = ImGui::GetMainViewport();
        if (top_left) {
            *top_left = frame->position.GetTopLeftOnScreen(relative_to);
            top_left->x = std::round(top_left->x);
            top_left->y = std::round(top_left->y);
            top_left->x += imgui_viewport->Pos.x;
            top_left->y += imgui_viewport->Pos.y;
        }
        if (bottom_right) {
            *bottom_right = frame->position.GetBottomRightOnScreen(relative_to);
            bottom_right->x = std::round(bottom_right->x);
            bottom_right->y = std::round(bottom_right->y);
            bottom_right->x += imgui_viewport->Pos.x;
            bottom_right->y += imgui_viewport->Pos.y;
        }
        return true;
    }
    GW::UI::Frame* party_window_health_bars = nullptr;
    GW::UI::UIInteractionCallback OnPartyWindowHealthBars_UICallback_Ret = 0, OnPartyWindowHealthBars_UICallback_Func = 0;
    void __cdecl OnPartyWindowHealthBars_UICallback(GW::UI::InteractionMessage* message, void* wParam, void* lParam) {
        GW::Hook::EnterHook();
        switch (message->message_id) {
        case GW::UI::UIMessage::kDestroyFrame:
        case GW::UI::UIMessage::kFrameMessage_0x15:
        case GW::UI::UIMessage::kSetLayout:
            party_window_health_bars = nullptr; // Forces a recalculation
            break;
        }
        OnPartyWindowHealthBars_UICallback_Ret(message, wParam, lParam);
        GW::Hook::LeaveHook();
    }
    GW::HookEntry OnUIMessage_HookEntry;
    void OnUIMessage(GW::HookStatus*, GW::UI::UIMessage, void*, void*) {
        party_window_health_bars = nullptr;
    }

    struct PendingPartyMember {
        uint32_t agent_id;
        const wchar_t* enc_name;
    };
    std::vector<PendingPartyMember> pending_party_members;
}

std::unordered_map<uint32_t, SnapsToPartyWindow::PartyFramePosition> SnapsToPartyWindow::agent_health_bar_positions;
SnapsToPartyWindow::PartyFramePosition SnapsToPartyWindow::party_health_bars_position;
uint32_t SnapsToPartyWindow::henchmen_start_idx = 0xff;
uint32_t SnapsToPartyWindow::pets_start_idx = 0xff;
uint32_t SnapsToPartyWindow::allies_start_idx = 0xff;
std::unordered_map<uint32_t, uint32_t> SnapsToPartyWindow::party_indeces_by_agent_id;
std::vector<uint32_t> SnapsToPartyWindow::party_agent_ids_by_index;
std::vector<std::unique_ptr<GuiUtils::EncString>> SnapsToPartyWindow::party_names_by_index;

SnapsToPartyWindow::PartyFramePosition* SnapsToPartyWindow::GetAgentHealthBarPosition(uint32_t agent_id) {
    const auto found = agent_health_bar_positions.find(agent_id);
    if (agent_id == 0 || found == agent_health_bar_positions.end())
        return nullptr;
    return &found->second;
}

bool SnapsToPartyWindow::FetchPartyInfo()
{
    const GW::PartyInfo* info = GW::PartyMgr::GetPartyInfo();
    if (!info) {
        pending_party_members.clear();
        party_indeces_by_agent_id.clear();
        party_agent_ids_by_index.clear();
        return false;
    }

    pending_party_members.clear();
    auto consider_agent = [&](const uint32_t agent_id, const wchar_t* enc_name = nullptr) {
        if (std::find_if(pending_party_members.begin(), pending_party_members.end(),
                [agent_id](const PendingPartyMember& m) { return m.agent_id == agent_id; }) != pending_party_members.end())
            return;
        pending_party_members.push_back({agent_id, enc_name});
        };

    for (const auto& player : info->players) {
        if (const auto gwplayer = GW::PlayerMgr::GetPlayerByID(player.login_number)) {
            consider_agent(gwplayer->agent_id, gwplayer->name_enc);
        }
        for (const auto& hero : info->heroes) {
            if (hero.owner_player_id == player.login_number) {
                consider_agent(hero.agent_id);
            }
        }
    }

    const auto henchmen_count = pending_party_members.size();
    for (const auto& hench : info->henchmen) {
        consider_agent(hench.agent_id);
    }

    const auto pets_count = pending_party_members.size();
    const auto w = GW::GetWorldContext();
    if (w) {
        for (const auto& pet : w->pets) {
            consider_agent(pet.agent_id);
        }
    }

    const auto allies_count = pending_party_members.size();
    for (const auto& other_agent_id : info->others) {
        consider_agent(other_agent_id);
    }

    const auto unchanged = party_names_by_index.size() >= pending_party_members.size()
        && pending_party_members.size() == party_agent_ids_by_index.size()
        && std::equal(pending_party_members.begin(), pending_party_members.end(), party_agent_ids_by_index.begin(),
            [](const PendingPartyMember& m, const uint32_t agent_id) { return m.agent_id == agent_id; });
    if (unchanged)
        return true;

    party_indeces_by_agent_id.clear();
    party_agent_ids_by_index.clear();
    for (const auto& str : party_names_by_index) {
        if (str->IsDecoding())
            return false;
    }

    henchmen_start_idx = henchmen_count;
    pets_start_idx = pets_count;
    allies_start_idx = allies_count;
    for (const auto& member : pending_party_members) {
        party_indeces_by_agent_id[member.agent_id] = party_agent_ids_by_index.size();
        party_agent_ids_by_index.push_back(member.agent_id);
        while (party_names_by_index.size() < party_agent_ids_by_index.size()) {
            party_names_by_index.push_back(std::make_unique<GuiUtils::EncString>());
        }
        auto* str = party_names_by_index[party_agent_ids_by_index.size() - 1].get();
        str->reset(member.enc_name ? member.enc_name : GW::Agents::GetAgentEncName(member.agent_id))
            ->wstring();
    }
    return true;
}

GW::UI::Frame* SnapsToPartyWindow::GetPartyWindowHealthBars(GW::UI::Frame** party_frame_out) {
    return ::GetPartyWindowHealthBars(party_frame_out);
}
void SnapsToPartyWindow::Initialize()
{
    ToolboxWidget::Initialize();
    is_movable = is_resizable = false;
    RegisterUIMessageCallback(&OnUIMessage_HookEntry, GW::UI::UIMessage::kPreferenceValueChanged, OnUIMessage, 0x8000);
}

void SnapsToPartyWindow::Terminate()
{
    ToolboxWidget::Terminate();
    // NB: Don't remove the ui callback! Other modules that extend this class will use it. Toolbox will remove all hooks at the end
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
    RecalculatePartyPositions();
}

bool SnapsToPartyWindow::RecalculatePartyPositions() {
    if (party_window_health_bars)
        return true;
    agent_health_bar_positions.clear();
    ImVec2 top_left;
    ImVec2 bottom_right;
    const auto party = GW::PartyMgr::GetPartyInfo();
    GW::UI::Frame* party_frame = 0;
    party_window_health_bars = GetPartyWindowHealthBars(&party_frame);

    if (!party_window_health_bars)
        return false;
    ASSERT(party_window_health_bars->frame_callbacks.size());
    if (!OnPartyWindowHealthBars_UICallback_Func) {
        OnPartyWindowHealthBars_UICallback_Func = party_window_health_bars->frame_callbacks[0].callback;
        GW::Hook::CreateHook((void**)&OnPartyWindowHealthBars_UICallback_Func, OnPartyWindowHealthBars_UICallback, reinterpret_cast<void**>(&OnPartyWindowHealthBars_UICallback_Ret));
        GW::Hook::EnableHooks(OnPartyWindowHealthBars_UICallback_Func);
    }

    const auto player_health_bars = GW::UI::GetChildFrame(party_window_health_bars, 0);
    if (!player_health_bars) // Child frames by player id (includes heroes)
        return false;

    const auto relative_to = party_frame;
    GetFramePosition(party_window_health_bars->relation.GetParent(), relative_to, &party_health_bars_position.top_left, &party_health_bars_position.bottom_right);

    // Add some padding left and right
    GetFramePosition(party_frame, relative_to, &top_left, &bottom_right);
    const auto diff = (party_health_bars_position.top_left.x - top_left.x) / 2.f;
    party_health_bars_position.top_left.x -= diff;
    party_health_bars_position.bottom_right.x += diff;

    GW::UI::Frame* agent_health_bar = nullptr;

    for (auto& player : party->players) {
        const auto player_container = GW::UI::GetChildFrame(player_health_bars, player.login_number);
        agent_health_bar = GW::UI::GetChildFrame(player_container, 0);
        if (!agent_health_bar)
            continue;
        const auto agent_id = GW::PlayerMgr::GetPlayerAgentId(player.login_number);
        if (!agent_id)
            continue;
        GetFramePosition(agent_health_bar, relative_to, &top_left, &bottom_right);
        agent_health_bar_positions[agent_id] = { top_left, bottom_right };
        for (auto& hero : party->heroes) {
            if (hero.owner_player_id != player.login_number)
                continue;
            agent_health_bar = GW::UI::GetChildFrame(player_container, 5 + hero.agent_id);
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
        agent_health_bar = GW::UI::GetChildFrame(henchmen_health_bars, henchman.agent_id);
        if (!agent_health_bar)
            continue;
        GetFramePosition(agent_health_bar, relative_to, &top_left, &bottom_right);
        agent_health_bar_positions[henchman.agent_id] = { top_left, bottom_right };
    }
    auto pet_health_bars = GW::UI::GetChildFrame(party_window_health_bars, 3); // Find matching agent frames frames by agent_id
    if (!(pet_health_bars && pet_health_bars->IsVisible()))
        pet_health_bars = nullptr;

    auto allies_health_bars = GW::UI::GetChildFrame(party_window_health_bars, 4); // Find matching agent frames frames by agent_id
    if (!(allies_health_bars && allies_health_bars->IsVisible()))
        allies_health_bars = nullptr;

    for (auto& agent_id : party->others) {
        agent_health_bar = GW::UI::GetChildFrame(pet_health_bars, agent_id);
        if(!agent_health_bar)
            agent_health_bar = GW::UI::GetChildFrame(allies_health_bars, agent_id);
        if (!agent_health_bar)
            continue;
        GetFramePosition(agent_health_bar, relative_to, &top_left, &bottom_right);
        agent_health_bar_positions[agent_id] = { top_left, bottom_right };
    }
    return true;
}
