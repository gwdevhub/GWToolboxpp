#include <Follow.h>

#include <GWCA/GWCA.h>
#include <GWCA/GameEntities/Agent.h>

#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>

#include <GWCA/Utilities/Hooker.h>

DLLAPI ToolboxPlugin* ToolboxPluginInstance()
{
    static FollowPlugin instance;
    return &instance;
}

namespace 
{
    bool following = false;

    GW::HookEntry ChatCmd_HookEntry;

    void logMessage(std::string_view message)
    {
        const auto wMessage = std::wstring{message.begin(), message.end()};
        const size_t len = 40 + wcslen(wMessage.c_str());
        auto to_send = new wchar_t[len];
        swprintf(to_send, len - 1, L"<a=1>%s</a><c=#%6X>: %s</c>", L"Follow plugin", 0xFFFFFF, wMessage.c_str());
        GW::GameThread::Enqueue([to_send] {
            GW::Chat::WriteChat(GW::Chat::Channel::CHANNEL_GWCA2, to_send, nullptr);
            delete[] to_send;
        });
    }
}

void FollowPlugin::Update(float delay)
{
    ToolboxPlugin::Update(delay);

    const auto player = GW::Agents::GetControlledCharacter();
    const auto currentTarget = GW::Agents::GetTargetAsAgentLiving();
    if (!following || !player || !currentTarget || !player->GetIsIdle() || !currentTarget->GetIsLivingType()) 
        return;
    if (GW::GetSquareDistance(player->pos, currentTarget->pos) < followDistance * followDistance) 
        return;
    
    GW::GameThread::Enqueue([]
    {
        GW::UI::Keydown(GW::UI::ControlAction::ControlAction_Interact);
    });
}

void FollowPlugin::LoadSettings(const wchar_t* folder)
{
    ToolboxPlugin::LoadSettings(folder);

    const auto ini = LoadIni(folder);

    followDistance = (float)ini.GetDoubleValue(Name(), VAR_NAME(followDistance), followDistance);
}

void FollowPlugin::SaveSettings(const wchar_t* folder)
{
    ToolboxPlugin::SaveSettings(folder);

    auto ini = LoadIni(folder);

    ini.SetDoubleValue(Name(), VAR_NAME(followDistance), followDistance);

    PLUGIN_ASSERT(ini.SaveFile(ini.location_on_disk) == SI_OK);
}

void FollowPlugin::DrawSettings()
{
    ToolboxPlugin::DrawSettings();

    ImGui::Text("Usage: Type /follow to activate or deactivate.");

    ImGui::PushItemWidth(100.f);
    ImGui::Text("Start following once target is further than");
    ImGui::SameLine();
    ImGui::InputFloat("inches away", &followDistance);
    ImGui::PopItemWidth();

    ImGui::Text("Version 1.0.3");
}

void FollowPlugin::Initialize(ImGuiContext* ctx, ImGuiAllocFns fns, HMODULE toolbox_dll)
{
    ToolboxPlugin::Initialize(ctx, fns, toolbox_dll);
    
    GW::Chat::CreateCommand(&ChatCmd_HookEntry, L"follow", [](GW::HookStatus*, const wchar_t*, const int, const LPWSTR*) 
    {
        following = !following;
        logMessage(following ? "Activated" : "Deactivated");
    });
}

void FollowPlugin::SignalTerminate()
{
    GW::Chat::DeleteCommand(&ChatCmd_HookEntry, L"follow");
    ToolboxPlugin::SignalTerminate();
}
