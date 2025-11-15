#include "stdafx.h"

#include <GWCA/Utilities/Debug.h>
#include <GWCA/Utilities/Hooker.h>
#include <GWCA/Utilities/Scanner.h>
#include <GWCA/Managers/RenderMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include "FpsFix.h"

namespace {

    typedef void(__cdecl* SetFrameLimit_pt)(uint32_t frame_limit);
    SetFrameLimit_pt SetFrameLimit_Func = 0, SetFrameLimit_Ret = 0;
    void OnSetFrameLimit(uint32_t frame_limit)
    {
        GW::Hook::EnterHook();

        if (frame_limit == 90) {
            uint32_t monitor_frame_rate = GW::Render::GetGraphicsRendererValue(GW::Render::Metric::MonitorRefreshRate);
            if (monitor_frame_rate > frame_limit) {
                frame_limit = monitor_frame_rate;
            }
        }
        SetFrameLimit_Ret(frame_limit);
        GW::Hook::LeaveHook();
    }
}
void FpsFix::Initialize() {
    ToolboxModule::Initialize();

    SetFrameLimit_Func = (SetFrameLimit_pt)GW::Scanner::ToFunctionStart(GW::Scanner::Find("\x6a\x00\x68\x40\x42\x0f\x00\xe8", "xxxxxxxx", 0));
    if (SetFrameLimit_Func) {
        GW::Hook::CreateHook((void**)&SetFrameLimit_Func, OnSetFrameLimit, (void**)&SetFrameLimit_Ret);
        GW::Hook::EnableHooks(SetFrameLimit_Func);
    }
#ifdef _DEBUG
    ASSERT(SetFrameLimit_Func);
#endif
}
void FpsFix::Terminate()
{
    ToolboxModule::Terminate();

    GW::Hook::RemoveHook(SetFrameLimit_Func);
}
