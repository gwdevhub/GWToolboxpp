#include "clock.h"

#include "GWCA/Managers/ChatMgr.h"
#include "GWCA/Managers/GameThreadMgr.h"

#include <format>
#include <imgui.h>

DLLAPI TBModule* TBModuleInstance()
{
    static Clock instance;
    return &instance;
}

void Clock::Draw(IDirect3DDevice9* device)
{
    ImGui::Begin("clock test");
    ImGui::Text("hello, I am a plugin!");
    ImGui::End();
}

void CmdClock(const wchar_t*, int, wchar_t**)
{
    const auto now = std::chrono::system_clock::now();
    std::time_t time = std::chrono::system_clock::to_time_t(now);
    const auto print_time = std::ctime(&time);
    auto str = std::format("Current time is {}", print_time);
    str.pop_back();
    GW::GameThread::Enqueue([str] {
        GW::Chat::SendChat('#', str.c_str());
    });
}

void Clock::Initialize(ImGuiContext* ctx)
{
    TBModule::Initialize(ctx);
    GW::Chat::CreateCommand(L"clock", CmdClock);
}

void Clock::Terminate()
{
    TBModule::Terminate();
    GW::Chat::DeleteCommand(L"clock");
}