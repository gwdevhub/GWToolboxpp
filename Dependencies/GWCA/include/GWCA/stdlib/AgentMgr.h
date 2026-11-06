#pragma once
#include <GWCA/stdlib/detail/Support.h>
#include <GWCA/Managers/AgentMgr.h>

namespace GW::Agents {
    using AgentEffectCallback = HookCallback<const AgentLiving*, const AgentEffect*>;
}
