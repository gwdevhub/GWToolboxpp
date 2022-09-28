#include "TargetOverride.h"

#include "GWCA/Managers/GameThreadMgr.h"

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/ChatMgr.h>

#include <Windows.h>

#include <format>

HMODULE plugin_handle;
DLLAPI ToolboxPlugin* ToolboxPluginInstance()
{
    static TargetOverride instance;
    return &instance;
}

namespace {
    GW::Chat::CmdCB original_callback;
}

bool IsNearestStr(const wchar_t* str)
{
    return wcscmp(str, L"nearest") == 0 || wcscmp(str, L"closest") == 0;
}

const wchar_t* GetRemainingArgsWstr(const wchar_t* message, int argc_start)
{
    const wchar_t* out = message;
    for (int i = 0; i < argc_start && out; i++) {
        out = wcschr(out, ' ');
        if (out) out++;
    }
    return out;
}

std::wstring ToLower(std::wstring s)
{
    std::ranges::transform(s, s.begin(), [](wchar_t c) -> wchar_t { return static_cast<wchar_t>(::tolower(c)); });
    return s;
}

bool ParseUInt(const wchar_t* str, unsigned int* val, int base = 10)
{
    wchar_t* end;
    if (!str) return false;
    *val = wcstoul(str, &end, base);
    if (str == end || errno == ERANGE)
        return false;
    return true;
}

bool TargetNearest(const wchar_t* model_id_)
{
    uint32_t model_id = 0;
    uint32_t index = 0; // 0=nearest. 1=first by id, 2=second by id, etc.

    if (ParseUInt(model_id_, &model_id)) {
        // check if there's an index component
        if (const wchar_t* rest = GetRemainingArgsWstr(model_id_, 1)) {
            ParseUInt(rest, &index);
        }
    }

    // target nearest agent
    const GW::AgentArray* agents = GW::Agents::GetAgentArray();
    const GW::AgentLiving* me = GW::Agents::GetPlayerAsAgentLiving() ;
    if (me == nullptr || agents == nullptr) return false;

    float distance = GW::Constants::SqrRange::Compass;
    size_t closest = 0;
    size_t count = 0;

    for (const GW::Agent* agent : *agents) {
        if (!agent || agent == me) continue;
        if (index == 0) { // target closest
            const float newDistance = GW::GetSquareDistance(me->pos, agent->pos);
            if (newDistance < distance) {
                closest = agent->agent_id;
                distance = newDistance;
            }
        }
        else { // target based on id
            if (++count == index) {
                closest = agent->agent_id;
                break;
            }
        }
    }
    if (closest) {
        GW::Agents::ChangeTarget(closest);
        return true;
    }
    return false;
}

void CmdTargetOverride(const wchar_t* message, int argc, LPWSTR* argv)
{
    if (argc < 2) return;

    const wchar_t* zero_w = L"0";

    const std::wstring arg1 = ToLower(argv[1]);
    if (IsNearestStr(arg1.c_str())) {
        if (argc < 3) { // target nearest
            if (!TargetNearest(zero_w)) {
                return original_callback(message, argc, argv);
            }
        }
        const std::wstring arg2 = ToLower(argv[2]);
        // /target nearest 1234
        if (!TargetNearest(arg2.c_str())) {
            return original_callback(message, argc, argv);
        }
    }
    if (!TargetNearest(GetRemainingArgsWstr(message, 1))) {
        original_callback(message, argc, argv);
    }
}

void TargetOverride::Initialize(ImGuiContext* ctx, ImGuiAllocFns fns, HMODULE toolbox_dll)
{
    ToolboxPlugin::Initialize(ctx, fns, toolbox_dll);

    original_callback = GW::Chat::GetCommand(L"target");
    GW::Chat::CreateCommand(L"target", CmdTargetOverride);

    constexpr auto color = 0xffffff;
    const auto init_msg = L"Target Plugin Initialized!";
    const auto msg = std::format(L"<a=1>GWToolbox</a><c=#{:6X}>: {}</c>", color, init_msg);

    GW::GameThread::Enqueue([msg] {
        GW::Chat::WriteChat(GW::Chat::CHANNEL_GWCA1, msg.c_str());
    });
}

void TargetOverride::Terminate()
{
    ToolboxPlugin::Terminate();
    if (original_callback) {
        GW::Chat::CreateCommand(L"target", original_callback);
    }
    else {
        GW::Chat::DeleteCommand(L"target");
    }
}
