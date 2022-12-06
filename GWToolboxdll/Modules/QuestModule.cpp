#include "stdafx.h"

#include <GWCA/Utilities/Hook.h>

#include <GWCA/Managers/UIMgr.h>

#include "QuestModule.h"

namespace { // Settings

    void OnPreUIMessage(GW::HookStatus*, GW::UI::UIMessage message_id, void*, void*) {
        switch (message_id) {
        case GW::UI::UIMessage::kMapLoaded:
            break;
        }
    }
    void OnPostUIMessage(GW::HookStatus*, GW::UI::UIMessage message_id, void*, void*) {
        switch (message_id) {
        case GW::UI::UIMessage::kMapLoaded:
            break;
        }
    }

} // namespace

void QuestModule::Initialize() {
    ToolboxModule::Initialize();




}
