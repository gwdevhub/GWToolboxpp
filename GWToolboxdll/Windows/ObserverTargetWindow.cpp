#include "stdafx.h"

#include <GWCA/GameEntities/Agent.h>

#include <GWCA/Managers/AgentMgr.h>

#include <Modules/ObserverModule.h>
#include <Windows/ObserverTargetWindow.h>

#define NO_AGENT 0

void ObserverTargetWindow::Prepare() {
    // do not change the state if not active
    if (!ObserverModule::Instance().IsActive()) return;

    // prepare the current tracking and compared agents
    // keep tracking up-to-date with the current desired target
    const GW::Agent* tracking_agent = GW::Agents::GetTarget();
    const GW::AgentLiving* tracking_living = nullptr;
    if (tracking_agent) tracking_living = tracking_agent->GetAsAgentLiving();

    const GW::Agent* compare_agent = GW::Agents::GetPlayer();
    const GW::AgentLiving* compare_living = nullptr;
    if (compare_agent) compare_living = compare_agent->GetAsAgentLiving();

    uint32_t next_compare_id = compare_living ? compare_living->agent_id : previously_compared_agent_id;
    uint32_t next_tracked_id = tracking_living ? tracking_living->agent_id : previously_tracked_agent_id;

    // tracking & comparing the same agent is left up to the Player window, not the Target window
    if (next_compare_id == next_tracked_id) {
        previously_compared_agent_id = NO_AGENT;
        previously_tracked_agent_id = NO_AGENT;
        return;
    }

    previously_compared_agent_id = next_compare_id;
    previously_tracked_agent_id = next_tracked_id;
}

// Get the agent we're currently tracking
uint32_t ObserverTargetWindow::GetTracking() {
    return previously_tracked_agent_id;
}

// Get the agent we're comparing to
uint32_t ObserverTargetWindow::GetComparison() {
    return previously_compared_agent_id;
}

