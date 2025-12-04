#include "stdafx.h"

#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Utilities/Hooker.h>
#include <GWCA/Utilities/Scanner.h>

#include "Defines.h"
#include "UIScaler.h"
#include <Utils/TextUtils.h>
#include <ImGuiAddons.h>


namespace {


    float custom_ui_scale = 1.f;
    bool smart_scale_fonts = true;
    bool initialised = false;
    GW::HookEntry ChatCmd_HookEntry;

    const char* syntax = "Syntax: /setuiscale <0.1 - 10.0>";

    float GetUIScaleFactor()
    {
        return initialised ? custom_ui_scale : 1.f;
    }

    void UIScaleUpdated() {
        custom_ui_scale = std::clamp(custom_ui_scale, 0.1f, 10.f);
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

        // This could be found from the in-game font data with a scan, but its static data so no point
        const float font_sizes_by_style_index[] = {12.f, 12.f, 12.f, 12.f, 12.f, 12.f, 12.f, 13.5f, 15.8f, 24.8f, 52.5f, 15.8f, 12.f, 13.5f, 15.8f};
        static_assert(_countof(font_sizes_by_style_index) == 0xf, "Font sizes array size mismatch");

        uint32_t best_fit_font_index = font_style_index;
        const auto scale_factor = GetUIScaleFactor();
        if (smart_scale_fonts && scale_factor > 1.0f) {
            const auto target_font_height = font_sizes_by_style_index[font_style_index] * scale_factor;
            float smallest_font_size_upper = 999.f;
            for (size_t i = font_style_index; i < _countof(font_sizes_by_style_index); ++i) {
                if (font_sizes_by_style_index[i] > target_font_height && font_sizes_by_style_index[i] < smallest_font_size_upper) {
                    smallest_font_size_upper = font_sizes_by_style_index[i];
                    best_fit_font_index = i;
                }
            }
        }
       
        if (best_fit_font_index != font_style_index) {
            // Get the font size needed for the original font style...
            float original_calculated_height = .0f;
            GetFontMetrics_Ret(font_style_index, 0, &original_calculated_height, 0);
            original_calculated_height *= scale_factor;

            // ...then get the rest of the metrics for the best fit font style...
            GetFontMetrics_Ret(best_fit_font_index, out_font_handle, 0, out_font_data);

            // ...and swap the original height back in
            *out_font_height = original_calculated_height;
            GW::Hook::LeaveHook();
            return;
        }

        GetFontMetrics_Ret(font_style_index, out_font_handle, out_font_height, out_font_data);
        *out_font_height *= scale_factor;
        GW::Hook::LeaveHook();
    }

    void CHAT_CMD_FUNC(CmdSetUIScale)
    {
        if (argc < 2)
           return Log::Error(syntax);
        if (!TextUtils::ParseFloat(argv[1], &custom_ui_scale)) 
            return Log::Error(syntax);
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
        //ASSERT(GetUIScaleFloat_Func);
        //ASSERT(GetFontMetrics_Func);
    if (!GetUIScaleFloat_Func) {
        GetUIScaleFloat_Func || (Log::Warning("GetUIScaleFloat_Func missing"), true);
        GetFontMetrics_Func || (Log::Warning("GetFontMetrics_Func missing"),true);
    }
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
    LOAD_BOOL(smart_scale_fonts);
    UIScaleUpdated();
}
void UIScaler::SaveSettings(ToolboxIni* ini)
{
    ToolboxModule::SaveSettings(ini);
    SAVE_FLOAT(custom_ui_scale);
    SAVE_BOOL(smart_scale_fonts);
}

void UIScaler::DrawSettingsInternal()
{
    ImGui::TextDisabled(Description());
    ImGui::Text("UI Scale Factor: %.2f", custom_ui_scale);
    if (ImGui::SliderFloat("##UIScaleFactor", &custom_ui_scale, 0.1f, 10.f, "%.2f")) {
        UIScaleUpdated();
    }
    if (ImGui::Button("Reset to Default")) {
        custom_ui_scale = 1.f;
        UIScaleUpdated();
    }
    if (ImGui::Checkbox("Smart scale fonts", &smart_scale_fonts)) {
        UIScaleUpdated();
    }
    if(ImGui::IsItemHovered())
        ImGui::SetTooltip("Smart scale fonts will swap to a larger font if the current font is smaller than the UI scale factor.");
}
