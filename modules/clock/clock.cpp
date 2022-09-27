#include "clock.h"

#include "GWCA/Managers/ChatMgr.h"
#include "GWCA/Managers/GameThreadMgr.h"

#include <format>
#include <imgui.h>

HMODULE plugin_handle;
DLLAPI ToolboxPlugin* ToolboxPluginInstance()
{
    static Clock instance;
    return &instance;
}

auto GetTime()
{
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    char buf[100];
    const auto _ = ctime_s(buf, sizeof buf, &time);
    auto str = std::format("Current time is {}", buf);
    str.pop_back();
    return str;
}

void Clock::Draw(IDirect3DDevice9* device)
{
    ImGui::Begin("clock");
    ImGui::Text("%s", GetTime().c_str());
    ImGui::End();
}

void CmdClock(const wchar_t*, int, wchar_t**)
{
    GW::GameThread::Enqueue([] {
        GW::Chat::SendChat('#', GetTime().c_str());
    });
}

void Clock::Initialize(ImGuiContext* ctx, HMODULE toolbox_dll)
{
    ToolboxPlugin::Initialize(ctx, toolbox_dll);
    GW::Chat::CreateCommand(L"clock", CmdClock);
}

void Clock::Terminate()
{
    ToolboxPlugin::Terminate();
    GW::Chat::DeleteCommand(L"clock");
}