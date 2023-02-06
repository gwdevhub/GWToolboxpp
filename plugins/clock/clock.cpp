#include "Clock.h"

#include "GWCA/Managers/ChatMgr.h"
#include "GWCA/Managers/GameThreadMgr.h"

#include <format>
#include <imgui.h>

namespace {
    bool send_to_chat = false;

    using CreateCommandFn = void (*)(const wchar_t*, const GW::Chat::CmdCB&);
    using DeleteCommandFn = void (*)(const wchar_t*);
    using SendChatFn = void (*)(char, const char*);

    CreateCommandFn create_command = nullptr;
    DeleteCommandFn delete_command = nullptr;
    SendChatFn send_chat = nullptr;

    void CmdClock(const wchar_t*, int, wchar_t**)
    {
        send_to_chat = true;
    }
    std::string GetTime()
    {
        const auto now = std::chrono::system_clock::now();
        const auto time = std::chrono::system_clock::to_time_t(now);
        char buf[100];
        ctime_s(buf, sizeof buf, &time);
        auto str = std::format("{}", buf);
        str.pop_back();
        return str;
    }
}


DLLAPI ToolboxPlugin* ToolboxPluginInstance()
{
    static Clock instance;
    return &instance;
}


void Clock::Update(float)
{
    if (!toolbox_handle)
        return;
    if (send_to_chat && send_chat) {
        send_chat('#', GetTime().c_str());
        send_to_chat = false;
    }
}
void Clock::Draw(IDirect3DDevice9*)
{
    if (!toolbox_handle)
        return;
    ImGui::Begin("clock");
    ImGui::TextUnformatted(GetTime().c_str());
    ImGui::End();
}

void Clock::Initialize(ImGuiContext* ctx, ImGuiAllocFns fns, HMODULE toolbox_dll)
{
    ToolboxPlugin::Initialize(ctx, fns, toolbox_dll);

    // we load our gwca methods dynamically
    create_command = reinterpret_cast<CreateCommandFn>(GetProcAddress(toolbox_dll, "?CreateCommand@Chat@GW@@YAXPB_WABV?$function@$$A6AXPB_WHPAPA_W@Z@std@@@Z"));
    delete_command = reinterpret_cast<DeleteCommandFn>(GetProcAddress(toolbox_dll, "?DeleteCommand@Chat@GW@@YAXPB_W@Z"));
    send_chat = reinterpret_cast<SendChatFn>(GetProcAddress(toolbox_dll, "?SendChat@Chat@GW@@YAXDPBD@Z"));
    if(create_command)
        create_command(L"clock", CmdClock);
}

void Clock::Terminate()
{
    ToolboxPlugin::Terminate();
    if(delete_command)
        delete_command(L"clock");
}
