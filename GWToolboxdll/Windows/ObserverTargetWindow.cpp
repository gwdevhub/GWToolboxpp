#include "stdafx.h"

#include <GWCA/Managers/AgentMgr.h>

#include <Modules/ObserverModule.h>
#include <Windows/ObserverTargetWindow.h>

void ObserverTargetWindow::Prepare()
{
    // 如果未激活则不变更状态
    if (!ObserverModule::Instance().IsActive()) {
        return;
    }

    // keep tracking up-to-date with the current desired target
    const GW::Agent* tracking_agent = GW::Agents::GetTarget();
    const GW::AgentLiving* tracking_living = nullptr;
    if (tracking_agent) {
        tracking_living = tracking_agent->GetAsAgentLiving();
    }

    const GW::Agent* compare_agent = GW::Agents::GetObservingAgent();
    const GW::AgentLiving* compare_living = nullptr;
    if (compare_agent) {
        compare_living = compare_agent->GetAsAgentLiving();
    }

    const uint32_t next_compare_id = compare_living ? compare_living->agent_id : previously_compared_agent_id;
    const uint32_t next_tracked_id = tracking_living ? tracking_living->agent_id : previously_tracked_agent_id;

    // 追踪和比较同一成员由玩家窗口处理，而非目标窗口
    if (next_compare_id == next_tracked_id) {
        previously_compared_agent_id = NO_AGENT;
        previously_tracked_agent_id = NO_AGENT;
        return;
    }

    previously_compared_agent_id = next_compare_id;
    previously_tracked_agent_id = next_tracked_id;
}

uint32_t ObserverTargetWindow::GetTracking()
{
    return previously_tracked_agent_id;
}

uint32_t ObserverTargetWindow::GetComparison()
{
    return previously_compared_agent_id;
}
