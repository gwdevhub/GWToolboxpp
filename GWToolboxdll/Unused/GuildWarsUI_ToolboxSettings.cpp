#include "stdafx.h"

#include <GWCA/GameEntities/Frame.h>

#include <GWCA/Managers/GameThreadMgr.h>

#include "GuildWarsUI_ToolboxSettings.h"
#include "Utils/TextUtils.h"


namespace {
    bool terminating = false;

    
    const auto settings_tab_label = L"\x108\x107Toolbox Settings\x1";
    const auto settings_tab_id = 8;

    GW::TabsFrame* GetSettingsTabs()
    {
        return (GW::TabsFrame*)GW::UI::GetChildFrame(GW::UI::GetFrameByLabel(L"Options"), 1);
    }

    struct CheckboxPref {
        std::wstring label;
        bool* variable;
        GW::CheckboxFrame* frame = nullptr;
    };
    std::vector<CheckboxPref*> prefs;

    void CreatePrefCheckbox(CheckboxPref* pref, uint32_t parent_frame_id = 0)
    {
        if (!parent_frame_id) {
            const auto frame = GW::UI::GetChildFrame(GetSettingsTabs(), settings_tab_id);
            if (!frame) return;
            parent_frame_id = frame->frame_id;
        }
        const auto encoded = std::format(L"\x108\x107{}\x1", pref->label);
        pref->frame = GW::CheckboxFrame::Create(parent_frame_id, 0, 0, encoded.c_str(), pref->label.c_str());
        pref->frame->SetChecked(*pref->variable);
    }

    void OnToolboxTab_UICallback(GW::UI::InteractionMessage* message, void* wparam, void* lparam)
    {
        switch (message->message_id) {
            case GW::UI::UIMessage::kInitFrame: {
                // Add your initialization logic here
                for (auto& pref : prefs) {
                    CreatePrefCheckbox(pref, message->frame_id);
                }

            } break;
            case GW::UI::UIMessage::kSetLayout: {
                const auto scrollable = GW::UI::GetChildFrame(GetSettingsTabs(), settings_tab_id);
                auto parent = GW::UI::GetFrameById(message->frame_id);

                size_t i = 0;
                const auto height = 24.f;
                for (auto& relation : parent->relation.siblings) {
                    const auto checkbox = (GW::CheckboxFrame*)relation.GetFrame();
                    checkbox->position.top = parent->position.top - (height * i);
                    checkbox->position.bottom = checkbox->position.top - height;
                    checkbox->position.left = parent->position.left;
                    checkbox->position.right = parent->position.right;
                    i++;
                }
                GW::UI::TriggerFrameRedraw(scrollable);

            } break;
            case GW::UI::UIMessage::kRefreshContent: {
                GW::UI::TriggerFrameRedraw(GW::UI::GetFrameById(message->frame_id));
            } break;
            case GW::UI::UIMessage::kMeasureContent: { // Measure/size calculation message
                                                       // auto packet = (GW::UI::UIPacket::kMeasureContent*)wparam;
                //  Let default handler run first
                GW::UI::Default_UICallback(message, wparam, lparam);

                float* size_output = *(float**)((char*)wparam + 8);
                if (size_output) {
                    size_output[0] = 220.0f; // width
                    size_output[1] = 1000.f; // height
                }
                return; // Don't call default handler again
            } break;
            case GW::UI::UIMessage::kMouseClick2: {
                const auto packet = (GW::UI::UIPacket::kMouseAction*)wparam;
                if (packet->current_state != GW::UI::UIPacket::ActionState::MouseUp) return;
                const auto checkbox = (GW::CheckboxFrame*)GW::UI::GetFrameById(packet->frame_id);
                auto found = std::find_if(prefs.begin(), prefs.end(), [checkbox](CheckboxPref* pref) {
                    return pref->frame == checkbox;
                });
                if (found != prefs.end()) {
                    *(*found)->variable = checkbox->IsChecked();
                }
            } break;
            case GW::UI::UIMessage::kDestroyFrame: {
                for (auto& pref : prefs) {
                    pref->frame = 0;
                }
                // GW remembers your last opened tab in memory when it gets ui message 0x2e.
                // we don't want it to try and restore our tab when settings is re-opened because we won't have made it yet!
                const auto tabs = GetSettingsTabs();
                ASSERT(tabs);
                auto context = (uint32_t*)GW::UI::GetFrameContext(tabs);
                if (context && context[1] == settings_tab_id) {
                    // If we're the current tab on closing, we need to not be!
                    // So this is mad hairy - we need to tell the game that the general settings tab is selected, but the general settings tab is already destroyed!
                    const auto parent = tabs->relation.GetParent();
                    GW::UI::UIPacket::kMouseAction packet = {.child_offset_id = tabs->child_offset_id, .current_state = GW::UI::UIPacket::ActionState::MouseUp};
                    const auto original_frame_state = parent->frame_state;
                    parent->frame_state &= ~0x2000;
                    context[1] = tabs->relation.siblings.begin()->GetFrame()->child_offset_id; // Manually change to tab 0
                    ASSERT(GW::UI::SendFrameUIMessage(parent, GW::UI::UIMessage::kMouseClick2, &packet));
                    parent->frame_state = original_frame_state;
                }
            } break;
        }
        GW::UI::Default_UICallback(message, wparam, lparam);
    }

