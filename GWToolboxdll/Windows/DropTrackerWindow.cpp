#include "stdafx.h"

#include <GWCA/Constants/Constants.h>

#include <GWCA/Context/WorldContext.h>
#include <GWCA/GameEntities/Agent.h>

#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/PartyMgr.h>

#include <Color.h>
#include <Utils/GuiUtils.h>
#include <Windows/DropTrackerWindow.h>

#include <Timer.h>

namespace {
    
}

void DropTrackerWindow::Terminate()
{
    ToolboxWindow::Terminate();
}

void DropTrackerWindow::Draw(IDirect3DDevice9*)
{
    if (!visible) {
        return;
    }

    ImGui::SetNextWindowCenter(ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(350, 208), ImGuiCond_FirstUseEver);

    if (ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {
        
    
    }

    ImGui::End();
}

void DropTrackerWindow::DrawSettingsInternal()
{
    
}

void DropTrackerWindow::LoadSettings(ToolboxIni* ini)
{
    ToolboxWindow::LoadSettings(ini);
    //LOAD_BOOL(show_minds_counter);
    //LOAD_FLOAT(souls_threshhold);
}

void DropTrackerWindow::SaveSettings(ToolboxIni* ini)
{
    ToolboxWindow::SaveSettings(ini);
    // SAVE_BOOL(hide_when_nothing);
    //SAVE_FLOAT(souls_threshhold);

}
