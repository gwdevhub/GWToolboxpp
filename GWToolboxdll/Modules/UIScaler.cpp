#include "stdafx.h"

#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Utilities/Hooker.h>
#include <GWCA/Utilities/Scanner.h>

#include "Defines.h"
#include "UIScaler.h"
#include <Utils/TextUtils.h>


namespace {


    float custom_ui_scale = 1.f;
    bool initialised = false;
    GW::HookEntry ChatCmd_HookEntry;

    const char* syntax = "Syntax: /setuiscale <0.1 - 10.0>";

    float GetUIScaleFactor()
    {
        return initialised ? custom_ui_scale : 1.f;
    }

    void UIScaleUpdated() {
        GW::GameThread::Enqueue([]() {
            GW::UI::SetPreference(GW::UI::EnumPreference::InterfaceSize, GW::UI::GetPreference(GW::UI::EnumPreference::InterfaceSize));
        });
    }

    using GetUIScaleFloat_pt = float(__cdecl*)();
    GetUIScaleFloat_pt GetUIScaleFloat_Func = 0, GetUIScaleFloat_Ret = 0;
    float OnGetUIScaleFloat()
    {
        GW::Hook::EnterHook();
        const auto res = GetUIScaleFloat_Ret() * GetUIScaleFactor();
        GW::Hook::LeaveHook();
        return res;
    }

    using OnGetFontMetrics_pt = void(__cdecl*)(uint32_t font_style_index, void** out_font_handle, float* out_font_height, uint32_t* out_font_data);
    OnGetFontMetrics_pt GetFontMetrics_Func = 0, GetFontMetrics_Ret = 0;
    void OnGetFontMetrics(uint32_t font_style_index, void** out_font_handle, float* out_font_height, uint32_t* out_font_data)
    {
        GW::Hook::EnterHook();
        GetFontMetrics_Ret(font_style_index, out_font_handle, out_font_height, out_font_data);
        *out_font_height *= GetUIScaleFactor();
        GW::Hook::LeaveHook();
    }

    void CHAT_CMD_FUNC(CmdSetUIScale)
    {
        if (argc < 2)
           return Log::Error(syntax);
        if (!TextUtils::ParseFloat(argv[1], &custom_ui_scale)) 
            return Log::Error(syntax);
        if (custom_ui_scale < 0.1f) custom_ui_scale = .1f;
        if (custom_ui_scale > 10.f) custom_ui_scale = 10.f;
        UIScaleUpdated();
    }
} // namespace


void UIScaler::Initialize()
{
    ToolboxModule::Initialize();
    GetUIScaleFloat_Func = (GetUIScaleFloat_pt)GW::Scanner::ToFunctionStart(GW::Scanner::FindAssertion("FrFont.cpp", "s_curScale > -10", 0, 0));
    if (GetUIScaleFloat_Func) {
        GW::Hook::CreateHook((void**)&GetUIScaleFloat_Func, OnGetUIScaleFloat, (void**)&GetUIScaleFloat_Ret);
        GW::Hook::EnableHooks(GetUIScaleFloat_Func);
    }
    GetFontMetrics_Func = (OnGetFontMetrics_pt)GW::Scanner::ToFunctionStart(GW::Scanner::FindAssertion("FrFont.cpp", "(*fontDataArray)[fontDataIndex].height != 0", 0, 0), 0xfff);
    if (GetFontMetrics_Func) {
        GW::Hook::CreateHook((void**)&GetFontMetrics_Func, OnGetFontMetrics, (void**)&GetFontMetrics_Ret);
        GW::Hook::EnableHooks(GetFontMetrics_Func);
    }
    GW::Chat::CreateCommand(&ChatCmd_HookEntry, L"setuiscale", CmdSetUIScale);
    initialised = true;
    #ifdef _DEBUG
        ASSERT(GetUIScaleFloat_Func);
        ASSERT(GetFontMetrics_Func);
    #endif
    UIScaleUpdated();
}
void UIScaler::SignalTerminate()
{
    ToolboxModule::Terminate();
    initialised = false;
    GW::Chat::DeleteCommand(&ChatCmd_HookEntry);
    if (GetFontMetrics_Func) {
        GW::Hook::DisableHooks(GetFontMetrics_Func);
        GetFontMetrics_Func = 0;
    }
    if (GetUIScaleFloat_Func) {
        GW::Hook::DisableHooks(GetUIScaleFloat_Func);
        GetUIScaleFloat_Func = 0;
    }
    UIScaleUpdated();
}
float UIScaler::GetUIScaleFactor()
{
    return ::GetUIScaleFactor();
}
void UIScaler::LoadSettings(ToolboxIni* ini)
{
    ToolboxModule::LoadSettings(ini);
    LOAD_FLOAT(custom_ui_scale);
}
void UIScaler::SaveSettings(ToolboxIni* ini)
{
    ToolboxModule::SaveSettings(ini);
    SAVE_FLOAT(custom_ui_scale);
}

void UIScaler::DrawSettingsInternal() {
    ImGui::TextDisabled(Description());
    ImGui::Text("UI Scale Factor: %.2f", custom_ui_scale);
    if (ImGui::SliderFloat("##UIScaleFactor", &custom_ui_scale, 0.1f, 10.f, "%.2f")) {
        UIScaleUpdated();
    }
    if (ImGui::Button("Reset to Default")) {
        custom_ui_scale = 1.f;
        UIScaleUpdated();
    }
}
