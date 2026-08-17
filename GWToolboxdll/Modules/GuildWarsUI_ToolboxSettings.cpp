#include "stdafx.h"

#include <GWCA/GameEntities/Frame.h>

#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWToolbox.h>


#include "GuildWarsUI_ToolboxSettings.h"
#include "Utils/TextUtils.h"
#include "Utils/GuildWarsUIFrame.h"

namespace {
    bool terminating = false;
    GuildWarsUI_Tab settings_tab(L"Toolbox Settings");
    bool example_checkbox_value = false;

    GW::TabsFrame* GetOptionsTabs()
    {
        return (GW::TabsFrame*)GW::UI::GetChildFrame(GW::UI::GetFrameByLabel(L"Options"), 1);
    }

    bool waiting_for_login = true;

    // Self-test for the close-then-reopen crash (CtlPage.cpp(558), assertion "btnFrame"); see
    // GuildWarsUIFrame.cpp's CreateNativeFrame comment for the fix under test.
    bool opened_once = false;
    bool closed_once = false;
    bool reopened_once = false;
    clock_t opened_at = 0;
    clock_t closed_at = 0;

    // Diagnostic only: logs native UI component creations to compare GW's own tab_index values
    // against ours (255/0xff) up to the reopen crash, which the crash dump alone doesn't show.
    bool log_create_ui = false;
    GW::HookEntry create_ui_hook;
}

void GuildWarsUI_ToolboxSettings::Initialize()
{
    ToolboxModule::Initialize();
    GW::UI::RegisterCreateUIComponentCallback(&create_ui_hook, [](GW::UI::CreateUIComponentPacket* packet) {
        if (!log_create_ui) return;
        Log::LogW(L"[createui] frame_id=%u tab_index=%u flags=0x%x label=%s",
                  packet->frame_id, packet->tab_index, packet->component_flags,
                  packet->component_label ? packet->component_label : L"(null)");
        Log::FlushFile();
    });
    GW::GameThread::Enqueue([]() {
        settings_tab.AddChild(new GuildWarsUI_Checkbox(L"Example Checkbox", []() {
            example_checkbox_value = !example_checkbox_value;
        }));
        settings_tab.AddChild(new GuildWarsUI_Button(L"Example Button", []() {
            Log::Info("Button clicked");
        }));
        settings_tab.AddChild(new GuildWarsUI_Button(L"Exit Toolbox++", []() {
            GWToolbox::Instance().SignalTerminate();
        }));
    });
}

void GuildWarsUI_ToolboxSettings::Update(float)
{
    // Poll rather than hook kFrameVisibilityChanged: that fires for every frame, and walking
    // Options' relation.children mid open-cascade froze the game. Poll() throttles anyway.
    settings_tab.Poll(GetOptionsTabs());
}

void GuildWarsUI_ToolboxSettings::SignalTerminate()
{
    ToolboxModule::SignalTerminate();
    terminating = true;
    GW::GameThread::Enqueue([]() {
        // Destroy the visible Options window before removing our tab: pulling the tab out of a
        // live TabsFrame leaves it mid-cascade (same reentrancy as the earlier kSetLayout freeze).
        if (settings_tab.IsActive()) {
            if (const auto options_frame = GW::UI::GetFrameByLabel(L"Options")) {
                GW::UI::DestroyUIComponent(options_frame);
            }
        }
        settings_tab.Remove();
    });
}

bool GuildWarsUI_ToolboxSettings::CanTerminate()
{
    // Remove() only *requests* removal; the native frame teardown can lag a poll or two, and
    // unloading before it completes leaves the engine holding our ItemCallback pointer.
    return !terminating || !settings_tab.IsActive();
}
