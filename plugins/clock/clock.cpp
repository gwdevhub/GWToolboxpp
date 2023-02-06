#include "Clock.h"

#include <GWCA/Utilities/Scanner.h>
#include <GWCA/Utilities/Hooker.h>

#include <imgui.h>
#include <cstdint>
#include <corecrt_wstdio.h>
#include <time.h>

#define _countof(var) sizeof(var) / sizeof(*var)

namespace {
    char time_buf[100] = { 0 };
    errno_t GetTime(char* out, size_t len)
    {
        time_t timestamp;
        if (time(&timestamp) != timestamp)
            return -1;
        if (ctime_s(out, len, &timestamp) != 0)
            return -1;
        out[strlen(out) - 1] = 0; // Remove newline char
        return 0;
    }

    typedef void(__cdecl *SendChat_pt)(wchar_t *message, uint32_t agent_id);
    SendChat_pt SendChat_Func = nullptr;
    SendChat_pt SendChat_Ret = nullptr;

    void __cdecl OnSendChat(wchar_t *message, uint32_t agent_id) {
        GW::Hook::EnterHook();
        if (message && wcsncmp(L"/clock", message, 6) == 0) {
            wchar_t sendchat_buf[100];
            GetTime(time_buf, _countof(time_buf));
            swprintf(sendchat_buf, _countof(sendchat_buf), L"#%S", time_buf);
            message = sendchat_buf;
            agent_id = 0;
        }
        SendChat_Ret(message, agent_id);
        GW::Hook::LeaveHook();
    }
}

DLLAPI ToolboxPlugin* ToolboxPluginInstance()
{
    static Clock instance;
    return &instance;
}
void Clock::Draw(IDirect3DDevice9*)
{
    if (!toolbox_handle)
        return;
    ImGui::Begin("clock");
    GetTime(time_buf, _countof(time_buf));
    ImGui::TextUnformatted(time_buf);
    ImGui::End();
}

void Clock::Initialize(ImGuiContext* ctx, ImGuiAllocFns fns, HMODULE toolbox_dll)
{
    ToolboxPlugin::Initialize(ctx, fns, toolbox_dll);

    GW::HookBase::Initialize();
    GW::Scanner::Initialize();
    // Copied from GWCA's ChatMgr module
    SendChat_Func = (SendChat_pt)GW::Scanner::Find("\x8D\x85\xE0\xFE\xFF\xFF\x50\x68\x1C\x01", "xxxxxxxxx", -0x3E);

    if (SendChat_Func) {
        GW::HookBase::CreateHook(SendChat_Func, OnSendChat, (void **)&SendChat_Ret);
        GW::HookBase::EnableHooks(SendChat_Func);
    }

}

void Clock::Terminate()
{
    ToolboxPlugin::Terminate();
    if(SendChat_Func)
        GW::HookBase::RemoveHook(SendChat_Func);

    GW::HookBase::Deinitialize();
}
