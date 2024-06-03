#include "stdafx.h"

#include <Modules/MouseWalkingModule.h>
#include <Defines.h>
#include <Logger.h>
#include <ctime>
#include <GWCA/Managers/UIMgr.h>

namespace {
}

void MouseWalkingModule::DrawSettingsInternal()
{
}

void MouseWalkingModule::Initialize() {

}

void MouseWalkingModule::Terminate() {

}

void MouseWalkingModule::Update(float)
{
    if (!ImGui::IsKeyDown(ImGui::GetKeyIndex(ImGuiKey_LeftAlt)))
    {
        if (GW::UI::GetPreference(GW::UI::FlagPreference::DisableMouseWalking)) {
            GW::UI::SetPreference(GW::UI::FlagPreference::DisableMouseWalking, false);
        }
    }
    else
    {
        if (!GW::UI::GetPreference(GW::UI::FlagPreference::DisableMouseWalking)) {
            GW::UI::SetPreference(GW::UI::FlagPreference::DisableMouseWalking, true);
        }
    }
}