    void AddToolboxTab()
    {
        const auto tabs = GetSettingsTabs();
        if (!tabs) return;
        if (!tabs->GetTabByLabel(settings_tab_label)) {
            // 1. Create a temporary scrollable frame - we'll use this to grab the callbacks we need to add it as a tab.
            const auto tmp_frame = GW::ScrollableFrame::Create(tabs->frame_id);
            const auto scrollable_callback = tmp_frame->frame_callbacks[0].callback;
            const auto inner_callback = tmp_frame->GetPage()->frame_callbacks[0].callback;
            GW::UI::DestroyUIComponent(tmp_frame);

            GW::ScrollableFrame::ScrollablePageContext packet = {.flags = 0, .page_callback = inner_callback, .wparam = 0};
            const auto existing = GW::UI::GetChildFrame(tabs, settings_tab_id);
            if (existing) {
                tabs->RemoveTab(existing->child_offset_id);
            }
            const auto scrollable_tab = (GW::ScrollableFrame*)tabs->AddTab(settings_tab_label, 0x20000, settings_tab_id, scrollable_callback, &packet);

            scrollable_tab->AddItem(0, 0, OnToolboxTab_UICallback);
            GW::UI::TriggerFrameRedraw(tabs);
        }
    }
    void RemoveToolboxTab()
    {
        const auto tabs = GetSettingsTabs();
        if (!tabs) return;
        tabs->RemoveTab(tabs->GetTabByLabel(settings_tab_label)->child_offset_id);
    }

    GW::HookEntry OnPostUIMessage_HookEntry;
    void OnPostUIMessage(GW::HookStatus*, GW::UI::UIMessage message_id, void* , void*) {
        switch (message_id) {
            case GW::UI::UIMessage::kChangeSettingsTab:
                AddToolboxTab();
                break;
        }
    }
}

void GuildWarsUI_ToolboxSettings::Initialize() {
    ToolboxModule::Initialize();
    GW::UI::UIMessage ui_messages[] = {GW::UI::UIMessage::kChangeSettingsTab};
    for (auto message_id : ui_messages) {
        GW::UI::RegisterUIMessageCallback(&OnPostUIMessage_HookEntry, message_id, OnPostUIMessage, 0x8000);
    }
}
void GuildWarsUI_ToolboxSettings::SignalTerminate()
{
    ToolboxModule::SignalTerminate();
    terminating = true;
    for (auto pref : prefs) {
        delete pref;
    }
    prefs.clear();
    GW::GameThread::Enqueue([]() {
        RemoveToolboxTab();
        terminating = false;
    });
}
bool GuildWarsUI_ToolboxSettings::CanTerminate() {
    return !terminating;
}

void GuildWarsUI_ToolboxSettings::AddCheckboxPref(const char* label, bool* variable) {
    auto checkbox_pref = new CheckboxPref({.label = TextUtils::StringToWString(label), .variable = variable});
    GW::GameThread::Enqueue([checkbox_pref]() {
        CreatePrefCheckbox(checkbox_pref);
    });
}
