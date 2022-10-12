#include "Clock.h"

#include "GWCA/Managers/ChatMgr.h"
#include "GWCA/Managers/GameThreadMgr.h"

#include <format>
#include <imgui.h>

using EnqueueFn = void (*)(std::function<void()>&&);
using CreateCommandFn = void (*)(const wchar_t*, const GW::Chat::CmdCB&);
using DeleteCommandFn = void (*)(const wchar_t*);
using SendChatFn = void (*)(char, const char*);

EnqueueFn enqueue = nullptr;
CreateCommandFn create_command = nullptr;
DeleteCommandFn delete_command = nullptr;
SendChatFn send_chat = nullptr;

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
    ctime_s(buf, sizeof buf, &time);
    auto str = std::format("{}", buf);
    str.pop_back();
    return str;
}

void Clock::Draw(IDirect3DDevice9*)
{
    if (!toolbox_handle)
        return;
    ImGui::Begin("clock");
    ImGui::Text("%s", GetTime().c_str());
    ImGui::End();
}

void CmdClock(const wchar_t*, int, wchar_t**)
{
    enqueue([] {
        send_chat('#', GetTime().c_str());
    });
}

void Clock::Initialize(ImGuiContext* ctx, ImGuiAllocFns fns, HMODULE toolbox_dll)
{
    ToolboxPlugin::Initialize(ctx, fns, toolbox_dll);

    // we load our gwca methods dynamically
    enqueue = reinterpret_cast<EnqueueFn>(GetProcAddress(toolbox_dll, "?Enqueue@GameThread@GW@@YAXV?$function@$$A6AXXZ@std@@@Z"));
    create_command = reinterpret_cast<CreateCommandFn>(GetProcAddress(toolbox_dll, "?CreateCommand@Chat@GW@@YAXPB_WABV?$function@$$A6AXPB_WHPAPA_W@Z@std@@@Z"));
    delete_command = reinterpret_cast<DeleteCommandFn>(GetProcAddress(toolbox_dll, "?DeleteCommand@Chat@GW@@YAXPB_W@Z"));
    send_chat = reinterpret_cast<SendChatFn>(GetProcAddress(toolbox_dll, "?SendChat@Chat@GW@@YAXDPBD@Z"));

    create_command(L"clock", CmdClock);
}

void Clock::Terminate()
{
    ToolboxPlugin::Terminate();
    delete_command(L"clock");
}
