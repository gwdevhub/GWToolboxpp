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

    // Self-test for the close-then-reopen crash (P:\Code\Engine\Controls\CtlPage.cpp(558),
    // assertion "btnFrame") - reproduces it automatically instead of needing a manual repro each
    // build. See GuildWarsUIFrame.cpp's CreateNativeFrame comment for the fix under test.
    bool opened_once = false;
    bool closed_once = false;
    bool reopened_once = false;
    clock_t opened_at = 0;
    clock_t closed_at = 0;

    // Diagnostic only: logs every native UI component creation while active, so we can compare
    // what tab_index values GW's own real tabs (General/Graphics/...) use against ours (255/0xff)
    // right up to the point of the reopen crash - the crash dump alone doesn't show this since the
    // fault is inside the engine's own tab-button refresh code, not anything of ours on the stack.
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
    // Tried triggering this off kFrameVisibilityChanged instead of polling (GW::UI::
    // RegisterFrameUIMessageCallback) - froze the game on opening Options. That hook fires for
    // *every* frame in the game receiving the message, not just Options (confirmed via GWCA's own
    // implementation, Source/UIMgr.cpp ~2191) - opening a window cascades visibility changes across
    // dozens of children near-simultaneously, and our handler's GetOptionsTabs() walks Options'
    // relation.children right as the engine may be mid-mutation of that same list from its own
    // window-open cascade. Reverted to polling rather than chasing the exact reentrant mechanism
    // further - Poll() itself already throttles and no-ops once active, so this is cheap.
    settings_tab.Poll(GetOptionsTabs());
}

void GuildWarsUI_ToolboxSettings::SignalTerminate()
{
    ToolboxModule::SignalTerminate();
    terminating = true;
    GW::GameThread::Enqueue([]() {
        // Close the Options window first if our tab is still showing under it - removing/destroying
        // our tab while its parent window is open and visible leaves the TabsFrame mid-cascade for
        // that same window (the same class of reentrancy that caused the kSetLayout freeze earlier
        // in this file's history). Destroying the whole window first (see WorldMapWidget.cpp's
        // TriggerWorldMapRedraw for the same DestroyUIComponent pattern) forces a clean teardown
        // path before we touch our own tab at all.
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
    // Not blocking anything until SignalTerminate has actually been called (matches the original
    // !terminating check for that case). Once it has, block DLL unload until our tab's native
    // frame has genuinely been torn down - Remove() above only *requests* removal (TabsFrame::
    // RemoveTab); the actual destruction, and our own kDestroyFrame handler nulling GuildWarsUI_
    // Tab::frame (see HandleMessage), can lag a poll or two behind. Unloading before then leaves
    // the engine holding a callback pointer (ItemCallback) into memory that's about to become
    // invalid.
    return !terminating || !settings_tab.IsActive();
}
