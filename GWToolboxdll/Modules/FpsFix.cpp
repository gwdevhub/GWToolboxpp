#include "stdafx.h"

#include <GWCA/Utilities/Debug.h>
#include <GWCA/Utilities/Hooker.h>
#include <GWCA/Utilities/Scanner.h>
#include <GWCA/Managers/RenderMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include "FpsFix.h"

namespace {

    typedef void(__cdecl* ClampFps_pt)(uint32_t frame_limit);
    ClampFps_pt ClampFps_Func = 0, ClampFps_Ret = 0;
    void OnClampFps(uint32_t frame_limit) {
        GW::Hook::EnterHook();
        uint32_t old_fps_limit = 0;
        if (!GW::UI::GetCommandLinePref(L"fps", &old_fps_limit) || old_fps_limit) {
            ClampFps_Ret(frame_limit);
            GW::Hook::LeaveHook();
            return;
        }
        
        uint32_t monitor_frame_rate = GW::Render::GetGraphicsRendererValue(GW::Render::Metric::MonitorRefreshRate);
        uint32_t vsync_enabled = GetGraphicsRendererValue(GW::Render::Metric::Vsync);
        uint32_t pref = GW::UI::GetPreference(GW::UI::EnumPreference::FrameLimiter);
        if (monitor_frame_rate && (pref == 3 || vsync_enabled)) {
            GW::UI::SetCommandLinePref(L"fps", monitor_frame_rate);
            ClampFps_Ret(frame_limit);
            GW::UI::SetCommandLinePref(L"fps", old_fps_limit);
            GW::Hook::LeaveHook();
            return;
        }
        ClampFps_Ret(frame_limit);
        GW::Hook::LeaveHook();
    }
}
void FpsFix::Initialize() {
    ToolboxModule::Initialize();

    ClampFps_Func = (ClampFps_pt)GW::Scanner::ToFunctionStart(GW::Scanner::Find("\x75\x00\xbe\x5a\x00\x00\x00", "x?xxxxx", 0));
    if (ClampFps_Func) {
        GW::Hook::CreateHook((void**)&ClampFps_Func, OnClampFps, (void**)&ClampFps_Ret);
        GW::Hook::EnableHooks(ClampFps_Func);
    }
#ifdef _DEBUG
    ASSERT(ClampFps_Func);
#endif
}
void FpsFix::Terminate()
{
    ToolboxModule::Terminate();

    GW::Hook::RemoveHook(ClampFps_Func);
}
